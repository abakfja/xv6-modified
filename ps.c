#include "param.h"
#include "types.h"
#include "fcntl.h"
#include "stat.h"
#include "user.h"
#include "procstat.h"

struct procstat buf[NPROC];


// enum procstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

int main(int argc, char** argv) {
    procinfo(buf);
    printf(1, "pid\tprty\tstate     \trtime\twtime\tnrun\t");
#ifdef MLFQ
    printf(1, "currq\tq0\tq1\tq2\tq3\tq4\n");
#endif
    for(int i = 0; i < NPROC; i++){
        if(buf[i].pid > 0 && buf[i].state > 0){
            printf(1,"%d\t%d\t", buf[i].pid, buf[i].priority);
            switch (buf[i].state){
            case 1:
                printf(1,"embryo  \t");
                break;
            case 2:
                printf(1,"sleeping\t");
                break;
            case 3:
                printf(1,"waiting \t");
                break;
            case 4:
                printf(1,"running \t");
                break;
            case 5:
                printf(1,"zombie  \t");
                break;
            default:
                break;
            }
            printf(1, "%d\t%d\t%d\t", buf[i].rtime, buf[i].wtime, buf[i].nrun);
#ifdef MLFQ
            printf(1, "%d\t", buf[i].curq);
            for(int j = 0; j < NQUEUE; j++){
                printf(1, "%d\t",buf[i].ticks[j]);
            }
#endif       
            printf(1, "\n");     
        }
    }
    exit();
}