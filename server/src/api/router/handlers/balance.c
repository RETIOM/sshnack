#include "balance.h"
#include "balance_service.h"
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
        status == 404 ? "404 Not Found" :
        "500 Internal Server Error";
    int len = snprintf(resp, sizeof(resp),
        "HTTP/1.1 %s\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n\r\n%s",
        status_str, strlen(body), body);
    send(sock, resp, len, 0);
}

void handle_get_balance(int sock, server_t *server, request_t *req) {
    if (req->auth.role == AUTH_NONE) {
        send_json(sock, 401, "{\"error\":\"unauthorized\"}");
        return;
    }
    int balance_gr;
    int rc = balance_get(server->db, req->auth.user_id, &balance_gr);
    if (rc == BALANCE_NOT_FOUND) {
        send_json(sock, 404, "{\"error\":\"user not found\"}");
        return;
    }
    if (rc != 0) {
        send_json(sock, 500, "{\"error\":\"internal error\"}");
        return;
    }
    char body[64];
    snprintf(body, sizeof(body), "{\"balance_gr\":%d}", balance_gr);
    send_json(sock, 200, body);
}

void handle_post_deposit(int sock, server_t *server, request_t *req) {
    if (req->auth.role == AUTH_NONE) {
        send_json(sock, 401, "{\"error\":\"unauthorized\"}");
        return;
    }
    int amount_gr;
    if (sscanf(req->body, "{\"amount_gr\":%d}", &amount_gr) != 1) {
        send_json(sock, 400, "{\"error\":\"missing amount_gr\"}");
        return;
    }
    if (balance_deposit(server->db, req->auth.user_id, amount_gr) != 0) {
        send_json(sock, 500, "{\"error\":\"internal error\"}");
        return;
    }
    send_json(sock, 200, "{\"ok\":true}");
}

void handle_delete_balance(int sock, server_t *server, request_t *req) {
    if (req->auth.role == AUTH_NONE) {
        send_json(sock, 401, "{\"error\":\"unauthorized\"}");
        return;
    }
    int refund_gr;
    int rc = balance_reset(server->db, req->auth.user_id, &refund_gr);
    if (rc == BALANCE_NOT_FOUND) {
        send_json(sock, 404, "{\"error\":\"user not found\"}");
        return;
    }
    if (rc != 0) {
        send_json(sock, 500, "{\"error\":\"internal error\"}");
        return;
    }
    char body[64];
    snprintf(body, sizeof(body), "{\"refund_gr\":%d}", refund_gr);
    send_json(sock, 200, body);
}
