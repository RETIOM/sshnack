# sshnack

sshnack is a vending machine you operate over OpenSSH. It is heavily inspired by
[terminal.shop](https://terminal.shop): instead of a website, you `ssh` into the
machine and a terminal client drops you straight in front of the slots. The crown
jewel is exactly that SSH front-end -- no password, no sign-up, your SSH key is
your identity. The same machine is also a plain HTTP/JSON API, so you can drive it
with `curl` just as well as with the TUI.

## How it works

- **Thread pool** -- a hand-written pthread pool (10 fixed workers) with the work
  queue kept as a linked list, guarded by a mutex and two condition variables.
- **Socket listening** -- raw BSD sockets; the main thread runs the `accept()`
  loop with `SO_REUSEADDR` and a `SIGINT` handler for graceful shutdown, handing
  each accepted connection to the pool.
- **Thread communication** -- the accept loop is the producer and the workers are
  the consumers: one condition variable wakes an idle worker when a connection is
  queued, a second lets the pool drain and join cleanly on shutdown.
- **SQLite** -- the `sqlite3` C API in WAL mode with foreign keys on; every query
  goes through prepared statements (`prepare`/`bind`/`step`/`finalize`). The schema
  is seeded from `init.sql` on first start.
- **HTTP and routing** -- requests are parsed by hand and dispatched through a
  radix (prefix) tree router that supports path parameters such as
  `/stock/:slot_id`.

## The sshgate

The interesting part is getting OpenSSH to hand every visitor straight to the
vending machine. `sshd` is configured by hand at image build time -- `sshd_config`
is overwritten rather than using the distro default -- and the key pieces are:

- **`AuthorizedKeysCommand`** -- instead of checking `~/.ssh/authorized_keys`,
  `sshd` runs [ssh/auth.py](ssh/auth.py) on every login. The script gets the
  offered key and its fingerprint, asks the server (`POST /users/lookup`) which
  user that fingerprint belongs to, and prints back a synthetic `authorized_keys`
  line. That line echoes the presented key (so the login is always accepted) and
  pins a forced command: the TUI launched with the matched `--user <id>`, plus
  `restrict,pty`. The result is that any key gets in and identity is resolved
  server-side from the key fingerprint -- no passwords, no manual key files.
- **A passwordless `vending` user** exists in the image so the login can actually
  complete and the forced command can run.
- **`ssh/entrypoint.sh`** runs before `sshd` and copies the container's environment
  variables (`SSHNACK_SERVER_URL`, `SSHNACK_USER_ID`, `SSHNACK_ADMIN_TOKEN`) into
  `/etc/environment`. That is its main job: variables passed to the container reach
  `sshd` itself, but SSH does not forward them to login sessions, so seeding
  `/etc/environment` is what makes them visible to the per-connection clients (the
  TUI) at all.
- **`UsePAM yes`** is what injects `/etc/environment` into each login session. The
  exception is `auth.py`: it runs at the `AuthorizedKeysCommand` stage, before PAM,
  so it reads `/etc/environment` as a file directly instead of relying on the
  injected variables.

## Usage

### Requirements

For a local build you need `gcc`, `make`, and the development headers for libcurl,
ncurses (wide) and SQLite:

- Debian/Ubuntu: `libcurl4-openssl-dev`, `libncursesw5-dev`, `libsqlite3-dev`

### Build and run locally

```
make                              # builds ./bin/server and ./bin/tui
sqlite3 sshnack.db < init.sql     # seed the database once
./bin/server --db sshnack.db      # serves the API on :8080
```

Then either use the TUI client or talk to the API directly:

```
./bin/tui --user 1                # terminal client against the local server
curl localhost:8080/stock         # or drive the API yourself
```

The full ssh-in experience is wired up in Docker; locally you just run the pieces
directly.

### Docker

Everything in one go:

```
docker compose up --build
```

This starts the SSH front-end on `localhost:2222` and the API on `localhost:8080`.
Or build and run the two services separately (they must share a network so the SSH
box can reach the server by name):

```
docker network create sshnack
docker build -f server/Dockerfile -t sshnack-server .
docker build -f ssh/Dockerfile    -t sshnack-ssh .
docker run -d --name sshnack-server --network sshnack -p 8080:8080 sshnack-server
docker run -d --name sshnack-ssh    --network sshnack -p 2222:22 \
    -e SSHNACK_SERVER_URL=http://sshnack-server:8080 sshnack-ssh
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
| --- | --- | --- |
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
   sshd  ──▶  AuthorizedKeysCommand  ──▶  auth.py
      │
      │  auth.py POSTs the offered key's fingerprint
      ▼
   server  ──▶  /users/lookup  ──▶  user_id        (reads SQLite)
      │
      │  auth.py returns a synthetic authorized_keys line:
      │  command="tui --user <id>"
      ▼
   sshd  ──▶  runs the forced command  ──▶  launches the TUI
      │
      │  HTTP / JSON
      ▼
   TUI (ncurses + libcurl)  ◀──▶  server (thread pool + radix router)  ◀──▶  SQLite
```
