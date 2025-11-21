#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

void error_exit(int errnum, const char* mes){
    char buf[256];
    strerror_r(errnum, buf, sizeof(buf));   // use rc, not errno
    fprintf(stderr, "%s: %s\n", mes,buf);
  exit(EXIT_FAILURE);
}


typedef struct {
   int* shared;
   int iter;
} Tdata;

pthread_mutex_t shared_mut;
pthread_rwlock_t shared_rw;

void* update_with_mut(void* args){
    int* shared = (int*)((Tdata*) args)->shared;
    int iter = (int)((Tdata*) args)->iter;

    int errnu;
    for(int i=0; i<iter; i++){
        if((errnu = pthread_mutex_lock(&shared_mut)) != 0)
            error_exit(errnu,"pthread_mutex_lock");
        (*shared)++;
        if((errnu = pthread_mutex_unlock(&shared_mut)) != 0) 
            error_exit(errnu,"pthread_mutex_unlock");
    }

    return NULL;
}

void* update_with_rw(void* args){
    int* shared = (int*)((Tdata*) args)->shared;
    int iter = (int)((Tdata*) args)->iter;

    int errnu;
    for(int i=0; i<iter; i++){
        if((errnu = pthread_rwlock_wrlock(&shared_rw)) != 0)
            error_exit(errnu,"pthread_mutex_lock");
        (*shared)++;
        if((errnu = pthread_rwlock_unlock(&shared_rw)) != 0)
            error_exit(errnu,"pthread_mutex_unlock");
    }

    return NULL;
}

void* update_with_atomic(void* args){
    int* shared = (int*)((Tdata*) args)->shared;
    int iter = (int)((Tdata*) args)->iter;
    
    for(int i=0; i<iter; i++){
        __atomic_add_fetch(shared, 1, __ATOMIC_RELAXED);
    }

    return NULL;
}

int main(int argc, const char** argv){
    if(argc != 7){
        fprintf(stderr,"Usage: ./%s -c <m,rw,a> -t <num_of_threads> -i <num_of_iterations>\n",argv[0]);
        exit(EXIT_FAILURE);
    }
    int shared=0;
    const char* choice = argv[2];
    int threads= atoi(argv[4]);
    int iter = atoi(argv[6]);
    Tdata data = {&shared,iter};
    pthread_t thread[threads];
    void* (*funct)(void*);
    uint8_t choice_ = 0;
    struct timespec start_t, end_t;

    int errnu;
    if(strcmp(choice,"m") == 0){
        if((errnu = pthread_mutex_init(&shared_mut,NULL)) != 0)
            error_exit(errnu,"pthread_mutex_init");
        funct = update_with_mut;
    }else if(strcmp(choice,"rw") == 0){
        if((errnu = pthread_rwlock_init(&shared_rw,NULL)) != 0)
            error_exit(errnu,"pthread_rwlock_init");
        funct = update_with_rw;
        choice_ = 1;
    }else if(strcmp(choice,"a") == 0) {
        funct = update_with_atomic;
        choice_ = 2;
    }else{
        fprintf(stderr,"Not valid choice\n");
        exit(EXIT_FAILURE);
    }


    clock_gettime(CLOCK_MONOTONIC, &start_t);
    for(int i=0; i<threads; i++){
        if((errnu = pthread_create(&thread[i],NULL,funct,&data)) != 0)
            error_exit(errnu,"pthread_create");
    }
    for(int i=0; i<threads; i++){
        if((errnu = pthread_join(thread[i],NULL)) != 0)
            error_exit(errnu,"pthread_join");
    }
    clock_gettime(CLOCK_MONOTONIC, &end_t);
    double total = (end_t.tv_sec - start_t.tv_sec)
            + (end_t.tv_nsec - start_t.tv_nsec) / 1e9;



    printf("Shared: %d\n",shared);
    printf("Time: %.9f\n",total);

    if(choice_ == 0){
        if((errnu = pthread_mutex_destroy(&shared_mut)) != 0) 
            error_exit(errnu,"pthread_mutex_destroy");
    }else if(choice_ == 1){
        if((errnu = pthread_rwlock_destroy(&shared_rw)) != 0) 
            error_exit(errnu,"pthread_rwlock_destroy");
    }


}
