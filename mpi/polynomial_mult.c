#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <mpi.h>
#include <time.h>
#include "timer.h"

void print_pol(int* pol, int n){
    printf("[");
    for(int i=n; i>0; --i){
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

int main(int argc, const char** argv){
    if(argc != 3){
        fprintf(stderr,"Usage: %s -n <polynomial degree>\n",argv[0]);
        exit(EXIT_FAILURE);
    }
    int n = atoi(argv[2]);
    int my_rank;
    int num_proc;

    MPI_Init(NULL, NULL);
    MPI_Comm_size(MPI_COMM_WORLD,&num_proc);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

    int* pol1 = NULL;
    int* par_res = NULL;
    int local_size = (n + 1) / num_proc; // what about if its not divisible?
    int rem = (n + 1) % num_proc;
    int* l_pol1 = malloc((local_size+1)*sizeof(*l_pol1)); // local pol1 (chunk of it)
    int* pol2 = malloc((n+1)*sizeof(*pol2)); // local pol2 of each proc (whole polynomial)
    int* l_res  = calloc(2*n+1,sizeof(*l_res)); // local result (whole polynomial, 2n+1)
    int* sendcount = NULL;
    int* displacements = NULL;

    if(my_rank == 0){ // send
        pol1 = malloc((n+1)*sizeof(int));
        par_res = malloc((2*n+1)*sizeof(int));
        sendcount = alloca(num_proc*sizeof(int));
        displacements = alloca(num_proc*sizeof(int));
        srand(time(NULL));
        for(int i=0; i<n+1; ++i){
            pol1[i] = get_rand(-50,50);
            pol2[i] = get_rand(-50,50);
        }
        int csum = 0;
        for(int i=0; i<num_proc; ++i){
            sendcount[i] = local_size + (i < rem ? 1 : 0);
            displacements[i] = csum;
            csum += sendcount[i];
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);
    double start_time, send_time, par_time, rec_time;
    int my_l_pol1_size = my_rank < rem ? local_size + 1 : local_size;
    GET_TIME(start_time);
    MPI_Bcast(pol2, n+1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Scatterv(pol1, sendcount, displacements, MPI_INT, l_pol1, my_l_pol1_size, MPI_INT, 0, MPI_COMM_WORLD);
    GET_TIME(send_time);
    send_time -= start_time;

    int my_start = my_rank * local_size +  (my_rank < rem ? my_rank : rem);
    GET_TIME(start_time);
    for(int i=0; i<my_l_pol1_size; ++i){
        for(int j=0; j<n+1; ++j){
            int index = my_start + i;
            l_res[index + j] += l_pol1[i] * pol2[j];
        }
    }
    GET_TIME(par_time);
    par_time -= start_time;
    GET_TIME(start_time);
    MPI_Reduce(l_res, par_res, 2*n+1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    GET_TIME(rec_time);
    rec_time -= start_time;

    if(my_rank == 0){
        // print times
        printf("Send time: %.10f\n",send_time);
        printf("Parallel time: %.10f\n",par_time);
        printf("Recieve time: %.10f\n",rec_time);
        double tot_time = send_time + par_time + rec_time;
        printf("Total time: %.10f\n",tot_time);
        /* print_pol(par_res,2*n+1); */
    }

    // free
    free(l_pol1);
    if(my_rank != 0) free(pol2);
    free(l_res);
    MPI_Finalize();

    if(my_rank != 0) exit(EXIT_SUCCESS); // exit other proc

    // time serial
    int* serial_res  = calloc(2*n+1,sizeof(*serial_res));
    GET_TIME(start_time);
    for(int i=0; i<n+1; ++i){
        for(int j=0; j<n+1; ++j){
            serial_res[i+j] += pol1[i]*pol2[j];
        }
    }
    double tot_time;
    GET_TIME(tot_time);
    tot_time -= start_time;
    printf("Serial time: %.10f\n",tot_time);


    // check serial with par and print ok or not
    uint8_t are_same = 1;
    for(int i=0; i<2*n+1; ++i){
        if(par_res[i] != serial_res[i]){
            fprintf(stderr,"serial doesnt match with parallel\n");
            are_same = 0;
            break;
        }
    }

    if(are_same)
        printf("Serial and Parallel are the same\n");

    free(pol1);
    free(pol2);
    free(par_res);
    free(serial_res);
}
