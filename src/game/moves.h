#ifndef MOVES_H
#define MOVES_H

/* Move result codes */
#define MOVE_INVALID  0
#define MOVE_SIMPLE   1
#define MOVE_CAPTURE  2

/* Attempt a move or capture in the given direction.
   Updates row/col to the new position on success.
   Returns: MOVE_INVALID, MOVE_SIMPLE, or MOVE_CAPTURE. */
int moves_try(char **board, int *row, int *col, int dr, int dc,
              char myNormal, char myKing, char oppNormal, char oppKing);

/* Check if a piece at (row,col) can capture in any valid direction.
   forwardDr: the forward direction for non-king pieces. */
int moves_can_capture_from(char **board, int row, int col,
                           char myNormal, char myKing,
                           char oppNormal, char oppKing,
                           int forwardDr);

/* Check if ANY piece of a player has a capture available. */
int moves_player_has_capture(char **board,
                             char myNormal, char myKing,
                             char oppNormal, char oppKing,
                             int forwardDr);

/* Check if a specific direction from (row,col) has a valid capture. */
int moves_is_capture_in_dir(char **board, int row, int col,
                            int dr, int dc,
                            char oppNormal, char oppKing);

/* Convert direction number (1-4) to row/col offsets.
   1=upper-left, 2=upper-right, 3=lower-left, 4=lower-right. */
void moves_get_dir_offsets(int direction, int *dr, int *dc);

#endif
