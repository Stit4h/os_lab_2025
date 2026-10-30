
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <getopt.h>

#include "find_min_max.h"
#include "utils.h"

volatile sig_atomic_t timeout_expired = 0;

void alarm_handler(int sig) {
    (void)sig;
    timeout_expired = 1;
}

int main(int argc, char **argv) {
  int seed = -1;
  int array_size = -1;
  int pnum = -1;
  int timeout = 0;
  bool with_files = false;

  while (true) {
    static struct option options[] = {{"seed", required_argument, 0, 0},
                                      {"array_size", required_argument, 0, 0},
                                      {"pnum", required_argument, 0, 0},
                                      {"by_files", no_argument, 0, 'f'},
                                      {"timeout", required_argument, 0, 0},
                                      {0, 0, 0, 0}};

    int option_index = 0;
    int c = getopt_long(argc, argv, "f", options, &option_index);

    if (c == -1) break;

    switch (c) {
      case 0:
        switch (option_index) {
          case 0:
            seed = atoi(optarg);
            break;
          case 1:
            array_size = atoi(optarg);
            break;
          case 2:
            pnum = atoi(optarg);
            break;
          case 3:
            with_files = true;
            break;
          case 4:
            timeout = atoi(optarg);
            break;

          default:
            printf("Index %d is out of options\n", option_index);
        }
        break;

      case 'f':
        with_files = true;
        break;

      case '?':
        return 1;

      default:
        printf("getopt returned character code 0%o?\n", c);
    }
  }

  if (optind < argc) {
    printf("Has at least one no option argument\n");
    return 1;
  }

  if (seed <= 0 || array_size <= 0 || pnum <= 0 ||
      pnum > array_size || timeout < 0) {
    printf("Usage: %s --seed \"num\" --array_size \"num\" --pnum \"num\" [--timeout \"num\"]\n",
           argv[0]);
    return 1;
  }

  int *array = malloc(sizeof(int) * array_size);
  if (array == NULL) return 1;

  GenerateArray(array, array_size, seed);
  int active_child_processes = 0;

  int pipes[pnum][2];
  pid_t child_pids[pnum];
  pid_t parent_pid = getpid();

  struct timeval start_time;
  gettimeofday(&start_time, NULL);

  // Register SIGALRM handler before creating children
  if (timeout > 0) {
    if (signal(SIGALRM, alarm_handler) == SIG_ERR) {
      perror("signal");
      free(array);
      return 1;
    }
  }

  for (int i = 0; i < pnum; i++) {

    if (!with_files && pipe(pipes[i]) == -1) {
      perror("pipe");
      free(array);
      return 1;
    }

    pid_t child_pid = fork();

    if (child_pid >= 0) {
      // successful fork
      active_child_processes += 1;

      if (child_pid == 0) {
        // child process

        int begin = (long long)i * array_size / pnum;
        int end = (long long)(i + 1) * array_size / pnum;

        struct MinMax result = GetMinMax(array, begin, end);

        if (with_files) {
          // use files here

          char filename[64];
          sprintf(filename, "minmax_%ld_%d.tmp", (long)parent_pid, i);

          FILE *file = fopen(filename, "w");
          if (file == NULL) _exit(1);

          fprintf(file, "%d %d", result.min, result.max);
          if (fclose(file) != 0) _exit(1);

        } else {
          // use pipe here

          close(pipes[i][0]);

          if (write(pipes[i][1], &result, sizeof(result))
              != sizeof(result)) {
            _exit(1);
          }

          close(pipes[i][1]);
        }

        _exit(0);
      }

      child_pids[i] = child_pid;

      if (!with_files) {
        close(pipes[i][1]);
      }

    } else {
      printf("Fork failed!\n");
      free(array);
      return 1;
    }
  }

  // Start timer after all child processes have been created
  if (timeout > 0) {
    alarm(timeout);
  }

  while (active_child_processes > 0) {
    int status;

    if (timeout > 0 && timeout_expired) {
      printf("Timeout expired! Killing child processes\n");

      // Kill child processes whose statuses have not been collected
      for (int i = 0; i < pnum; i++) {
        if (child_pids[i] > 0) {
          kill(child_pids[i], SIGKILL);
        }
      }

      // Collect remaining child statuses
      while (waitpid(-1, NULL, 0) > 0 || errno == EINTR) {
      }

      if (!with_files) {
        for (int i = 0; i < pnum; i++) {
          close(pipes[i][0]);
        }
      } else {
        for (int i = 0; i < pnum; i++) {
          char filename[64];
          sprintf(filename, "minmax_%ld_%d.tmp", (long)parent_pid, i);
          remove(filename);
        }
      }

      free(array);
      return 1;
    }

    pid_t result;

    if (timeout > 0) {
      result = waitpid(-1, &status, WNOHANG);
    } else {
      result = wait(&status);
    }

    if (result == -1) {
      if (errno == EINTR) {
        continue;
      }

      perror("waitpid");
      free(array);
      return 1;
    }

    if (result == 0) {
      usleep(10000);
      continue;
    }

    // Mark the collected child as finished
    for (int i = 0; i < pnum; i++) {
      if (child_pids[i] == result) {
        child_pids[i] = -1;
        break;
      }
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
      printf("Child process failed\n");
      free(array);
      return 1;
    }

    active_child_processes -= 1;
  }

  if (timeout > 0) {
    alarm(0);
  }

  struct MinMax min_max;
  min_max.min = INT_MAX;
  min_max.max = INT_MIN;

  for (int i = 0; i < pnum; i++) {
    int min = INT_MAX;
    int max = INT_MIN;

    if (with_files) {
      // read from files

      char filename[64];
      sprintf(filename, "minmax_%ld_%d.tmp", (long)parent_pid, i);

      FILE *file = fopen(filename, "r");
      if (file == NULL) return 1;

      if (fscanf(file, "%d %d", &min, &max) != 2) {
        fclose(file);
        return 1;
      }

      fclose(file);
      remove(filename);

    } else {
      // read from pipes

      struct MinMax result;

      if (read(pipes[i][0], &result, sizeof(result))
          != sizeof(result)) {
        return 1;
      }

      min = result.min;
      max = result.max;

      close(pipes[i][0]);
    }

    if (min < min_max.min) min_max.min = min;
    if (max > min_max.max) min_max.max = max;
  }

  struct timeval finish_time;
  gettimeofday(&finish_time, NULL);

  double elapsed_time = (finish_time.tv_sec - start_time.tv_sec) * 1000.0;
  elapsed_time += (finish_time.tv_usec - start_time.tv_usec) / 1000.0;

  free(array);

  printf("Min: %d\n", min_max.min);
  printf("Max: %d\n", min_max.max);
  printf("Elapsed time: %fms\n", elapsed_time);
  fflush(NULL);

  return 0;
}
