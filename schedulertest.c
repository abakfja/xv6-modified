#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

#define N 5000000
#define loop 20
#define NFORK 5

int main() {
    for(int i = 0; i < NFORK; i++) {
        int f = fork();

        if(f < 0) {
            printf(2, "fork() failed\n");
            exit();
        } else if(f == 0) {
            volatile int id = getpid();
            sleep(100 - 9 * i);
#ifdef PBS
            setpriority(70 + (id % 4), id);
#endif    
            // printf(1, "process %d started\n", id);
            for(int i = 0; i < loop; i++) {
                for(int j = 0; j < N; j++) {
                    id++;
                    id--;
                }
            }
            // printf(1, "process %d completed\n", id);
            exit();
        }
    }

    int totalr = 0, totalw = 0;
    for(int i = 0; i < NFORK; i++){
        int rtime, wtime;
        waitx(&wtime, &rtime);
        totalr += rtime;
        totalw += wtime;
        printf(1, "%d: %d, %d\n",i, wtime, rtime);
    }
    printf(1, "Average:\n rtime:%d, wtime:%d\n", totalr / NFORK, totalw / NFORK);
    printf(1, "Total:\n rtime:%d, wtime:%d\n", totalr, totalw);
    exit();
}