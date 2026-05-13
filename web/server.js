/*
 * server.js — Checkers multiplayer WebSocket server.
 *
 * Serves static files from the web/ directory and provides
 * WebSocket-based real-time multiplayer over LAN.
 *
 * Usage:
 *   cd web && npm install && node server.js
 *   Both players open http://<your-ip>:3000 in their browsers.
 */

const http = require('http');
const fs = require('fs');
const path = require('path');
const { WebSocketServer } = require('ws');
const os = require('os');

const PORT = 3000;
const BOARD_SIZE = 8;

/* ================================================================== */
/*  Embedded Game Engine (mirrors game.js logic)                       */
/* ================================================================== */

class CheckersGame {
    constructor() {
        this.board = [];
        this.currentTurn = 1;
        this.inMultiCapture = false;
        this.mcRow = -1;
        this.mcCol = -1;
        this.gameOver = false;
        this.winner = null;
        this.p1Score = 0;
        this.p2Score = 0;
        this.init();
    }

    init() {
        this.board = Array.from({ length: BOARD_SIZE }, () =>
            Array.from({ length: BOARD_SIZE }, () => null));

        /* P2 at rows 0-2 (top), P1 at rows 5-7 (bottom) */
        for (let r = 0; r < 3; r++)
            for (let c = 0; c < BOARD_SIZE; c++)
                if ((r + c) % 2 === 1)
                    this.board[r][c] = { player: 2, king: false };

        for (let r = 5; r < 8; r++)
            for (let c = 0; c < BOARD_SIZE; c++)
                if ((r + c) % 2 === 1)
                    this.board[r][c] = { player: 1, king: false };

        this.currentTurn = 1;
        this.inMultiCapture = false;
        this.gameOver = false;
        this.winner = null;
        this.calcScores();
    }

    fwd(player) { return player === 1 ? -1 : 1; }
    backRow(player) { return player === 1 ? 0 : 7; }
    inBounds(r, c) { return r >= 0 && r < BOARD_SIZE && c >= 0 && c < BOARD_SIZE; }

    isValidDir(dr, player, isKing) {
        return isKing || dr === this.fwd(player);
    }

    isCaptureInDir(r, c, dr, dc, player) {
        const nr = r + dr, nc = c + dc;
        const jr = r + 2 * dr, jc = c + 2 * dc;
        if (!this.inBounds(nr, nc) || !this.inBounds(jr, jc)) return false;
        const mid = this.board[nr][nc];
        return mid && mid.player !== player && this.board[jr][jc] === null;
    }

    canCaptureFrom(r, c) {
        const p = this.board[r][c];
        if (!p) return false;
        const DIRS = [[-1,-1],[-1,1],[1,-1],[1,1]];
        for (const [dr, dc] of DIRS) {
            if (!this.isValidDir(dr, p.player, p.king)) continue;
            if (this.isCaptureInDir(r, c, dr, dc, p.player)) return true;
        }
        return false;
    }

    playerHasCapture(player) {
        for (let r = 0; r < BOARD_SIZE; r++)
            for (let c = 0; c < BOARD_SIZE; c++) {
                const p = this.board[r][c];
                if (p && p.player === player && this.canCaptureFrom(r, c)) return true;
            }
        return false;
    }

    getValidMoves(player) {
        const moves = [];
        const DIRS = [[-1,-1],[-1,1],[1,-1],[1,1]];
        const hasCapture = this.inMultiCapture
            ? this.canCaptureFrom(this.mcRow, this.mcCol)
            : this.playerHasCapture(player);

        const rS = this.inMultiCapture ? this.mcRow : 0;
        const rE = this.inMultiCapture ? this.mcRow + 1 : BOARD_SIZE;
        const cS = this.inMultiCapture ? this.mcCol : 0;
        const cE = this.inMultiCapture ? this.mcCol + 1 : BOARD_SIZE;

        for (let r = rS; r < rE; r++) {
            for (let c = cS; c < cE; c++) {
                const p = this.board[r][c];
                if (!p || p.player !== player) continue;
                for (const [dr, dc] of DIRS) {
                    if (!this.isValidDir(dr, player, p.king)) continue;
                    if (hasCapture) {
                        if (this.isCaptureInDir(r, c, dr, dc, player))
                            moves.push({ fromR: r, fromC: c, toR: r+2*dr, toC: c+2*dc, isCapture: true });
                    } else {
                        const nr = r+dr, nc = c+dc;
                        if (this.inBounds(nr, nc) && !this.board[nr][nc])
                            moves.push({ fromR: r, fromC: c, toR: nr, toC: nc, isCapture: false });
                    }
                }
            }
        }
        return moves;
    }

    executeMove(fromR, fromC, toR, toC) {
        const valid = this.getValidMoves(this.currentTurn);
        const move = valid.find(m => m.fromR === fromR && m.fromC === fromC &&
                                     m.toR === toR && m.toC === toC);
        if (!move) return { success: false, message: 'Invalid move!' };

        const piece = this.board[fromR][fromC];
        this.board[fromR][fromC] = null;
        this.board[toR][toC] = { ...piece };

        if (move.isCapture) {
            this.board[(fromR+toR)/2][(fromC+toC)/2] = null;
        }

        let promoted = false;
        if (!piece.king && toR === this.backRow(piece.player)) {
            this.board[toR][toC].king = true;
            promoted = true;
        }

        /* Multi-capture check */
        if (move.isCapture && this.canCaptureFrom(toR, toC)) {
            this.inMultiCapture = true;
            this.mcRow = toR;
            this.mcCol = toC;
            this.calcScores();
            return { success: true, multiCapture: true, promoted,
                     message: `Multi-capture! Continue from ${this.posLabel(toR, toC)}.` };
        }

        this.inMultiCapture = false;
        this.mcRow = -1;
        this.mcCol = -1;
        this.calcScores();

        /* Game over checks */
        const nextPlayer = this.currentTurn === 1 ? 2 : 1;

        if (this.countPieces(1) === 0 || this.countPieces(2) === 0) {
            this.gameOver = true;
            this.winner = this.countPieces(1) === 0 ? 2 : 1;
            return { success: true, gameOver: true, promoted,
                     message: `Player ${this.winner} wins!` };
        }

        if (this.getValidMoves(nextPlayer).length === 0) {
            this.gameOver = true;
            this.winner = this.currentTurn;
            return { success: true, gameOver: true, promoted,
                     message: `Player ${nextPlayer} has no moves! Player ${this.currentTurn} wins!` };
        }

        this.currentTurn = nextPlayer;
        return { success: true, promoted, message: `Player ${this.currentTurn}'s turn.` };
    }

    calcScores() {
        this.p1Score = 0; this.p2Score = 0;
        for (let r = 0; r < BOARD_SIZE; r++)
            for (let c = 0; c < BOARD_SIZE; c++) {
                const p = this.board[r][c];
                if (!p) continue;
                const pts = p.king ? 10 : 5;
                if (p.player === 1) this.p1Score += pts; else this.p2Score += pts;
            }
    }

    countPieces(player) {
        let n = 0;
        for (let r = 0; r < BOARD_SIZE; r++)
            for (let c = 0; c < BOARD_SIZE; c++)
                if (this.board[r][c] && this.board[r][c].player === player) n++;
        return n;
    }

    posLabel(r, c) { return String.fromCharCode(65 + c) + (r + 1); }

    getState() {
        return {
            board: this.board,
            currentTurn: this.currentTurn,
            p1Score: this.p1Score,
            p2Score: this.p2Score,
            inMultiCapture: this.inMultiCapture,
            mcRow: this.mcRow,
            mcCol: this.mcCol,
            p1Pieces: this.countPieces(1),
            p2Pieces: this.countPieces(2),
        };
    }
}

/* ================================================================== */
/*  Static File Server                                                 */
/* ================================================================== */

const MIME = {
    '.html': 'text/html',
    '.css': 'text/css',
    '.js': 'application/javascript',
    '.json': 'application/json',
    '.png': 'image/png',
    '.svg': 'image/svg+xml',
    '.ico': 'image/x-icon',
};

const webDir = __dirname;

const httpServer = http.createServer((req, res) => {
    let filePath = req.url === '/' ? '/index.html' : req.url;
    filePath = path.join(webDir, filePath);

    /* Security: prevent directory traversal */
    if (!filePath.startsWith(webDir)) {
        res.writeHead(403); res.end('Forbidden'); return;
    }

    const ext = path.extname(filePath);
    const mime = MIME[ext] || 'application/octet-stream';

    fs.readFile(filePath, (err, data) => {
        if (err) {
            res.writeHead(404); res.end('Not found'); return;
        }
        res.writeHead(200, { 'Content-Type': mime });
        res.end(data);
    });
});

/* ================================================================== */
/*  WebSocket Game Server                                              */
/* ================================================================== */

const wss = new WebSocketServer({ server: httpServer });

/* Matchmaking queue */
let waitingPlayer = null;

/* Active game rooms */
const rooms = new Map();
let nextRoomId = 1;

function sendJSON(ws, data) {
    if (ws.readyState === ws.OPEN) {
        ws.send(JSON.stringify(data));
    }
}

function broadcastBoard(room, message) {
    const state = room.game.getState();
    const payload = { type: 'board', ...state, message };
    sendJSON(room.p1.ws, payload);
    sendJSON(room.p2.ws, payload);
}

wss.on('connection', (ws) => {
    console.log(`[WS] Client connected (total: ${wss.clients.size})`);

    ws.playerData = { name: '', room: null, playerNum: 0 };

    ws.on('message', (raw) => {
        let msg;
        try { msg = JSON.parse(raw); } catch { return; }

        switch (msg.type) {
            case 'join': {
                const name = (msg.name || 'Player').substring(0, 20);
                ws.playerData.name = name;

                if (waitingPlayer && waitingPlayer.ws.readyState === ws.OPEN) {
                    /* Match found! Create a room */
                    const roomId = nextRoomId++;
                    const game = new CheckersGame();
                    const room = {
                        id: roomId,
                        game,
                        p1: { ws: waitingPlayer.ws, name: waitingPlayer.name },
                        p2: { ws, name },
                    };
                    rooms.set(roomId, room);

                    waitingPlayer.ws.playerData.room = roomId;
                    waitingPlayer.ws.playerData.playerNum = 1;
                    ws.playerData.room = roomId;
                    ws.playerData.playerNum = 2;

                    console.log(`[ROOM ${roomId}] ${room.p1.name} vs ${room.p2.name}`);

                    /* Send start to both */
                    sendJSON(room.p1.ws, {
                        type: 'start', playerNum: 1,
                        p1Name: room.p1.name, p2Name: room.p2.name
                    });
                    sendJSON(room.p2.ws, {
                        type: 'start', playerNum: 2,
                        p1Name: room.p1.name, p2Name: room.p2.name
                    });

                    /* Send initial board */
                    broadcastBoard(room, `${room.p1.name}'s turn.`);
                    waitingPlayer = null;
                } else {
                    /* Queue this player */
                    waitingPlayer = { ws, name };
                    sendJSON(ws, { type: 'waiting', message: 'Waiting for an opponent...' });
                    console.log(`[LOBBY] ${name} waiting for opponent`);
                }
                break;
            }

            case 'move': {
                const roomId = ws.playerData.room;
                const room = rooms.get(roomId);
                if (!room) { sendJSON(ws, { type: 'error', message: 'Not in a game.' }); break; }

                const playerNum = ws.playerData.playerNum;
                if (room.game.currentTurn !== playerNum) {
                    sendJSON(ws, { type: 'error', message: "It's not your turn!" });
                    break;
                }

                const result = room.game.executeMove(msg.fromR, msg.fromC, msg.toR, msg.toC);
                if (!result.success) {
                    sendJSON(ws, { type: 'error', message: result.message });
                    break;
                }

                if (result.gameOver) {
                    broadcastBoard(room, result.message);
                    sendJSON(room.p1.ws, {
                        type: 'win', winner: room.game.winner,
                        p1Score: room.game.p1Score, p2Score: room.game.p2Score,
                        message: result.message
                    });
                    sendJSON(room.p2.ws, {
                        type: 'win', winner: room.game.winner,
                        p1Score: room.game.p1Score, p2Score: room.game.p2Score,
                        message: result.message
                    });
                    console.log(`[ROOM ${roomId}] Game over: ${result.message}`);
                    rooms.delete(roomId);
                    break;
                }

                broadcastBoard(room, result.message);
                break;
            }

            default:
                break;
        }
    });

    ws.on('close', () => {
        console.log(`[WS] Client disconnected`);

        /* Remove from waiting queue */
        if (waitingPlayer && waitingPlayer.ws === ws) {
            waitingPlayer = null;
        }

        /* Handle in-game disconnect */
        const roomId = ws.playerData.room;
        if (roomId) {
            const room = rooms.get(roomId);
            if (room) {
                const other = (room.p1.ws === ws) ? room.p2.ws : room.p1.ws;
                const otherNum = (room.p1.ws === ws) ? 2 : 1;
                sendJSON(other, {
                    type: 'opponent_disconnected',
                    message: 'Your opponent disconnected. You win!'
                });
                console.log(`[ROOM ${roomId}] Player disconnected, Player ${otherNum} wins`);
                rooms.delete(roomId);
            }
        }
    });
});

/* ================================================================== */
/*  Start Server                                                       */
/* ================================================================== */

function getLocalIP() {
    const nets = os.networkInterfaces();
    for (const name of Object.keys(nets)) {
        for (const net of nets[name]) {
            if (net.family === 'IPv4' && !net.internal) {
                return net.address;
            }
        }
    }
    return '127.0.0.1';
}

httpServer.listen(PORT, '0.0.0.0', () => {
    const ip = getLocalIP();
    console.log('');
    console.log('  ♟  Checkers Multiplayer Server');
    console.log('  ─────────────────────────────');
    console.log(`  Local:   http://localhost:${PORT}`);
    console.log(`  Network: http://${ip}:${PORT}`);
    console.log('');
    console.log('  Share the Network URL with Player 2!');
    console.log('  Both players click "Online Multiplayer" to start.');
    console.log('');
});
