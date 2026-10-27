
#include <pthread.h>
#include "sudoku_core.h"

int sudoku[SIZE][SIZE];
int results[TOTAL_THREADS];
int thread_status[TOTAL_THREADS];

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
    int status = THREAD_ERROR;

    if (index < 0 || index >= TOTAL_THREADS)
        return THREAD_ERROR;

    pthread_mutex_lock(&status_mutex);
    status = thread_status[index];
    pthread_mutex_unlock(&status_mutex);

    return status;
}

/* Check all rows */
void *check_rows(void *arg)
{
    (void)arg;
    set_thread_status(0, THREAD_RUNNING);

    for (int row = 0; row < SIZE; row++) {
        int seen[10] = {0};

        for (int col = 0; col < SIZE; col++) {
            int value = sudoku[row][col];

            if (value < 1 || value > 9 || seen[value]) {
                results[0] = 0;
                set_thread_status(0, THREAD_FAIL);
                return NULL;
            }

            seen[value] = 1;
        }
    }

    results[0] = 1;
    set_thread_status(0, THREAD_PASS);
    return NULL;
}

/* Check all columns */
void *check_columns(void *arg)
{
    (void)arg;
    set_thread_status(1, THREAD_RUNNING);

    for (int col = 0; col < SIZE; col++) {
        int seen[10] = {0};

        for (int row = 0; row < SIZE; row++) {
            int value = sudoku[row][col];

            if (value < 1 || value > 9 || seen[value]) {
                results[1] = 0;
                set_thread_status(1, THREAD_FAIL);
                return NULL;
            }

            seen[value] = 1;
        }
    }

    results[1] = 1;
    set_thread_status(1, THREAD_PASS);
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

/* Create and join 11 pthreads */
int validate_sudoku(void)
{
    pthread_t threads[TOTAL_THREADS];
    int subgrid_ids[9];
    int created = 0;

    for (int i = 0; i < TOTAL_THREADS; i++) {
        results[i] = 0;
        set_thread_status(i, THREAD_WAITING);
    }

    if (pthread_create(&threads[0], NULL, check_rows, NULL) != 0) {
        set_thread_status(0, THREAD_ERROR);
        return -1;
    }
    created++;

    if (pthread_create(&threads[1], NULL, check_columns, NULL) != 0) {
        set_thread_status(1, THREAD_ERROR);

        for (int i = 0; i < created; i++)
            pthread_join(threads[i], NULL);

        return -1;
    }
    created++;

    for (int i = 0; i < 9; i++) {
        subgrid_ids[i] = i;

        if (pthread_create(
                &threads[i + 2],
                NULL,
                check_subgrid,
                &subgrid_ids[i]) != 0) {

            set_thread_status(i + 2, THREAD_ERROR);

            for (int j = 0; j < created; j++)
                pthread_join(threads[j], NULL);

            return -1;
        }

        created++;
    }

    for (int i = 0; i < TOTAL_THREADS; i++) {
        if (pthread_join(threads[i], NULL) != 0)
            return -1;
    }

    for (int i = 0; i < TOTAL_THREADS; i++) {
        if (results[i] == 0)
            return 0;
    }

    return 1;
}
