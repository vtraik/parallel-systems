#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include "sense.h"

typedef enum{
    PTHREAD,
    CONDVAR,
    SENSE
} CHOICE;

void error_exit(int errnum, const char* mes){
    char buf[256];
    strerror_r(errnum, buf, sizeof(buf));
    fprintf(stderr, "%s: %s\n", mes,buf);
    exit(EXIT_FAILURE);
}

int iterations = 0;
pthread_barrier_t barrier;
pthread_mutex_t barrier_mutex;
pthread_cond_t ok_to_proceed;
thread_barrier sense_barrier;
int threads;
int barrier_thread_count = 0;



void* b_pthread(void* args){
    int iter = *(int*) args;
    int errnu;
    for(int i=0; i<iter; i++){
        errnu = pthread_barrier_wait(&barrier);
        if(errnu != 0 && errnu != PTHREAD_BARRIER_SERIAL_THREAD)
            error_exit(errnu,"pthread_barrier_wait");
    }

    return NULL;
}

void* b_condvar(void* args){
    int iter = *(int*) args; 
    int errnu;
    for (int i = 0; i < iter; i++) {
      if((errnu = pthread_mutex_lock(&barrier_mutex)) != 0)
          error_exit(errnu,"pthread_mutex_lock");
      barrier_thread_count++;
      if (barrier_thread_count == threads) {
         barrier_thread_count = 0;
         if((errnu = pthread_cond_broadcast(&ok_to_proceed)) != 0)
             error_exit(errnu,"pthread_cond_broadcast");
      } else {
         while (pthread_cond_wait(&ok_to_proceed,
                   &barrier_mutex) != 0);
      }
      if((errnu = pthread_mutex_unlock(&barrier_mutex)) != 0)
          error_exit(errnu,"pthread_mutex_unlock");
    }
    return NULL;
}

void* b_senserev(void* args){
    int iter = *(int*) args;
    for(int i=0; i<iter; i++){
        thread_barrier_wait(&sense_barrier);
    }

    return NULL;
}

void parallel_exec(pthread_t* thread, int threads, CHOICE choice){
    void* (*funct)(void*);
    char mes[8];
    if(choice == PTHREAD){
        funct = b_pthread;
        strcpy(mes,"Pthread");
    }else if(choice == CONDVAR){
        funct = b_condvar;
        strcpy(mes,"Condvar");
    }else{
        funct = b_senserev;
        strcpy(mes,"Sense");
    }

    struct timespec start_t, end_t;
    clock_gettime(CLOCK_MONOTONIC, &start_t);
    int errnu;
    for(int i=0; i<threads; i++){
        if((errnu = pthread_create(&thread[i],NULL,funct,&iterations)) != 0)
            error_exit(errnu,"pthread_create");
    }

    for(int i=0; i<threads; i++){
        if((errnu = pthread_join(thread[i],NULL)) != 0)
            error_exit(errnu,"pthread_join");
    }
    clock_gettime(CLOCK_MONOTONIC, &end_t);
    double total = (end_t.tv_sec - start_t.tv_sec)
            + (end_t.tv_nsec - start_t.tv_nsec) / 1e9;
    printf("%s time: %.9f\n",mes,total);

}

int main(int argc, const char** argv){
    if(argc != 5){
        fprintf(stderr,"Usage: ./%s -t <num of threads> -i <num of iterations>\n",argv[0]);
    }
    threads = atoi(argv[2]);
    iterations = atoi(argv[4]);
    pthread_t thread[threads];

    int errnu;
    if((errnu = pthread_barrier_init(&barrier,NULL,threads)) != 0)
        error_exit(errnu,"pthread_barrier_init");
    if((errnu = pthread_mutex_init(&barrier_mutex,NULL)) != 0)
        error_exit(errnu,"pthread_mutex_init");
    if((errnu = pthread_cond_init(&ok_to_proceed,NULL)) != 0)
        error_exit(errnu,"pthread_cond_init");
    thread_barrier_init(&sense_barrier,threads);

    parallel_exec(thread, threads, PTHREAD);
    parallel_exec(thread, threads, CONDVAR);
    parallel_exec(thread, threads, SENSE);

    if((errnu = pthread_barrier_destroy(&barrier)) != 0)
        error_exit(errnu,"pthread_barrier_destroy");
    if((errnu = pthread_mutex_destroy(&barrier_mutex)) != 0)
        error_exit(errnu,"pthread_mutex_destroy");
    if((errnu = pthread_cond_destroy(&ok_to_proceed)) != 0)
        error_exit(errnu,"pthread_cond_destroy");
    thread_barrier_destroy(&sense_barrier);
}
