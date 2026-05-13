/*
 * ai.c — Checkers AI with minimax alpha-beta pruning.
 *
 * Strategy by difficulty:
 *   Easy:   purely random valid move
 *   Medium: prefer captures, then random
 *   Hard:   minimax depth-6 with alpha-beta pruning
 *
 * Evaluation function considers:
 *   - Material: normal pieces = 3, kings = 5
 *   - Positional: center bonus, advancement bonus
 *   - Mobility: number of available moves
 */

#include "ai.h"
#include "../src/game/moves.h"
#include "../src/game/rules.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>

#define MAX_AI_MOVES 64
#define MINIMAX_DEPTH 6

/* ---- Move representation ---- */
typedef struct {
    int fromR, fromC, toR, toC;
} AIMove;

/* ---- Random seed ---- */
static int ai_seeded = 0;
static void ai_seed(void) {
    if (!ai_seeded) { srand((unsigned)time(NULL) ^ 0xA1); ai_seeded = 1; }
}

/* ---- Board cloning for minimax ---- */
static char **clone_board(char **board) {
    char **b = malloc(BOARD_SIZE * sizeof(char *));
    if (!b) return NULL;
    for (int i = 0; i < BOARD_SIZE; i++) {
        b[i] = malloc(BOARD_SIZE);
        if (!b[i]) {
            for (int j = 0; j < i; j++) free(b[j]);
            free(b);
            return NULL;
        }
        memcpy(b[i], board[i], BOARD_SIZE);
    }
    return b;
}

static void free_board(char **b) {
    if (!b) return;
    for (int i = 0; i < BOARD_SIZE; i++) free(b[i]);
    free(b);
}

/* ---- Generate all valid moves for a player ---- */
static int generate_moves(char **board, PlayerInfo *me, PlayerInfo *opp,
                          AIMove *moves, int max_moves) {
    int count = 0;
    int has_capture = moves_player_has_capture(board,
        me->symbol, me->king, opp->symbol, opp->king, me->forwardDir);

    for (int i = 0; i < BOARD_SIZE && count < max_moves; i++) {
        for (int j = 0; j < BOARD_SIZE && count < max_moves; j++) {
            if (!board_is_player_piece(board[i][j], me->symbol, me->king))
                continue;

            int isK = (board[i][j] == me->king);
            int dr[] = {-1, -1, 1, 1};
            int dc[] = {-1, 1, -1, 1};

            for (int d = 0; d < 4; d++) {
                /* Non-kings: only forward */
                if (!isK && dr[d] != me->forwardDir) continue;

                if (has_capture) {
                    /* Only capture moves */
                    if (moves_is_capture_in_dir(board, i, j, dr[d], dc[d],
                            opp->symbol, opp->king)) {
                        moves[count].fromR = i;
                        moves[count].fromC = j;
                        moves[count].toR = i + 2 * dr[d];
                        moves[count].toC = j + 2 * dc[d];
                        count++;
                    }
                } else {
                    /* Simple moves */
                    int ni = i + dr[d], nj = j + dc[d];
                    if (ni >= 0 && ni < BOARD_SIZE && nj >= 0 && nj < BOARD_SIZE
                        && board[ni][nj] == ' ') {
                        moves[count].fromR = i;
                        moves[count].fromC = j;
                        moves[count].toR = ni;
                        moves[count].toC = nj;
                        count++;
                    }
                }
            }
        }
    }
    return count;
}

/* ---- Board evaluation function ---- */
static int evaluate(char **board, PlayerInfo *me, PlayerInfo *opp) {
    int score = 0;
    int my_pieces = 0, opp_pieces = 0;

    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            char c = board[i][j];
            if (c == me->symbol) {
                score += 3;
                my_pieces++;
                /* Advancement bonus: closer to promotion = better */
                if (me->forwardDir == -1)
                    score += (BOARD_SIZE - 1 - i);  /* P1 moves up */
                else
                    score += i;  /* P2 moves down */
                /* Center bonus */
                if (j >= 2 && j <= 5 && i >= 2 && i <= 5)
                    score += 1;
            } else if (c == me->king) {
                score += 5;
                my_pieces++;
                /* Kings near center are stronger */
                if (j >= 2 && j <= 5 && i >= 2 && i <= 5)
                    score += 2;
            } else if (c == opp->symbol) {
                score -= 3;
                opp_pieces++;
                if (opp->forwardDir == -1)
                    score -= (BOARD_SIZE - 1 - i);
                else
                    score -= i;
                if (j >= 2 && j <= 5 && i >= 2 && i <= 5)
                    score -= 1;
            } else if (c == opp->king) {
                score -= 5;
                opp_pieces++;
                if (j >= 2 && j <= 5 && i >= 2 && i <= 5)
                    score -= 2;
            }
        }
    }

    /* Massive bonus/penalty for eliminating all pieces */
    if (opp_pieces == 0) score += 1000;
    if (my_pieces == 0)  score -= 1000;

    return score;
}

/* ---- Execute a move on a board (for simulation) ---- */
static int sim_move(char **board, int fromR, int fromC, int toR, int toC,
                    char myNormal, char myKing, char oppNormal, char oppKing,
                    int backRow) {
    int dr = (toR > fromR) ? 1 : -1;
    int dc = (toC > fromC) ? 1 : -1;
    int dist = abs(toR - fromR);

    char piece = board[fromR][fromC];
    board[fromR][fromC] = ' ';
    int captured = 0;

    if (dist == 2) {
        /* Capture */
        int mr = fromR + dr, mc = fromC + dc;
        board[mr][mc] = ' ';
        captured = 1;
    }

    board[toR][toC] = piece;

    /* Promotion */
    if (toR == backRow && piece == myNormal)
        board[toR][toC] = myKing;

    (void)oppNormal; (void)oppKing;
    return captured;
}

/* ---- Minimax with alpha-beta pruning ---- */
static int minimax(char **board, PlayerInfo *me, PlayerInfo *opp,
                   int depth, int alpha, int beta, int maximizing) {
    if (depth == 0) return evaluate(board, me, opp);

    PlayerInfo *cur = maximizing ? me : opp;
    PlayerInfo *other = maximizing ? opp : me;

    AIMove moves[MAX_AI_MOVES];
    int n = generate_moves(board, cur, other, moves, MAX_AI_MOVES);

    if (n == 0) {
        /* No moves = loss for current player */
        return maximizing ? -900 - depth : 900 + depth;
    }

    if (maximizing) {
        int best = INT_MIN;
        for (int i = 0; i < n; i++) {
            char **child = clone_board(board);
            if (!child) continue;
            sim_move(child, moves[i].fromR, moves[i].fromC,
                     moves[i].toR, moves[i].toC,
                     cur->symbol, cur->king, other->symbol, other->king,
                     cur->backRow);
            int val = minimax(child, me, opp, depth - 1, alpha, beta, 0);
            free_board(child);
            if (val > best) best = val;
            if (val > alpha) alpha = val;
            if (beta <= alpha) break; /* prune */
        }
        return best;
    } else {
        int best = INT_MAX;
        for (int i = 0; i < n; i++) {
            char **child = clone_board(board);
            if (!child) continue;
            sim_move(child, moves[i].fromR, moves[i].fromC,
                     moves[i].toR, moves[i].toC,
                     cur->symbol, cur->king, other->symbol, other->king,
                     cur->backRow);
            int val = minimax(child, me, opp, depth - 1, alpha, beta, 1);
            free_board(child);
            if (val < best) best = val;
            if (val < beta) beta = val;
            if (beta <= alpha) break; /* prune */
        }
        return best;
    }
}

/* ---- Public API ---- */

int ai_choose_move(char **board, PlayerInfo *me, PlayerInfo *opp,
                   int *fromR, int *fromC, int *toR, int *toC,
                   int difficulty) {
    ai_seed();

    AIMove moves[MAX_AI_MOVES];
    int n = generate_moves(board, me, opp, moves, MAX_AI_MOVES);
    if (n == 0) return -1;

    int chosen = 0;

    switch (difficulty) {
    case AI_EASY:
        /* Purely random */
        chosen = rand() % n;
        break;

    case AI_MEDIUM: {
        /* Prefer captures (they're already filtered if available), then random */
        /* If there are captures, pick a random one. Otherwise random move. */
        chosen = rand() % n;

        /* Among available moves, prefer those that capture kings */
        int best_score = -1;
        for (int i = 0; i < n; i++) {
            int dr = moves[i].toR - moves[i].fromR;
            if (abs(dr) == 2) {
                int mr = (moves[i].fromR + moves[i].toR) / 2;
                int mc = (moves[i].fromC + moves[i].toC) / 2;
                int s = (board[mr][mc] == opp->king) ? 2 : 1;
                if (s > best_score) { best_score = s; chosen = i; }
            }
        }
        break;
    }

    case AI_HARD: {
        /* Minimax with alpha-beta */
        int best_score = INT_MIN;
        for (int i = 0; i < n; i++) {
            char **child = clone_board(board);
            if (!child) continue;
            sim_move(child, moves[i].fromR, moves[i].fromC,
                     moves[i].toR, moves[i].toC,
                     me->symbol, me->king, opp->symbol, opp->king,
                     me->backRow);
            int score = minimax(child, me, opp, MINIMAX_DEPTH - 1,
                               INT_MIN, INT_MAX, 0);
            free_board(child);

            /* Add small random noise to break ties */
            score += (rand() % 3) - 1;

            if (score > best_score) {
                best_score = score;
                chosen = i;
            }
        }
        break;
    }

    default:
        chosen = rand() % n;
        break;
    }

    *fromR = moves[chosen].fromR;
    *fromC = moves[chosen].fromC;
    *toR   = moves[chosen].toR;
    *toC   = moves[chosen].toC;
    return 0;
}
