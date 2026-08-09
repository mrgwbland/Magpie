#include "magpie.h"

typedef struct {
    int dr;
    int df;
} Offset;

// Direction deltas for non-sliding and sliding pieces
static const Offset KNIGHT_OFFSETS[8] = {
    {-2, -1}, {-2, 1}, {-1, -2}, {-1, 2},
    { 1, -2}, { 1, 2}, { 2, -1}, { 2, 1}
};

static const Offset KING_OFFSETS[8] = {
    {-1, -1}, {-1, 0}, {-1, 1},
    { 0, -1},          { 0, 1},
    { 1, -1}, { 1, 0}, { 1, 1}
};

static const Offset BISHOP_OFFSETS[4] = {
    {-1, -1}, {-1, 1}, { 1, -1}, { 1, 1}
};

static const Offset ROOK_OFFSETS[4] = {
    {-1,  0}, { 1,  0}, { 0, -1}, { 0,  1}
};

// Helper to add a move to the move array
static inline void add_move(Move moves[], int *count, int from, int to, int piece, int captured, int promotion)
{
    moves[*count].from = from;
    moves[*count].to = to;
    moves[*count].piece = piece;
    moves[*count].captured = captured;
    moves[*count].promotion = promotion;
    (*count)++;
}

// Check if a square is attacked by any piece of attacker_colour
bool is_square_attacked(const int brd[BOARD_SIZE], int square, Colour attacker_colour)
{
    if (!is_on_board(square)) return false;

    int r = get_rank(square);
    int f = get_file(square);

    // 1. Pawn attacks
    int pawn_r = (attacker_colour == COLOUR_WHITE) ? r + 1 : r - 1;
    if (pawn_r >= 0 && pawn_r < 8) {
        int pawn_target = (int)(attacker_colour | PIECE_PAWN);
        if (f - 1 >= 0 && brd[make_square(pawn_r, f - 1)] == pawn_target) return true;
        if (f + 1 < 8  && brd[make_square(pawn_r, f + 1)] == pawn_target) return true;
    }

    // 2. Knight attacks
    for (int i = 0; i < 8; i++) {
        int nr = r + KNIGHT_OFFSETS[i].dr;
        int nf = f + KNIGHT_OFFSETS[i].df;
        if (nr >= 0 && nr < 8 && nf >= 0 && nf < 8) {
            if (brd[make_square(nr, nf)] == (int)(attacker_colour | PIECE_KNIGHT)) return true;
        }
    }

    // 3. King attacks
    for (int i = 0; i < 8; i++) {
        int nr = r + KING_OFFSETS[i].dr;
        int nf = f + KING_OFFSETS[i].df;
        if (nr >= 0 && nr < 8 && nf >= 0 && nf < 8) {
            if (brd[make_square(nr, nf)] == (int)(attacker_colour | PIECE_KING)) return true;
        }
    }

    // 4. Bishop & Queen diagonal attacks
    for (int i = 0; i < 4; i++) {
        int nr = r + BISHOP_OFFSETS[i].dr;
        int nf = f + BISHOP_OFFSETS[i].df;
        while (nr >= 0 && nr < 8 && nf >= 0 && nf < 8) {
            int target_sq = make_square(nr, nf);
            int p = brd[target_sq];
            if (p != PIECE_NONE) {
                if (get_piece_colour(p) == attacker_colour &&
                    (get_piece_type(p) == PIECE_BISHOP || get_piece_type(p) == PIECE_QUEEN)) {
                    return true;
                }
                break; // Blocked by piece
            }
            nr += BISHOP_OFFSETS[i].dr;
            nf += BISHOP_OFFSETS[i].df;
        }
    }

    // 5. Rook & Queen orthogonal attacks
    for (int i = 0; i < 4; i++) {
        int nr = r + ROOK_OFFSETS[i].dr;
        int nf = f + ROOK_OFFSETS[i].df;
        while (nr >= 0 && nr < 8 && nf >= 0 && nf < 8) {
            int target_sq = make_square(nr, nf);
            int p = brd[target_sq];
            if (p != PIECE_NONE) {
                if (get_piece_colour(p) == attacker_colour &&
                    (get_piece_type(p) == PIECE_ROOK || get_piece_type(p) == PIECE_QUEEN)) {
                    return true;
                }
                break; // Blocked by piece
            }
            nr += ROOK_OFFSETS[i].dr;
            nf += ROOK_OFFSETS[i].df;
        }
    }

    return false;
}

// Check if the given side's King is in check
bool is_in_check(const int brd[BOARD_SIZE], Colour side)
{
    int king_square = -1;
    int target_king = (int)(side | PIECE_KING);

    for (int sq = 0; sq < BOARD_SIZE; sq++) {
        if (brd[sq] == target_king) {
            king_square = sq;
            break;
        }
    }

    if (king_square == -1) return false;

    return is_square_attacked(brd, king_square, opponent_of(side));
}

// Generate pseudo-legal moves for side_to_move
int generate_moves(const int brd[BOARD_SIZE], Colour side_to_move, Move moves[])
{
    int count = 0;

    for (int from = 0; from < BOARD_SIZE; from++) {
        int piece = brd[from];
        if (piece == PIECE_NONE || get_piece_colour(piece) != side_to_move) continue;

        PieceType type = get_piece_type(piece);
        int r = get_rank(from);
        int f = get_file(from);

        // -------------------------------------------------------------------
        // Pawn Move Generation
        // -------------------------------------------------------------------
        if (type == PIECE_PAWN) {
            int forward_dr = (side_to_move == COLOUR_WHITE) ? -1 : 1;
            int start_rank = (side_to_move == COLOUR_WHITE) ? 6 : 1;
            int promo_rank = (side_to_move == COLOUR_WHITE) ? 0 : 7;

            // Single square push
            int next_r = r + forward_dr;
            if (next_r >= 0 && next_r < 8) {
                int to = make_square(next_r, f);
                if (brd[to] == PIECE_NONE) {
                    if (next_r == promo_rank) {
                        add_move(moves, &count, from, to, piece, PIECE_NONE, PIECE_QUEEN);
                        add_move(moves, &count, from, to, piece, PIECE_NONE, PIECE_ROOK);
                        add_move(moves, &count, from, to, piece, PIECE_NONE, PIECE_BISHOP);
                        add_move(moves, &count, from, to, piece, PIECE_NONE, PIECE_KNIGHT);
                    } else {
                        add_move(moves, &count, from, to, piece, PIECE_NONE, PIECE_NONE);

                        // Double square push from starting rank
                        if (r == start_rank) {
                            int double_r = r + (2 * forward_dr);
                            int double_to = make_square(double_r, f);
                            if (brd[double_to] == PIECE_NONE) {
                                add_move(moves, &count, from, double_to, piece, PIECE_NONE, PIECE_NONE);
                            }
                        }
                    }
                }
            }

            // Pawn Captures (Standard and En Passant)
            int cap_dfs[2] = { -1, 1 };
            for (int i = 0; i < 2; i++) {
                int cap_f = f + cap_dfs[i];
                int cap_r = r + forward_dr;
                if (cap_f >= 0 && cap_f < 8 && cap_r >= 0 && cap_r < 8) {
                    int cap_to = make_square(cap_r, cap_f);
                    int victim = brd[cap_to];
                    if (victim != PIECE_NONE && get_piece_colour(victim) != side_to_move) {
                        if (cap_r == promo_rank) {
                            add_move(moves, &count, from, cap_to, piece, victim, PIECE_QUEEN);
                            add_move(moves, &count, from, cap_to, piece, victim, PIECE_ROOK);
                            add_move(moves, &count, from, cap_to, piece, victim, PIECE_BISHOP);
                            add_move(moves, &count, from, cap_to, piece, victim, PIECE_KNIGHT);
                        } else {
                            add_move(moves, &count, from, cap_to, piece, victim, PIECE_NONE);
                        }
                    } else if (cap_to == ep_square && ep_square != -1) {
                        // En Passant Capture
                        int opp_pawn = (int)(opponent_of(side_to_move) | PIECE_PAWN);
                        add_move(moves, &count, from, cap_to, piece, opp_pawn, PIECE_NONE);
                    }
                }
            }
        }

        // -------------------------------------------------------------------
        // Knight Move Generation
        // -------------------------------------------------------------------
        else if (type == PIECE_KNIGHT) {
            for (int i = 0; i < 8; i++) {
                int nr = r + KNIGHT_OFFSETS[i].dr;
                int nf = f + KNIGHT_OFFSETS[i].df;
                if (nr >= 0 && nr < 8 && nf >= 0 && nf < 8) {
                    int to = make_square(nr, nf);
                    int victim = brd[to];
                    if (victim == PIECE_NONE || get_piece_colour(victim) != side_to_move) {
                        add_move(moves, &count, from, to, piece, victim, PIECE_NONE);
                    }
                }
            }
        }

        // -------------------------------------------------------------------
        // King Move Generation
        // -------------------------------------------------------------------
        else if (type == PIECE_KING) {
            for (int i = 0; i < 8; i++) {
                int nr = r + KING_OFFSETS[i].dr;
                int nf = f + KING_OFFSETS[i].df;
                if (nr >= 0 && nr < 8 && nf >= 0 && nf < 8) {
                    int to = make_square(nr, nf);
                    int victim = brd[to];
                    if (victim == PIECE_NONE || get_piece_colour(victim) != side_to_move) {
                        add_move(moves, &count, from, to, piece, victim, PIECE_NONE);
                    }
                }
            }

            // Castling Move Generation
            if (side_to_move == COLOUR_WHITE && from == 60) {
                if (!is_in_check(brd, COLOUR_WHITE)) {
                    // White Kingside Castling (e1g1)
                    if ((castling_rights & CASTLE_WK) &&
                        brd[61] == PIECE_NONE && brd[62] == PIECE_NONE &&
                        brd[63] == (COLOUR_WHITE | PIECE_ROOK) &&
                        !is_square_attacked(brd, 61, COLOUR_BLACK) &&
                        !is_square_attacked(brd, 62, COLOUR_BLACK)) {
                        add_move(moves, &count, 60, 62, piece, PIECE_NONE, PIECE_NONE);
                    }
                    // White Queenside Castling (e1c1)
                    if ((castling_rights & CASTLE_WQ) &&
                        brd[59] == PIECE_NONE && brd[58] == PIECE_NONE && brd[57] == PIECE_NONE &&
                        brd[56] == (COLOUR_WHITE | PIECE_ROOK) &&
                        !is_square_attacked(brd, 59, COLOUR_BLACK) &&
                        !is_square_attacked(brd, 58, COLOUR_BLACK)) {
                        add_move(moves, &count, 60, 58, piece, PIECE_NONE, PIECE_NONE);
                    }
                }
            } else if (side_to_move == COLOUR_BLACK && from == 4) {
                if (!is_in_check(brd, COLOUR_BLACK)) {
                    // Black Kingside Castling (e8g8)
                    if ((castling_rights & CASTLE_BK) &&
                        brd[5] == PIECE_NONE && brd[6] == PIECE_NONE &&
                        brd[7] == (COLOUR_BLACK | PIECE_ROOK) &&
                        !is_square_attacked(brd, 5, COLOUR_WHITE) &&
                        !is_square_attacked(brd, 6, COLOUR_WHITE)) {
                        add_move(moves, &count, 4, 6, piece, PIECE_NONE, PIECE_NONE);
                    }
                    // Black Queenside Castling (e8c8)
                    if ((castling_rights & CASTLE_BQ) &&
                        brd[3] == PIECE_NONE && brd[2] == PIECE_NONE && brd[1] == PIECE_NONE &&
                        brd[0] == (COLOUR_BLACK | PIECE_ROOK) &&
                        !is_square_attacked(brd, 3, COLOUR_WHITE) &&
                        !is_square_attacked(brd, 2, COLOUR_WHITE)) {
                        add_move(moves, &count, 4, 2, piece, PIECE_NONE, PIECE_NONE);
                    }
                }
            }
        }

        // -------------------------------------------------------------------
        // Sliding Piece Move Generation (Bishop, Rook, Queen)
        // -------------------------------------------------------------------
        else {
            const Offset *offsets = NULL;
            int num_offsets = 0;

            if (type == PIECE_BISHOP) {
                offsets = BISHOP_OFFSETS;
                num_offsets = 4;
            } else if (type == PIECE_ROOK) {
                offsets = ROOK_OFFSETS;
                num_offsets = 4;
            } else if (type == PIECE_QUEEN) {
                offsets = KING_OFFSETS; // 8 directions (4 diag + 4 ortho)
                num_offsets = 8;
            }

            for (int i = 0; i < num_offsets; i++) {
                int nr = r + offsets[i].dr;
                int nf = f + offsets[i].df;

                while (nr >= 0 && nr < 8 && nf >= 0 && nf < 8) {
                    int to = make_square(nr, nf);
                    int victim = brd[to];
                    if (victim == PIECE_NONE) {
                        add_move(moves, &count, from, to, piece, PIECE_NONE, PIECE_NONE);
                    } else {
                        if (get_piece_colour(victim) != side_to_move) {
                            add_move(moves, &count, from, to, piece, victim, PIECE_NONE);
                        }
                        break; // Blocked by piece
                    }
                    nr += offsets[i].dr;
                    nf += offsets[i].df;
                }
            }
        }
    }

    return count;
}

typedef struct {
    bool pinned;
    int dr;
    int df;
} PinInfo;

static void compute_pins(const int brd[BOARD_SIZE], Colour side, PinInfo pins[BOARD_SIZE])
{
    for (int i = 0; i < BOARD_SIZE; i++) {
        pins[i].pinned = false;
        pins[i].dr = 0;
        pins[i].df = 0;
    }

    int king_sq = -1;
    int target_king = (int)(side | PIECE_KING);
    for (int sq = 0; sq < BOARD_SIZE; sq++) {
        if (brd[sq] == target_king) {
            king_sq = sq;
            break;
        }
    }
    if (king_sq == -1) return;

    int kr = get_rank(king_sq);
    int kf = get_file(king_sq);
    Colour opp = opponent_of(side);

    // Scan all 8 directions from king
    for (int i = 0; i < 8; i++) {
        int dr = KING_OFFSETS[i].dr;
        int df = KING_OFFSETS[i].df;

        int r = kr + dr;
        int f = kf + df;
        int friendly_sq = -1;
        int friendly_count = 0;

        while (r >= 0 && r < 8 && f >= 0 && f < 8) {
            int sq = make_square(r, f);
            int p = brd[sq];

            if (p != PIECE_NONE) {
                if (get_piece_colour(p) == side) {
                    friendly_count++;
                    if (friendly_count == 1) {
                        friendly_sq = sq;
                    } else {
                        break; // 2+ friendly pieces along ray
                    }
                } else if (get_piece_colour(p) == opp) {
                    if (friendly_count == 1) {
                        PieceType pt = get_piece_type(p);
                        bool is_diag = (dr != 0 && df != 0);
                        if (is_diag && (pt == PIECE_BISHOP || pt == PIECE_QUEEN)) {
                            pins[friendly_sq].pinned = true;
                            pins[friendly_sq].dr = dr;
                            pins[friendly_sq].df = df;
                        } else if (!is_diag && (pt == PIECE_ROOK || pt == PIECE_QUEEN)) {
                            pins[friendly_sq].pinned = true;
                            pins[friendly_sq].dr = dr;
                            pins[friendly_sq].df = df;
                        }
                    }
                    break; // Enemy piece, end of ray scan
                }
            }

            r += dr;
            f += df;
        }
    }
}

static inline bool is_aligned(int fr, int ff, int tr, int tf, int pin_dr, int pin_df)
{
    int move_dr = tr - fr;
    int move_df = tf - ff;

    if (pin_dr == 0 && move_dr != 0) return false;
    if (pin_df == 0 && move_df != 0) return false;

    return (move_dr * pin_df == move_df * pin_dr);
}

// Static Exchange Evaluation (SEE) with hard pin awareness
int static_exchange_evaluation(const int brd[BOARD_SIZE], Move move)
{
    int from = move.from;
    int to = move.to;

    int moving_piece = move.piece;
    Colour side = get_piece_colour(moving_piece);

    // Initial victim value
    int captured_type = get_piece_type(move.captured);
    int initial_gain = PIECE_VALUES[captured_type];
    if (move.promotion != PIECE_NONE) {
        initial_gain += PIECE_VALUES[move.promotion] - PIECE_VALUES[PIECE_PAWN];
    }

    // Temporary board state for simulation
    int temp_board[BOARD_SIZE];
    memcpy(temp_board, brd, sizeof(int) * BOARD_SIZE);

    // Precalculate pins for BOTH sides
    PinInfo pins_white[BOARD_SIZE];
    PinInfo pins_black[BOARD_SIZE];
    compute_pins(temp_board, COLOUR_WHITE, pins_white);
    compute_pins(temp_board, COLOUR_BLACK, pins_black);

    int swap_list[32];
    int depth = 0;
    swap_list[depth++] = initial_gain;

    // Place moving piece on target square
    int current_piece = (move.promotion != PIECE_NONE) ? (int)(side | move.promotion) : moving_piece;
    temp_board[from] = PIECE_NONE;
    temp_board[to] = current_piece;

    // Handle En Passant capture on temp_board
    if (get_piece_type(moving_piece) == PIECE_PAWN && get_file(from) != get_file(to) && brd[to] == PIECE_NONE) {
        int ep_captured_sq = make_square(get_rank(from), get_file(to));
        if (is_on_board(ep_captured_sq)) {
            temp_board[ep_captured_sq] = PIECE_NONE;
        }
    }

    Colour curr_side = opponent_of(side);

    int to_r = get_rank(to);
    int to_f = get_file(to);

    while (depth < 32) {
        int least_val = 999999;
        int least_sq = -1;
        int least_piece = PIECE_NONE;

        const PinInfo *pins = (curr_side == COLOUR_WHITE) ? pins_white : pins_black;

        for (int sq = 0; sq < BOARD_SIZE; sq++) {
            int p = temp_board[sq];
            if (p == PIECE_NONE || get_piece_colour(p) != curr_side) continue;

            PieceType pt = get_piece_type(p);
            int sq_r = get_rank(sq);
            int sq_f = get_file(sq);

            // Filter out attackers that are hard-pinned off the target square's ray
            if (pins[sq].pinned) {
                if (!is_aligned(sq_r, sq_f, to_r, to_f, pins[sq].dr, pins[sq].df)) {
                    continue;
                }
            }

            bool attacks = false;

            if (pt == PIECE_PAWN) {
                int pawn_dr = (curr_side == COLOUR_WHITE) ? -1 : 1;
                if (to_r == sq_r + pawn_dr && (to_f == sq_f - 1 || to_f == sq_f + 1)) {
                    attacks = true;
                }
            } else if (pt == PIECE_KNIGHT) {
                int dr = abs(to_r - sq_r);
                int df = abs(to_f - sq_f);
                if ((dr == 1 && df == 2) || (dr == 2 && df == 1)) {
                    attacks = true;
                }
            } else if (pt == PIECE_KING) {
                int dr = abs(to_r - sq_r);
                int df = abs(to_f - sq_f);
                if (dr <= 1 && df <= 1) {
                    attacks = true;
                }
            } else {
                int dr = to_r - sq_r;
                int df = to_f - sq_f;
                bool is_diag = (abs(dr) == abs(df));
                bool is_ortho = (dr == 0 || df == 0);

                bool valid_dir = false;
                if (pt == PIECE_BISHOP && is_diag) valid_dir = true;
                if (pt == PIECE_ROOK && is_ortho) valid_dir = true;
                if (pt == PIECE_QUEEN && (is_diag || is_ortho)) valid_dir = true;

                if (valid_dir) {
                    int step_r = (dr > 0) ? 1 : ((dr < 0) ? -1 : 0);
                    int step_f = (df > 0) ? 1 : ((df < 0) ? -1 : 0);

                    int check_r = sq_r + step_r;
                    int check_f = sq_f + step_f;
                    bool blocked = false;

                    while (check_r != to_r || check_f != to_f) {
                        if (temp_board[make_square(check_r, check_f)] != PIECE_NONE) {
                            blocked = true;
                            break;
                        }
                        check_r += step_r;
                        check_f += step_f;
                    }

                    if (!blocked) attacks = true;
                }
            }

            if (attacks) {
                int val = PIECE_VALUES[pt];
                if (val < least_val) {
                    least_val = val;
                    least_sq = sq;
                    least_piece = p;
                }
            }
        }

        if (least_sq == -1) break;

        swap_list[depth] = PIECE_VALUES[get_piece_type(current_piece)];
        depth++;

        current_piece = least_piece;
        temp_board[least_sq] = PIECE_NONE;
        temp_board[to] = current_piece;

        curr_side = opponent_of(curr_side);
    }

    int gain[32];
    gain[depth - 1] = swap_list[depth - 1];

    for (int i = depth - 1; i > 0; i--) {
        int next_gain = (gain[i] > 0) ? gain[i] : 0;
        gain[i - 1] = swap_list[i - 1] - next_gain;
    }

    return gain[0];
}

// Evaluate maximum net material score opponent 'opp' can gain by capturing on target_sq
int see_square_for_opponent(const int brd[BOARD_SIZE], int target_sq, Colour opp)
{
    int victim_piece = brd[target_sq];
    if (victim_piece == PIECE_NONE) return 0;

    int initial_gain = PIECE_VALUES[get_piece_type(victim_piece)];

    int temp_board[BOARD_SIZE];
    memcpy(temp_board, brd, sizeof(int) * BOARD_SIZE);

    PinInfo pins_white[BOARD_SIZE];
    PinInfo pins_black[BOARD_SIZE];
    compute_pins(temp_board, COLOUR_WHITE, pins_white);
    compute_pins(temp_board, COLOUR_BLACK, pins_black);

    int swap_list[32];
    int depth = 0;

    Colour curr_side = opp;
    int current_piece = victim_piece;
    int to_r = get_rank(target_sq);
    int to_f = get_file(target_sq);

    while (depth < 32) {
        int least_val = 999999;
        int least_sq = -1;
        int least_piece = PIECE_NONE;

        const PinInfo *pins = (curr_side == COLOUR_WHITE) ? pins_white : pins_black;

        for (int sq = 0; sq < BOARD_SIZE; sq++) {
            int p = temp_board[sq];
            if (p == PIECE_NONE || get_piece_colour(p) != curr_side) continue;

            if (sq == target_sq) continue;

            PieceType pt = get_piece_type(p);
            int sq_r = get_rank(sq);
            int sq_f = get_file(sq);

            if (pins[sq].pinned) {
                if (!is_aligned(sq_r, sq_f, to_r, to_f, pins[sq].dr, pins[sq].df)) {
                    continue;
                }
            }

            bool attacks = false;

            if (pt == PIECE_PAWN) {
                int pawn_dr = (curr_side == COLOUR_WHITE) ? -1 : 1;
                if (to_r == sq_r + pawn_dr && (to_f == sq_f - 1 || to_f == sq_f + 1)) {
                    attacks = true;
                }
            } else if (pt == PIECE_KNIGHT) {
                int dr = abs(to_r - sq_r);
                int df = abs(to_f - sq_f);
                if ((dr == 1 && df == 2) || (dr == 2 && df == 1)) {
                    attacks = true;
                }
            } else if (pt == PIECE_KING) {
                int dr = abs(to_r - sq_r);
                int df = abs(to_f - sq_f);
                if (dr <= 1 && df <= 1) {
                    attacks = true;
                }
            } else {
                int dr = to_r - sq_r;
                int df = to_f - sq_f;
                bool is_diag = (abs(dr) == abs(df));
                bool is_ortho = (dr == 0 || df == 0);

                bool valid_dir = false;
                if (pt == PIECE_BISHOP && is_diag) valid_dir = true;
                if (pt == PIECE_ROOK && is_ortho) valid_dir = true;
                if (pt == PIECE_QUEEN && (is_diag || is_ortho)) valid_dir = true;

                if (valid_dir) {
                    int step_r = (dr > 0) ? 1 : ((dr < 0) ? -1 : 0);
                    int step_f = (df > 0) ? 1 : ((df < 0) ? -1 : 0);

                    int check_r = sq_r + step_r;
                    int check_f = sq_f + step_f;
                    bool blocked = false;

                    while (check_r != to_r || check_f != to_f) {
                        if (temp_board[make_square(check_r, check_f)] != PIECE_NONE) {
                            blocked = true;
                            break;
                        }
                        check_r += step_r;
                        check_f += step_f;
                    }

                    if (!blocked) attacks = true;
                }
            }

            if (attacks) {
                int val = PIECE_VALUES[pt];
                if (val < least_val) {
                    least_val = val;
                    least_sq = sq;
                    least_piece = p;
                }
            }
        }

        if (least_sq == -1) break;

        swap_list[depth] = (depth == 0) ? initial_gain : PIECE_VALUES[get_piece_type(current_piece)];
        depth++;

        current_piece = least_piece;
        temp_board[least_sq] = PIECE_NONE;
        temp_board[target_sq] = current_piece;

        curr_side = opponent_of(curr_side);
    }

    if (depth == 0) return 0;

    int gain[32];
    gain[depth - 1] = swap_list[depth - 1];

    for (int i = depth - 1; i > 0; i--) {
        int next_gain = (gain[i] > 0) ? gain[i] : 0;
        gain[i - 1] = swap_list[i - 1] - next_gain;
    }

    return (gain[0] > 0) ? gain[0] : 0;
}

// Compute total board material threat to friendly pieces of 'side', excluding except_sq and PIECE_KING
int evaluate_board_threat_except(const int brd[BOARD_SIZE], Colour side, int except_sq)
{
    Colour opp = opponent_of(side);
    int total_threat = 0;

    for (int sq = 0; sq < BOARD_SIZE; sq++) {
        if (sq == except_sq) continue;

        int p = brd[sq];
        if (p == PIECE_NONE || get_piece_colour(p) != side) continue;
        if (get_piece_type(p) == PIECE_KING) continue; //Would inflate score using actualy "king value"

        int opp_gain = see_square_for_opponent(brd, sq, opp);
        if (opp_gain > 0) {
            total_threat += opp_gain;
        }
    }

    return total_threat;
}

// Compute total board material threat to all friendly pieces of 'side'
int evaluate_board_threat(const int brd[BOARD_SIZE], Colour side)
{
    return evaluate_board_threat_except(brd, side, -1);
}


