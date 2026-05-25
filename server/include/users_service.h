#ifndef USERS_SERVICE_H
#define USERS_SERVICE_H

#include <sqlite3.h>

int users_lookup(sqlite3 *db, const char *fingerprint);

#endif /* USERS_SERVICE_H */
