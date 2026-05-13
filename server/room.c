/*
 * room.c — Game room with reconnect, timeouts, and structured logging.
 *
 * Threading: each room runs in its own pthread.
 * The game loop uses select() to monitor:
 *   - Current player's socket (for moves)
 *   - notify_pipe (for reconnect signals from main thread)
 * This enables move timeouts and reconnect without extra threads.
 */

#include "room.h"
#include "db.h"
#include "log.h"
#include "../src/game/moves.h"
#include "../src/game/rules.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/select.h>

pthread_mutex_t g_rooms_mutex = PTHREAD_MUTEX_INITIALIZER;
static int next_room_id = 1;

/* ---- Lobby (main thread only) ---- */

void lobby_init(LobbyClient *lobby) {
    for (int i = 0; i < MAX_LOBBY; i++) { lobby[i].fd = -1; lobby[i].has_name = 0; }
}
int lobby_add(LobbyClient *lobby, int fd) {
    for (int i = 0; i < MAX_LOBBY; i++)
        if (lobby[i].fd < 0) { lobby[i].fd = fd; lobby[i].has_name = 0; lobby[i].name[0] = '\0'; return i; }
    return -1;
}
void lobby_remove(LobbyClient *lobby, int index) {
    if (lobby[index].fd >= 0) close(lobby[index].fd);
    lobby[index].fd = -1; lobby[index].has_name = 0;
}
int lobby_find_match(LobbyClient *lobby, int *p1, int *p2) {
    int found = 0;
    for (int i = 0; i < MAX_LOBBY; i++)
        if (lobby[i].fd >= 0 && lobby[i].has_name) {
            if (!found) { *p1 = i; found = 1; } else { *p2 = i; return 1; }
        }
    return 0;
}

/* ---- Room lifecycle ---- */

void rooms_init(GameRoom *rooms) {
    for (int i = 0; i < MAX_ROOMS; i++) { rooms[i].active = 0; rooms[i].board = NULL; }
}

int room_create_and_start(GameRoom *rooms, int p1_fd, const char *p1_name,
                          int p2_fd, const char *p2_name) {
    pthread_mutex_lock(&g_rooms_mutex);
    int slot = -1;
    for (int i = 0; i < MAX_ROOMS; i++) if (!rooms[i].active) { slot = i; break; }
    if (slot < 0) { pthread_mutex_unlock(&g_rooms_mutex); return -1; }

    GameRoom *r = &rooms[slot];
    memset(r, 0, sizeof(GameRoom));
    r->room_id = next_room_id++; r->active = 1;
    r->p1_fd = p1_fd; r->p2_fd = p2_fd;
    pthread_mutex_init(&r->room_lock, NULL);

    strncpy(r->p1.name, p1_name, sizeof(r->p1.name) - 1);
    r->p1.symbol = 'x'; r->p1.king = 'X'; r->p1.forwardDir = -1; r->p1.backRow = 0;
    strncpy(r->p2.name, p2_name, sizeof(r->p2.name) - 1);
    r->p2.symbol = 'o'; r->p2.king = 'O'; r->p2.forwardDir = 1; r->p2.backRow = 7;

    r->board = board_create();
    if (!r->board) { r->active = 0; pthread_mutex_unlock(&g_rooms_mutex); return -1; }
    board_init(r->board, r->p1.symbol, r->p2.symbol);
    r->currentTurn = 1; r->gameStatus = STATUS_PLAYING;

    /* Generate reconnect tokens */
    proto_generate_token(r->p1_token);
    proto_generate_token(r->p2_token);

    /* Create notify pipe for reconnect signaling */
    if (pipe(r->notify_pipe) < 0) {
        board_destroy(r->board); r->board = NULL; r->active = 0;
        pthread_mutex_unlock(&g_rooms_mutex); return -1;
    }

    LOG_INFO("[ROOM %d] Created: %s vs %s", r->room_id, r->p1.name, r->p2.name);
    pthread_mutex_unlock(&g_rooms_mutex);

    int err = pthread_create(&r->thread, NULL, room_thread_func, r);
    if (err) {
        LOG_ERROR("[ROOM %d] pthread_create failed: %d", r->room_id, err);
        pthread_mutex_lock(&g_rooms_mutex);
        board_destroy(r->board); r->board = NULL; r->active = 0;
        close(r->notify_pipe[0]); close(r->notify_pipe[1]);
        pthread_mutex_unlock(&g_rooms_mutex);
        return -1;
    }
    pthread_detach(r->thread);
    return slot;
}

/* Called by main thread: find room by token, set new fd, signal room thread */
int room_reconnect(GameRoom *rooms, const char *token, int new_fd) {
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (!rooms[i].active) continue;
        GameRoom *r = &rooms[i];
        pthread_mutex_lock(&r->room_lock);
        if (r->p1_disconnected && strcmp(r->p1_token, token) == 0) {
            r->p1_fd = new_fd;
            r->p1_disconnected = 0;
            LOG_INFO("[ROOM %d] %s reconnected (P1)", r->room_id, r->p1.name);
            /* Signal room thread */
            char sig = 1;
            if (write(r->notify_pipe[1], &sig, 1) < 0)
                LOG_ERROR("[ROOM %d] pipe write failed", r->room_id);
            pthread_mutex_unlock(&r->room_lock);
            return 1;
        }
        if (r->p2_disconnected && strcmp(r->p2_token, token) == 0) {
            r->p2_fd = new_fd;
            r->p2_disconnected = 0;
            LOG_INFO("[ROOM %d] %s reconnected (P2)", r->room_id, r->p2.name);
            char sig = 1;
            if (write(r->notify_pipe[1], &sig, 1) < 0)
                LOG_ERROR("[ROOM %d] pipe write failed", r->room_id);
            pthread_mutex_unlock(&r->room_lock);
            return 1;
        }
        pthread_mutex_unlock(&r->room_lock);
    }
    return 0;
}

/* ---- Room thread helpers ---- */

static void flatten_board(char **board, char flat[BOARD_SZ][BOARD_SZ]) {
    for (int i = 0; i < BOARD_SZ; i++)
        for (int j = 0; j < BOARD_SZ; j++) flat[i][j] = board[i][j];
}

static int broadcast_board(GameRoom *r, const char *msg) {
    char flat[BOARD_SZ][BOARD_SZ]; flatten_board(r->board, flat);
    PlayerInfo *cur = (r->currentTurn == 1) ? &r->p1 : &r->p2;
    PlayerInfo *opp = (r->currentTurn == 1) ? &r->p2 : &r->p1;
    int mc = r->inMultiCapture || moves_player_has_capture(r->board,
        cur->symbol, cur->king, opp->symbol, opp->king, cur->forwardDir);
    Packet pkt;
    proto_pack_board(&pkt, flat, r->p1.score, r->p2.score, r->currentTurn,
                     mc, r->inMultiCapture, r->mcRow, r->mcCol, msg);
    int fail = 0;
    pthread_mutex_lock(&r->room_lock);
    if (!r->p1_disconnected && send_packet(r->p1_fd, &pkt) < 0) fail = -1;
    if (!r->p2_disconnected && send_packet(r->p2_fd, &pkt) < 0) fail = -1;
    pthread_mutex_unlock(&r->room_lock);
    return fail;
}

static int send_error(int fd, const char *msg) {
    Packet pkt; proto_pack_error(&pkt, msg);
    return send_packet(fd, &pkt);
}

static void broadcast_win(GameRoom *r, const char *msg) {
    Packet pkt; proto_pack_win(&pkt, r->gameStatus, r->p1.score, r->p2.score, msg);
    pthread_mutex_lock(&r->room_lock);
    if (!r->p1_disconnected) send_packet(r->p1_fd, &pkt);
    if (!r->p2_disconnected) send_packet(r->p2_fd, &pkt);
    pthread_mutex_unlock(&r->room_lock);

    /* Record result in database and send leaderboard */
    if (r->gameStatus == STATUS_P1_WINS)
        db_record_win(r->p1.name, r->p2.name);
    else if (r->gameStatus == STATUS_P2_WINS)
        db_record_win(r->p2.name, r->p1.name);
    else if (r->gameStatus == STATUS_DRAW)
        db_record_draw(r->p1.name, r->p2.name);

    LeaderboardEntry lb[LB_MAX_ENTRIES];
    int count = db_get_leaderboard(lb, LB_MAX_ENTRIES);
    if (count > 0) {
        LBEntry lbe[LB_MAX_ENTRIES];
        for (int i = 0; i < count; i++) {
            memset(&lbe[i], 0, sizeof(LBEntry));
            strncpy(lbe[i].name, lb[i].name, LB_NAME_LEN - 1);
            lbe[i].wins = lb[i].wins;
            lbe[i].losses = lb[i].losses;
            lbe[i].rating = lb[i].rating;
        }
        Packet lp; proto_pack_leaderboard(&lp, lbe, count);
        pthread_mutex_lock(&r->room_lock);
        if (!r->p1_disconnected) send_packet(r->p1_fd, &lp);
        if (!r->p2_disconnected) send_packet(r->p2_fd, &lp);
        pthread_mutex_unlock(&r->room_lock);
    }
}

static int move_to_dir(const MoveData *m, int *dr, int *dc) {
    int rd = m->toRow - m->fromRow, cd = m->toCol - m->fromCol;
    if (abs(rd) == 1 && abs(cd) == 1) { *dr = rd; *dc = cd; return 0; }
    if (abs(rd) == 2 && abs(cd) == 2) { *dr = rd/2; *dc = cd/2; return 0; }
    return -1;
}
static int dir_to_num(int dr, int dc) {
    if (dr==-1&&dc==-1) return 1; if (dr==-1&&dc==1) return 2;
    if (dr==1&&dc==-1) return 3; if (dr==1&&dc==1) return 4; return 0;
}

static void room_cleanup(GameRoom *r) {
    LOG_INFO("[ROOM %d] Cleaning up.", r->room_id);
    pthread_mutex_lock(&r->room_lock);
    if (r->p1_fd >= 0) close(r->p1_fd);
    if (r->p2_fd >= 0) close(r->p2_fd);
    r->p1_fd = -1; r->p2_fd = -1;
    pthread_mutex_unlock(&r->room_lock);
    close(r->notify_pipe[0]); close(r->notify_pipe[1]);
    if (r->board) { board_destroy(r->board); r->board = NULL; }
    pthread_mutex_destroy(&r->room_lock);
    pthread_mutex_lock(&g_rooms_mutex);
    r->active = 0;
    pthread_mutex_unlock(&g_rooms_mutex);
}

/* ---- Receive with timeout using select() ----
   Returns: 0 = packet received, -1 = disconnect, -2 = timeout, -3 = reconnect signal */
static int recv_with_timeout(GameRoom *r, int fd, Packet *pkt, int timeout_sec) {
    struct timeval tv;
    tv.tv_sec = timeout_sec; tv.tv_usec = 0;
    fd_set fds; FD_ZERO(&fds);
    FD_SET(fd, &fds);
    FD_SET(r->notify_pipe[0], &fds);
    int maxfd = (fd > r->notify_pipe[0]) ? fd : r->notify_pipe[0];

    int ret = select(maxfd + 1, &fds, NULL, NULL, &tv);
    if (ret < 0) {
        if (errno == EINTR) return -2; /* treat interrupt as timeout */
        return -1;
    }
    if (ret == 0) return -2; /* timeout */

    /* Check notify pipe first (reconnect signal) */
    if (FD_ISSET(r->notify_pipe[0], &fds)) {
        char buf; read(r->notify_pipe[0], &buf, 1); /* drain */
        return -3; /* reconnect happened */
    }

    /* Data on player socket */
    if (FD_ISSET(fd, &fds)) {
        if (recv_packet(fd, pkt) < 0) return -1; /* disconnect */
        return 0;
    }
    return -2;
}

/* ---- Wait for reconnect (blocks up to RECONNECT_TIMEOUT) ----
   Returns: 1 = reconnected, 0 = timed out */
static int wait_for_reconnect(GameRoom *r, int playerNum) {
    const char *name = (playerNum == 1) ? r->p1.name : r->p2.name;
    LOG_WARN("[ROOM %d] %s disconnected. Waiting %ds for reconnect...",
             r->room_id, name, RECONNECT_TIMEOUT);

    /* Notify the other player */
    int other_fd = (playerNum == 1) ? r->p2_fd : r->p1_fd;
    int other_dc = (playerNum == 1) ? r->p2_disconnected : r->p1_disconnected;
    if (!other_dc) {
        char msg[MSG_LEN];
        snprintf(msg, MSG_LEN, "%s disconnected. Waiting for reconnect (%ds)...",
                 name, RECONNECT_TIMEOUT);
        Packet pkt; proto_pack_turn(&pkt, r->currentTurn, msg);
        send_packet(other_fd, &pkt);
    }

    /* Wait on notify_pipe with timeout */
    struct timeval tv;
    tv.tv_sec = RECONNECT_TIMEOUT; tv.tv_usec = 0;
    fd_set fds; FD_ZERO(&fds);
    FD_SET(r->notify_pipe[0], &fds);

    int ret = select(r->notify_pipe[0] + 1, &fds, NULL, NULL, &tv);
    if (ret > 0 && FD_ISSET(r->notify_pipe[0], &fds)) {
        char buf; read(r->notify_pipe[0], &buf, 1);
        /* Check if the player actually reconnected */
        pthread_mutex_lock(&r->room_lock);
        int dc = (playerNum == 1) ? r->p1_disconnected : r->p2_disconnected;
        pthread_mutex_unlock(&r->room_lock);
        if (!dc) {
            LOG_INFO("[ROOM %d] %s reconnected! Resuming game.", r->room_id, name);
            /* Send current board state to reconnected player */
            int new_fd = (playerNum == 1) ? r->p1_fd : r->p2_fd;
            /* Re-send welcome so client has game info */
            const char *tok = (playerNum == 1) ? r->p1_token : r->p2_token;
            Packet wp;
            proto_pack_welcome(&wp, playerNum, r->p1.name, r->p2.name,
                               r->p1.symbol, r->p2.symbol, r->p1.king, r->p2.king, tok);
            send_packet(new_fd, &wp);
            /* Notify opponent that player is back */
            if (!other_dc) {
                char msg[MSG_LEN];
                snprintf(msg, MSG_LEN, "%s reconnected!", name);
                Packet tp; proto_pack_turn(&tp, r->currentTurn, msg);
                send_packet(other_fd, &tp);
            }
            return 1;
        }
    }
    LOG_WARN("[ROOM %d] Reconnect timeout for %s.", r->room_id, name);
    return 0;
}

/* ================================================================== */
/*  Room thread — game loop with select-based timeouts                 */
/* ================================================================== */
void *room_thread_func(void *arg) {
    GameRoom *r = (GameRoom *)arg;
    int rid = r->room_id;

    /* Send MSG_WELCOME with tokens */
    Packet pkt;
    proto_pack_welcome(&pkt, 1, r->p1.name, r->p2.name,
                       r->p1.symbol, r->p2.symbol, r->p1.king, r->p2.king, r->p1_token);
    if (send_packet(r->p1_fd, &pkt) < 0) goto done;
    proto_pack_welcome(&pkt, 2, r->p1.name, r->p2.name,
                       r->p1.symbol, r->p2.symbol, r->p1.king, r->p2.king, r->p2_token);
    if (send_packet(r->p2_fd, &pkt) < 0) goto done;

    rules_calc_scores(r->board, &r->p1, &r->p2);
    { char m[MSG_LEN]; snprintf(m, MSG_LEN, "%s's turn.", r->p1.name); broadcast_board(r, m); }
    LOG_INFO("[ROOM %d] Game started.", rid);

    while (r->gameStatus == STATUS_PLAYING) {
        PlayerInfo *cur = (r->currentTurn == 1) ? &r->p1 : &r->p2;
        PlayerInfo *opp = (r->currentTurn == 1) ? &r->p2 : &r->p1;

        if (!rules_has_valid_move(r->board, cur->symbol, opp->symbol,
                                  cur->king, opp->king, cur->forwardDir)) {
            r->gameStatus = (r->currentTurn == 1) ? STATUS_P2_WINS : STATUS_P1_WINS;
            rules_calc_scores(r->board, &r->p1, &r->p2);
            char m[MSG_LEN]; snprintf(m, MSG_LEN, "%s has no moves! %s wins!", cur->name, opp->name);
            LOG_INFO("[ROOM %d] %s", rid, m); broadcast_win(r, m); break;
        }

        /* Get current player's fd (under lock since reconnect can change it) */
        pthread_mutex_lock(&r->room_lock);
        int cur_fd = (r->currentTurn == 1) ? r->p1_fd : r->p2_fd;
        pthread_mutex_unlock(&r->room_lock);

        /* Receive move with timeout */
        int rc = recv_with_timeout(r, cur_fd, &pkt, MOVE_TIMEOUT);

        if (rc == -2) {
            /* Move timeout — current player loses */
            r->gameStatus = (r->currentTurn == 1) ? STATUS_P2_WINS : STATUS_P1_WINS;
            rules_calc_scores(r->board, &r->p1, &r->p2);
            char m[MSG_LEN]; snprintf(m, MSG_LEN, "%s timed out! %s wins!", cur->name, opp->name);
            LOG_WARN("[ROOM %d] %s", rid, m); broadcast_win(r, m); break;
        }

        if (rc == -1) {
            /* Disconnect — try reconnect */
            pthread_mutex_lock(&r->room_lock);
            if (r->currentTurn == 1) { r->p1_disconnected = 1; close(r->p1_fd); r->p1_fd = -1; }
            else                     { r->p2_disconnected = 1; close(r->p2_fd); r->p2_fd = -1; }
            pthread_mutex_unlock(&r->room_lock);

            if (!wait_for_reconnect(r, r->currentTurn)) {
                /* Reconnect failed — opponent wins */
                r->gameStatus = (r->currentTurn == 1) ? STATUS_P2_WINS : STATUS_P1_WINS;
                rules_calc_scores(r->board, &r->p1, &r->p2);
                char m[MSG_LEN]; snprintf(m, MSG_LEN, "%s disconnected. %s wins!", cur->name, opp->name);
                LOG_INFO("[ROOM %d] %s", rid, m); broadcast_win(r, m); break;
            }
            /* Reconnected! Re-send board and continue */
            char m[MSG_LEN]; snprintf(m, MSG_LEN, "%s's turn.", cur->name);
            broadcast_board(r, m);
            continue; /* re-enter the loop */
        }

        if (rc == -3) {
            /* Reconnect signal while waiting for move (shouldn't normally happen,
               but handle gracefully — just re-loop) */
            continue;
        }

        /* rc == 0: packet received */
        if (pkt.type != MSG_MOVE) {
            LOG_WARN("[ROOM %d] Unexpected %s from P%d", rid, proto_type_name(pkt.type), r->currentTurn);
            continue;
        }

        MoveData mv; proto_unpack_move(&pkt, &mv);
        LOG_INFO("[ROOM %d] P%d %s: (%d,%d)->(%d,%d)", rid, r->currentTurn,
                 cur->name, mv.fromRow, mv.fromCol, mv.toRow, mv.toCol);

        /* ---- Validation ---- */
        if (mv.fromRow < 0 || mv.fromRow >= BOARD_SZ || mv.fromCol < 0 || mv.fromCol >= BOARD_SZ ||
            mv.toRow < 0 || mv.toRow >= BOARD_SZ || mv.toCol < 0 || mv.toCol >= BOARD_SZ) {
            send_error(cur_fd, "Invalid coordinates!"); continue; }
        if (r->inMultiCapture && (mv.fromRow != r->mcRow || mv.fromCol != r->mcCol)) {
            char e[MSG_LEN]; snprintf(e, MSG_LEN, "Must jump from %c%d!", 'A'+r->mcCol, r->mcRow+1);
            send_error(cur_fd, e); continue; }
        if (!board_is_player_piece(r->board[mv.fromRow][mv.fromCol], cur->symbol, cur->king)) {
            send_error(cur_fd, "That's not your piece!"); continue; }

        int dr, dc;
        if (move_to_dir(&mv, &dr, &dc) < 0) { send_error(cur_fd, "Invalid geometry!"); continue; }
        if (!rules_is_valid_direction(dir_to_num(dr, dc), r->board[mv.fromRow][mv.fromCol],
                r->p1.symbol, r->p1.king, r->p2.symbol, r->p2.king)) {
            send_error(cur_fd, "Only kings move backward!"); continue; }

        int mustCap = r->inMultiCapture || moves_player_has_capture(r->board,
            cur->symbol, cur->king, opp->symbol, opp->king, cur->forwardDir);
        if (mustCap) {
            if (!r->inMultiCapture && !moves_can_capture_from(r->board, mv.fromRow, mv.fromCol,
                    cur->symbol, cur->king, opp->symbol, opp->king, cur->forwardDir)) {
                send_error(cur_fd, "This piece can't capture!"); continue; }
            if (!moves_is_capture_in_dir(r->board, mv.fromRow, mv.fromCol,
                    dr, dc, opp->symbol, opp->king)) {
                send_error(cur_fd, "You must capture!"); continue; }
        }

        /* Execute move */
        int row = mv.fromRow, col = mv.fromCol;
        int result = moves_try(r->board, &row, &col, dr, dc,
                               cur->symbol, cur->king, opp->symbol, opp->king);
        if (result == MOVE_INVALID) { send_error(cur_fd, "Move invalid!"); continue; }
        rules_check_promotion(r->board, row, col, cur->symbol, cur->king, cur->backRow);

        if (result == MOVE_CAPTURE && moves_can_capture_from(r->board, row, col,
                cur->symbol, cur->king, opp->symbol, opp->king, cur->forwardDir)) {
            r->inMultiCapture = 1; r->mcRow = row; r->mcCol = col;
            char m[MSG_LEN]; snprintf(m, MSG_LEN, "Multi-capture! Jump from %c%d.", 'A'+col, row+1);
            broadcast_board(r, m); continue;
        }

        r->inMultiCapture = 0;
        rules_calc_scores(r->board, &r->p1, &r->p2);

        if (rules_is_game_over(r->board, &r->p1, &r->p2)) {
            if (r->p1.score > r->p2.score) r->gameStatus = STATUS_P1_WINS;
            else if (r->p2.score > r->p1.score) r->gameStatus = STATUS_P2_WINS;
            else r->gameStatus = STATUS_DRAW;
            char m[MSG_LEN];
            if (r->gameStatus == STATUS_DRAW) snprintf(m, MSG_LEN, "It's a draw!");
            else snprintf(m, MSG_LEN, "%s wins!", (r->gameStatus == STATUS_P1_WINS) ? r->p1.name : r->p2.name);
            LOG_INFO("[ROOM %d] %s", rid, m); broadcast_win(r, m); break;
        }

        r->currentTurn = (r->currentTurn == 1) ? 2 : 1;
        PlayerInfo *next = (r->currentTurn == 1) ? &r->p1 : &r->p2;
        PlayerInfo *prev = (r->currentTurn == 1) ? &r->p2 : &r->p1;
        if (!rules_has_valid_move(r->board, next->symbol, prev->symbol,
                                  next->king, prev->king, next->forwardDir)) {
            r->gameStatus = (r->currentTurn == 1) ? STATUS_P2_WINS : STATUS_P1_WINS;
            char m[MSG_LEN]; snprintf(m, MSG_LEN, "%s has no moves! %s wins!", next->name, prev->name);
            LOG_INFO("[ROOM %d] %s", rid, m); broadcast_win(r, m); break;
        }

        char m[MSG_LEN]; snprintf(m, MSG_LEN, "%s's turn.", next->name);
        broadcast_board(r, m);
    }

done:
    room_cleanup(r);
    LOG_INFO("[ROOM %d] Thread exiting.", rid);
    return NULL;
}

/* ---- AI game (stub — creates room with AI as P2) ---- */
int room_create_ai_game(GameRoom *rooms, int p1_fd,
                        const char *p1_name, int difficulty) {
    (void)difficulty;
    /* Reuse normal room creation with a placeholder AI name.
       The AI move logic would be integrated into room_thread_func
       when ai_enabled is set. For now, return -1 (not yet wired). */
    int slot = room_create_and_start(rooms, p1_fd, p1_name, -1, "AI");
    if (slot >= 0) {
        pthread_mutex_lock(&g_rooms_mutex);
        rooms[slot].ai_enabled = 1;
        rooms[slot].ai_difficulty = difficulty;
        pthread_mutex_unlock(&g_rooms_mutex);
    }
    return slot;
}

/* ---- Spectator support ---- */
int room_add_spectator(GameRoom *rooms, int room_id, int fd) {
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (!rooms[i].active || rooms[i].room_id != room_id) continue;
        GameRoom *r = &rooms[i];
        pthread_mutex_lock(&r->room_lock);
        if (r->spectator_count >= MAX_SPECTATORS) {
            pthread_mutex_unlock(&r->room_lock);
            return 0;
        }
        r->spectator_fds[r->spectator_count++] = fd;
        pthread_mutex_unlock(&r->room_lock);
        LOG_INFO("[ROOM %d] Spectator added (fd=%d, total=%d)",
                 room_id, fd, r->spectator_count);
        return 1;
    }
    return 0; /* room not found */
}

/* ---- Room list for lobby ---- */
int room_get_list(GameRoom *rooms, RoomInfo *out, int max) {
    int count = 0;
    pthread_mutex_lock(&g_rooms_mutex);
    for (int i = 0; i < MAX_ROOMS && count < max; i++) {
        if (!rooms[i].active) continue;
        memset(&out[count], 0, sizeof(RoomInfo));
        out[count].room_id = rooms[i].room_id;
        strncpy(out[count].p1_name, rooms[i].p1.name, LB_NAME_LEN - 1);
        strncpy(out[count].p2_name, rooms[i].p2.name, LB_NAME_LEN - 1);
        out[count].turn_number = rooms[i].turn_number;
        count++;
    }
    pthread_mutex_unlock(&g_rooms_mutex);
    return count;
}
