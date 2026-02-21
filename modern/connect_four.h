#ifndef CONNECT_FOUR_H
#define CONNECT_FOUR_H

#include <stdbool.h>

#define CF_ROWS 6
#define CF_COLS 6

typedef enum {
    CF_EMPTY = 0,
    CF_HUMAN = 1,
    CF_AI = 2
} CfCell;

typedef struct {
    CfCell board[CF_ROWS][CF_COLS];
    int moves;
} CfGame;

void cf_init(CfGame *game);
bool cf_is_valid_move(const CfGame *game, int col);
int cf_drop_piece(CfGame *game, int col, CfCell piece);
bool cf_undo_piece(CfGame *game, int col);
bool cf_has_winner(const CfGame *game, CfCell piece);
bool cf_is_draw(const CfGame *game);
int cf_valid_moves(const CfGame *game, int out_cols[CF_COLS]);

#endif
