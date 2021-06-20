# xv6 OS

### Syscalls
**waitx**
```C
 int waitx(int *, int *);
```
As mentioned this syscall stores the value of run time and waiting time of the process which was being waited on.

The process is assigned an `etime` when it ends, thus the waiting time `wtime`is equal to `etime - ctime - rtime` where `ctime` is the creation time of the process.

The user program time uses this syscall to get the time of the command given as argument. `time [command]*`

For example `time ls` gives the `rtime` and the `wtime` of the ls command

**ps**

`ps` is explained after the `MLFQ` has bee explained

**setpriority**

```c
int setpriority(int);
```
This syscall takes the new priority for a process and returns it's old priority.If the new priority is not valid, the priority is not changed. Priority is not valid if the PBS scheduler is not being used. If no such process exists returns 
-1, else 0;

### Scheduling: 
RR(Round-Robin)
 
The default scheduler is the `RR` scheduler.

The scheduler flag is used to signify the scheduler used while compiling.

FCFS(First Come First Serve)

This is pretty standard. The simplest approach

PBS(Prority Based Scheduling)

When set to `PBS` the process has a priority and higher priority process are scheduled first. The `setpriority` user program is used to set the priority of a process. Usage 
`setpriority [pid] [val]`

MLFQ(Multi-Level Feedback Queue)

1. On the initiation of a process, push it to the end of the highest priority queue.

2. The highest priority queue should be running always, if not empty.

3. If the process completes, it leaves the system.

4. If the process uses the complete time slice assigned for its current priority queue, it is preempted and inserted at the end of the next lower level queue.

5. If a process voluntarily relinquishes control of the CPU, it leaves the queuing network, and when the process becomes ready again after the I/O, it is inserted at the tail of the same queue, from which it is relinquished earlier. This can happen  if a process goes to preform a syscall before the time slice is over, thus a process would always be on the higher level queue and lower process would be kept waiting, although the process would be entered at the end of the queue so ageing process would at sometime
be scheduled.

6. A round-robin scheduler should be used for processes at the lowest priority
  queue.

7. To prevent starvation, implement the ageing phenomenon: 

  a. If the wait time of a process in lower priority queues exceeds a given
  limit(assign a suitable limit to prevent starvation), their priority is
  increased and they are pushed to the next higher priority queue.
  b. The wait time is reset to 0 whenever a process gets selected by the
  scheduler or if a change in the queue takes place (because of ageing).

Kept 30 time slices as the limit for ageing.

Implemented a user program `schedulertest` that can be used to test the scheduler
Prints the average time for each of the x sub processes. And the total time as well.

Here are some comparisons:

On a single CPU, 5 sub processes(all spawn at the same time).

| Scheduler | rtime | wtime |
| :-------: | :---: | :---: |
|   `RR`    |  290  | 1144  |
|   `PBS`   |  289  |  705  |
|  `FCFS`   |  336  |  618  |
|  `MLFQ`   |  283  | 1301  |

2 CPU's for 10 sub processes.(MLFQ is using only one CPU, So not included)

| Scheduler | rtime | wtime |
| :-------: | :---: | :---: |
|   `RR`    |  536  | 2111  |
|   `PBS`   |  582  | 1172  |
|  `FCFS`   |  563  | 1335  |

As expected `MLFQ` has higher `wtime` due to more work done by the scheduler to decide which process to schedule next.  `MLFQ` and `RR` we considerably slower time wise. `RR` suffers from high amounts of context switching. `FCFS` has highest `rtime` as CPU spent very less time deciding which process to take up next and let the process do it's own job.

**ps**
The user program ps uses the following structure to get the values of the current state of the process:

```C
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
```
They correspond directly to the required quantities.
Note:
Some quantities may not appear which using scheduling algorithms that don't support those quantities.

```C
int procinfo(struct procstat*);
```
pass an array of structures to the `procinfo` command;
```c
struct procstat buf[NPROC];
```
These would be filled at the end of the command.

FROM ORIGINAL AUTHORS

NOTE: we have stopped maintaining the x86 version of xv6, and switched
our efforts to the RISC-V version
(https://github.com/mit-pdos/xv6-riscv.git)

xv6 is a re-implementation of Dennis Ritchie's and Ken Thompson's Unix
Version 6 (v6).  xv6 loosely follows the structure and style of v6,
but is implemented for a modern x86-based multiprocessor using ANSI C.

ACKNOWLEDGMENTS

xv6 is inspired by John Lions's Commentary on UNIX 6th Edition (Peer
to Peer Communications; ISBN: 1-57398-013-7; 1st edition (June 14,
2000)). See also https://pdos.csail.mit.edu/6.828/, which
provides pointers to on-line resources for v6.

xv6 borrows code from the following sources:
    JOS (asm.h, elf.h, mmu.h, bootasm.S, ide.c, console.c, and others)
    Plan 9 (entryother.S, mp.h, mp.c, lapic.c)
    FreeBSD (ioapic.c)
    NetBSD (console.c)

The following people have made contributions: Russ Cox (context switching,
locking), Cliff Frey (MP), Xiao Yu (MP), Nickolai Zeldovich, and Austin
Clements.

We are also grateful for the bug reports and patches contributed by Silas
Boyd-Wickizer, Anton Burtsev, Cody Cutler, Mike CAT, Tej Chajed, eyalz800,
Nelson Elhage, Saar Ettinger, Alice Ferrazzi, Nathaniel Filardo, Peter
Froehlich, Yakir Goaron,Shivam Handa, Bryan Henry, Jim Huang, Alexander
Kapshuk, Anders Kaseorg, kehao95, Wolfgang Keller, Eddie Kohler, Austin
Liew, Imbar Marinescu, Yandong Mao, Matan Shabtay, Hitoshi Mitake, Carmi
Merimovich, Mark Morrissey, mtasm, Joel Nider, Greg Price, Ayan Shafqat,
Eldar Sehayek, Yongming Shen, Cam Tenny, tyfkda, Rafael Ubal, Warren
Toomey, Stephen Tu, Pablo Ventura, Xi Wang, Keiichi Watanabe, Nicolas
Wolovick, wxdao, Grant Wu, Jindong Zhang, Icenowy Zheng, and Zou Chang Wei.

The code in the files that constitute xv6 is
Copyright 2006-2018 Frans Kaashoek, Robert Morris, and Russ Cox.

ERROR REPORTS

We don't process error reports (see note on top of this file).

BUILDING AND RUNNING XV6

To build xv6 on an x86 ELF machine (like Linux or FreeBSD), run
"make". On non-x86 or non-ELF machines (like OS X, even on x86), you
will need to install a cross-compiler gcc suite capable of producing
x86 ELF binaries (see https://pdos.csail.mit.edu/6.828/).
Then run "make TOOLPREFIX=i386-jos-elf-". Now install the QEMU PC
simulator and run "make qemu".
