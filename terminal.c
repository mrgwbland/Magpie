#include "magpie.h"

// Zobrist 64-bit keys
static uint64_t zobrist_pieces[64][25];
static uint64_t zobrist_side;
static uint64_t zobrist_castling[16];
static bool zobrist_initialized = false;

// Position History & Halfmove Clock
#define MAX_HISTORY 1024
static uint64_t history_hashes[MAX_HISTORY];
static int history_count = 0;
static int halfmove_clock = 0;

// Simple LCG PRNG for reproducible Zobrist keys
static uint64_t rand64(void)
{
    static uint64_t seed = 1070372ULL;
    seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
    return seed;
}

// Initialize Zobrist random keys
void init_terminal(void)
{
    if (zobrist_initialized) return;

    for (int sq = 0; sq < 64; sq++) {
        for (int p = 0; p < 25; p++) {
            zobrist_pieces[sq][p] = rand64();
        }
    }
    zobrist_side = rand64();
    for (int c = 0; c < 16; c++) {
        zobrist_castling[c] = rand64();
    }

    zobrist_initialized = true;
}

// Compute 64-bit Zobrist Hash of a position
static uint64_t compute_zobrist_hash(const int brd[BOARD_SIZE], Colour side, unsigned int castling)
{
    init_terminal();
    uint64_t hash = 0ULL;

    for (int sq = 0; sq < BOARD_SIZE; sq++) {
        int piece = brd[sq];
        if (piece != PIECE_NONE && piece < 25) {
            hash ^= zobrist_pieces[sq][piece];
        }
    }

    if (side == COLOUR_BLACK) {
        hash ^= zobrist_side;
    }

    hash ^= zobrist_castling[castling & 15];

    return hash;
}

// Clear position history and halfmove clock
void clear_terminal_history(void)
{
    history_count = 0;
    halfmove_clock = 0;
}

// Record current board position into history
void push_position_history(const int brd[BOARD_SIZE], Colour side, unsigned int castling)
{
    if (history_count < MAX_HISTORY) {
        history_hashes[history_count++] = compute_zobrist_hash(brd, side, castling);
    }
}

// Update halfmove clock based on executed move
void update_halfmove_clock(const Move *m)
{
    if (m->captured != PIECE_NONE || get_piece_type(m->piece) == PIECE_PAWN) {
        halfmove_clock = 0;
    } else {
        halfmove_clock++;
    }
}

// Helper to count legal responses for a side on a board state
static int count_legal_moves(const int temp_board[BOARD_SIZE], Colour side)
{
    Move opp_moves[256];
    int count = generate_moves(temp_board, side, opp_moves);
    int legal_count = 0;

    for (int i = 0; i < count; i++) {
        Move m = opp_moves[i];

        int temp_board2[BOARD_SIZE];
        memcpy(temp_board2, temp_board, sizeof(temp_board2));

        temp_board2[m.from] = PIECE_NONE;
        int placed_piece = (m.promotion != PIECE_NONE) ? (int)(side | m.promotion) : m.piece;
        temp_board2[m.to] = placed_piece;

        // Move Rook on temp_board2 if castling
        if (get_piece_type(m.piece) == PIECE_KING && abs(m.to - m.from) == 2) {
            if (m.to - m.from == 2) {
                temp_board2[m.from + 1] = temp_board2[m.to + 1];
                temp_board2[m.to + 1] = PIECE_NONE;
            } else if (m.from - m.to == 2) {
                temp_board2[m.from - 1] = temp_board2[m.to - 2];
                temp_board2[m.to - 2] = PIECE_NONE;
            }
        }

        if (!is_in_check(temp_board2, side)) {
            legal_count++;
            break; // We only need to know if at least 1 legal move exists
        }
    }

    return legal_count;
}

// 1-Ply Terminal Checker: evaluates candidate move m for Mate, Stalemate, 3-Fold Repetition, and 50-Move Rule
bool is_terminal_move(const int brd[BOARD_SIZE], const Move *m, int *out_score)
{
    Colour my_side = get_piece_colour(m->piece);
    Colour opp_side = opponent_of(my_side);

    // 1. Simulate candidate move m on temporary board copy
    int temp_board[BOARD_SIZE];
    memcpy(temp_board, brd, sizeof(temp_board));

    temp_board[m->from] = PIECE_NONE;
    int placed_piece = (m->promotion != PIECE_NONE) ? (int)(my_side | m->promotion) : m->piece;
    temp_board[m->to] = placed_piece;

    // Move Rook if castling
    if (get_piece_type(m->piece) == PIECE_KING && abs(m->to - m->from) == 2) {
        if (m->to - m->from == 2) {
            temp_board[m->from + 1] = temp_board[m->to + 1];
            temp_board[m->to + 1] = PIECE_NONE;
        } else if (m->from - m->to == 2) {
            temp_board[m->from - 1] = temp_board[m->to - 2];
            temp_board[m->to - 2] = PIECE_NONE;
        }
    }

    // 2. Count opponent legal responses
    bool opp_in_check = is_in_check(temp_board, opp_side);
    int opp_legal_moves = count_legal_moves(temp_board, opp_side);

    // -------------------------------------------------------------------
    // A. Checkmate (+100,000 Instant Win)
    // -------------------------------------------------------------------
    if (opp_in_check && opp_legal_moves == 0) {
        *out_score = 100000;
        return true;
    }

    // -------------------------------------------------------------------
    // B. Stalemate (Draw score 0)
    // -------------------------------------------------------------------
    if (!opp_in_check && opp_legal_moves == 0) {
        *out_score = 0;
        return true;
    }

    // Calculate candidate castling rights after move
    unsigned int cand_castling = castling_rights;
    if (m->from == 60 || m->to == 60) cand_castling &= ~(CASTLE_WK | CASTLE_WQ);
    if (m->from == 4  || m->to == 4)  cand_castling &= ~(CASTLE_BK | CASTLE_BQ);
    if (m->from == 63 || m->to == 63) cand_castling &= ~CASTLE_WK;
    if (m->from == 56 || m->to == 56) cand_castling &= ~CASTLE_WQ;
    if (m->from == 7  || m->to == 7)  cand_castling &= ~CASTLE_BK;
    if (m->from == 0  || m->to == 0)  cand_castling &= ~CASTLE_BQ;

    // Calculate Zobrist hash of resulting position
    uint64_t cand_hash = compute_zobrist_hash(temp_board, opp_side, cand_castling);

    // -------------------------------------------------------------------
    // C. Threefold Repetition
    // -------------------------------------------------------------------
    int rep_count = 0;
    for (int i = 0; i < history_count; i++) {
        if (history_hashes[i] == cand_hash) {
            rep_count++;
        }
    }
    if (rep_count >= 2) {
        *out_score = 0; // Draw
        return true;
    }

    // -------------------------------------------------------------------
    // D. 50-Move Rule
    // -------------------------------------------------------------------
    int cand_halfmove = (m->captured != PIECE_NONE || get_piece_type(m->piece) == PIECE_PAWN) ? 0 : (halfmove_clock + 1);
    if (cand_halfmove >= 100) {
        *out_score = 0; // Draw
        return true;
    }

    return false;
}
