#include "debug.h"
#include <unistd.h>
#include <sys/socket.h>
#include <string.h>

void handle_get_slow(int sock, server_t *server, request_t *req) {
    (void)server; (void)req;
    sleep(10);
    const char *resp =
        "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 2\r\n\r\nok";
    send(sock, resp, strlen(resp), 0);
}
