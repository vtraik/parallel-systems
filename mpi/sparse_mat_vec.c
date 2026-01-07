#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <mpi.h>
#include "timer.h"
#ifdef HYB
#include <omp.h>
#endif

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

#ifdef HYB
void csr_mat_vect_mult(int* l_vals, int* l_rowindx, int* l_colindx,
                       int* x, int* local_y, int local_n, int iter, int* y_recount, int* y_recdispl, int threads){
#else
void csr_mat_vect_mult(int* l_vals, int* l_rowindx, int* l_colindx,
                       int* x, int* local_y, int local_n, int iter, int* y_recount, int* y_recdispl){
#endif

    #ifdef HYB
    #pragma omp parallel num_threads(threads)
    #endif
    {
        for(int it=0; it<iter; ++it){
            #ifdef HYB
            #pragma omp for
            #endif
            for(int i=0; i<local_n; ++i){ // num of rows of this proc
                int sum = 0;
                for(int k=l_rowindx[i]; k<l_rowindx[i+1]; ++k) //  (nz in row i) * (respective inp vec pos)
                    sum += l_vals[k] * x[l_colindx[k]];

                local_y[i] = sum;
            }
            #ifdef HYB
            #pragma omp single
            #endif
            {
                if(it == iter - 1){ // if last, gather to 0
                    MPI_Gatherv(local_y, local_n, MPI_INT, x, y_recount, y_recdispl, MPI_INT, 0, MPI_COMM_WORLD);
                }else{
                    MPI_Allgatherv(local_y, local_n, MPI_INT, x, y_recount, y_recdispl, MPI_INT, MPI_COMM_WORLD);
                }
            }
        }
    }
}

#ifdef HYB
void mat_vect_mult(int* local_row, int*  x, int* local_y, int local_n, int n,
                   int iter, int* y_recount, int* y_recdispl, int threads){
#else
void mat_vect_mult(int* local_row, int*  x, int* local_y, int local_n, int n,
                   int iter, int* y_recount, int* y_recdispl){
#endif

    #ifdef HYB
    #pragma omp parallel num_threads(threads)
    #endif
    {
        for(int it=0; it<iter; ++it){
            #ifdef HYB
            #pragma omp for
            #endif
            for(int i=0; i<local_n; ++i){
                int sum = 0;
                for(int j=0; j<n; ++j)
                    sum += local_row[i*n + j] * x[j];
                local_y[i] = sum;
            }
            #ifdef HYB
            #pragma omp single
            #endif
            {
                if(it == iter - 1){ // if last, gather to 0
                    MPI_Gatherv(local_y, local_n, MPI_INT, x, y_recount, y_recdispl, MPI_INT, 0, MPI_COMM_WORLD);
                }else{
                    MPI_Allgatherv(local_y, local_n, MPI_INT, x, y_recount, y_recdispl, MPI_INT, MPI_COMM_WORLD);
                }
            }
        }
    }
}


void ser_mat_vect_mult(int* array, int*  x, int* y, int n, int iter){
    for(int it=0; it<iter; ++it){
        for(int i=0; i<n; ++i){
            y[i] = 0;
            for(int j=0; j<n; ++j)
                y[i] += array[i*n + j] * x[j];
        }
        memcpy(x,y,n*sizeof(int));
    }
}

int main(int argc, char** argv){
    if(argc < 7 || argc == 8 || argc > 9){
        fprintf(stderr,"Usage: %s -n <size of nxn matrix> -p <perc of 0>\
                -i <iter count> -t <threads: If HYB defined>\n",argv[0]);
        exit(EXIT_FAILURE);
    }

    int n = atoi(argv[2]);
    int perc = atoi(argv[4]);
    int iter = atoi(argv[6]);
    int num_proc, my_rank;
    int zer;
    double start_t, end_t, global_tot_time;
    #ifdef HYB
    int threads = argc == 9 ? atoi(argv[8]) : 0;
    #endif

    MPI_Init(NULL,NULL);
    MPI_Comm_size(MPI_COMM_WORLD, &num_proc);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

    // DENSE
    int local_size = n / num_proc;
    int rem = n % num_proc;
    int my_proc_row_size = my_rank < rem ? local_size + 1 : local_size;
    int* vector = malloc(n*sizeof(*vector));
    int* vector_in_csr = malloc(n*sizeof(*vector_in_csr));
    int* l_out_vector = malloc(my_proc_row_size*sizeof(*l_out_vector)); // every proc computes local_size (/+1) elem of y
    int* l_rows = calloc(my_proc_row_size*n,sizeof(int));
    int* sendcount = NULL;
    int* displacements = NULL;
    int* array = NULL;
    int* ser_in_vec = NULL;
    int y_recount[num_proc];
    int y_recdispl[num_proc];

    // compute recieve count,displ arrays (gatherallv, gatherv)
    int csum = 0;
    for(int i=0; i<num_proc; ++i){
        y_recount[i] = (local_size + (i < rem ? 1 : 0));
        y_recdispl[i] = csum;
        csum += y_recount[i];
    }
    if(my_rank == 0){
        srand(time(NULL));

        // init dense matrix, vector
        ser_in_vec = malloc(n*sizeof(*ser_in_vec));
        sendcount = alloca(num_proc*sizeof(int));
        displacements = alloca(num_proc*sizeof(int));
        array = malloc(n*n*sizeof(*array));

        zer = 0;
        for(int i=0; i<n; ++i){
            for(int j=0; j<n; ++j){
                array[i*n + j] = get_rand(perc,-50,50);
                if(array[i*n + j] == 0) // for csr init
                    ++zer;
            }
            vector[i] = -50 + rand()%(50+50);
        }

        memcpy(vector_in_csr,vector,n*sizeof(int)); // csr's input vector x
        memcpy(ser_in_vec,vector,n*sizeof(int)); // serial input vector x

        int csum = 0;
        for(int i=0; i<num_proc; ++i){
            sendcount[i] = (local_size + (i < rem ? 1 : 0)) * n;
            displacements[i] = csum;
            csum += sendcount[i];
        }
    }


    // send for dense parallel compute
    MPI_Barrier(MPI_COMM_WORLD);
    GET_TIME(start_t);
    // x gets broadcasted
    MPI_Bcast(vector, n, MPI_INT, 0, MPI_COMM_WORLD);
    // A gets scattered into l_rows in every process
    MPI_Scatterv(array, sendcount, displacements, MPI_INT, l_rows, my_proc_row_size*n, MPI_INT, 0, MPI_COMM_WORLD);
    GET_TIME(end_t);
    double dense_send_time = end_t - start_t;
    if(my_rank == 0) printf("Dense: total send time (from 0): %.10f\n",dense_send_time);

    // dense parallel
    GET_TIME(start_t);
    #ifdef HYB
        mat_vect_mult(l_rows, vector, l_out_vector, my_proc_row_size, n, iter, y_recount, y_recdispl, threads);
    #else
        mat_vect_mult(l_rows, vector, l_out_vector, my_proc_row_size, n, iter, y_recount, y_recdispl); // vector has the result
    #endif
    GET_TIME(end_t);
    double dense_ex_time = end_t - start_t;
    // keep slowest proc time
    MPI_Reduce(&dense_ex_time, &global_tot_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if(my_rank == 0){
        printf("Dense: execute time: %.10f\n",global_tot_time);
        printf("Dense: total time (send+ex): %.10f\n",global_tot_time+dense_send_time);
    }

    if(my_rank != 0) free(vector);
    free(l_out_vector);
    free(l_rows);

    // CSR
    double csr_init_time = 0.0;
    int* values = NULL;
    int* col_index = NULL;
    int* row_index = NULL;
    int* csr_sendcount = alloca(num_proc*sizeof(int));
    int* csr_displ = alloca(num_proc*sizeof(int));
    int* csr_non_zero_per_rank = alloca(num_proc*sizeof(int));

    if(my_rank == 0){
        // init csr 
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
        printf("CSR: init time: %.10f\n",total_time);
        csr_init_time = total_time;

        // Distribute by non-zero arrays and not row-wise
        local_size = (n*n - zer) / num_proc;
        rem = (n*n - zer) % num_proc;
        int crank= 0, last_split = 0; // current rank's sendcount/displ, last row that we had a split
        for(int i=1; i<=n; ++i){
            // if the total num of non zero values for this rank is >= fair size
            // split here (exclusive)
            if(row_index[i] - row_index[last_split] >= local_size && crank < num_proc-1){
                csr_sendcount[crank] = i - last_split;
                csr_displ[crank] = row_index[last_split];
                csr_non_zero_per_rank[crank] = row_index[i] - row_index[last_split];
                last_split = i;
                crank++;
            }
        }
        // give the rest to last rank
        csr_sendcount[num_proc - 1] = n - last_split;
        csr_displ[num_proc - 1] = row_index[last_split];
        csr_non_zero_per_rank[num_proc - 1] = row_index[n] - row_index[last_split];
    }

    MPI_Barrier(MPI_COMM_WORLD);

    // broadcast input vector x, send/displ arrays
    double csr_send_time = 0.0;
    GET_TIME(start_t);
    MPI_Bcast(vector_in_csr, n, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(csr_sendcount, num_proc, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(csr_non_zero_per_rank, num_proc, MPI_INT, 0, MPI_COMM_WORLD);
    GET_TIME(end_t);
    csr_send_time = end_t - start_t;

    int l_num_rows = csr_sendcount[my_rank];
    int l_num_nnz = csr_non_zero_per_rank[my_rank];
    int* l_vals = malloc(l_num_nnz*sizeof(*l_vals));
    int* l_cols = malloc(l_num_nnz*sizeof(*l_cols));
    l_rows = malloc((l_num_rows+1)*sizeof(*l_rows));
    l_out_vector = malloc(l_num_rows*sizeof(*l_out_vector));
    int rowindx_sendcount[num_proc];
    int rowindx_displ[num_proc];

    // scatter values, col/row_index arrays in processes
    GET_TIME(start_t);
    MPI_Scatterv(values, csr_non_zero_per_rank, csr_displ, MPI_INT, l_vals, l_num_nnz, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Scatterv(col_index, csr_non_zero_per_rank, csr_displ, MPI_INT, l_cols, l_num_nnz, MPI_INT, 0, MPI_COMM_WORLD);
    GET_TIME(end_t);

    csr_send_time += end_t - start_t;

    // compute row_index send/displ arrays
    if(my_rank == 0){
        csum = 0;
        for(int i = 0; i < num_proc; ++i){
            rowindx_sendcount[i] = csr_sendcount[i] + 1; // The +1 for the end-pointer
            rowindx_displ[i] = csum;
            csum += csr_sendcount[i];
        }
    }

    // scatter row_index
    GET_TIME(start_t);
    MPI_Scatterv(row_index, rowindx_sendcount, rowindx_displ, MPI_INT, l_rows, l_num_rows+1, MPI_INT, 0, MPI_COMM_WORLD);
    GET_TIME(end_t);
    csr_send_time += end_t - start_t;
    if(my_rank == 0) printf("CSR: total send time (from 0): %.10f\n",csr_send_time);

    // compute recieve displ array (gatherallv, gatherv)
    csum = 0;
    for(int i=0; i<num_proc; ++i){
        y_recdispl[i] = csum;
        csum += csr_sendcount[i];
    }

    int offset = l_rows[0];
    for(int i=0; i<=l_num_rows; ++i){
        l_rows[i] -= offset;
    }

    GET_TIME(start_t); 
    #ifdef HYB
        csr_mat_vect_mult(l_vals, l_rows, l_cols, vector_in_csr, l_out_vector, l_num_rows, iter, csr_sendcount, y_recdispl, threads);
    #else
        csr_mat_vect_mult(l_vals, l_rows, l_cols, vector_in_csr, l_out_vector, l_num_rows, iter, csr_sendcount, y_recdispl);
    #endif
    GET_TIME(end_t);

    double total_time = end_t - start_t;
    // keep slowest proc time
    MPI_Reduce(&total_time, &global_tot_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    if(my_rank == 0){
        printf("CSR: execute time: %.10f\n",global_tot_time);
        printf("CSR: total time (init+send+ex): %.10f\n",global_tot_time+csr_init_time+csr_send_time);
    }

    // free
    free(l_vals);
    free(l_cols);
    free(l_rows);
    free(l_out_vector);
    if(my_rank != 0) free(vector_in_csr);

    MPI_Finalize();

    // serial mat-vect and check with others
    #ifdef HYB
    goto END;
    #endif
    if(my_rank == 0){
        int* ser_out_vec = malloc(n*sizeof(*ser_in_vec));
        GET_TIME(start_t);
        ser_mat_vect_mult(array, ser_in_vec, ser_out_vec, n, iter);
        GET_TIME(end_t);
        double total_time = end_t - start_t;
        printf("Serial: execute time: %.10f\n",total_time);
        int is_wrong = 0;
        for(int i=0; i<n; ++i){
            if(ser_in_vec[i] != vector[i]){ // compare with dense res
                is_wrong = 1;
                printf("Serial and Dense results differ\n");
            }else if(ser_out_vec[i] != vector_in_csr[i]){ // compare with csr res
                is_wrong = 1;
                printf("Serial and CSR results differ\n");
            }
        }
        if(!is_wrong)
            printf("All results are the same\n");

        free(ser_out_vec);
    }

#ifdef HYB
END:
#endif
    // only root
    if(my_rank == 0){
        free(array);
        free(vector);

        free(values);
        free(col_index);
        free(row_index);
        free(vector_in_csr);
        free(ser_in_vec);
    }
}
