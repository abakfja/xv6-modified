#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

#define N 5000000
#define loop 20
#define NFORK 4

int main() {
    for(int i = 0; i < NFORK; i++) {
        int f = fork();

        if(f < 0) {
            printf(2, "fork() failed\n");
            exit();
        } else if(f == 0) {
            volatile int id = getpid();
#ifdef PBS
            setpriority(70 + (id % 3), id);
#endif    
            printf(1, "process %d started\n", id);
            for(int i = 0; i < loop; i++) {
                for(int j = 0; j < N; j++) {
                    id++;
                    id--;
                }
            }
            printf(1, "process %d completed\n", id);
            exit();
        }
    }

    for(int i = 0; i < NFORK; i++)
        wait();

    exit();
}