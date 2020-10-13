#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    int buf[10];
    int pid;

    int fds[2];
    pipe(fds);

    pid = fork();
    if (pid == 0) {
        read(fds[0], buf, sizeof(buf));
        printf("%d: received ping\n", getpid());
        write(fds[0], "\n", 1);
        exit(0);
    } else {
        write(fds[1], "\n", 1);

        wait((int *)0);

        read(fds[1], buf, sizeof(buf));

        printf("%d: received pong\n", getpid());
    }

    exit(0);
}
