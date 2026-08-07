#include "magpie.h"

// Piece values for material evaluation
const int PIECE_VALUES[] = {
    [PIECE_NONE]   = 0,
    [PIECE_PAWN]   = 100,
    [PIECE_KING]   = 20000,
    [PIECE_KNIGHT] = 300,
    [PIECE_BISHOP] = 320,
    [PIECE_ROOK]   = 500,
    [PIECE_QUEEN]  = 1000
};

// Global 64-square chess board & castling rights
int board[BOARD_SIZE];
unsigned int castling_rights = 0;

// Resets board to all empty squares
void init_board(void)
{
    memset(board, 0, sizeof(board));
    castling_rights = 0;
}

// Update castling rights when pieces move or are captured
void update_castling_rights(int from, int to)
{
    if (from == 60 || to == 60) castling_rights &= ~(CASTLE_WK | CASTLE_WQ);
    if (from == 4  || to == 4)  castling_rights &= ~(CASTLE_BK | CASTLE_BQ);
    if (from == 63 || to == 63) castling_rights &= ~CASTLE_WK;
    if (from == 56 || to == 56) castling_rights &= ~CASTLE_WQ;
    if (from == 7  || to == 7)  castling_rights &= ~CASTLE_BK;
    if (from == 0  || to == 0)  castling_rights &= ~CASTLE_BQ;
}

// Convert square index (0..63) to algebraic string (e.g., 60 -> "e1")
void square_to_algebraic(int square, char *out_str)
{
    if (!is_on_board(square) || !out_str) return;

    int file = get_file(square);
    int rank = get_rank(square);

    out_str[0] = 'a' + file;
    out_str[1] = '8' - rank;
    out_str[2] = '\0';
}

// Convert algebraic string to square index (0..63) (e.g., "e1" -> 60)
int algebraic_to_square(const char *str)
{
    if (!str || strlen(str) < 2) return -1;

    int file = str[0] - 'a';
    int rank = '8' - str[1];

    if (file < 0 || file > 7 || rank < 0 || rank > 7) {
        return -1;
    }

    return make_square(rank, file);
}

// Parse FEN string and populate board array. Returns active side to move.
Colour setup_fen(const char *fen)
{
    init_board();

    if (!fen) return COLOUR_WHITE;

    int square = 0; // Starts at rank 8, file A (index 0)
    const char *ptr = fen;

    // 1. Parse piece placement
    while (*ptr && *ptr != ' ') {
        char ch = *ptr++;

        if (ch == '/') {
            // Jump to start of next rank
            if (square % 8 != 0) {
                square = (square / 8 + 1) * 8;
            }
        } else if (ch >= '1' && ch <= '8') {
            // Skip empty squares
            square += (ch - '0');
        } else {
            // Parse piece character
            Colour colour = (ch >= 'A' && ch <= 'Z') ? COLOUR_WHITE : COLOUR_BLACK;
            char piece_char = (ch >= 'A' && ch <= 'Z') ? ch + ('a' - 'A') : ch;

            PieceType type = PIECE_NONE;
            switch (piece_char) {
                case 'p': type = PIECE_PAWN;   break;
                case 'n': type = PIECE_KNIGHT; break;
                case 'b': type = PIECE_BISHOP; break;
                case 'r': type = PIECE_ROOK;   break;
                case 'q': type = PIECE_QUEEN;  break;
                case 'k': type = PIECE_KING;   break;
                default:                       break;
            }

            if (type != PIECE_NONE && is_on_board(square)) {
                board[square] = colour | type;
                square++;
            }
        }
    }

    // 2. Parse active colour
    while (*ptr == ' ') ptr++;
    Colour side_to_move = (*ptr == 'b') ? COLOUR_BLACK : COLOUR_WHITE;
    if (*ptr) ptr++;

    // 3. Parse castling availability
    while (*ptr == ' ') ptr++;
    while (*ptr && *ptr != ' ') {
        if (*ptr == 'K') castling_rights |= CASTLE_WK;
        if (*ptr == 'Q') castling_rights |= CASTLE_WQ;
        if (*ptr == 'k') castling_rights |= CASTLE_BK;
        if (*ptr == 'q') castling_rights |= CASTLE_BQ;
        ptr++;
    }

    return side_to_move;
}
