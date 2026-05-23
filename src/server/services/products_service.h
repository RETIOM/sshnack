#ifndef PRODUCTS_SERVICE_H
#define PRODUCTS_SERVICE_H

#include <sqlite3.h>
#include "../db/db.h"

int products_list(sqlite3 *db, product_t **out, int *out_count);
int product_get(sqlite3 *db, int slot_id, product_t *out);
int product_restock(sqlite3 *db, int slot_id, int qty);
int product_update_price(sqlite3 *db, int item_id, int price_gr);

#endif /* PRODUCTS_SERVICE_H */
