#ifndef CONNECT_FOUR_AI_H
#define CONNECT_FOUR_AI_H

#include "connect_four.h"

int cf_ai_choose_move(CfGame *game, int depth);
int cf_ai_choose_move_ex(CfGame *game, int depth, const bool blocked_cols[CF_COLS]);

#endif
