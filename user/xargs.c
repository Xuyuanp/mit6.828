#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"

int main(int argc, char *argv[]) {
    if (argc <= 1) {
        fprintf(2, "usage: xargs cmd[ args...]\n");
        exit(1);
    }

    char *exec_argv[MAXARG];
    char c;
    char buf[512];
    char *cmd = argv[1];
    for (int i = 1; i < argc; i++) {
        exec_argv[i-1] = argv[i];
    }

    int i = 0;
    while (read(0, &c, sizeof(char)) == sizeof(char)) {
        if (c == '\n') {
            buf[i] = '\0';
            exec_argv[argc-1] = buf;
            exec_argv[argc] = 0;
            i = 0;
            if (fork() == 0) {
                exec(cmd, exec_argv);
                exit(0);
            }
        } else {
            buf[i] = c;
            i++;
        }
    }

    int pid, status;
    while((pid = wait(&status)) > 0);
    /* wait((int *)0); */

    exit(0);
}
