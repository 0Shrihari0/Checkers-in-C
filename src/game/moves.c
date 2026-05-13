#include "moves.h"
#include "board.h"

int moves_try(char **board, int *row, int *col, int dr, int dc,
              char myNormal, char myKing, char oppNormal, char oppKing) {
    int r = *row, c = *col;
    char myPiece = board[r][c]; /* preserve whether it's normal or king */
    int nr = r + dr, nc = c + dc;

    /* Boundary check for adjacent square */
    if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE)
        return MOVE_INVALID;

    /* Can't move to a square occupied by own piece */
    if (board_is_player_piece(board[nr][nc], myNormal, myKing))
        return MOVE_INVALID;

    /* Simple move to empty square */
    if (board[nr][nc] == ' ') {
        board[r][c] = ' ';
        board[nr][nc] = myPiece;
        *row = nr;
        *col = nc;
        return MOVE_SIMPLE;
    }

    /* Capture: adjacent has opponent piece, check landing square */
    if (board_is_opponent_piece(board[nr][nc], oppNormal, oppKing)) {
        int jr = r + 2 * dr, jc = c + 2 * dc;
        if (jr < 0 || jr >= BOARD_SIZE || jc < 0 || jc >= BOARD_SIZE)
            return MOVE_INVALID;
        if (board[jr][jc] != ' ')
            return MOVE_INVALID;
        board[r][c] = ' ';
        board[nr][nc] = ' '; /* remove captured piece */
        board[jr][jc] = myPiece;
        *row = jr;
        *col = jc;
        return MOVE_CAPTURE;
    }

    return MOVE_INVALID;
}

int moves_can_capture_from(char **board, int row, int col,
                           char myNormal, char myKing,
                           char oppNormal, char oppKing,
                           int forwardDr) {
    (void)myNormal;
    int isK = (board[row][col] == myKing);
    int dr[] = {-1, -1, 1, 1};
    int dc[] = {-1, 1, -1, 1};

    for (int d = 0; d < 4; d++) {
        /* Non-kings can only capture in their forward direction */
        if (!isK && dr[d] != forwardDr) continue;
        if (moves_is_capture_in_dir(board, row, col, dr[d], dc[d],
                                    oppNormal, oppKing))
            return 1;
    }
    return 0;
}

int moves_player_has_capture(char **board,
                             char myNormal, char myKing,
                             char oppNormal, char oppKing,
                             int forwardDr) {
    for (int i = 0; i < BOARD_SIZE; i++)
        for (int j = 0; j < BOARD_SIZE; j++)
            if (board_is_player_piece(board[i][j], myNormal, myKing))
                if (moves_can_capture_from(board, i, j,
                        myNormal, myKing, oppNormal, oppKing, forwardDr))
                    return 1;
    return 0;
}

int moves_is_capture_in_dir(char **board, int row, int col,
                            int dr, int dc,
                            char oppNormal, char oppKing) {
    int nr = row + dr, nc = col + dc;
    int jr = row + 2 * dr, jc = col + 2 * dc;

    if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) return 0;
    if (jr < 0 || jr >= BOARD_SIZE || jc < 0 || jc >= BOARD_SIZE) return 0;

    return (board_is_opponent_piece(board[nr][nc], oppNormal, oppKing)
            && board[jr][jc] == ' ');
}

void moves_get_dir_offsets(int direction, int *dr, int *dc) {
    switch (direction) {
        case 1: *dr = -1; *dc = -1; break; /* upper left  */
        case 2: *dr = -1; *dc =  1; break; /* upper right */
        case 3: *dr =  1; *dc = -1; break; /* lower left  */
        case 4: *dr =  1; *dc =  1; break; /* lower right */
        default: *dr = 0; *dc = 0; break;
    }
}
