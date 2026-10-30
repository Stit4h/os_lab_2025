#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main() {
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        printf("Child process finished. PID: %d\n", getpid());
        fflush(stdout);
        _exit(0);
    }

    printf("Parent PID: %d\n", getpid());
    printf("Child PID: %d\n", pid);
    fflush(stdout);

    sleep(60);

    waitpid(pid, NULL, 0);

    printf("Zombie process removed\n");

    return 0;
}
