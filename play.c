#include "magpie.h"

// Positional Piece-Square Table (PST) favoring central control
static const int PST[64] = {
    0,  2,  4,  6,  6,  4,  2, 0,
    2,  8, 10, 12, 12, 10,  8, 2,
    6, 12, 16, 18, 18, 16, 12, 6,
    8, 14, 18, 20, 20, 18, 14, 8,
    8, 14, 18, 20, 20, 18, 14, 8,
    6, 12, 16, 18, 18, 16, 12, 6,
    2,  8, 10, 12, 12, 10,  8, 2,
    0,  2,  4,  6,  6,  4,  2, 0
};

// Evaluate a move using SEE, threat mitigation, and PST positioning
int evaluate_move(const int brd[BOARD_SIZE], const Move *m)
{
    Colour side = get_piece_colour(m->piece);
    int initial_threat = evaluate_board_threat(brd, side);

    // Make move on temp_board to evaluate resulting threat
    int temp_board[BOARD_SIZE];
    memcpy(temp_board, brd, sizeof(int) * BOARD_SIZE);
    temp_board[m->from] = PIECE_NONE;
    int placed_piece = (m->promotion != PIECE_NONE) ? (int)(side | m->promotion) : m->piece;
    temp_board[m->to] = placed_piece;

    int resulting_threat = evaluate_board_threat(temp_board, side);
    int threat_reduction = initial_threat - resulting_threat;

    int see_score = static_exchange_evaluation(brd, *m);

    PieceType type = get_piece_type(m->piece);
    int multiplier = (type != PIECE_KING ? 1 : -1);
    int previous_pst_val = PST[m->from] * multiplier;
    int new_pst_val = PST[m->to] * multiplier;
    int pst_diff = new_pst_val - previous_pst_val;

    bool is_capture = (m->captured != PIECE_NONE);

    if (see_score >= 0) {
        if (is_capture) {
            // Category 1: Good / Equal Captures
            return 20000 + see_score * 10 + pst_diff;
        } else if (initial_threat > 0 && threat_reduction > 0) {
            // Category 2: Threat Mitigation Moves (Escapes / Blocks / Defenses / Counter-attacks)
            return 15000 + threat_reduction * 10 + see_score * 10 + pst_diff;
        } else {
            // Category 3: Safe Quiet Moves
            return pst_diff;
        }
    } else {
        if (is_capture) {
            // Category 5: Losing Captures (placed strictly AFTER quiet moves)
            return -20000 + see_score * 10 + pst_diff;
        } else {
            // Category 4: Unsafe Quiet Moves (moves into undefended attack)
            return -10000 + see_score * 10 + pst_diff;
        }
    }
}

// Select and execute the best move for side_to_move using 0-ply static move evaluation
void make_engine_move(Colour *side_to_move)
{
    Move moves[256];
    int move_count = generate_moves(board, *side_to_move, moves);

    int best_score = -999999;
    Move best_move = { .from = -1, .to = -1, .piece = PIECE_NONE, .captured = PIECE_NONE, .promotion = PIECE_NONE };
    bool found_legal_move = false;

    Colour opp = opponent_of(*side_to_move);

    for (int i = 0; i < move_count; i++) {
        Move m = moves[i];

        // 1. Make move on temporary board copy
        int temp_board[BOARD_SIZE];
        memcpy(temp_board, board, sizeof(board));

        temp_board[m.from] = PIECE_NONE;
        int placed_piece = m.piece;
        if (m.promotion != PIECE_NONE) {
            placed_piece = *side_to_move | m.promotion;
        }
        temp_board[m.to] = placed_piece;

        // 2. Reject move if it leaves player's King in check
        if (is_in_check(temp_board, *side_to_move)) {
            continue;
        }

        // 3. Evaluate candidate move directly
        int score = evaluate_move(board, &m);

        if (!found_legal_move || score > best_score) {
            best_score = score;
            best_move = m;
            found_legal_move = true;
        }
    }

    // Output UCI bestmove
    if (found_legal_move && best_move.from != -1 && best_move.to != -1) {
        // Execute best move on main board
        board[best_move.from] = PIECE_NONE;
        int placed_piece = best_move.piece;
        if (best_move.promotion != PIECE_NONE) {
            placed_piece = *side_to_move | best_move.promotion;
        }
        board[best_move.to] = placed_piece;

        char from_str[4], to_str[4];
        square_to_algebraic(best_move.from, from_str);
        square_to_algebraic(best_move.to, to_str);

        const char *promo_str = "";
        if (best_move.promotion == PIECE_QUEEN)  promo_str = "q";
        if (best_move.promotion == PIECE_ROOK)   promo_str = "r";
        if (best_move.promotion == PIECE_BISHOP) promo_str = "b";
        if (best_move.promotion == PIECE_KNIGHT) promo_str = "n";

        printf("bestmove %s%s%s\n", from_str, to_str, promo_str);
        *side_to_move = opp;
    } else {
        printf("bestmove 0000\n");
    }

    fflush(stdout);
}
