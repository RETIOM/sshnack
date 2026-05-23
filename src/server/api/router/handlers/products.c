#include "products.h"
#include "products_service.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

static void send_json(int sock, int status, const char *body) {
    char resp[8192];
    const char *status_str =
        status == 200 ? "200 OK" :
        status == 404 ? "404 Not Found" :
        "500 Internal Server Error";
    int len = snprintf(resp, sizeof(resp),
        "HTTP/1.1 %s\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n\r\n%s",
        status_str, strlen(body), body);
    send(sock, resp, len, 0);
}

void handle_get_stock(int sock, server_t *server, request_t *req) {
    product_t *products = NULL;
    int count = 0;

    if (products_list(server->db, &products, &count) != 0) {
        send_json(sock, 500, "{\"error\":\"internal error\"}");
        return;
    }

    char *body = malloc(count * 128 + 8);
    if (!body) {
        free(products);
        send_json(sock, 500, "{\"error\":\"internal error\"}");
        return;
    }

    int pos = 0;
    pos += sprintf(body + pos, "[");
    for (int i = 0; i < count; i++) {
        pos += sprintf(body + pos,
            "%s{\"slot_id\":%d,\"item_id\":%d,\"name\":\"%s\","
            "\"price_gr\":%d,\"stock_qty\":%d}",
            i > 0 ? "," : "",
            products[i].slot_id, products[i].item_id, products[i].name,
            products[i].price_gr, products[i].stock_qty);
    }
    sprintf(body + pos, "]");

    free(products);
    send_json(sock, 200, body);
    free(body);
}
