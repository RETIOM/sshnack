#include "api.h"
#include "router/router.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#define BUF_SIZE 256
#define METHOD_SIZE 8

void parseRequest(int sock, char *req, server_t *server);
static method_t parse_method(const char *s);

void init_api(void)    { init_router(); }
void destroy_api(void) { destroy_router(); }

void handle_client(void *c) {
    client_t *client = (client_t *) c;
    if (!client) {
        perror("could not create client");
        return;
    }
    
    int read = 0;
    char buf[BUF_SIZE];

    if ((read = recv(client->sock, buf, BUF_SIZE, 0)) <= 0) {
        perror("Client disconnected");
        close(client->sock);
        return;
    }

    parseRequest(client->sock, buf, client->ctx);

    close(client->sock);
    free(client);
}

void parseRequest(int sock, char *req, server_t *server) {
    // TODO: add proper parsing
}

static method_t parse_method(const char *s) {
    if (strcmp(s, "GET")    == 0) return GET;
    if (strcmp(s, "POST")   == 0) return POST;
    if (strcmp(s, "DELETE") == 0) return DELETE;
    return -1;
}