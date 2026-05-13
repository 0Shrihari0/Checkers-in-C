#include "terminal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <unistd.h>
#include <termios.h>
#endif

/* ------------------------------------------------------------------ */
/*  Internal: EOF-safe input helpers                                   */
/* ------------------------------------------------------------------ */

static char **g_board = NULL; /* global ref for EOF cleanup */

static void safe_exit(void) {
    board_destroy(g_board);
    printf("\nEnd of input. Exiting.\n");
    exit(0);
}

void ui_set_board_ref(char **board) {
    g_board = board;
}

/* ------------------------------------------------------------------ */
/*  Platform helpers                                                   */
/* ------------------------------------------------------------------ */

void ui_sleep_ms(int ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    usleep((unsigned)(ms * 1000));
#endif
}

void ui_clear_screen(void) {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void ui_wait_for_key(void) {
#ifdef _WIN32
    getch();
#else
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(tcflag_t)(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
#endif
}

/* ------------------------------------------------------------------ */
/*  Input prompts (all I/O lives here)                                 */
/* ------------------------------------------------------------------ */

void ui_prompt_string(const char *prompt, char *buf, int size) {
    printf("%s", prompt);
    if (!fgets(buf, size, stdin)) safe_exit();
    /* Strip trailing newline */
    buf[strcspn(buf, "\n")] = '\0';
    /* Strip leading whitespace */
    char *start = buf;
    while (*start == ' ' || *start == '\t') start++;
    if (start != buf) memmove(buf, start, strlen(start) + 1);
    if (buf[0] == '\0') safe_exit();
}

void ui_prompt_char(const char *prompt, char *c) {
    printf("%s", prompt);
    if (scanf(" %c", c) != 1) safe_exit();
}

void ui_prompt_int(const char *prompt, int *val) {
    printf("%s", prompt);
    if (scanf(" %d", val) != 1) safe_exit();
}

int ui_prompt_direction(void) {
    int dir;
    printf("Which direction?\n"
           "1-upper left diagonal\n"
           "2-upper right diagonal\n"
           "3-lower left diagonal\n"
           "4-lower right diagonal\n");
    if (scanf(" %d", &dir) != 1) safe_exit();
    return dir;
}

/* ------------------------------------------------------------------ */
/*  Display functions                                                  */
/* ------------------------------------------------------------------ */

void ui_display_board(char **board) {
    printf("\n");
    for (int i = 0; i < BOARD_SIZE; i++) {
        printf("\n  +--+--+--+--+--+--+--+--+");
        printf("\n%d | %c| %c| %c| %c| %c| %c| %c| %c|",
               i + 1,
               board[i][0], board[i][1], board[i][2], board[i][3],
               board[i][4], board[i][5], board[i][6], board[i][7]);
    }
    printf("\n  +--+--+--+--+--+--+--+--+");
    printf("\n    A  B  C  D  E  F  G  H");
}

void ui_show_scores(PlayerInfo *p1, PlayerInfo *p2) {
    printf("\n\t\t\t\tCurrent scores:");
    printf("\n\t\t\t\t%s = %d", p1->name, p1->score);
    printf("\n\t\t\t\t%s = %d\n", p2->name, p2->score);
}

void ui_show_promotion(int col, int row, char king) {
    printf("\n*** KING! Piece at %c%d has been promoted to King (%c)! ***\n",
           'A' + col, row + 1, king);
}

void ui_show_welcome(void) {
    printf("\n\n\t\t\t\t\tWELCOME TO THE CHECKERS GAME!!!!\n\n\n");
}

int ui_show_records(char *highName, int nameSize, int *highScore) {
    printf("\n\t\t\t\t\t----------------------------------");
    printf("\n\t\t\t\t\t\tPrevious Records:");

    FILE *f = fopen("highscore", "r");
    if (f == NULL) {
        printf("\n\t\t\t\t\t  No previous records found.");
        printf("\n\t\t\t\t\t-----------------------------------");
        *highScore = 0;
        highName[0] = '\0';
        return 0;
    }

    char nameBuf[50];
    int scoreBuf;
    if (fscanf(f, " %49s = %d", nameBuf, &scoreBuf) == 2) {
        printf("\n %s = %d", nameBuf, scoreBuf);
        if (nameSize > 0) {
            strncpy(highName, nameBuf, nameSize - 1);
            highName[nameSize - 1] = '\0';
        }
        *highScore = scoreBuf;
        fclose(f);
        printf("\n\t\t\t\t\t-----------------------------------");
        return 1;
    }

    printf("\n\t\t\t\t\t  No valid records found.");
    fclose(f);
    printf("\n\t\t\t\t\t-----------------------------------");
    *highScore = 0;
    highName[0] = '\0';
    return 0;
}

void ui_print(const char *msg) {
    printf("%s", msg);
}

void ui_show_turn(const char *name, char symbol) {
    printf("\n %s's (%c) turn...", name, symbol);
}

void ui_show_forced_capture(void) {
    printf(" [You MUST capture!]");
}

void ui_show_multi_capture(int col, int row) {
    printf("\n*** Multi-capture available! You must continue jumping. ***\n");
    printf("Current position: %c%d\n", 'A' + col, row + 1);
}

void ui_show_error(const char *msg) {
    printf("%s", msg);
}

void ui_show_king_symbols(PlayerInfo *p1, PlayerInfo *p2) {
    printf("\nKing symbols: %s='%c'(king:'%c')  %s='%c'(king:'%c')\n",
           p1->name, p1->symbol, p1->king,
           p2->name, p2->symbol, p2->king);
}

void ui_show_same_symbol_error(void) {
    printf("   LOL!!! Both the symbols are same.... ");
}

void ui_show_game_start(void) {
    printf("\nAND THE GAME STARTS NOW...");
}

void ui_show_no_moves(const char *loserName, const char *winnerName) {
    printf("\n%s has no valid moves! %s wins!\n", loserName, winnerName);
}

void ui_save_scores_to_file(int p1Score, int p2Score) {
    FILE *file = fopen("scores.txt", "w");
    if (file == NULL) {
        printf("Error opening file.\n");
        return;
    }
    fprintf(file, "Player 1's score: %d\n", p1Score);
    fprintf(file, "Player 2's score: %d\n", p2Score);
    fclose(file);
}

void ui_show_game_result(PlayerInfo *p1, PlayerInfo *p2,
                         int *highScore, char *highName) {
    ui_sleep_ms(1000);
    printf("\n\n");

    printf("\n%s's score:%d", p1->name, p1->score);
    printf("\n%s's score:%d", p2->name, p2->score);

    /* Determine winner */
    if (p1->score > p2->score) {
        printf("\n%s won the game.....", p1->name);
    } else if (p1->score < p2->score) {
        printf("\n%s won the game....", p2->name);
    } else {
        printf("\nIt's a draw!!!!");
    }

    ui_sleep_ms(1000);

    /* Check for new high score */
    int winnerScore = (p1->score >= p2->score) ? p1->score : p2->score;
    const char *winnerName = (p1->score >= p2->score) ? p1->name : p2->name;

    if (winnerScore > *highScore) {
        ui_sleep_ms(1000);
        *highScore = winnerScore;
        strcpy(highName, winnerName);

        printf("\nNEW HIGH SCORE!!!!");
        FILE *f = fopen("highscore", "w");
        if (f) {
            fprintf(f, "%s = %d", highName, *highScore);
            fclose(f);
        }
        printf("\nHighest score:\n%s = %d", highName, *highScore);
    }

    printf("\n\nPress any key to exit...\n");
}
