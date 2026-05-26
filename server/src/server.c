#include "db.h"
#include "api.h"
#include "threadpool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

    const char *db_path = getenv("SSHNACK_DB_PATH");
    for (int i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], "--db") == 0) {
            db_path = argv[i + 1];
            break;
        }
    }

    server_t server;

    server.db = db_init(db_path);
    server.admin_token = admin_token;
    if (!server.db) {
        fprintf(stderr, "db init failed\n");
        return -1;
    }

    init_api();

    tpool_t *tm = tpool_create(MAX);
    if (!tm) {
        fprintf(stderr, "threadpool init failed\n");
        return -1;
    }

    int serverSock = create_socket(PORT, MAX), clientSock;
    if (serverSock < 0) {
        fprintf(stderr, "socket init failed\n");
        return -1;
    }

    while (keep_running) {
        socklen_t clientLen = sizeof(clientAddr);
        if ((clientSock = accept(serverSock, (struct sockaddr *)&clientAddr, &clientLen)) < 0) {
            if (errno == EINTR) continue;
            fprintf(stderr, "could not accept client: %s\n", strerror(errno));
            continue;
        }

        client_t *client = (client_t *)malloc(sizeof(client_t));
        if (!client) {
            fprintf(stderr, "failed to create client: %s\n", strerror(errno));
            continue;
        }

        client->sock = clientSock;
        client->ctx = &server;

        if (!tpool_add_work(tm, handle_client, client)) {
            fprintf(stderr, "failed to handle client\n");
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
        fprintf(stderr, "socket creation failed: %s\n", strerror(errno));
        return -1;
    }

    // allows for quick rebinding after reboot
    int on = 1;
    if (setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0) {
        fprintf(stderr, "listen setsockopt failed: %s\n", strerror(errno));
        return -1;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(port);

    if (bind(serverSock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        fprintf(stderr, "failed to bind: %s\n", strerror(errno));
        return -1;
    }

    if (listen(serverSock, maxClients) < 0) {
        fprintf(stderr, "failed to listen: %s\n", strerror(errno));
        return -1;
    }

    return serverSock;
}

