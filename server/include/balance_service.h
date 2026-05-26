#ifndef BALANCE_SERVICE_H
#define BALANCE_SERVICE_H

#include <sqlite3.h>

#define BALANCE_NOT_FOUND -2

int balance_get(sqlite3 *db, int user_id, int *out_balance_gr);
int balance_deposit(sqlite3 *db, int user_id, int amount_gr);
int balance_reset(sqlite3 *db, int user_id, int *out_refund_gr);

#endif /* BALANCE_SERVICE_H */
