#include<stdio.h>
#include<stdlib.h>
#include<conio.h>
#include<string.h>
#include<windows.h>
#include<time.h>
#include"..\include\functions.h"

void setpiece(char **board, char A, char B){
    board[0][0]=B;board[0][2]=B;board[0][4]=B;board[0][6]=B;
    board[1][1]=B;board[1][3]=B;board[1][5]=B;board[1][7]=B;
    board[2][0]=B;board[2][2]=B;board[2][4]=B;board[2][6]=B;
    board[5][1]=A;board[5][3]=A;board[5][5]=A;board[5][7]=A;
    board[6][0]=A;board[6][2]=A;board[6][4]=A;board[6][6]=A;
    board[7][1]=A;board[7][3]=A;board[7][5]=A;board[7][7]=A;
}


void updateScore(int playerNumber, int player1Score, int player2Score) {
    FILE *file = fopen("scores.txt", "w");  // Open in write mode to overwrite the file
    if (file == NULL) {
        printf("Error opening file.\n");
        return;
    }

    if (playerNumber == 1) {
        fprintf(file, "Player 1's score: %d\n", player1Score);
        fprintf(file, "Player 2's score: %d\n", player2Score);  // Keep Player 2's score unchanged
    } else if (playerNumber == 2) {
        fprintf(file, "Player 1's score: %d\n", player1Score);  // Keep Player 1's score unchanged
        fprintf(file, "Player 2's score: %d\n", player2Score);  // Update Player 2's score
    }

    fclose(file);
}

void display(char **board)
{
    int i;
    printf("\n");

//board display
    printf("\a");//bell
    for(i=0;i<8;)
    {
        printf("\n  +--+--+--+--+--+--+--+--+");
        printf("\n%d | %c| %c| %c| %c| %c| %c| %c| %c|",i + 1,board[i][0],board[i][1],board[i][2],board[i][3],board[i][4],board[i][5],board[i][6],board[i][7]);
        i++;
    }
    printf("\n  +--+--+--+--+--+--+--+--+");
    printf("\n    A  B  C  D  E  F  G  H");
}