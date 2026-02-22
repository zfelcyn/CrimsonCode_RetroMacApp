#include "connect_four_ai.h"

#include <limits.h>
#include <stdlib.h>

enum {
    WIN_SCORE = 100000000,
    LOSS_SCORE = -100000000
};

static bool is_col_blocked(const bool blocked_cols[CF_COLS], int col) {
    return blocked_cols != NULL && blocked_cols[col];
}

static int collect_valid_moves(
    const CfGame *game,
    const bool blocked_cols[CF_COLS],
    int out_cols[CF_COLS]
) {
    int ordered_cols[CF_COLS];
    int ordered_count = cf_valid_moves(game, ordered_cols);
    int filtered_count = 0;

    for (int i = 0; i < ordered_count; ++i) {
        int col = ordered_cols[i];
        if (!is_col_blocked(blocked_cols, col)) {
            out_cols[filtered_count++] = col;
        }
    }

    return filtered_count;
}

static int center_distance(int col) {
    int midpoint_scaled = CF_COLS - 1;
    int col_scaled = col * 2;
    int distance = col_scaled - midpoint_scaled;
    return distance < 0 ? -distance : distance;
}

static bool is_better_tie_break(int candidate, int current) {
    return center_distance(candidate) < center_distance(current);
}

static int evaluate_window(const CfCell window[4]) {
    int ai_count = 0;
    int human_count = 0;
    int empty_count = 0;

    for (int i = 0; i < 4; ++i) {
        if (window[i] == CF_AI) {
            ai_count += 1;
        } else if (window[i] == CF_HUMAN) {
            human_count += 1;
        } else {
            empty_count += 1;
        }
    }

    if (ai_count == 4) {
        return 100000;
    }
    if (human_count == 4) {
        return -100000;
    }
    if (ai_count == 3 && empty_count == 1) {
        return 120;
    }
    if (ai_count == 2 && empty_count == 2) {
        return 14;
    }
    if (human_count == 3 && empty_count == 1) {
        return -150;
    }
    if (human_count == 2 && empty_count == 2) {
        return -12;
    }

    return 0;
}

static int score_position(const CfGame *game) {
    int score = 0;
    CfCell window[4];

    for (int row = 0; row < CF_ROWS; ++row) {
        if (game->board[row][CF_COLS / 2] == CF_AI) {
            score += 7;
        } else if (game->board[row][CF_COLS / 2] == CF_HUMAN) {
            score -= 7;
        }
    }

    for (int row = 0; row < CF_ROWS; ++row) {
        for (int col = 0; col <= CF_COLS - 4; ++col) {
            for (int i = 0; i < 4; ++i) {
                window[i] = game->board[row][col + i];
            }
            score += evaluate_window(window);
        }
    }

    for (int row = 0; row <= CF_ROWS - 4; ++row) {
        for (int col = 0; col < CF_COLS; ++col) {
            for (int i = 0; i < 4; ++i) {
                window[i] = game->board[row + i][col];
            }
            score += evaluate_window(window);
        }
    }

    for (int row = 0; row <= CF_ROWS - 4; ++row) {
        for (int col = 0; col <= CF_COLS - 4; ++col) {
            for (int i = 0; i < 4; ++i) {
                window[i] = game->board[row + i][col + i];
            }
            score += evaluate_window(window);
        }
    }

    for (int row = 3; row < CF_ROWS; ++row) {
        for (int col = 0; col <= CF_COLS - 4; ++col) {
            for (int i = 0; i < 4; ++i) {
                window[i] = game->board[row - i][col + i];
            }
            score += evaluate_window(window);
        }
    }

    return score;
}

static int minimax(
    CfGame *game,
    int depth,
    int alpha,
    int beta,
    bool maximizing,
    int ply,
    int *best_col,
    const bool blocked_cols[CF_COLS]
) {
    int valid_cols[CF_COLS];
    int valid_count = collect_valid_moves(game, blocked_cols, valid_cols);

    if (cf_has_winner(game, CF_AI)) {
        return WIN_SCORE - ply;
    }
    if (cf_has_winner(game, CF_HUMAN)) {
        return LOSS_SCORE + ply;
    }
    if (depth == 0 || valid_count == 0) {
        return score_position(game);
    }

    if (maximizing) {
        int best_score = INT_MIN;
        int local_best = valid_cols[0];

        for (int i = 0; i < valid_count; ++i) {
            int col = valid_cols[i];
            int score;

            cf_drop_piece(game, col, CF_AI);
            score = minimax(game, depth - 1, alpha, beta, false, ply + 1, NULL, blocked_cols);
            cf_undo_piece(game, col);

            if (score > best_score || (score == best_score && is_better_tie_break(col, local_best))) {
                best_score = score;
                local_best = col;
            }

            if (best_score > alpha) {
                alpha = best_score;
            }
            if (alpha >= beta) {
                break;
            }
        }

        if (best_col != NULL) {
            *best_col = local_best;
        }
        return best_score;
    }

    int best_score = INT_MAX;
    int local_best = valid_cols[0];

    for (int i = 0; i < valid_count; ++i) {
        int col = valid_cols[i];
        int score;

        cf_drop_piece(game, col, CF_HUMAN);
        score = minimax(game, depth - 1, alpha, beta, true, ply + 1, NULL, blocked_cols);
        cf_undo_piece(game, col);

        if (score < best_score || (score == best_score && is_better_tie_break(col, local_best))) {
            best_score = score;
            local_best = col;
        }

        if (best_score < beta) {
            beta = best_score;
        }
        if (alpha >= beta) {
            break;
        }
    }

    if (best_col != NULL) {
        *best_col = local_best;
    }
    return best_score;
}

int cf_ai_choose_move_ex(CfGame *game, int depth, const bool blocked_cols[CF_COLS]) {
    int valid_cols[CF_COLS];
    int valid_count = collect_valid_moves(game, blocked_cols, valid_cols);
    int forced_block = -1;
    int best = -1;
    int empties = CF_ROWS * CF_COLS - game->moves;
    int search_depth = depth;

    if (valid_count == 0) {
        return -1;
    }

    for (int i = 0; i < valid_count; ++i) {
        int col = valid_cols[i];
        cf_drop_piece(game, col, CF_AI);
        if (cf_has_winner(game, CF_AI)) {
            cf_undo_piece(game, col);
            return col;
        }
        cf_undo_piece(game, col);
    }

    for (int i = 0; i < valid_count; ++i) {
        int col = valid_cols[i];
        cf_drop_piece(game, col, CF_HUMAN);
        if (cf_has_winner(game, CF_HUMAN)) {
            if (forced_block < 0 || is_better_tie_break(col, forced_block)) {
                forced_block = col;
            }
        }
        cf_undo_piece(game, col);
    }
    if (forced_block >= 0) {
        return forced_block;
    }

    if (search_depth < 1) {
        search_depth = 1;
    }
    if (search_depth > 8) {
        search_depth = 8;
    }
    if (empties <= 20 && search_depth < 7) {
        search_depth = 7;
    }
    if (empties <= 12 && search_depth < 8) {
        search_depth = 8;
    }

    minimax(game, search_depth, INT_MIN, INT_MAX, true, 0, &best, blocked_cols);

    if (best < 0) {
        return valid_cols[0];
    }
    return best;
}

int cf_ai_choose_move(CfGame *game, int depth) {
    return cf_ai_choose_move_ex(game, depth, NULL);
}
