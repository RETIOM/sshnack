#include "balance_service.h"
#include "../db/db.h"
#include <stdio.h>

int balance_get(sqlite3 *db, int user_id, int *out_balance_gr) {
    sqlite3_stmt *stmt;
    int rc;

    rc = sqlite3_exec(db, "BEGIN IMMEDIATE TRANSACTION;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "balance_get: begin transaction failed: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    const char *sql =
        "INSERT INTO users (user_id) VALUES (?) "
        "ON CONFLICT(user_id) DO UPDATE SET user_id = user_id "
        "RETURNING balance;";

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "balance_get: prepare failed: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }
    sqlite3_bind_int(stmt, 1, user_id);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        fprintf(stderr, "balance_get: fetch failed: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }
    *out_balance_gr = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    rc = sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "balance_get: commit failed: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }
    return 0;
}

int balance_deposit(sqlite3 *db, int user_id, int amount_gr) {
    sqlite3_stmt *stmt;
    int rc;

    if (amount_gr <= 0) {
        fprintf(stderr, "balance_deposit: amount must be positive\n");
        return -1;
    }

    rc = sqlite3_exec(db, "BEGIN IMMEDIATE TRANSACTION;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "balance_deposit: begin transaction failed: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    const char *sql =
        "INSERT INTO users (user_id, balance) VALUES (?, ?) "
        "ON CONFLICT(user_id) DO UPDATE SET balance = balance + excluded.balance;";

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "balance_deposit: prepare failed: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }
    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_int(stmt, 2, amount_gr);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "balance_deposit: update failed: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }

    if (db_log_cash_transaction(db, user_id, amount_gr, "deposit") != 0) {
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }

    rc = sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "balance_deposit: commit failed: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }
    return 0;
}

int balance_reset(sqlite3 *db, int user_id, int *out_refund_gr) {
    sqlite3_stmt *stmt;
    int rc, balance;

    rc = sqlite3_exec(db, "BEGIN IMMEDIATE TRANSACTION;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "balance_reset: begin transaction failed: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    const char *sel_sql = "SELECT balance FROM users WHERE user_id = ?;";
    rc = sqlite3_prepare_v2(db, sel_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "balance_reset: prepare failed: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }
    sqlite3_bind_int(stmt, 1, user_id);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        fprintf(stderr, "balance_reset: user %d not found\n", user_id);
        sqlite3_finalize(stmt);
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }
    balance = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    if (balance == 0) {
        *out_refund_gr = 0;
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return 0;
    }

    const char *upd_sql = "UPDATE users SET balance = 0 WHERE user_id = ?;";
    rc = sqlite3_prepare_v2(db, upd_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "balance_reset: update prepare failed: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }
    sqlite3_bind_int(stmt, 1, user_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "balance_reset: zero balance failed: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }

    if (db_log_cash_transaction(db, user_id, -balance, "withdrawal") != 0) {
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }

    rc = sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "balance_reset: commit failed: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }

    *out_refund_gr = balance;
    return 0;
}
