#ifndef RULES_H
#define RULES_H

#include "board.h"

/* Validate if a direction (1-4) is allowed for the given piece type.
   Kings can move in any direction; normal pieces only move forward. */
int rules_is_valid_direction(int direction, char piece,
                             char p1Normal, char p1King,
                             char p2Normal, char p2King);

/* Check and apply king promotion if piece reached its back row.
   Returns 1 if promoted, 0 otherwise. Does NOT print anything. */
int rules_check_promotion(char **board, int row, int col,
                          char normal, char king, int backRow);

/* Calculate scores for both players based on board state.
   Normal pieces = 5 points, kings = 10 points. */
void rules_calc_scores(char **board, PlayerInfo *p1, PlayerInfo *p2);

/* Check if the game is over (a player has no pieces).
   Returns 1 if game over, 0 if still playing. */
int rules_is_game_over(char **board, PlayerInfo *p1, PlayerInfo *p2);

/* Check if a player has any valid move (simple move or capture). */
int rules_has_valid_move(char **board, char mySymbol, char oppSymbol,
                         char myKing, char oppKing, int forwardDr);

/* Parse column letter (A-H, case insensitive) to index 0-7.
   Returns -1 on invalid input. */
int rules_parse_col(char c);

/* Parse row character ('1'-'8') to index 0-7.
   Returns -1 on invalid input. */
int rules_parse_row(char c);

/* Assign king symbols to both players, handling collisions. */
void rules_assign_king_symbols(PlayerInfo *p1, PlayerInfo *p2);

#endif
