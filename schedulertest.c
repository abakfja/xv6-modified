#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

#define NFORK 5

char * argv[] = { "longwait" , 0 };

int main() {
    for(int i = 0; i < NFORK; i++) {
        int f = fork();

        if(f < 0) {
            printf(2, "fork() failed\n");
            exit();
        } else if(f == 0) {
            exec(argv[0], argv);
            printf(2, "exec() failed\n");
            exit();
        }
    }

    for(int i = 0; i < NFORK; i++)
        wait();

    exit();
}