#include "products_service.h"
#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int products_list(sqlite3 *db, product_t **out, int *out_count) {
    sqlite3_stmt *stmt;
    int rc;

    const char *sql =
        "SELECT s.slot_id, s.item_id, i.name, i.price, s.quantity "
        "FROM stock s JOIN items i ON s.item_id = i.item_id "
        "ORDER BY s.slot_id;";

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "products_list: prepare failed: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    int capacity = 16, count = 0;
    product_t *products = malloc(capacity * sizeof(product_t));
    if (!products) {
        sqlite3_finalize(stmt);
        return -1;
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (count == capacity) {
            capacity *= 2;
            product_t *tmp = realloc(products, capacity * sizeof(product_t));
            if (!tmp) {
                free(products);
                sqlite3_finalize(stmt);
                return -1;
            }
            products = tmp;
        }
        products[count].slot_id = sqlite3_column_int(stmt, 0);
        products[count].item_id = sqlite3_column_int(stmt, 1);
        const char *name = (const char *)sqlite3_column_text(stmt, 2);
        strncpy(products[count].name, name, MAX_NAME_LEN - 1);
        products[count].name[MAX_NAME_LEN - 1] = '\0';
        products[count].price_gr = sqlite3_column_int(stmt, 3);
        products[count].stock_qty = sqlite3_column_int(stmt, 4);
        count++;
    }
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "products_list: query error: %s\n", sqlite3_errmsg(db));
        free(products);
        return -1;
    }

    *out = products;
    *out_count = count;
    return 0;
}

int product_get(sqlite3 *db, int slot_id, product_t *out) {
    sqlite3_stmt *stmt;
    int rc;

    const char *sql =
        "SELECT s.slot_id, s.item_id, i.name, i.price, s.quantity "
        "FROM stock s JOIN items i ON s.item_id = i.item_id "
        "WHERE s.slot_id = ?;";

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "product_get: prepare failed: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    sqlite3_bind_int(stmt, 1, slot_id);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return -1;
    }

    out->slot_id = sqlite3_column_int(stmt, 0);
    out->item_id = sqlite3_column_int(stmt, 1);
    const char *name = (const char *)sqlite3_column_text(stmt, 2);
    strncpy(out->name, name, MAX_NAME_LEN - 1);
    out->name[MAX_NAME_LEN - 1] = '\0';
    out->price_gr = sqlite3_column_int(stmt, 3);
    out->stock_qty = sqlite3_column_int(stmt, 4);

    sqlite3_finalize(stmt);
    return 0;
}

int product_restock(sqlite3 *db, int slot_id, int qty) {
    sqlite3_stmt *stmt;
    int rc;

    if (qty <= 0) {
        fprintf(stderr, "product_restock: quantity must be positive\n");
        return -1;
    }

    rc = sqlite3_exec(db, "BEGIN IMMEDIATE TRANSACTION;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "product_restock: begin transaction failed: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    const char *sql = "UPDATE stock SET quantity = quantity + ? WHERE slot_id = ?;";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "product_restock: prepare failed: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }
    sqlite3_bind_int(stmt, 1, qty);
    sqlite3_bind_int(stmt, 2, slot_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "product_restock: update failed: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }

    if (db_log_stock_change(db, slot_id, qty, "addition") != 0) {
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }

    rc = sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "product_restock: commit failed: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }
    return 0;
}

int product_update_price(sqlite3 *db, int item_id, int price_gr) {
    sqlite3_stmt *stmt;
    int rc;

    if (price_gr <= 0) {
        fprintf(stderr, "product_update_price: price must be positive\n");
        return -1;
    }

    const char *sql = "UPDATE items SET price = ? WHERE item_id = ?;";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "product_update_price: prepare failed: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    sqlite3_bind_int(stmt, 1, price_gr);
    sqlite3_bind_int(stmt, 2, item_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "product_update_price: update failed: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    return 0;
}
