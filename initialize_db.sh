#!/usr/bin/env bash
# Usage: initialize_db.sh [DB_PATH]
#   DB_PATH  path to the SQLite database file (default: data/sshnack.db)

DB="${1:-data/sshnack.db}"
sqlite3 "$DB" < init.sql
