#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

typedef struct{
    pthread_mutex_t mt;
} Tdata;

int get_rand(int min, int max){
    return min + rand() % (max-min+1);
}

// temp will change
pthread_mutex_t acc_mutex; 
pthread_rwlock_t acc_rw; 

int main(int argc, const char** argv){
    if(argc != 11){
        fprintf(stderr,
                "Usage: ./%s -n <arr_size> -t <num of transfers/thread>
                -q <Percentage of query transactions:1..100> 
                -type <type of locks:m,rw> -thr <number of threads>\n");
    }

    int arr_size = atoi(argv[2]);
    int ntransfers = atoi(argv[4]);
    int perc = atoi(argv[6]);
    const char* type = argv[8];
    int threads = atoi(argv[10]);
    int* acc_balances = malloc(arr_size*sizeof(int));
    /* void* (*funct)(void*); */

    // init
    int starting_sum = 0;
    for(int i=0; i<arr_size; i++){
        acc_balances[i] = get_rand(0,10000);
        starting_sum += acc_balances[i];
    }


    if(strcmp(type,"m") == 0){
        if(pthread_mutex_init(&acc_mutex,NULL) != 0) perror_exit("pthread_mutex_init");
        /* funct = update_with_mut; */
    }else if(strcmp(type,"rw") == 0){
        if(pthread_rwlock_init(&acc_rw,NULL) != 0) perror_exit("pthread_rwlock_init");
        /* funct = update_with_rw; */
    }else{
        fprintf(stderr,"Not valid type\n");
        exit(EXIT_FAILURE);
    }



    // check (starting_sum = ending_sum)

}
