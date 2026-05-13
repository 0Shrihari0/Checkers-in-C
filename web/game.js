/*
 * game.js — Core Checkers game engine.
 *
 * Implements the same rules as the C codebase:
 *   - 8×8 board, pieces on dark squares only
 *   - P1 (bottom, moves up), P2 (top, moves down)
 *   - Forced captures, multi-capture chains
 *   - King promotion at back row
 *   - Game over when opponent has no pieces or no valid moves
 */

const BOARD_SIZE = 8;

/* Piece representation: { player: 1|2, king: false|true } or null */

const Game = {
    board: [],
    currentTurn: 1,
    p1Score: 0,
    p2Score: 0,
    gameOver: false,
    winner: null,       // 1, 2, or 'draw'
    inMultiCapture: false,
    multiCaptureRow: -1,
    multiCaptureCol: -1,
    moveHistory: [],
    mode: null,         // 'pvp' or 'ai'
    aiDifficulty: 1,    // 1=easy, 2=medium, 3=hard

    /* Direction offsets: [dr, dc] for the 4 diagonals */
    DIRS: [[-1, -1], [-1, 1], [1, -1], [1, 1]],

    /* Forward direction: P1 moves up (-1), P2 moves down (+1) */
    forwardDir(player) {
        return player === 1 ? -1 : 1;
    },

    /* Back row (promotion row): P1 promotes at row 0, P2 at row 7 */
    backRow(player) {
        return player === 1 ? 0 : 7;
    },

    /* ---- Board Setup ---- */

    init() {
        this.board = [];
        for (let i = 0; i < BOARD_SIZE; i++) {
            this.board[i] = [];
            for (let j = 0; j < BOARD_SIZE; j++) {
                this.board[i][j] = null;
            }
        }

        /* P2 pieces at rows 0-2 (top) — same layout as C code */
        for (let r = 0; r < 3; r++) {
            for (let c = 0; c < BOARD_SIZE; c++) {
                if ((r + c) % 2 === 1) {
                    this.board[r][c] = { player: 2, king: false };
                }
            }
        }

        /* P1 pieces at rows 5-7 (bottom) */
        for (let r = 5; r < 8; r++) {
            for (let c = 0; c < BOARD_SIZE; c++) {
                if ((r + c) % 2 === 1) {
                    this.board[r][c] = { player: 1, king: false };
                }
            }
        }

        this.currentTurn = 1;
        this.p1Score = 0;
        this.p2Score = 0;
        this.gameOver = false;
        this.winner = null;
        this.inMultiCapture = false;
        this.multiCaptureRow = -1;
        this.multiCaptureCol = -1;
        this.moveHistory = [];
    },

    /* ---- Board Queries ---- */

    inBounds(r, c) {
        return r >= 0 && r < BOARD_SIZE && c >= 0 && c < BOARD_SIZE;
    },

    getPiece(r, c) {
        if (!this.inBounds(r, c)) return null;
        return this.board[r][c];
    },

    isPlayerPiece(r, c, player) {
        const p = this.getPiece(r, c);
        return p !== null && p.player === player;
    },

    isOpponentPiece(r, c, player) {
        const p = this.getPiece(r, c);
        return p !== null && p.player !== player;
    },

    isEmpty(r, c) {
        return this.inBounds(r, c) && this.board[r][c] === null;
    },

    /* ---- Direction Validation ---- */

    isValidDirection(dr, player, isKing) {
        if (isKing) return true;
        return dr === this.forwardDir(player);
    },

    /* ---- Capture Detection ---- */

    /* Check if a specific direction from (r,c) has a valid capture */
    isCaptureInDir(r, c, dr, dc, player) {
        const nr = r + dr, nc = c + dc;
        const jr = r + 2 * dr, jc = c + 2 * dc;
        if (!this.inBounds(nr, nc) || !this.inBounds(jr, jc)) return false;
        return this.isOpponentPiece(nr, nc, player) && this.isEmpty(jr, jc);
    },

    /* Check if piece at (r,c) can capture in any valid direction */
    canCaptureFrom(r, c) {
        const piece = this.getPiece(r, c);
        if (!piece) return false;
        const player = piece.player;

        for (const [dr, dc] of this.DIRS) {
            if (!this.isValidDirection(dr, player, piece.king)) continue;
            if (this.isCaptureInDir(r, c, dr, dc, player)) return true;
        }
        return false;
    },

    /* Check if ANY piece of a player has a capture available */
    playerHasCapture(player) {
        for (let r = 0; r < BOARD_SIZE; r++) {
            for (let c = 0; c < BOARD_SIZE; c++) {
                if (this.isPlayerPiece(r, c, player) && this.canCaptureFrom(r, c))
                    return true;
            }
        }
        return false;
    },

    /* ---- Valid Move Generation ---- */

    /* Get all valid moves for the current player.
       Returns array of { fromR, fromC, toR, toC, isCapture } */
    getValidMoves() {
        const player = this.currentTurn;
        const moves = [];
        const hasCapture = this.inMultiCapture
            ? this.canCaptureFrom(this.multiCaptureRow, this.multiCaptureCol)
            : this.playerHasCapture(player);

        const rStart = this.inMultiCapture ? this.multiCaptureRow : 0;
        const rEnd = this.inMultiCapture ? this.multiCaptureRow + 1 : BOARD_SIZE;
        const cStart = this.inMultiCapture ? this.multiCaptureCol : 0;
        const cEnd = this.inMultiCapture ? this.multiCaptureCol + 1 : BOARD_SIZE;

        for (let r = rStart; r < rEnd; r++) {
            for (let c = cStart; c < cEnd; c++) {
                if (!this.isPlayerPiece(r, c, player)) continue;
                const piece = this.getPiece(r, c);

                for (const [dr, dc] of this.DIRS) {
                    if (!this.isValidDirection(dr, player, piece.king)) continue;

                    if (hasCapture) {
                        /* Only capture moves when captures are available */
                        if (this.isCaptureInDir(r, c, dr, dc, player)) {
                            moves.push({
                                fromR: r, fromC: c,
                                toR: r + 2 * dr, toC: c + 2 * dc,
                                isCapture: true
                            });
                        }
                    } else {
                        /* Simple moves */
                        const nr = r + dr, nc = c + dc;
                        if (this.isEmpty(nr, nc)) {
                            moves.push({
                                fromR: r, fromC: c,
                                toR: nr, toC: nc,
                                isCapture: false
                            });
                        }
                    }
                }
            }
        }
        return moves;
    },

    /* Get valid moves from a specific piece */
    getMovesFrom(r, c) {
        return this.getValidMoves().filter(m => m.fromR === r && m.fromC === c);
    },

    /* Check if player has any valid move */
    hasValidMove(player) {
        const saved = this.currentTurn;
        this.currentTurn = player;
        const moves = this.getValidMoves();
        this.currentTurn = saved;
        return moves.length > 0;
    },

    /* ---- Move Execution ---- */

    /* Execute a move. Returns { success, capture, message } */
    executeMove(fromR, fromC, toR, toC) {
        const validMoves = this.getValidMoves();
        const move = validMoves.find(m =>
            m.fromR === fromR && m.fromC === fromC &&
            m.toR === toR && m.toC === toC);

        if (!move) {
            return { success: false, capture: false, message: 'Invalid move!' };
        }

        const piece = this.board[fromR][fromC];
        this.board[fromR][fromC] = null;
        this.board[toR][toC] = piece;

        let captured = false;
        if (move.isCapture) {
            const midR = (fromR + toR) / 2;
            const midC = (fromC + toC) / 2;
            this.board[midR][midC] = null;
            captured = true;
        }

        /* King promotion */
        let promoted = false;
        if (!piece.king && toR === this.backRow(piece.player)) {
            piece.king = true;
            promoted = true;
        }

        /* Record move */
        this.moveHistory.push({
            player: this.currentTurn,
            fromR, fromC, toR, toC,
            captured, promoted
        });

        /* Check for multi-capture */
        if (captured && this.canCaptureFrom(toR, toC)) {
            this.inMultiCapture = true;
            this.multiCaptureRow = toR;
            this.multiCaptureCol = toC;
            this.calcScores();
            return {
                success: true,
                capture: true,
                multiCapture: true,
                promoted,
                message: `Multi-capture! Continue jumping from ${this.posLabel(toR, toC)}.`
            };
        }

        /* Turn complete */
        this.inMultiCapture = false;
        this.multiCaptureRow = -1;
        this.multiCaptureCol = -1;
        this.calcScores();

        /* Check game over */
        const nextPlayer = this.currentTurn === 1 ? 2 : 1;

        if (this.isGameOver()) {
            this.gameOver = true;
            if (this.p1Score > this.p2Score) this.winner = 1;
            else if (this.p2Score > this.p1Score) this.winner = 2;
            else this.winner = 'draw';
            return { success: true, capture: captured, promoted, gameOver: true,
                     message: this.getWinMessage() };
        }

        if (!this.hasValidMove(nextPlayer)) {
            this.gameOver = true;
            this.winner = this.currentTurn;
            return { success: true, capture: captured, promoted, gameOver: true,
                     message: `Player ${nextPlayer} has no valid moves! Player ${this.currentTurn} wins!` };
        }

        this.currentTurn = nextPlayer;
        return {
            success: true,
            capture: captured,
            promoted,
            multiCapture: false,
            message: `Player ${this.currentTurn}'s turn.`
        };
    },

    /* ---- Scoring ---- */

    calcScores() {
        this.p1Score = 0;
        this.p2Score = 0;
        for (let r = 0; r < BOARD_SIZE; r++) {
            for (let c = 0; c < BOARD_SIZE; c++) {
                const p = this.board[r][c];
                if (!p) continue;
                const pts = p.king ? 10 : 5;
                if (p.player === 1) this.p1Score += pts;
                else this.p2Score += pts;
            }
        }
    },

    countPieces(player) {
        let count = 0;
        for (let r = 0; r < BOARD_SIZE; r++)
            for (let c = 0; c < BOARD_SIZE; c++)
                if (this.isPlayerPiece(r, c, player)) count++;
        return count;
    },

    isGameOver() {
        return this.countPieces(1) === 0 || this.countPieces(2) === 0;
    },

    getWinMessage() {
        if (this.winner === 'draw') return "It's a draw!";
        return `Player ${this.winner} wins!`;
    },

    /* ---- Utilities ---- */

    posLabel(r, c) {
        return String.fromCharCode(65 + c) + (r + 1);
    },

    /* Deep clone the board for AI simulation */
    cloneBoard() {
        return this.board.map(row =>
            row.map(cell => cell ? { ...cell } : null)
        );
    },

    /* Restore board from clone */
    setBoard(boardClone) {
        this.board = boardClone;
    }
};
