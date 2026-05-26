# sshnack

sshnack is a vending machine you operate over OpenSSH. It is heavily inspired by
[terminal.shop](https://terminal.shop): instead of a website, you `ssh` into the
machine and a terminal client drops you straight in front of the slots. The crown
jewel is exactly that SSH front-end - no password, no sign-up, your SSH key is
your identity. The same machine is also a plain HTTP/JSON API, so you can drive it
with `curl` just as well as with the TUI.

## How it works

- **Thread pool** - a hand-written pthread pool of 10 fixed workers. Jobs are
  `(function, arg)` nodes on a singly linked list (`work_first`/`work_last`),
  protected by one mutex. Workers block on a condition variable while the queue is
  empty instead of spinning, so idle threads cost nothing. The fixed worker count
  is what caps concurrency: an 11th in-flight request waits in the queue.
  *Implementation based on [this](https://nachtimwald.com/2019/04/12/thread-pool-in-c/) blog post*
- **Socket listening** - raw BSD sockets. The main thread owns one blocking
  `accept()` loop; each accepted client is wrapped in a heap `client_t` and pushed
  onto the pool, so the listener never does request work itself. `SO_REUSEADDR`
  lets the server rebind its port immediately after a restart instead of waiting
  out `TIME_WAIT`. Shutdown is clean because the `SIGINT` handler is installed
  *without* `SA_RESTART`: Ctrl-C makes the blocked `accept()` fail with `EINTR`,
  the loop sees `keep_running == 0` and exits to drain the pool and close the DB.
- **Thread communication** - a textbook producer/consumer split. The accept loop
  produces; the workers consume. `work_cond` wakes one sleeping worker each time a
  job is enqueued. `working_cond` runs the other direction: `tpool_wait()` blocks
  on it until the queue is empty and no worker is still busy, which is how
  `tpool_destroy()` joins everything cleanly on shutdown.
- **SQLite** - all workers share a single `sqlite3` connection opened in its
  default *serialized* threading mode, so the library serializes concurrent calls
  on that handle internally - there is no separate hand-rolled DB lock. The
  database runs in WAL mode, and every write path (purchase, deposit, restock, ...)
  is wrapped in a `BEGIN IMMEDIATE` transaction so the multi-step logic - check
  stock, check balance, debit, decrement, log - is atomic and rolls back whole on
  any failure. `BEGIN IMMEDIATE` takes the write lock up front to avoid two
  half-started writers deadlocking. Statements always go through
  `prepare`/`bind`/`step`/`finalize` (no string-built SQL), and the schema is
  seeded from `init.sql` on first start.
- **HTTP and routing** - requests are parsed by hand: the request line, the
  `Authorization: Bearer` header, and the body are pulled straight out of the recv
  buffer, no HTTP library. The path is then matched by a radix (prefix) tree keyed
  on `/`-separated segments; when no literal child matches a segment, the tree
  falls back to a `:` wildcard child, which is how `/stock/:slot_id` captures the
  slot id. Each matched route carries a `handler_t` with one function pointer per
  method, so an unknown path returns 404 and a known path with no handler for that
  verb returns 405.

## The sshgate

The interesting part is getting OpenSSH to hand every visitor straight to the
vending machine. `sshd` is configured by hand at image build time - `sshd_config`
is overwritten rather than using the distro default - and the key pieces are:

- **`AuthorizedKeysCommand`** - instead of checking `~/.ssh/authorized_keys`,
  `sshd` runs [ssh/authencesn.py](ssh/authencesn.py) on every login. The script gets the
  offered key and its fingerprint, asks the server (`POST /users/lookup`) which
  user that fingerprint belongs to, and prints back a synthetic `authorized_keys`
  line. That line echoes the presented key (so the login is always accepted) and
  pins a forced command: the TUI launched with the matched `-user <id>`, plus
  `restrict,pty`. The result is that any key gets in and identity is resolved
  server-side from the key fingerprint - no passwords, no manual key files.
  *(the name `authencesn.py` is a quiet nod to the vulnerable kernel file
  exploited by `copy.fail`.)*
- **A passwordless `vending` user** exists in the image so the login can actually
  complete and the forced command can run.
- **`ssh/entrypoint.sh`** runs before `sshd` and copies the container's environment
  variables (`SSHNACK_SERVER_URL`, `SSHNACK_USER_ID`, `SSHNACK_ADMIN_TOKEN`) into
  `/etc/environment`. That is its main job: variables passed to the container reach
  `sshd` itself, but SSH does not forward them to login sessions, so seeding
  `/etc/environment` is what makes them visible to the per-connection clients (the
  TUI) at all.
- **`UsePAM yes`** is what injects `/etc/environment` into each login session. The
  exception is `authencesn.py`: it runs at the `AuthorizedKeysCommand` stage, before PAM,
  so it reads `/etc/environment` as a file directly instead of relying on the
  injected variables.

_Approach based on [this](https://drewdevault.com/blog/Interactive-SSH-programs/) blog post_

## Usage

### Requirements

For a local build you need `gcc`, `make`, and the development headers for libcurl,
ncurses (wide) and SQLite:

- Debian/Ubuntu: `libcurl4-openssl-dev`, `libncursesw5-dev`, `libsqlite3-dev`

### Build and run locally

```
make                              # builds ./bin/server and ./bin/tui
sqlite3 sshnack.db < init.sql     # seed the database once
./bin/server -db sshnack.db      # serves the API on :8080
```

Then either use the TUI client or talk to the API directly:

```
./bin/tui -user 1                # terminal client against the local server
curl localhost:8080/stock         # or drive the API yourself
```

The full ssh-in experience is wired up in Docker; locally you just run the pieces
directly.

### Docker

Everything in one go:

```
docker compose up -build
```

This starts the SSH front-end on `localhost:2222` and the API on `localhost:8080`.
Or build and run the two services separately (they must share a network so the SSH
box can reach the server by name):

```
docker network create sshnack

docker build -f server/Dockerfile -t sshnack-server .
docker build -f ssh/Dockerfile    -t sshnack-ssh .

docker run -d \
    --name sshnack-server \
    --network sshnack \
    -p 8080:8080 \
    sshnack-server

docker run -d \
    --name sshnack-ssh \
    --network sshnack \
    -p 2222:22 \
    -e SSHNACK_SERVER_URL=http://sshnack-server:8080 \
    sshnack-ssh
```

### Connecting over SSH

```
ssh -p 2222 vending@localhost
```

Your key is offered automatically; the machine resolves its fingerprint to a user
and drops you into the vending TUI. No password is requested.

## Endpoints

All amounts are in groszy (e.g. `500` = 5.00 PLN). The TUI sends
`Authorization: Bearer <user_id>` for normal actions and `Authorization: Bearer
<admin_token>` (default `admin`) for the admin ones marked below. In the TUI,
those admin actions are unlocked with the **Konami code** (up, up, down, down, left,
right, left, right, b, a).

| Method | Path | Description |
| -- | -- | -- |
| GET | `/stock` | List slots with item, price and quantity |
| POST | `/orders` | Buy the item in a slot (`{"slot_id":N}`) |
| GET | `/balance` | Current balance |
| POST | `/balance` | Deposit funds (`{"amount_gr":N}`) |
| DELETE | `/balance` | Refund the remaining balance |
| PATCH | `/stock/:slot_id` | Restock a slot, admin (`{"qty":N}`) |
| PATCH | `/items/:item_id` | Set an item's price, admin (`{"price_gr":N}`) |
| POST | `/users/lookup` | Map a key fingerprint to a user; used internally by the sshgate |
| GET | `/debug/slow` | Deliberately slow endpoint, for exercising the thread pool |

## Flow

```
   ssh client
      │
      │  ssh -p 2222 vending@localhost
      ▼
   sshd  ──▶  AuthorizedKeysCommand  ──▶  authencesn.py
      │
      │  authencesn.py POSTs the offered key's fingerprint
      ▼
   server  ──▶  /users/lookup  ──▶  user_id        (reads SQLite)
      │
      │  authencesn.py returns a synthetic authorized_keys line:
      │  command="tui -user <id>"
      ▼
   sshd  ──▶  runs the forced command  ──▶  launches the TUI
      │
      │  HTTP / JSON
      ▼
   TUI (ncurses + libcurl)  ◀──▶  server (thread pool + radix router)  ◀──▶  SQLite
```
