#ifndef FUNCTIONS_H
#define FUNCTIONS_H

void setpiece(char **board, char A, char B);
void updateScore(int playerNumber, int player1Score, int player2Score);
void display(char **board);
int countPieces(char **board, char symbol);
int hasValidMove(char **board, char mySymbol, char oppSymbol, char myKing, char oppKing, int forwardDr);
int isKing(char c, char kingA, char kingB);
void sleepMs(int ms);
void clearScreen(void);
void waitForKey(void);

#endif
