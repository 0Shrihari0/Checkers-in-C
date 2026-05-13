#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include "../include/functions.h"

/* Safe input helpers that exit on EOF */
static void safeExit(char **board) {
    if (board) {
        for (int i = 0; i < 8; i++) free(board[i]);
        free(board);
    }
    printf("\nEnd of input. Exiting.\n");
    exit(0);
}

static char **g_board = NULL; /* global ref for EOF cleanup */

static void scanStr(char *buf, int size) {
    char fmt[16];
    snprintf(fmt, sizeof(fmt), " %%%ds", size - 1);
    if (scanf(fmt, buf) != 1) {
        safeExit(g_board);
    }
}

static void scanChar(char *c) {
    if (scanf(" %c", c) != 1) safeExit(g_board);
}

static void scanInt(int *val) {
    if (scanf(" %d", val) != 1) safeExit(g_board);
}

struct info
{
    char name[50];
    int score; // Defining structure
    char symbol;
};

/* Parse column letter (A-H, case insensitive) to index 0-7. Returns -1 on invalid. */
static int parseCol(char c) {
    c = toupper(c);
    if (c >= 'A' && c <= 'H') return c - 'A';
    return -1;
}

/* Parse row character ('1'-'8') to index 0-7. Returns -1 on invalid. */
static int parseRow(char c) {
    if (c >= '1' && c <= '8') return c - '1';
    return -1;
}

/* Check if a piece belongs to a player (normal or king) */
static int isPlayerPiece(char cell, char normal, char king) {
    return (cell == normal || cell == king);
}

/* Check if a piece belongs to opponent */
static int isOpponentPiece(char cell, char oppNormal, char oppKing) {
    return (cell == oppNormal || cell == oppKing);
}

/* Attempt to perform a move or capture in a given direction.
   dr, dc: direction offsets (-1 or +1)
   Returns: 0 = invalid move, 1 = simple move done, 2 = capture done */
static int tryMove(char **board, int *row, int *col, int dr, int dc,
                   char myNormal, char myKing, char oppNormal, char oppKing) {
    int r = *row, c = *col;
    char myPiece = board[r][c]; /* preserve whether it's normal or king */
    int nr = r + dr, nc = c + dc;

    /* Boundary check for adjacent square */
    if (nr < 0 || nr > 7 || nc < 0 || nc > 7) return 0;

    /* Can't move to a square occupied by own piece */
    if (isPlayerPiece(board[nr][nc], myNormal, myKing)) return 0;

    /* Simple move to empty square */
    if (board[nr][nc] == ' ') {
        board[r][c] = ' ';
        board[nr][nc] = myPiece;
        *row = nr;
        *col = nc;
        return 1;
    }

    /* Capture: adjacent has opponent piece, check landing square */
    if (isOpponentPiece(board[nr][nc], oppNormal, oppKing)) {
        int jr = r + 2*dr, jc = c + 2*dc;
        if (jr < 0 || jr > 7 || jc < 0 || jc > 7) return 0;
        if (board[jr][jc] != ' ') return 0;
        board[r][c] = ' ';
        board[nr][nc] = ' '; /* remove captured piece */
        board[jr][jc] = myPiece;
        *row = jr;
        *col = jc;
        return 2;
    }

    return 0;
}

/* Check if a piece can capture in a valid direction from (row,col).
   forwardDr: -1 for P1 (moves upward), +1 for P2 (moves downward).
   Kings can capture in any direction. */
static int canCaptureFrom(char **board, int row, int col,
                          char myNormal, char myKing, char oppNormal, char oppKing,
                          int forwardDr) {
    (void)myNormal;
    int isK = (board[row][col] == myKing);
    int dr[] = {-1, -1, 1, 1};
    int dc[] = {-1, 1, -1, 1};
    for (int d = 0; d < 4; d++) {
        /* Non-kings can only capture in their forward direction */
        if (!isK && dr[d] != forwardDr) continue;
        int nr = row + dr[d], nc = col + dc[d];
        int jr = row + 2*dr[d], jc = col + 2*dc[d];
        if (nr < 0 || nr > 7 || nc < 0 || nc > 7) continue;
        if (jr < 0 || jr > 7 || jc < 0 || jc > 7) continue;
        if (isOpponentPiece(board[nr][nc], oppNormal, oppKing) && board[jr][jc] == ' ')
            return 1;
    }
    return 0;
}

/* Check if ANY piece of a player has a capture available */
static int playerHasCapture(char **board, char myNormal, char myKing, char oppNormal, char oppKing,
                            int forwardDr) {
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 8; j++)
            if (isPlayerPiece(board[i][j], myNormal, myKing))
                if (canCaptureFrom(board, i, j, myNormal, myKing, oppNormal, oppKing, forwardDr))
                    return 1;
    return 0;
}

/* Promote piece to king if it reached the back row */
static void checkPromotion(char **board, int row, int col, char normal, char king, int backRow) {
    if (row == backRow && board[row][col] == normal) {
        board[row][col] = king;
        printf("\n*** KING! Piece at %c%d has been promoted to King (%c)! ***\n",
               'A' + col, row + 1, king);
    }
}

/* Get direction offsets from direction number 1-4 */
static void getDirOffsets(int direction, int *dr, int *dc) {
    switch (direction) {
        case 1: *dr = -1; *dc = -1; break; /* upper left */
        case 2: *dr = -1; *dc =  1; break; /* upper right */
        case 3: *dr =  1; *dc = -1; break; /* lower left */
        case 4: *dr =  1; *dc =  1; break; /* lower right */
        default: *dr = 0; *dc = 0; break;
    }
}

/* Validate direction choice for a non-king piece.
   Player 1 pieces start at bottom (rows 5-7) and move upward (directions 1,2).
   Player 2 pieces start at top (rows 0-2) and move downward (directions 3,4).
   Kings can move in any direction. */
static int isValidDirection(int direction, char piece, char p1Normal, char p1King, char p2Normal, char p2King) {
    if (direction < 1 || direction > 4) return 0;
    if (piece == p1King || piece == p2King) return 1; /* Kings can go anywhere */
    if (piece == p1Normal) return (direction == 1 || direction == 2); /* P1 moves upward */
    if (piece == p2Normal) return (direction == 3 || direction == 4); /* P2 moves downward */
    return 0;
}

/* Calculate and display scores, returns 1 if game should end */
static int updateAndShowScores(char **board, struct info *p1, struct info *p2,
                               char piece1, char piece2, char king1, char king2) {
    p1->score = 0;
    p2->score = 0;
    p1->symbol = piece1;
    p2->symbol = piece2;

    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            if (board[i][j] == piece1)
                p1->score += 5;
            else if (board[i][j] == king1)
                p1->score += 10; /* Kings worth more */
            else if (board[i][j] == piece2)
                p2->score += 5;
            else if (board[i][j] == king2)
                p2->score += 10;
        }
    }

    updateScore(1, p1->score, p2->score);

    printf("\n\t\t\t\tCurrent scores:");
    printf("\n\t\t\t\t%s = %d", p1->name, p1->score);
    printf("\n\t\t\t\t%s = %d\n", p2->name, p2->score);

    /* Check win conditions: no pieces or no valid moves */
    int p1pieces = countPieces(board, piece1) + countPieces(board, king1);
    int p2pieces = countPieces(board, piece2) + countPieces(board, king2);

    if (p1pieces == 0 || p2pieces == 0)
        return 1;

    return 0;
}

int main(void)
{

    struct info p1; // person 1
    struct info p2; // person2

intro:;

    // file creation for score
    FILE *f;

    char newHigh[50];
    int x, yindex, direction, high_score = 0;
    char **board, position[10]; // board and variable for position input
    char king1, king2; // king symbols

    board = (char **)malloc(8 * sizeof(char *)); // memory allocation
    for (int i = 0; i < 8; i++)
    {
        board[i] = (char *)malloc(8 * sizeof(char)); // memory allocation for each column in each row
    }
    g_board = board; /* set global ref for EOF cleanup */

    printf("\n\t\t\t\t\t----------------------------------");
    printf("\n\t\t\t\t\t\tPrevious Records:");
    // open a file for reading
    f = fopen("highscore", "r");

    if (f == NULL)
    {
        printf("\n\t\t\t\t\t  No previous records found.");
    }
    else
    {
        if (fscanf(f, " %49s = %d", newHigh, &high_score) == 2)
        {
            printf("\n %s = %d", newHigh, high_score);
        }
        else
        {
            printf("\n\t\t\t\t\t  No valid records found.");
        }
        fclose(f);
    }
    printf("\n\t\t\t\t\t-----------------------------------");

    for (int i = 0; i < 8; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            board[i][j] = ' '; // initializing each element to  ' '
        }
    }

    printf("\n\n\t\t\t\t\tWELCOME TO THE CHECKERS GAME!!!!\n\n\n");

    printf("\n\tEnter the name of the player-1:");
    scanStr(p1.name, 50);
    printf("\n\tEnter the name of the player-2:");
    scanStr(p2.name, 50);
    printf("\nThe rules are as usual...\n");
    printf("In the end,player who has his/her tokens left wins the game...\n");
    printf("\n 'x'  'o' ");

    sleepMs(1000); // system sleep for 1000 ms
    printf("\n\nEnter %s symbol:", p1.name);
    scanChar(&p1.symbol);
    printf("\nEnter the %s symbol:", p2.name);
    scanChar(&p2.symbol);
    if (p1.symbol == p2.symbol)
    {
        printf("   LOL!!! Both the symbols are same.... ");
        /* Free board before going back */
        for (int i = 0; i < 8; i++) free(board[i]);
        free(board);
        goto intro;
    }

    /* King symbols: uppercase version of player symbols */
    king1 = toupper(p1.symbol);
    king2 = toupper(p2.symbol);
    /* If the normal symbol is already uppercase, use a different king marker */
    if (king1 == p1.symbol) king1 = (p1.symbol == 'K') ? '#' : 'K';
    if (king2 == p2.symbol) king2 = (p2.symbol == 'Q') ? '$' : 'Q';
    /* Make sure king symbols don't collide */
    if (king1 == king2 || king1 == p2.symbol || king2 == p1.symbol) {
        king1 = '#';
        king2 = '$';
    }

    printf("\nKing symbols: %s='%c'(king:'%c')  %s='%c'(king:'%c')\n",
           p1.name, p1.symbol, king1, p2.name, p2.symbol, king2);

    printf("\nAND THE GAME STARTS NOW...");
    char piece1 = p1.symbol;
    char piece2 = p2.symbol;

    /* Player 1 back row (for promotion) is row 0, Player 2 back row is row 7 */
    int p1BackRow = 0;
    int p2BackRow = 7;

    setpiece(board, piece1, piece2);

    int gameOver = 0;

start:
    if (gameOver) goto end;

    display(board);

    /* Check if P1 has any valid moves */
    if (!hasValidMove(board, piece1, piece2, king1, king2, -1)) {
        printf("\n%s has no valid moves! %s wins!\n", p1.name, p2.name);
        /* Set scores so p2 wins */
        p1.score = 0;
        p2.score = countPieces(board, piece2) * 5 + countPieces(board, king2) * 10;
        goto end;
    }

    printf("\n %s's (%c) turn...", p1.name, p1.symbol);

    /* Check if P1 has a forced capture */
    int p1MustCapture = playerHasCapture(board, piece1, king1, piece2, king2, -1);
    if (p1MustCapture) {
        printf(" [You MUST capture!]");
    }

    printf("\n Enter position:"); // the position of the coin which we are going to move
    scanStr(position, 10);
    printf("\n");

    x = parseCol(position[0]);
    if (x < 0) {
        printf("Invalid column! Enter again...");
        goto start;
    }

    yindex = parseRow(position[1]);
    if (yindex < 0) {
        printf("Invalid row! Enter again...");
        goto start;
    }

    if (!isPlayerPiece(board[yindex][x], piece1, king1))
    {
        // input again if symbol is not present
        printf("That's not your piece! Enter again...");
        goto start;
    }

    /* If forced capture exists, this piece must be able to capture */
    if (p1MustCapture && !canCaptureFrom(board, yindex, x, piece1, king1, piece2, king2, -1)) {
        printf("You must select a piece that can capture! Enter again...");
        goto start;
    }

    printf("Which direction?\n1-upper left diagonal\n2-upper right diagonal\n3-lower left diagonal\n4-lower right diagonal\n");
    scanInt(&direction); // directional input

    /* Validate direction for piece type */
    if (!isValidDirection(direction, board[yindex][x], piece1, king1, piece2, king2)) {
        printf("Invalid direction for this piece! (Only kings can move backward)\n");
        goto start;
    }

    p1.symbol = piece1;
    p2.symbol = piece2;

    int dr, dc;
    getDirOffsets(direction, &dr, &dc);

    /* If must capture, only allow capture moves */
    if (p1MustCapture) {
        /* Check if this specific direction has a capture */
        int nr = yindex + dr, nc = x + dc;
        int jr = yindex + 2*dr, jc = x + 2*dc;
        if (nr < 0 || nr > 7 || nc < 0 || nc > 7 ||
            jr < 0 || jr > 7 || jc < 0 || jc > 7 ||
            !isOpponentPiece(board[nr][nc], piece2, king2) ||
            board[jr][jc] != ' ') {
            printf("You must capture! Choose a direction with a valid capture.\n");
            goto start;
        }
    }

    int moveResult = tryMove(board, &yindex, &x, dr, dc, piece1, king1, piece2, king2);

    if (moveResult == 0) {
        printf("move invalid!\n");
        goto start;
    }

    /* Check for king promotion */
    checkPromotion(board, yindex, x, piece1, king1, p1BackRow);

    /* Multi-capture: if we just captured, check for additional captures */
    if (moveResult == 2) {
        while (canCaptureFrom(board, yindex, x, piece1, king1, piece2, king2, -1)) {
            display(board);
            printf("\n*** Multi-capture available! You must continue jumping. ***\n");
            printf("Current position: %c%d\n", 'A' + x, yindex + 1);
            printf("Which direction?\n1-upper left diagonal\n2-upper right diagonal\n3-lower left diagonal\n4-lower right diagonal\n");
            scanInt(&direction);

            if (!isValidDirection(direction, board[yindex][x], piece1, king1, piece2, king2)) {
                printf("Invalid direction! Try again.\n");
                continue;
            }

            getDirOffsets(direction, &dr, &dc);
            int nr = yindex + dr, nc = x + dc;
            int jr = yindex + 2*dr, jc = x + 2*dc;
            if (nr < 0 || nr > 7 || nc < 0 || nc > 7 ||
                jr < 0 || jr > 7 || jc < 0 || jc > 7 ||
                !isOpponentPiece(board[nr][nc], piece2, king2) ||
                board[jr][jc] != ' ') {
                printf("No capture in that direction! Try again.\n");
                continue;
            }

            tryMove(board, &yindex, &x, dr, dc, piece1, king1, piece2, king2);
            checkPromotion(board, yindex, x, piece1, king1, p1BackRow);
        }
    }

// after jumping to the desired position,the compiler reads the done1 statement

    display(board);

    if (updateAndShowScores(board, &p1, &p2, piece1, piece2, king1, king2))
        goto end;

    /* ===== Player 2's turn ===== */
    printf("\n");
    printf("%s's (%c) turn...", p2.name, p2.symbol);

    /* Check if P2 has any valid moves */
    if (!hasValidMove(board, piece2, piece1, king2, king1, 1)) {
        printf("\n%s has no valid moves! %s wins!\n", p2.name, p1.name);
        p2.score = 0;
        p1.score = countPieces(board, piece1) * 5 + countPieces(board, king1) * 10;
        goto end;
    }

    /* Check if P2 has a forced capture */
    int p2MustCapture = playerHasCapture(board, piece2, king2, piece1, king1, 1);
    if (p2MustCapture) {
        printf(" [You MUST capture!]");
    }

again:
    printf("\n Enter position:"); // the position of the coin which we are going to move
    scanStr(position, 10);

    x = parseCol(position[0]);
    if (x < 0) {
        printf("Invalid column! Enter again...");
        goto again;
    }

    yindex = parseRow(position[1]);
    if (yindex < 0) {
        printf("Invalid row! Enter again...");
        goto again;
    }

    if (!isPlayerPiece(board[yindex][x], piece2, king2))
    {
        printf("That's not your piece! Enter again...");
        goto again;
    }

    /* If forced capture exists, this piece must be able to capture */
    if (p2MustCapture && !canCaptureFrom(board, yindex, x, piece2, king2, piece1, king1, 1)) {
        printf("You must select a piece that can capture! Enter again...");
        goto again;
    }

    printf("Which direction?\n1-upper left diagonal\n2-upper right diagonal\n3-lower left diagonal\n4-lower right diagonal\n");
    scanInt(&direction);

    /* Validate direction for piece type */
    if (!isValidDirection(direction, board[yindex][x], piece1, king1, piece2, king2)) {
        printf("Invalid direction for this piece! (Only kings can move backward)\n");
        goto again;
    }

    p1.symbol = piece1;
    p2.symbol = piece2;

    getDirOffsets(direction, &dr, &dc);

    /* If must capture, only allow capture moves */
    if (p2MustCapture) {
        int nr = yindex + dr, nc = x + dc;
        int jr = yindex + 2*dr, jc = x + 2*dc;
        if (nr < 0 || nr > 7 || nc < 0 || nc > 7 ||
            jr < 0 || jr > 7 || jc < 0 || jc > 7 ||
            !isOpponentPiece(board[nr][nc], piece1, king1) ||
            board[jr][jc] != ' ') {
            printf("You must capture! Choose a direction with a valid capture.\n");
            goto again;
        }
    }

    moveResult = tryMove(board, &yindex, &x, dr, dc, piece2, king2, piece1, king1);

    if (moveResult == 0) {
        printf("move invalid!\n");
        goto again;
    }

    /* Check for king promotion */
    checkPromotion(board, yindex, x, piece2, king2, p2BackRow);

    /* Multi-capture for P2 */
    if (moveResult == 2) {
        while (canCaptureFrom(board, yindex, x, piece2, king2, piece1, king1, 1)) {
            display(board);
            printf("\n*** Multi-capture available! You must continue jumping. ***\n");
            printf("Current position: %c%d\n", 'A' + x, yindex + 1);
            printf("Which direction?\n1-upper left diagonal\n2-upper right diagonal\n3-lower left diagonal\n4-lower right diagonal\n");
            scanInt(&direction);

            if (!isValidDirection(direction, board[yindex][x], piece1, king1, piece2, king2)) {
                printf("Invalid direction! Try again.\n");
                continue;
            }

            getDirOffsets(direction, &dr, &dc);
            int nr = yindex + dr, nc = x + dc;
            int jr = yindex + 2*dr, jc = x + 2*dc;
            if (nr < 0 || nr > 7 || nc < 0 || nc > 7 ||
                jr < 0 || jr > 7 || jc < 0 || jc > 7 ||
                !isOpponentPiece(board[nr][nc], piece1, king1) ||
                board[jr][jc] != ' ') {
                printf("No capture in that direction! Try again.\n");
                continue;
            }

            tryMove(board, &yindex, &x, dr, dc, piece2, king2, piece1, king1);
            checkPromotion(board, yindex, x, piece2, king2, p2BackRow);
        }
    }


    display(board);

    if (updateAndShowScores(board, &p1, &p2, piece1, piece2, king1, king2))
        goto end;

    goto start;

end:
    if (p1.score > p2.score)
    {
        sleepMs(1000);
        printf("\n\n");
        // displays the final score of each player and displays who won the game.This block is for player-1,if he/she wins..
        printf("\n%s's score:%d", p1.name, p1.score);
        printf("\n%s's score:%d", p2.name, p2.score);
        printf("\n%s won the game.....", p1.name);
        sleepMs(1000);

        if (p1.score > high_score)
        {
            sleepMs(1000);
            high_score = p1.score;
            strcpy(newHigh, p1.name);

            // if player-1 wins the high score screen is displayed
            printf("\nNEW HIGH SCORE!!!!");
            f = fopen("highscore", "w");
            if (f) {
                fprintf(f, "%s = %d", newHigh, high_score);
                fclose(f);
            }
            printf("\nHighest score:\n%s = %d", newHigh, high_score);
        }
    }
    else if (p1.score < p2.score)
    {
        sleepMs(1000);
        printf("\n\n");
        // this displays the score and shows who won the game.This blocks gets executed when player-2 wins.
        printf("\n%s's score:%d", p1.name, p1.score);
        printf("\n%s's score:%d", p2.name, p2.score);
        printf("\n%s won the game....", p2.name);
        sleepMs(1000);

        if (p2.score > high_score)
        {
            sleepMs(1000);
            high_score = p2.score;
            strcpy(newHigh, p2.name);

            // if player-2 wins,the high score screen is displayed
            printf("\nNEW HIGH SCORE!!!");
            f = fopen("highscore", "w");
            if (f) {
                fprintf(f, "%s = %d", newHigh, high_score);
                fclose(f);
            }
            printf("\nHighest score:\n%s = %d", newHigh, high_score);
        }
    }
    else
    {
        sleepMs(1000);
        printf("\n\n");
        printf("\n%s's score:%d", p1.name, p1.score);
        printf("\n%s's score:%d", p2.name, p2.score);
        printf("\nIt's a draw!!!!");
        sleepMs(1000);

        if (p1.score > high_score)
        {
            sleepMs(1000);
            high_score = p1.score;
            strcpy(newHigh, p1.name);

            f = fopen("highscore", "w");
            if (f) {
                fprintf(f, "%s = %d", newHigh, high_score);
                fclose(f);
            }
            printf("\nHighest score:\n%s = %d", newHigh, high_score);
        }
    }

    printf("\n\nPress any key to exit...\n");

    for (int i = 0; i < 8; i++)
    {
        free(board[i]); // Free each row
    }
    free(board); // Free the array of row pointers

    waitForKey();
    return 0;
}
