#ifndef ORDERS_H
#define ORDERS_H

#include "router.h"

void handle_post_purchase(int sock, server_t *server, request_t *req);

#endif /* ORDERS_H */
