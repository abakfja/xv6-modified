#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

#define N 5000000
#define loop 20

int main() {
    volatile int id = getpid();
#ifdef PBS
    sprty(100 - id, id);
#endif    
    printf(1, "process %d started\n", id);
    for(int i = 0; i < loop; i++) {
        for(int j = 0; j < N; j++) {
            id = id + 1;
            id = id - 1;
        }
    }
    printf(1, "process %d completed\n", id);
    exit();
}