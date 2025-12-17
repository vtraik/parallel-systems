#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <omp.h>

int* pol1 = NULL; 
int* pol2 = NULL;
int* prod_par = NULL;

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

double par_mul(int n, int threads){
    struct timespec start_t, end_t;
    clock_gettime(CLOCK_MONOTONIC, &start_t);
    #pragma omp parallel for num_threads(threads)
    for(int k=0; k<2*n+1; ++k){
        int sum = 0;
        int start_i = (k > n) ? k-n : 0;
        int end_i = (k < n) ? k : n;
        #pragma omp simd reduction(+:sum)
        for(int i=start_i; i<=end_i; ++i){
            sum += pol1[i]*pol2[k-i];
        }
        prod_par[k] = sum; 
    }
    clock_gettime(CLOCK_MONOTONIC, &end_t);
    double total_time = (end_t.tv_sec - start_t.tv_sec)
            + (end_t.tv_nsec - start_t.tv_nsec) / 1e9;
    return total_time;
}

int main(int argc, const char** argv){
    if(argc != 5){
        fprintf(stderr,"Usage: %s -n <polynomial degree> -t <num of threads>\n",argv[0]);
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

    srand(time(NULL));

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
    for(int i=0; i<n+1; i++){
        for(int j=0; j<n+1; j++){
            prod_serial[i+j] += pol1[i]*pol2[j]; 
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &end_t);

    total = (end_t.tv_sec - start_t.tv_sec)
            + (end_t.tv_nsec - start_t.tv_nsec) / 1e9;
    printf("Serial time: %.9f\n",total);

    // time parallel
    clock_gettime(CLOCK_MONOTONIC, &start_t);
    total = par_mul(n,threads);
    clock_gettime(CLOCK_MONOTONIC, &end_t);
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
