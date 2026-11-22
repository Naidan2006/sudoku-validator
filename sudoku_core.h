#ifndef SUDOKU_CORE_H
#define SUDOKU_CORE_H

#define SIZE 9
#define TOTAL_THREADS 11

/* Thread status values */
#define THREAD_WAITING 0
#define THREAD_RUNNING 1
#define THREAD_PASS    2
#define THREAD_FAIL    3
#define THREAD_ERROR   4

extern int sudoku[SIZE][SIZE];

/* results[0] = all rows, results[1] = all columns,
 * results[2..10] = 3x3 subgrids 1..9. 1 = PASS, 0 = FAIL */
extern int results[TOTAL_THREADS];

/* Per-row / per-column results (1 = PASS, 0 = FAIL).
 * Filled by the row and column threads, so the UI can tell
 * exactly which row or column is wrong. */
extern int row_ok[SIZE];
extern int col_ok[SIZE];

extern int thread_status[TOTAL_THREADS];

/* Wall-clock time of the last validate_sudoku() call, in milliseconds */
extern double last_validation_ms;

void set_thread_status(int index, int status);
int get_thread_status(int index);

/* Put all 11 thread states back to THREAD_WAITING and clear results */
void reset_validation_state(void);

void *check_rows(void *arg);
void *check_columns(void *arg);
void *check_subgrid(void *arg);

/* Returns 1 = valid, 0 = invalid, -1 = thread creation/join error */
int validate_sudoku(void);

#endif
