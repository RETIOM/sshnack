#!/bin/sh
# Seeds the db if it doesn't exist, then starts the server.
# Set SSHNACK_REINIT_DB=1 to force-drop and re-seed the db on startup.

set -e
DB=/data/sshnack.db
if [ "${SSHNACK_REINIT_DB}" = "1" ] || [ ! -f "$DB" ]; then
    rm -f "$DB"
    sqlite3 "$DB" < /usr/local/app/init.sql
fi
exec "$@"
