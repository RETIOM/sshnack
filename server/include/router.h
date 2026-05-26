#ifndef ROUTER_H
#define ROUTER_H

#include "api.h"
#include "auth.h"
#include "radix_tree.h"

#define MAX_PATH_LEN 64
#define MAX_BODY_LEN 512

typedef enum { GET, POST, PATCH, DELETE } method_t;

typedef struct {
    method_t method;
    char     path[MAX_PATH_LEN];
    char     body[MAX_BODY_LEN];
    auth_t   auth;
} request_t;

typedef void (*handler_fn)(int sock, server_t *server, request_t *req);

typedef struct {
    handler_fn get;
    handler_fn post;
    handler_fn patch;
    handler_fn del;
    int internal;
} handler_t;

void init_router(void);
void destroy_router(void);
void dispatch(int sock, server_t *server, request_t *req, int internal);

#endif /* ROUTER_H */
