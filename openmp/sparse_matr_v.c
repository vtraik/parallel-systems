#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <omp.h>
#include <string.h>

#define PAR // if defined , parallel matrix_mul runs, else serial

void print_vector(int* vec, int n){
    for(int i=0; i<n; ++i)
        printf("%d ",vec[i]);
    printf("\n");
}

int get_rand(int perc, int min, int max){
    int r;
    if(rand() % 100 < perc){ 
        return 0;
    }else{
        do{
            r = min + rand()%(max-min);
        }while(r == 0);
    }
    return r;
}

void csr_vect_mult(int* values, int* row_index,int* col_index,
                   int* x, int* y, int n, int iter, int threads){
    #pragma omp parallel num_threads(threads)
    {
        for(int it=0; it<iter; ++it){
            #pragma omp for
            for(int i=0; i<n; ++i){
                int sum = 0;
                for(int k=row_index[i]; k<row_index[i+1]; ++k)
                    sum += values[k] * x[col_index[k]];
                
                y[i] = sum;
            }

            #pragma omp single
            memcpy(x,y,n*sizeof(int)); // copy to x the result
        }
    }
}

void mat_vect_mult(int* array, int*  x, int* y, int n, int iter, int threads){
    #ifdef PAR 
    #pragma omp parallel num_threads(threads)
    #endif
    {
        for(int it=0; it<iter; ++it){ 
            #ifdef PAR 
            #pragma omp for
            #endif 
            for(int i=0; i<n; ++i){
                int sum = 0;
                for(int j=0; j<n; ++j)
                    sum += array[i*n+j] * x[j];
                
                y[i] = sum;
            }

            #ifdef PAR 
            #pragma omp single
            #endif
            memcpy(x,y,n*sizeof(int)); // copy to x the result
        }
    }
}

int main(int argc, char** argv){
    if(argc != 9){
        fprintf(stderr,"Usage: ./%s -n <size of nxn matrix> -p <perc of 0>\
                -i <iter count> -t <num of threads>\n",argv[0]);
        exit(EXIT_FAILURE);
    }

    struct timespec start_t, end_t;
    int n = atoi(argv[2]);
    int perc = atoi(argv[4]);
    int iter = atoi(argv[6]);
    int threads = atoi(argv[8]);

    srand(time(NULL));

    // init dense matrix, vector
    int* array = malloc(n*n*sizeof(*array));
    int* vector = malloc(n*sizeof(*vector));
    int* vector_in_dense = malloc(n*sizeof(*vector_in_dense));
    int* out_vector = malloc(n*sizeof(*vector));
    int zer = 0;

    for(int i=0; i<n; ++i){
        for(int j=0; j<n; ++j){
            array[i*n + j] = get_rand(perc,-50,50);
            if(array[i*n + j] == 0) // for csr init
                ++zer;
        }
        vector[i] = -50 + rand()%(50+50);
    }
    memcpy(vector_in_dense,vector,n*sizeof(int));


    // init csr in parallel
    double ini_plus_time = 0.0;
    clock_gettime(CLOCK_MONOTONIC, &start_t);
    int non_zero = n*n - zer;
    int* values = malloc(non_zero*sizeof(*values));
    int* col_index = malloc(non_zero*sizeof(*col_index));
    int* row_index = malloc((n+1)*sizeof(*row_index));
    int* row_nz = malloc((n+1)*sizeof(*row_index));

    #pragma omp parallel num_threads(threads)
    {
        #pragma omp for
        for(int i=0; i<n; ++i){
            int nz_count = 0;
            for(int j=0; j<n; ++j){
                if(array[i*n+j] != 0)
                    ++nz_count;
            }
            row_nz[i] = nz_count; 
        }


        #pragma omp single
        row_index[0] = 0;
        for(int i=0; i<n; ++i){
            row_index[i+1] = row_index[i] + row_nz[i];
        }

        #pragma omp for
        for(int i=0; i<n; ++i){
            int p = row_index[i];
            for(int j=0; j<n; ++j){
                if(array[i*n+j] != 0){
                    values[p] = array[i*n+j];
                    col_index[p] = j;
                    ++p;
                }
            }
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end_t);


    double total_time = (end_t.tv_sec - start_t.tv_sec)
            + (end_t.tv_nsec - start_t.tv_nsec) / 1e9;
    printf("Init csr time: %f\n",total_time);
    ini_plus_time = total_time;


    // csr parall
    clock_gettime(CLOCK_MONOTONIC, &start_t);
    csr_vect_mult(values,row_index,col_index,vector,out_vector,n,iter,threads);
    clock_gettime(CLOCK_MONOTONIC, &end_t);
    total_time = (end_t.tv_sec - start_t.tv_sec)
            + (end_t.tv_nsec - start_t.tv_nsec) / 1e9;
    printf("CSR parallel time: %f\n",total_time);
    ini_plus_time += total_time;
    printf("CSR init + time: %f\n",ini_plus_time);

    
    free(values);
    free(col_index);
    free(row_index);
    free(row_nz);

    // dense parall
    clock_gettime(CLOCK_MONOTONIC, &start_t);
    mat_vect_mult(array,vector_in_dense,out_vector,n,iter,threads);
    clock_gettime(CLOCK_MONOTONIC, &end_t);
    total_time = (end_t.tv_sec - start_t.tv_sec)
            + (end_t.tv_nsec - start_t.tv_nsec) / 1e9;

    #ifdef PAR
        printf("Dense parallel time: %f\n",total_time);
    #else
        printf("Dense serial time: %f\n",total_time);
    #endif

   
    free(array);
    free(vector);
    free(out_vector);
    free(vector_in_dense);

}


