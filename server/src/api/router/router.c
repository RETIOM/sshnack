#include "router.h"
#include "products.h"
#include "balance.h"
#include "orders.h"
#include "admin.h"
#include "debug.h"
#include "users.h"
#include <string.h>
#include <sys/socket.h>

static radix_tree_t *tree;

static void register_route(char *path, handler_t *handler) {
    add_node(tree, path, handler);
}

void init_router(void) {
    tree = init_tree('/', ':');

    static handler_t stock_handler = {
        .get = handle_get_stock,
    };
    register_route("/stock", &stock_handler);

    static handler_t stock_slot_handler = {
        .patch = handle_post_restock,
    };
    register_route("/stock/:slot_id", &stock_slot_handler);

    static handler_t balance_handler = {
        .get = handle_get_balance,
        .post = handle_post_deposit,
        .del = handle_delete_balance,
    };
    register_route("/balance", &balance_handler);

    static handler_t orders_handler = {
        .post = handle_post_purchase,
    };
    register_route("/orders", &orders_handler);

    static handler_t items_handler = {
        .patch = handle_post_update_price,
    };
    register_route("/items/:item_id", &items_handler);

    static handler_t debug_handler = {
        .get = handle_get_slow,
    };
    register_route("/debug/slow", &debug_handler);

    static handler_t user_lookup_handler = {
        .post = handle_lookup_user,
    };
    register_route("/users/lookup", &user_lookup_handler);
}

void destroy_router(void) {
    destroy_tree(tree);
}

void dispatch(int sock, server_t *server, request_t *req) {
    handler_t *h = (handler_t *) search_node(tree, req->path);

    if (!h) {
        const char *resp = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        send(sock, resp, strlen(resp), 0);
        return;
    }

    handler_fn fn = NULL;
    switch (req->method) {
        case GET:    fn = h->get;   break;
        case POST:   fn = h->post;  break;
        case PATCH:  fn = h->patch; break;
        case DELETE: fn = h->del;   break;
    }

    if (!fn) {
        const char *resp = "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 0\r\n\r\n";
        send(sock, resp, strlen(resp), 0);
        return;
    }

    fn(sock, server, req);
}
