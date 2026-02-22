#include <ncurses.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "connect_four.h"
#include "connect_four_ai.h"

enum {
    VM_LOG_LINES = 8,
    VM_LOG_CHARS = 100,
    LOSS_MSG_CHARS = 96,
    INCIDENT_TYPES = 6
};

enum {
    INCIDENT_NVIR = 1,
    INCIDENT_MDEF,
    INCIDENT_WDEF,
    INCIDENT_MACRO,
    INCIDENT_AUTOSTART,
    INCIDENT_SEVENDUST
};

typedef struct {
    int max_y;
    int max_x;
    int color_count;

    CfGame game;
    int cursor_col;
    bool game_over;
    int winner;

    char status[160];

    char loss_msg[LOSS_MSG_CHARS];
    int loss_msg_len;

    char vm_logs[VM_LOG_LINES][VM_LOG_CHARS];
    int vm_log_count;
    time_t vm_boot_time;
    unsigned long vm_ticks;
    char vm_current_alert[80];

    int incident_stacks[INCIDENT_TYPES];
    int last_incident_code;
    int total_losses;
    int total_wins;
    int compromised_pct;

    bool blocked_cols[CF_COLS];
    int blocked_count;
    int active_input_glitch_pct;
    int active_forced_move_pct;
    int active_control_shift;
    int active_control_direction;
    int active_ai_depth_bonus;
    int active_ai_opening_moves;
    int active_player_piece_corrupt_pct;
    int active_flip_turns_remaining;
    int active_purple_turns_remaining;
    char effect_summary[220];
} AppState;

static unsigned int make_seed(void) {
    return (unsigned int)time(NULL) ^ (unsigned int)getpid();
}

static int clamp_int(int value, int lo, int hi) {
    if (value < lo) {
        return lo;
    }
    if (value > hi) {
        return hi;
    }
    return value;
}

static int normalize_col(int col) {
    while (col < 0) {
        col += CF_COLS;
    }
    while (col >= CF_COLS) {
        col -= CF_COLS;
    }
    return col;
}

static int random_color_pair(int color_count) {
    return (color_count > 0) ? (rand() % color_count) + 1 : 0;
}

static void app_update_dimensions(AppState *s) {
    getmaxyx(stdscr, s->max_y, s->max_x);
}

static int infection_pressure(const AppState *s) {
    int total = 0;

    for (int i = 0; i < INCIDENT_TYPES; ++i) {
        total += s->incident_stacks[i];
    }

    return total;
}

static int incident_stack(const AppState *s, int code) {
    if (code < 1 || code > INCIDENT_TYPES) {
        return 0;
    }
    return s->incident_stacks[code - 1];
}

static int max_incident_stack(const AppState *s) {
    int max_stack = 0;

    for (int i = 0; i < INCIDENT_TYPES; ++i) {
        if (s->incident_stacks[i] > max_stack) {
            max_stack = s->incident_stacks[i];
        }
    }

    return max_stack;
}

static int compromised_floor(const AppState *s) {
    int pressure = infection_pressure(s);
    int max_stack = max_incident_stack(s);
    int floor_pct = pressure * 4 + max_stack * 5;
    return clamp_int(floor_pct, 0, 96);
}

static void sync_compromised_floor(AppState *s) {
    int floor_pct = compromised_floor(s);
    if (s->compromised_pct < floor_pct) {
        s->compromised_pct = floor_pct;
    }
    s->compromised_pct = clamp_int(s->compromised_pct, 0, 100);
}

static const char *player_greeting(const AppState *s) {
    if (s->compromised_pct >= 60) {
        return "Hello Loser";
    }
    return "Hello Player";
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

static void vm_clear_logs(AppState *s) {
    memset(s->vm_logs, 0, sizeof(s->vm_logs));
    s->vm_log_count = 0;
}

static void vm_add_log(AppState *s, const char *fmt, ...) {
    va_list args;
    char line[VM_LOG_CHARS];

    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);

    if (s->vm_log_count < VM_LOG_LINES) {
        snprintf(s->vm_logs[s->vm_log_count], VM_LOG_CHARS, "%s", line);
        s->vm_log_count += 1;
        return;
    }

    memmove(s->vm_logs, s->vm_logs + 1, sizeof(s->vm_logs[0]) * (VM_LOG_LINES - 1));
    snprintf(s->vm_logs[VM_LOG_LINES - 1], VM_LOG_CHARS, "%s", line);
}

static void vm_set_alert(AppState *s, const char *alert, const char *loss_msg) {
    snprintf(s->vm_current_alert, sizeof(s->vm_current_alert), "%s", alert);
    snprintf(s->loss_msg, sizeof(s->loss_msg), "%s", loss_msg);
    s->loss_msg_len = (int)strlen(s->loss_msg);
}

static bool is_playable_col(const AppState *s, int col) {
    if (col < 0 || col >= CF_COLS) {
        return false;
    }
    if (s->blocked_cols[col]) {
        return false;
    }
    return cf_is_valid_move(&s->game, col);
}

static int find_first_playable_col(const AppState *s) {
    for (int col = 0; col < CF_COLS; ++col) {
        if (is_playable_col(s, col)) {
            return col;
        }
    }
    return -1;
}

static bool round_is_draw(const AppState *s) {
    return find_first_playable_col(s) < 0;
}

static bool is_grid_flipped(const AppState *s) {
    return s->active_flip_turns_remaining > 0;
}

static bool is_purple_takeover(const AppState *s) {
    return s->active_purple_turns_remaining > 0;
}

static int logical_col_from_display(const AppState *s, int display_col) {
    if (!is_grid_flipped(s)) {
        return display_col;
    }
    return (CF_COLS - 1) - display_col;
}

static int apply_flip_to_drop_col(const AppState *s, int col) {
    if (!is_grid_flipped(s)) {
        return col;
    }
    return (CF_COLS - 1) - col;
}

static void choose_blocked_columns(AppState *s, int count) {
    int pool[CF_COLS];

    memset(s->blocked_cols, 0, sizeof(s->blocked_cols));
    s->blocked_count = 0;

    if (count <= 0) {
        return;
    }

    for (int i = 0; i < CF_COLS; ++i) {
        pool[i] = i;
    }

    for (int i = CF_COLS - 1; i > 0; --i) {
        int j = rand() % (i + 1);
        int tmp = pool[i];
        pool[i] = pool[j];
        pool[j] = tmp;
    }

    count = clamp_int(count, 0, CF_COLS - 1);
    for (int i = 0; i < count; ++i) {
        s->blocked_cols[pool[i]] = true;
        s->blocked_count += 1;
    }
}

static void append_effect_segment(char *dst, size_t cap, bool *first, const char *segment) {
    size_t len = strlen(dst);

    if (!*first && len < cap - 1) {
        strncat(dst, " | ", cap - len - 1);
        len = strlen(dst);
    }

    if (len < cap - 1) {
        strncat(dst, segment, cap - len - 1);
    }

    *first = false;
}

static void build_effect_summary(AppState *s) {
    bool first = true;
    char piece[80];

    snprintf(s->effect_summary, sizeof(s->effect_summary), "Round effects: ");

    if (infection_pressure(s) == 0 && s->compromised_pct == 0) {
        append_effect_segment(s->effect_summary, sizeof(s->effect_summary), &first, "clean boot");
        return;
    }

    if (s->active_input_glitch_pct > 0) {
        snprintf(piece, sizeof(piece), "nVIR jitter %d%%", s->active_input_glitch_pct);
        append_effect_segment(s->effect_summary, sizeof(s->effect_summary), &first, piece);
    }
    if (s->active_forced_move_pct > 0) {
        snprintf(piece, sizeof(piece), "forced move %d%%", s->active_forced_move_pct);
        append_effect_segment(s->effect_summary, sizeof(s->effect_summary), &first, piece);
    }
    if (s->active_control_shift > 0) {
        snprintf(piece, sizeof(piece), "MDEF remap %c%d", s->active_control_direction > 0 ? '+' : '-', s->active_control_shift);
        append_effect_segment(s->effect_summary, sizeof(s->effect_summary), &first, piece);
    }
    if (s->blocked_count > 0) {
        snprintf(piece, sizeof(piece), "WDEF locked cols %d", s->blocked_count);
        append_effect_segment(s->effect_summary, sizeof(s->effect_summary), &first, piece);
    }
    if (s->active_ai_depth_bonus > 0) {
        snprintf(piece, sizeof(piece), "Macro AI +%d", s->active_ai_depth_bonus);
        append_effect_segment(s->effect_summary, sizeof(s->effect_summary), &first, piece);
    }
    if (s->active_ai_opening_moves > 0) {
        snprintf(piece, sizeof(piece), "AutoStart opener x%d", s->active_ai_opening_moves);
        append_effect_segment(s->effect_summary, sizeof(s->effect_summary), &first, piece);
    }
    if (s->active_player_piece_corrupt_pct > 0) {
        snprintf(piece, sizeof(piece), "666 corruption %d%%", s->active_player_piece_corrupt_pct);
        append_effect_segment(s->effect_summary, sizeof(s->effect_summary), &first, piece);
    }
    if (s->active_flip_turns_remaining > 0) {
        append_effect_segment(s->effect_summary, sizeof(s->effect_summary), &first, "grid flip");
    }
    if (s->active_purple_turns_remaining > 0) {
        append_effect_segment(s->effect_summary, sizeof(s->effect_summary), &first, "purple takeover");
    }
}

static int ai_search_depth(const AppState *s) {
    int depth = 6 + s->active_ai_depth_bonus;

    if (infection_pressure(s) >= 12 || s->compromised_pct >= 70) {
        depth += 1;
    }

    return clamp_int(depth, 6, 8);
}

static void apply_round_effects(AppState *s) {
    int nvir = incident_stack(s, INCIDENT_NVIR);
    int mdef = incident_stack(s, INCIDENT_MDEF);
    int wdef = incident_stack(s, INCIDENT_WDEF);
    int macro = incident_stack(s, INCIDENT_MACRO);
    int autostart = incident_stack(s, INCIDENT_AUTOSTART);
    int sevendust = incident_stack(s, INCIDENT_SEVENDUST);
    int pressure = infection_pressure(s);
    int compromised = s->compromised_pct;
    int blocked_count;

    memset(s->blocked_cols, 0, sizeof(s->blocked_cols));
    s->blocked_count = 0;
    s->active_input_glitch_pct = 0;
    s->active_forced_move_pct = 0;
    s->active_control_shift = 0;
    s->active_control_direction = 1;
    s->active_ai_depth_bonus = 0;
    s->active_ai_opening_moves = 0;
    s->active_player_piece_corrupt_pct = 0;
    s->active_flip_turns_remaining = 0;
    s->active_purple_turns_remaining = 0;

    s->active_input_glitch_pct = clamp_int(nvir * 10 + pressure / 4 + compromised / 8, 0, 72);
    if (compromised >= 35) {
        s->active_forced_move_pct = clamp_int((compromised - 30) / 2 + nvir * 6, 0, 68);
    }

    if (mdef > 0) {
        s->active_control_shift = (mdef >= 3 || compromised >= 80) ? 2 : 1;
        s->active_control_direction = (rand() % 2 == 0) ? 1 : -1;
    }

    blocked_count = clamp_int(wdef, 0, 2);
    if (pressure >= 14 && blocked_count < 2 && wdef > 0) {
        blocked_count += 1;
    }
    if (compromised >= 88 && blocked_count < 3 && wdef > 0) {
        blocked_count += 1;
    }
    choose_blocked_columns(s, blocked_count);

    s->active_ai_depth_bonus = clamp_int(macro + (compromised >= 75 ? 1 : 0), 0, 2);

    if (autostart >= 1) {
        s->active_ai_opening_moves = 1;
    }
    if (autostart >= 3) {
        s->active_ai_opening_moves = 2;
    }
    if (compromised >= 92) {
        s->active_ai_opening_moves = 2;
    }

    s->active_player_piece_corrupt_pct = clamp_int(sevendust * 12 + (compromised >= 72 ? 10 : 0), 0, 66);

    if (compromised >= 50) {
        int trigger = 14 + compromised / 4 + incident_stack(s, INCIDENT_MDEF) * 5;
        if ((rand() % 100) < trigger) {
            s->active_flip_turns_remaining = (compromised >= 85) ? 2 : 1;
        }
    }
    if (compromised >= 58) {
        int trigger = 10 + compromised / 5 + incident_stack(s, INCIDENT_SEVENDUST) * 5;
        if ((rand() % 100) < trigger) {
            s->active_purple_turns_remaining = (compromised >= 85) ? 2 : 1;
        }
    }

    build_effect_summary(s);

    vm_add_log(s, "[THREAT] Persistent infection pressure: %d.", pressure);
    vm_add_log(s, "[THREAT] System compromised: %d%%.", compromised);
    vm_add_log(s, "[ROUND] %s", s->effect_summary);

    if (s->blocked_count > 0) {
        vm_add_log(s, "[WDEF] %d columns quarantined this match.", s->blocked_count);
    }
}

static void vm_boot(AppState *s) {
    s->vm_boot_time = time(NULL);
    s->vm_ticks = 0;

    vm_clear_logs(s);
    vm_set_alert(s, "No active incident.", "You lost to AI!");

    vm_add_log(s, "[BOOT] InfiniteMac Hypervisor 9.2.2 (SIMULATED)");
    vm_add_log(s, "[BOOT] Guest: Mac OS 9.2.2 / Finder 9.2");
    vm_add_log(s, "[AV] Legacy defs loaded: Disinfectant archive + heuristics");
    vm_add_log(s, "[NOTE] All incidents are fake terminal effects only.");
}

static void board_clear(AppState *s) {
    cf_init(&s->game);
    s->cursor_col = CF_COLS / 2;
    s->game_over = false;
    s->winner = 0;
    sync_compromised_floor(s);
    snprintf(s->status, sizeof(s->status), "%s. Your move.", player_greeting(s));

    vm_boot(s);
    apply_round_effects(s);
    vm_add_log(
        s,
        "[USER] %s | Record W:%d L:%d | Compromised %d%%",
        player_greeting(s),
        s->total_wins,
        s->total_losses,
        s->compromised_pct
    );

    if (s->active_ai_opening_moves > 0) {
        int dropped = 0;

        for (int i = 0; i < s->active_ai_opening_moves; ++i) {
            int col = cf_ai_choose_move_ex(&s->game, ai_search_depth(s), s->blocked_cols);
            if (col < 0) {
                break;
            }

            cf_drop_piece(&s->game, col, CF_AI);
            dropped += 1;
            vm_add_log(s, "[AUTOSTART] AI opener deployed in column %d.", col + 1);
        }

        if (dropped > 0) {
            snprintf(s->status, sizeof(s->status), "VM boot was compromised. AI opened with %d move(s).", dropped);
        }
    }

    if (s->blocked_cols[s->cursor_col] || !is_playable_col(s, s->cursor_col)) {
        int fallback = find_first_playable_col(s);
        if (fallback >= 0) {
            s->cursor_col = fallback;
        }
    }

    if (round_is_draw(s)) {
        s->game_over = true;
        s->winner = 0;
        snprintf(s->status, sizeof(s->status), "No playable columns this round.");
    }
}

static void reduce_infection_after_player_win(AppState *s) {
    int before = infection_pressure(s);
    int before_pct = s->compromised_pct;

    if (before <= 0) {
        s->total_wins += 1;
        s->compromised_pct = clamp_int(s->compromised_pct - 2, 0, 100);
        return;
    }

    s->total_wins += 1;

    for (int i = 0; i < INCIDENT_TYPES; ++i) {
        if (s->incident_stacks[i] > 0) {
            s->incident_stacks[i] -= 1;
        }
    }

    if (s->last_incident_code >= 1 && s->last_incident_code <= INCIDENT_TYPES) {
        int idx = s->last_incident_code - 1;
        if (s->incident_stacks[idx] > 0) {
            s->incident_stacks[idx] -= 1;
        }
    }

    s->compromised_pct = clamp_int(s->compromised_pct - 5, 0, 100);
    sync_compromised_floor(s);

    vm_add_log(s, "[AV] Recovery sweep lowered threat %d -> %d.", before, infection_pressure(s));
    vm_add_log(s, "[AV] Compromised reduced %d%% -> %d%%.", before_pct, s->compromised_pct);
}

static int remap_drop_col(const AppState *s, int raw_col) {
    if (s->active_control_shift <= 0) {
        return raw_col;
    }

    return normalize_col(raw_col + (s->active_control_direction * s->active_control_shift));
}

static int maybe_glitch_drop_col(AppState *s, int mapped_col) {
    if (s->active_input_glitch_pct <= 0) {
        return mapped_col;
    }

    if ((rand() % 100) >= s->active_input_glitch_pct) {
        return mapped_col;
    }

    int drift = (rand() % 2 == 0) ? -1 : 1;
    if (s->active_input_glitch_pct >= 35 && (rand() % 100) < 35) {
        drift *= 2;
    }

    int glitched = normalize_col(mapped_col + drift);

    beep();
    vm_add_log(s, "[nVIR] Input glitch rerouted %d -> %d.", mapped_col + 1, glitched + 1);
    snprintf(s->status, sizeof(s->status), "Virus jitter moved your drop %d -> %d.", mapped_col + 1, glitched + 1);
    return glitched;
}

static int maybe_forced_virus_move(AppState *s, int raw_col, int current_col) {
    if (s->active_forced_move_pct <= 0) {
        return current_col;
    }

    if ((rand() % 100) >= s->active_forced_move_pct) {
        return current_col;
    }

    int forced = normalize_col(current_col + 1);
    int attempts = 0;

    while (attempts < CF_COLS && !is_playable_col(s, forced)) {
        forced = normalize_col(forced + 1);
        attempts += 1;
    }

    if (attempts >= CF_COLS) {
        return current_col;
    }

    flash();
    beep();
    vm_add_log(s, "[HIJACK] Virus moved drop from %d to %d.", raw_col + 1, forced + 1);
    snprintf(
        s->status,
        sizeof(s->status),
        "Virus moved you haha! Requested %d -> landed %d.",
        raw_col + 1,
        forced + 1
    );
    return forced;
}

static void consume_player_turn_effects(AppState *s) {
    if (s->active_flip_turns_remaining > 0) {
        s->active_flip_turns_remaining -= 1;
    }
    if (s->active_purple_turns_remaining > 0) {
        s->active_purple_turns_remaining -= 1;
    }
}

static void maybe_corrupt_player_piece(AppState *s) {
    int candidate_cols[CF_COLS];
    int candidate_rows[CF_COLS];
    int count = 0;

    if (s->active_player_piece_corrupt_pct <= 0) {
        return;
    }

    if ((rand() % 100) >= s->active_player_piece_corrupt_pct) {
        return;
    }

    for (int col = 0; col < CF_COLS; ++col) {
        for (int row = 0; row < CF_ROWS; ++row) {
            if (s->game.board[row][col] != CF_EMPTY) {
                if (s->game.board[row][col] == CF_HUMAN) {
                    candidate_cols[count] = col;
                    candidate_rows[count] = row;
                    count += 1;
                }
                break;
            }
        }
    }

    if (count == 0) {
        return;
    }

    int pick = rand() % count;
    int col = candidate_cols[pick];
    int row = candidate_rows[pick];

    s->game.board[row][col] = CF_EMPTY;
    if (s->game.moves > 0) {
        s->game.moves -= 1;
    }

    flash();
    vm_add_log(s, "[666] Corruption removed your top token in column %d.", col + 1);
    snprintf(s->status, sizeof(s->status), "Payload hit: your token in column %d was deleted.", col + 1);
}

static void move_cursor_to_next_open(AppState *s, int direction) {
    int base = s->cursor_col;
    int actual_direction = is_grid_flipped(s) ? -direction : direction;

    for (int step = 1; step <= CF_COLS; ++step) {
        int col = normalize_col(base + (step * actual_direction));
        if (!s->blocked_cols[col]) {
            s->cursor_col = col;
            return;
        }
    }
}

static void draw_vm_console(const AppState *s, int start_y) {
    int uptime_seconds = (int)difftime(time(NULL), s->vm_boot_time);
    int mm = uptime_seconds / 60;
    int ss = uptime_seconds % 60;
    const char spinner[4] = {'|', '/', '-', '\\'};
    int width = s->max_x - 1;

    if (start_y >= s->max_y - 2) {
        return;
    }

    if (has_colors()) {
        attron(A_BOLD | COLOR_PAIR(6));
    } else {
        attron(A_BOLD);
    }
    mvprintw(start_y, 0, "Mac OS 9 VM Console [%c] Uptime %02d:%02d", spinner[s->vm_ticks % 4], mm, ss);
    if (has_colors()) {
        attroff(A_BOLD | COLOR_PAIR(6));
    } else {
        attroff(A_BOLD);
    }

    mvprintw(
        start_y + 1,
        0,
        "Alert: %s | Compromised: %d%% | Stacks N:%d M:%d W:%d O:%d A:%d 6:%d",
        s->vm_current_alert,
        s->compromised_pct,
        incident_stack(s, INCIDENT_NVIR),
        incident_stack(s, INCIDENT_MDEF),
        incident_stack(s, INCIDENT_WDEF),
        incident_stack(s, INCIDENT_MACRO),
        incident_stack(s, INCIDENT_AUTOSTART),
        incident_stack(s, INCIDENT_SEVENDUST)
    );

    for (int i = 0; i < width && i < 78; ++i) {
        mvaddch(start_y + 2, i, '-');
    }

    for (int i = 0; i < VM_LOG_LINES; ++i) {
        int row = start_y + 3 + i;
        if (row >= s->max_y) {
            break;
        }

        if (i < s->vm_log_count) {
            mvprintw(row, 0, "%s", s->vm_logs[i]);
        } else {
            mvprintw(row, 0, "");
        }
    }
}

/* --------------------- Rendering --------------------- */
static void draw_board_ui(const AppState *s) {
    int top = 5;
    int grid_y = top + 3;
    int vm_y;
    int info_row = grid_y + CF_ROWS + 2;
    bool flipped = is_grid_flipped(s);
    bool purple = is_purple_takeover(s);

    erase();

    if (has_colors()) {
        attron(A_BOLD | COLOR_PAIR(3));
    } else {
        attron(A_BOLD);
    }
    mvprintw(0, 0, "%s :: Connect Four Virus :: Fake Mac OS 9 VM", player_greeting(s));
    if (has_colors()) {
        attroff(A_BOLD | COLOR_PAIR(3));
    } else {
        attroff(A_BOLD);
    }

    mvprintw(1, 0, "LEFT/RIGHT or A/D move | Enter/Space drop | 1-6 quick select | r restart | q quit");
    mvprintw(
        2,
        0,
        "You = O   AI = X   First to connect 4 wins.   Threat Level: %d   System Compromised: %d%%",
        infection_pressure(s),
        s->compromised_pct
    );
    mvprintw(3, 0, "%s", s->effect_summary);

    mvprintw(top, 0, "   ");
    for (int display_col = 0; display_col < CF_COLS; ++display_col) {
        int logical_col = logical_col_from_display(s, display_col);
        if (s->blocked_cols[logical_col]) {
            if (has_colors()) {
                attron(COLOR_PAIR(5) | A_BOLD);
            }
            printw(" X ");
            if (has_colors()) {
                attroff(COLOR_PAIR(5) | A_BOLD);
            }
        } else {
            printw(" %d ", display_col + 1);
        }
    }

    mvprintw(top + 1, 0, "   ");
    for (int display_col = 0; display_col < CF_COLS; ++display_col) {
        int logical_col = logical_col_from_display(s, display_col);
        if (s->blocked_cols[logical_col]) {
            printw(" x ");
        } else if (logical_col == s->cursor_col && !s->game_over) {
            attron(A_REVERSE);
            printw(" ^ ");
            attroff(A_REVERSE);
        } else {
            printw("   ");
        }
    }

    for (int r = 0; r < CF_ROWS; ++r) {
        mvprintw(grid_y + r, 0, "%d |", r);
        for (int display_col = 0; display_col < CF_COLS; ++display_col) {
            int logical_col = logical_col_from_display(s, display_col);
            CfCell cell = s->game.board[r][logical_col];
            char token = '.';
            int pair = 0;

            if (cell == CF_HUMAN) {
                token = 'O';
                pair = purple ? 5 : 2;
            } else if (cell == CF_AI) {
                token = 'X';
                pair = purple ? 5 : 1;
            } else if (s->blocked_cols[logical_col]) {
                token = '#';
                pair = 5;
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

    if (s->active_control_shift > 0) {
        int mapped = remap_drop_col(s, s->cursor_col);
        mvprintw(
            info_row,
            0,
            "Control remap active: selected %d -> mapped %d",
            s->cursor_col + 1,
            mapped + 1
        );
        info_row += 1;
    }

    if (s->active_forced_move_pct > 0) {
        mvprintw(info_row, 0, "Forced virus move chance: %d%%", s->active_forced_move_pct);
        info_row += 1;
    }

    if (flipped) {
        mvprintw(info_row, 0, "Grid inversion active for %d turn(s).", s->active_flip_turns_remaining);
        info_row += 1;
    }

    if (purple) {
        mvprintw(info_row, 0, "Purple takeover active for %d turn(s).", s->active_purple_turns_remaining);
        info_row += 1;
    }

    if (s->game_over) {
        if (has_colors()) {
            attron(A_BOLD | COLOR_PAIR(4));
        } else {
            attron(A_BOLD);
        }

        if (s->winner == 1) {
            mvprintw(info_row, 0, "You win. Press r to play again or q to quit.");
        } else if (s->winner == 2) {
            mvprintw(info_row, 0, "AI wins. Press r to play again or q to quit.");
        } else {
            mvprintw(info_row, 0, "Draw. Press r to play again or q to quit.");
        }

        if (has_colors()) {
            attroff(A_BOLD | COLOR_PAIR(4));
        } else {
            attroff(A_BOLD);
        }
    }

    vm_y = info_row + 2;
    if (vm_y < grid_y + CF_ROWS + 5) {
        vm_y = grid_y + CF_ROWS + 5;
    }

    draw_vm_console(s, vm_y);
    refresh();
}

/* --------------------- "Punishment" action (safe VM incident simulation) --------------------- */
static void run_punishment_action(AppState *s) {
    int code = 1 + (rand() % INCIDENT_TYPES);
    int y = s->max_y - 13;
    int *stack = &s->incident_stacks[code - 1];
    int severity;
    int pressure;

    const char *incident = "Unknown";
    const char *l1 = "";
    const char *l2 = "";
    const char *l3 = "";
    const char *l4 = "";
    const char *impact = "";

    if (y < 2) {
        y = 2;
    }

    *stack += 1;
    severity = *stack;
    pressure = infection_pressure(s);
    s->last_incident_code = code;
    s->total_losses += 1;
    s->compromised_pct = clamp_int(s->compromised_pct + 6 + severity * 3, 0, 100);
    sync_compromised_floor(s);

    switch (code) {
        case INCIDENT_NVIR:
            incident = "nVIR family";
            vm_set_alert(s, "nVIR-like resource infection detected.", "[nVIR] Don't panic!");
            l1 = "[nVIR] System file resource fork patched (simulated).";
            l2 = "[nVIR] Random beep payload triggered.";
            l3 = "[nVIR] MacinTalk ghost message: \"Don't panic!\"";
            l4 = "[AV] Quarantine complete. No host changes were made.";
            impact = "Next games: input jitter and random move reroutes intensify.";
            beep();
            beep();
            vm_add_log(s, "[ALERT] nVIR signature matched in guest System file.");
            break;

        case INCIDENT_MDEF:
            incident = "MDEF / Garfield + CDEF";
            vm_set_alert(s, "Menu definition resources corrupted.", "[MDEF] Menus are cursed");
            l1 = "[MDEF] Menu manager hooks replaced (simulated).";
            l2 = "[CDEF] Control definition conflict injects visual glitches.";
            l3 = "[UI] Menus become garbled; random crash dialog appears.";
            l4 = "[AV] Restored clean menu resources in fake VM snapshot.";
            impact = "Next games: control remap drift gets stronger.";
            flash();
            vm_add_log(s, "[ALERT] MDEF/CDEF resource tampering event.");
            break;

        case INCIDENT_WDEF:
            incident = "WDEF + Zuc floppy chain";
            vm_set_alert(s, "Desktop and floppy boot chain anomalies.", "[WDEF] Desktop file chaos");
            l1 = "[WDEF] Desktop file metadata drift detected.";
            l2 = "[Zuc] Infected floppy boot block mounted (simulated).";
            l3 = "[FINDER] Icons flicker, folder views degrade, boot slows down.";
            l4 = "[AV] Desktop rebuilt; floppy image isolated from startup path.";
            impact = "Next games: locked columns persist and stack with repeats.";
            for (int i = 0; i < 2; ++i) {
                beep();
                usleep(180000);
            }
            vm_add_log(s, "[ALERT] WDEF desktop integrity mismatch.");
            break;

        case INCIDENT_MACRO:
            incident = "Office macro wave (Concept/Laroux)";
            vm_set_alert(s, "Macro propagation via shared docs.", "[MACRO] Concept/Laroux spread");
            l1 = "[DOC] Word template altered by Concept-like macro (simulated).";
            l2 = "[XLS] Laroux-style macro copied into workbook startup path.";
            l3 = "[NET] Cross-platform file share became infection route.";
            l4 = "[AV] Macros disabled and startup templates replaced.";
            impact = "Next games: AI search depth increases.";
            flash();
            beep();
            vm_add_log(s, "[ALERT] Macro payload detected in Office documents.");
            break;

        case INCIDENT_AUTOSTART:
            incident = "AutoStart 9805 worm";
            vm_set_alert(s, "AutoStart media autorun exploited.", "[AUTOSTART] CD worm loaded");
            l1 = "[CD-ROM] AutoStart trigger fired on media insert (simulated).";
            l2 = "[WORM] Autorun app copied itself to removable volumes.";
            l3 = "[CHAIN] No click required once disc was inserted.";
            l4 = "[AV] AutoStart disabled in guest control panel profile.";
            impact = "Next games: AI starts with opening move(s).";
            flash();
            vm_add_log(s, "[ALERT] AutoStart worm behavior in guest media stack.");
            break;

        case INCIDENT_SEVENDUST:
        default:
            incident = "SevenDust / 666 polymorph";
            vm_set_alert(s, "SevenDust timed payload window entered.", "[666] Timed payload trip");
            l1 = "[666] MDEF-extension polymorph variant A/F observed.";
            l2 = "[TIME] 06:00-07:00 trigger window reached (simulated).";
            l3 = "[PAYLOAD] Attempted non-app file deletion on startup disk.";
            l4 = "[AV] Snapshot rollback blocked all destructive writes.";
            impact = "Next games: your placed tokens may randomly be deleted.";
            for (int i = 0; i < 3; ++i) {
                flash();
                usleep(200000);
            }
            vm_add_log(s, "[ALERT] SevenDust/666 polymorphic chain detected.");
            break;
    }

    vm_add_log(s, "[STACK] %s severity increased to %d.", incident, severity);
    vm_add_log(s, "[THREAT] Global pressure now %d.", pressure);
    vm_add_log(s, "[THREAT] System compromised now %d%%.", s->compromised_pct);

    if (has_colors()) {
        attron(A_BOLD | COLOR_PAIR(1));
    } else {
        attron(A_BOLD);
    }
    mvprintw(y, 2, "AI VICTORY TAX COLLECTED! [Persistent Incident] ");
    if (has_colors()) {
        attroff(A_BOLD | COLOR_PAIR(1));
    } else {
        attroff(A_BOLD);
    }

    mvprintw(y + 1, 4, "Incident: %s", incident);
    mvprintw(y + 2, 4, "%s", l1);
    mvprintw(y + 3, 4, "%s", l2);
    mvprintw(y + 4, 4, "%s", l3);
    mvprintw(y + 5, 4, "%s", l4);
    mvprintw(y + 6, 4, "Stack level: %d (repeats get worse)", severity);
    mvprintw(y + 7, 4, "Threat level: %d", pressure);
    mvprintw(y + 8, 4, "System compromised: %d%%", s->compromised_pct);
    mvprintw(y + 9, 4, "%s", impact);
    mvprintw(y + 10, 4, "Press any key to acknowledge incident report.");

    flushinp();
    refresh();
    while (getch() == ERR) {
        usleep(100000);
    }

    for (int i = 0; i < 12; ++i) {
        move(y + i, 0);
        clrtoeol();
    }
}

/* --------------------- Loss flood --------------------- */
static void show_loss_squiggles(AppState *s) {
    erase();
    mvprintw(0, 0, "Classic Mac VM corruption mode: press any key to return...");
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
    int pick = cf_ai_choose_move_ex(&s->game, ai_search_depth(s), s->blocked_cols);
    if (pick >= 0) {
        cf_drop_piece(&s->game, pick, CF_AI);
        vm_add_log(s, "[MOVE] AI dropped in column %d.", pick + 1);
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

        s.vm_ticks += 1;
        app_update_dimensions(&s);
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
            move_cursor_to_next_open(&s, -1);
            continue;
        }
        if (ch == KEY_RIGHT || ch == 'd' || ch == 'D') {
            move_cursor_to_next_open(&s, 1);
            continue;
        }
        if (ch >= '1' && ch <= '0' + CF_COLS) {
            int requested = ch - '1';
            s.cursor_col = requested;
            if (s.blocked_cols[requested]) {
                snprintf(s.status, sizeof(s.status), "Column %d is locked this round.", requested + 1);
            }
            continue;
        }

        if (ch == ' ' || ch == '\n' || ch == KEY_ENTER) {
            int raw_col = s.cursor_col;
            int mapped_col = remap_drop_col(&s, raw_col);
            int flipped_col = apply_flip_to_drop_col(&s, mapped_col);
            int final_col = maybe_glitch_drop_col(&s, flipped_col);
            int before_forced = final_col;
            int ai_col;

            if (flipped_col != mapped_col) {
                vm_add_log(&s, "[MIRROR] Grid flip redirected %d -> %d.", mapped_col + 1, flipped_col + 1);
            }

            final_col = maybe_forced_virus_move(&s, raw_col, final_col);
            if (final_col != before_forced) {
                draw_board_ui(&s);
                usleep(180000);
            }

            if (!is_playable_col(&s, final_col)) {
                beep();
                snprintf(
                    s.status,
                    sizeof(s.status),
                    "Mapped column %d is unavailable (raw %d).",
                    final_col + 1,
                    raw_col + 1
                );
                continue;
            }

            cf_drop_piece(&s.game, final_col, CF_HUMAN);
            if (final_col == raw_col) {
                vm_add_log(&s, "[MOVE] Human dropped in column %d.", final_col + 1);
            } else {
                vm_add_log(&s, "[MOVE] Human selected %d -> landed %d.", raw_col + 1, final_col + 1);
            }
            consume_player_turn_effects(&s);

            if (cf_has_winner(&s.game, CF_HUMAN)) {
                s.game_over = true;
                s.winner = 1;
                snprintf(s.status, sizeof(s.status), "You connected four first.");
                vm_set_alert(&s, "No active incident.", "You beat the VM");
                reduce_infection_after_player_win(&s);
                vm_add_log(&s, "[RESULT] Human victory. Guest stabilized.");
                continue;
            }
            if (round_is_draw(&s)) {
                s.game_over = true;
                s.winner = 0;
                snprintf(s.status, sizeof(s.status), "No playable columns remain.");
                vm_add_log(&s, "[RESULT] Draw. No incident triggered.");
                continue;
            }

            maybe_corrupt_player_piece(&s);

            if (round_is_draw(&s)) {
                s.game_over = true;
                s.winner = 0;
                snprintf(s.status, sizeof(s.status), "No playable columns remain.");
                vm_add_log(&s, "[RESULT] Draw after corruption pulse.");
                continue;
            }

            snprintf(s.status, sizeof(s.status), "AI is thinking...");
            draw_board_ui(&s);
            usleep(220000);

            ai_col = ai_take_turn(&s);
            if (ai_col < 0) {
                s.game_over = true;
                s.winner = 0;
                snprintf(s.status, sizeof(s.status), "No playable columns remain.");
                vm_add_log(&s, "[RESULT] Draw. Move queue exhausted.");
                continue;
            }

            if (cf_has_winner(&s.game, CF_AI)) {
                s.game_over = true;
                s.winner = 2;
                snprintf(s.status, sizeof(s.status), "AI played column %d and won.", ai_col + 1);
                vm_add_log(&s, "[RESULT] AI victory. Incident simulation armed.");

                app_update_dimensions(&s);
                run_punishment_action(&s);
                show_loss_squiggles(&s);
                vm_add_log(&s, "[INFO] Incident overlay dismissed by operator.");
                continue;
            }
            if (round_is_draw(&s)) {
                s.game_over = true;
                s.winner = 0;
                snprintf(s.status, sizeof(s.status), "No playable columns remain.");
                vm_add_log(&s, "[RESULT] Draw. Guest state unchanged.");
                continue;
            }

            snprintf(s.status, sizeof(s.status), "AI played column %d. Your move.", ai_col + 1);
        }
    }

    nc_shutdown();
    return 0;
}
