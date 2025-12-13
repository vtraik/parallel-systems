#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <omp.h>

void merge(int* array, int left, int middle, int right){
    int* L;
    int* R;
    bool is_malloc_l = false, is_malloc_r = false;
    int size_left = middle - left + 1;
    int size_right = right - middle;
    if(size_left > 1024){
        L = malloc((size_left)*sizeof(int));
        is_malloc_l = true;
    }else{
        L = alloca((size_left)*sizeof(int));
    }

    if(size_right > 1024){
        R = malloc((size_right)*sizeof(int));
        is_malloc_r = true;
    }else{
        R = alloca((size_right)*sizeof(int));
    }

    /* #pragma omp simd  */ // They give ~0.4sec in parallel, i'll think if i will keep them 
    for(int i=0; i<size_left; ++i)
        L[i] = array[left + i];
    /* #pragma omp simd  */
    for(int i=0; i<size_right; ++i)
        R[i] = array[middle+1+i];

    int i = 0;
    int j = 0;
    int p = left;
    while(i < size_left && j < size_right){
        if(L[i] > R[j]){
            array[p++] = R[j++]; 
        }else{
            array[p++] = L[i++];
        }
    }

    while(i < size_left){
        array[p++] = L[i++];
    }

    while(j < size_right){
        array[p++] = R[j++];
    }

    if(is_malloc_l)
        free(L);
    if(is_malloc_r)
        free(R);
}

void serial_mergesort(int* array, int left, int right){
    if(left >= right) return;
    int middle = left + (right - left) / 2;
    serial_mergesort(array,left,middle);
    serial_mergesort(array,middle+1,right);
    merge(array,left,middle,right);
}

/* void parallel_mergesort(int* array, int left, int right, int depth){ // depth ? */
void parallel_mergesort(int* array, int left, int right){ // depth ?
    if(left >= right) return;
    int middle = left + (right - left) / 2;

    /* #pragma omp task shared(array) firstprivate(left,middle,depth) if(depth<6) */
    /* {parallel_mergesort(array,left,middle,depth+1);} */
    /* #pragma omp task shared(array) firstprivate(middle,right,depth) if(depth<6) */
    /* {parallel_mergesort(array,middle+1,right,depth+1);} */

    #pragma omp task shared(array) firstprivate(left,middle) if(middle-left+1 > 20000)
    {parallel_mergesort(array,left,middle);}
    #pragma omp task shared(array) firstprivate(middle,right) if(right-middle > 20000) 
    {parallel_mergesort(array,middle+1,right);}

    #pragma omp taskwait

    merge(array,left,middle,right);
    
}

int get_rand_int(){
    uint64_t upper = ((uint64_t) rand() << 32);
    uint64_t lower = (uint64_t) rand();
    return (int)(upper | lower);
}

int main(int argc, char** argv){
    if(argc != 7){
        fprintf(stderr,"Usage: ./%s -n <array size> -c <choice:s,p> -t <num of threads>\n",argv[0]);
        exit(EXIT_FAILURE);
    }
    struct timespec start_t, end_t;
    int size = atoi(argv[2]);
    char* choice = argv[4];
    char s_choice[10];
    bool is_serial = false; 
    int threads = atoi(argv[6]);
    double total;

    int seed = 1; // should change to time null
    srand(seed);
    /* srand(time(NULL)); */


    if(strcmp(choice,"s") == 0){
        is_serial = true;
        strcpy(s_choice,"Serial");
    }else if(strcmp(choice,"p") == 0){
        strcpy(s_choice,"Parallel");
    }else{
        fprintf(stderr,"Invalid choice, should be: s or p\n");
        exit(EXIT_FAILURE);
    }

    /* printf("size: %d\n",size); */
    int* array = malloc(size*sizeof(int));
    for(int i=0; i<size; ++i){
        array[i] = get_rand_int();
        /* printf("%d ",array[i]); */
    }
    /* printf("\n\n"); */

    // time choice 
    if(is_serial){
        printf("Calling Serial\n");
        clock_gettime(CLOCK_MONOTONIC, &start_t);
        serial_mergesort(array,0,size-1);
        clock_gettime(CLOCK_MONOTONIC, &end_t);
    }else{
        clock_gettime(CLOCK_MONOTONIC, &start_t);
        #pragma omp parallel num_threads(threads)
        {
            #pragma omp single
            {
                parallel_mergesort(array,0,size-1);
            }

        }
        clock_gettime(CLOCK_MONOTONIC, &end_t);
    }

    total = (end_t.tv_sec - start_t.tv_sec)
            + (end_t.tv_nsec - start_t.tv_nsec) / 1e9;
    printf("%s time: %.9f\n",s_choice,total);

    /* for(int i=0; i<size; ++i){ */
    /*     printf("%d ",array[i]); */
    /* } */
    /* printf("\n"); */

    // check if it is ordered 
    uint8_t sorted = 1;
    for(int i=0; i<size-1; ++i){
        if(array[i] > array[i+1]){
            fprintf(stderr,"Not sorted\n");
            sorted = 0;
            break; 
        }
    }

    if(sorted)
        printf("Sorted\n");

    free(array);
}




