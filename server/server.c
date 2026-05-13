/*
 * server.c — Multithreaded game server with reconnect support.
 * Main thread: lobby + matchmaking + reconnect routing.
 */

#include "room.h"
#include "db.h"
#include "log.h"
#include "../shared/protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BACKLOG 8

/* ---- Lobby helpers for DB queries ---- */

static void handle_stats_req(int fd, const char *username) {
    PlayerStats ps;
    if (db_get_stats(username, &ps) < 0) {
        Packet ep; proto_pack_error(&ep, "Could not retrieve stats.");
        send_packet(fd, &ep); return;
    }
    int rank = db_get_rank(username);

    StatsData sd;
    memset(&sd, 0, sizeof(sd));
    strncpy(sd.name, ps.name, LB_NAME_LEN - 1);
    sd.wins   = ps.wins;
    sd.losses = ps.losses;
    sd.draws  = ps.draws;
    sd.rating = ps.rating;
    sd.rank   = (rank > 0) ? rank : 0;

    Packet pkt; proto_pack_stats_resp(&pkt, &sd);
    send_packet(fd, &pkt);
    LOG_INFO("[LOBBY] Sent stats for '%s' (rating %d, rank #%d)", username, sd.rating, sd.rank);
}

static void handle_lb_req(int fd) {
    LeaderboardEntry lb[LB_MAX_ENTRIES];
    int count = db_get_leaderboard(lb, LB_MAX_ENTRIES);

    LBEntry lbe[LB_MAX_ENTRIES];
    for (int i = 0; i < count; i++) {
        memset(&lbe[i], 0, sizeof(LBEntry));
        strncpy(lbe[i].name, lb[i].name, LB_NAME_LEN - 1);
        lbe[i].wins   = lb[i].wins;
        lbe[i].losses = lb[i].losses;
        lbe[i].rating = lb[i].rating;
    }

    Packet pkt; proto_pack_leaderboard(&pkt, lbe, count);
    send_packet(fd, &pkt);
    LOG_INFO("[LOBBY] Sent leaderboard (%d entries)", count);
}

/* ================================================================== */

int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;
    if (argc >= 2) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Invalid port: %s\n", argv[1]);
            return EXIT_FAILURE;
        }
    }

    /* Initialize SQLite database */
    if (db_init(DB_DEFAULT_PATH) < 0) {
        fprintf(stderr, "Failed to initialize database.\n");
        return EXIT_FAILURE;
    }

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) { perror("socket"); db_close(); return EXIT_FAILURE; }
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); db_close(); return EXIT_FAILURE;
    }
    if (listen(listen_fd, BACKLOG) < 0) {
        perror("listen"); db_close(); return EXIT_FAILURE;
    }

    LOG_INFO("[SERVER] Listening on port %d (multithreaded + reconnect)", port);
    LOG_INFO("[SERVER] Move timeout: %ds, Reconnect window: %ds",
             MOVE_TIMEOUT, RECONNECT_TIMEOUT);
    LOG_INFO("[SERVER] SQLite database: %s", DB_DEFAULT_PATH);

    LobbyClient lobby[MAX_LOBBY];
    GameRoom    rooms[MAX_ROOMS];
    lobby_init(lobby);
    rooms_init(rooms);

    while (1) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(listen_fd, &readfds);
        int maxfd = listen_fd;

        for (int i = 0; i < MAX_LOBBY; i++)
            if (lobby[i].fd >= 0) {
                FD_SET(lobby[i].fd, &readfds);
                if (lobby[i].fd > maxfd) maxfd = lobby[i].fd;
            }

        int activity = select(maxfd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0) {
            if (errno == EINTR) continue;
            perror("select"); break;
        }

        /* New connection */
        if (FD_ISSET(listen_fd, &readfds)) {
            struct sockaddr_in cli_addr;
            socklen_t cli_len = sizeof(cli_addr);
            int new_fd = accept(listen_fd, (struct sockaddr *)&cli_addr, &cli_len);
            if (new_fd >= 0) {
                int slot = lobby_add(lobby, new_fd);
                if (slot < 0) {
                    LOG_WARN("[LOBBY] Full, rejecting connection.");
                    close(new_fd);
                } else {
                    char ip[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &cli_addr.sin_addr, ip, sizeof(ip));
                    LOG_INFO("[LOBBY] Client %d from %s:%d", slot, ip, ntohs(cli_addr.sin_port));
                }
            }
        }

        /* Check lobby clients */
        for (int i = 0; i < MAX_LOBBY; i++) {
            if (lobby[i].fd < 0 || !FD_ISSET(lobby[i].fd, &readfds)) continue;

            Packet pkt;
            if (recv_packet(lobby[i].fd, &pkt) < 0) {
                LOG_INFO("[LOBBY] Client %d disconnected.", i);
                lobby_remove(lobby, i); continue;
            }

            switch (pkt.type) {
            case MSG_JOIN: {
                if (lobby[i].has_name) break;
                JoinData jd; proto_unpack_join(&pkt, &jd);
                strncpy(lobby[i].name, jd.name, NAME_LEN - 1);
                lobby[i].has_name = 1;
                LOG_INFO("[LOBBY] Client %d = '%s'", i, lobby[i].name);

                /* Register player in database */
                PlayerStats ps;
                if (db_get_or_create(jd.name, &ps) == 0) {
                    LOG_INFO("[LOBBY] Player '%s': rating=%d W=%d L=%d D=%d",
                             ps.name, ps.rating, ps.wins, ps.losses, ps.draws);
                }

                Packet wp; proto_pack_wait(&wp);
                send_packet(lobby[i].fd, &wp);

                int p1, p2;
                while (lobby_find_match(lobby, &p1, &p2)) {
                    LOG_INFO("[LOBBY] Pairing '%s' + '%s'", lobby[p1].name, lobby[p2].name);
                    int ri = room_create_and_start(rooms,
                        lobby[p1].fd, lobby[p1].name,
                        lobby[p2].fd, lobby[p2].name);
                    if (ri >= 0) {
                        lobby[p1].fd = -1; lobby[p1].has_name = 0;
                        lobby[p2].fd = -1; lobby[p2].has_name = 0;
                    } else break;
                }
                break;
            }

            case MSG_RECONNECT: {
                ReconnectData rd; proto_unpack_reconnect(&pkt, &rd);
                LOG_INFO("[LOBBY] Reconnect request with token %.8s...", rd.token);

                if (room_reconnect(rooms, rd.token, lobby[i].fd)) {
                    /* Success — fd is now owned by the room thread */
                    lobby[i].fd = -1; /* don't close it! */
                    lobby[i].has_name = 0;
                } else {
                    LOG_WARN("[LOBBY] Reconnect failed: token not found.");
                    Packet ep; proto_pack_error(&ep, "Reconnect failed: invalid or expired token.");
                    send_packet(lobby[i].fd, &ep);
                    lobby_remove(lobby, i);
                }
                break;
            }

            case MSG_STATS_REQ: {
                if (!lobby[i].has_name) {
                    Packet ep; proto_pack_error(&ep, "Send JOIN first.");
                    send_packet(lobby[i].fd, &ep); break;
                }
                handle_stats_req(lobby[i].fd, lobby[i].name);
                break;
            }

            case MSG_LB_REQ: {
                handle_lb_req(lobby[i].fd);
                break;
            }

            default:
                LOG_WARN("[LOBBY] Unexpected %s from client %d",
                         proto_type_name(pkt.type), i);
                break;
            }
        }
    }

    close(listen_fd);
    db_close();
    return EXIT_SUCCESS;
}
