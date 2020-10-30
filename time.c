#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int 
main(int argc, char ** argv) 
{
  int pid = fork();
  if(pid < 0) {
    printf(2, "fork(): failed\n");
    exit();
  } else if(pid == 0) {
    if(argc == 1) {
      sleep(10);
      exit();
    } else {
      exec(argv[1], argv + 1);
      printf(2, "exec(): failed\n");
      exit();
    }  
  } else {
    int rtime, wtime;
    waitx(&wtime, &rtime);
    printf(1, "\nwaiting:%d\nrunning:%d\n", wtime, rtime);
  }
  exit();
}
