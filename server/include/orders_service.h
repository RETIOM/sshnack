#ifndef ORDERS_SERVICE_H
#define ORDERS_SERVICE_H

#include <sqlite3.h>

int orders_purchase(sqlite3 *db, int user_id, int slot_id);

#endif /* ORDERS_SERVICE_H */
