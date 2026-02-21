#include <ncurses.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "connect_four.h"
#include "connect_four_ai.h"

typedef struct {
    int max_y;
    int max_x;
    int color_count;

    CfGame game;
    int cursor_col;
    bool game_over;
    int winner;

    char status[160];

    const char *loss_msg;
    int loss_msg_len;
} AppState;

static unsigned int make_seed(void) {
    return (unsigned int)time(NULL) ^ (unsigned int)getpid();
}

static int random_color_pair(int color_count) {
    return (color_count > 0) ? (rand() % color_count) + 1 : 0;
}

static void app_update_dimensions(AppState *s) {
    getmaxyx(stdscr, s->max_y, s->max_x);
}

static void nc_init(AppState *s) {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, true);
    nodelay(stdscr, true);
    curs_set(0);

    s->color_count = 0;
    if (has_colors()) {
        start_color();
        use_default_colors();

        init_pair(1, COLOR_RED, -1);
        init_pair(2, COLOR_YELLOW, -1);
        init_pair(3, COLOR_CYAN, -1);
        init_pair(4, COLOR_GREEN, -1);
        init_pair(5, COLOR_MAGENTA, -1);
        init_pair(6, COLOR_BLUE, -1);
        init_pair(7, COLOR_WHITE, -1);
        s->color_count = 7;
    }

    erase();
    refresh();
}

static void nc_shutdown(void) {
    endwin();
}

static void board_clear(AppState *s) {
    cf_init(&s->game);
    s->cursor_col = CF_COLS / 2;
    s->game_over = false;
    s->winner = 0;
    snprintf(s->status, sizeof(s->status), "Your move.");

    s->loss_msg = "You lost to AI!";
    s->loss_msg_len = (int)strlen(s->loss_msg);
}

/* --------------------- Rendering --------------------- */
static void draw_board_ui(const AppState *s) {
    int top = 4;
    int grid_y = top + 3;

    erase();

    if (has_colors()) {
        attron(A_BOLD | COLOR_PAIR(3));
    } else {
        attron(A_BOLD);
    }
    mvprintw(0, 0, "Connect Four Virus");
    if (has_colors()) {
        attroff(A_BOLD | COLOR_PAIR(3));
    } else {
        attroff(A_BOLD);
    }

    mvprintw(1, 0, "LEFT/RIGHT or A/D move | Enter/Space drop | 1-6 quick select | r restart | q quit");
    mvprintw(2, 0, "You = O   AI = X   First to connect 4 wins.");

    mvprintw(top, 0, "   ");
    for (int c = 0; c < CF_COLS; ++c) {
        printw(" %d ", c + 1);
    }

    mvprintw(top + 1, 0, "   ");
    for (int c = 0; c < CF_COLS; ++c) {
        if (c == s->cursor_col && !s->game_over) {
            attron(A_REVERSE);
            printw(" ^ ");
            attroff(A_REVERSE);
        } else {
            printw("   ");
        }
    }

    for (int r = 0; r < CF_ROWS; ++r) {
        mvprintw(grid_y + r, 0, "%d |", r);
        for (int c = 0; c < CF_COLS; ++c) {
            CfCell cell = s->game.board[r][c];
            char token = '.';
            int pair = 0;

            if (cell == CF_HUMAN) {
                token = 'O';
                pair = 2;
            } else if (cell == CF_AI) {
                token = 'X';
                pair = 1;
            }

            if (pair > 0 && has_colors()) {
                attron(COLOR_PAIR(pair) | A_BOLD);
            }
            printw(" %c ", token);
            if (pair > 0 && has_colors()) {
                attroff(COLOR_PAIR(pair) | A_BOLD);
            }
        }
        printw("|");
    }

    mvprintw(grid_y + CF_ROWS + 1, 0, "%s", s->status);

    if (s->game_over) {
        if (has_colors()) {
            attron(A_BOLD | COLOR_PAIR(4));
        } else {
            attron(A_BOLD);
        }

        if (s->winner == 1) {
            mvprintw(grid_y + CF_ROWS + 3, 0, "You win. Press r to play again or q to quit.");
        } else if (s->winner == 2) {
            mvprintw(grid_y + CF_ROWS + 3, 0, "AI wins. Press r to play again or q to quit.");
        } else {
            mvprintw(grid_y + CF_ROWS + 3, 0, "Draw. Press r to play again or q to quit.");
        }

        if (has_colors()) {
            attroff(A_BOLD | COLOR_PAIR(4));
        } else {
            attroff(A_BOLD);
        }
    }

    refresh();
}

/* --------------------- "Punishment" action (safe, self-contained, 6 cases) --------------------- */
static void run_punishment_action(AppState *s)
{
    int code = 1 + (rand() % 6);   // 1 through 6

    int y = s->max_y - 6;
    if (y < 2) y = 2;              // safe top margin

    if (has_colors()) {
        attron(A_BOLD | COLOR_PAIR(1));
    } else {
        attron(A_BOLD);
    }
    mvprintw(y, 2, "AI VICTORY TAX COLLECTED!");
    if (has_colors()) {
        attroff(A_BOLD | COLOR_PAIR(1));
    } else {
        attroff(A_BOLD);
    }

    mvprintw(y + 1, 4, "Pay the price below... (press any key when done)");

    // The 6 punishments - all text + effects, no external side-effects
    switch (code)
    {
        case 1:
            mvprintw(y + 3, 4, "1. Say out loud: \"The AI is superior and I am humbled.\"");
            beep(); beep();
            break;

        case 2:
            mvprintw(y + 3, 4, "2. Type \"nice try loser\" into the terminal right now.");
            flash();
            break;

        case 3:
            mvprintw(y + 3, 4, "3. Perform a 5-second victory dance for the AI (in real life).");
            for (int i = 0; i < 4; i++) { beep(); usleep(400000); }
            break;

        case 4:
            mvprintw(y + 3, 4, "4. Change your terminal prompt to \"loser@mac:~ $ \" for the next game.");
            mvprintw(y + 4, 4, "(just kidding - but think about it...)");
            break;

        case 5:
            mvprintw(y + 3, 4, "5. Post \"I lost to a random AI in Connect 4\" somewhere online.");
            flash(); beep();
            break;

        case 6:
            mvprintw(y + 3, 4, "6. Stare at this screen and whisper \"gg ez\" with maximum smugness.");
            for (int i = 0; i < 3; i++) { flash(); usleep(500000); }
            break;
    }

    // Wait for user acknowledgment (makes it feel like a real "penalty")
    flushinp();
    refresh();
    while (getch() == ERR) {
        usleep(100000);
    }

    // Clear the punishment area before returning to game
    for (int i = 0; i < 8; i++) {
        move(y + i, 0);
        clrtoeol();
    }
}

/* --------------------- Loss flood --------------------- */
static void show_loss_squiggles(AppState *s) {
    erase();
    mvprintw(0, 0, "AI Victory Mode: press any key to stop the suffering...");
    refresh();

    while (true) {
        int ch;
        app_update_dimensions(s);

        ch = getch();
        if (ch != ERR) {
            break;
        }

        if (s->max_y > 1 && s->max_x > s->loss_msg_len) {
            int y = 1 + (rand() % (s->max_y - 1));
            int x = rand() % (s->max_x - s->loss_msg_len);
            int pair = random_color_pair(s->color_count);

            if (pair > 0 && has_colors()) {
                attron(COLOR_PAIR(pair) | A_BOLD);
                mvaddstr(y, x, s->loss_msg);
                attroff(COLOR_PAIR(pair) | A_BOLD);
            } else {
                mvaddstr(y, x, s->loss_msg);
            }
        }

        refresh();
        usleep(25000);
    }
}

/* --------------------- AI turn --------------------- */
static int ai_take_turn(AppState *s) {
    int pick = cf_ai_choose_move(&s->game, 6);
    if (pick >= 0) {
        cf_drop_piece(&s->game, pick, CF_AI);
    }
    return pick;
}

int main(void) {
    AppState s = {0};
    unsigned int seed = make_seed();

    srand(seed);

    nc_init(&s);
    app_update_dimensions(&s);
    board_clear(&s);

    while (true) {
        int ch;

        draw_board_ui(&s);

        ch = getch();
        if (ch == ERR) {
            usleep(12000);
            continue;
        }

        if (ch == 'q' || ch == 'Q') {
            break;
        }

        if (ch == 'r' || ch == 'R') {
            board_clear(&s);
            continue;
        }

        if (s.game_over) {
            continue;
        }

        if (ch == KEY_LEFT || ch == 'a' || ch == 'A') {
            s.cursor_col = (s.cursor_col > 0) ? s.cursor_col - 1 : 0;
            continue;
        }
        if (ch == KEY_RIGHT || ch == 'd' || ch == 'D') {
            s.cursor_col = (s.cursor_col < CF_COLS - 1) ? s.cursor_col + 1 : CF_COLS - 1;
            continue;
        }
        if (ch >= '1' && ch <= '0' + CF_COLS) {
            s.cursor_col = ch - '1';
            continue;
        }

        if (ch == ' ' || ch == '\n' || ch == KEY_ENTER) {
            int ai_col;

            if (cf_drop_piece(&s.game, s.cursor_col, CF_HUMAN) < 0) {
                beep();
                snprintf(s.status, sizeof(s.status), "Column %d is full.", s.cursor_col + 1);
                continue;
            }

            if (cf_has_winner(&s.game, CF_HUMAN)) {
                s.game_over = true;
                s.winner = 1;
                snprintf(s.status, sizeof(s.status), "You connected four first.");
                continue;
            }
            if (cf_is_draw(&s.game)) {
                s.game_over = true;
                s.winner = 0;
                snprintf(s.status, sizeof(s.status), "Board is full.");
                continue;
            }

            snprintf(s.status, sizeof(s.status), "AI is thinking...");
            draw_board_ui(&s);
            usleep(220000);

            ai_col = ai_take_turn(&s);
            if (ai_col < 0) {
                s.game_over = true;
                s.winner = 0;
                snprintf(s.status, sizeof(s.status), "Board is full.");
                continue;
            }

            if (cf_has_winner(&s.game, CF_AI)) {
                s.game_over = true;
                s.winner = 2;
                snprintf(s.status, sizeof(s.status), "AI played column %d and won.", ai_col + 1);

                app_update_dimensions(&s);
                run_punishment_action(&s);
                show_loss_squiggles(&s);
                continue;
            }
            if (cf_is_draw(&s.game)) {
                s.game_over = true;
                s.winner = 0;
                snprintf(s.status, sizeof(s.status), "Board is full.");
                continue;
            }

            snprintf(s.status, sizeof(s.status), "AI played column %d. Your move.", ai_col + 1);
        }
    }

    nc_shutdown();
    return 0;
}
