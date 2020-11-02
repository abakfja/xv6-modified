#include "types.h"
#include "fcntl.h"
#include "stat.h"
#include "user.h"

int 
main(int argc, char** argv) 
{
    if(argc != 3){
        printf(2, "ps: Invalid arguments\n");
        exit();
    }

    int pid = atoi(argv[1]);
    int prty = atoi(argv[2]);
    if(pid <= 0){
        printf(2, "ps: Invalid arguments. Specify the pid of the process\n");
        exit();
    }
    if(prty < 0 || prty > 100){
        printf(2, "ps:  Invalid arguments. Specify the priority of the process\n");
        exit();
    }

    setpriority(prty, pid);
    printf(1, "Done");
    exit();
} 