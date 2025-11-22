#pragma once

#include <pthread.h>
#include <stdbool.h>

typedef struct _thread_barrier
{
    int thread_barrier_number;
    int total_thread;
    pthread_mutex_t lock;
    bool flag;
} thread_barrier;

void thread_barrier_init(thread_barrier *barrier, int thread_barrier_number);
void thread_barrier_wait(thread_barrier *barrier);
void thread_barrier_destroy(thread_barrier *barrier);

