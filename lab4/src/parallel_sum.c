#define _POSIX_C_SOURCE 200809L
#include <getopt.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "cli.h"
#include "sum.h"
#include "utils.h"

int main(int argc, char **argv) {
  unsigned int count = 0, size = 0, seed = 0;
  int have_seed = 0, c;
  const struct option options[] = {
      {"threads_num", required_argument, NULL, 't'},
      {"seed", required_argument, NULL, 's'},
      {"array_size", required_argument, NULL, 'a'}, {NULL, 0, NULL, 0}};
  while ((c = getopt_long(argc, argv, "", options, NULL)) != -1) {
    switch (c) {
      case 't': if (ParseNumber(optarg, &count, INT_MAX)) goto usage; break;
      case 's': if (ParseNumber(optarg, &seed, UINT_MAX)) goto usage;
                have_seed = 1; break;
      case 'a': if (ParseNumber(optarg, &size, INT_MAX)) goto usage; break;
      default: goto usage;
    }
  }
  if (optind != argc || !count || !size || count > size || !have_seed) goto usage;
  int *array = calloc(size, sizeof(*array));
  pthread_t *threads = calloc(count, sizeof(*threads));
  struct SumArgs *args = calloc(count, sizeof(*args));
  if (!array || !threads || !args) {
    perror("calloc"); free(array); free(threads); free(args); return 1;
  }
  GenerateArray(array, size, seed);
  struct timespec start, finish;
  if (clock_gettime(CLOCK_MONOTONIC, &start)) {
    perror("clock_gettime"); free(array); free(threads); free(args); return 1;
  }
  unsigned int created = 0;
  int failed = 0;
  for (unsigned int i = 0; i < count; ++i) {
    args[i] = (struct SumArgs){array,
        (unsigned int)((uint64_t)i * size / count),
        (unsigned int)((uint64_t)(i + 1) * size / count), 0};
    int error = pthread_create(&threads[i], NULL, ThreadSum, &args[i]);
    if (error) {
      fprintf(stderr, "pthread_create: %s\n", strerror(error));
      failed = 1; break;
    }
    ++created;
  }
  uint64_t total = 0;
  for (unsigned int i = 0; i < created; ++i) {
    int error = pthread_join(threads[i], NULL);
    if (error) {
      fprintf(stderr, "pthread_join: %s\n", strerror(error));
      /* Avoid freeing data still in use if join unexpectedly fails. */
      exit(1);
    }
    total += args[i].result;
  }
  if (clock_gettime(CLOCK_MONOTONIC, &finish)) {
    perror("clock_gettime"); failed = 1;
  }
  if (!failed) {
    double ms = (finish.tv_sec - start.tv_sec) * 1000.0
              + (finish.tv_nsec - start.tv_nsec) / 1000000.0;
    printf("Threads: %u\nArray size: %u\nSeed: %u\nTotal: %" PRIu64
           "\nElapsed time: %.6fms\n", count, size, seed, total, ms);
  }
  free(array); free(threads); free(args);
  return failed;
usage:
  fprintf(stderr, "Usage: %s --threads_num N --seed N --array_size N\n", argv[0]);
  return 1;
}
