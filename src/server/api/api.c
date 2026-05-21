#include "api.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>

#define BUF_SIZE 256

void parseRequest(int sock, char *req, server_t *server);

void handleClient(void *c) {
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
    printf("%s\n", req);

}