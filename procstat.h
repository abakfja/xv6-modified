#include "param.h"

struct procstat {
  int pid;
  int priority;
  int state;
  int rtime;
  int wtime;
  int nrun;
  int curq;
  int ticks[NQUEUE];
};