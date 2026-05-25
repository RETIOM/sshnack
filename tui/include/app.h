#ifndef APP_H
#define APP_H

#include "api_client.h"
#include <stdbool.h>
#include <stddef.h>

typedef enum { MODE_BROWSE, MODE_PROMPT } input_mode_t;

typedef enum {
    PROMPT_NONE,
    PROMPT_DEPOSIT,
    PROMPT_RESTOCK,
    PROMPT_PRICE
} prompt_action_t;

#define MSG_LEN          128
#define PROMPT_LABEL_LEN 48
#define PROMPT_BUF_LEN   16
#define KONAMI_LEN       10

typedef struct {
    api_client_t *client;

    product_t *products;
    int        count;
    int        balance_gr;

    int        selected;
    bool       admin;
    bool       running;
    bool       server_down;

    input_mode_t    mode;
    prompt_action_t pending;
    char            prompt_label[PROMPT_LABEL_LEN];
    char            prompt_buf[PROMPT_BUF_LEN];

    char       message[MSG_LEN];

    int        konami[KONAMI_LEN];
    int        konami_pos;
} app_t;

void app_init(app_t *a, api_client_t *client);
void app_free(app_t *a);
void app_refresh(app_t *a);
void app_handle_key(app_t *a, int key);

/* pure helpers shared with the UI layer */
void app_slot_rowcol(int slot_id, int *row, int *col);
void app_fmt_money(int gr, char *buf, size_t n);

#endif /* APP_H */
