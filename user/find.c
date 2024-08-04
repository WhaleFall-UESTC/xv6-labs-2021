#include "user/helper.h"
#include "kernel/stat.h"
#include "kernel/fs.h"

#define CHECK(exp, info, ...)           \
    if (exp) {                          \
        printf(info, ## __VA_ARGS__);   \
        return -1;                      \
    }

int checkpath(char *path, struct stat *st) {
    int fd = 0;
    CHECK(strlen(path) + 1 + DIRSIZ + 1 > BUFSIZ, "find: path too long\n");
    CHECK((fd = open(path, 0)) < 0, "find: cannot open %s\n", path);
    CHECK(fstat(fd, st) < 0, "find: cannot stat %s\n", path);
    return fd;
}

inline int match(char *name1, char *name2) {
    return strcmp(name1, name2);
}

void find(int fdpath, char *path, char *filename) {
    struct dirent de;
    struct stat st;
    int fd;
    char buf[BUFSIZ];
    strcpy(buf, path);
    char *p = buf + strlen(buf);
    *p++ = '/';

    while (read(fdpath, &de, sizeof(de)) == sizeof(de)) {
        if (de.inum == 0)
            continue;
        memmove(p, de.name, DIRSIZ);
        if ((fd = checkpath(buf, &st)) < 0 || !strcmp(de.name, ".") || !strcmp(de.name, "..")) 
            continue;
        switch(st.type) {
            case T_FILE:
                if (match(de.name, filename) == 0) 
                    printf("%s\n", buf);
                close(fd);
                break;
            case T_DIR:
                find(fd, buf, filename);
        }
    }
    close(fdpath);
}

int main(int argc, char *argv[]) {
    EXIT(argc != 3, "Usage: find path filename\n");
    char *path = argv[1];
    char *filename = argv[2];
    struct stat st;
    int fd;
    if ((fd = checkpath(path, &st)) < 0) exit(1);
    EXIT(st.type != T_DIR, "find: %s is nor a directory\n", path);
    find(fd, path, filename);
    exit(0);
}