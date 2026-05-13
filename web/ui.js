/*
 * ui.js — DOM rendering, event handling, and game flow.
 *
 * Handles:
 *   - Board rendering with piece animations
 *   - Click-to-select, click-to-move interaction
 *   - Valid move highlighting
 *   - Score display, status messages, move history
 *   - Mode selection (PvP / AI)
 *   - AI move execution with delay for natural feel
 */

const UI = {
    selectedCell: null,
    validMoves: [],
    boardEl: null,
    animating: false,

    /* ---- Initialization ---- */

    init() {
        this.boardEl = document.getElementById('board');
        this.bindMenuEvents();
    },

    bindMenuEvents() {
        document.getElementById('btn-pvp').addEventListener('click', () => {
            this.startGame('pvp');
        });
        document.getElementById('btn-ai-easy').addEventListener('click', () => {
            this.startGame('ai', 1);
        });
        document.getElementById('btn-ai-medium').addEventListener('click', () => {
            this.startGame('ai', 2);
        });
        document.getElementById('btn-ai-hard').addEventListener('click', () => {
            this.startGame('ai', 3);
        });
        document.getElementById('btn-online').addEventListener('click', () => {
            this.startOnlineGame();
        });
        document.getElementById('btn-new-game').addEventListener('click', () => {
            this.showMenu();
        });
        document.getElementById('btn-restart').addEventListener('click', () => {
            if (Game.mode === 'online') {
                this.showMenu();
            } else {
                this.startGame(Game.mode, Game.aiDifficulty);
            }
        });
    },

    showMenu() {
        /* Disconnect from online game if active */
        if (Game.mode === 'online') {
            Multiplayer.disconnect();
        }
        document.getElementById('menu-screen').classList.remove('hidden');
        document.getElementById('game-screen').classList.add('hidden');
        document.getElementById('game-over-overlay').classList.add('hidden');
    },

    startGame(mode, difficulty) {
        Game.mode = mode;
        Game.aiDifficulty = difficulty || 1;
        Game.init();
        Game.calcScores();

        document.getElementById('menu-screen').classList.add('hidden');
        document.getElementById('game-screen').classList.remove('hidden');
        document.getElementById('game-over-overlay').classList.add('hidden');

        /* Set player labels */
        document.getElementById('p1-label').textContent = 'Player 1';
        document.getElementById('p2-label').textContent =
            mode === 'ai' ? `AI (${['', 'Easy', 'Medium', 'Hard'][difficulty]})` : 'Player 2';

        this.selectedCell = null;
        this.validMoves = [];
        this.renderBoard();
        this.updateScores();
        this.updateStatus(`Player 1's turn`);
        this.clearHistory();
        this.highlightActivePlayer();
    },

    startOnlineGame() {
        /* Prompt for player name */
        const name = prompt('Enter your name:', 'Player') || 'Player';

        Game.mode = 'online';
        Game.init();
        Game.calcScores();

        document.getElementById('menu-screen').classList.add('hidden');
        document.getElementById('game-screen').classList.remove('hidden');
        document.getElementById('game-over-overlay').classList.add('hidden');

        document.getElementById('p1-label').textContent = 'Player 1';
        document.getElementById('p2-label').textContent = 'Player 2';

        this.selectedCell = null;
        this.validMoves = [];
        this.renderBoard();
        this.updateScores();
        this.updateStatus('Connecting to server...');
        this.clearHistory();

        /* Connect to WebSocket server */
        Multiplayer.connect(name);
    },

    /* ---- Board Rendering ---- */

    renderBoard() {
        this.boardEl.innerHTML = '';

        for (let r = 0; r < BOARD_SIZE; r++) {
            for (let c = 0; c < BOARD_SIZE; c++) {
                const cell = document.createElement('div');
                const isDark = (r + c) % 2 === 1;
                cell.className = `cell ${isDark ? 'dark' : 'light'}`;
                cell.dataset.row = r;
                cell.dataset.col = c;

                /* Highlight selected cell */
                if (this.selectedCell &&
                    this.selectedCell.r === r && this.selectedCell.c === c) {
                    cell.classList.add('selected');
                }

                /* Highlight valid move destinations */
                const isValidDest = this.validMoves.some(m => m.toR === r && m.toC === c);
                if (isValidDest) {
                    const move = this.validMoves.find(m => m.toR === r && m.toC === c);
                    cell.classList.add('valid-move');
                    if (move.isCapture) cell.classList.add('capture-move');
                }

                /* Multi-capture highlight */
                if (Game.inMultiCapture &&
                    r === Game.multiCaptureRow && c === Game.multiCaptureCol) {
                    cell.classList.add('multi-capture');
                }

                /* Add piece */
                const piece = Game.getPiece(r, c);
                if (piece) {
                    const pieceEl = document.createElement('div');
                    pieceEl.className = `piece player${piece.player}`;
                    if (piece.king) pieceEl.classList.add('king');

                    /* Add king crown indicator */
                    if (piece.king) {
                        const crown = document.createElement('span');
                        crown.className = 'crown';
                        crown.textContent = '♛';
                        pieceEl.appendChild(crown);
                    }

                    cell.appendChild(pieceEl);
                }

                /* Valid move dot indicator */
                if (isValidDest && !piece) {
                    const dot = document.createElement('div');
                    dot.className = 'move-dot';
                    cell.appendChild(dot);
                }

                /* Click handler */
                if (isDark) {
                    cell.addEventListener('click', () => this.handleCellClick(r, c));
                }

                this.boardEl.appendChild(cell);
            }
        }

        /* Add column labels */
        const colLabels = document.createElement('div');
        colLabels.className = 'col-labels';
        for (let c = 0; c < BOARD_SIZE; c++) {
            const label = document.createElement('span');
            label.textContent = String.fromCharCode(65 + c);
            colLabels.appendChild(label);
        }
        this.boardEl.parentElement.appendChild(colLabels);

        /* Remove duplicate col-labels */
        const existing = this.boardEl.parentElement.querySelectorAll('.col-labels');
        if (existing.length > 1) {
            for (let i = 0; i < existing.length - 1; i++) existing[i].remove();
        }
    },

    /* ---- Interaction ---- */

    handleCellClick(r, c) {
        if (Game.gameOver || this.animating) return;

        /* If AI's turn, ignore clicks */
        if (Game.mode === 'ai' && Game.currentTurn === 2) return;

        /* In online mode, only allow moves on your turn */
        if (Game.mode === 'online' && Game.currentTurn !== Multiplayer.playerNum) return;

        const piece = Game.getPiece(r, c);

        /* If clicking a valid move destination, execute the move */
        const targetMove = this.validMoves.find(m => m.toR === r && m.toC === c);
        if (targetMove) {
            this.executePlayerMove(targetMove);
            return;
        }

        /* If in multi-capture, can only click the multi-capture piece */
        if (Game.inMultiCapture) {
            if (r === Game.multiCaptureRow && c === Game.multiCaptureCol) {
                this.selectPiece(r, c);
            }
            return;
        }

        /* Select own piece */
        if (piece && piece.player === Game.currentTurn) {
            this.selectPiece(r, c);
        } else {
            /* Deselect */
            this.selectedCell = null;
            this.validMoves = [];
            this.renderBoard();
        }
    },

    selectPiece(r, c) {
        this.selectedCell = { r, c };
        this.validMoves = Game.getMovesFrom(r, c);
        this.renderBoard();

        if (this.validMoves.length === 0) {
            this.flashStatus('This piece has no valid moves!', 'warning');
        }
    },

    executePlayerMove(move) {
        /* In online mode, send the move to the server instead of executing locally */
        if (Game.mode === 'online') {
            Multiplayer.sendMove(move.fromR, move.fromC, move.toR, move.toC);
            this.selectedCell = null;
            this.validMoves = [];
            this.renderBoard();
            this.updateStatus('Sending move...');
            return;
        }

        const result = Game.executeMove(move.fromR, move.fromC, move.toR, move.toC);
        if (!result.success) {
            this.flashStatus(result.message, 'error');
            return;
        }

        this.addHistory(move, result);

        if (result.promoted) {
            this.flashStatus(`King promotion at ${Game.posLabel(move.toR, move.toC)}! ♛`, 'success');
        }

        if (result.gameOver) {
            this.selectedCell = null;
            this.validMoves = [];
            this.renderBoard();
            this.updateScores();
            this.showGameOver(result.message);
            return;
        }

        if (result.multiCapture) {
            /* Auto-select the multi-capture piece */
            this.selectedCell = { r: Game.multiCaptureRow, c: Game.multiCaptureCol };
            this.validMoves = Game.getMovesFrom(Game.multiCaptureRow, Game.multiCaptureCol);
            this.renderBoard();
            this.updateScores();
            this.updateStatus(result.message);
            return;
        }

        /* Normal turn end */
        this.selectedCell = null;
        this.validMoves = [];
        this.renderBoard();
        this.updateScores();
        this.highlightActivePlayer();

        if (Game.mode === 'ai' && Game.currentTurn === 2) {
            this.updateStatus('AI is thinking...');
            this.scheduleAIMove();
        } else {
            this.updateStatus(`Player ${Game.currentTurn}'s turn`);
        }
    },

    /* ---- AI Move ---- */

    scheduleAIMove() {
        this.animating = true;
        setTimeout(() => {
            this.doAIMove();
        }, 600);
    },

    doAIMove() {
        if (Game.gameOver) { this.animating = false; return; }

        const move = AI.chooseMove(Game.aiDifficulty);
        if (!move) {
            Game.gameOver = true;
            Game.winner = 1;
            this.renderBoard();
            this.showGameOver('AI has no valid moves! Player 1 wins!');
            this.animating = false;
            return;
        }

        const result = Game.executeMove(move.fromR, move.fromC, move.toR, move.toC);
        this.addHistory(move, result);

        if (result.gameOver) {
            this.selectedCell = null;
            this.validMoves = [];
            this.renderBoard();
            this.updateScores();
            this.showGameOver(result.message);
            this.animating = false;
            return;
        }

        if (result.multiCapture) {
            this.renderBoard();
            this.updateScores();
            this.updateStatus('AI continues capturing...');
            /* AI does another capture */
            setTimeout(() => this.doAIMove(), 400);
            return;
        }

        this.selectedCell = null;
        this.validMoves = [];
        this.renderBoard();
        this.updateScores();
        this.highlightActivePlayer();
        this.updateStatus(`Player 1's turn`);
        this.animating = false;
    },

    /* ---- UI Updates ---- */

    updateScores() {
        document.getElementById('p1-score').textContent = Game.p1Score;
        document.getElementById('p2-score').textContent = Game.p2Score;
        document.getElementById('p1-pieces').textContent = `${Game.countPieces(1)} pieces`;
        document.getElementById('p2-pieces').textContent = `${Game.countPieces(2)} pieces`;
    },

    updateStatus(msg) {
        const el = document.getElementById('status-text');
        el.textContent = msg;
        el.className = 'status-text';
    },

    flashStatus(msg, type) {
        const el = document.getElementById('status-text');
        el.textContent = msg;
        el.className = `status-text flash-${type}`;
        setTimeout(() => { el.className = 'status-text'; }, 2000);
    },

    highlightActivePlayer() {
        document.getElementById('p1-panel').classList.toggle('active', Game.currentTurn === 1);
        document.getElementById('p2-panel').classList.toggle('active', Game.currentTurn === 2);
    },

    addHistory(move, result) {
        const list = document.getElementById('move-list');
        const entry = document.createElement('div');
        entry.className = `history-entry p${result.capture ? 'capture' : 'move'}`;
        const player = Game.mode === 'ai' && move.fromR !== undefined
            ? (result.capture ? '⚔' : '→')
            : (result.capture ? '⚔' : '→');
        const from = Game.posLabel(move.fromR, move.fromC);
        const to = Game.posLabel(move.toR, move.toC);
        const num = list.children.length + 1;
        entry.innerHTML = `<span class="move-num">${num}.</span> ${from} ${player} ${to}`;
        if (result.promoted) entry.innerHTML += ' ♛';
        list.appendChild(entry);
        list.scrollTop = list.scrollHeight;
    },

    clearHistory() {
        document.getElementById('move-list').innerHTML = '';
    },

    showGameOver(message) {
        const overlay = document.getElementById('game-over-overlay');
        document.getElementById('game-over-message').textContent = message;
        document.getElementById('final-p1-score').textContent = Game.p1Score;
        document.getElementById('final-p2-score').textContent = Game.p2Score;
        overlay.classList.remove('hidden');
    }
};

/* ---- Bootstrap ---- */
document.addEventListener('DOMContentLoaded', () => {
    UI.init();
});
