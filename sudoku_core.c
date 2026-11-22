#include <pthread.h>
#include <time.h>
#include "sudoku_core.h"

int sudoku[SIZE][SIZE];
int results[TOTAL_THREADS];
int row_ok[SIZE];
int col_ok[SIZE];
int thread_status[TOTAL_THREADS];
double last_validation_ms = 0.0;

static pthread_mutex_t status_mutex = PTHREAD_MUTEX_INITIALIZER;

void set_thread_status(int index, int status)
{
    if (index < 0 || index >= TOTAL_THREADS)
        return;

    pthread_mutex_lock(&status_mutex);
    thread_status[index] = status;
    pthread_mutex_unlock(&status_mutex);
}

int get_thread_status(int index)
{
    int status;

    if (index < 0 || index >= TOTAL_THREADS)
        return THREAD_ERROR;

    pthread_mutex_lock(&status_mutex);
    status = thread_status[index];
    pthread_mutex_unlock(&status_mutex);

    return status;
}

void reset_validation_state(void)
{
    for (int i = 0; i < TOTAL_THREADS; i++) {
        results[i] = 0;
        set_thread_status(i, THREAD_WAITING);
    }

    for (int i = 0; i < SIZE; i++) {
        row_ok[i] = 0;
        col_ok[i] = 0;
    }

    last_validation_ms = 0.0;
}

/* Check all rows. Every row is checked (no early return), so
 * row_ok[] says exactly which rows are wrong. */
void *check_rows(void *arg)
{
    (void)arg;
    int all_ok = 1;

    set_thread_status(0, THREAD_RUNNING);

    for (int row = 0; row < SIZE; row++) {
        int seen[10] = {0};
        int ok = 1;

        for (int col = 0; col < SIZE; col++) {
            int value = sudoku[row][col];

            if (value < 1 || value > 9 || seen[value]) {
                ok = 0;
                break;
            }

            seen[value] = 1;
        }

        row_ok[row] = ok;

        if (!ok)
            all_ok = 0;
    }

    results[0] = all_ok;
    set_thread_status(0, all_ok ? THREAD_PASS : THREAD_FAIL);
    return NULL;
}

/* Check all columns (same idea, col_ok[] per column) */
void *check_columns(void *arg)
{
    (void)arg;
    int all_ok = 1;

    set_thread_status(1, THREAD_RUNNING);

    for (int col = 0; col < SIZE; col++) {
        int seen[10] = {0};
        int ok = 1;

        for (int row = 0; row < SIZE; row++) {
            int value = sudoku[row][col];

            if (value < 1 || value > 9 || seen[value]) {
                ok = 0;
                break;
            }

            seen[value] = 1;
        }

        col_ok[col] = ok;

        if (!ok)
            all_ok = 0;
    }

    results[1] = all_ok;
    set_thread_status(1, all_ok ? THREAD_PASS : THREAD_FAIL);
    return NULL;
}

/* Check one 3x3 subgrid */
void *check_subgrid(void *arg)
{
    int index = *(int *)arg;
    int status_index = index + 2;

    set_thread_status(status_index, THREAD_RUNNING);

    int start_row = (index / 3) * 3;
    int start_col = (index % 3) * 3;
    int seen[10] = {0};

    for (int row = start_row; row < start_row + 3; row++) {
        for (int col = start_col; col < start_col + 3; col++) {
            int value = sudoku[row][col];

            if (value < 1 || value > 9 || seen[value]) {
                results[status_index] = 0;
                set_thread_status(status_index, THREAD_FAIL);
                return NULL;
            }

            seen[value] = 1;
        }
    }

    results[status_index] = 1;
    set_thread_status(status_index, THREAD_PASS);
    return NULL;
}

static double elapsed_ms(const struct timespec *a, const struct timespec *b)
{
    return (double)(b->tv_sec - a->tv_sec) * 1000.0 +
           (double)(b->tv_nsec - a->tv_nsec) / 1000000.0;
}

/* Create and join 11 pthreads */
int validate_sudoku(void)
{
    pthread_t threads[TOTAL_THREADS];
    int subgrid_ids[9];
    int created = 0;
    int failed = 0;
    struct timespec t_start, t_end;

    reset_validation_state();
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    if (pthread_create(&threads[created], NULL, check_rows, NULL) != 0) {
        set_thread_status(0, THREAD_ERROR);
        return -1;
    }
    created++;

    if (pthread_create(&threads[created], NULL, check_columns, NULL) != 0) {
        set_thread_status(1, THREAD_ERROR);
        failed = 1;
    } else {
        created++;
    }

    for (int i = 0; i < 9 && !failed; i++) {
        subgrid_ids[i] = i;

        if (pthread_create(&threads[created], NULL,
                           check_subgrid, &subgrid_ids[i]) != 0) {
            set_thread_status(i + 2, THREAD_ERROR);
            failed = 1;
        } else {
            created++;
        }
    }

    /* Always join every thread that was created, even on failure */
    for (int i = 0; i < created; i++) {
        if (pthread_join(threads[i], NULL) != 0)
            failed = 1;
    }

    clock_gettime(CLOCK_MONOTONIC, &t_end);
    last_validation_ms = elapsed_ms(&t_start, &t_end);

    if (failed)
        return -1;

    for (int i = 0; i < TOTAL_THREADS; i++) {
        if (results[i] == 0)
            return 0;
    }

    return 1;
}
