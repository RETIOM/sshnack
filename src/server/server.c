// 1. accept connection
// 2. read raw HTTP request
// 3. enqueue request/socket
// 4. worker thread wakes up
// 5. parse HTTP
// 6. route request
// 7. call business logic
// 8. send response
// 9. close connection

// handleConnection?

#include "db.h"
#include "api.h"
#include "threadpool.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <signal.h>
#include <unistd.h>


#define BUF_SIZE 512
#define PORT 8080
#define MAX 10


volatile sig_atomic_t keep_running = 1;


struct sockaddr_in serverAddr, clientAddr;




int create_socket(int port, int maxClients);
void handle_sigint(int sig) { keep_running = 0; }


int main(int argc, char* argv[]) {
    struct sigaction sa = {0};
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; /* no SA_RESTART — accept() must be interruptible */
    sigaction(SIGINT, &sa, NULL);

    const char *admin_token = getenv("SSHNACK_ADMIN_TOKEN");
    if (!admin_token) {
        admin_token = "admin";
        fprintf(stderr, "SSHNACK_ADMIN_TOKEN not set, using default\n");
    }

    server_t server;

    server.db          = db_init();
    server.admin_token = admin_token;
    if (!server.db) {
        perror("db init failed");
        return -1;
    }

    init_api();

    tpool_t *tm = tpool_create(MAX);
    if (!tm) {
        perror("threadpool init failed");
        return -1;
    }

    int serverSock = create_socket(PORT, MAX), clientSock;
    if (serverSock < 0) {
        perror("socket init failed");
        return -1;
    }

    while (keep_running) {
        socklen_t clientLen = sizeof(clientAddr);
        if ((clientSock = accept(serverSock, (struct sockaddr *)&clientAddr, &clientLen)) < 0) {
            if (errno == EINTR) continue;
            perror("could not accept client");
            continue;
        }

        client_t *client = (client_t *)malloc(sizeof(client_t));
        if (!client) {
            perror("failed to create client");
            continue;
        }

        client->sock = clientSock;
        client->ctx = &server;

        if (!tpool_add_work(tm, handle_client, client)) {
            perror("failed to handle client");
            continue;
        }
    }

    printf("\nShutting down gracefully...\n");

    close(serverSock);
    tpool_destroy(tm);
    destroy_api();
    db_close(server.db);
    
    return 0;
}

int create_socket(int port, int maxClients) {
    int serverSock = 0;

    serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock < 0) {
        perror("socket creation failed");
        return -1;
    }

    // allows for quick rebinding after reboot
    int on = 1;
    if (setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0) {
        perror("listen setsockopt failed");
        return -1;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(port);

    if (bind(serverSock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("failed to bind");
        return -1;
    }

    if (listen(serverSock, maxClients) < 0) {
        perror("failed to listen");
        return -1;
    }

    return serverSock;
}

