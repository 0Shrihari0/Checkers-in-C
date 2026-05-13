#include "board.h"
#include <stdlib.h>

char **board_create(void) {
    char **board = (char **)malloc(BOARD_SIZE * sizeof(char *));
    if (!board) return NULL;

    for (int i = 0; i < BOARD_SIZE; i++) {
        board[i] = (char *)malloc(BOARD_SIZE * sizeof(char));
        if (!board[i]) {
            for (int j = 0; j < i; j++) free(board[j]);
            free(board);
            return NULL;
        }
    }

    /* Initialize all squares to empty */
    for (int i = 0; i < BOARD_SIZE; i++)
        for (int j = 0; j < BOARD_SIZE; j++)
            board[i][j] = ' ';

    return board;
}

void board_destroy(char **board) {
    if (!board) return;
    for (int i = 0; i < BOARD_SIZE; i++)
        free(board[i]);
    free(board);
}

void board_init(char **board, char p1Symbol, char p2Symbol) {
    /* P2 pieces at rows 0-2 (top of board) */
    board[0][1] = p2Symbol; board[0][3] = p2Symbol;
    board[0][5] = p2Symbol; board[0][7] = p2Symbol;
    board[1][0] = p2Symbol; board[1][2] = p2Symbol;
    board[1][4] = p2Symbol; board[1][6] = p2Symbol;
    board[2][1] = p2Symbol; board[2][3] = p2Symbol;
    board[2][5] = p2Symbol; board[2][7] = p2Symbol;

    /* P1 pieces at rows 5-7 (bottom of board) */
    board[5][0] = p1Symbol; board[5][2] = p1Symbol;
    board[5][4] = p1Symbol; board[5][6] = p1Symbol;
    board[6][1] = p1Symbol; board[6][3] = p1Symbol;
    board[6][5] = p1Symbol; board[6][7] = p1Symbol;
    board[7][0] = p1Symbol; board[7][2] = p1Symbol;
    board[7][4] = p1Symbol; board[7][6] = p1Symbol;
}

char board_get(char **board, int row, int col) {
    if (row < 0 || row >= BOARD_SIZE || col < 0 || col >= BOARD_SIZE)
        return ' ';
    return board[row][col];
}

int board_count_pieces(char **board, char symbol) {
    int count = 0;
    for (int i = 0; i < BOARD_SIZE; i++)
        for (int j = 0; j < BOARD_SIZE; j++)
            if (board[i][j] == symbol)
                count++;
    return count;
}

int board_is_player_piece(char cell, char normal, char king) {
    return (cell == normal || cell == king);
}

int board_is_opponent_piece(char cell, char oppNormal, char oppKing) {
    return (cell == oppNormal || cell == oppKing);
}
