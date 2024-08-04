#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    if (argc > 1) {
        fprintf(2, "Usage: pingpong\n");
        exit(1);
    }

    int ptoc[2];
    int ctop[2];
    char buf[10];
    pipe(ptoc);
    pipe(ctop);

    if (fork() == 0) {
        close(ptoc[1]);
        read(ptoc[0], buf, 1);
        printf("%d: received ping\n", getpid());
        close(ctop[0]);
        write(ctop[1], "c", 1);
        exit(0);
    } 
    else {
        close(ptoc[0]);
        write(ptoc[1], "p", 1);
        wait(0);
        close(ctop[1]);
        read(ctop[0], buf, 1);
        printf("%d: received pong\n", getpid());
    }
    
    exit(0);
}