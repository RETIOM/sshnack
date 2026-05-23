#include "api_client.h"
#include "app.h"
#include "ui.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char base_url[256];
    int  user_id;
    char admin_token[128];
} config_t;

static void load_config(int argc, char **argv, config_t *cfg) {
    strncpy(cfg->base_url, "http://127.0.0.1:8080", sizeof(cfg->base_url) - 1);
    cfg->base_url[sizeof(cfg->base_url) - 1] = '\0';
    cfg->user_id = 67;
    strncpy(cfg->admin_token, "admin", sizeof(cfg->admin_token) - 1);
    cfg->admin_token[sizeof(cfg->admin_token) - 1] = '\0';

    const char *env_url = getenv("SSHNACK_SERVER_URL");
    if (env_url && env_url[0]) {
        strncpy(cfg->base_url, env_url, sizeof(cfg->base_url) - 1);
        cfg->base_url[sizeof(cfg->base_url) - 1] = '\0';
    }
    const char *env_uid = getenv("SSHNACK_USER_ID");
    if (env_uid && env_uid[0]) {
        int v = atoi(env_uid);
        if (v > 0) cfg->user_id = v;
    }
    const char *env_tok = getenv("SSHNACK_ADMIN_TOKEN");
    if (env_tok && env_tok[0]) {
        strncpy(cfg->admin_token, env_tok, sizeof(cfg->admin_token) - 1);
        cfg->admin_token[sizeof(cfg->admin_token) - 1] = '\0';
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--user") == 0 && i + 1 < argc) {
            int v = atoi(argv[++i]);
            if (v > 0) cfg->user_id = v;
        }
    }
}

int main(int argc, char **argv) {
    config_t cfg;
    load_config(argc, argv, &cfg);

    curl_global_init(CURL_GLOBAL_DEFAULT);

    api_client_t client;
    if (api_client_init(&client, cfg.base_url, cfg.user_id, cfg.admin_token) != API_OK) {
        fprintf(stderr, "failed to init HTTP client\n");
        curl_global_cleanup();
        return 1;
    }

    ui_init();

    app_t app;
    app_init(&app, &client);

    while (app.running) {
        ui_render(&app);
        int key = ui_get_key();
        app_handle_key(&app, key);
    }

    app_free(&app);
    ui_teardown();
    api_client_cleanup(&client);
    curl_global_cleanup();
    return 0;
}
