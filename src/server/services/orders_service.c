#include "orders_service.h"
#include "../db/db.h"
#include <stdio.h>

int orders_purchase(sqlite3 *db, int user_id, int slot_id) {
    sqlite3_stmt *stmt;
    int rc, price, item_id, quantity, balance;

    rc = sqlite3_exec(db, "BEGIN IMMEDIATE TRANSACTION;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "orders_purchase: begin transaction failed: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    const char *item_sql =
        "SELECT s.item_id, i.price, s.quantity "
        "FROM stock s JOIN items i ON s.item_id = i.item_id "
        "WHERE s.slot_id = ?;";
    rc = sqlite3_prepare_v2(db, item_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "orders_purchase: prepare failed: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }
    sqlite3_bind_int(stmt, 1, slot_id);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        fprintf(stderr, "orders_purchase: slot %d not found\n", slot_id);
        sqlite3_finalize(stmt);
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }
    item_id  = sqlite3_column_int(stmt, 0);
    price    = sqlite3_column_int(stmt, 1);
    quantity = sqlite3_column_int(stmt, 2);
    sqlite3_finalize(stmt);

    if (quantity <= 0) {
        fprintf(stderr, "orders_purchase: slot %d out of stock\n", slot_id);
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }

    const char *bal_sql = "SELECT balance FROM users WHERE user_id = ?;";
    rc = sqlite3_prepare_v2(db, bal_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "orders_purchase: prepare failed: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }
    sqlite3_bind_int(stmt, 1, user_id);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        fprintf(stderr, "orders_purchase: user %d not found\n", user_id);
        sqlite3_finalize(stmt);
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }
    balance = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    if (balance < price) {
        fprintf(stderr, "orders_purchase: insufficient balance: have %d, need %d\n", balance, price);
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }

    const char *upd_bal = "UPDATE users SET balance = balance - ? WHERE user_id = ?;";
    rc = sqlite3_prepare_v2(db, upd_bal, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }
    sqlite3_bind_int(stmt, 1, price);
    sqlite3_bind_int(stmt, 2, user_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "orders_purchase: deduct balance failed: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }

    const char *upd_stock = "UPDATE stock SET quantity = quantity - 1 WHERE slot_id = ?;";
    rc = sqlite3_prepare_v2(db, upd_stock, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }
    sqlite3_bind_int(stmt, 1, slot_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "orders_purchase: deduct stock failed: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }

    const char *trx_sql =
        "INSERT INTO transactions (type, user_id, item_id, quantity, total_amount) "
        "VALUES ('purchase', ?, ?, 1, ?);";
    rc = sqlite3_prepare_v2(db, trx_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }
    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_int(stmt, 2, item_id);
    sqlite3_bind_int(stmt, 3, -price);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "orders_purchase: log transaction failed: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }

    if (db_log_stock_change(db, slot_id, -1, "removal") != 0) {
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }

    rc = sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "orders_purchase: commit failed: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }
    return 0;
}
