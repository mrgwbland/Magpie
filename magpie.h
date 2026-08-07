#ifndef MAGPIE_H
#define MAGPIE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>


/*
 * ---------------------------------------------------------------------------
 * Board Representation & Constants
 * ---------------------------------------------------------------------------
 * We use a 0x88 board representation (128-element array).
 * Squares 0..119 (where (sq & 0x88) == 0) are valid board squares.
 * Rank 8 is row 0 (squares 0..7), Rank 1 is row 7 (squares 112..119).
 */

#define BOARD_SIZE 64

// Colours
typedef enum {
    COLOUR_NONE  = 0,
    COLOUR_WHITE = 8,
    COLOUR_BLACK = 16,
    COLOUR_BOTH  = 24
} Colour;

// Piece Types
typedef enum {
    PIECE_NONE   = 0,
    PIECE_PAWN   = 1,
    PIECE_KING   = 2,
    PIECE_KNIGHT = 3,
    PIECE_BISHOP = 4,
    PIECE_ROOK   = 5,
    PIECE_QUEEN  = 6
} PieceType;

// Piece values used in material evaluation
extern const int PIECE_VALUES[];

// Board state array (64-element representation)
extern int board[BOARD_SIZE];

// Move Structure
typedef struct {
    int from;           // Source square index (0..63)
    int to;             // Destination square index (0..63)
    int piece;          // Moving piece (colour | piece_type)
    int captured;       // Captured piece, if any
    int promotion;      // Promotion piece type (PIECE_QUEEN, etc.), if any
} Move;

// ---------------------------------------------------------------------------
// Helper Inline Functions
// ---------------------------------------------------------------------------

// Check if a square index is on the board
static inline bool is_on_board(int square) {
    return square >= 0 && square < 64;
}

// Extract piece colour from a piece value
static inline Colour get_piece_colour(int piece) {
    return (Colour)(piece & COLOUR_BOTH);
}

// Extract piece type from a piece value
static inline PieceType get_piece_type(int piece) {
    return (PieceType)(piece & 7);
}

// Convert rank and file (0..7) to square index (0..63)
static inline int make_square(int rank, int file) {
    return rank * 8 + file;
}

// Extract rank (0..7) from square index (Rank 8 = 0, Rank 1 = 7)
static inline int get_rank(int square) {
    return square / 8;
}

// Extract file (0..7) from square index (File A = 0, File H = 7)
static inline int get_file(int square) {
    return square % 8;
}

// Get opposing colour
static inline Colour opponent_of(Colour colour) {
    return (colour == COLOUR_WHITE) ? COLOUR_BLACK : COLOUR_WHITE;
}

// ---------------------------------------------------------------------------
// Function Declarations
// ---------------------------------------------------------------------------

// board.c
void init_board(void);
Colour setup_fen(const char *fen);
void square_to_algebraic(int square, char *out_str);
int algebraic_to_square(const char *str);

// movegen.c
int generate_moves(const int brd[BOARD_SIZE], Colour side_to_move, Move moves[]);
bool is_in_check(const int brd[BOARD_SIZE], Colour side);
bool is_square_attacked(const int brd[BOARD_SIZE], int square, Colour attacker_colour);
int static_exchange_evaluation(const int brd[BOARD_SIZE], Move move);

// play.c
int evaluate_move(const int brd[BOARD_SIZE], const Move *m);
void make_engine_move(Colour *side_to_move);

// uci.c
void apply_move_string(const char *move_str, Colour *side_to_move);
void run_uci(void);

#endif // MAGPIE_H
