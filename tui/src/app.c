#include "app.h"
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

static const int KONAMI_SEQ[KONAMI_LEN] = {
    KEY_UP, KEY_UP, KEY_DOWN, KEY_DOWN,
    KEY_LEFT, KEY_RIGHT, KEY_LEFT, KEY_RIGHT, 'b', 'a'
};

static void set_status(app_t *a, api_status_t st, const char *what);
static void feed_konami(app_t *a, int key);
static void move_horizontal(app_t *a, int dir);
static void move_vertical(app_t *a, int dir);
static int  find_in_row(app_t *a, int target_row, int desired_col);
static void open_prompt(app_t *a, prompt_action_t action, const char *label);
static void confirm_prompt(app_t *a);
static void do_buy(app_t *a);
static void do_cashout(app_t *a);
static int  parse_zl_to_gr(const char *s);
static int  parse_pos_int(const char *s);

void app_slot_rowcol(int slot_id, int *row, int *col) {
    *row = slot_id / 10;
    *col = slot_id % 10;
}

void app_fmt_money(int gr, char *buf, size_t n) {
    snprintf(buf, n, "%d.%02d", gr / 100, gr % 100);
}

void app_init(app_t *a, api_client_t *client) {
    memset(a, 0, sizeof(*a));
    a->client  = client;
    a->running = true;
    a->mode    = MODE_BROWSE;
    a->pending = PROMPT_NONE;
    app_refresh(a);
    a->message[0] = '\0';
}

void app_free(app_t *a) {
    free(a->products);
    a->products = NULL;
    a->count    = 0;
}

void app_refresh(app_t *a) {
    a->server_down = false;

    product_t *prods = NULL;
    int n = 0;
    api_status_t st = api_get_stock(a->client, &prods, &n);
    if (st == API_OK) {
        free(a->products);
        a->products = prods;
        a->count    = n;
        if (a->selected >= a->count) a->selected = a->count > 0 ? a->count - 1 : 0;
    } else {
        set_status(a, st, "stock");
    }

    int bal;
    st = api_get_balance(a->client, &bal);
    if (st == API_OK) a->balance_gr = bal;
    else set_status(a, st, "balance");
}

static void set_status(app_t *a, api_status_t st, const char *what) {
    switch (st) {
        case API_ERR_NETWORK:
            a->server_down = true;
            snprintf(a->message, MSG_LEN, "Cannot reach server");
            break;
        case API_ERR_HTTP:
            snprintf(a->message, MSG_LEN, "%s: server rejected request", what);
            break;
        case API_ERR_PARSE:
            snprintf(a->message, MSG_LEN, "%s: unexpected server response", what);
            break;
        default:
            break;
    }
}

static void feed_konami(app_t *a, int key) {
    if (a->konami_pos < KONAMI_LEN) {
        a->konami[a->konami_pos++] = key;
    } else {
        memmove(a->konami, a->konami + 1, (KONAMI_LEN - 1) * sizeof(int));
        a->konami[KONAMI_LEN - 1] = key;
    }
    if (a->konami_pos == KONAMI_LEN &&
        memcmp(a->konami, KONAMI_SEQ, sizeof(KONAMI_SEQ)) == 0) {
        a->admin = !a->admin;
        snprintf(a->message, MSG_LEN, "%s", a->admin ? "ADMIN unlocked" : "ADMIN locked");
        a->konami_pos = 0;
    }
}

static void move_horizontal(app_t *a, int dir) {
    if (a->count == 0) return;
    a->selected = (a->selected + dir + a->count) % a->count;
}

static int find_in_row(app_t *a, int target_row, int desired_col) {
    int best = -1, best_dist = INT_MAX;
    for (int i = 0; i < a->count; i++) {
        int r, c;
        app_slot_rowcol(a->products[i].slot_id, &r, &c);
        if (r == target_row) {
            int d = c > desired_col ? c - desired_col : desired_col - c;
            if (d < best_dist) { best_dist = d; best = i; }
        }
    }
    return best;
}

static void move_vertical(app_t *a, int dir) {
    if (a->count == 0) return;
    int r, c;
    app_slot_rowcol(a->products[a->selected].slot_id, &r, &c);

    int minr = INT_MAX, maxr = INT_MIN;
    for (int i = 0; i < a->count; i++) {
        int rr, cc;
        app_slot_rowcol(a->products[i].slot_id, &rr, &cc);
        if (rr < minr) minr = rr;
        if (rr > maxr) maxr = rr;
    }

    int target = r + dir, idx = -1;
    while (target >= minr && target <= maxr) {
        idx = find_in_row(a, target, c);
        if (idx >= 0) break;
        target += dir;
    }
    if (idx < 0) {
        target = (dir > 0) ? minr : maxr;
        while (target >= minr && target <= maxr) {
            idx = find_in_row(a, target, c);
            if (idx >= 0) break;
            target += dir;
        }
    }
    if (idx >= 0) a->selected = idx;
}

static void open_prompt(app_t *a, prompt_action_t action, const char *label) {
    if (a->count == 0 && action != PROMPT_DEPOSIT) {
        snprintf(a->message, MSG_LEN, "No slot selected");
        return;
    }
    a->mode    = MODE_PROMPT;
    a->pending = action;
    strncpy(a->prompt_label, label, PROMPT_LABEL_LEN - 1);
    a->prompt_label[PROMPT_LABEL_LEN - 1] = '\0';
    a->prompt_buf[0] = '\0';
}

static int parse_zl_to_gr(const char *s) {
    long zl = 0, gr = 0;
    int seen_digit = 0, i = 0;
    while (s[i] && s[i] != '.') {
        if (s[i] < '0' || s[i] > '9') return -1;
        zl = zl * 10 + (s[i] - '0');
        if (zl > 99999999L) return -1;
        seen_digit = 1;
        i++;
    }
    if (s[i] == '.') {
        i++;
        int frac_digits = 0;
        while (s[i]) {
            if (s[i] < '0' || s[i] > '9') return -1;
            if (frac_digits < 2) { gr = gr * 10 + (s[i] - '0'); frac_digits++; }
            else return -1;
            i++;
        }
        if (frac_digits == 1) gr *= 10;
    }
    if (!seen_digit) return -1;
    long result = zl * 100 + gr;
    if (result > (long)INT_MAX) return -1;
    return (int)result;
}

static int parse_pos_int(const char *s) {
    if (!s[0]) return -1;
    long v = 0;
    for (int i = 0; s[i]; i++) {
        if (s[i] < '0' || s[i] > '9') return -1;
        v = v * 10 + (s[i] - '0');
        if (v > (long)INT_MAX) return -1;
    }
    return v > 0 ? (int)v : -1;
}

static void do_buy(app_t *a) {
    if (a->count == 0) return;
    product_t *p = &a->products[a->selected];
    if (p->stock_qty <= 0) {
        snprintf(a->message, MSG_LEN, "Sold out");
        return;
    }
    if (a->balance_gr < p->price_gr) {
        char m[16];
        app_fmt_money(p->price_gr, m, sizeof(m));
        snprintf(a->message, MSG_LEN, "Insufficient balance (need %s zl)", m);
        return;
    }
    api_status_t st = api_purchase(a->client, p->slot_id);
    if (st == API_OK)               snprintf(a->message, MSG_LEN, "Dispensing %s!", p->name);
    else if (st == API_ERR_NETWORK) { a->server_down = true; snprintf(a->message, MSG_LEN, "Cannot reach server"); return; }
    else                            snprintf(a->message, MSG_LEN, "Purchase failed");
    app_refresh(a);
}

static void do_cashout(app_t *a) {
    int refund = 0;
    api_status_t st = api_refund(a->client, &refund);
    if (st == API_OK) {
        char m[16];
        app_fmt_money(refund, m, sizeof(m));
        snprintf(a->message, MSG_LEN, "Cashed out %s zl", m);
        app_refresh(a);
    } else if (st == API_ERR_NETWORK) {
        a->server_down = true;
        snprintf(a->message, MSG_LEN, "Cannot reach server");
    } else {
        snprintf(a->message, MSG_LEN, "Cash out failed");
    }
}

static void confirm_prompt(app_t *a) {
    product_t *p = (a->count > 0) ? &a->products[a->selected] : NULL;

    if (a->pending == PROMPT_DEPOSIT) {
        int gr = parse_zl_to_gr(a->prompt_buf);
        if (gr <= 0) {
            snprintf(a->message, MSG_LEN, "Invalid amount");
        } else {
            api_status_t st = api_deposit(a->client, gr);
            if (st == API_OK) {
                char m[16];
                app_fmt_money(gr, m, sizeof(m));
                snprintf(a->message, MSG_LEN, "Deposited %s zl", m);
            } else if (st == API_ERR_NETWORK) {
                a->server_down = true;
                snprintf(a->message, MSG_LEN, "Cannot reach server");
            } else {
                snprintf(a->message, MSG_LEN, "Deposit failed");
            }
        }
    } else if (a->pending == PROMPT_RESTOCK && p) {
        int q = parse_pos_int(a->prompt_buf);
        if (q < 0) {
            snprintf(a->message, MSG_LEN, "Invalid quantity");
        } else {
            api_status_t st = api_restock(a->client, p->slot_id, q);
            if (st == API_OK)               snprintf(a->message, MSG_LEN, "Restocked slot %d (+%d)", p->slot_id, q);
            else if (st == API_ERR_NETWORK) { a->server_down = true; snprintf(a->message, MSG_LEN, "Cannot reach server"); }
            else                            snprintf(a->message, MSG_LEN, "Restock failed");
        }
    } else if (a->pending == PROMPT_PRICE && p) {
        int gr = parse_zl_to_gr(a->prompt_buf);
        if (gr <= 0) {
            snprintf(a->message, MSG_LEN, "Invalid price");
        } else {
            api_status_t st = api_set_price(a->client, p->item_id, gr);
            if (st == API_OK)               snprintf(a->message, MSG_LEN, "Updated price of %s", p->name);
            else if (st == API_ERR_NETWORK) { a->server_down = true; snprintf(a->message, MSG_LEN, "Cannot reach server"); }
            else                            snprintf(a->message, MSG_LEN, "Price update failed");
        }
    }

    a->mode    = MODE_BROWSE;
    a->pending = PROMPT_NONE;
    a->prompt_buf[0] = '\0';
    if (!a->server_down) app_refresh(a);
}

void app_handle_key(app_t *a, int key) {
    feed_konami(a, key);

    if (a->server_down) {
        if (key == 'r' || key == 'R')      app_refresh(a);
        else if (key == 'q' || key == 'Q') a->running = false;
        return;
    }

    if (a->mode == MODE_PROMPT) {
        if (key == 27) {
            a->mode = MODE_BROWSE;
            a->pending = PROMPT_NONE;
            a->prompt_buf[0] = '\0';
            snprintf(a->message, MSG_LEN, "Cancelled");
        } else if (key == '\n' || key == '\r' || key == KEY_ENTER) {
            confirm_prompt(a);
        } else if (key == KEY_BACKSPACE || key == 127 || key == 8) {
            size_t l = strlen(a->prompt_buf);
            if (l > 0) a->prompt_buf[l - 1] = '\0';
        } else if (((key >= '0' && key <= '9') || key == '.') &&
                   strlen(a->prompt_buf) < PROMPT_BUF_LEN - 1) {
            size_t l = strlen(a->prompt_buf);
            a->prompt_buf[l]     = (char)key;
            a->prompt_buf[l + 1] = '\0';
        }
        return;
    }

    switch (key) {
        case KEY_LEFT:  move_horizontal(a, -1); break;
        case KEY_RIGHT: move_horizontal(a, +1); break;
        case KEY_UP:    move_vertical(a, -1);   break;
        case KEY_DOWN:  move_vertical(a, +1);   break;
        case '\n': case '\r': case KEY_ENTER: case ' ':
            do_buy(a); break;
        case 'd': open_prompt(a, PROMPT_DEPOSIT, "Deposit amount (zl):"); break;
        case 'c': do_cashout(a); break;
        case 'r': app_refresh(a); snprintf(a->message, MSG_LEN, "Refreshed"); break;
        case 'q': a->running = false; break;
        case 's': if (a->admin) open_prompt(a, PROMPT_RESTOCK, "Restock qty:"); break;
        case 'p': if (a->admin) open_prompt(a, PROMPT_PRICE, "New price (zl):"); break;
        case 27:  if (a->admin) { a->admin = false; snprintf(a->message, MSG_LEN, "ADMIN locked"); } break;
        default: break;
    }
}
