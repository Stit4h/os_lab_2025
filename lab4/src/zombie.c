#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "cli.h"
static int ShowProcess(pid_t child) {
  char pid_text[32];
  snprintf(pid_text, sizeof(pid_text), "%ld", (long)child);
  fflush(NULL);
  pid_t inspector = fork();
  if (inspector < 0) { perror("fork ps"); return -1; }
  if (inspector == 0) {
    execlp("ps", "ps", "-o", "pid,ppid,stat,comm", "-p", pid_text, (char *)NULL);
    _exit(127);
  }
  int status;
  while (waitpid(inspector, &status, 0) < 0) {
    if (errno != EINTR) return -1;
  }
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}
int main(int argc, char **argv) {
  unsigned int hold = 3;
  if (argc > 2 || (argc == 2 && ParseNumber(argv[1], &hold, 60))) {
    fprintf(stderr, "Usage: %s [hold_seconds 0..60]\n", argv[0]); return 1;
  }
  pid_t child = fork();
  if (child < 0) { perror("fork"); return 1; }
  if (child == 0) _exit(42);
  siginfo_t info;
  int ok;
  do { ok = waitid(P_PID, (id_t)child, &info, WEXITED | WNOWAIT); }
  while (ok < 0 && errno == EINTR);
  if (ok < 0) {
    perror("waitid");
    while (waitpid(child, NULL, 0) < 0 && errno == EINTR) {}
    return 1;
  }
  printf("Parent PID: %ld; child PID: %ld\n", (long)getpid(), (long)child);
  puts("Before waitpid: child has exited, status is not collected");
  int ps_before = ShowProcess(child);
  printf("Keeping zombie for %u second(s)\n", hold);
  fflush(stdout);
  while (hold) hold = sleep(hold);
  int status;
  pid_t done;
  do { done = waitpid(child, &status, 0); } while (done < 0 && errno == EINTR);
  if (done < 0) { perror("waitpid"); return 1; }
  if (!WIFEXITED(status)) return 1;
  printf("waitpid collected exit status: %d\n", WEXITSTATUS(status));
  puts("After waitpid: ps must show only the header");
  int ps_after = ShowProcess(child);
  errno = 0;
  done = waitpid(child, &status, WNOHANG);
  int no_child = done == -1 && errno == ECHILD;
  printf("Second waitpid: %s\n", no_child ? "ECHILD (no zombie remains)" : "unexpected result");
  return ps_before == 0 && ps_after == 1 && no_child ? 0 : 1;
}
