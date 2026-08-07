#include "magpie.h"

#if defined(WIN32) || defined(_WIN32)
#  include <windows.h>
#else
#  include <sys/time.h>
   static long get_tick_count_ms(void) {
       struct timeval t;
       gettimeofday(&t, NULL);
       return t.tv_sec * 1000 + t.tv_usec / 1000;
   }
#  define GetTickCount get_tick_count_ms
#endif

// Horizontally mirrored 32-element Piece-Square Tables (PST) for each piece type
static const int PST[7][32] = {
    [PIECE_NONE] = {
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0
    },
    [PIECE_PAWN] = {
        0, 2, 4, 6,
        2, 8, 10, 12,
        6, 12, 16, 18,
        8, 14, 18, 20,
        8, 14, 18, 20,
        6, 12, 16, 18,
        2, 8, 10, 12,
        0, 2, 4, 6
    },
    [PIECE_KING] = {
        0, -2, -4, -6,
        -2, -8, -10, -12,
        -6, -12, -16, -18,
        -8, -14, -18, -20,
        -8, -14, -18, -20,
        -6, -12, -16, -18,
        -2, -8, -10, -12,
        0, -2, -4, -6
    },
    [PIECE_KNIGHT] = {
        0, 2, 4, 6,
        2, 8, 10, 12,
        6, 12, 16, 18,
        8, 14, 18, 20,
        8, 14, 18, 20,
        6, 12, 16, 18,
        2, 8, 10, 12,
        0, 2, 4, 6
    },
    [PIECE_BISHOP] = {
        0, 2, 4, 6,
        2, 8, 10, 12,
        6, 12, 16, 18,
        8, 14, 18, 20,
        8, 14, 18, 20,
        6, 12, 16, 18,
        2, 8, 10, 12,
        0, 2, 4, 6
    },
    [PIECE_ROOK] = {
        0, 2, 4, 6,
        2, 8, 10, 12,
        6, 12, 16, 18,
        8, 14, 18, 20,
        8, 14, 18, 20,
        6, 12, 16, 18,
        2, 8, 10, 12,
        0, 2, 4, 6
    },
    [PIECE_QUEEN] = {
        0, 2, 4, 6,
        2, 8, 10, 12,
        6, 12, 16, 18,
        8, 14, 18, 20,
        8, 14, 18, 20,
        6, 12, 16, 18,
        2, 8, 10, 12,
        0, 2, 4, 6
    }
};

static inline int get_pst_val(PieceType type, int sq)
{
    int r = sq / 8;
    int f = sq % 8;
    if (f >= 4) f = 7 - f;
    return PST[type][r * 4 + f];
}

// Evaluate a move
int evaluate_move(const int brd[BOARD_SIZE], const Move *m)
{
    // =========================================================================
    // STAGE 1: Terminal Game State Check (Checkmate, Stalemate, 3-fold, 50-move)
    // =========================================================================
    int term_score = 0;
    if (is_terminal_move(brd, m, &term_score)) {
        return term_score;
    }

    // =========================================================================
    // STAGE 2: Board State & Move Simulation
    // =========================================================================
    Colour side = get_piece_colour(m->piece);
    Colour opp  = opponent_of(side);

    int temp_board[BOARD_SIZE];
    memcpy(temp_board, brd, sizeof(int) * BOARD_SIZE);
    temp_board[m->from] = PIECE_NONE;
    int placed_piece = (m->promotion != PIECE_NONE) ? (int)(side | m->promotion) : m->piece;
    temp_board[m->to] = placed_piece;

    // Handle Rook placement on temp_board for castling moves
    if (get_piece_type(m->piece) == PIECE_KING && abs(m->to - m->from) == 2) {
        if (m->to - m->from == 2) {
            temp_board[m->from + 1] = temp_board[m->to + 1];
            temp_board[m->to + 1] = PIECE_NONE;
        } else if (m->from - m->to == 2) {
            temp_board[m->from - 1] = temp_board[m->to - 2];
            temp_board[m->to - 2] = PIECE_NONE;
        }
    }

    // =========================================================================
    // STAGE 3: Defensive Threat Evaluation (Saving Friendly Pieces)
    // =========================================================================
    // Note: Destination square m->to is excluded from resulting_friendly_threat
    // because exchange safety on m->to is already computed by SEE in Stage 4.
    int initial_friendly_threat = evaluate_board_threat(brd, side);
    int resulting_friendly_threat = evaluate_board_threat_except(temp_board, side, m->to);
    int threat_reduction = initial_friendly_threat - resulting_friendly_threat;

    // =========================================================================
    // STAGE 4: Static Exchange Evaluation (SEE Target Square Safety & Material)
    // =========================================================================
    int see_score = static_exchange_evaluation(brd, *m);

    // =========================================================================
    // STAGE 5: Offensive Threat Evaluation (Creating New Enemy Threats) - this stage gives an inherent knowledge of forks
    // =========================================================================
    // Material value of enemy pieces threatened after move minus before move
    int initial_enemy_threat = evaluate_board_threat(brd, opp);
    int resulting_enemy_threat = evaluate_board_threat(temp_board, opp);
    int offensive_threat_diff = resulting_enemy_threat - initial_enemy_threat;

    // Low-magnitude offensive bonus applied on top of positional PST scoring (1/10th of material threat)
    int offensive_bonus = offensive_threat_diff / 10;

    // =========================================================================
    // STAGE 6: Positional Piece-Square Table (PST) Delta
    // =========================================================================
    PieceType type = get_piece_type(m->piece);
    int pst_diff = 0;

    if (type == PIECE_KING && abs(m->to - m->from) == 2) {
        // Dual PST scoring for Castling moves
        int king_pst_diff = get_pst_val(PIECE_KING, m->to) - get_pst_val(PIECE_KING, m->from);
        int rook_from = (m->to > m->from) ? (m->to + 1) : (m->to - 2);
        int rook_to   = (m->to > m->from) ? (m->from + 1) : (m->from - 1);
        int rook_pst_diff = get_pst_val(PIECE_ROOK, rook_to) - get_pst_val(PIECE_ROOK, rook_from);
        pst_diff = king_pst_diff + rook_pst_diff + 50; // includes +50 castling bonus
    } else {
        int previous_pst_val = get_pst_val(type, m->from);
        int new_pst_val = get_pst_val(type, m->to);
        pst_diff = new_pst_val - previous_pst_val;
    }

    // =========================================================================
    // STAGE 7: Final Move Scoring & Tier Categorization
    // =========================================================================
    bool is_capture = (m->captured != PIECE_NONE);
    int positional_tactical_bonus = pst_diff + offensive_bonus;

    if (see_score >= 0) {
        // Category 1: Safe Moves (Blend SEE score and defensive threat reduction into Expected Value)
        int ev_score = see_score + (threat_reduction > 0 ? threat_reduction : 0);
        return 10000 + ev_score * 10 + positional_tactical_bonus;
    } else {
        if (is_capture) {
            // Category 3: Losing Captures (placed strictly AFTER quiet moves)
            return -20000 + see_score * 10 + positional_tactical_bonus;
        } else {
            // Category 2: Unsafe Quiet Moves (moves into undefended attack)
            return -10000 + see_score * 10 + positional_tactical_bonus;
        }
    }
}

// Select and execute the best move for side_to_move using 0-ply static move evaluation
void make_engine_move(Colour *side_to_move)
{
    long start_time = GetTickCount();
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

        // Also move Rook on temp_board if castling
        if (get_piece_type(m.piece) == PIECE_KING && abs(m.to - m.from) == 2) {
            if (m.to - m.from == 2) {
                temp_board[m.from + 1] = temp_board[m.to + 1];
                temp_board[m.to + 1] = PIECE_NONE;
            } else if (m.from - m.to == 2) {
                temp_board[m.from - 1] = temp_board[m.to - 2];
                temp_board[m.to - 2] = PIECE_NONE;
            }
        }

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

    long elapsed_ms = GetTickCount() - start_time;
    printf("info depth 0 time %ld\n", elapsed_ms);

    // Output UCI bestmove
    if (found_legal_move && best_move.from != -1 && best_move.to != -1) {
        // Execute best move on main board
        board[best_move.from] = PIECE_NONE;
        int placed_piece = best_move.piece;
        if (best_move.promotion != PIECE_NONE) {
            placed_piece = *side_to_move | best_move.promotion;
        }
        board[best_move.to] = placed_piece;

        // Also move Rook on main board if castling
        if (get_piece_type(best_move.piece) == PIECE_KING && abs(best_move.to - best_move.from) == 2) {
            if (best_move.to - best_move.from == 2) {
                board[best_move.from + 1] = board[best_move.to + 1];
                board[best_move.to + 1] = PIECE_NONE;
            } else if (best_move.from - best_move.to == 2) {
                board[best_move.from - 1] = board[best_move.to - 2];
                board[best_move.to - 2] = PIECE_NONE;
            }
        }

        update_castling_rights(best_move.from, best_move.to);
        update_halfmove_clock(&best_move);
        push_position_history(board, *side_to_move, castling_rights);

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
