#!/bin/sh
# seeds the db if it doesn't exist, then executes the command passed as arguments to the container

set -e
DB=/data/sshnack.db
if [ ! -f "$DB" ]; then
    sqlite3 "$DB" < /usr/local/app/init.sql
fi
exec "$@"
