#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <string.h>
#include <windows.h>
#include <time.h>

#include "..\include\functions.h"

struct info
{
    char name[50];
    int score; // Defining structure
    char symbol;
};

int main()
{

    struct info p1; // person 1
    struct info p2; // person2

intro:;

    // file creation for score
    FILE *f;

    char newHigh[25];
    int x, yindex, direction, high_score = 0, tosee = 0, limit = 0;
    char **board, position[2]; // board and variable for position input

    board = (char **)malloc(8 * sizeof(char *)); // memory allocation
    for (int i = 0; i < 8; i++)
    {
        board[i] = (char *)malloc(8 * sizeof(char)); // memory allocation for each column in each row
    }

    printf("\n\t\t\t\t\t----------------------------------");
    printf("\n\t\t\t\t\t\tPrevious Records:");
    // open a file for reading
    f = fopen("highscore", "r");

    if (f == NULL)
    {
        printf("file does not exist \n"); // ensuring that file exists
    }

    fprintf(stdout, "\n %s = %d", newHigh, high_score); // reading data
    // to close the file
    fclose(f);
    printf("\n\t\t\t\t\t-----------------------------------");

    for (int i = 0; i < 8; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            board[i][j] = ' '; // initializing each element to  ' '
        }
    }

    printf("\n\n\t\t\t\t\tWELCOME TO THE CHECKERS GAME!!!!\n\n\n");

    printf("\a");
    printf("\n\tEnter the name of the player-1:");
    scanf("%s", p1.name);
    printf("\a");
    printf("\n\tEnter the name of the player-2:");
    scanf("%s", p2.name);
    printf("\nThe rules are as usual...\n");
    printf("In the end,player who has his/her tokens left wins the game...\n");
    printf("\n 'x'  'o' ");

    Sleep(1000); // system sleep for 1000 ms
    printf("\a");
    printf("\n\nEnter %s symbol:", p1.name);
    scanf(" %c", &p1.symbol);
    printf("\a");
    printf("\nEnter the %s symbol:", p2.name);
    scanf(" %c", &p2.symbol);
    if (p1.symbol == p2.symbol)
    {
        printf("   LOL!!! Both the symbols are same.... ");
        goto intro;
    }
    printf("\a");
    printf("\nAND THE GAME STARTS NOW...");
    char piece1 = p1.symbol;
    char piece2 = p2.symbol;

    setpiece(board, piece1, piece2);

start:
    display(board);

    printf("\a");
    printf("\n %s's (%c) turn...", p1.name, p1.symbol);
    printf("\n Enter position:"); // the position of the coin which we are going to move
    scanf("%s", position);
    printf("\n");
    if (position[0] == 'A')
        x = 0;
    else if (position[0] == 'B')
        x = 1;
    else if (position[0] == 'C')
        x = 2;
    else if (position[0] == 'D')
        x = 3;
    else if (position[0] == 'E')
        x = 4;
    else if (position[0] == 'F')
        x = 5;
    else if (position[0] == 'G')
        x = 6;
    else if (position[0] == 'H')
        x = 7;
    else
    {
        //// only 8 valid inputs otherwise take input again
        printf("Enter again...");
        // goto start statement will take the compiler
        goto start;
    }
    if (position[1] == '1')
        yindex = 0;
    else if (position[1] == '2')
        yindex = 1;
    else if (position[1] == '3')
        yindex = 2;
    else if (position[1] == '4')
        yindex = 3;
    else if (position[1] == '5')
        yindex = 4;
    else if (position[1] == '6')
        yindex = 5;
    else if (position[1] == '7')
        yindex = 6;
    else if (position[1] == '8')
        yindex = 7;
    else
    {
        // if anything other than these 8 numbers are given as input,it will ask us to enter the position again
        printf("Enter again...");
        goto start;
    }
    if (board[yindex][x] != p1.symbol)
    {
        // input again if symbol is not present
        printf("Enter again...");
        goto start;
    }
    printf("\a"); // bell
    printf("Which direction?\n1-upper left diagonal\n2-upper right diagonal\n3-lower left diagonal\n4-lower right diagonal\n");
    scanf(" %d", &direction); // directional input
    p1.symbol = piece1;
    p2.symbol = piece2;

    // direction=1 indicates the upper left diagonal
    if (direction == 1)
    {
        if (x == 0 || yindex == 0)
        {
            printf("move invalid!"); // boundary of board
            goto start;
        }
        if (board[yindex - 1][x - 1] == p1.symbol)
        {
            printf("move invalid!"); // same symbol
            goto start;
        }
        if (board[yindex - 1][x - 1] == ' ') // valid
        {
            board[yindex][x] = ' ';           // emptying previous spot
            board[--yindex][--x] = p1.symbol; // occupying current spot
            goto done1;
        }
        // if the jumping position contains our opponent,the following if block takes place
        if (board[yindex - 1][x - 1] == p2.symbol)
        {
            if (x <= 1)
            {
                printf("move invalid!");
                goto start;
            }
            if (board[yindex - 2][x - 2] != ' ')
            {
                printf("move invalid!");
                goto start;
            }
            else
            {
                board[yindex][x] = ' ';
                board[yindex - 1][x - 1] = ' '; //opponent piece dies
                yindex -= 2;
                x -= 2;
                board[yindex][x] = p1.symbol;
                goto done1;
            }
        }
    }
    if (direction == 2)
    {
        if (x == 7 || yindex == 0)
        {
            printf("move invalid!");
            goto start;
        }
        if (board[yindex - 1][x + 1] == p1.symbol)
        {
            printf("move invalid!");
            goto start;
        }
        if (board[yindex - 1][x + 1] == ' ')
        {
            board[yindex][x] = ' ';
            board[--yindex][++x] = p1.symbol;
            goto done1;
        }
        if (board[yindex - 1][x + 1] == p2.symbol)
        {
            if (x >= 6)
            {
                printf("move invalid!");
                goto start;
            }
            if (board[yindex - 2][x + 2] != ' ')
            {
                printf("move invalid!");
                goto start;
            }
            else
            {
                board[yindex][x] = ' ';
                board[yindex - 1][x + 1] = ' ';
                yindex -= 2;
                x += 2;
                board[yindex][x] = p1.symbol;
                goto done1;
            }
        }
    }
    if (direction == 3)
    {
        if (x == 0 || yindex == 7)
        {
            printf("move invalid!");
            goto start;
        }
        if (board[yindex + 1][x - 1] == p1.symbol)
        {
            printf("move invalid!");
            goto start;
        }
        if (board[yindex + 1][x - 1] == ' ')
        {
            board[yindex][x] = ' ';
            board[++yindex][--x] = p1.symbol;
            goto done1;
        }
        if (board[yindex + 1][x - 1] == p2.symbol)
        {
            if (x <= 1)
            {
                printf("move invalid!");
                goto start;
            }
            if (board[yindex + 2][x - 2] != ' ')
            {
                printf("move invalid!");
                goto start;
            }
            else
            {
                board[yindex][x] = ' ';
                board[yindex + 1][x - 1] = ' ';
                yindex += 2;
                x -= 2;
                board[yindex][x] = p1.symbol;
                goto done1;
            }
        }
    }
    if (direction == 4)
    {
        if (x == 7 || yindex == 7)
        {
            printf("move invalid!");
            goto start;
        }
        if (board[yindex + 1][x + 1] == p1.symbol)
        {
            printf("move invalid!");
            goto start;
        }
        if (board[yindex + 1][x + 1] == ' ')
        {
            board[yindex][x] = ' ';
            board[++yindex][++x] = p1.symbol;
            goto done1;
        }
        if (board[yindex + 1][x + 1] == p2.symbol)
        {
            if (x >= 6)
            {
                printf("move invalid!");
                goto start;
            }
            if (board[yindex + 2][x + 2] != ' ')
            {
                printf("move invalid!");
                goto start;
            }
            else
            {
                board[yindex][x] = ' ';
                board[yindex + 1][x + 1] = ' ';
                yindex += 2;
                x += 2;
                board[yindex][x] = p1.symbol;
                goto done1;
            }
        }
    }
// after jumping to the desired position,the compiler reads the done1 statement
done1:
    display(board);
    p1.score = 0;
    p2.score = 0;
    p1.symbol = piece1;
    p2.symbol = piece2;

    //each piece's points are 5
    for (int i = 0; i < 8; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            if (board[i][j] == p1.symbol)
                p1.score += 5; // score of player-1
            updateScore(2, p2.score, p1.score);
        }
    }
    for (int i = 0; i < 8; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            if (board[i][j] == p2.symbol)
                p2.score += 5; // score of player-2
            updateScore(1, p2.score, p1.score);
        }
    }
    // displays the scores of the players after each moves
    printf("\n\t\t\t\tCurrent scores:");
    printf("\n\t\t\t\t%s = %d", p1.name, p1.score);
    printf("\n\t\t\t\t%s = %d", p2.name, p2.score);
    if (p1.score == 0 || p2.score == 0)
        goto end;
    printf("\n");
    printf("%s's (%c) turn...", p2.name, p2.symbol);
again:
    if (limit == 15)
    {
        printf("\n%s got confused:-P", p2.name);
        limit = 0;
        goto start;
    }
    if (tosee == 15)
    {
        for (int i = 0; i < 8; i++)
        {
            for (int j = 0; j < 8; j++)
            {
                if (board[i][j] == p2.symbol)
                
                {
                    yindex = i;
                    x = j;
                }
            }
        }
        tosee = 0;
        limit++;
    }
    else
    {
        printf("\n Enter position:"); // the position of the coin which we are going to move
        scanf("%s", position);
        getchar();
        if (position[0] == 'A')
            x = 0;
        else if (position[0] == 'B')
            x = 1;
        else if (position[0] == 'C')
            x = 2;
        else if (position[0] == 'D')
            x = 3;
        else if (position[0] == 'E')
            x = 4;
        else if (position[0] == 'F')
            x = 5;
        else if (position[0] == 'G')
            x = 6;
        else if (position[0] == 'H')
            x = 7;
        else
        {
            // if anything other than these 8 alphabets is given,the compiler will ask us to enter the position again
            printf("Enter again...");
            // goto start statement will take the compiler to 89th line.
            goto start;
        }
        if (position[1] == '1')
            yindex = 0;
        else if (position[1] == '2')
            yindex = 1;
        else if (position[1] == '3')
            yindex = 2;
        else if (position[1] == '4')
            yindex = 3;
        else if (position[1] == '5')
            yindex = 4;
        else if (position[1] == '6')
            yindex = 5;
        else if (position[1] == '7')
            yindex = 6;
        else if (position[1] == '8')
            yindex = 7;
        else
        {
            // if anything other than these 8 numbers are given as input,it will ask us to enter the position again
            printf("Enter again...");
            goto start;
        }
    }
    if (board[yindex][x] != p2.symbol) // need to understand this
        goto again;
    printf("Which direction?\n1-upper left diagonal\n2-upper right diagonal\n3-lower left diagonal\n4-lower right diagonal\n");
    scanf(" %d", &direction);
    if (board[yindex - 1][x - 1] == p1.symbol && x > 1 && yindex > 1)
        direction = 1;
    if (board[yindex - 1][x + 1] == p1.symbol && x < 6 && yindex > 1)
        direction = 2;
    if (board[yindex + 1][x - 1] == p1.symbol && x > 1 && yindex < 6)
        direction = 3;
    if (board[yindex + 1][x + 1] == p1.symbol && x < 6 && yindex < 6)
        direction = 4;
    p1.symbol = piece1;
    p2.symbol = piece2;

    if (direction == 1)
    {
        if (x == 0 || yindex == 0)
        {
            tosee++;
            Sleep(1000);
            goto again;
        }
        if (board[yindex - 1][x - 1] == p2.symbol)
        {
            tosee++;
            Sleep(1000);
            goto again;
        }
        if (board[yindex - 1][x - 1] == ' ')
        {
            board[yindex][x] = ' ';
            board[--yindex][--x] = p2.symbol;
            goto done2;
        }
        if (board[yindex - 1][x - 1] == p1.symbol)
        {
            if (x >= 1)
            {
                tosee++;
                Sleep(1000);
                goto again;
            }
            if (board[yindex - 2][x - 2] != ' ')
            {
                tosee++;
                Sleep(1000);
                goto again;
            }
            else
            {
                board[yindex][x] = ' ';
                board[yindex - 1][x - 1] = ' ';
                yindex -= 2;
                x -= 2;
                board[yindex][x] = p2.symbol;
                goto done2;
            }
        }
    }
    if (direction == 2)
    {
        if (x == 7 || yindex == 0)
        {
            tosee++;
            Sleep(1000);
            goto again;
        }
        if (board[yindex - 1][x + 1] == p2.symbol)
        {
            tosee++;
            Sleep(1000);
            goto again;
        }
        if (board[yindex - 1][x + 1] == ' ')
        {
            board[yindex][x] = ' ';
            board[--yindex][++x] = p2.symbol;
            goto done2;
        }
        if (board[yindex - 1][x + 1] == p1.symbol)
        {
            if (x >= 6)
            {
                tosee++;
                Sleep(1000);
                goto again;
            }
            if (board[yindex - 2][x + 2] != ' ')
            {
                tosee++;
                Sleep(1000);
                goto again;
            }
            else
            {
                board[yindex][x] = ' ';
                board[yindex - 1][x + 1] = ' ';
                yindex -= 2;
                x += 2;
                board[yindex][x] = p2.symbol;
                goto done2;
            }
        }
    }
    if (direction == 3)
    {
        if (x == 0 || yindex == 7)
        {
            tosee++;
            Sleep(1000);
            goto again;
        }
        if (board[yindex + 1][x - 1] == p2.symbol)
        {
            tosee++;
            Sleep(1000);
            goto again;
        }
        if (board[yindex + 1][x - 1] == ' ')
        {
            board[yindex][x] = ' ';
            board[++yindex][--x] = p2.symbol;
            goto done2;
        }
        if (board[yindex + 1][x - 1] == p1.symbol)
        {
            if (x >= 1)
            {
                tosee++;
                Sleep(1000);
                goto again;
            }
            if (board[yindex + 2][x - 2] != ' ')
            {
                tosee++;
                Sleep(1000);
                goto again;
            }
            else
            {
                board[yindex][x] = ' ';
                board[yindex + 1][x - 1] = ' ';
                yindex += 2;
                x -= 2;
                board[yindex][x] = p2.symbol;
                goto done2;
            }
        }
    }
    if (direction == 4)
    {
        if (x == 7 || yindex == 7)
        {
            tosee++;
            Sleep(1000);
            goto again;
        }
        if (board[yindex + 1][x + 1] == p2.symbol)
        {
            tosee++;
            Sleep(1000);
            goto again;
        }
        if (board[yindex + 1][x + 1] == ' ')
        {
            board[yindex][x] = ' ';
            board[++yindex][++x] = p2.symbol;
            goto done2;
        }
        if (board[yindex + 1][x + 1] == p1.symbol)
        {
            if (x >= 6)
            {
                tosee++;
                Sleep(1000);
                goto again;
            }
            if (board[yindex + 2][x + 2] != ' ')
            {
                tosee++;
                Sleep(1000);
                goto again;
            }
            else
            {
                board[yindex][x] = ' ';
                board[yindex + 1][x + 1] = ' ';
                yindex += 2;
                x += 2;
                board[yindex][x] = p2.symbol;
                goto done2;
            }
        }
    }
done2:
    display(board);

    p1.score = 0;
    p2.score = 0;
    p1.symbol = piece1;
    p2.symbol = piece2;
    for (int i = 0; i < 8; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            if (board[i][j] == p1.symbol)
                p1.score += 5;
        }
    }
    for (int i = 0; i < 8; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            if (board[i][j] == p2.symbol)
                p2.score += 5;
            updateScore(1, p2.score, p1.score);
        }
    }
    /*this too displays the scores after each move.The only difference is this block is for player-2's move,
    while which we saw above is for player-1's move*/
    printf("\n\t\t\t\tCurrent scores:");
    printf("\n\t\t\t\t%s = %d", p1.name, p1.score);
    printf("\n\t\t\t\t%s = %d", p2.name, p2.score);
    if (p1.score == 0 || p2.score == 0)
        // if score of player-1 or player-2 is 0,the compiler goes to the end part,or else it goes to start part
        goto end;
    else
        goto start;
end:
    if (p1.score > p2.score)
    {
        Sleep(1000);
        printf("\n\n");
        // displays the final score of each player and displays who won the game.This block is for player-1,if he/she wins..
        printf("\n%s's score:%d", p1.name, p1.score);
        printf("\n%s's score:%d", p2.name, p2.score);
        printf("\n%s won the game.....", p1.name);
        Sleep(1000);

        // system("cls") clears the screen
        system("cls");
        if (p1.score > high_score)
        {
            Sleep(1000);
            high_score = p1.score;
            strcpy(newHigh, p1.name);

            // if player-1 wins the high score screen is displayed
            printf("\nNEW HIGH SCORE!!!!");
            f = fopen("highscore", "w");
            fprintf(f, "\n %s = %d", newHigh, high_score);
            fclose(f);
            printf("\nHighest score:\n%s = %d", newHigh, high_score);
        }
    }
    if (p1.score < p2.score)
    {
        Sleep(1000);
        printf("\n\n");
        // this displays the score and shows who won the game.This blocks gets executed when player-2 wins.
        printf("\n%s's score:%d", p1.name, p1.score);
        printf("\n%s's score:%d", p2.name, p2.score);
        printf("\n%s won the game....", p2.name);
        Sleep(1000);

        system("cls");
        if (p2.score > high_score)
        {
            Sleep(1000);
            high_score = p2.score;
            strcpy(newHigh, p2.name);

            // if player-2 wins,the high score screen is displayed
            printf("\nNEW HIGH SCORE!!!");
            f = fopen("highscore", "w");
            fprintf(f, "\n %s = %d", newHigh, high_score);
            fclose(f);
            printf("\nHighest score:\n%s = %d", newHigh, high_score);
        }
    }
    if (p1.score == p2.score)
    {
        Sleep(1000);
        printf("\n\n");
        printf("\n%s's score:%d", p1.name, p1.score);
        printf("\n%s's score:%d", p2.name, p2.score);
        printf("\nIt's a draw!!!!");
        Sleep(1000);

        system("cls");
        if (p1.score > high_score)
        {
            Sleep(1000);
            high_score = p1.score;
            strcpy(newHigh, p1.name);

            f = fopen("highscore", "w");
            fprintf(f, "\n %s = %d", newHigh, high_score);
            fclose(f);
            printf("\nHighest score:\n%s = %d", newHigh, high_score);
        }
        if (p2.score > high_score)
        {
            Sleep(1000);
            high_score = p2.score;
            strcpy(newHigh, p2.name);

            f = fopen("highscore", "w");
            fprintf(f, "\n %s = %d", newHigh, high_score);
            fclose(f);
            printf("\nHighest score:\n%s = %d", newHigh, high_score);
        }
    }

    for (int i = 0; i < 8; i++)
    {
        free(board[i]); // Free each row
    }
    free(board); // Free the array of row pointers

    getch();
    return 0;
}