#ifndef API_H
#define API_H

#include <sqlite3.h>

typedef struct server_t {
    sqlite3 *db;
} server_t;

typedef struct client_t {
    int sock;
    server_t *ctx;
} client_t;

void handleClient(void* c);


#endif /* API_H */