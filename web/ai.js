/*
 * ai.js — Checkers AI with minimax alpha-beta pruning.
 *
 * Mirrors the C ai.c implementation:
 *   Easy:   purely random valid move
 *   Medium: prefer captures (prioritize king captures)
 *   Hard:   minimax depth-6 with alpha-beta pruning
 */

const AI = {
    DEPTH: 6,

    /* ---- Move generation for simulation ---- */

    generateMoves(board, player) {
        const moves = [];
        const fwd = player === 1 ? -1 : 1;
        let hasCapture = false;

        /* First pass: check if any capture exists */
        for (let r = 0; r < BOARD_SIZE && !hasCapture; r++) {
            for (let c = 0; c < BOARD_SIZE && !hasCapture; c++) {
                const p = board[r][c];
                if (!p || p.player !== player) continue;
                for (const [dr, dc] of Game.DIRS) {
                    if (!p.king && dr !== fwd) continue;
                    const nr = r + dr, nc = c + dc;
                    const jr = r + 2 * dr, jc = c + 2 * dc;
                    if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) continue;
                    if (jr < 0 || jr >= BOARD_SIZE || jc < 0 || jc >= BOARD_SIZE) continue;
                    const mid = board[nr][nc];
                    if (mid && mid.player !== player && board[jr][jc] === null) {
                        hasCapture = true;
                    }
                }
            }
        }

        /* Second pass: generate moves */
        for (let r = 0; r < BOARD_SIZE; r++) {
            for (let c = 0; c < BOARD_SIZE; c++) {
                const p = board[r][c];
                if (!p || p.player !== player) continue;
                for (const [dr, dc] of Game.DIRS) {
                    if (!p.king && dr !== fwd) continue;
                    if (hasCapture) {
                        const nr = r + dr, nc = c + dc;
                        const jr = r + 2 * dr, jc = c + 2 * dc;
                        if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) continue;
                        if (jr < 0 || jr >= BOARD_SIZE || jc < 0 || jc >= BOARD_SIZE) continue;
                        const mid = board[nr][nc];
                        if (mid && mid.player !== player && board[jr][jc] === null) {
                            moves.push({ fromR: r, fromC: c, toR: jr, toC: jc });
                        }
                    } else {
                        const nr = r + dr, nc = c + dc;
                        if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) continue;
                        if (board[nr][nc] === null) {
                            moves.push({ fromR: r, fromC: c, toR: nr, toC: nc });
                        }
                    }
                }
            }
        }
        return moves;
    },

    /* ---- Simulate a move on a cloned board ---- */

    simMove(board, move, player) {
        const b = board.map(row => row.map(cell => cell ? { ...cell } : null));
        const piece = { ...b[move.fromR][move.fromC] };
        b[move.fromR][move.fromC] = null;
        const dist = Math.abs(move.toR - move.fromR);
        if (dist === 2) {
            const mr = (move.fromR + move.toR) / 2;
            const mc = (move.fromC + move.toC) / 2;
            b[mr][mc] = null;
        }
        const backRow = player === 1 ? 0 : 7;
        if (!piece.king && move.toR === backRow) piece.king = true;
        b[move.toR][move.toC] = piece;
        return b;
    },

    /* ---- Board Evaluation ---- */

    evaluate(board, aiPlayer) {
        const opp = aiPlayer === 1 ? 2 : 1;
        let score = 0;
        let myPieces = 0, oppPieces = 0;
        const aiFwd = aiPlayer === 1 ? -1 : 1;
        const oppFwd = opp === 1 ? -1 : 1;

        for (let r = 0; r < BOARD_SIZE; r++) {
            for (let c = 0; c < BOARD_SIZE; c++) {
                const p = board[r][c];
                if (!p) continue;

                if (p.player === aiPlayer) {
                    myPieces++;
                    if (p.king) {
                        score += 5;
                        if (c >= 2 && c <= 5 && r >= 2 && r <= 5) score += 2;
                    } else {
                        score += 3;
                        score += (aiFwd === -1) ? (BOARD_SIZE - 1 - r) : r;
                        if (c >= 2 && c <= 5 && r >= 2 && r <= 5) score += 1;
                    }
                } else {
                    oppPieces++;
                    if (p.king) {
                        score -= 5;
                        if (c >= 2 && c <= 5 && r >= 2 && r <= 5) score -= 2;
                    } else {
                        score -= 3;
                        score -= (oppFwd === -1) ? (BOARD_SIZE - 1 - r) : r;
                        if (c >= 2 && c <= 5 && r >= 2 && r <= 5) score -= 1;
                    }
                }
            }
        }
        if (oppPieces === 0) score += 1000;
        if (myPieces === 0) score -= 1000;
        return score;
    },

    /* ---- Minimax with Alpha-Beta Pruning ---- */

    minimax(board, aiPlayer, depth, alpha, beta, maximizing) {
        if (depth === 0) return this.evaluate(board, aiPlayer);

        const curPlayer = maximizing ? aiPlayer : (aiPlayer === 1 ? 2 : 1);
        const moves = this.generateMoves(board, curPlayer);

        if (moves.length === 0) {
            return maximizing ? (-900 - depth) : (900 + depth);
        }

        if (maximizing) {
            let best = -Infinity;
            for (const move of moves) {
                const child = this.simMove(board, move, curPlayer);
                const val = this.minimax(child, aiPlayer, depth - 1, alpha, beta, false);
                if (val > best) best = val;
                if (val > alpha) alpha = val;
                if (beta <= alpha) break;
            }
            return best;
        } else {
            let best = Infinity;
            for (const move of moves) {
                const child = this.simMove(board, move, curPlayer);
                const val = this.minimax(child, aiPlayer, depth - 1, alpha, beta, true);
                if (val < best) best = val;
                if (val < beta) beta = val;
                if (beta <= alpha) break;
            }
            return best;
        }
    },

    /* ---- Public API ---- */

    chooseMove(difficulty) {
        const aiPlayer = 2; /* AI is always player 2 */
        const board = Game.cloneBoard();
        const moves = this.generateMoves(board, aiPlayer);
        if (moves.length === 0) return null;

        let chosen = 0;

        switch (difficulty) {
            case 1: /* Easy — random */
                chosen = Math.floor(Math.random() * moves.length);
                break;

            case 2: { /* Medium — prefer captures, especially king captures */
                chosen = Math.floor(Math.random() * moves.length);
                let bestScore = -1;
                for (let i = 0; i < moves.length; i++) {
                    const dr = Math.abs(moves[i].toR - moves[i].fromR);
                    if (dr === 2) {
                        const mr = (moves[i].fromR + moves[i].toR) / 2;
                        const mc = (moves[i].fromC + moves[i].toC) / 2;
                        const captured = board[mr][mc];
                        const s = (captured && captured.king) ? 2 : 1;
                        if (s > bestScore) { bestScore = s; chosen = i; }
                    }
                }
                break;
            }

            case 3: { /* Hard — minimax */
                let bestScore = -Infinity;
                for (let i = 0; i < moves.length; i++) {
                    const child = this.simMove(board, moves[i], aiPlayer);
                    let score = this.minimax(child, aiPlayer, this.DEPTH - 1,
                                             -Infinity, Infinity, false);
                    score += (Math.random() * 2 - 1); /* tie-breaking noise */
                    if (score > bestScore) {
                        bestScore = score;
                        chosen = i;
                    }
                }
                break;
            }

            default:
                chosen = Math.floor(Math.random() * moves.length);
        }

        return moves[chosen];
    }
};
