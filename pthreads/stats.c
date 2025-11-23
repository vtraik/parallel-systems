#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

void error_exit(int errnum, const char* mes){
    char buf[256];
    strerror_r(errnum, buf, sizeof(buf));   // use rc, not errno
    fprintf(stderr, "%s: %s\n", mes,buf);
  exit(EXIT_FAILURE);
}

typedef struct {
   int* array;
   int size;
   int index;
} Tdata;

/* struct array_stats_s { */
/*      long long int info_array_0; */
/*      long long int info_array_1; */
/*      long long int info_array_2; */
/*      long long int info_array_3; */
/* } array_stats_par, array_stats_serial; */
struct array_stats_s {
     long long int info_array_0;
     long long int info_array_1;
     long long int info_array_2;
     long long int info_array_3;
} array_stats_serial;

struct array_stats {
     long long int info_array_0;
     int64_t pad[7];
     long long int info_array_1;
     int64_t pad2[7];
     long long int info_array_2;
     int64_t pad3[7];
     long long int info_array_3;
     int64_t pad4[7];
} array_stats_par;

int get_rand(int min, int max){
    return min + rand() % (max-min+1);
}

void analyze_serial(int** arrays, int size_arrays, int size_array){
    for(int i=0; i<size_arrays; i++){
        for(int j=0; j<size_array; j++){
            if(arrays[i][j] != 0)
                (*((long long int*)&array_stats_serial + i))++;
        }
    }

}

void* analyze_parallel(void* args){
    int* arr = ((Tdata*) args)->array; 
    int size = ((Tdata*) args)->size; 
    int index = ((Tdata*) args)->index; 
    for(int i=0; i<size; i++){
        if(arr[i] != 0)
            (*((long long int*)&array_stats_par + index*8))++;
            /* (*((long long int*)&array_stats_par + index))++; */
    }

    return NULL;
}

int main(int argc, const char** argv){
    if(argc != 3){
        fprintf(stderr,"Usage: ./%s -s <size of each array>\n",argv[0]);
        exit(EXIT_FAILURE);
    }
    int size = atoi(argv[2]);
    printf("size of lli: %lu\n",sizeof(long long int));
    // time init
    struct timespec start_t, end_t;
    clock_gettime(CLOCK_MONOTONIC, &start_t);
    int* arrays[4];
    for(int i=0; i<4; i++)
        arrays[i] = malloc(size*sizeof(int));

    int seed = 1;
    srand(seed);

    for(int i=0; i<size; i++){
        arrays[0][i] = get_rand(0,9);
        arrays[1][i] = get_rand(0,9);
        arrays[2][i] = get_rand(0,9);
        arrays[3][i] = get_rand(0,9);
    }

    clock_gettime(CLOCK_MONOTONIC, &end_t);
    double total = (end_t.tv_sec - start_t.tv_sec)
               + (end_t.tv_nsec - start_t.tv_nsec) / 1e9;
    printf("Init time: %.9f\n",total);

    // time serial
    clock_gettime(CLOCK_MONOTONIC, &start_t);
    analyze_serial(arrays,4,size);
    clock_gettime(CLOCK_MONOTONIC, &end_t);

    total = (end_t.tv_sec - start_t.tv_sec)
            + (end_t.tv_nsec - start_t.tv_nsec) / 1e9;
    printf("Serial time: %.9f\n",total);

    // time parallel
    pthread_t thread[4];
    Tdata data[4]; 
    int errnu;
    clock_gettime(CLOCK_MONOTONIC, &start_t);
    for(int i=0; i<4; i++){
        data[i].array = arrays[i];
        data[i].size = size;
        data[i].index = i;
        if((errnu = pthread_create(&thread[i],NULL,analyze_parallel,&data[i])) != 0)
            error_exit(errnu,"pthread_create");
    }
    for(int i=0; i<4; i++){
        if((errnu = pthread_join(thread[i],NULL)) != 0)
            error_exit(errnu,"pthread_join");
    }
    clock_gettime(CLOCK_MONOTONIC, &end_t);

    double par_total = (end_t.tv_sec - start_t.tv_sec)
            + (end_t.tv_nsec - start_t.tv_nsec) / 1e9;
    printf("Parallel time: %.9f\n",par_total);
    printf("Speedup: %.4f\n",total/par_total);
    
    // check
    uint8_t ok = 1;
    for(int i=0; i<4; i++){
        long long int serial_i = 
            *((long long int*)&array_stats_serial + i);
        long long int parallel_i = 
            *((long long int*)&array_stats_par + i*8);
        printf("serial_i: %lld | par_i: %lld\n",serial_i,parallel_i);
 
        if(serial_i != parallel_i){
            fprintf(stderr,"Serial and Parallel dont match\n");
            ok = 0;
        }
        free(arrays[i]);
    }

    if(ok) printf("Serial and Parallel are same\n"); 

}
