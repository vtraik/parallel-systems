#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <immintrin.h>
#include "timer.h"

#define ACCUMULATE_8(poly2_ptr, prod_ptr, v_poly1) { \
    _mm256_storeu_si256((__m256i*)(prod_ptr), \
        _mm256_add_epi32( \
            _mm256_loadu_si256((__m256i*)(prod_ptr)), \
            _mm256_mullo_epi32(v_poly1, _mm256_loadu_si256((__m256i*)(poly2_ptr))) \
        ) \
    ); \
}

int* pol1 = NULL;
int* pol2 = NULL;
int* prod_par = NULL;
int* prod_serial = NULL;

void print_pol(const int* pol, const int n){
    printf("[");
    for(int i=n; i>0; i--){
        printf("%d, ",pol[i]);
    }
    printf("%d]\n\n",pol[0]);
}

int get_rand(const int min, const int max){
    int r;
    do{
        r = min + rand() % (max-min+1);
    }while(r == 0);
    return r;
}

__attribute__((optimize("O1")))
double simd_mul_O1(const int n){
    double start_t, end_t;
    GET_TIME(start_t);
    for(int i=0; i<=n; ++i){
        int j;
        __m256i vp1 = _mm256_set1_epi32(pol1[i]); // extend to vec of 8 elem
        for (j=0; j+31<=n; j+=32){
            __m256i vp2_1 = _mm256_loadu_si256((__m256i*)&pol2[j]);
            __m256i vp2_2 = _mm256_loadu_si256((__m256i*)&pol2[j+8]);
            __m256i vp2_3 = _mm256_loadu_si256((__m256i*)&pol2[j+16]);
            __m256i vp2_4 = _mm256_loadu_si256((__m256i*)&pol2[j+24]);

            __m256i pprod_pack_1 = _mm256_loadu_si256((__m256i*)&prod_par[i+j]);
            __m256i pprod_pack_2 = _mm256_loadu_si256((__m256i*)&prod_par[i+j+8]);
            __m256i pprod_pack_3 = _mm256_loadu_si256((__m256i*)&prod_par[i+j+16]);
            __m256i pprod_pack_4 = _mm256_loadu_si256((__m256i*)&prod_par[i+j+24]);

            pprod_pack_1 = _mm256_add_epi32(pprod_pack_1, _mm256_mullo_epi32(vp1, vp2_1));
            pprod_pack_2 = _mm256_add_epi32(pprod_pack_2, _mm256_mullo_epi32(vp1, vp2_2));
            pprod_pack_3 = _mm256_add_epi32(pprod_pack_3, _mm256_mullo_epi32(vp1, vp2_3));
            pprod_pack_4 = _mm256_add_epi32(pprod_pack_4, _mm256_mullo_epi32(vp1, vp2_4));

            _mm256_storeu_si256((__m256i*)&prod_par[i+j], pprod_pack_1);
            _mm256_storeu_si256((__m256i*)&prod_par[i+j+8], pprod_pack_2);
            _mm256_storeu_si256((__m256i*)&prod_par[i+j+16], pprod_pack_3);
            _mm256_storeu_si256((__m256i*)&prod_par[i+j+24], pprod_pack_4);
        }

        // 0..7 loops serial
        for(; j<=n; ++j){
            prod_par[i+j] += pol1[i]*pol2[j];
        }

    }
    GET_TIME(end_t);
    return end_t - start_t;
}

double simd_mul(const int n){
    double start_t, end_t;
    GET_TIME(start_t);
    for(int i=0; i<=n; ++i){
        int j;
        __m256i vp1 = _mm256_set1_epi32(pol1[i]); // extend to vec of 8 elem
        register int* base_prod = &prod_par[i];
        register int* base_pol2 = &pol2[0];
        for (j=0; j+31<=n; j+=32){
            _mm_prefetch((const char*)(base_pol2 + 64), _MM_HINT_T0);
            _mm_prefetch((const char*)(base_prod + 64), _MM_HINT_T0);
            ACCUMULATE_8(base_pol2,    base_prod,      vp1);
            ACCUMULATE_8(base_pol2+8,  base_prod+8,  vp1);
            ACCUMULATE_8(base_pol2+16, base_prod+16, vp1);
            ACCUMULATE_8(base_pol2+24, base_prod+24, vp1);
            base_prod += 32;
            base_pol2 += 32;
        }

        // 0..7 loops serial
        for(; j<=n; ++j){
            prod_par[i+j] += pol1[i]*pol2[j];
        }

    }
    GET_TIME(end_t);
    return end_t - start_t;
}

double serial_mul(const int n){
    double start_t, end_t;
    GET_TIME(start_t);
    for(int i=0; i<n+1; ++i){
        for(int j=0; j<n+1; ++j){
            prod_serial[i+j] += pol1[i]*pol2[j];
        }
    }
    GET_TIME(end_t);
    return end_t - start_t;
}

int main(int argc, const char** argv){
    if(argc != 3){
        fprintf(stderr,"Usage: %s -n <polynomial degree>\n",argv[0]);
        exit(EXIT_FAILURE);
    }
    double start_t, end_t;
    int n = atoi(argv[2]);

    GET_TIME(start_t);
    pol1 = malloc((n+1)*sizeof(int));
    pol2 = malloc((n+1)*sizeof(int));
    prod_serial = calloc(2*n+1,sizeof(int));
    prod_par = calloc(2*n+1,sizeof(int));

    srand(time(NULL));

    // time init
    for(int i=0; i<n+1; ++i){
        pol1[i] = get_rand(-50,50);
        pol2[i] = get_rand(-50,50);
    }
    GET_TIME(end_t);

    double total = end_t - start_t;
    printf("Init time: %.9f\n",total);

    total = serial_mul(n);
    printf("Serial time: %.9f\n",total);

    #ifdef O0
        total = simd_mul(n);
        printf("Parallel time (O0): %.9f\n",total);
    #elif O1
        total = simd_mul_O1(n);
        printf("Parallel time (O1): %.9f\n",total);
    #endif

    // check serial with par and print ok or not
    unsigned are_same = 1;
    for(int i=0; i<2*n+1; ++i){
        if(prod_par[i] != prod_serial[i]){
            fprintf(stderr,"serial doesnt match with parallel\n");
            are_same = 0;
            break;
        }
    }

    if(are_same == 1)
        printf("Serial and Parallel are same\n");

    free(pol1);
    free(pol2);
    free(prod_serial);
    free(prod_par);
}
