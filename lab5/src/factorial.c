
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

int k, pnum, mod;
long long result = 1;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void *calculate(void *arg) {
    int id = *(int *)arg;
    long long part = 1;

    for (int i = id + 1; i <= k; i += pnum) {
        part = (part * i) % mod;
    }

    pthread_mutex_lock(&mutex);
    result = (result * part) % mod;
    pthread_mutex_unlock(&mutex);

    return NULL;
}

int main(int argc, char *argv[]) {
    int opt;
    int option_index = 0;

    struct option options[] = {
        {"pnum", required_argument, 0, 'p'},
        {"mod", required_argument, 0, 'm'},
        {0, 0, 0, 0}
    };

    k = -1;
    pnum = 0;
    mod = 0;

    while ((opt = getopt_long(argc, argv, "k:", options,
                              &option_index)) != -1) {
        switch (opt) {
            case 'k': k = atoi(optarg); break;
            case 'p': pnum = atoi(optarg); break;
            case 'm': mod = atoi(optarg); break;
            default: return 1;
        }
    }

    if (k < 0 || pnum <= 0 || mod <= 0) {
        printf("Usage: %s -k N --pnum=N --mod=N\n", argv[0]);
        return 1;
    }

    pthread_t threads[pnum];
    int ids[pnum];

    result = 1 % mod;

    for (int i = 0; i < pnum; i++) {
        ids[i] = i;
        pthread_create(&threads[i], NULL, calculate, &ids[i]);
    }

    for (int i = 0; i < pnum; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("%d! mod %d = %lld\n", k, mod, result);

    pthread_mutex_destroy(&mutex);
    return 0;
}
