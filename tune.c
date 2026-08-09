/*
 * tune.c
 *
 * Dedicated high-performance tuning binary for Magpie PST optimization.
 * Reads the aggregated dataset directly, loads PST values from a file,
 * evaluates all positions in a tight C loop, and outputs accuracy metrics.
 *
 * Usage:
 *   ./build/Magpie_tune <pst_file> <dataset_file> [sample_size] [sample_seed]
 *
 * Output (single line, machine-readable):
 *   <weighted_accuracy> <top1_accuracy> <positions_evaluated> <elapsed_ms>
 *
 * Dataset format (from 3_Aggregate_Positions.py):
 *   <FEN> | <move1> <count1> <move2> <count2> ...
 */

#include "magpie.h"
#include <sys/time.h>

static long get_tick_count_ms_tune(void) {
    struct timeval t;
    gettimeofday(&t, NULL);
    return t.tv_sec * 1000 + t.tv_usec / 1000;
}


/*
 * Parse a single dataset line:
 *   <FEN> | <move1> <count1> <move2> <count2> ...
 *
 * Returns:
 *   fen_end: pointer to end of FEN portion (for null-termination)
 *   moves[]: array of UCI move strings
 *   counts[]: array of counts
 *   num_moves: number of moves parsed
 */
#define MAX_DATASET_MOVES 64

typedef struct {
    char uci[8];
    int count;
} DatasetMove;

static int parse_dataset_line(char *line, char **fen_out, DatasetMove moves[], int max_moves)
{
    /* Find the " | " separator */
    char *sep = strstr(line, " | ");
    if (!sep) return 0;

    *sep = '\0';
    *fen_out = line;

    char *ptr = sep + 3;
    int num_moves = 0;

    while (*ptr && num_moves < max_moves) {
        /* Skip whitespace */
        while (*ptr == ' ' || *ptr == '\t') ptr++;
        if (*ptr == '\0' || *ptr == '\n' || *ptr == '\r') break;

        /* Read UCI move string */
        int mi = 0;
        while (*ptr && *ptr != ' ' && *ptr != '\t' && *ptr != '\n' && *ptr != '\r' && mi < 7) {
            moves[num_moves].uci[mi++] = *ptr++;
        }
        moves[num_moves].uci[mi] = '\0';

        /* Skip whitespace */
        while (*ptr == ' ' || *ptr == '\t') ptr++;

        /* Read count */
        int count = 0;
        while (*ptr >= '0' && *ptr <= '9') {
            count = count * 10 + (*ptr - '0');
            ptr++;
        }
        moves[num_moves].count = count;
        num_moves++;
    }

    return num_moves;
}

/*
 * Find the engine's best move for the current board position.
 * Returns the best move as a UCI string written to best_uci.
 */
static void find_best_move(Colour side_to_move, char *best_uci)
{
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
        const char *promo = "";
        if (best_move.promotion == PIECE_QUEEN)  promo = "q";
        if (best_move.promotion == PIECE_ROOK)   promo = "r";
        if (best_move.promotion == PIECE_BISHOP) promo = "b";
        if (best_move.promotion == PIECE_KNIGHT) promo = "n";
        sprintf(best_uci, "%s%s%s", from_str, to_str, promo);
    } else {
        strcpy(best_uci, "0000");
    }
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <pst_file> <dataset_file> [sample_size] [sample_seed]\n", argv[0]);
        fprintf(stderr, "\nEvaluates PST values against the aggregated dataset.\n");
        fprintf(stderr, "Output: <weighted_accuracy> <top1_accuracy> <positions_evaluated> <elapsed_ms>\n");
        return 1;
    }

    const char *pst_path = argv[1];
    const char *dataset_path = argv[2];
    int sample_size = 0;     /* 0 = use all positions */

    if (argc >= 4) sample_size = atoi(argv[3]);

    /* Load PST values */
    if (load_pst_from_file(pst_path) != 0) {
        fprintf(stderr, "Error: Failed to load PST from '%s'\n", pst_path);
        return 1;
    }

    /* Initialise terminal state (for zobrist etc.) */
    init_terminal();

    /* Open dataset */
    FILE *f = fopen(dataset_path, "r");
    if (!f) {
        fprintf(stderr, "Error: Failed to open dataset '%s'\n", dataset_path);
        return 1;
    }

    long start_ms = get_tick_count_ms_tune();

    double weighted_score_sum = 0.0;
    int top1_correct = 0;
    int positions_evaluated = 0;
    unsigned int line_index = 0;

    char line[65536];
    while (fgets(line, sizeof(line), f)) {
        /* Strip trailing whitespace */
        line[strcspn(line, "\r\n")] = '\0';
        if (strlen(line) == 0) continue;

        line_index++;

        /* Head-N sampling: stop after evaluating sample_size positions */
        if (sample_size > 0 && positions_evaluated >= sample_size) {
            break;
        }

        /* Parse the dataset line */
        char *fen;
        DatasetMove dataset_moves[MAX_DATASET_MOVES];
        int num_moves = parse_dataset_line(line, &fen, dataset_moves, MAX_DATASET_MOVES);
        if (num_moves == 0) continue;

        /* Compute total count for this position */
        int total_count = 0;
        for (int i = 0; i < num_moves; i++) {
            total_count += dataset_moves[i].count;
        }
        if (total_count == 0) continue;

        /* Set up position from FEN */
        Colour side = setup_fen(fen);

        /* Find engine's best move */
        char best_uci[16];
        find_best_move(side, best_uci);

        /* Check against dataset moves */
        int engine_move_count = 0;
        bool is_top1 = false;

        for (int i = 0; i < num_moves; i++) {
            if (strcmp(best_uci, dataset_moves[i].uci) == 0) {
                engine_move_count = dataset_moves[i].count;
                if (i == 0) is_top1 = true;
                break;
            }
        }

        double weight = (double)engine_move_count / (double)total_count;
        weighted_score_sum += weight;
        if (is_top1) top1_correct++;
        positions_evaluated++;
    }

    fclose(f);

    long elapsed_ms = get_tick_count_ms_tune() - start_ms;

    double weighted_accuracy = (positions_evaluated > 0) ? (weighted_score_sum / positions_evaluated) : 0.0;
    double top1_accuracy = (positions_evaluated > 0) ? ((double)top1_correct / positions_evaluated) : 0.0;

    /* Machine-readable output */
    printf("%.6f %.6f %d %ld\n", weighted_accuracy, top1_accuracy, positions_evaluated, elapsed_ms);
    fflush(stdout);

    return 0;
}
