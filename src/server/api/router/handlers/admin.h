#ifndef ADMIN_H
#define ADMIN_H

#include "../router.h"

void handle_post_restock(int sock, server_t *server, request_t *req);
void handle_post_update_price(int sock, server_t *server, request_t *req);

#endif /* ADMIN_H */
