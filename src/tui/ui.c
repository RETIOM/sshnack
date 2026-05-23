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
    PAIR_G1, PAIR_G2, PAIR_G3, PAIR_G4, PAIR_G5, PAIR_G6, PAIR_G7
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
        init_pair(PAIR_RED,    COLOR_RED,   -1);
        init_pair(PAIR_SELECT, COLOR_BLACK, COLOR_CYAN);
        init_pair(PAIR_G1, COLOR_RED,     -1);
        init_pair(PAIR_G2, COLOR_YELLOW,  -1);
        init_pair(PAIR_G3, COLOR_GREEN,   -1);
        init_pair(PAIR_G4, COLOR_CYAN,    -1);
        init_pair(PAIR_G5, COLOR_BLUE,    -1);
        init_pair(PAIR_G6, COLOR_MAGENTA, -1);
        init_pair(PAIR_G7, COLOR_WHITE,   -1);
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

static void draw_cell(int y, int x, const product_t *p, int selected) {
    int sold_out = (p->stock_qty <= 0);
    int attr = 0;
    if (selected)      attr = COLOR_PAIR(PAIR_SELECT) | A_BOLD;
    else if (sold_out) attr = A_DIM;

    if (attr) attron(attr);
    draw_box(y, x, CELL_H, CELL_W);

    /* inner width = CELL_W - 2 = 8 */
    char l1[32];
    snprintf(l1, sizeof(l1), "%d %-5.5s", p->slot_id, p->name);
    mvprintw(y + 1, x + 1, "%-8.8s", l1);

    if (sold_out) {
        mvprintw(y + 2, x + 1, "%-8.8s", "SOLD OUT");
    } else {
        char money[16], l2[32];
        app_fmt_money(p->price_gr, money, sizeof(money));
        snprintf(l2, sizeof(l2), "%s zl", money);
        mvprintw(y + 2, x + 1, "%-8.8s", l2);
    }

    char l3[32];
    snprintf(l3, sizeof(l3), " x%d", p->stock_qty);
    mvprintw(y + 3, x + 1, "%-8.8s", l3);

    if (attr) attroff(attr);
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

    int status_row = LINES - 2;
    int msg_row    = LINES - 1;
    int title_h    = 3;

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
        int rows    = maxr - minr + 1;
        int colsn   = maxc - minc + 1;
        int grid_w  = colsn * CELL_W + (colsn - 1) * GAP_X;
        int grid_h  = rows * CELL_H;
        int avail_h = status_row - title_h;

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
                draw_cell(cy, cx, &a->products[i], i == a->selected);
            }
        }
    }

    char bal[16];
    app_fmt_money(a->balance_gr, bal, sizeof(bal));
    mvprintw(status_row, 1, "user #%d    Balance: %s zl", a->client->user_id, bal);
    if (a->admin) {
        const char *adm = "ADMIN";
        int ax = COLS - (int)strlen(adm) - 1;
        if (ax < 0) ax = 0;
        attron(COLOR_PAIR(PAIR_RED) | A_BOLD);
        mvprintw(status_row, ax, "%s", adm);
        attroff(COLOR_PAIR(PAIR_RED) | A_BOLD);
    }

    if (a->admin)
        mvprintw(msg_row, 1, "[s]restock [e]price [Esc]exit | <-/-> ^/v move [Enter]buy [d]ep [c]ash [q]uit");
    else
        mvprintw(msg_row, 1, "<-/-> ^/v move  [Enter]buy  [d]eposit  [c]ashout  [r]efresh  [q]uit");

    if (a->message[0])
        draw_centered(2, a->message);

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
