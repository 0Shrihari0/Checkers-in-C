/*
 * ai.h — Checkers AI opponent with multiple difficulty levels.
 *
 * Easy:   Random valid move
 * Medium: Prioritize captures, otherwise random
 * Hard:   Minimax with alpha-beta pruning (depth 6)
 */
#ifndef AI_H
#define AI_H

#include "../src/game/board.h"
#include "../shared/protocol.h"

/* Choose a move for the given player.
   Fills fromR/fromC/toR/toC with the chosen move.
   Returns 0 on success, -1 if no valid move exists. */
int ai_choose_move(char **board, PlayerInfo *me, PlayerInfo *opp,
                   int *fromR, int *fromC, int *toR, int *toC,
                   int difficulty);

#endif
