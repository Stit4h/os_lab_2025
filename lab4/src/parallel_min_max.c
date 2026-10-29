#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "cli.h"
#include "find_min_max.h"
#include "utils.h"

static volatile sig_atomic_t expired = 0;
static void OnAlarm(int signum) { (void)signum; expired = 1; }
static void FileName(char *name, size_t len, pid_t parent, unsigned int i) {
  snprintf(name, len, "minmax_%ld_%u.tmp", (long)parent, i);
}

int main(int argc, char **argv) {
  unsigned int seed = 0, size = 0, count = 0, timeout = 0;
  int by_files = 0, has_timeout = 0, c;
  const struct option options[] = {
    {"seed", required_argument, NULL, 's'},
    {"array_size", required_argument, NULL, 'a'},
    {"pnum", required_argument, NULL, 'p'},
    {"by_files", no_argument, NULL, 'f'},
    {"timeout", required_argument, NULL, 't'}, {NULL, 0, NULL, 0}};
  while ((c = getopt_long(argc, argv, "f", options, NULL)) != -1) {
    switch (c) {
      case 's': if (ParseNumber(optarg, &seed, INT_MAX)) goto usage; break;
      case 'a': if (ParseNumber(optarg, &size, INT_MAX)) goto usage; break;
      case 'p': if (ParseNumber(optarg, &count, INT_MAX)) goto usage; break;
      case 'f': by_files = 1; break;
      case 't': if (ParseNumber(optarg, &timeout, UINT_MAX) || !timeout) goto usage;
                has_timeout = 1; break;
      default: goto usage;
    }
  }
  if (optind != argc || !seed || !size || !count || count > size) goto usage;
  int *array = calloc(size, sizeof(*array));
  pid_t *pids = calloc(count, sizeof(*pids));
  int (*pipes)[2] = calloc(count, sizeof(*pipes));
  if (!array || !pids || !pipes) {
    perror("calloc"); free(array); free(pids); free(pipes); return 1;
  }
  for (unsigned int i = 0; i < count; ++i) pipes[i][0] = pipes[i][1] = -1;
  GenerateArray(array, size, seed);
  pid_t parent = getpid();
  int failed = 0, result_code = 1;
  if (!by_files) {
    for (unsigned int i = 0; i < count; ++i) {
      if (pipe(pipes[i])) { perror("pipe"); failed = 1; break; }
    }
  }
  struct timespec start, finish;
  if (clock_gettime(CLOCK_MONOTONIC, &start)) { perror("clock_gettime"); failed = 1; }
  if (has_timeout && !failed) {
    struct sigaction action = {0};
    action.sa_handler = OnAlarm;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGALRM, &action, NULL)) { perror("sigaction"); failed = 1; }
    else alarm(timeout);
  }
  for (unsigned int i = 0; i < count && !failed; ++i) {
    if (expired) break;
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); failed = 1; break; }
    if (pid == 0) {
      for (unsigned int j = 0; j < count; ++j) {
        if (pipes[j][0] >= 0) close(pipes[j][0]);
        if (j != i && pipes[j][1] >= 0) close(pipes[j][1]);
      }
      unsigned int begin = (unsigned int)((uint64_t)i * size / count);
      unsigned int end = (unsigned int)((uint64_t)(i + 1) * size / count);
      struct MinMax part = GetMinMax(array, begin, end);
      if (by_files) {
        char name[80]; FileName(name, sizeof(name), parent, i);
        FILE *file = fopen(name, "w");
        if (!file) _exit(1);
        int error = fprintf(file, "%d %d\n", part.min, part.max) < 0;
        if (fclose(file)) error = 1;
        _exit(error);
      }
      ssize_t written;
      do { written = write(pipes[i][1], &part, sizeof(part)); }
      while (written < 0 && errno == EINTR);
      close(pipes[i][1]);
      _exit(written == (ssize_t)sizeof(part) ? 0 : 1);
    }
    pids[i] = pid;
    if (!by_files) { close(pipes[i][1]); pipes[i][1] = -1; }
  }
  unsigned int remaining = 0;
  for (unsigned int i = 0; i < count; ++i) if (pids[i] > 0) ++remaining;
  while (remaining && !failed && !expired) {
    for (unsigned int i = 0; i < count; ++i) {
      if (!pids[i]) continue;
      int status;
      pid_t done = waitpid(pids[i], &status, has_timeout ? WNOHANG : 0);
      if (done < 0) {
        if (errno == EINTR) { if (expired) break; continue; }
        perror("waitpid"); failed = 1; break;
      }
      if (done > 0) {
        pids[i] = 0; --remaining;
        if (!WIFEXITED(status) || WEXITSTATUS(status)) { failed = 1; break; }
      }
      if (expired) break;
    }
    if (has_timeout && remaining && !failed && !expired) {
      struct timespec delay = {0, 1000000};
      nanosleep(&delay, NULL);
    }
  }
  alarm(0);
  if (expired || failed) {
    if (expired) {
      fprintf(stderr, "Timeout after %u second(s): sending SIGKILL\n", timeout);
      result_code = 124;
    } else fprintf(stderr, "Child creation or execution failed\n");
    for (unsigned int i = 0; i < count; ++i) if (pids[i] > 0) {
      if (kill(pids[i], SIGKILL) && errno != ESRCH) perror("kill");
    }
    for (unsigned int i = 0; i < count; ++i) if (pids[i] > 0) {
      int status;
      pid_t done;
      do { done = waitpid(pids[i], &status, 0); } while (done < 0 && errno == EINTR);
      if (done > 0 && WIFSIGNALED(status))
        fprintf(stderr, "Child %ld: signal %d, reaped\n", (long)pids[i], WTERMSIG(status));
      pids[i] = 0;
    }
  } else {
    struct MinMax total = {INT_MAX, INT_MIN};
    for (unsigned int i = 0; i < count; ++i) {
      struct MinMax part;
      if (by_files) {
        char name[80]; FileName(name, sizeof(name), parent, i);
        FILE *file = fopen(name, "r");
        if (!file) { perror("fopen"); failed = 1; break; }
        int ok = fscanf(file, "%d %d", &part.min, &part.max) == 2;
        if (fclose(file) || !ok) { failed = 1; break; }
      } else {
        ssize_t received;
        do { received = read(pipes[i][0], &part, sizeof(part)); }
        while (received < 0 && errno == EINTR);
        if (received != (ssize_t)sizeof(part)) { failed = 1; break; }
      }
      if (part.min < total.min) total.min = part.min;
      if (part.max > total.max) total.max = part.max;
    }
    if (!failed && !clock_gettime(CLOCK_MONOTONIC, &finish)) {
      double ms = (finish.tv_sec - start.tv_sec) * 1000.0
                + (finish.tv_nsec - start.tv_nsec) / 1000000.0;
      printf("Min: %d\nMax: %d\nElapsed time: %.6fms\n", total.min, total.max, ms);
      result_code = 0;
    }
  }
  for (unsigned int i = 0; i < count; ++i) {
    if (pipes[i][0] >= 0) close(pipes[i][0]);
    if (pipes[i][1] >= 0) close(pipes[i][1]);
    if (by_files) {
      char name[80]; FileName(name, sizeof(name), parent, i); unlink(name);
    }
  }
  free(array); free(pids); free(pipes);
  return result_code;
usage:
  fprintf(stderr, "Usage: %s --seed N --array_size N --pnum N [--by_files] [--timeout SEC]\n", argv[0]);
  return 1;
}
