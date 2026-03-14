# ♟️ Checkers in C

A terminal-based implementation of the classic **Checkers** (Draughts) board game, written entirely in C. Play against a friend on the same machine in a clean command-line interface.

---

## 📋 Table of Contents

- [About](#about)
- [Features](#features)
- [Game Rules](#game-rules)
- [Project Structure](#project-structure)
- [Getting Started](#getting-started)
  - [Prerequisites](#prerequisites)
  - [Compilation](#compilation)
  - [Running the Game](#running-the-game)
- [How to Play](#how-to-play)
- [Contributing](#contributing)
- [License](#license)

---

## About

This project is a fully functional Checkers game built in C and playable from the terminal. It follows standard American Checkers rules on an 8×8 board, supporting piece movement, captures (jumps), multi-jumps, and King promotion.

---

## ✨ Features

- 🎮 **Two-player local gameplay** — take turns on the same terminal
- ♟️ **Standard 8×8 Checkers board** with 12 pieces per player
- 👑 **King promotion** — pieces reaching the opponent's back row become Kings
- 🔁 **Capture (jump) mechanics** — mandatory captures and multi-jump chains
- 🖥️ **ASCII board rendering** — clean, readable board displayed after every move
- ⚡ **Lightweight** — no external libraries required, pure C

---

## 📜 Game Rules

1. The game is played on an **8×8 board** with pieces placed on dark squares only.
2. Each player starts with **12 pieces** on opposite sides of the board.
3. Pieces move **diagonally forward** to an adjacent empty square.
4. **Capturing** an opponent's piece is done by jumping over it diagonally — the landing square must be empty.
5. **Multi-jumps** are allowed: if after a capture another jump is available with the same piece, it must be taken.
6. When a piece reaches the **opponent's back row**, it becomes a **King** and can move both forward and backward diagonally.
7. A player **wins** when the opponent has no pieces left or no valid moves.

---

## 🗂️ Project Structure

```
Checkers-in-C/
└── Checkers/
    ├── checkers.c      # Main game logic and board management
    └── (other .c/.h files if present)
```

---

## 🚀 Getting Started

### Prerequisites

- A C compiler such as **GCC** or **Clang**
- A Unix/Linux/macOS terminal, or **MinGW** / **WSL** on Windows

### Compilation

Clone the repository and compile with GCC:

```bash
git clone https://github.com/0Shrihari0/Checkers-in-C.git
cd Checkers-in-C/Checkers
gcc -o checkers checkers.c
```

Or if there are multiple source files:

```bash
gcc -o checkers *.c
```

### Running the Game

After compilation, start the game with:

```bash
./checkers
```

On Windows (MinGW):

```bash
checkers.exe
```

---

## 🕹️ How to Play

1. The board is displayed in the terminal after each turn.
2. Players are prompted to enter their move using **row and column coordinates**.
3. Follow the on-screen instructions to select a piece and choose a destination.
4. Captures are performed automatically when a valid jump is made.
5. The game announces the **winner** when one player has no remaining pieces or moves.
