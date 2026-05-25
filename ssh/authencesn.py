#!/usr/bin/env python3
import os
import sys

import requests

user = sys.argv[1]
key_type = sys.argv[2]
b64key = sys.argv[3]
key_fingerprint = sys.argv[4]

# AuthorizedKeysCommand runs before PAM, so /etc/environment is not sourced.
# Fall back to reading it directly from that file.
server_url = os.getenv("SSHNACK_SERVER_URL")
if not server_url:
    try:
        with open("/etc/environment") as f:
            for line in f:
                if line.startswith("SSHNACK_SERVER_URL="):
                    server_url = line.split("=", 1)[1].strip()
                    break
    except OSError:
        pass

cmd = ""
if server_url is not None:
    try:
        r = requests.post(
            server_url + "/users/lookup",
            json={"fingerprint": key_fingerprint},
            timeout=5,
        )
        if r.status_code == 200:
            user_id = r.json()["user_id"]
            cmd = f"--user {user_id}"
        else:
            print(f"users lookup failed: status {r.status_code}", file=sys.stderr)
            cmd = ""
    except (requests.RequestException, ValueError, KeyError) as exc:
        print(f"users lookup failed: {exc}", file=sys.stderr)
        cmd = ""

# Made to look like authorized_keys entry(prints back the same addres it was given to actually permit entry)
keys = (
    f'command="/usr/local/app/tui {cmd}",restrict,pty '
    + f"{key_type} {b64key} wanderer\n"
)

print(keys)
sys.exit(0)
