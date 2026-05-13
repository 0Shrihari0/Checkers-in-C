#ifndef BOARD_H
#define BOARD_H

#define BOARD_SIZE 8

/* Player information structure */
typedef struct {
    char name[50];
    int  score;
    char symbol;     /* normal piece symbol */
    char king;       /* king piece symbol */
    int  forwardDir; /* -1 for P1 (moves up), +1 for P2 (moves down) */
    int  backRow;    /* promotion row: 0 for P1, 7 for P2 */
} PlayerInfo;

/* Create a new 8x8 board (dynamically allocated). Returns NULL on failure. */
char **board_create(void);

/* Free a board created with board_create. */
void board_destroy(char **board);

/* Initialize board with starting piece positions on dark squares. */
void board_init(char **board, char p1Symbol, char p2Symbol);

/* Get the piece at a board position. Returns ' ' if out of bounds. */
char board_get(char **board, int row, int col);

/* Count how many pieces of a given symbol are on the board. */
int board_count_pieces(char **board, char symbol);

/* Check if a cell contains a player's piece (normal or king). */
int board_is_player_piece(char cell, char normal, char king);

/* Check if a cell contains an opponent's piece (normal or king). */
int board_is_opponent_piece(char cell, char oppNormal, char oppKing);

#endif
