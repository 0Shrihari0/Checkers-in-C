#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>
#include"../include/functions.h"

#ifdef _WIN32
#include<conio.h>
#include<windows.h>
#else
#include<unistd.h>
#include<termios.h>
#endif

void sleepMs(int ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

void clearScreen(void) {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void waitForKey(void) {
#ifdef _WIN32
    getch();
#else
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
#endif
}

void setpiece(char **board, char A, char B){
    /* B (P2) at rows 1-3 (board[0]-board[2]), displayed at top */
    board[0][1]=B;board[0][3]=B;board[0][5]=B;board[0][7]=B;  /* row 1: b1,d1,f1,h1 */
    board[1][0]=B;board[1][2]=B;board[1][4]=B;board[1][6]=B;  /* row 2: a2,c2,e2,g2 */
    board[2][1]=B;board[2][3]=B;board[2][5]=B;board[2][7]=B;  /* row 3: b3,d3,f3,h3 */
    /* A (P1) at rows 6-8 (board[5]-board[7]), displayed at bottom */
    board[5][0]=A;board[5][2]=A;board[5][4]=A;board[5][6]=A;  /* row 6: a6,c6,e6,g6 */
    board[6][1]=A;board[6][3]=A;board[6][5]=A;board[6][7]=A;  /* row 7: b7,d7,f7,h7 */
    board[7][0]=A;board[7][2]=A;board[7][4]=A;board[7][6]=A;  /* row 8: a8,c8,e8,g8 */
}


void updateScore(int playerNumber, int player1Score, int player2Score) {
    (void)playerNumber;
    FILE *file = fopen("scores.txt", "w");  // Open in write mode to overwrite the file
    if (file == NULL) {
        printf("Error opening file.\n");
        return;
    }

    fprintf(file, "Player 1's score: %d\n", player1Score);
    fprintf(file, "Player 2's score: %d\n", player2Score);

    fclose(file);
}

void display(char **board)
{
    int i;
    printf("\n");

//board display
    for(i=0;i<8;)
    {
        printf("\n  +--+--+--+--+--+--+--+--+");
        printf("\n%d | %c| %c| %c| %c| %c| %c| %c| %c|",i + 1,board[i][0],board[i][1],board[i][2],board[i][3],board[i][4],board[i][5],board[i][6],board[i][7]);
        i++;
    }
    printf("\n  +--+--+--+--+--+--+--+--+");
    printf("\n    A  B  C  D  E  F  G  H");
}

int countPieces(char **board, char symbol) {
    int count = 0;
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            if (board[i][j] == symbol)
                count++;
        }
    }
    return count;
}

int isKing(char c, char kingA, char kingB) {
    return (c == kingA || c == kingB);
}

/* Check if a player has any valid move (simple move or capture) */
int hasValidMove(char **board, char mySymbol, char oppSymbol, char myKing, char oppKing, int forwardDr) {
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            if (board[i][j] != mySymbol && board[i][j] != myKing)
                continue;
            int isK = (board[i][j] == myKing);
            /* Check all 4 diagonal directions */
            int dr[] = {-1, -1, 1, 1};
            int dc[] = {-1, 1, -1, 1};
            for (int d = 0; d < 4; d++) {
                /* Non-kings can only move/capture in their forward direction */
                if (!isK && dr[d] != forwardDr) continue;
                int ni = i + dr[d];
                int nj = j + dc[d];
                if (ni >= 0 && ni < 8 && nj >= 0 && nj < 8) {
                    if (board[ni][nj] == ' ')
                        return 1; /* simple move available */
                    if (board[ni][nj] == oppSymbol || board[ni][nj] == oppKing) {
                        int ji = i + 2*dr[d];
                        int jj = j + 2*dc[d];
                        if (ji >= 0 && ji < 8 && jj >= 0 && jj < 8 && board[ji][jj] == ' ')
                            return 1; /* capture available */
                    }
                }
            }
        }
    }
    return 0;
}
