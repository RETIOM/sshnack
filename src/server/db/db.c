#include "db.h"
#include <sqlite3.h>
#include <stdio.h>

sqlite3 *initDB() {
    sqlite3 *db;

    int rc = sqlite3_open("sshnack.db", &db);
    if (rc < 0) {
        perror("failed to open db");
        sqlite3_close(db);
        return NULL;
    }
    return db;
}

void closeDB(sqlite3 *db) {
    if (db) {
        sqlite3_close(db);
    }
}