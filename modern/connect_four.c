#include "connect_four.h"

void cf_init(CfGame *game) {
    for (int row = 0; row < CF_ROWS; ++row) {
        for (int col = 0; col < CF_COLS; ++col) {
            game->board[row][col] = CF_EMPTY;
        }
    }
    game->moves = 0;
}

bool cf_is_valid_move(const CfGame *game, int col) {
    return col >= 0 && col < CF_COLS && game->board[0][col] == CF_EMPTY;
}

int cf_drop_piece(CfGame *game, int col, CfCell piece) {
    if (!cf_is_valid_move(game, col)) {
        return -1;
    }

    for (int row = CF_ROWS - 1; row >= 0; --row) {
        if (game->board[row][col] == CF_EMPTY) {
            game->board[row][col] = piece;
            game->moves += 1;
            return row;
        }
    }

    return -1;
}

bool cf_undo_piece(CfGame *game, int col) {
    if (col < 0 || col >= CF_COLS) {
        return false;
    }

    for (int row = 0; row < CF_ROWS; ++row) {
        if (game->board[row][col] != CF_EMPTY) {
            game->board[row][col] = CF_EMPTY;
            if (game->moves > 0) {
                game->moves -= 1;
            }
            return true;
        }
    }

    return false;
}

bool cf_has_winner(const CfGame *game, CfCell piece) {
    for (int row = 0; row < CF_ROWS; ++row) {
        for (int col = 0; col <= CF_COLS - 4; ++col) {
            if (game->board[row][col] == piece &&
                game->board[row][col + 1] == piece &&
                game->board[row][col + 2] == piece &&
                game->board[row][col + 3] == piece) {
                return true;
            }
        }
    }

    for (int row = 0; row <= CF_ROWS - 4; ++row) {
        for (int col = 0; col < CF_COLS; ++col) {
            if (game->board[row][col] == piece &&
                game->board[row + 1][col] == piece &&
                game->board[row + 2][col] == piece &&
                game->board[row + 3][col] == piece) {
                return true;
            }
        }
    }

    for (int row = 0; row <= CF_ROWS - 4; ++row) {
        for (int col = 0; col <= CF_COLS - 4; ++col) {
            if (game->board[row][col] == piece &&
                game->board[row + 1][col + 1] == piece &&
                game->board[row + 2][col + 2] == piece &&
                game->board[row + 3][col + 3] == piece) {
                return true;
            }
        }
    }

    for (int row = 3; row < CF_ROWS; ++row) {
        for (int col = 0; col <= CF_COLS - 4; ++col) {
            if (game->board[row][col] == piece &&
                game->board[row - 1][col + 1] == piece &&
                game->board[row - 2][col + 2] == piece &&
                game->board[row - 3][col + 3] == piece) {
                return true;
            }
        }
    }

    return false;
}

bool cf_is_draw(const CfGame *game) {
    return game->moves >= CF_ROWS * CF_COLS;
}

int cf_valid_moves(const CfGame *game, int out_cols[CF_COLS]) {
    static const int kPreferredOrder[CF_COLS] = {2, 3, 1, 4, 0, 5};
    int count = 0;

    for (int i = 0; i < CF_COLS; ++i) {
        int col = kPreferredOrder[i];
        if (cf_is_valid_move(game, col)) {
            out_cols[count++] = col;
        }
    }

    return count;
}
