#ifndef TERMINAL_H
#define TERMINAL_H

#include "../game/board.h"

/* Set the board pointer used for cleanup on unexpected EOF. */
void ui_set_board_ref(char **board);

/* Display the board with row/column labels. */
void ui_display_board(char **board);

/* Display current scores for both players. */
void ui_show_scores(PlayerInfo *p1, PlayerInfo *p2);

/* Display king promotion message. */
void ui_show_promotion(int col, int row, char king);

/* Display the welcome banner. */
void ui_show_welcome(void);

/* Read and display previous high score records.
   Populates highName and highScore if a record is found.
   Returns 1 if a valid record was found, 0 otherwise. */
int ui_show_records(char *highName, int nameSize, int *highScore);

/* Prompt for and read a string from the user. Exits on EOF. */
void ui_prompt_string(const char *prompt, char *buf, int size);

/* Prompt for and read a single character. Exits on EOF. */
void ui_prompt_char(const char *prompt, char *c);

/* Prompt for and read an integer. Exits on EOF. */
void ui_prompt_int(const char *prompt, int *val);

/* Print a plain message. */
void ui_print(const char *msg);

/* Display whose turn it is. */
void ui_show_turn(const char *name, char symbol);

/* Display forced capture warning. */
void ui_show_forced_capture(void);

/* Display multi-capture prompt with current position. */
void ui_show_multi_capture(int col, int row);

/* Display an error message. */
void ui_show_error(const char *msg);

/* Display king symbol assignments for both players. */
void ui_show_king_symbols(PlayerInfo *p1, PlayerInfo *p2);

/* Display same-symbol error. */
void ui_show_same_symbol_error(void);

/* Display game start message. */
void ui_show_game_start(void);

/* Display the direction menu and read a choice. */
int ui_prompt_direction(void);

/* Display no-valid-moves message and declare the winner. */
void ui_show_no_moves(const char *loserName, const char *winnerName);

/* Display game-over results, handle high score file save.
   highScore/highName are the previous records (updated if beaten). */
void ui_show_game_result(PlayerInfo *p1, PlayerInfo *p2,
                         int *highScore, char *highName);

/* Save current scores to scores.txt */
void ui_save_scores_to_file(int p1Score, int p2Score);

/* Sleep for the given number of milliseconds. */
void ui_sleep_ms(int ms);

/* Clear the terminal screen. */
void ui_clear_screen(void);

/* Wait for a key press before continuing. */
void ui_wait_for_key(void);

#endif
