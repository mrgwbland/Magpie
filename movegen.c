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

            // Pawn Captures
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
                        } else {
                            add_move(moves, &count, from, cap_to, piece, victim, PIECE_NONE);
                        }
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
