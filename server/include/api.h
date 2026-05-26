#ifndef API_H
#define API_H

#include <sqlite3.h>

typedef struct server_t {
    sqlite3 *db;
    const char *admin_token;
} server_t;

typedef struct client_t {
    int sock;
    int internal;
    server_t *ctx;
} client_t;

void init_api(void);
void destroy_api(void);
void handle_client(void *c);


#endif /* API_H */