#include "sense.h"

void thread_barrier_init(thread_barrier *barrier, int thread_barrier_number){
    pthread_mutex_init(&(barrier->lock), NULL);

    barrier->total_thread = 0;
    barrier->thread_barrier_number = thread_barrier_number;
    barrier->flag = false;
}

void thread_barrier_wait(thread_barrier *barrier){
    bool local_sense = barrier->flag;
    if(!pthread_mutex_lock(&(barrier->lock))){
        barrier->total_thread += 1;
        local_sense = !local_sense;
        
        if (barrier->total_thread == barrier->thread_barrier_number){
            barrier->total_thread = 0;
            barrier->flag = local_sense;
            pthread_mutex_unlock(&(barrier->lock));
        } else {
            pthread_mutex_unlock(&(barrier->lock));
            while (barrier->flag != local_sense); // wait for flag
        }
    }
}

void thread_barrier_destroy(thread_barrier *barrier){
    pthread_mutex_destroy(&(barrier->lock));
}

