# Checkers in C

A full-featured Checkers (Draughts) game with a terminal client, multiplayer server, AI opponent, and a web-based interface — all written in C and JavaScript.

---

## Features

- **Local game** — two players on the same terminal
- **Multiplayer server** — TCP-based server with game rooms, reconnection support, and ELO matchmaking
- **AI opponent** — minimax with alpha-beta pruning (Easy / Medium / Hard)
- **Web interface** — play in the browser with local, AI, or online multiplayer modes
- **Online multiplayer** — play with a friend over WiFi/LAN via WebSocket
- **Game history** — move replay and persistent stats via SQLite
- Standard 8×8 board, forced captures, multi-jump chains, king promotion

---

## Project Structure

```
Checkers/
├── src/                   # Local terminal game
│   ├── main.c             # Game loop
│   ├── game/
│   │   ├── board.c/h      # Board state
│   │   ├── moves.c/h      # Move execution
│   │   └── rules.c/h      # Game rules, validation
│   └── ui/
│       └── terminal.c/h   # Terminal rendering, input
│
├── server/                # Multiplayer game server
│   ├── server.c           # Main server loop, lobby
│   ├── room.c/h           # Game rooms, threading
│   ├── db.c/h             # SQLite player stats, ELO
│   ├── ai.c/h             # AI move selection
│   ├── history.c/h        # Game history logging
│   └── log.h              # Server logging
│
├── client/                # Terminal multiplayer client
│   └── client.c           # Network client
│
├── shared/                # Shared between server and client
│   └── protocol.c/h       # Packet serialization, transport
│
├── web/                   # Web-based interface
│   ├── index.html         # Main page
│   ├── style.css          # UI styling
│   ├── game.js            # Game engine
│   ├── ai.js              # AI opponent
│   ├── ui.js              # DOM rendering, interaction
│   ├── multiplayer.js     # WebSocket client
│   ├── server.js          # Node.js WebSocket server
│   └── package.json       # Dependencies
│
└── Makefile               # Build targets
```

---

## Getting Started

### Prerequisites

- **C compiler** — GCC or Clang
- **SQLite3** — for the multiplayer server (`brew install sqlite3` on macOS)
- **Node.js** — for the web interface (optional)

### Build

```bash
git clone https://github.com/0Shrihari0/Checkers-in-C.git
cd Checkers-in-C
make all
```

This builds three binaries:

| Binary | Description |
|---|---|
| `checkers` | Local 2-player terminal game |
| `checkers_server` | Multiplayer TCP server |
| `checkers_client` | Terminal multiplayer client |

---

## How to Play

### 1. Local Game (terminal)

```bash
./checkers
```

Two players take turns entering moves in `A1` to `H8` notation.

### 2. Multiplayer (terminal)

Start the server in one terminal:
```bash
./checkers_server          # default port 8080
```

Connect two clients from separate terminals:
```bash
./checkers_client 127.0.0.1 8080    # Player 1
./checkers_client 127.0.0.1 8080    # Player 2
```

The server pairs players automatically.

### 3. Web Interface (browser)

Play in the browser with local, AI, or online multiplayer modes.

**For local play / AI** — just open the file directly:
```bash
open web/index.html
```

**For online multiplayer over WiFi:**
```bash
cd web
npm install        # one-time setup
node server.js     # starts the server
```

The server prints your local network URL:
```
  ♟  Checkers Multiplayer Server
  Local:   http://localhost:3000
  Network: http://192.168.x.x:3000

  Share the Network URL with Player 2!
```

Both players open the URL in their browsers and click **Multiplayer**.

---

## Game Rules

1. Played on an **8×8 board** — pieces occupy dark squares only
2. Each player starts with **12 pieces**
3. Pieces move **diagonally forward** one square
4. **Captures** are mandatory — jump over an opponent's piece to an empty square
5. **Multi-jumps** — if another capture is available after a jump, it must be taken
6. **King promotion** — pieces reaching the opponent's back row become Kings and can move in any diagonal direction
7. A player **wins** when the opponent has no pieces or no valid moves

---

## Contributing

1. Fork the repository
2. Create a branch (`git checkout -b feature/your-feature`)
3. Commit your changes (`git commit -m 'Add your feature'`)
4. Push (`git push origin feature/your-feature`)
5. Open a Pull Request

---

## License

This project is open source. Feel free to use, modify, and distribute it.

---
