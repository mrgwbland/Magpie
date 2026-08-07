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

    // Toggle active player
    *side_to_move = opponent_of(*side_to_move);
}

// Main UCI Command Processing Loop
void run_uci(void)
{
    Colour side_to_move = COLOUR_WHITE;
    char line[2048];

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
                char moves_buf[1024];
                strncpy(moves_buf, moves_ptr, sizeof(moves_buf) - 1);
                moves_buf[sizeof(moves_buf) - 1] = '\0';

                char *token = strtok(moves_buf, " ");
                while (token) {
                    apply_move_string(token, &side_to_move);
                    token = strtok(NULL, " ");
                }
            }
        }
        else if (strncmp(line, "go", 2) == 0) {
            make_engine_move(&side_to_move);
        }
        else if (strcmp(line, "quit") == 0) {
            break;
        }
    }
}
