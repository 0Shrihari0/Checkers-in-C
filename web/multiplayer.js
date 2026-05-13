/*
 * multiplayer.js — WebSocket client for online multiplayer.
 *
 * Connects to the Node.js server, handles matchmaking,
 * sends moves, and receives board updates for rendering.
 */

const Multiplayer = {
    ws: null,
    connected: false,
    playerNum: 0,
    p1Name: '',
    p2Name: '',
    gameActive: false,

    /* ---- Connection ---- */

    connect(name) {
        /* Auto-detect server address: same host as the page */
        const protocol = location.protocol === 'https:' ? 'wss:' : 'ws:';
        const host = location.host || 'localhost:3000';
        const url = `${protocol}//${host}`;

        console.log(`[MP] Connecting to ${url}...`);
        this.ws = new WebSocket(url);

        this.ws.onopen = () => {
            console.log('[MP] Connected');
            this.connected = true;
            /* Send join request */
            this.send({ type: 'join', name: name });
        };

        this.ws.onmessage = (event) => {
            let msg;
            try { msg = JSON.parse(event.data); } catch { return; }
            this.handleMessage(msg);
        };

        this.ws.onclose = () => {
            console.log('[MP] Disconnected');
            this.connected = false;
            if (this.gameActive) {
                UI.updateStatus('Connection lost!');
                UI.flashStatus('Disconnected from server', 'error');
            }
        };

        this.ws.onerror = (err) => {
            console.error('[MP] WebSocket error:', err);
            UI.flashStatus('Could not connect to server. Is it running?', 'error');
        };
    },

    disconnect() {
        if (this.ws) {
            this.ws.close();
            this.ws = null;
        }
        this.connected = false;
        this.gameActive = false;
        this.playerNum = 0;
    },

    send(data) {
        if (this.ws && this.ws.readyState === WebSocket.OPEN) {
            this.ws.send(JSON.stringify(data));
        }
    },

    /* ---- Send a move to the server ---- */

    sendMove(fromR, fromC, toR, toC) {
        this.send({ type: 'move', fromR, fromC, toR, toC });
    },

    /* ---- Handle incoming messages ---- */

    handleMessage(msg) {
        switch (msg.type) {
            case 'waiting':
                UI.updateStatus('Waiting for an opponent to connect...');
                break;

            case 'start':
                this.playerNum = msg.playerNum;
                this.p1Name = msg.p1Name;
                this.p2Name = msg.p2Name;
                this.gameActive = true;

                /* Update player labels */
                document.getElementById('p1-label').textContent = msg.p1Name;
                document.getElementById('p2-label').textContent = msg.p2Name;

                /* Show who you are */
                const youAre = msg.playerNum === 1 ? msg.p1Name : msg.p2Name;
                UI.flashStatus(`Game found! You are ${youAre} (Player ${msg.playerNum})`, 'success');
                break;

            case 'board':
                this.handleBoardUpdate(msg);
                break;

            case 'error':
                UI.flashStatus(msg.message, 'error');
                break;

            case 'win':
                this.handleWin(msg);
                break;

            case 'opponent_disconnected':
                this.gameActive = false;
                UI.showGameOver(msg.message);
                break;
        }
    },

    /* ---- Apply server board state to local Game object ---- */

    handleBoardUpdate(msg) {
        /* Update the local Game state from server data */
        for (let r = 0; r < BOARD_SIZE; r++) {
            for (let c = 0; c < BOARD_SIZE; c++) {
                Game.board[r][c] = msg.board[r][c];
            }
        }
        Game.currentTurn = msg.currentTurn;
        Game.p1Score = msg.p1Score;
        Game.p2Score = msg.p2Score;
        Game.inMultiCapture = msg.inMultiCapture;
        Game.multiCaptureRow = msg.mcRow;
        Game.multiCaptureCol = msg.mcCol;
        Game.gameOver = false;

        /* Clear selection */
        UI.selectedCell = null;
        UI.validMoves = [];

        /* In online mode, compute valid moves locally for highlighting,
           but only for our own turn */
        if (msg.currentTurn === this.playerNum) {
            /* Allow piece selection */
        }

        UI.renderBoard();
        UI.updateScores();
        UI.highlightActivePlayer();

        /* Update player labels if not set */
        if (this.p1Name) {
            document.getElementById('p1-label').textContent = this.p1Name;
            document.getElementById('p2-label').textContent = this.p2Name;
        }

        /* Status message */
        if (msg.message) {
            UI.updateStatus(msg.message);
        }

        /* Show whose turn it is relative to the player */
        if (msg.currentTurn === this.playerNum) {
            if (!msg.inMultiCapture) {
                UI.updateStatus('Your turn! Click a piece to move.');
            }
            /* Auto-select multi-capture piece */
            if (msg.inMultiCapture) {
                UI.selectedCell = { r: msg.mcRow, c: msg.mcCol };
                UI.validMoves = Game.getMovesFrom(msg.mcRow, msg.mcCol);
                UI.renderBoard();
                UI.updateStatus(`Multi-capture! Continue from ${Game.posLabel(msg.mcRow, msg.mcCol)}.`);
            }
        } else {
            const oppName = msg.currentTurn === 1 ? this.p1Name : this.p2Name;
            UI.updateStatus(`Waiting for ${oppName}...`);
        }
    },

    handleWin(msg) {
        this.gameActive = false;
        Game.gameOver = true;
        Game.winner = msg.winner;
        Game.p1Score = msg.p1Score;
        Game.p2Score = msg.p2Score;
        UI.updateScores();

        /* Personalized message */
        let displayMsg = msg.message;
        if (msg.winner === this.playerNum) {
            displayMsg = '🎉 You win! ' + msg.message;
        } else if (msg.winner && msg.winner !== 'draw') {
            displayMsg = 'You lost. ' + msg.message;
        }
        UI.showGameOver(displayMsg);
    }
};
