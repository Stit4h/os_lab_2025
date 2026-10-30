#include <sys/time.h>
#include <getopt.h>
#include "sum.h"
#include "utils.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <pthread.h>

void *ThreadSum(void *args) {
    struct SumArgs *sum_args = (struct SumArgs *)args;
    sum_args->result = Sum(sum_args);
    return NULL;
}

int main(int argc, char **argv) {
    int threads_num = 0;
    int array_size = 0;
    int seed = 0;

    static struct option options[] = {
        {"threads_num", required_argument, 0, 't'},
        {"seed", required_argument, 0, 's'},
        {"array_size", required_argument, 0, 'a'},
        {0, 0, 0, 0}
    };

    int c;

    while ((c = getopt_long(argc, argv, "", options, NULL)) != -1) {
        switch (c) {
            case 't':
                threads_num = atoi(optarg);
                break;
            case 's':
                seed = atoi(optarg);
                break;
            case 'a':
                array_size = atoi(optarg);
                break;
            default:
                return 1;
        }
    }

    if (threads_num <= 0 || array_size <= 0 ||
        seed <= 0 || threads_num > array_size) {
        printf("Usage: %s --threads_num num --seed num --array_size num\n",
               argv[0]);
        return 1;
    }
   
    pthread_t threads[threads_num];

    int *array = malloc(sizeof(int) * array_size);
    if (array == NULL) {
        return 1;
    }

    GenerateArray(array, array_size, seed);

    struct timeval start_time, finish_time;
    gettimeofday(&start_time, NULL);

    struct SumArgs args[threads_num];

    for (int i = 0; i < threads_num; i++) {
        args[i].array = array;
        args[i].begin = (long long)i * array_size / threads_num;
        args[i].end = (long long)(i + 1) * array_size / threads_num;

        if (pthread_create(&threads[i], NULL, ThreadSum, &args[i]) != 0) {
            printf("Error: pthread_create failed!\n");
            return 1;
        }
    }

    int64_t total_sum = 0;

    for (int i = 0; i < threads_num; i++) {
        pthread_join(threads[i], NULL);
        total_sum += args[i].result;
    }
    gettimeofday(&finish_time, NULL);

    double elapsed_time = (finish_time.tv_sec - start_time.tv_sec) * 1000.0;

    elapsed_time += (finish_time.tv_usec - start_time.tv_usec) / 1000.0;
    free(array);

    printf("Total: %lld\n", (long long)total_sum);
    printf("Elapsed time: %fms\n", elapsed_time);

    return 0;
}
