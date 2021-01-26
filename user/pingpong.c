#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    char c;
    int pid;

    int fds[2];
    pipe(fds);

    pid = fork();
    if (pid == 0) {
        read(fds[0], &c, sizeof(char));
        printf("%d: received ping\n", getpid());
        write(fds[1], "c", 1);
        exit(0);
    } else {
        write(fds[1], "p", 1);

        wait((int *)0);

        read(fds[0], &c, sizeof(char));

        printf("%d: received pong\n", getpid());
    }

    exit(0);
}
