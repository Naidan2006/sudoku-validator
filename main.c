/* Console version of the Sudoku validator.
 * The validation itself (11 pthreads) lives in sudoku_core.c and is
 * shared with the GTK GUI, so there is a single copy of the logic. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include "sudoku_core.h"

#define LINE_BUF 512

enum { PARSE_OK = 0, PARSE_NOT_INTEGER, PARSE_TOO_FEW, PARSE_TOO_MANY };

/* Reads one line. Returns 1 = ok, 0 = EOF, -1 = line too long. */
static int read_line(char *buf, size_t size)
{
    if (fgets(buf, (int)size, stdin) == NULL)
        return 0;

    size_t len = strlen(buf);

    if (len > 0 && buf[len - 1] == '\n') {
        buf[--len] = '\0';
        if (len > 0 && buf[len - 1] == '\r')
            buf[--len] = '\0';
        return 1;
    }

    if (feof(stdin))
        return 1; /* last line without a trailing newline */

    /* line did not fit: discard the rest of it */
    int c;
    while ((c = getchar()) != '\n' && c != EOF)
        ;
    return -1;
}

/* Parses exactly SIZE integers from one line.
 * Values outside 1..9 (or too large for long) are stored as 0 and flagged
 * in out_of_range[]; they make the grid INVALID but do not break parsing. */
static int parse_row(const char *line, int out[SIZE], int out_of_range[SIZE])
{
    const char *p = line;
    int count = 0;

    while (1) {
        while (isspace((unsigned char)*p))
            p++;

        if (*p == '\0')
            break;

        char *end;
        errno = 0;
        long v = strtol(p, &end, 10);

        if (end == p || (*end != '\0' && !isspace((unsigned char)*end)))
            return PARSE_NOT_INTEGER;

        if (count >= SIZE)
            return PARSE_TOO_MANY;

        if (errno == ERANGE || v < 1 || v > 9) {
            out[count] = 0;
            out_of_range[count] = 1;
        } else {
            out[count] = (int)v;
            out_of_range[count] = 0;
        }

        count++;
        p = end;
    }

    return count < SIZE ? PARSE_TOO_FEW : PARSE_OK;
}

/* Returns 1 on success, 0 on end of input */
static int read_sudoku(void)
{
    char line[LINE_BUF];

    printf("\nEnter 81 integers (1-9) for the Sudoku grid.\n");
    printf("Enter exactly 9 numbers per row, separated by spaces.\n\n");

    for (int row = 0; row < SIZE; row++) {
        int values[SIZE];
        int oor[SIZE];
        int ok = 0;

        while (!ok) {
            printf("Row %d: ", row + 1);
            fflush(stdout);

            int r = read_line(line, sizeof(line));

            if (r == 0) {
                printf("\nError: Input ended before all 9 rows were entered.\n");
                return 0;
            }

            if (r < 0) {
                printf("Error: Row %d is too long. Enter exactly 9 numbers.\n",
                       row + 1);
                continue;
            }

            switch (parse_row(line, values, oor)) {
            case PARSE_NOT_INTEGER:
                printf("Error: Row %d: please enter integers only. Try again.\n",
                       row + 1);
                break;
            case PARSE_TOO_FEW:
                printf("Error: Row %d must contain exactly 9 numbers (too few). Try again.\n",
                       row + 1);
                break;
            case PARSE_TOO_MANY:
                printf("Error: Row %d must contain exactly 9 numbers (too many). Try again.\n",
                       row + 1);
                break;
            default:
                ok = 1;
            }
        }

        for (int col = 0; col < SIZE; col++) {
            sudoku[row][col] = values[col];

            if (oor[col]) {
                printf("Warning: row %d, column %d is outside 1-9; "
                       "the grid will be INVALID.\n", row + 1, col + 1);
            }
        }
    }

    return 1;
}

/* Prints e.g. " (rows: 1, 4)" for every failing entry of ok[] */
static void print_failing(const char *label, const int ok[SIZE])
{
    int first = 1;

    for (int i = 0; i < SIZE; i++) {
        if (!ok[i]) {
            if (first)
                printf(" (%s: %d", label, i + 1);
            else
                printf(", %d", i + 1);
            first = 0;
        }
    }

    if (!first)
        printf(")");
}

static int run_validation(void)
{
    int valid = validate_sudoku();

    if (valid < 0) {
        printf("Error: Could not create or join the validation threads.\n");
        return 0;
    }

    printf("\n--- Validation Results ---\n");

    printf("Row check: %s", results[0] ? "PASS" : "FAIL");
    print_failing("rows", row_ok);
    printf("\n");

    printf("Column check: %s", results[1] ? "PASS" : "FAIL");
    print_failing("columns", col_ok);
    printf("\n");

    for (int i = 0; i < 9; i++) {
        printf("3x3 subgrid %d: %s\n", i + 1,
               results[i + 2] ? "PASS" : "FAIL");
    }

    printf("\nSudoku result: %s\n", valid ? "VALID" : "INVALID");
    return 1;
}

int main(void)
{
    char line[LINE_BUF];

    do {
        if (!read_sudoku())
            return 1;

        if (!run_validation())
            return 1;

        printf("\nCheck another Sudoku? (y/n): ");
        fflush(stdout);

        if (read_line(line, sizeof(line)) == 0)
            return 1;

    } while (line[0] == 'y' || line[0] == 'Y');

    printf("\nProgram finished.\n");
    return 0;
}
