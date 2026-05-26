#!/usr/bin/env python3
import os
import sys
import time

import requests

# Retry the lookup a few times so a freshly started (still-waking) server gets
# a chance to come up before we give up on resolving the user.
MAX_ATTEMPTS = 5
RETRY_DELAY = 2

user = sys.argv[1]
key_type = sys.argv[2]
b64key = sys.argv[3]
key_fingerprint = sys.argv[4]

# AuthorizedKeysCommand runs before PAM, so /etc/environment is not sourced.
# Fall back to reading it directly from that file.
def _read_env_file(key):
    try:
        with open("/etc/environment") as f:
            for line in f:
                if line.startswith(key + "="):
                    return line.split("=", 1)[1].strip()
    except OSError:
        pass
    return None

internal_url = os.getenv("SSHNACK_INTERNAL_URL") or _read_env_file("SSHNACK_INTERNAL_URL")
server_url = os.getenv("SSHNACK_SERVER_URL")    or _read_env_file("SSHNACK_SERVER_URL")
lookup_url = internal_url or server_url

cmd = ""
if lookup_url is not None:
    for attempt in range(1, MAX_ATTEMPTS + 1):
        try:
            r = requests.post(
                lookup_url + "/users/lookup",
                json={"fingerprint": key_fingerprint},
                timeout=5,
            )
            r.raise_for_status()
            cmd = f"--user {r.json()['user_id']}"
            break
        except (requests.RequestException, ValueError, KeyError) as exc:
            print(
                f"users lookup failed (attempt {attempt}/{MAX_ATTEMPTS}): {exc}",
                file=sys.stderr,
            )
            if attempt < MAX_ATTEMPTS:
                time.sleep(RETRY_DELAY)

# Made to look like authorized_keys entry(prints back the same addres it was given to actually permit entry)
keys = (
    f'command="/usr/local/app/tui {cmd}",restrict,pty '
    + f"{key_type} {b64key} wanderer\n"
)

print(keys)
sys.exit(0)
