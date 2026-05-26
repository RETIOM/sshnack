#include "ui.h"
#include "app.h"
#include <ncurses.h>
#include <locale.h>
#include <string.h>
#include <stdio.h>

#define CELL_W 10
#define CELL_H 5
#define GAP_X  1

enum {
    PAIR_RED = 1,
    PAIR_SELECT,
    PAIR_G1, PAIR_G2, PAIR_G3, PAIR_G4, PAIR_G5, PAIR_G6, PAIR_G7,
    PAIR_AFFORD
};

void ui_init(void) {
    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(PAIR_RED,    COLOR_RED,     -1);
        init_pair(PAIR_SELECT, COLOR_BLACK,   COLOR_CYAN);
        init_pair(PAIR_G1, COLOR_RED,     -1);
        init_pair(PAIR_G2, COLOR_YELLOW,  -1);
        init_pair(PAIR_G3, COLOR_GREEN,   -1);
        init_pair(PAIR_G4, COLOR_CYAN,    -1);
        init_pair(PAIR_G5, COLOR_BLUE,    -1);
        init_pair(PAIR_G6, COLOR_MAGENTA, -1);
        init_pair(PAIR_G7, COLOR_WHITE,   -1);
        init_pair(PAIR_AFFORD, COLOR_GREEN, -1);
    }
}

void ui_teardown(void) {
    endwin();
}

int ui_get_key(void) {
    return getch();
}

static void draw_centered(int row, const char *s) {
    int x = (COLS - (int)strlen(s)) / 2;
    if (x < 0) x = 0;
    mvprintw(row, x, "%s", s);
}

static void draw_centered_colored(int row, const char *s, int attr) {
    int x = (COLS - (int)strlen(s)) / 2;
    if (x < 0) x = 0;
    attron(attr);
    mvprintw(row, x, "%s", s);
    attroff(attr);
}

static void draw_title(void) {
    const char *title = "sshnack";
    int len = (int)strlen(title);
    int x = (COLS - len) / 2;
    if (x < 0) x = 0;
    int pairs[7] = { PAIR_G1, PAIR_G2, PAIR_G3, PAIR_G4, PAIR_G5, PAIR_G6, PAIR_G7 };
    for (int i = 0; i < len; i++) {
        attron(COLOR_PAIR(pairs[i % 7]) | A_BOLD);
        mvaddch(1, x + i, title[i]);
        attroff(COLOR_PAIR(pairs[i % 7]) | A_BOLD);
    }
}

static void draw_box(int y, int x, int h, int w) {
    mvhline(y, x + 1, ACS_HLINE, w - 2);
    mvhline(y + h - 1, x + 1, ACS_HLINE, w - 2);
    mvvline(y + 1, x, ACS_VLINE, h - 2);
    mvvline(y + 1, x + w - 1, ACS_VLINE, h - 2);
    mvaddch(y, x, ACS_ULCORNER);
    mvaddch(y, x + w - 1, ACS_URCORNER);
    mvaddch(y + h - 1, x, ACS_LLCORNER);
    mvaddch(y + h - 1, x + w - 1, ACS_LRCORNER);
}

/* col_pairs cycles through 5 hues for the 5 columns (slot % 10 in 1..5) */
static const int col_pairs[] = {PAIR_G2, PAIR_G3, PAIR_G4, PAIR_G5, PAIR_G6};

static void draw_cell(int y, int x, const product_t *p, int selected, int balance_gr) {
    int sold_out = (p->stock_qty <= 0);
    int col_pair = col_pairs[((p->slot_id % 10) - 1 + 5) % 5];

    int cell_attr;
    if (selected)      cell_attr = COLOR_PAIR(PAIR_SELECT) | A_BOLD;
    else if (sold_out) cell_attr = A_DIM;
    else               cell_attr = COLOR_PAIR(col_pair);

    /* border + slot header share the cell colour */
    attron(cell_attr);
    draw_box(y, x, CELL_H, CELL_W);
    char l1[32];
    snprintf(l1, sizeof(l1), "%d %-5.5s", p->slot_id, p->name);
    mvprintw(y + 1, x + 1, "%-8.8s", l1);
    attroff(cell_attr);

    if (sold_out) {
        attron(A_DIM);
        mvprintw(y + 2, x + 1, "%-8.8s", "SOLD OUT");
        mvprintw(y + 3, x + 1, "%-8.8s", "");
        attroff(A_DIM);
    } else if (selected) {
        attron(COLOR_PAIR(PAIR_SELECT) | A_BOLD);
        char money[16], l2[32];
        app_fmt_money(p->price_gr, money, sizeof(money));
        snprintf(l2, sizeof(l2), "%s zl", money);
        mvprintw(y + 2, x + 1, "%-8.8s", l2);
        char l3[32];
        snprintf(l3, sizeof(l3), " x%d", p->stock_qty);
        mvprintw(y + 3, x + 1, "%-8.8s", l3);
        attroff(COLOR_PAIR(PAIR_SELECT) | A_BOLD);
    } else {
        /* price: green if affordable, red if not */
        int affordable = (balance_gr >= p->price_gr);
        int price_attr = affordable ? (COLOR_PAIR(PAIR_AFFORD) | A_BOLD)
                                    : (COLOR_PAIR(PAIR_RED)    | A_BOLD);
        char money[16], l2[32];
        app_fmt_money(p->price_gr, money, sizeof(money));
        snprintf(l2, sizeof(l2), "%s zl", money);
        attron(price_attr);
        mvprintw(y + 2, x + 1, "%-8.8s", l2);
        attroff(price_attr);

        /* qty: yellow when low (≤5), green otherwise */
        int qty_attr = (p->stock_qty <= 5) ? (COLOR_PAIR(PAIR_G2) | A_BOLD)
                                           :  COLOR_PAIR(PAIR_G3);
        char l3[32];
        snprintf(l3, sizeof(l3), " x%d", p->stock_qty);
        attron(qty_attr);
        mvprintw(y + 3, x + 1, "%-8.8s", l3);
        attroff(qty_attr);
    }
}

void ui_render(const app_t *a) {
    erase();

    if (a->server_down) {
        draw_centered(LINES / 2 - 1, "Cannot reach server");
        draw_centered(LINES / 2 + 1, "[r] retry    [q] quit");
        refresh();
        return;
    }

    draw_title();

    int admin_row = LINES - 4;
    int status_row = LINES - 3;
    int detail_row = LINES - 2;
    int guide_row = LINES - 1;
    int title_h = 3;

    if (a->count == 0) {
        draw_centered(LINES / 2, "Machine empty");
    } else {
        int minr = 1 << 30, maxr = -(1 << 30), minc = 1 << 30, maxc = -(1 << 30);
        for (int i = 0; i < a->count; i++) {
            int r, c;
            app_slot_rowcol(a->products[i].slot_id, &r, &c);
            if (r < minr) minr = r;
            if (r > maxr) maxr = r;
            if (c < minc) minc = c;
            if (c > maxc) maxc = c;
        }
        int rows = maxr - minr + 1;
        int colsn = maxc - minc + 1;
        int grid_w = colsn * CELL_W + (colsn - 1) * GAP_X;
        int grid_h = rows * CELL_H;
        int avail_h = admin_row - title_h;

        if (grid_w > COLS || grid_h > avail_h) {
            draw_centered(LINES / 2, "Terminal too small");
        } else {
            int ox = (COLS - grid_w) / 2;
            if (ox < 0) ox = 0;
            int oy = title_h + (avail_h - grid_h) / 2;
            if (oy < title_h) oy = title_h;
            for (int i = 0; i < a->count; i++) {
                int r, c;
                app_slot_rowcol(a->products[i].slot_id, &r, &c);
                int cx = ox + (c - minc) * (CELL_W + GAP_X);
                int cy = oy + (r - minr) * CELL_H;
                draw_cell(cy, cx, &a->products[i], i == a->selected, a->balance_gr);
            }
        }
    }

    /* ADMIN badge on its own row above status */
    if (a->admin)
        draw_centered_colored(admin_row, "ADMIN", COLOR_PAIR(PAIR_RED) | A_BOLD);

    /* centered status bar */
    char bal[16];
    app_fmt_money(a->balance_gr, bal, sizeof(bal));
    char status[64];
    snprintf(status, sizeof(status), "user #%d    Balance: %8s zl", a->client->user_id, bal);
    draw_centered_colored(status_row, status, COLOR_PAIR(PAIR_G7));

    /* detail row: full product name while hovering (ASCII separators for correct centering) */
    if (a->count > 0 && a->selected >= 0 && a->selected < a->count) {
        const product_t *p = &a->products[a->selected];
        char detail[128];
        if (p->stock_qty <= 0) {
            snprintf(detail, sizeof(detail), "[ %s | SOLD OUT ]", p->name);
        } else {
            char money[16];
            app_fmt_money(p->price_gr, money, sizeof(money));
            snprintf(detail, sizeof(detail), "[ %s | %s zl | x%d in stock ]",
                     p->name, money, p->stock_qty);
        }
        draw_centered_colored(detail_row, detail, COLOR_PAIR(PAIR_G4) | A_BOLD);
    }

    /* action guide (no movement hints) */
    if (a->admin)
        draw_centered_colored(guide_row, "re[s]tock  set [p]rice  [Esc] exit admin  [q]uit",
                              COLOR_PAIR(PAIR_G7));
    else
        draw_centered_colored(guide_row, "[Enter] buy  [d]eposit  [c]ashout  [r]efresh  [q]uit",
                              COLOR_PAIR(PAIR_G7));

    /* feedback message near title (yellow, temporary) */
    if (a->message[0])
        draw_centered_colored(2, a->message, COLOR_PAIR(PAIR_G2) | A_BOLD);

    if (a->mode == MODE_PROMPT) {
        int pw = 40, ph = 5;
        int px = (COLS - pw) / 2, py = (LINES - ph) / 2;
        if (px < 0) px = 0;
        if (py < 0) py = 0;
        for (int yy = 0; yy < ph; yy++) mvhline(py + yy, px, ' ', pw);
        draw_box(py, px, ph, pw);
        mvprintw(py + 1, px + 2, "%.*s", pw - 4, a->prompt_label);
        mvprintw(py + 2, px + 2, "> %.*s_", pw - 6, a->prompt_buf);
        mvprintw(py + 3, px + 2, "[Enter] confirm   [Esc] cancel");
    }

    refresh();
}
