#ifndef BALANCE_H
#define BALANCE_H

#include "router.h"

void handle_get_balance(int sock, server_t *server, request_t *req);
void handle_post_deposit(int sock, server_t *server, request_t *req);
void handle_delete_balance(int sock, server_t *server, request_t *req);

#endif /* BALANCE_H */
