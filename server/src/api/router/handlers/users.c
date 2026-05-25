#include "users.h"
#include "users_service.h"
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>

#define MAX_FINGERPRINT_SIZE 128

static void send_json(int sock, int status, const char *body) {
    char resp[512];
    const char *status_str =
        status == 200 ? "200 OK" :
        status == 400 ? "400 Bad Request" :
        "500 Internal Server Error";
    int len = snprintf(resp, sizeof(resp),
        "HTTP/1.1 %s\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n\r\n%s",
        status_str, strlen(body), body);
    send(sock, resp, len, 0);
}

void handle_lookup_user(int sock, server_t *server, request_t *req) {
    char fingerprint[MAX_FINGERPRINT_SIZE];
    char body[64];
    int user_id;

    printf("RESPONSE RECEIVED AND ROUTED");

    if (sscanf(req->body, " {\"fingerprint\" : \"%127[^\"]\" }", fingerprint) != 1) {
        send_json(sock, 400, "{\"error\":\"missing fingerprint\"}");
        return;
    }
    if ((user_id=users_lookup(server->db, fingerprint)) == -1) {
        send_json(sock,  500, "{\"error\":\"lookup failed\"}");
        return;
    };
    snprintf(body, sizeof(body), "{\"user_id\":%d}", user_id);
    send_json(sock, 200, body);
}