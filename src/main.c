/*
 * main.c — Game loop orchestrator.
 *
 * Calls game logic modules (board, moves, rules) for state management
 * and the UI module (terminal) for all user-facing I/O.
 * This file contains no game logic and no direct printf/scanf.
 */

#include "game/board.h"
#include "game/moves.h"
#include "game/rules.h"
#include "ui/terminal.h"

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  play_turn — handles one player's complete turn                     */
/*                                                                     */
/*  Returns:  0 = turn completed normally                              */
/*            1 = opponent wins (this player has no valid moves)        */
/*            2 = game over (no pieces left after this move)            */
/* ------------------------------------------------------------------ */
static int play_turn(char **board,
                     PlayerInfo *current, PlayerInfo *opponent,
                     PlayerInfo *p1, PlayerInfo *p2) {

    /* Check if current player has any valid moves */
    if (!rules_has_valid_move(board, current->symbol, opponent->symbol,
                              current->king, opponent->king,
                              current->forwardDir)) {
        ui_show_no_moves(current->name, opponent->name);
        /* Set scores so opponent wins */
        current->score = 0;
        opponent->score = board_count_pieces(board, opponent->symbol) * 5
                        + board_count_pieces(board, opponent->king) * 10;
        return 1;
    }

    ui_show_turn(current->name, current->symbol);

    /* Check if current player has a forced capture */
    int mustCapture = moves_player_has_capture(board,
                          current->symbol, current->king,
                          opponent->symbol, opponent->king,
                          current->forwardDir);
    if (mustCapture) {
        ui_show_forced_capture();
    }

    /* --- Piece selection loop --- */
    int row, col;
    char position[10];
    int pieceSelected = 0;

    while (!pieceSelected) {
        ui_prompt_string("\n Enter position:", position, 10);
        ui_print("\n");

        col = rules_parse_col(position[0]);
        if (col < 0) {
            ui_show_error("Invalid column! Enter again...");
            continue;
        }

        row = rules_parse_row(position[1]);
        if (row < 0) {
            ui_show_error("Invalid row! Enter again...");
            continue;
        }

        if (!board_is_player_piece(board[row][col],
                                   current->symbol, current->king)) {
            ui_show_error("That's not your piece! Enter again...");
            continue;
        }

        /* If forced capture, piece must be able to capture */
        if (mustCapture && !moves_can_capture_from(board, row, col,
                current->symbol, current->king,
                opponent->symbol, opponent->king,
                current->forwardDir)) {
            ui_show_error("You must select a piece that can capture! "
                          "Enter again...");
            continue;
        }

        pieceSelected = 1;
    }

    /* --- Direction selection and move execution loop --- */
    int moved = 0;

    while (!moved) {
        int direction = ui_prompt_direction();

        /* Validate direction for piece type */
        if (!rules_is_valid_direction(direction, board[row][col],
                p1->symbol, p1->king, p2->symbol, p2->king)) {
            ui_show_error("Invalid direction for this piece! "
                          "(Only kings can move backward)\n");
            continue;
        }

        int dr, dc;
        moves_get_dir_offsets(direction, &dr, &dc);

        /* If must capture, only allow capture moves */
        if (mustCapture) {
            if (!moves_is_capture_in_dir(board, row, col, dr, dc,
                    opponent->symbol, opponent->king)) {
                ui_show_error("You must capture! Choose a direction "
                              "with a valid capture.\n");
                continue;
            }
        }

        int result = moves_try(board, &row, &col, dr, dc,
                               current->symbol, current->king,
                               opponent->symbol, opponent->king);

        if (result == MOVE_INVALID) {
            ui_show_error("move invalid!\n");
            continue;
        }

        /* Check for king promotion */
        if (rules_check_promotion(board, row, col,
                current->symbol, current->king, current->backRow)) {
            ui_show_promotion(col, row, current->king);
        }

        /* Multi-capture chain */
        if (result == MOVE_CAPTURE) {
            while (moves_can_capture_from(board, row, col,
                       current->symbol, current->king,
                       opponent->symbol, opponent->king,
                       current->forwardDir)) {
                ui_display_board(board);
                ui_show_multi_capture(col, row);

                int chainDir = ui_prompt_direction();

                if (!rules_is_valid_direction(chainDir, board[row][col],
                        p1->symbol, p1->king, p2->symbol, p2->king)) {
                    ui_show_error("Invalid direction! Try again.\n");
                    continue;
                }

                moves_get_dir_offsets(chainDir, &dr, &dc);

                if (!moves_is_capture_in_dir(board, row, col, dr, dc,
                        opponent->symbol, opponent->king)) {
                    ui_show_error("No capture in that direction! "
                                  "Try again.\n");
                    continue;
                }

                moves_try(board, &row, &col, dr, dc,
                          current->symbol, current->king,
                          opponent->symbol, opponent->king);

                if (rules_check_promotion(board, row, col,
                        current->symbol, current->king,
                        current->backRow)) {
                    ui_show_promotion(col, row, current->king);
                }
            }
        }

        moved = 1;
    }

    /* Display board after move and update scores */
    ui_display_board(board);

    rules_calc_scores(board, p1, p2);
    ui_save_scores_to_file(p1->score, p2->score);
    ui_show_scores(p1, p2);

    if (rules_is_game_over(board, p1, p2))
        return 2;

    return 0;
}

/* ------------------------------------------------------------------ */
/*  main — game setup and main loop                                    */
/* ------------------------------------------------------------------ */
int main(void) {
    PlayerInfo p1, p2;
    char highName[50] = "";
    int highScore = 0;

    /* --- Intro / symbol selection loop --- */
    int symbolsOk = 0;

    while (!symbolsOk) {
        /* Show previous records */
        ui_show_records(highName, sizeof(highName), &highScore);
        ui_show_welcome();

        /* Get player names */
        ui_prompt_string("\n\tEnter the name of the player-1:", p1.name, 50);
        ui_prompt_string("\n\tEnter the name of the player-2:", p2.name, 50);

        ui_print("\nThe rules are as usual...\n");
        ui_print("In the end,player who has his/her tokens left "
                 "wins the game...\n");
        ui_print("\n 'x'  'o' ");

        ui_sleep_ms(1000);

        /* Get player symbols */
        char prompt1[80], prompt2[80];
        snprintf(prompt1, sizeof(prompt1),
                 "\n\nEnter %s symbol:", p1.name);
        snprintf(prompt2, sizeof(prompt2),
                 "\nEnter the %s symbol:", p2.name);

        ui_prompt_char(prompt1, &p1.symbol);
        ui_prompt_char(prompt2, &p2.symbol);

        if (p1.symbol == p2.symbol) {
            ui_show_same_symbol_error();
            continue; /* retry */
        }

        symbolsOk = 1;
    }

    /* Assign king symbols and player directions */
    rules_assign_king_symbols(&p1, &p2);

    p1.forwardDir = -1;  /* P1 moves upward */
    p1.backRow    = 0;   /* P1 promotes at row 0 */
    p2.forwardDir = 1;   /* P2 moves downward */
    p2.backRow    = 7;   /* P2 promotes at row 7 */

    ui_show_king_symbols(&p1, &p2);
    ui_show_game_start();

    /* Create and initialize the board */
    char **board = board_create();
    ui_set_board_ref(board);
    board_init(board, p1.symbol, p2.symbol);

    /* --- Main game loop --- */
    int gameOver = 0;

    while (!gameOver) {
        /* Player 1's turn */
        ui_display_board(board);
        int result = play_turn(board, &p1, &p2, &p1, &p2);

        if (result == 1 || result == 2) {
            gameOver = 1;
            break;
        }

        /* Player 2's turn */
        ui_print("\n");
        result = play_turn(board, &p2, &p1, &p1, &p2);

        if (result == 1 || result == 2) {
            gameOver = 1;
            break;
        }
    }

    /* --- Game over --- */
    ui_show_game_result(&p1, &p2, &highScore, highName);
    board_destroy(board);
    ui_wait_for_key();

    return 0;
}
