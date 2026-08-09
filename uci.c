#include "magpie.h"

// Parse and execute a move string (e.g., "e2e4", "e7e8q") on the board
void apply_move_string(const char *move_str, Colour *side_to_move)
{
    if (!move_str || strlen(move_str) < 4) return;

    int from = algebraic_to_square(move_str);
    int to   = algebraic_to_square(move_str + 2);

    if (!is_on_board(from) || !is_on_board(to)) return;

    int piece = board[from];
    PieceType type = get_piece_type(piece);

    // 1. Handle King Castling
    if (type == PIECE_KING) {
        if (to - from == 2) {
            // King-side castling: move Rook from H rank to F rank
            int rook_from = to + 1;
            int rook_to   = from + 1;
            board[rook_to] = board[rook_from];
            board[rook_from] = PIECE_NONE;
        } else if (from - to == 2) {
            // Queen-side castling: move Rook from A rank to D rank
            int rook_from = to - 2;
            int rook_to   = from - 1;
            board[rook_to] = board[rook_from];
            board[rook_from] = PIECE_NONE;
        }
    }

    // 2. Handle En-Passant Capture
    if (type == PIECE_PAWN && board[to] == PIECE_NONE && get_file(from) != get_file(to)) {
        int ep_captured_sq = make_square(get_rank(from), get_file(to));
        if (is_on_board(ep_captured_sq)) {
            board[ep_captured_sq] = PIECE_NONE;
        }
    }

    // 3. Move Piece
    board[to] = board[from];
    board[from] = PIECE_NONE;

    // 4. Handle Promotion
    if (strlen(move_str) >= 5) {
        char promo = move_str[4];
        PieceType promo_type = PIECE_QUEEN;
        if (promo == 'r') promo_type = PIECE_ROOK;
        if (promo == 'b') promo_type = PIECE_BISHOP;
        if (promo == 'n') promo_type = PIECE_KNIGHT;

        board[to] = *side_to_move | promo_type;
    }

    // Update castling rights, halfmove clock, and position history
    Move m_executed = { .from = from, .to = to, .piece = piece, .captured = board[to], .promotion = (strlen(move_str) >= 5) ? PIECE_QUEEN : PIECE_NONE };
    update_castling_rights(from, to);
    update_halfmove_clock(&m_executed);

    // Update ep_square state
    if (type == PIECE_PAWN && abs(get_rank(to) - get_rank(from)) == 2) {
        ep_square = (from + to) / 2;
    } else {
        ep_square = -1;
    }

    push_position_history(board, *side_to_move, castling_rights);

    // Toggle active player
    *side_to_move = opponent_of(*side_to_move);
}

// Main UCI Command Processing Loop
void run_uci(void)
{
    Colour side_to_move = COLOUR_WHITE;
    char line[32768];

    side_to_move = setup_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    while (fgets(line, sizeof(line), stdin)) {
        // Strip trailing newlines/carriage returns
        line[strcspn(line, "\r\n")] = '\0';
        if (strlen(line) == 0) continue;

        if (strcmp(line, "uci") == 0) {
            printf("id name %s %s\n", ENGINE_NAME, ENGINE_VERSION);
            printf("id author George Bland\n");
            printf("uciok\n");
            fflush(stdout);
        }
        else if (strcmp(line, "isready") == 0) {
            printf("readyok\n");
            fflush(stdout);
        }
        else if (strcmp(line, "ucinewgame") == 0) {
            side_to_move = setup_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        }
        else if (strncmp(line, "position", 8) == 0) {
            const char *ptr = line + 8;
            while (*ptr == ' ') ptr++;

            if (strncmp(ptr, "startpos", 8) == 0) {
                side_to_move = setup_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
                ptr += 8;
            }
            else if (strncmp(ptr, "fen ", 4) == 0) {
                ptr += 4;
                side_to_move = setup_fen(ptr);
                // Advance pointer past FEN string
                while (*ptr && strncmp(ptr, "moves", 5) != 0) {
                    ptr++;
                }
            }

            const char *moves_ptr = strstr(ptr, "moves");
            if (moves_ptr) {
                moves_ptr += 5;
                char *moves_buf = strdup(moves_ptr);
                if (moves_buf) {
                    char *token = strtok(moves_buf, " \r\n");
                    while (token) {
                        apply_move_string(token, &side_to_move);
                        token = strtok(NULL, " \r\n");
                    }
                    free(moves_buf);
                }
            }
        }
        else if (strncmp(line, "go", 2) == 0) {
            make_engine_move(&side_to_move);
        }
        else if (strcmp(line, "tuneeval") == 0) {
            tune_evaluate_position(side_to_move);
        }
        else if (strncmp(line, "loadpst ", 8) == 0) {
            const char *path = line + 8;
            while (*path == ' ') path++;
            if (load_pst_from_file(path) == 0) {
                printf("info string PST loaded from %s\n", path);
            } else {
                printf("info string ERROR: Failed to load PST from %s\n", path);
            }
            fflush(stdout);
        }
        else if (strncmp(line, "savepst ", 8) == 0) {
            const char *path = line + 8;
            while (*path == ' ') path++;
            if (save_pst_to_file(path) == 0) {
                printf("info string PST saved to %s\n", path);
            } else {
                printf("info string ERROR: Failed to save PST to %s\n", path);
            }
            fflush(stdout);
        }
        else if (strcmp(line, "tunebatch") == 0) {
            // High-throughput batch mode for tuning:
            // Reads FEN lines from stdin, outputs best move for each.
            // Empty line terminates batch mode.
            // Output per FEN: "<best_move_uci> <score>"
            // Output "done" when batch ends.
            char fen_line[32768];
            while (fgets(fen_line, sizeof(fen_line), stdin)) {
                fen_line[strcspn(fen_line, "\r\n")] = '\0';
                if (strlen(fen_line) == 0) break;

                side_to_move = setup_fen(fen_line);

                Move moves[256];
                int move_count = generate_moves(board, side_to_move, moves, ep_square);

                int best_score = -999999;
                Move best_move = { .from = -1, .to = -1, .piece = PIECE_NONE,
                                   .captured = PIECE_NONE, .promotion = PIECE_NONE };
                bool found_legal = false;

                for (int i = 0; i < move_count; i++) {
                    Move m = moves[i];
                    int temp_board[BOARD_SIZE];
                    memcpy(temp_board, board, sizeof(board));
                    temp_board[m.from] = PIECE_NONE;
                    int placed = m.piece;
                    if (m.promotion != PIECE_NONE) placed = side_to_move | m.promotion;
                    temp_board[m.to] = placed;
                    if (get_piece_type(m.piece) == PIECE_KING && abs(m.to - m.from) == 2) {
                        if (m.to - m.from == 2) {
                            temp_board[m.from + 1] = temp_board[m.to + 1];
                            temp_board[m.to + 1] = PIECE_NONE;
                        } else if (m.from - m.to == 2) {
                            temp_board[m.from - 1] = temp_board[m.to - 2];
                            temp_board[m.to - 2] = PIECE_NONE;
                        }
                    }
                    if (is_in_check(temp_board, side_to_move)) continue;

                    int score = evaluate_move(board, &m);
                    if (!found_legal || score > best_score) {
                        best_score = score;
                        best_move = m;
                        found_legal = true;
                    }
                }

                if (found_legal && best_move.from != -1) {
                    char from_str[4], to_str[4];
                    square_to_algebraic(best_move.from, from_str);
                    square_to_algebraic(best_move.to, to_str);
                    const char *promo_str = "";
                    if (best_move.promotion == PIECE_QUEEN)  promo_str = "q";
                    if (best_move.promotion == PIECE_ROOK)   promo_str = "r";
                    if (best_move.promotion == PIECE_BISHOP) promo_str = "b";
                    if (best_move.promotion == PIECE_KNIGHT) promo_str = "n";
                    printf("%s%s%s %d\n", from_str, to_str, promo_str, best_score);
                } else {
                    printf("0000 0\n");
                }
            }
            printf("done\n");
            fflush(stdout);
        }
        else if (strcmp(line, "quit") == 0) {
            break;
        }
    }
}
