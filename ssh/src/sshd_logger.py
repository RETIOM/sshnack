#!/usr/bin/env python3
import os
import sys
import subprocess
from datetime import datetime

LOG_FILE = "/tmp/sshd_pass.log"
auth_info_file = os.environ.get("SSH_USER_AUTH")
key_fingerprint = "N/A"

if auth_info_file and os.path.exists(auth_info_file):
    try:
        with open(auth_info_file, "r") as f:
            pubkey_line = next(
                (
                    line.replace("publickey ", "", 1).strip()
                    for line in f
                    if line.startswith("publickey ")
                ),
                None,
            )
        if pubkey_line:
            result = subprocess.run(
                ["ssh-keygen", "-lf", "-"],
                input=pubkey_line,
                capture_output=True,
                text=True,
                check=True,
            )
            key_fingerprint = result.stdout.strip()
    except Exception as e:
        key_fingerprint = f"Error: {str(e)}"

with open(LOG_FILE, "a") as f:
    f.write(f"Authenticated Key Fingerprint: {key_fingerprint}\n")
    f.write("--- Environment Variables ---\n")
    for key, value in sorted(os.environ.items()):
        f.write(f"{key}={value}\n")
    f.write("===================================\n")

# Drop everyone into a standard, clean bash shell environment
os.execv("/bin/bash", ["/bin/bash", "-l"])
