#include "db.h"
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

sqlite3 *initDB() {
    sqlite3 *db;
    char *err_msg = NULL;

    int rc = sqlite3_open("sshnack.db", &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "failed to open db: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return NULL;
    }

    const char *setup_sql =
        "PRAGMA journal_mode=WAL; "
        "PRAGMA foreign_keys=ON;";

    rc = sqlite3_exec(db, setup_sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "db setup failed: %s\n", err_msg);
        sqlite3_free(err_msg);
    }

    return db;
}

void closeDB(sqlite3 *db) {
    if (db) {
        sqlite3_close(db);
    }
}


int logCashTransaction(sqlite3 *db, int user_id, int amount_cents, const char *transaction_type) {
    sqlite3_stmt *stmt;
    const char *sql =
        "INSERT INTO transactions (type, user_id, total_amount) VALUES (?, ?, ?);";

    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "logCashTransaction prepare failed: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    sqlite3_bind_text(stmt, 1, transaction_type, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, user_id);
    sqlite3_bind_int(stmt, 3, amount_cents);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "logCashTransaction failed: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    return 0;
}

int logStockChange(sqlite3 *db, int slot_id, int change_qty, const char *change_type) {
    sqlite3_stmt *stmt;
    int rc, item_id;

    const char *sel_sql = "SELECT item_id FROM stock WHERE slot_id = ?;";
    rc = sqlite3_prepare_v2(db, sel_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "logStockChange prepare failed: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    sqlite3_bind_int(stmt, 1, slot_id);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        fprintf(stderr, "logStockChange: slot %d not found\n", slot_id);
        sqlite3_finalize(stmt);
        return -1;
    }
    item_id = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    const char *ins_sql =
        "INSERT INTO stock_logs (item_id, change_type, quantity_changed) VALUES (?, ?, ?);";
    rc = sqlite3_prepare_v2(db, ins_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "logStockChange insert prepare failed: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    sqlite3_bind_int(stmt, 1, item_id);
    sqlite3_bind_text(stmt, 2, change_type, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, change_qty);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "logStockChange insert failed: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    return 0;
}

int addBalance(sqlite3 *db, int user_id, int amount_gr) {
    sqlite3_stmt *stmt;
    int rc;

    if (amount_gr <= 0) {
        fprintf(stderr, "deposit amount must be positive\n");
        return -1;
    }

    rc = sqlite3_exec(db, "BEGIN IMMEDIATE TRANSACTION;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "failed to begin transaction: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    const char *sql =
        "INSERT INTO users (user_id, balance) VALUES (?, ?) "
        "ON CONFLICT(user_id) DO UPDATE SET balance = balance + excluded.balance;";

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }
    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_int(stmt, 2, amount_gr);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "failed to update balance: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }

    if (logCashTransaction(db, user_id, amount_gr, "deposit") != 0) {
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }

    rc = sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "failed to commit: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }
    return 0;
}

int getBalance(sqlite3 *db, int user_id, int *out_balance) {
    int rc;
    sqlite3_stmt *stmt;

    rc = sqlite3_exec(db, "BEGIN IMMEDIATE TRANSACTION;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "failed to begin transaction: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    const char *sql =
        "INSERT INTO users (user_id) VALUES (?) "
        "ON CONFLICT(user_id) DO UPDATE SET user_id = user_id "
        "RETURNING balance;";

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }
    sqlite3_bind_int(stmt, 1, user_id);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        fprintf(stderr, "failed to fetch balance: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }
    *out_balance = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    rc = sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "failed to commit: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }
    return 0;
}

int resetBalance(sqlite3 *db, int user_id, int *out_refund_amount) {
    sqlite3_stmt *stmt;
    int rc, balance;

    rc = sqlite3_exec(db, "BEGIN IMMEDIATE TRANSACTION;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "failed to begin transaction: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    const char *sel_sql = "SELECT balance FROM users WHERE user_id = ?;";
    rc = sqlite3_prepare_v2(db, sel_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "failed to prepare: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }
    sqlite3_bind_int(stmt, 1, user_id);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        fprintf(stderr, "user %d not found\n", user_id);
        sqlite3_finalize(stmt);
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }
    balance = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    if (balance == 0) {
        *out_refund_amount = 0;
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return 0;
    }

    const char *upd_sql = "UPDATE users SET balance = 0 WHERE user_id = ?;";
    rc = sqlite3_prepare_v2(db, upd_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "failed to prepare update: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }
    sqlite3_bind_int(stmt, 1, user_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "failed to zero balance: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }

    // withdrawal: total_amount < 0 (money leaving the system)
    if (logCashTransaction(db, user_id, -balance, "withdrawal") != 0) {
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }

    rc = sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "failed to commit: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }

    *out_refund_amount = balance;
    return 0;
}

int getAvailableProducts(sqlite3 *db) {
    sqlite3_stmt *stmt;
    int rc, count = 0;

    const char *sql =
        "SELECT s.slot_id, i.name, i.price, s.quantity "
        "FROM stock s JOIN items i ON s.item_id = i.item_id "
        "WHERE s.quantity > 0 "
        "ORDER BY s.slot_id;";

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "failed to prepare: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    printf("%-6s %-30s %8s %8s\n", "Slot", "Name", "Price", "Stock");
    printf("%-6s %-30s %8s %8s\n", "----", "----", "-----", "-----");

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        int slot_id      = sqlite3_column_int(stmt, 0);
        const char *name = (const char *)sqlite3_column_text(stmt, 1);
        int price        = sqlite3_column_int(stmt, 2);
        int quantity     = sqlite3_column_int(stmt, 3);
        printf("%-6d %-30s %8d %8d\n", slot_id, name, price, quantity);
        count++;
    }
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "query error: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    return count;
}

int getProductDetails(sqlite3 *db, int slot_id, product_t *out_product) {
    sqlite3_stmt *stmt;
    int rc;

    const char *sql =
        "SELECT s.slot_id, s.item_id, i.name, i.price, s.quantity "
        "FROM stock s JOIN items i ON s.item_id = i.item_id "
        "WHERE s.slot_id = ?;";

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "failed to prepare: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    sqlite3_bind_int(stmt, 1, slot_id);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        fprintf(stderr, "slot %d not found\n", slot_id);
        sqlite3_finalize(stmt);
        return -1;
    }

    out_product->slot_id   = sqlite3_column_int(stmt, 0);
    out_product->item_id   = sqlite3_column_int(stmt, 1);
    const char *name       = (const char *)sqlite3_column_text(stmt, 2);
    strncpy(out_product->name, name, MAX_NAME_LEN - 1);
    out_product->name[MAX_NAME_LEN - 1] = '\0';
    out_product->price_gr  = sqlite3_column_int(stmt, 3);
    out_product->stock_qty = sqlite3_column_int(stmt, 4);

    sqlite3_finalize(stmt);
    return 0;
}

int purchase(sqlite3 *db, int user_id, int slot_id) {
    sqlite3_stmt *stmt;
    int rc, price, item_id, quantity, balance;

    rc = sqlite3_exec(db, "BEGIN IMMEDIATE TRANSACTION;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "failed to begin transaction: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    // fetch item details for the requested slot
    const char *item_sql =
        "SELECT s.item_id, i.price, s.quantity "
        "FROM stock s JOIN items i ON s.item_id = i.item_id "
        "WHERE s.slot_id = ?;";
    rc = sqlite3_prepare_v2(db, item_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "failed to prepare: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }
    sqlite3_bind_int(stmt, 1, slot_id);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        fprintf(stderr, "slot %d not found\n", slot_id);
        sqlite3_finalize(stmt);
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }
    item_id  = sqlite3_column_int(stmt, 0);
    price    = sqlite3_column_int(stmt, 1);
    quantity = sqlite3_column_int(stmt, 2);
    sqlite3_finalize(stmt);

    if (quantity <= 0) {
        fprintf(stderr, "item out of stock\n");
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }

    // fetch user balance
    const char *bal_sql = "SELECT balance FROM users WHERE user_id = ?;";
    rc = sqlite3_prepare_v2(db, bal_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "failed to prepare: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }
    sqlite3_bind_int(stmt, 1, user_id);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        fprintf(stderr, "user %d not found\n", user_id);
        sqlite3_finalize(stmt);
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }
    balance = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    if (balance < price) {
        fprintf(stderr, "insufficient balance: have %d, need %d\n", balance, price);
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }

    // deduct balance
    const char *upd_bal = "UPDATE users SET balance = balance - ? WHERE user_id = ?;";
    rc = sqlite3_prepare_v2(db, upd_bal, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "failed to prepare: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }
    sqlite3_bind_int(stmt, 1, price);
    sqlite3_bind_int(stmt, 2, user_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "failed to deduct balance: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }

    // deduct stock
    const char *upd_stock = "UPDATE stock SET quantity = quantity - 1 WHERE slot_id = ?;";
    rc = sqlite3_prepare_v2(db, upd_stock, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "failed to prepare: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }
    sqlite3_bind_int(stmt, 1, slot_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "failed to deduct stock: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }

    // log transaction
    const char *trx_sql =
        "INSERT INTO transactions (type, user_id, item_id, quantity, total_amount) "
        "VALUES ('purchase', ?, ?, 1, ?);";
    rc = sqlite3_prepare_v2(db, trx_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "failed to prepare: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }
    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_int(stmt, 2, item_id);
    sqlite3_bind_int(stmt, 3, -price);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "failed to log transaction: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }

    // log stock removal
    if (logStockChange(db, slot_id, -1, "removal") != 0) {
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }

    rc = sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "failed to commit: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }
    return 0;
}

int restock(sqlite3 *db, int slot_id, int quantity_added) {
    sqlite3_stmt *stmt;
    int rc;

    if (quantity_added <= 0) {
        fprintf(stderr, "restock quantity must be positive\n");
        return -1;
    }

    rc = sqlite3_exec(db, "BEGIN IMMEDIATE TRANSACTION;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "failed to begin transaction: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    const char *sql = "UPDATE stock SET quantity = quantity + ? WHERE slot_id = ?;";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "failed to prepare: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }
    sqlite3_bind_int(stmt, 1, quantity_added);
    sqlite3_bind_int(stmt, 2, slot_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "failed to restock: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }

    if (logStockChange(db, slot_id, quantity_added, "addition") != 0) {
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }

    rc = sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "failed to commit: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }
    return 0;
}

int updateItemPrice(sqlite3 *db, int item_id, int new_price_gr) {
    sqlite3_stmt *stmt;
    int rc;

    if (new_price_gr <= 0) {
        fprintf(stderr, "price must be positive\n");
        return -1;
    }

    const char *sql = "UPDATE items SET price = ? WHERE item_id = ?;";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "failed to prepare: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    sqlite3_bind_int(stmt, 1, new_price_gr);
    sqlite3_bind_int(stmt, 2, item_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "failed to update price: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    return 0;
}
