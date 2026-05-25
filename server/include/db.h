#ifndef DB_H
#define DB_H

#include <sqlite3.h>

#define MAX_NAME_LEN 50

typedef struct {
    int slot_id;
    int item_id;
    char name[MAX_NAME_LEN];
    int price_gr;
    int stock_qty;
} product_t;

sqlite3 *db_init(const char *path);
void     db_close(sqlite3 *db);

int db_log_cash_transaction(sqlite3 *db, int user_id, int amount_gr, const char *type);
int db_log_stock_change(sqlite3 *db, int slot_id, int change_qty, const char *type);

#endif /* DB_H */
