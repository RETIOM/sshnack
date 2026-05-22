// Handles db interactions
#ifndef DB_H
#define DB_H

#include <sqlite3.h>

#define MAX_NAME_LEN 50

sqlite3 *initDB();
void closeDB(sqlite3 *db);

typedef struct {
    int slot_id;
    int item_id;
    char name[MAX_NAME_LEN];
    int price_gr;
    int stock_qty;
} product_t;

// --- Session & Wallet Management ---

int addBalance(sqlite3 *db, int user_id, int amount_gr);
int getBalance(sqlite3 *db, int user_id, int *out_balance);
int resetBalance(sqlite3 *db, int user_id, int *out_refund_amount);

// --- Display & Inventory ---

int getAvailableProducts(sqlite3 *db);
int getProductDetails(sqlite3 *db, int slot_id, product_t *out_product);

// --- Core Transaction ---

int purchase(sqlite3 *db, int user_id, int slot_id);

// --- Admin & Operations ---

int restock(sqlite3 *db, int slot_id, int quantity_added);
int updateItemPrice(sqlite3 *db, int item_id, int new_price_gr);

// --- Internal Logging Helpers ---

int logCashTransaction(sqlite3 *db, int user_id, int amount_cents, const char *transaction_type);
int logStockChange(sqlite3 *db, int slot_id, int change_qty, const char *change_type);

#endif /* DB_H */