#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <mpi.h>
#include "timer.h"

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

void mat_vect_mult(int* array, int*  x, int* y, int local_n, int iter){
    for(int it=0; it<iter; ++it){
        for(int i=0; i<local_n; ++i){
            y[i] = 0;
            for(int j=0; j<local_n; ++j)
                y[i] += array[i*local_n+j] * x[j];
        }
        memcpy(x,y,local_n*sizeof(int)); // copy to x the result
    }
}

int main(int argc, char** argv){
    if(argc != 7){
        fprintf(stderr,"Usage: %s -n <size of nxn matrix> -p <perc of 0>\
                -i <iter count>\n",argv[0]);
        exit(EXIT_FAILURE);
    }

    int n = atoi(argv[2]);
    int perc = atoi(argv[4]);
    int iter = atoi(argv[6]);
    int num_proc, my_rank;
    double start_t, end_t;

    MPI_Init(NULL,NULL);
    MPI_Comm_size(MPI_COMM_WORLD, &num_proc);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

    int* vector = malloc(n*sizeof(*vector));
    int* l_out_vector = malloc(n*sizeof(*l_out_vector));
    int local_size = n / num_proc;
    int rem = n % num_proc;
    int* l_rows = calloc(local_size+1,sizeof(int));
    int* out_vector = NULL;
    int* sendcount = NULL;
    int* displacements = NULL;
    int* array = NULL;
    int* values = NULL;
    int* col_index = NULL;
    int* row_index = NULL;

    if(my_rank == 0){
        srand(time(NULL));

        // init dense matrix, vector
        array = malloc(n*n*sizeof(*array));
        out_vector = malloc(n*sizeof(*array));
        /* int* vector_in_dense = malloc(n*sizeof(*vector_in_dense)); */
        int zer = 0;

        for(int i=0; i<n; ++i){
            for(int j=0; j<n; ++j){
                array[i*n + j] = get_rand(perc,-50,50);
                if(array[i*n + j] == 0) // for csr init
                    ++zer;
            }
            vector[i] = -50 + rand()%(50+50);
        }
        /* memcpy(vector_in_dense,vector,n*sizeof(int)); */

    //    print_vector(array,n*n);

        // init csr in parallel
        double ini_plus_time = 0.0;
        GET_TIME(start_t);
        int non_zero = n*n - zer;
        values = malloc(non_zero*sizeof(*values));
        col_index = malloc(non_zero*sizeof(*col_index));
        row_index = malloc((n+1)*sizeof(*row_index));
        int current_nz = 0, val_col_ind = 0, row_ind = 0;

        for(int i=0; i<n*n; ++i){
            if(i%n == 0){ // in every row change (when cols restart), update row
                row_index[row_ind++] = current_nz;
            }
            if(array[i] != 0){
                values[val_col_ind] = array[i];
                col_index[val_col_ind] = i%n;
                ++val_col_ind;
                ++current_nz;
            }
        }
        row_index[row_ind] = current_nz;

        GET_TIME(end_t);

        double total_time = end_t - start_t;
        printf("Init csr time: %.10f\n",total_time);
        ini_plus_time = total_time;


        int csum = 0;
        for(int i=0; i<num_proc; ++i){
            sendcount[i] = local_size + (i < rem ? 1 : 0);
            displacements[i] = csum;
            csum += sendcount[i];
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);

    int my_l_rows_size = my_rank < rem ? local_size + 1 : local_size;
    MPI_Bcast(vector, n, MPI_INT, 0, MPI_COMM_WORLD); // x gets broadcasted
    // A gets scattered into l_rows in every process
    MPI_Scatterv(array, sendcount, displacements, MPI_INT, l_rows, my_l_rows_size, MPI_INT, 0, MPI_COMM_WORLD);

    /* printf("values:\n\n"); */
//    print_vector(values,non_zero);
    /* printf("col_indx:\n\n"); */
//    print_vector(col_index,non_zero);
    /* printf("row_indx:\n\n"); */
//    print_vector(row_index,n+1);

    // csr parall
    /* clock_gettime(CLOCK_MONOTONIC, &start_t); */
    /* csr_vect_mult(values,row_index,col_index,vector,out_vector,n,iter,threads); */
    /* clock_gettime(CLOCK_MONOTONIC, &end_t); */
    /* total_time = (end_t.tv_sec - start_t.tv_sec) */
    /*         + (end_t.tv_nsec - start_t.tv_nsec) / 1e9; */
    /* printf("CSR parallel time: %f\n",total_time); */
    /* ini_plus_time += total_time; */
    /* printf("CSR init + time: %f\n",ini_plus_time); */

    if(my_rank == 0){
        free(values);
        free(col_index);
        free(row_index);
    }

    // dense parall
    GET_TIME(start_t);
    mat_vect_mult(l_rows,vector,l_out_vector,my_l_rows_size,iter);
    GET_TIME(end_t);
    double total_time = end_t - start_t;
    printf("Dense parallel time: %f\n",total_time);

    MPI_Gatherv(l_out_vector, n, MPI_DOUBLE, out_vector, sendcount, displacements, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    free(vector);
    free(l_out_vector);

    // only root
    free(array);
    free(out_vector);
    /* free(vector_in_dense); */

}
