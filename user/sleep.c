#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    if (argc <= 1) {
        fprintf(2, "usage: sleep duration\n");
        exit(1);
    }
    int duration = atoi(argv[1]);
    sleep(duration*10);
    exit(0);
}
