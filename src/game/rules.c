#include "rules.h"
#include <ctype.h>

int rules_is_valid_direction(int direction, char piece,
                             char p1Normal, char p1King,
                             char p2Normal, char p2King) {
    if (direction < 1 || direction > 4) return 0;
    if (piece == p1King || piece == p2King) return 1; /* Kings go anywhere */
    if (piece == p1Normal) return (direction == 1 || direction == 2); /* P1 up */
    if (piece == p2Normal) return (direction == 3 || direction == 4); /* P2 down */
    return 0;
}

int rules_check_promotion(char **board, int row, int col,
                          char normal, char king, int backRow) {
    if (row == backRow && board[row][col] == normal) {
        board[row][col] = king;
        return 1; /* promoted */
    }
    return 0; /* not promoted */
}

void rules_calc_scores(char **board, PlayerInfo *p1, PlayerInfo *p2) {
    p1->score = 0;
    p2->score = 0;

    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] == p1->symbol)
                p1->score += 5;
            else if (board[i][j] == p1->king)
                p1->score += 10;
            else if (board[i][j] == p2->symbol)
                p2->score += 5;
            else if (board[i][j] == p2->king)
                p2->score += 10;
        }
    }
}

int rules_is_game_over(char **board, PlayerInfo *p1, PlayerInfo *p2) {
    int p1pieces = board_count_pieces(board, p1->symbol)
                 + board_count_pieces(board, p1->king);
    int p2pieces = board_count_pieces(board, p2->symbol)
                 + board_count_pieces(board, p2->king);
    return (p1pieces == 0 || p2pieces == 0);
}

int rules_has_valid_move(char **board, char mySymbol, char oppSymbol,
                         char myKing, char oppKing, int forwardDr) {
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] != mySymbol && board[i][j] != myKing)
                continue;
            int isK = (board[i][j] == myKing);
            int dr[] = {-1, -1, 1, 1};
            int dc[] = {-1, 1, -1, 1};
            for (int d = 0; d < 4; d++) {
                /* Non-kings can only move/capture in their forward direction */
                if (!isK && dr[d] != forwardDr) continue;
                int ni = i + dr[d];
                int nj = j + dc[d];
                if (ni >= 0 && ni < BOARD_SIZE && nj >= 0 && nj < BOARD_SIZE) {
                    if (board[ni][nj] == ' ')
                        return 1; /* simple move available */
                    if (board[ni][nj] == oppSymbol || board[ni][nj] == oppKing) {
                        int ji = i + 2 * dr[d];
                        int jj = j + 2 * dc[d];
                        if (ji >= 0 && ji < BOARD_SIZE && jj >= 0 && jj < BOARD_SIZE
                            && board[ji][jj] == ' ')
                            return 1; /* capture available */
                    }
                }
            }
        }
    }
    return 0;
}

int rules_parse_col(char c) {
    c = (char)toupper((unsigned char)c);
    if (c >= 'A' && c <= 'H') return c - 'A';
    return -1;
}

int rules_parse_row(char c) {
    if (c >= '1' && c <= '8') return c - '1';
    return -1;
}

void rules_assign_king_symbols(PlayerInfo *p1, PlayerInfo *p2) {
    /* King symbols: uppercase version of player symbols */
    p1->king = (char)toupper((unsigned char)p1->symbol);
    p2->king = (char)toupper((unsigned char)p2->symbol);

    /* If the normal symbol is already uppercase, use a fallback */
    if (p1->king == p1->symbol)
        p1->king = (p1->symbol == 'K') ? '#' : 'K';
    if (p2->king == p2->symbol)
        p2->king = (p2->symbol == 'Q') ? '$' : 'Q';

    /* Make sure king symbols don't collide with each other or normal symbols */
    if (p1->king == p2->king || p1->king == p2->symbol
        || p2->king == p1->symbol) {
        p1->king = '#';
        p2->king = '$';
    }
}
