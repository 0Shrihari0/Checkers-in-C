CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11 -I./src
LDFLAGS =

# SQLite (macOS homebrew)
SQLITE_CFLAGS = $(shell pkg-config --cflags sqlite3 2>/dev/null)
SQLITE_LIBS   = $(shell pkg-config --libs sqlite3 2>/dev/null || echo "-lsqlite3")

# ---------- Local game (single-machine, 2 players) ----------
GAME_SRCS = src/main.c \
            src/game/board.c \
            src/game/moves.c \
            src/game/rules.c \
            src/ui/terminal.c

TARGET = checkers

# ---------- Multiplayer server ----------
SERVER_SRCS = server/server.c \
              server/room.c \
              server/db.c \
              server/ai.c \
              server/history.c \
              shared/protocol.c \
              src/game/board.c \
              src/game/moves.c \
              src/game/rules.c

SERVER_BIN = checkers_server

# ---------- Multiplayer client ----------
CLIENT_SRCS = client/client.c \
              shared/protocol.c

CLIENT_BIN = checkers_client

# ---------- Targets ----------
all: $(TARGET) $(SERVER_BIN) $(CLIENT_BIN)

$(TARGET): $(GAME_SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(GAME_SRCS) $(LDFLAGS)

$(SERVER_BIN): $(SERVER_SRCS)
	$(CC) $(CFLAGS) $(SQLITE_CFLAGS) -pthread -o $(SERVER_BIN) $(SERVER_SRCS) $(SQLITE_LIBS) -lm $(LDFLAGS)

$(CLIENT_BIN): $(CLIENT_SRCS)
	$(CC) $(CFLAGS) -o $(CLIENT_BIN) $(CLIENT_SRCS) $(LDFLAGS)

debug: CFLAGS += -g -fsanitize=address,undefined -fno-omit-frame-pointer
debug: all

clean:
	rm -f $(TARGET) $(SERVER_BIN) $(CLIENT_BIN)

.PHONY: all debug clean
