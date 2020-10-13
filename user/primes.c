#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int INT_SIZE = sizeof(int);

int generate(int max) {
    int i, pid;
    int fds[2];

    pipe(fds);

    pid = fork();
    if (pid == 0) { // child
        close(fds[0]); // close unused read end
        for (i = 2; i <= max; i++) {
            write(fds[1], &i, INT_SIZE);
        }
        close(fds[1]); // close write end
        exit(0);
    } else { // parent
        close(fds[1]); // close unused write end
    }
    return fds[0];
}

int filter(int fd_in, int prime) {
    int x, pid;
    int fds[2];

    pipe(fds);

    pid = fork();
    if (pid == 0) { // child
        close(fds[0]); // close unused read end
        while (read(fd_in, &x, INT_SIZE) > 0) {
            if (x % prime != 0) {
                write(fds[1], &x, INT_SIZE);
            }
        }
        close(fds[1]); // close write end
        exit(0);
    } else { // parent
        close(fds[1]); // close unused write end
    }
    return fds[0];
}

int main(int argc, char *argv[]) {
    int fd_gen;
    int prime;

    fd_gen = generate(35);
    while (read(fd_gen, &prime, INT_SIZE) > 0) {
        printf("prime %d\n", prime);
        fd_gen = filter(fd_gen, prime);
    }
    close(fd_gen);

    exit(0);
}

