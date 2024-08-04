#include "kernel/types.h"
#include "user/user.h"

#define READ(fd, value) read(fd, &value, sizeof(int))
#define WRITE(fd, value) write(fd, &value, sizeof(int))

void passpipe(int pl[], int pr[]) {
    int p, n;
    close(pl[1]);
    if (READ(pl[0], p) == 0) exit(0);
    printf("prime %d\n", p);

    if (fork() > 0) {
        close(pr[0]);
        while (READ(pl[0], n)) {
            if (n % p != 0) 
                WRITE(pr[1], n);
        }
        close(pl[0]);
        close(pr[1]);
        wait(0);
    } 
    else {
        close(pl[0]);
        int *newpl = pr;
        int newpr[2];
        pipe(newpr);
        passpipe(newpl, newpr);
    }

    exit(0);
}


int main(int argc, char *argv[]) {
    int n = 35;
    if (argc > 2) {
        fprintf(2, "Usage: primes n\n");
        exit(1);
    }
    else if (argv[1]) {
        n = atoi(argv[1]);
    }

    int pl[2];
    pipe(pl);
    
    if (fork() > 0) {
        close(pl[0]);
        for (int i = 2; i < n; i++) 
            WRITE(pl[1], i);
        close(pl[1]);
        wait(0);
    }
    else {
        int pr[2];
        pipe(pr);
        passpipe(pl, pr);
    }

    exit(0);
}