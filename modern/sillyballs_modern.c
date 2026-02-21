// Connect 4 (ncurses) + simple AI + safe "punishment" placeholder
// Build (mac):
//   clang connect4.c -o connect4 -lncurses
// Run:
//   ./connect4

#include <ncurses.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

enum
{
    BOARD_ROWS = 6,
    BOARD_COLS = 7,
    kFrameDelayUs = 25000
};

typedef enum
{
    CELL_EMPTY = 0,
    CELL_P = 1, // player
    CELL_AI = 2 // AI
} Cell;

typedef struct
{
    int max_y, max_x;
    int color_count;

    Cell board[BOARD_ROWS][BOARD_COLS];
    int cursor_col; // selected column for player
    bool game_over;
    int winner; // 0 none, 1 player, 2 AI

    // Loss squiggles
    const char *loss_msg;
    int loss_msg_len;
} AppState;

static unsigned int make_seed(void)
{
    return (unsigned int)time(NULL) ^ (unsigned int)getpid();
}

static int random_color_pair(int color_count)
{
    return (color_count > 0) ? (rand() % color_count) + 1 : 0;
}

static void app_update_dimensions(AppState *s)
{
    getmaxyx(stdscr, s->max_y, s->max_x);
}

static void nc_init(AppState *s)
{
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, true);
    nodelay(stdscr, true);
    curs_set(0);

    s->color_count = 0;
    if (has_colors())
    {
        start_color();
        use_default_colors();
        for (int c = 1; c <= 7; ++c)
        {
            init_pair((short)c, (short)c, -1);
            s->color_count = c;
        }
    }

    erase();
    refresh();
}

static void nc_shutdown(void)
{
    endwin();
}

static void board_clear(AppState *s)
{
    for (int r = 0; r < BOARD_ROWS; ++r)
        for (int c = 0; c < BOARD_COLS; ++c)
            s->board[r][c] = CELL_EMPTY;

    s->cursor_col = BOARD_COLS / 2;
    s->game_over = false;
    s->winner = 0;

    s->loss_msg = "You lost to ai!";
    s->loss_msg_len = 14; // manual length
}

static bool col_has_space(const AppState *s, int col)
{
    return s->board[0][col] == CELL_EMPTY;
}

static int drop_piece(AppState *s, int col, Cell who)
{
    if (col < 0 || col >= BOARD_COLS)
        return -1;
    if (!col_has_space(s, col))
        return -1;

    for (int r = BOARD_ROWS - 1; r >= 0; --r)
    {
        if (s->board[r][col] == CELL_EMPTY)
        {
            s->board[r][col] = who;
            return r;
        }
    }
    return -1;
}

static int check_winner(const AppState *s)
{
    // 4 directions: horiz, vert, diag down-right, diag down-left
    const int dirs[4][2] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}};

    for (int r = 0; r < BOARD_ROWS; ++r)
    {
        for (int c = 0; c < BOARD_COLS; ++c)
        {
            Cell start = s->board[r][c];
            if (start == CELL_EMPTY)
                continue;

            for (int d = 0; d < 4; ++d)
            {
                int dr = dirs[d][0], dc = dirs[d][1];
                int count = 1;

                for (int k = 1; k < 4; ++k)
                {
                    int rr = r + dr * k;
                    int cc = c + dc * k;
                    if (rr < 0 || rr >= BOARD_ROWS || cc < 0 || cc >= BOARD_COLS)
                        break;
                    if (s->board[rr][cc] != start)
                        break;
                    count++;
                }

                if (count >= 4)
                {
                    return (start == CELL_P) ? 1 : 2;
                }
            }
        }
    }
    return 0;
}

static bool board_full(const AppState *s)
{
    for (int c = 0; c < BOARD_COLS; ++c)
        if (col_has_space(s, c))
            return false;
    return true;
}

/* --------------------- "Punishment" action (safe jokes for now) --------------------- */
static void run_punishment_action(AppState *s)
{
    int code = 1 + (rand() % 6);

    // Implement punishment action later (keep it safe + fun)
    int y = s->max_y - 4;
    if (y < 0)
        y = 0;

    switch (code)
    {
    case 1:
        mvprintw(y, 2, "AI Action #1: \"I'm not mad, I'm just disappointed.\"");
        beep();
        break;
    case 2:
        mvprintw(y, 2, "AI Action #2: You must say \"nice try\" out loud. (Optional but recommended)");
        break;
    case 3:
        mvprintw(y, 2, "AI Action #3: The AI does a tiny victory dance in binary: 01010101");
        flash();
        break;
    case 4:
        mvprintw(y, 2, "AI Action #4: Skill tax collected. (0 dollars, purely emotional)");
        break;
    case 5:
        mvprintw(y, 2, "AI Action #5: Your next win must be recorded for the highlight reel.");
        break;
    case 6:
        mvprintw(y, 2, "AI Action #6: \"gg\" but with maximum smugness.");
        break;
    default:
        mvprintw(y, 2, "AI Action: ???");
        break;
    }
}

/* --------------------- Rendering --------------------- */
static void draw_board_ui(const AppState *s)
{
    erase();

    // 1) Fix header: remove unicode arrows
    mvprintw(0, 0, "Connect 4 (ncurses) | LEFT/RIGHT move | Space/Enter drop | r restart | q quit");
    mvprintw(1, 0, "You = O   AI = X");

    int top = 3;

    // 2) Column numbers aligned to same spacing as cells (" %c" => 2 chars each)
    mvprintw(top, 0, "   "); // left padding to match row labels like "0 |"
    for (int c = 0; c < BOARD_COLS; ++c)
    {
        printw(" %d", c);
    }

    // Selector row aligned under the columns
    mvprintw(top + 1, 0, "   ");
    for (int c = 0; c < BOARD_COLS; ++c)
    {
        if (c == s->cursor_col)
        {
            attron(A_REVERSE);
            printw(" ^");
            attroff(A_REVERSE);
        }
        else
        {
            printw("  ");
        }
    }

    // Board
    int grid_y = top + 3;
    for (int r = 0; r < BOARD_ROWS; ++r)
    {
        mvprintw(grid_y + r, 0, "%d |", r);
        for (int c = 0; c < BOARD_COLS; ++c)
        {
            char ch = '.';
            int pair = 0;
            bool bold = false;

            if (s->board[r][c] == CELL_P)
            {
                ch = 'O';
                pair = 2;
                bold = true;
            }
            if (s->board[r][c] == CELL_AI)
            {
                ch = 'X';
                pair = 1;
                bold = true;
            }

            if (pair > 0)
                attron(COLOR_PAIR(pair));
            if (bold)
                attron(A_BOLD);
            printw(" %c", ch);
            if (bold)
                attroff(A_BOLD);
            if (pair > 0)
                attroff(COLOR_PAIR(pair));
        }
        printw(" |");
    }

    mvprintw(grid_y + BOARD_ROWS + 1, 0, "Tip: aim for diagonals. AI is random (for now).");

    if (s->game_over)
    {
        if (s->winner == 1)
        {
            mvprintw(grid_y + BOARD_ROWS + 3, 0, "You win! Press r to play again, or q to quit.");
        }
        else if (s->winner == 2)
        {
            mvprintw(grid_y + BOARD_ROWS + 3, 0, "AI wins! Press r to play again, or q to quit.");
        }
        else
        {
            mvprintw(grid_y + BOARD_ROWS + 3, 0, "Draw! Press r to play again, or q to quit.");
        }
    }

    refresh();
}

/* --------------------- AI turn --------------------- */
static void ai_take_turn(AppState *s)
{
    int valid[BOARD_COLS];
    int count = 0;

    for (int c = 0; c < BOARD_COLS; ++c)
    {
        if (col_has_space(s, c))
            valid[count++] = c;
    }
    if (count == 0)
        return;

    int pick = valid[rand() % count];
    drop_piece(s, pick, CELL_AI);
}

/* --------------------- Loss "SillyBalls style" flood --------------------- */
static void show_loss_squiggles(AppState *s)
{
    erase();
    mvprintw(0, 0, "AI Victory Mode: press any key to stop the suffering...");
    refresh();

    while (true)
    {
        app_update_dimensions(s);

        int ch = getch();
        if (ch != ERR)
            break;

        if (s->max_y > 1 && s->max_x > s->loss_msg_len)
        {
            int y = 1 + (rand() % (s->max_y - 1));
            int x = rand() % (s->max_x - s->loss_msg_len);
            int pair = random_color_pair(s->color_count);

            if (pair > 0)
            {
                attron(COLOR_PAIR(pair) | A_BOLD);
                mvaddstr(y, x, s->loss_msg);
                attroff(COLOR_PAIR(pair) | A_BOLD);
            }
            else
            {
                mvaddstr(y, x, s->loss_msg);
            }
        }

        refresh();
        usleep(kFrameDelayUs);
    }
}

int main(void)
{
    AppState s = {0};

    unsigned int seed = make_seed();
    printf("seed = %u\n", seed);
    srand(seed);

    nc_init(&s);
    app_update_dimensions(&s);
    board_clear(&s);

    while (true)
    {
        draw_board_ui(&s);

        int ch = getch();
        if (ch == ERR)
        {
            usleep(12000);
            continue;
        }

        if (ch == 'q' || ch == 'Q')
            break;

        if (ch == 'r' || ch == 'R')
        {
            board_clear(&s);
            continue;
        }

        if (s.game_over)
        {
            // ignore gameplay input when game over (except r/q)
            continue;
        }

        if (ch == KEY_LEFT)
            s.cursor_col = (s.cursor_col > 0) ? s.cursor_col - 1 : 0;
        if (ch == KEY_RIGHT)
            s.cursor_col = (s.cursor_col < BOARD_COLS - 1) ? s.cursor_col + 1 : BOARD_COLS - 1;

        if (ch == ' ' || ch == '\n' || ch == KEY_ENTER)
        {
            if (drop_piece(&s, s.cursor_col, CELL_P) >= 0)
            {
                int w = check_winner(&s);
                if (w != 0)
                {
                    s.game_over = true;
                    s.winner = w;
                    continue;
                }
                if (board_full(&s))
                {
                    s.game_over = true;
                    s.winner = 0;
                    continue;
                }

                ai_take_turn(&s);

                w = check_winner(&s);
                if (w != 0)
                {
                    s.game_over = true;
                    s.winner = w;

                    if (s.winner == 2)
                    {
                        app_update_dimensions(&s);
                        run_punishment_action(&s); // switch 1..6 random
                        refresh();
                        usleep(900000);          // let user read it
                        show_loss_squiggles(&s); // press any key to return
                    }
                    continue;
                }

                if (board_full(&s))
                {
                    s.game_over = true;
                    s.winner = 0;
                    continue;
                }
            }
            else
            {
                beep(); // column full
            }
        }
    }

    nc_shutdown();
    return 0;
}