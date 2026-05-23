#include "admin.h"
#include "products_service.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

static void send_json(int sock, int status, const char *body) {
    char resp[512];
    const char *status_str =
        status == 200 ? "200 OK" :
        status == 400 ? "400 Bad Request" :
        status == 403 ? "403 Forbidden" :
        "500 Internal Server Error";
    int len = snprintf(resp, sizeof(resp),
        "HTTP/1.1 %s\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n\r\n%s",
        status_str, strlen(body), body);
    send(sock, resp, len, 0);
}

void handle_post_restock(int sock, server_t *server, request_t *req) {
    if (req->auth.role != AUTH_ADMIN) {
        send_json(sock, 403, "{\"error\":\"forbidden\"}");
        return;
    }

    int slot_id;
    if (sscanf(req->path, "/stock/%d", &slot_id) != 1) {
        send_json(sock, 400, "{\"error\":\"invalid path\"}");
        return;
    }

    int qty;
    if (sscanf(req->body, "{\"qty\":%d}", &qty) != 1) {
        send_json(sock, 400, "{\"error\":\"missing qty\"}");
        return;
    }

    if (product_restock(server->db, slot_id, qty) != 0) {
        send_json(sock, 500, "{\"error\":\"restock failed\"}");
        return;
    }
    send_json(sock, 200, "{\"ok\":true}");
}

void handle_post_update_price(int sock, server_t *server, request_t *req) {
    if (req->auth.role != AUTH_ADMIN) {
        send_json(sock, 403, "{\"error\":\"forbidden\"}");
        return;
    }

    int item_id;
    if (sscanf(req->path, "/items/%d", &item_id) != 1) {
        send_json(sock, 400, "{\"error\":\"invalid path\"}");
        return;
    }

    int price_gr;
    if (sscanf(req->body, "{\"price_gr\":%d}", &price_gr) != 1) {
        send_json(sock, 400, "{\"error\":\"missing price_gr\"}");
        return;
    }

    if (product_update_price(server->db, item_id, price_gr) != 0) {
        send_json(sock, 500, "{\"error\":\"update failed\"}");
        return;
    }
    send_json(sock, 200, "{\"ok\":true}");
}
