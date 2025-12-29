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

void csr_mat_vect_mult(int* l_vals, int* l_rowindx, int* l_colindx,
                       int* x, int* local_y, int local_n, int iter, int* y_recount, int* y_recdispl){
    for(int it=0; it<iter; ++it){
        for(int i=0; i<local_n; ++i){ // num of rows of this proc
            local_y[i] = 0;
            for(int k=l_rowindx[i]; k<l_rowindx[i+1]; ++k) //  (nz in row i) * (respective inp vec pos)
                local_y[i] += l_vals[k] * x[l_colindx[k]];
        }
        if(it == iter - 1){ // if last, gather to 0
            MPI_Gatherv(local_y, local_n, MPI_INT, x, y_recount, y_recdispl, MPI_INT, 0, MPI_COMM_WORLD);
        }else{
            MPI_Allgatherv(local_y, local_n, MPI_INT, x, y_recount, y_recdispl, MPI_INT, MPI_COMM_WORLD);
        }
    }
}

void mat_vect_mult(int* local_row, int*  x, int* local_y, int local_n, int n,
                   int iter, int* y_recount, int* y_recdispl){
    for(int it=0; it<iter; ++it){
        for(int i=0; i<local_n; ++i){
            local_y[i] = 0;
            for(int j=0; j<n; ++j)
                local_y[i] += local_row[i*n + j] * x[j];
        }
        if(it == iter - 1){ // if last, gather to 0
            MPI_Gatherv(local_y, local_n, MPI_INT, x, y_recount, y_recdispl, MPI_INT, 0, MPI_COMM_WORLD);
        }else{
            MPI_Allgatherv(local_y, local_n, MPI_INT, x, y_recount, y_recdispl, MPI_INT, MPI_COMM_WORLD);
        }
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
    int zer;
    double start_t, end_t;

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
    int* out_vector = NULL;
    int* sendcount = NULL;
    int* displacements = NULL;
    int* array = NULL;
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
        array = malloc(n*n*sizeof(*array));
        out_vector = malloc(n*sizeof(*array));
        sendcount = alloca(num_proc*sizeof(int));
        displacements = alloca(num_proc*sizeof(int));

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

        int csum = 0;
        for(int i=0; i<num_proc; ++i){
            sendcount[i] = (local_size + (i < rem ? 1 : 0)) * n;
            displacements[i] = csum;
            csum += sendcount[i];
        }
    }


    // send for dense parallel compute
    MPI_Barrier(MPI_COMM_WORLD);
    // x gets broadcasted
    MPI_Bcast(vector, n, MPI_INT, 0, MPI_COMM_WORLD);
    // A gets scattered into l_rows in every process
    MPI_Scatterv(array, sendcount, displacements, MPI_INT, l_rows, my_proc_row_size*n, MPI_INT, 0, MPI_COMM_WORLD);

    // dense parallel
    GET_TIME(start_t); // think about this time . Shouldnt i get from all proc ?
    mat_vect_mult(l_rows, vector, l_out_vector, my_proc_row_size, n, iter, y_recount, y_recdispl); // vector has the result
    GET_TIME(end_t);
    double total_time = end_t - start_t;
    if(my_rank == 0)
        printf("Dense parallel time: %.10f\n",total_time);

    free(vector);
    free(l_out_vector);
    free(l_rows);

    // CSR
    // MPI_Bcast(&zer,1,MPI_INT,0,MPI_COMM_WORLD);
    // local_size = (n*n - zer) / num_proc;
    // rem = (n*n - zer) % num_proc;
    int* values = NULL;
    int* col_index = NULL;
    int* row_index = NULL;
    int* csr_sendcount = alloca(num_proc*sizeof(int));
    int* csr_displ = alloca(num_proc*sizeof(int));

    if(my_rank == 0){
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

        // Distribute by non-zero arrays and not row-wise (maybe i'll add row-wise also)
        local_size = (n*n - zer) / num_proc;
        rem = (n*n - zer) % num_proc;
        int crank= 0, last_split = 0; // current rank's sendcount/displ, last row that we had a split
        for(int i=0; i<n; ++i){
            // if the total num of non zero values for this rank is >= fair size
            // split here (exclusive)
            if(row_index[i] - row_index[last_split] >= local_size && crank < num_proc-1){
                csr_sendcount[crank] = i - last_split;
                csr_displ[crank] = row_index[i] - row_index[last_split];
                last_split = i;
                crank++;
            }
        }
        // give the rest to last rank
        csr_sendcount[num_proc - 1] = n - last_split;
        csr_displ[num_proc - 1] = row_index[n] - row_index[last_split];
    }

    MPI_Barrier(MPI_COMM_WORLD);

    // broadcast input vector x, send/displ arrays
    MPI_Bcast(vector_in_csr, n, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(csr_sendcount, num_proc, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(csr_displ, num_proc, MPI_INT, 0, MPI_COMM_WORLD);

    int l_nz = csr_displ[my_rank];
    int* l_vals = malloc(l_nz*sizeof(*l_vals));
    int* l_cols = malloc(l_nz*sizeof(*l_cols));
    int rowindx_sendcount[num_proc];
    int rowindx_displ[num_proc];
    l_rows = malloc((n+1)*sizeof(*l_rows));

    // compute row_index send/displ arrays
    csum = 0;
    for (int i = 0; i < csum; i++) {
        rowindx_sendcount[i] = csr_sendcount[i] + 1; // The +1 for the end-pointer
        rowindx_displ[i] = csum;
        csum += csr_sendcount[i];
    }

    // scatter values, col/row_index arrays in processes
    my_proc_row_size = csr_sendcount[my_rank];
    printf("h1\n");
    MPI_Scatterv(values, csr_sendcount, csr_displ, MPI_INT, l_vals, my_proc_row_size*n, MPI_INT, 0, MPI_COMM_WORLD);
    printf("h2\n");
    MPI_Scatterv(col_index, csr_sendcount, csr_displ, MPI_INT, l_cols, my_proc_row_size*n, MPI_INT, 0, MPI_COMM_WORLD);
    printf("h3\n");
    MPI_Scatterv(row_index, rowindx_sendcount, rowindx_displ, MPI_INT, l_rows, n+1, MPI_INT, 0, MPI_COMM_WORLD);

    // compute recieve displ array (gatherallv, gatherv)
    csum = 0;
    for(int i=0; i<num_proc; ++i){
        y_recdispl[i] = csum;
        csum += csr_sendcount[i];
    }

    GET_TIME(start_t); // think about this time . Shouldnt i get from all proc ?
    csr_mat_vect_mult(l_vals, l_rows, l_cols, vector_in_csr, out_vector, l_nz, iter, csr_sendcount, y_recdispl);
    GET_TIME(end_t);

    total_time = end_t - start_t;
    if(my_rank == 0)
        printf("CSR parallel time: %.10f\n",total_time);

    MPI_Finalize();

    // only root
    /* free(vector_in_dense); */

    if(my_rank == 0){
        free(array);
        free(out_vector);

        free(values);
        free(col_index);
        free(row_index);
    }
}
