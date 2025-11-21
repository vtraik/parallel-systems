#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include "wrappers.h"

void error_exit(int errnum, const char* mes){
    char buf[256];
    strerror_r(errnum, buf, sizeof(buf));   // use rc, not errno
    fprintf(stderr, "%s: %s\n", mes,buf);
  exit(EXIT_FAILURE);
}

void** locks = NULL;
int locks_size = 0;
int* acc_balances = NULL;
int acc_size = 0;
int ntransfers = 0;
int perc = 0;

void* thread_work(void* args){
    TYPE type = *(TYPE*) args;
    int sum = 0;
    int errnu;
    for(int i=0; i<ntransfers; i++){
        // choose type
        if((int)arc4random_uniform(100) < perc){ // query
            int pos = acc_size == 1 ? 0 : arc4random_uniform(acc_size-1);
            int pos_lock = locks_size == 1 ? 0 : pos;
            if((errnu = lock(locks[pos_lock],type,READ)) != 0)
                error_exit(errnu,"lock");
            /* usleep(500); */
            sum += acc_balances[pos];
            if((errnu = unlock(locks[pos_lock],type)) != 0)
                error_exit(errnu,"unlock");
        }else{ // transaction
            int money;
            int pos_sender;
            int pos_lock_s;
            do{
                money = 0;
                pos_sender = acc_size == 1 ? 0 : arc4random_uniform(acc_size-1);
                pos_lock_s = locks_size == 1 ? 0 : pos_sender; 
                if((errnu = lock(locks[pos_lock_s],type,READ)) != 0)
                    error_exit(errnu,"lock");
                money = acc_balances[pos_sender];
                if((errnu = unlock(locks[pos_lock_s],type)) != 0)
                    error_exit(errnu,"unlock");
            }while(money == 0);
            int amount = arc4random_uniform(money);
            if((errnu = lock(locks[pos_lock_s],type,WRITE)) != 0)
                error_exit(errnu,"lock");
            acc_balances[pos_sender] -= amount;
            if((errnu = unlock(locks[pos_lock_s],type)) != 0)
                error_exit(errnu,"unlock");

            int pos_rec = acc_size == 1 ? 0 : arc4random_uniform(acc_size-1);
            int pos_lock_r = locks_size == 1 ? 0 : pos_rec;
            if((errnu = lock(locks[pos_lock_r],type,WRITE)) != 0)
                error_exit(errnu,"lock");
            acc_balances[pos_rec] += amount;
            if((errnu = unlock(locks[pos_lock_r],type)) != 0)
                error_exit(errnu,"unlock");
             
        }

    }

    return NULL;
}

int main(int argc, const char** argv){
    if(argc != 13){
        fprintf(stderr,
                "Usage: ./%s -n <arr_size> -t <num of transfers/thread>\
                -q <Percentage of query transactions:1..100>\
                -thr <number of threads> -type <lock type:m,rw>\
                -l <type of locking:f,c>\n",argv[0]);
    }

    acc_size = atoi(argv[2]);
    ntransfers = atoi(argv[4]);
    perc = atoi(argv[6]);
    const char* type = argv[10];
    int threads = atoi(argv[8]);
    const char* locking_type = argv[12];
    acc_balances = malloc(acc_size*sizeof(int));


    // init
    int starting_sum = 0;
    for(int i=0; i<acc_size; i++){
        acc_balances[i] = arc4random_uniform(10000);
        starting_sum += acc_balances[i];
    }



    if(strcmp(locking_type,"f") == 0){
        locks = malloc(acc_size*sizeof(void*));
        locks_size = acc_size;
    }else if(strcmp(locking_type,"c") == 0){
        locks = malloc(sizeof(void*));
        locks_size = 1;
    }

    TYPE lock_type;
    int errnu;
    if(strcmp(type,"m") == 0){
        lock_type = MUTEX_LOCK;
        for(int i=0; i<locks_size; i++){
            locks[i] = malloc(sizeof(pthread_mutex_t));
            if((errnu = pthread_mutex_init(locks[i],NULL)) != 0)
                error_exit(errnu,"pthread_mutex_init");
        }
    }else if(strcmp(type,"rw") == 0){
        lock_type = RW_LOCK;
        for(int i=0; i<locks_size; i++){
            locks[i] = malloc(sizeof(pthread_rwlock_t));
            if((errnu = pthread_rwlock_init(locks[i],NULL)) != 0)
                error_exit(errnu,"pthread_rwlock_init");
        }
    }else{
        fprintf(stderr,"Not valid type\n");
        exit(EXIT_FAILURE);
    }

    struct timespec start_t, end_t;

    clock_gettime(CLOCK_MONOTONIC, &start_t);
    pthread_t thread[threads];
    for(int i=0; i<threads; i++){
        if((errnu = pthread_create(&thread[i],NULL,thread_work,&lock_type)) != 0)
            error_exit(errnu,"pthread_create");
    }
    for(int i=0; i<threads; i++){
        if((errnu = pthread_join(thread[i],NULL)) != 0)
            error_exit(errnu,"pthread_join");
    }
    clock_gettime(CLOCK_MONOTONIC, &end_t);
    double total = (end_t.tv_sec - start_t.tv_sec)
               + (end_t.tv_nsec - start_t.tv_nsec) / 1e9;
    printf("Parallel time: %.9f\n",total);


    // check (starting_sum = ending_sum)
    int ending_sum = 0;
    for(int i=0; i<acc_size; i++){
        ending_sum += acc_balances[i];
    }
    if(starting_sum != ending_sum)
        fprintf(stderr,"Starting sum differs from ending sum\n");
    else
        printf("Starting sum equals ending sum\n");


    if(lock_type == MUTEX_LOCK){
        for(int i=0; i<locks_size; i++){
            pthread_mutex_destroy(locks[i]);
            free(locks[i]);
        }
    }else{
        for(int i=0; i<locks_size; i++){
            pthread_rwlock_destroy(locks[i]);
            free(locks[i]);
        }
    }

    free(locks);
    free(acc_balances);
}
