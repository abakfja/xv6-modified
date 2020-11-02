#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

struct {
  int tail;
  struct proc* proc[NPROC];
} queue[NQUEUE];

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

void
qinit(void)
{
  for(int i = 0; i < NQUEUE; i++){
    queue[i].tail = 0;
    for(int j = 0; j < NPROC; j++)
      queue[i].proc[j] = 0;
  }
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Allocate timings
  p->ctime = ticks;
  p->rtime = 0;
  p->etime = 0;
  p->priority = 60;
  p->ntimes = 0;
  p->queue = 0;
  p->timeslice = 0;
  for(int i = 0; i < NQUEUE; i++)
    p->ticks[i] = 0;
  p->lastref = ticks;
  p->qtime = 0;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;
#ifdef MLFQ
  addproc(p, 0);
#endif
  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;
#ifdef MLFQ
  addproc(np, 0);
#endif
  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;
  curproc->etime = ticks;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
#ifdef MLFQ
        removeproc(p,p->queue);
#endif
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

// Waits for a child process to exit,
// Additionaly stores the saved runtime and wait time of the child process 
// Return -1 if this process has no chidren
int
waitx(int *wtime, int *rtime)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        *rtime = p->rtime;
        // Set waiting time to be total time - run time
        *wtime = (p->etime - p->ctime) - p->rtime;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
#ifdef MLFQ
        removeproc(p,p->queue);
#endif
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

// Update the time of each running process with every tick of CPU
void 
updatetime(void)
{
  struct proc *p;
  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == RUNNING){
      p->rtime++;
      p->ticks[p->queue]++;
      p->timeslice++;
      cprintf("[%d] [%d] timeslice _%d_ in _%d_\n", 
              ticks, p->pid, p->timeslice, p->queue);
    }
  }
  release(&ptable.lock);
}

// Set the priority of a given process
// Return -1 if no pid found
// Else Return old priority
int
setpriority(int priority, int pid)
{
  if(priority < 0 || priority > 100) 
    return -1;
  struct proc *p;
  int old = -1;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->pid == pid){
      old = p->priority;
      p->priority = priority;
      release(&ptable.lock);
      break;
    }

  if(old < 0){
    release(&ptable.lock);
    return -1;
  }
  
  if(old > priority)
    yield();
  
  return old;
}

// Add process to a new queue
void
addproc(struct proc *p, int id)
{ 
  if(id < 0 || id > 4)
    return;
  if(queue[id].proc[queue[id].tail] != 0 || queue[id].tail == NPROC)
    panic("fill queue");
  // cprintf("added proc %d to %d\n",p->pid, id);
  queue[id].proc[queue[id].tail++] = p;
  p->queue = id;
  p->qtime = 0;
  p->lastref = ticks;
  return;
}     

void
removeproc(struct proc *p, int id)
{
  // cprintf("Removed proc %d from %d\n",p->pid, id);
  for(int i = 0; i < queue[id].tail; i++){
    if(queue[id].proc[i]->pid == p->pid) {
      queue[id].proc[i] = 0;
      for(int j = i; j < queue[id].tail; j++){
        if(j < queue[id].tail - 1)
          queue[id].proc[j] = queue[id].proc[j + 1];
        else
          queue[id].proc[j] = 0;
      }
      queue[id].tail--;
      return;
    }
  }
  return;
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
#ifndef RR
  struct proc *chosen;
#endif
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
#ifdef RR
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
#endif
#ifdef FCFS
    chosen = (struct proc*) 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state == RUNNABLE){
        if(chosen == (struct proc*)0)
          chosen = p;
        else if(p->ctime < chosen->ctime)
          chosen = p;
      }
    }

    if(chosen == 0){
      release(&ptable.lock);
      continue;
    }

    c->proc = chosen;
    switchuvm(chosen);
    chosen->state = RUNNING;

    swtch(&(c->scheduler), chosen->context);
    switchkvm();

    // Process is done running for now.
    // It should have changed its p->state before coming back.
    c->proc = 0;
#endif
#ifdef PBS
    chosen = (struct proc *) 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state == RUNNABLE){        
        if(chosen == (struct proc *)0)
          chosen = p;
        else if(p->priority < chosen->priority)
          chosen = p;
        else if(p->priority == chosen->priority){
          if(p->ntimes < chosen->ntimes)
            chosen = p;
          else if(p->ntimes == chosen->ntimes && p->ctime < chosen->ctime)
            chosen = p;
        }
      }
    }
    if(chosen == 0){
      release(&ptable.lock);
      continue;
    }
    cprintf("[%d] Running [%d]\n", 
            ticks, chosen->pid);
    c->proc = chosen;
    switchuvm(chosen);
    chosen->state = RUNNING;
    chosen->ntimes++;
    swtch(&(c->scheduler), chosen->context);
    switchkvm();
 
     // Process is done running for now.
    // It should have changed its p->state before coming back.
    c->proc = 0;
#endif
#ifdef MLFQ
    // Add new processes
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state == RUNNABLE && p->queue < 0){
        addproc(p, 0);
      }
    }
    // Promote processes
    for(int i = 0; i < NQUEUE; i++){
      if(i > 0){
        for(int j = 0; j < queue[i].tail; j++){
          struct proc* p = queue[i].proc[j];
          if(p->state == RUNNABLE && 
              ticks - p->lastref >= 30){ // Over 30 ticks from last scheduling
            cprintf("[%d] Promoted [%d] froom queue _%d_ to _%d_\n",
                    ticks, p->pid, i, i - 1);
            removeproc(p, i);
            addproc(p, i - 1);
            p->timeslice = 0;
            j--;
          }
        }
      }
    }
    chosen = (struct proc *) 0;
    for(int i = 0; i < NQUEUE; i++){
      if(queue[i].tail > 0){
        chosen = queue[i].proc[0];

        if(chosen == 0) 
          continue;
        if(chosen->state == RUNNABLE)
          break;
        else
          removeproc(chosen, i);
      }
    }
    
    if(chosen == 0 || chosen->state != RUNNABLE){
      release(&ptable.lock);
      continue;
    }
    cprintf("[%d] Running [%d] in queue _%d_\n", 
            ticks, chosen->pid, chosen->queue);
    c->proc = chosen;
    switchuvm(chosen);
    chosen->state = RUNNING;
    chosen->ntimes++;
    chosen->lastref = ticks;
    swtch(&(c->scheduler), chosen->context);
    switchkvm();
 
     // Process is done running for now.
    // It should have changed its p->state before coming back.
    c->proc = 0;

    // If force-fully given up control
    if(chosen != 0 && chosen->state == RUNNABLE){
      int q = chosen->queue;
      if(chosen->demote){ // demote
        chosen->demote = 0;
        removeproc(chosen, q);
        if(q < NQUEUE - 1){
          cprintf("[%d] Demoted [%d] from _%d_ to _%d_\n", 
                ticks,chosen->pid, q, q + 1);
          addproc(chosen, q + 1);
        } else {
          addproc(chosen, q);
        }
        chosen->timeslice = 0;
      }
    }
    
#endif
    release(&ptable.lock);
  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan){
      p->state = RUNNABLE;
#ifdef MLFQ
      int q = p->queue;
      if(p->demote){ // demote
        p->demote = 0;
        removeproc(p, q);
        if(q < NQUEUE - 1){
          q++;
        }
      } else {
        removeproc(p, q);
      }
      p->timeslice = 0;
      addproc(p, q);
#endif
    }
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING){
        p->state = RUNNABLE;
#ifdef MLFQ
        int q = p->queue;
        if(p->demote){ // demote
          p->demote = 0;
          removeproc(p, q);
          if(q < NQUEUE - 1){
            q++;
          }
        } else {
          removeproc(p, q);
        }
        p->timeslice = 0;
        addproc(p, q);
#endif
      }
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s %d", p->pid, state, p->name, p->timeslice);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
#ifdef MLFQ
    cprintf("%d %d %d", p->queue, p->demote);
#endif
    cprintf("\n");
  }
#ifdef MLFQ
  cprintf("Queue status\n");
  for(int i = 0; i < NQUEUE; i++){
    cprintf("%d %d:\n", i, queue[i].tail);
    for(int j = 0; j < queue[i].tail; j++){
      cprintf("%d ",queue[i].proc[j]);
    }
    cprintf("\n");
  }
#endif
}
