#include "orders.h"
#include "orders_service.h"
#include "auth.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

static void send_json(int sock, int status, const char *body) {
    char resp[512];
    const char *status_str =
        status == 200 ? "200 OK" :
        status == 400 ? "400 Bad Request" :
        status == 401 ? "401 Unauthorized" :
        "500 Internal Server Error";
    int len = snprintf(resp, sizeof(resp),
        "HTTP/1.1 %s\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n\r\n%s",
        status_str, strlen(body), body);
    send(sock, resp, len, 0);
}

void handle_post_purchase(int sock, server_t *server, request_t *req) {
    if (req->auth.role == AUTH_NONE) {
        send_json(sock, 401, "{\"error\":\"unauthorized\"}");
        return;
    }
    int slot_id;
    if (sscanf(req->body, "{\"slot_id\":%d}", &slot_id) != 1) {
        send_json(sock, 400, "{\"error\":\"missing slot_id\"}");
        return;
    }
    if (orders_purchase(server->db, req->auth.user_id, slot_id) != 0) {
        send_json(sock, 500, "{\"error\":\"purchase failed\"}");
        return;
    }
    send_json(sock, 200, "{\"ok\":true}");
}
