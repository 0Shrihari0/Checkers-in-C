/*
 * client.c — Checkers Multiplayer Client
 * Uses the structured protocol for all communication.
 * Supports /stats and /leaderboard commands while in the lobby.
 */

#include "../shared/protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Cached game info */
static WelcomeData g_welcome;
static char g_token[TOKEN_LEN];  /* reconnect token */
static const char *g_host;
static int g_port;

/* ---- Board rendering ---- */
static void render_board(const BoardData *bd) {
    printf("\n");
    for (int i = 0; i < BOARD_SZ; i++) {
        printf("\n  +--+--+--+--+--+--+--+--+");
        printf("\n%d |", i + 1);
        for (int j = 0; j < BOARD_SZ; j++)
            printf(" %c|", bd->board[i][j]);
    }
    printf("\n  +--+--+--+--+--+--+--+--+");
    printf("\n    A  B  C  D  E  F  G  H\n");
}

static void render_info(const BoardData *bd, int myNum) {
    printf("\n  %s (%c/%c) : %d pts    vs    %s (%c/%c) : %d pts\n",
           g_welcome.p1Name, g_welcome.p1Symbol, g_welcome.p1King, bd->p1Score,
           g_welcome.p2Name, g_welcome.p2Symbol, g_welcome.p2King, bd->p2Score);
    printf("  You are Player %d (%c)\n", myNum,
           (myNum == 1) ? g_welcome.p1Symbol : g_welcome.p2Symbol);
    if (bd->message[0])
        printf("\n  >> %s\n", bd->message);
}

/* ---- Leaderboard rendering ---- */
static void render_leaderboard(const LeaderboardData *lb) {
    printf("\n  ╔══════════════════════════════════════════╗\n");
    printf("  ║           🏆  LEADERBOARD  🏆            ║\n");
    printf("  ╠════╦══════════════╦══════╦══════╦════════╣\n");
    printf("  ║ #  ║ Player       ║  W/L ║ Win%% ║ Rating ║\n");
    printf("  ╠════╬══════════════╬══════╬══════╬════════╣\n");
    for (int i = 0; i < lb->count; i++) {
        int total = lb->entries[i].wins + lb->entries[i].losses;
        int pct = (total > 0) ? (lb->entries[i].wins * 100 / total) : 0;
        printf("  ║ %-2d ║ %-12s ║ %d/%-2d ║ %3d%% ║ %5d  ║\n",
               i + 1,
               lb->entries[i].name,
               lb->entries[i].wins, lb->entries[i].losses,
               pct,
               lb->entries[i].rating);
    }
    if (lb->count == 0) {
        printf("  ║         No players yet!                  ║\n");
    }
    printf("  ╚════╩══════════════╩══════╩══════╩════════╝\n\n");
}

/* ---- Stats rendering ---- */
static void render_stats(const StatsData *sd) {
    int total = sd->wins + sd->losses + sd->draws;
    int pct = (sd->wins + sd->losses > 0)
              ? (sd->wins * 100 / (sd->wins + sd->losses)) : 0;
    printf("\n  ┌────────────────────────────────┐\n");
    printf("  │  📊  Stats for %-14s  │\n", sd->name);
    printf("  ├────────────────────────────────┤\n");
    printf("  │  Rating:  %-5d   Rank: #%-5d │\n", sd->rating, sd->rank);
    printf("  │  Wins:    %-5d   Losses: %-4d │\n", sd->wins, sd->losses);
    printf("  │  Draws:   %-5d   Total:  %-4d │\n", sd->draws, total);
    printf("  │  Win%%:    %3d%%                  │\n", pct);
    printf("  └────────────────────────────────┘\n\n");
}

/* ---- Position parsing ---- */
static int parse_pos(const char *input, int *row, int *col) {
    if (!input[0] || !input[1]) return -1;
    char c = (char)toupper((unsigned char)input[0]);
    if (c < 'A' || c > 'H') return -1;
    *col = c - 'A';
    if (input[1] < '1' || input[1] > '8') return -1;
    *row = input[1] - '1';
    return 0;
}

/* ---- Move input ---- */
static int prompt_move(const BoardData *bd, MoveData *mv) {
    char input[16];

    if (bd->multiCapture) {
        mv->fromRow = bd->captureRow; mv->fromCol = bd->captureCol;
        printf("\n  [Multi-capture] Jumping from %c%d\n",
               'A' + bd->captureCol, bd->captureRow + 1);
        printf("  Destination (e.g. C4): ");
    } else {
        printf("\n  Source (e.g. A6): ");
        if (!fgets(input, sizeof(input), stdin)) return -1;
        input[strcspn(input, "\n")] = '\0';
        if (parse_pos(input, &mv->fromRow, &mv->fromCol) < 0) {
            printf("  Invalid! Use format like A6.\n"); return 1;
        }
        printf("  Destination (e.g. B5): ");
    }

    if (!fgets(input, sizeof(input), stdin)) return -1;
    input[strcspn(input, "\n")] = '\0';
    if (parse_pos(input, &mv->toRow, &mv->toCol) < 0) {
        printf("  Invalid! Use format like B5.\n"); return 1;
    }
    return 0;
}

/* ---- Auto-reconnect ---- */
static int try_reconnect(void) {
    printf("\n  Connection lost! Attempting reconnect...\n");
    for (int attempt = 1; attempt <= 5; attempt++) {
        printf("  Attempt %d/5...\n", attempt);
        sleep(1);

        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) continue;

        struct sockaddr_in sa;
        memset(&sa, 0, sizeof(sa));
        sa.sin_family = AF_INET;
        sa.sin_port = htons(g_port);
        inet_pton(AF_INET, g_host, &sa.sin_addr);

        if (connect(sock, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
            close(sock); continue;
        }

        /* Send reconnect token */
        Packet pkt;
        proto_pack_reconnect(&pkt, g_token);
        if (send_packet(sock, &pkt) < 0) { close(sock); continue; }

        /* Wait for response — should be MSG_WELCOME on success */
        if (recv_packet(sock, &pkt) < 0) { close(sock); continue; }

        if (pkt.type == MSG_WELCOME) {
            proto_unpack_welcome(&pkt, &g_welcome);
            printf("  Reconnected!\n");
            return sock;
        }
        if (pkt.type == MSG_ERROR) {
            ErrorData ed; proto_unpack_error(&pkt, &ed);
            printf("  %s\n", ed.message);
        }
        close(sock);
    }
    printf("  Reconnect failed.\n");
    return -1;
}

/* ---- Lobby wait with command support ---- */
static int lobby_wait(int sock) {
    /*
     * While waiting for a match, the client can type commands.
     * We use select() to multiplex between stdin and the socket.
     */
    printf("\n  Commands:  /stats  /leaderboard  /quit\n");

    while (1) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(sock, &fds);
        FD_SET(STDIN_FILENO, &fds);
        int maxfd = (sock > STDIN_FILENO) ? sock : STDIN_FILENO;

        int ret = select(maxfd + 1, &fds, NULL, NULL, NULL);
        if (ret < 0) { perror("select"); return -1; }

        /* Server sent something */
        if (FD_ISSET(sock, &fds)) {
            Packet pkt;
            if (recv_packet(sock, &pkt) < 0) return -1;

            if (pkt.type == MSG_WELCOME) {
                proto_unpack_welcome(&pkt, &g_welcome);
                strncpy(g_token, g_welcome.token, TOKEN_LEN - 1);
                return g_welcome.playerNum;
            }
            if (pkt.type == MSG_LEADERBOARD) {
                LeaderboardData lb;
                proto_unpack_leaderboard(&pkt, &lb);
                render_leaderboard(&lb);
                printf("  Waiting for opponent... (type /stats or /leaderboard)\n");
                continue;
            }
            if (pkt.type == MSG_STATS_RESP) {
                StatsData sd;
                proto_unpack_stats_resp(&pkt, &sd);
                render_stats(&sd);
                printf("  Waiting for opponent... (type /stats or /leaderboard)\n");
                continue;
            }
            if (pkt.type == MSG_ERROR) {
                ErrorData ed; proto_unpack_error(&pkt, &ed);
                printf("  !! %s\n", ed.message);
                continue;
            }
            /* Unexpected message */
            printf("  [?] %s\n", proto_type_name(pkt.type));
        }

        /* User typed something */
        if (FD_ISSET(STDIN_FILENO, &fds)) {
            char cmd[64];
            if (!fgets(cmd, sizeof(cmd), stdin)) return -1;
            cmd[strcspn(cmd, "\n")] = '\0';

            if (strcmp(cmd, "/stats") == 0) {
                Packet pkt; proto_pack_stats_req(&pkt);
                if (send_packet(sock, &pkt) < 0) return -1;
            } else if (strcmp(cmd, "/leaderboard") == 0 || strcmp(cmd, "/lb") == 0) {
                Packet pkt; proto_pack_lb_req(&pkt);
                if (send_packet(sock, &pkt) < 0) return -1;
            } else if (strcmp(cmd, "/quit") == 0 || strcmp(cmd, "/q") == 0) {
                return -1;
            } else if (cmd[0] == '/') {
                printf("  Unknown command. Try /stats, /leaderboard, or /quit\n");
            }
            /* Ignore non-command input */
        }
    }
}

/* ================================================================== */
int main(int argc, char *argv[]) {
    const char *host = "127.0.0.1";
    int port = DEFAULT_PORT;
    if (argc >= 2) host = argv[1];
    if (argc >= 3) {
        port = atoi(argv[2]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Invalid port: %s\n", argv[2]);
            return EXIT_FAILURE;
        }
    }
    g_host = host; g_port = port;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return EXIT_FAILURE; }

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port   = htons(port);
    if (inet_pton(AF_INET, host, &sa.sin_addr) <= 0) {
        fprintf(stderr, "Invalid address: %s\n", host);
        close(sock); return EXIT_FAILURE;
    }

    printf("Connecting to %s:%d...\n", host, port);
    if (connect(sock, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        perror("connect"); close(sock); return EXIT_FAILURE;
    }
    printf("Connected!\n");

    /* Send MSG_JOIN */
    char name[NAME_LEN];
    printf("Enter your name: ");
    if (!fgets(name, sizeof(name), stdin)) { close(sock); return EXIT_FAILURE; }
    name[strcspn(name, "\n")] = '\0';
    if (!name[0]) strcpy(name, "Player");

    Packet pkt;
    proto_pack_join(&pkt, name);
    if (send_packet(sock, &pkt) < 0) {
        fprintf(stderr, "Failed to send name.\n");
        close(sock); return EXIT_FAILURE;
    }

    /* Wait for MSG_WAIT then enter interactive lobby */
    while (1) {
        if (recv_packet(sock, &pkt) < 0) {
            fprintf(stderr, "Disconnected.\n");
            close(sock); return EXIT_FAILURE;
        }
        if (pkt.type == MSG_WAIT) {
            printf("\033[2J\033[H=== CHECKERS ONLINE ===\n\n");
            printf("  Searching for opponent...\n");
            break;
        }
        if (pkt.type == MSG_WELCOME) {
            proto_unpack_welcome(&pkt, &g_welcome);
            strncpy(g_token, g_welcome.token, TOKEN_LEN - 1);
            printf("\nPlayer %d! Game starting...\n", g_welcome.playerNum);
            goto game_start;
        }
    }

    /* Interactive lobby — supports /stats and /leaderboard */
    {
        int myNum = lobby_wait(sock);
        if (myNum < 0) {
            close(sock);
            printf("\nGoodbye!\n");
            return EXIT_SUCCESS;
        }
        printf("\nPlayer %d! Game starting...\n", myNum);
    }

game_start:;
    int myNum = g_welcome.playerNum;

    /* ---- Game loop ---- */
    int running = 1;
    BoardData lastBoard;
    memset(&lastBoard, 0, sizeof(lastBoard));

    while (running) {
        if (recv_packet(sock, &pkt) < 0) {
            /* Try auto-reconnect */
            int new_sock = try_reconnect();
            if (new_sock < 0) {
                printf("\nDisconnected from server.\n"); break;
            }
            close(sock);
            sock = new_sock;
            continue; /* resume receiving */
        }

        switch (pkt.type) {
        case MSG_BOARD: {
            BoardData bd;
            proto_unpack_board(&pkt, &bd);
            lastBoard = bd;

            printf("\033[2J\033[H=== CHECKERS ONLINE ===\n");
            render_board(&bd);
            render_info(&bd, myNum);

            if (bd.currentTurn == myNum) {
                int rc = 1;
                while (rc == 1) {
                    MoveData mv; memset(&mv, 0, sizeof(mv));
                    rc = prompt_move(&bd, &mv);
                    if (rc == -1) { running = 0; break; }
                    if (rc == 1) continue;

                    proto_pack_move(&pkt, mv.fromRow, mv.fromCol,
                                   mv.toRow, mv.toCol);
                    if (send_packet(sock, &pkt) < 0) {
                        printf("\nDisconnected.\n");
                        running = 0;
                    }
                }
            } else {
                printf("\n  Waiting for %s...\n",
                       (bd.currentTurn == 1) ? g_welcome.p1Name
                                             : g_welcome.p2Name);
            }
            break;
        }

        case MSG_ERROR: {
            ErrorData ed;
            proto_unpack_error(&pkt, &ed);
            printf("\n  !! %s\n", ed.message);

            /* Re-prompt (we're still on our turn) */
            int rc = 1;
            while (rc == 1) {
                MoveData mv; memset(&mv, 0, sizeof(mv));
                rc = prompt_move(&lastBoard, &mv);
                if (rc == -1) { running = 0; break; }
                if (rc == 1) continue;

                proto_pack_move(&pkt, mv.fromRow, mv.fromCol,
                               mv.toRow, mv.toCol);
                if (send_packet(sock, &pkt) < 0) {
                    printf("\nDisconnected.\n");
                    running = 0;
                }
            }
            break;
        }

        case MSG_WIN: {
            WinData wd;
            proto_unpack_win(&pkt, &wd);

            /* Re-render board with final state if available */
            printf("\033[2J\033[H=== CHECKERS ONLINE ===\n");
            render_board(&lastBoard);

            printf("\n  =============================\n");
            printf("  GAME OVER!\n");
            printf("  %s: %d pts   %s: %d pts\n",
                   g_welcome.p1Name, wd.p1Score,
                   g_welcome.p2Name, wd.p2Score);
            printf("  %s\n", wd.message);
            printf("  =============================\n");
            /* Don't set running=0 yet — wait for leaderboard */
            break;
        }

        case MSG_LEADERBOARD: {
            LeaderboardData lb;
            proto_unpack_leaderboard(&pkt, &lb);
            render_leaderboard(&lb);
            running = 0; /* leaderboard comes after WIN, end game */
            break;
        }

        case MSG_STATS_RESP: {
            StatsData sd;
            proto_unpack_stats_resp(&pkt, &sd);
            render_stats(&sd);
            break;
        }

        case MSG_TURN: {
            TurnData td;
            proto_unpack_turn(&pkt, &td);
            printf("\n  >> %s\n", td.message);
            break;
        }

        default:
            printf("  [?] Unexpected: %s\n", proto_type_name(pkt.type));
            break;
        }
    }

    close(sock);
    printf("\nGoodbye!\n");
    return EXIT_SUCCESS;
}
