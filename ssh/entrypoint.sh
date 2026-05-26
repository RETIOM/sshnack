#!/bin/sh
# Populates environment variables for ssh child processes, then executes the command passed as arguments to the container

set -e
echo "SSHNACK_SERVER_URL=$SSHNACK_SERVER_URL" >> /etc/environment
echo "SSHNACK_INTERNAL_URL=$SSHNACK_INTERNAL_URL" >> /etc/environment
echo "SSHNACK_USER_ID=$SSHNACK_USER_ID" >> /etc/environment
echo "SSHNACK_ADMIN_TOKEN=$SSHNACK_ADMIN_TOKEN" >> /etc/environment

exec "$@"