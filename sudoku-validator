
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
extern int results[TOTAL_THREADS];

extern int thread_status[TOTAL_THREADS];

void set_thread_status(int index, int status);
int get_thread_status(int index);

void *check_rows(void *arg);
void *check_columns(void *arg);
void *check_subgrid(void *arg);

int validate_sudoku(void);

#endif
