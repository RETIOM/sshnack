#include "db.h"
#include "api.h"
#include "threadpool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <poll.h>
#include <signal.h>
#include <unistd.h>
#include <limits.h>


#define BUF_SIZE 512


volatile sig_atomic_t keep_running = 1;

struct sockaddr_in serverAddr, clientAddr;

typedef struct {
    char admin_token[128];
    char db_path[PATH_MAX];
    int port;
    int internal_port;
    int num_workers;
    int max_clients;
} config_t;

static void load_config(int argc, char *argv[], config_t *cfg);
static int  create_socket(const char *addr, int port, int max_clients);
void handle_sigint(int sig) { keep_running = 0; }


int main(int argc, char* argv[]) {
    struct sigaction sa = {0};
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; /* no SA_RESTART — poll() must be interruptible */
    sigaction(SIGINT, &sa, NULL);

    config_t config;
    load_config(argc, argv, &config);

    server_t server;

    server.db = db_init(config.db_path);
    server.admin_token = config.admin_token;
    if (!server.db) {
        fprintf(stderr, "db init failed\n");
        return -1;
    }

    init_api();

    tpool_t *tm = tpool_create(config.num_workers);
    if (!tm) {
        fprintf(stderr, "threadpool init failed\n");
        return -1;
    }

    int public_sock = create_socket("0.0.0.0", config.port, config.max_clients);
    int internal_sock = create_socket("0.0.0.0", config.internal_port, config.max_clients);
    if (public_sock < 0 || internal_sock < 0) {
        fprintf(stderr, "socket init failed\n");
        return -1;
    }

    struct pollfd fds[2] = {
        { .fd = public_sock,   .events = POLLIN },
        { .fd = internal_sock, .events = POLLIN },
    };

    while (keep_running) {
        int ret = poll(fds, 2, 500);
        if (ret < 0) {
            if (errno == EINTR) break;
            fprintf(stderr, "poll failed: %s\n", strerror(errno));
            break;
        }

        for (int i = 0; i < 2; i++) {
            if (!(fds[i].revents & POLLIN)) continue;

            socklen_t client_len = sizeof(clientAddr);
            int client_sock = accept(fds[i].fd, (struct sockaddr *)&clientAddr, &client_len);
            if (client_sock < 0) {
                fprintf(stderr, "could not accept client: %s\n", strerror(errno));
                continue;
            }

            client_t *client = malloc(sizeof(client_t));
            if (!client) {
                fprintf(stderr, "failed to create client: %s\n", strerror(errno));
                close(client_sock);
                continue;
            }

            client->sock = client_sock;
            client->internal = (i == 1);
            client->ctx = &server;

            if (!tpool_add_work(tm, handle_client, client)) {
                fprintf(stderr, "failed to handle client\n");
            }
        }
    }

    printf("\nShutting down gracefully...\n");

    close(public_sock);
    close(internal_sock);
    tpool_destroy(tm);
    destroy_api();
    db_close(server.db);

    return 0;
}


static void load_config(int argc, char *argv[], config_t *cfg) {
    int admin_token_set = 0;
    int db_path_set = 0;

    const char *default_admin_token = "admin";
    const char *env_admin_token = getenv("SSHNACK_ADMIN_TOKEN");
    if (env_admin_token) {
        strncpy(cfg->admin_token, env_admin_token, sizeof(cfg->admin_token) - 1);
        cfg->admin_token[sizeof(cfg->admin_token) - 1] = '\0';
        admin_token_set = 1;
    } else {
        strncpy(cfg->admin_token, default_admin_token, sizeof(cfg->admin_token) - 1);
        cfg->admin_token[sizeof(cfg->admin_token) - 1] = '\0';
    }

    const char *default_db_path = "data/sshnack.db";
    const char *env_db_path = getenv("SSHNACK_DB_PATH");
    if (env_db_path) {
        strncpy(cfg->db_path, env_db_path, sizeof(cfg->db_path) - 1);
        cfg->db_path[sizeof(cfg->db_path) - 1] = '\0';
        db_path_set = 1;
    } else {
        strncpy(cfg->db_path, default_db_path, sizeof(cfg->db_path) - 1);
        cfg->db_path[sizeof(cfg->db_path) - 1] = '\0';
    }

    const int default_port = 8080;
    const char *env_port = getenv("SSHNACK_PORT");
    if (env_port) {
        int v = atoi(env_port);
        if (v > 0 && v <= 65535) {
            cfg->port = v;
        } else {
            fprintf(stderr, "invalid SSHNACK_PORT value '%s', falling back on default: %d\n", env_port, default_port);
            cfg->port = default_port;
        }
    } else {
        cfg->port = default_port;
    }

    const int default_internal_port = 8081;
    const char *env_internal_port = getenv("SSHNACK_INTERNAL_PORT");
    if (env_internal_port) {
        int v = atoi(env_internal_port);
        if (v > 0 && v <= 65535) {
            cfg->internal_port = v;
        } else {
            fprintf(stderr, "invalid SSHNACK_INTERNAL_PORT value '%s', falling back on default: %d\n", env_internal_port, default_internal_port);
            cfg->internal_port = default_internal_port;
        }
    } else {
        cfg->internal_port = default_internal_port;
    }

    const int default_workers = 4;
    const char *env_workers = getenv("SSHNACK_NUM_WORKERS");
    if (env_workers) {
        int v = atoi(env_workers);
        if (v > 0) {
            cfg->num_workers = v;
        } else {
            fprintf(stderr, "invalid SSHNACK_NUM_WORKERS value '%s', falling back on default: %d\n", env_workers, default_workers);
            cfg->num_workers = default_workers;
        }
    } else {
        cfg->num_workers = default_workers;
    }

    const int default_max_clients = 10;
    const char *env_max_clients = getenv("SSHNACK_MAX_CLIENTS");
    if (env_max_clients) {
        int v = atoi(env_max_clients);
        if (v > 0) {
            cfg->max_clients = v;
        } else {
            fprintf(stderr, "invalid SSHNACK_MAX_CLIENTS value '%s', falling back on default: %d\n", env_max_clients, default_max_clients);
            cfg->max_clients = default_max_clients;
        }
    } else {
        cfg->max_clients = default_max_clients;
    }

    for (int i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], "--admin-token") == 0) {
            strncpy(cfg->admin_token, argv[i + 1], sizeof(cfg->admin_token) - 1);
            cfg->admin_token[sizeof(cfg->admin_token) - 1] = '\0';
            admin_token_set = 1;
            continue;
        } else if (strcmp(argv[i], "--db") == 0) {
            strncpy(cfg->db_path, argv[i + 1], sizeof(cfg->db_path) - 1);
            cfg->db_path[sizeof(cfg->db_path) - 1] = '\0';
            db_path_set = 1;
            continue;
        } else if (strcmp(argv[i], "--port") == 0) {
            int v = atoi(argv[i + 1]);
            if (v > 0 && v <= 65535) {
                cfg->port = v;
            } else {
                fprintf(stderr, "invalid --port value '%s', falling back on default: %d\n", argv[i + 1], cfg->port);
            }
            continue;
        } else if (strcmp(argv[i], "--internal-port") == 0) {
            int v = atoi(argv[i + 1]);
            if (v > 0 && v <= 65535) {
                cfg->internal_port = v;
            } else {
                fprintf(stderr, "invalid --internal-port value '%s', falling back on default: %d\n", argv[i + 1], cfg->internal_port);
            }
            continue;
        } else if (strcmp(argv[i], "--num-workers") == 0) {
            int v = atoi(argv[i + 1]);
            if (v > 0) {
                cfg->num_workers = v;
            } else {
                fprintf(stderr, "invalid --num-workers value '%s', falling back on default: %d\n", argv[i + 1], cfg->num_workers);
            }
            continue;
        } else if (strcmp(argv[i], "--max-clients") == 0) {
            int v = atoi(argv[i + 1]);
            if (v > 0) {
                cfg->max_clients = v;
            } else {
                fprintf(stderr, "invalid --max-clients value '%s', falling back on default: %d\n", argv[i + 1], cfg->max_clients);
            }
        }
    }

    if (!admin_token_set)
        fprintf(stderr, "SSHNACK_ADMIN_TOKEN(--admin-token) not set, using default: %s\n", default_admin_token);
    if (!db_path_set)
        fprintf(stderr, "SSHNACK_DB_PATH(--db) not set, using default: %s\n", default_db_path);
}

static int create_socket(const char *addr, int port, int max_clients) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        fprintf(stderr, "socket creation failed: %s\n", strerror(errno));
        return -1;
    }

    int on = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
        fprintf(stderr, "setsockopt failed: %s\n", strerror(errno));
        return -1;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    if (inet_pton(AF_INET, addr, &serverAddr.sin_addr) != 1) {
        fprintf(stderr, "invalid bind address: %s\n", addr);
        return -1;
    }

    if (bind(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        fprintf(stderr, "failed to bind: %s\n", strerror(errno));
        return -1;
    }

    if (listen(sock, max_clients) < 0) {
        fprintf(stderr, "failed to listen: %s\n", strerror(errno));
        return -1;
    }

    return sock;
}
