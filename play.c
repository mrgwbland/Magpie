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

// Tunable non-PST evaluation parameters
int CASTLING_BONUS = 53;
int OFFENSIVE_THREAT_WEIGHT = 23;
int EV_WEIGHT = 4;
int RISK_WEIGHT = 10;
int MOBILITY_WEIGHT[7] = {
    [PIECE_NONE]   = 0,
    [PIECE_PAWN]   = 0,
    [PIECE_KING]   = 0,
    [PIECE_KNIGHT] = 11,
    [PIECE_BISHOP] = 2,
    [PIECE_ROOK]   = 4,
    [PIECE_QUEEN]  = 4
};

// Helper function to calculate total non-pawn material on the board
int get_board_non_pawn_material(const int brd[BOARD_SIZE])
{
    int npm = 0;
    for (int i = 0; i < BOARD_SIZE; i++) {
        int piece = brd[i];
        if (piece == PIECE_NONE) continue;
        PieceType type = get_piece_type(piece);
        if (type == PIECE_KNIGHT || type == PIECE_BISHOP ||
            type == PIECE_ROOK || type == PIECE_QUEEN) {
            npm += PIECE_VALUES[type];
        }
    }
    return npm;
}

// Horizontally mirrored 32-element Piece-Square Tables
// Middlegame Piece-Square Tables
static int PST_MG[7][32] = {
    [PIECE_NONE] = {
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0
    },
    [PIECE_PAWN] = {
         -1,   1,   4,   6,
          1,   5,  18,   8,
          9,  18,  15,  24,
         14,  10,  15,  20,
          3,   8,  12,  26,
          8,  12,  15,  17,
         -4,   9,   5,  12,
          3,   0,   4,   7
    },
    [PIECE_KING] = {
         -4,   4,  -7,  -7,
         -2,  -3,  -2, -11,
        -12,  -9, -16, -19,
         -8, -17, -19, -22,
         -6, -13, -22, -20,
         -4, -10, -13, -18,
          2,  -4,   0,  -9,
         -5,   4,  -9,  -6
    },
    [PIECE_KNIGHT] = {
          1, -10,   2,   1,
          3,   4,  14,  10,
          6,  10,  22,  17,
         16,  17,  22,  27,
         11,  15,  18,  22,
          8,  10,  16,  17,
          1,  10,  10,   9,
         -3,  -7,  -1,   5
    },
    [PIECE_BISHOP] = {
          0,   4,  -6,  10,
         -1,  15,  11,  14,
          9,  13,  18,  17,
          6,  11,  20,  25,
          3,   9,  21,  18,
          7,  22,  16,  15,
          2,  15,  15,  12,
          1,   5,  -5,   0
    },
    [PIECE_ROOK] = {
          0,   5,   9,  12,
         -2,   8,  12,  12,
          4,  13,  18,  19,
          7,  17,  15,  13,
         10,  12,  13,  18,
          9,  15,  16,  12,
         -4,  15,  14,  20,
          2,   4,   8,  21
    },
    [PIECE_QUEEN] = {
         -1,   5,   3,   8,
          7,  12,  11,  16,
          5,  12,  14,  20,
         13,  15,  20,  14,
          5,   9,  17,  15,
          7,  10,  13,  12,
          1,  11,   9,  15,
          0,   1,   5,  12
    }
};

// Endgame Piece-Square Tables
static int PST_EG[7][32] = {
    [PIECE_NONE] = {
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0
    },
    [PIECE_PAWN] = {
          2,  -4,   0,   7,
         29,  33,  32,  42,
         17,  18,  20,  23,
          8,  10,   9,  10,
          3,   5,   5,   7,
         -2,   1,   2,  11,
          1,   2,   4,  21,
          3,  -2,   0,  -2
    },
    [PIECE_KING] = {
        -11,   2,   5,   3,
          5,  18,  36,  21,
          3,  13,  18,  19,
          5,  15,  11,  15,
          1,  11,  16,  17,
          0,  16,  25,  20,
          4,  16,  18,  21,
         -4,   2,   6,   0
    },
    [PIECE_KNIGHT] = {
          1,   6,   3,   6,
         -2,  13,   8,  10,
          7,  14,  14,  21,
          8,  17,  16,  21,
          6,  10,  21,  21,
          4,  16,  11,  19,
          6,   4,  12,   5,
         -3,  -4,   2,   9
    },
    [PIECE_BISHOP] = {
         -2,   1,   5,   5,
          0,   5,   8,  11,
          5,  13,  13,  15,
          5,  16,  14,  23,
         13,  15,  13,  32,
          7,   8,  13,  17,
         -1,   6,   9,  12,
         -1,   0,   1,  10
    },
    [PIECE_ROOK] = {
         -3,  -1,   8,   7,
          0,  12,  11,  18,
         11,  11,  16,  16,
         12,  12,  14,  15,
         11,  10,  17,  11,
          4,  14,  12,  16,
          1,  15,  14,  14,
          1,   4,   1,  10
    },
    [PIECE_QUEEN] = {
          3,   2,   3,  11,
          4,  18,  19,   9,
          6,  16,  13,  17,
          8,   9,  17,  11,
         10,  11,  19,  17,
          8,  15,  21,  15,
          2,   9,   8,  14,
         -7,   0,   2,   6
    }
};


static inline int get_pst_val(PieceType type, int sq, int npm)
{
    int r = sq / 8;
    int f = sq % 8;
    if (f >= 4) f = 7 - f;
    int idx = r * 4 + f;

    if (npm > MAX_NON_PAWN_MATERIAL) npm = MAX_NON_PAWN_MATERIAL;
    if (npm < 0) npm = 0;

    int mg_val = PST_MG[type][idx];
    int eg_val = PST_EG[type][idx];

    return (mg_val * npm + eg_val * (MAX_NON_PAWN_MATERIAL - npm)) / MAX_NON_PAWN_MATERIAL;
}

// Calculate mobility (legal moves including captures) for Rooks, Bishops, Knights, and Queens
int calculate_piece_mobility(const int brd[BOARD_SIZE], int square, PieceType type, Colour side)
{
    if (!is_on_board(square)) return 0;
    if (type != PIECE_KNIGHT && type != PIECE_BISHOP && type != PIECE_ROOK && type != PIECE_QUEEN) {
        return 0;
    }

    int r = get_rank(square);
    int f = get_file(square);
    int mobility = 0;

    // 1. Knight mobility
    if (type == PIECE_KNIGHT) {
        static const int dr[8] = {-2, -2, -1, -1, 1, 1, 2, 2};
        static const int df[8] = {-1, 1, -2, 2, -2, 2, -1, 1};
        for (int i = 0; i < 8; i++) {
            int nr = r + dr[i];
            int nf = f + df[i];
            if (nr >= 0 && nr < 8 && nf >= 0 && nf < 8) {
                int target_sq = make_square(nr, nf);
                int p = brd[target_sq];
                if (p == PIECE_NONE || get_piece_colour(p) != side) {
                    mobility++;
                }
            }
        }
        return mobility;
    }

    // 2. Diagonal directions (Bishop & Queen)
    if (type == PIECE_BISHOP || type == PIECE_QUEEN) {
        static const int dr_diag[4] = {-1, -1, 1, 1};
        static const int df_diag[4] = {-1, 1, -1, 1};
        for (int i = 0; i < 4; i++) {
            int nr = r + dr_diag[i];
            int nf = f + df_diag[i];
            while (nr >= 0 && nr < 8 && nf >= 0 && nf < 8) {
                int target_sq = make_square(nr, nf);
                int p = brd[target_sq];
                if (p == PIECE_NONE) {
                    mobility++;
                } else {
                    if (get_piece_colour(p) != side) {
                        mobility++; // Can capture enemy piece
                    }
                    break; // Blocked by piece
                }
                nr += dr_diag[i];
                nf += df_diag[i];
            }
        }
    }

    // 3. Orthogonal directions (Rook & Queen)
    if (type == PIECE_ROOK || type == PIECE_QUEEN) {
        static const int dr_ortho[4] = {-1, 1, 0, 0};
        static const int df_ortho[4] = {0, 0, -1, 1};
        for (int i = 0; i < 4; i++) {
            int nr = r + dr_ortho[i];
            int nf = f + df_ortho[i];
            while (nr >= 0 && nr < 8 && nf >= 0 && nf < 8) {
                int target_sq = make_square(nr, nf);
                int p = brd[target_sq];
                if (p == PIECE_NONE) {
                    mobility++;
                } else {
                    if (get_piece_colour(p) != side) {
                        mobility++; // Can capture enemy piece
                    }
                    break; // Blocked by piece
                }
                nr += dr_ortho[i];
                nf += df_ortho[i];
            }
        }
    }

    return mobility;
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

    // Handle En Passant capture on temp_board
    if (get_piece_type(m->piece) == PIECE_PAWN && get_file(m->from) != get_file(m->to) && brd[m->to] == PIECE_NONE) {
        int ep_captured_sq = make_square(get_rank(m->from), get_file(m->to));
        if (is_on_board(ep_captured_sq)) {
            temp_board[ep_captured_sq] = PIECE_NONE;
        }
    }

    // =========================================================================
    // STAGE 3: Defensive Threat Evaluation (Saving Friendly Pieces)
    // =========================================================================
    // Note: Destination square m->to is excluded from resulting_friendly_threat
    // because exchange safety on m->to is already computed by SEE in Stage 4.

    // Check if the move is en passant
    int cand_ep = (get_piece_type(m->piece) == PIECE_PAWN && abs(get_rank(m->to) - get_rank(m->from)) == 2) ? (m->from + m->to) / 2 : -1;
    // Attacks on our pieces
    int initial_friendly_threat = evaluate_board_threat(brd, side, ep_square);
    // Attacks on our pieces after the move
    int resulting_friendly_threat = evaluate_board_threat_except(temp_board, side, m->to, cand_ep);
    // The reduction in attacks on our pieces from this move
    int threat_reduction = initial_friendly_threat - resulting_friendly_threat;

    // =========================================================================
    // STAGE 4: Static Exchange Evaluation (SEE Target Square Safety & Material)
    // =========================================================================
    // Is it safe to move to the destination square?
    int see_score = static_exchange_evaluation(brd, *m);

    // =========================================================================
    // STAGE 5: Offensive Threat Evaluation (Creating New Enemy Threats) - this stage gives an inherent knowledge of forks
    // =========================================================================
    // Material value of enemy pieces threatened before move
    int initial_enemy_threat = evaluate_board_threat(brd, opp, ep_square);
    // Material value of enemy pieces threatened after move
    int resulting_enemy_threat = evaluate_board_threat(temp_board, opp, cand_ep);
    // The increase in attacks on enemy pieces from this move
    int offensive_threat_diff = resulting_enemy_threat - initial_enemy_threat;

    // Offensive bonus applied on top of positional PST scoring
    int offensive_bonus = (offensive_threat_diff * OFFENSIVE_THREAT_WEIGHT) / 100;

    // =========================================================================
    // STAGE 6: Positional Piece-Square Table (PST) Delta & Piece Mobility
    // =========================================================================
    // Acts as tiebreakers for the previous stages
    PieceType type = get_piece_type(m->piece);
    int pst_diff = 0;
    int npm = get_board_non_pawn_material(brd);
    // Find difference in PST from previous square to destination square
    if (type == PIECE_KING && abs(m->to - m->from) == 2) {
        // Dual PST scoring for Castling moves
        int king_pst_diff = get_pst_val(PIECE_KING, m->to, npm) - get_pst_val(PIECE_KING, m->from, npm);
        int rook_from = (m->to > m->from) ? (m->to + 1) : (m->to - 2);
        int rook_to   = (m->to > m->from) ? (m->from + 1) : (m->from - 1);
        int rook_pst_diff = get_pst_val(PIECE_ROOK, rook_to, npm) - get_pst_val(PIECE_ROOK, rook_from, npm);
        pst_diff = king_pst_diff + rook_pst_diff + CASTLING_BONUS; // includes castling bonus
    }
    // None-castling moves
    else {
        int previous_pst_val = get_pst_val(type, m->from, npm);
        int new_pst_val = get_pst_val(type, m->to, npm);
        pst_diff = new_pst_val - previous_pst_val;
    }

    // Calculate mobility difference for pieces other than pawns and kings
    int mobility_bonus = 0;
    if (type == PIECE_KNIGHT || type == PIECE_BISHOP || type == PIECE_ROOK || type == PIECE_QUEEN) {
        int current_mobility = calculate_piece_mobility(brd, m->from, type, side);
        int new_mobility     = calculate_piece_mobility(temp_board, m->to, type, side);
        mobility_bonus       = (new_mobility - current_mobility) * MOBILITY_WEIGHT[type];
    }

    // =========================================================================
    // STAGE 7: Final Move Scoring & Tier Categorization
    // =========================================================================
    bool is_capture = (m->captured != PIECE_NONE);
    int positional_tactical_bonus = pst_diff + offensive_bonus + mobility_bonus;

    if (see_score >= 0) {
        // Category 1: Safe Moves (Blend SEE score and defensive threat reduction into Expected Value)
        int ev_score = see_score + threat_reduction;
        return 10000 + ev_score * EV_WEIGHT + positional_tactical_bonus;
    } else {
        if (is_capture) {
            // Category 3: Losing Captures (placed strictly AFTER quiet moves)
            return -20000 + (see_score + threat_reduction) * RISK_WEIGHT + positional_tactical_bonus;
        } else {
            // Category 2: Unsafe Quiet Moves (moves into undefended attack)
            return -10000 + (see_score + threat_reduction) * RISK_WEIGHT + positional_tactical_bonus;
        }
    }
}

// Select and execute the best move for side_to_move using 0-ply static move evaluation
void make_engine_move(Colour *side_to_move)
{
    long start_time = GetTickCount();
    Move moves[256];
    int move_count = generate_moves(board, *side_to_move, moves, ep_square);

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

        // Also handle En Passant capture on temp_board
        if (get_piece_type(m.piece) == PIECE_PAWN && get_file(m.from) != get_file(m.to) && board[m.to] == PIECE_NONE) {
            int ep_captured_sq = make_square(get_rank(m.from), get_file(m.to));
            if (is_on_board(ep_captured_sq)) {
                temp_board[ep_captured_sq] = PIECE_NONE;
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
        // Handle En Passant capture on main board before overwriting destination
        if (get_piece_type(best_move.piece) == PIECE_PAWN && get_file(best_move.from) != get_file(best_move.to) && board[best_move.to] == PIECE_NONE) {
            int ep_captured_sq = make_square(get_rank(best_move.from), get_file(best_move.to));
            if (is_on_board(ep_captured_sq)) {
                board[ep_captured_sq] = PIECE_NONE;
            }
        }

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

        // Update ep_square state
        if (get_piece_type(best_move.piece) == PIECE_PAWN && abs(get_rank(best_move.to) - get_rank(best_move.from)) == 2) {
            ep_square = (best_move.from + best_move.to) / 2;
        } else {
            ep_square = -1;
        }

        update_castling_rights(best_move.from, best_move.to);
        update_halfmove_clock(&best_move);

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
        push_position_history(board, *side_to_move, castling_rights);
    } else {
        printf("bestmove 0000\n");
    }

    fflush(stdout);
}

// Load PST values from a text file
// File format: 14 lines of 32 space-separated integers
// (7 lines for MG PSTs, PIECE_NONE..PIECE_QUEEN, followed by 7 lines for EG PSTs)
// Fallback: If only 7 lines are present, EG PSTs will be populated from MG PSTs.
int load_pst_from_file(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    for (int piece = 0; piece < 7; piece++) {
        for (int i = 0; i < 32; i++) {
            if (fscanf(f, "%d", &PST_MG[piece][i]) != 1) {
                fclose(f);
                return -1;
            }
        }
    }

    bool eg_loaded = true;
    for (int piece = 0; piece < 7; piece++) {
        for (int i = 0; i < 32; i++) {
            if (fscanf(f, "%d", &PST_EG[piece][i]) != 1) {
                eg_loaded = false;
                break;
            }
        }
        if (!eg_loaded) break;
    }

    if (!eg_loaded) {
        for (int piece = 0; piece < 7; piece++) {
            for (int i = 0; i < 32; i++) {
                PST_EG[piece][i] = PST_MG[piece][i];
            }
        }
    }

    fclose(f);
    return 0;
}

// Save PST values to a text file (14 lines of 32 space-separated integers: 7 MG lines, 7 EG lines)
int save_pst_to_file(const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) return -1;

    for (int piece = 0; piece < 7; piece++) {
        for (int i = 0; i < 32; i++) {
            fprintf(f, "%d%s", PST_MG[piece][i], (i == 31) ? "\n" : " ");
        }
    }

    for (int piece = 0; piece < 7; piece++) {
        for (int i = 0; i < 32; i++) {
            fprintf(f, "%d%s", PST_EG[piece][i], (i == 31) ? "\n" : " ");
        }
    }

    fclose(f);
    return 0;
}

// Evaluate all legal moves for current board position and output to stdout
// Output format: one line per legal move: "<uci_move> <score>"
// Ends with a blank line
void tune_evaluate_position(Colour side_to_move)
{
    Move moves[256];
    int move_count = generate_moves(board, side_to_move, moves, ep_square);

    for (int i = 0; i < move_count; i++) {
        Move m = moves[i];

        // Make move on temporary board to check legality
        int temp_board[BOARD_SIZE];
        memcpy(temp_board, board, sizeof(board));

        temp_board[m.from] = PIECE_NONE;
        int placed_piece = m.piece;
        if (m.promotion != PIECE_NONE) {
            placed_piece = side_to_move | m.promotion;
        }
        temp_board[m.to] = placed_piece;

        // Handle castling rook movement
        if (get_piece_type(m.piece) == PIECE_KING && abs(m.to - m.from) == 2) {
            if (m.to - m.from == 2) {
                temp_board[m.from + 1] = temp_board[m.to + 1];
                temp_board[m.to + 1] = PIECE_NONE;
            } else if (m.from - m.to == 2) {
                temp_board[m.from - 1] = temp_board[m.to - 2];
                temp_board[m.to - 2] = PIECE_NONE;
            }
        }

        // Skip illegal moves (leaves king in check)
        if (is_in_check(temp_board, side_to_move)) {
            continue;
        }

        int score = evaluate_move(board, &m);

        char from_str[4], to_str[4];
        square_to_algebraic(m.from, from_str);
        square_to_algebraic(m.to, to_str);

        const char *promo_str = "";
        if (m.promotion == PIECE_QUEEN)  promo_str = "q";
        if (m.promotion == PIECE_ROOK)   promo_str = "r";
        if (m.promotion == PIECE_BISHOP) promo_str = "b";
        if (m.promotion == PIECE_KNIGHT) promo_str = "n";

        printf("%s%s%s %d\n", from_str, to_str, promo_str, score);
    }
    printf("\n");
    fflush(stdout);
}

#ifdef TUNE_BUILD
// Output non-PST parameters and 384 PST parameters as UCI spin options
void print_uci_options(void)
{
    // Category 1: Material Values
    printf("option name Val_Pawn type spin default %d min 50 max 200\n", PIECE_VALUES[PIECE_PAWN]);
    printf("option name Val_Knight type spin default %d min 150 max 600\n", PIECE_VALUES[PIECE_KNIGHT]);
    printf("option name Val_Bishop type spin default %d min 150 max 600\n", PIECE_VALUES[PIECE_BISHOP]);
    printf("option name Val_Rook type spin default %d min 250 max 1000\n", PIECE_VALUES[PIECE_ROOK]);
    printf("option name Val_Queen type spin default %d min 500 max 2000\n", PIECE_VALUES[PIECE_QUEEN]);

    // Category 2: Positional & Tactical Heuristics
    printf("option name Castling_Bonus type spin default %d min 0 max 200\n", CASTLING_BONUS);
    printf("option name Offensive_Threat_Weight type spin default %d min 0 max 50\n", OFFENSIVE_THREAT_WEIGHT);

    // Category 3: Risk Weights
    printf("option name EV_Weight type spin default %d min 1 max 50\n", EV_WEIGHT);
    printf("option name Risk_Weight type spin default %d min 1 max 50\n", RISK_WEIGHT);

    // Per-Piece Mobility Bonuses
    printf("option name Mobility_Knight type spin default %d min 0 max 20\n", MOBILITY_WEIGHT[PIECE_KNIGHT]);
    printf("option name Mobility_Bishop type spin default %d min 0 max 20\n", MOBILITY_WEIGHT[PIECE_BISHOP]);
    printf("option name Mobility_Rook type spin default %d min 0 max 20\n", MOBILITY_WEIGHT[PIECE_ROOK]);
    printf("option name Mobility_Queen type spin default %d min 0 max 20\n", MOBILITY_WEIGHT[PIECE_QUEEN]);

    static const char *piece_names[] = {
        "", "Pawn", "King", "Knight", "Bishop", "Rook", "Queen"
    };

    for (int piece = 1; piece <= 6; piece++) {
        for (int phase = 0; phase < 2; phase++) {
            const char *phase_str = (phase == 0) ? "MG" : "EG";
            int (*pst_table)[32] = (phase == 0) ? PST_MG : PST_EG;

            for (int i = 0; i < 32; i++) {
                int r = i / 4;
                int f = i % 4;
                char sq_str[3] = { (char)('a' + f), (char)('8' - r), '\0' };

                int def_val = pst_table[piece][i];
                printf("option name %s_%s_%s type spin default %d min -200 max 200\n",
                       piece_names[piece], phase_str, sq_str, def_val);
            }
        }
    }
}

// Set a single parameter by UCI option name
bool set_uci_option(const char *name, int value)
{
    if (!name) return false;

    // Non-PST parameters
    if (strcmp(name, "Val_Pawn") == 0)               { PIECE_VALUES[PIECE_PAWN] = value; return true; }
    if (strcmp(name, "Val_Knight") == 0)             { PIECE_VALUES[PIECE_KNIGHT] = value; return true; }
    if (strcmp(name, "Val_Bishop") == 0)             { PIECE_VALUES[PIECE_BISHOP] = value; return true; }
    if (strcmp(name, "Val_Rook") == 0)               { PIECE_VALUES[PIECE_ROOK] = value; return true; }
    if (strcmp(name, "Val_Queen") == 0)              { PIECE_VALUES[PIECE_QUEEN] = value; return true; }
    if (strcmp(name, "Castling_Bonus") == 0)         { CASTLING_BONUS = value; return true; }
    if (strcmp(name, "Offensive_Threat_Weight") == 0){ OFFENSIVE_THREAT_WEIGHT = value; return true; }
    if (strcmp(name, "EV_Weight") == 0)               { EV_WEIGHT = value; return true; }
    if (strcmp(name, "Risk_Weight") == 0)             { RISK_WEIGHT = value; return true; }
    if (strcmp(name, "Mobility_Knight") == 0)         { MOBILITY_WEIGHT[PIECE_KNIGHT] = value; return true; }
    if (strcmp(name, "Mobility_Bishop") == 0)         { MOBILITY_WEIGHT[PIECE_BISHOP] = value; return true; }
    if (strcmp(name, "Mobility_Rook") == 0)           { MOBILITY_WEIGHT[PIECE_ROOK] = value; return true; }
    if (strcmp(name, "Mobility_Queen") == 0)          { MOBILITY_WEIGHT[PIECE_QUEEN] = value; return true; }

    static const char *piece_names[] = {
        "", "Pawn", "King", "Knight", "Bishop", "Rook", "Queen"
    };

    for (int piece = 1; piece <= 6; piece++) {
        for (int phase = 0; phase < 2; phase++) {
            const char *phase_str = (phase == 0) ? "MG" : "EG";
            int (*pst_table)[32] = (phase == 0) ? PST_MG : PST_EG;

            for (int i = 0; i < 32; i++) {
                int r = i / 4;
                int f = i % 4;
                char expected_name[64];
                snprintf(expected_name, sizeof(expected_name), "%s_%s_%c%c",
                         piece_names[piece], phase_str, (char)('a' + f), (char)('8' - r));

                if (strcmp(name, expected_name) == 0) {
                    pst_table[piece][i] = value;
                    return true;
                }
            }
        }
    }

    return false;
}
#endif

