#include "db.h"
#include <stdio.h>

sqlite3 *db_init(void) {
    sqlite3 *db;

    int rc = sqlite3_open("sshnack.db", &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "db_init: failed to open: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return NULL;
    }

    char *err_msg = NULL;
    const char *setup_sql =
        "PRAGMA journal_mode=WAL; "
        "PRAGMA foreign_keys=ON;";

    rc = sqlite3_exec(db, setup_sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "db_init: setup failed: %s\n", err_msg);
        sqlite3_free(err_msg);
    }

    return db;
}

void db_close(sqlite3 *db) {
    if (db)
        sqlite3_close(db);
}

int db_log_cash_transaction(sqlite3 *db, int user_id, int amount_gr, const char *type) {
    sqlite3_stmt *stmt;
    const char *sql =
        "INSERT INTO transactions (type, user_id, total_amount) VALUES (?, ?, ?);";

    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "db_log_cash_transaction: prepare failed: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    sqlite3_bind_text(stmt, 1, type, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, user_id);
    sqlite3_bind_int(stmt, 3, amount_gr);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "db_log_cash_transaction: insert failed: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    return 0;
}

int db_log_stock_change(sqlite3 *db, int slot_id, int change_qty, const char *type) {
    sqlite3_stmt *stmt;
    int rc, item_id;

    const char *sel_sql = "SELECT item_id FROM stock WHERE slot_id = ?;";
    rc = sqlite3_prepare_v2(db, sel_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "db_log_stock_change: prepare failed: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    sqlite3_bind_int(stmt, 1, slot_id);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        fprintf(stderr, "db_log_stock_change: slot %d not found\n", slot_id);
        sqlite3_finalize(stmt);
        return -1;
    }
    item_id = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    const char *ins_sql =
        "INSERT INTO stock_logs (item_id, change_type, quantity_changed) VALUES (?, ?, ?);";
    rc = sqlite3_prepare_v2(db, ins_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "db_log_stock_change: insert prepare failed: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    sqlite3_bind_int(stmt, 1, item_id);
    sqlite3_bind_text(stmt, 2, type, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, change_qty);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "db_log_stock_change: insert failed: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    return 0;
}
