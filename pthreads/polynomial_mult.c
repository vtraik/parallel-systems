#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>
#include <math.h>

void perror_exit(const char* mes){
  perror(mes);
  exit(EXIT_FAILURE);
}

#define BLOCK_SIZE 256    // block size, test
                          
int* pol1 = NULL; 
int* pol2 = NULL;
int* prod_par = NULL;

typedef struct {
   int start_index;
   int end_index;
   int n;
} Tdata;

void print_pol(int* pol, int n){
    printf("[");
    for(int i=n; i>0; i--){
        printf("%d, ",pol[i]);
    }
    printf("%d]\n\n",pol[0]);
}

int get_rand(int min, int max){
    int r;
    do{
        r = min + rand() % (max-min+1);
    }while(r == 0);
    return r;
}

void* par_mul(void* args){
    Tdata* data = (Tdata*) args;
    if(data->end_index - data->start_index < 20000){ // wrong 20k , might need fix
        for(int k=data->start_index; k<data->end_index; k++){
            int sum = 0;
            int start_i = (k > data->n) ? k-data->n : 0;
            int end_i = (k < data->n) ? k : data->n;
            for(int i=start_i; i<=end_i; i++){
                sum += pol1[i]*pol2[k-i];
            }
            prod_par[k] = sum; 
        }
    }else{
        int start = data->start_index,end = data->end_index;
        for (int k_block=start; k_block<=end; k_block+=BLOCK_SIZE) {
            int Kmax = (k_block + BLOCK_SIZE - 1 <= data->end_index) ? 
                k_block + BLOCK_SIZE - 1 : data->end_index;
            for (int k=k_block; k<=Kmax; k++) {
                int sum = 0;
                int start_i = (k > data->n) ? k-data->n : 0;
                int end_i = (k < data->n) ? k : data->n;
                for (int i_block=start_i; i_block<=end_i; i_block+=BLOCK_SIZE) {
                    int Imax = (i_block + BLOCK_SIZE - 1 <= end_i) ?
                        i_block + BLOCK_SIZE - 1  : end_i;
                    for(int i=i_block; i<=Imax; i++){
                        sum += pol1[i]*pol2[k-i];
                    }
                }
                prod_par[k] = sum; 
            }
        }
    }
    return NULL;
}

int main(int argc, const char** argv){
    if(argc != 5){
        fprintf(stderr,"Usage: ./%s -n <polynomial degree> -t <num of threads>",argv[0]);
        exit(EXIT_FAILURE);
    }
    struct timespec start_t, end_t;
    int n = atoi(argv[2]);
    int threads = atoi(argv[4]);

    clock_gettime(CLOCK_MONOTONIC, &start_t);
    pol1 = malloc((n+1)*sizeof(int));
    pol2 = malloc((n+1)*sizeof(int));
    int* prod_serial = calloc(2*n+1,sizeof(int));
    prod_par = calloc(2*n+1,sizeof(int));

    int seed = 1;
    srand(seed);

    // time init
    for(int i=0; i<n+1; i++){
        pol1[i] = get_rand(-50,50); 
        pol2[i] = get_rand(-50,50);
    }
    clock_gettime(CLOCK_MONOTONIC, &end_t);

    double total = (end_t.tv_sec - start_t.tv_sec)
               + (end_t.tv_nsec - start_t.tv_nsec) / 1e9;
    printf("Init time: %.9f\n",total);


    // time serial
    clock_gettime(CLOCK_MONOTONIC, &start_t);
    if(n <= 20000){ // experiment for value
        printf("in r\n");
        for(int i=0; i<n+1; i++){
            for(int j=0; j<n+1; j++){
                prod_serial[i+j] += pol1[i]*pol2[j]; 
            }
        }
    }else{
        int start=0,end=n+1;
        for (int i0=start; i0<end; i0+=BLOCK_SIZE) {
            int iEnd = (i0 + BLOCK_SIZE < end) ? i0 + BLOCK_SIZE : end;
            for (int j0=0; j0<end; j0+=BLOCK_SIZE) {
                int jEnd = (j0 + BLOCK_SIZE < end) ? j0 + BLOCK_SIZE : end;
                for (int i=i0; i<iEnd; i++) {
                    for (int j=j0; j<jEnd; j++) {
                        prod_serial[i+j] += pol1[i] * pol2[j];
                    }
                }
            }
        }
     } 
    clock_gettime(CLOCK_MONOTONIC, &end_t);

    total = (end_t.tv_sec - start_t.tv_sec)
            + (end_t.tv_nsec - start_t.tv_nsec) / 1e9;
    printf("Serial time: %.9f\n",total);



    // time parallel
    // each thread computes n/thread_count k's in prod_par[k]
    int elements_per_thread = ceilf(((float)2*n+1)/threads);
    if(threads > 2*n+1) threads = 2*n+1;
    pthread_t thread[threads];
    Tdata data[threads];

    clock_gettime(CLOCK_MONOTONIC, &start_t);
    for(int i=0; i<threads; i++){
        data[i].start_index=i*elements_per_thread;
        data[i].end_index=(i+1)*elements_per_thread;
        data[i].n = n; 
        if(data[i].end_index > 2*n+1) data[i].end_index = 2*n+1;
        if(pthread_create(&thread[i],NULL,par_mul,&data[i]) != 0)
            perror_exit("pthread_create");
    }
    
    for(int i=0; i<threads; i++){
        if(pthread_join(thread[i],NULL) != 0)
            perror_exit("pthread_join");
    }
    clock_gettime(CLOCK_MONOTONIC, &end_t);
    total = (end_t.tv_sec - start_t.tv_sec)
            + (end_t.tv_nsec - start_t.tv_nsec) / 1e9;
    printf("Parallel time: %.9f\n",total);

    // check serial with par and print ok or not
    uint8_t are_same = 1;
    for(int i=0; i<2*n+1; i++){
        if(prod_par[i] != prod_serial[i]){
            fprintf(stderr,"serial doesnt match with parallel\n");
            are_same = 0;
            break; 
        }
    }

    if(are_same)
        printf("Serial and Parallel are same\n");

    free(pol1);
    free(pol2);
    free(prod_serial);
    free(prod_par);
}
