#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

int find(char *path, char *pattern) {
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if ((fd = open(path, 0)) < 0) {
        fprintf(2, "find: cannot open %s\n", path);
        return 1;
    }

    if (fstat(fd, &st) < 0) {
        fprintf(2, "find: cannot open %s\n", path);
        close(fd);
        return 1;
    }
    switch (st.type) {
        case T_FILE:
            if (strcmp(path + strlen(path) - strlen(pattern), pattern) == 0) {
                printf("%s\n", path);
            }
            break;
        case T_DIR:
            if (strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)) {
                fprintf(2, "find: path too long\n");
                break;
            }
            strcpy(buf, path);
            p = buf + strlen(buf);
            *p++ = '/';
            while (read(fd, &de, sizeof(de)) == sizeof(de)) {
                if (de.inum == 0) {
                    continue;
                }
                if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) {
                    continue;
                }
                memmove(p, de.name, DIRSIZ);
                p[DIRSIZ] = 0;
                if (find(buf, pattern) > 0) {
                    close(fd);
                    return 1;
                }
            }

    }
    close(fd);

    return 0;
}

int main(int argc, char *argv[]) {
    if (argc <= 2) {
        fprintf(2, "usage: find path pattern\n");
        exit(1);
    }

    char *path = argv[1];
    char *pattern = argv[2];

    exit(find(path, pattern));
}
