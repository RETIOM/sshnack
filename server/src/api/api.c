#include "api.h"
#include "auth.h"
#include "router.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>

#define BUF_SIZE 2048

static method_t   parse_method(const char *s);
static request_t  parse_request(const char *raw, server_t *server);

void init_api(void)    { init_router(); }
void destroy_api(void) { destroy_router(); }

void handle_client(void *c) {
    client_t *client = (client_t *) c;
    if (!client) return;

    char buf[BUF_SIZE];
    int nread = recv(client->sock, buf, BUF_SIZE - 1, 0);
    if (nread <= 0) {
        close(client->sock);
        free(client);
        return;
    }
    buf[nread] = '\0';

    request_t req = parse_request(buf, client->ctx);

    if (req.method == (method_t)-1) {
        const char *bad = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
        send(client->sock, bad, strlen(bad), 0);
    } else {
        dispatch(client->sock, client->ctx, &req, client->internal);
    }

    close(client->sock);
    free(client);
}

static request_t parse_request(const char *raw, server_t *server) {
    request_t req = {0};
    req.method = (method_t)-1;

    char method_str[16] = {0};
    if (sscanf(raw, "%15s %63s", method_str, req.path) != 2) return req;

    req.method = parse_method(method_str);

    const char *p = strstr(raw, "\r\n");
    if (!p) return req;
    p += 2;

    char bearer[128] = {0};
    while (*p) {
        if (p[0] == '\r' && p[1] == '\n') {
            p += 2;
            break;
        }

        const char *line_end = strstr(p, "\r\n");
        if (!line_end) break;

        if (strncasecmp(p, "Authorization: Bearer ", 22) == 0) {
            size_t tlen = (size_t)(line_end - (p + 22));
            if (tlen >= sizeof(bearer)) tlen = sizeof(bearer) - 1;
            memcpy(bearer, p + 22, tlen);
            bearer[tlen] = '\0';
        }

        p = line_end + 2;
    }

    strncpy(req.body, p, MAX_BODY_LEN - 1);
    req.body[MAX_BODY_LEN - 1] = '\0';

    req.auth = auth_parse(bearer, server->admin_token);

    return req;
}

static method_t parse_method(const char *s) {
    if (strcmp(s, "GET")    == 0) return GET;
    if (strcmp(s, "POST")   == 0) return POST;
    if (strcmp(s, "PATCH")  == 0) return PATCH;
    if (strcmp(s, "DELETE") == 0) return DELETE;
    return (method_t)-1;
}
