#ifndef USER_H
#define USER_H

#include "router.h"

void handle_lookup_user(int sock, server_t *server, request_t *req);

#endif /* USER_H */