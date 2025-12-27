#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

#define CA3_TEST 0


struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);
/// ///////////////////////
int metrics_active = 0;
uint metrics_start_tick = 0;
int metrics_proc_completed = 0;
void enqueue(struct cpu *c, struct proc *p);
int dequeue(struct cpu *c, struct proc *p);
void load_balance(void);
int start_measure(void);
int end_measure(void);
int print_info(void);
/// //////////////////////

static void wakeup1(void *chan);



struct cpu*
get_least_loaded_e_core(void)
{
  struct cpu *best_e_core = 0;
  int min_load = 0x7FFFFFFF; 
  
  for(int i = 0; i < ncpu; i++){
    struct cpu *c = &cpus[i];
    if (c->core_type == CORE_E) {
        if (c->queue_count < min_load) {
            min_load = c->queue_count;
            best_e_core = c;
        }
    }
  }
  if (best_e_core == 0) return &cpus[0]; 
  return best_e_core;
}

struct cpu*
get_lightest_p_core(void)
{
  struct cpu *best_p_core = 0;
  int min_load = 0x7FFFFFFF; // INT_MAX

  for (int i = 0; i < ncpu; i++) {
    struct cpu *c = &cpus[i];
    if (c->core_type == CORE_P) {
      if (c->queue_count < min_load) {
        min_load = c->queue_count;
        best_p_core = c;
      }
    }
  }
  return best_p_core;
}
void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
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
  int flag=1;
  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED){
      flag=0;
      break;
    }
      
  if (flag){
  release(&ptable.lock);
  return 0;}

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  acquire(&tickslock);
  p->ctime = ticks;
  //cprintf("[ALLOC ] New PID %d created. ctime=%d (ticks)\n", p->pid, p->ctime);
  p->slice_ticks = 0;
  p->next_run = 0;
  p->etime = 0;
  release(&tickslock);

  p->slice_ticks = 0;
  //cprintf("[ALLOC] New PID %d, ctime=%d\n", p->pid, p->ctime);
  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

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

struct cpu *min_cpu = get_least_loaded_e_core();
enqueue(min_cpu, p);

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

struct cpu *min_cpu = get_least_loaded_e_core();
enqueue(min_cpu, np);

//cprintf("[FORK  ] Created PID %d (parent=%d) -> assigned_cpu=%d queue_count_after=%d ctime=%d\n",
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

  acquire(&ptable.lock);

  curproc->etime = ticks;
  if(metrics_active) {
      metrics_proc_completed++;
  }

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
  struct cpu *c = mycpu();
  c->proc = 0;

  for(;;){
    sti();
    acquire(&ptable.lock);

    // [MODIFICATION 1: LOAD BALANCING TRIGGER]
    // Run load balancer on ALL E-Cores (CORE_E) every 5 ticks (assuming 50ms interval)
    if (c->core_type == CORE_E && ticks % 5 == 0) {
        // توجه: اگر 1 تیک = 10 میلی‌ثانیه باشد، ticks % 5 == 0 هر 50 میلی‌ثانیه اجرا می‌شود.
        load_balance();
    }

    struct proc *p = 0;


    if (c->runq != 0) {
      if(c->core_type == CORE_E){
        // [KEEP LOGIC]: E-Core: Round Robin (RR)
        // Select the first RUNNABLE process in the queue.
        struct proc *curr = c->runq;
        while(curr){
            if(curr->state == RUNNABLE){
                p = curr;
                break;
            }
            curr = curr->next_run;
        }
        
      } else { 
        // [MODIFICATION 2: P-CORE LOGIC]
        // P-Core: Modified FCFS (choose the process with the smallest Creation Time (ctime)).
        struct proc *curr = c->runq;
        struct proc *oldest = 0;
        
        while(curr){
            if(curr->state == RUNNABLE){
                // اگر هنوز فرآیندی انتخاب نشده (oldest == 0) یا فرآیند فعلی (curr) قدیمی‌تر است.
                if(oldest == 0 || curr->ctime < oldest->ctime){
                    oldest = curr;
                }
            }
            curr = curr->next_run;
        }
        p = oldest;
      }
    }
    
    // [CRITICAL CHECK]: If a process was found, dequeue it.
    if(p != 0){
        if (dequeue(c, p) != 0) { 
            // اگر به هر دلیلی نتوانستیم dequeue کنیم، فرآیند را کنار می‌گذاریم.
            p = 0; 
        }
    }


    if(p != 0){
      c->proc = p;
      p->slice_ticks = 0; // Reset slice for E-Core quantum or P-Core execution
      switchuvm(p);
      p->state = RUNNING;
      
      swtch(&c->scheduler, p->context);
      switchkvm();
      c->proc = 0;
    }

    release(&ptable.lock);
  }
}
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
  struct cpu *c = mycpu();
  
  myproc()->state = RUNNABLE;


  enqueue(c, myproc());


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
struct cpu *min_cpu = get_least_loaded_e_core();
enqueue(min_cpu, p);
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
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
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
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

int
simple_arithmetic_syscall(int a, int b)
{
  int sum = a + b;
  int diff = a - b;
  int res = sum * diff;
  cprintf("Calc: (%d+%d)*(%d-%d) = %d\n", a, b, a, b, res);
  return res;
}

int
collect_process_family(int pid, int *ppid,
                       int *child_pids, int max_child, int *num_child,
                       int *sib_pids,   int max_sib,   int *num_sib)
{
  struct proc *p, *target = 0;
  int nc = 0, ns = 0;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED) continue;
    if(p->pid == pid){
      target = p;
      break;
    }
  }

  if(target == 0){
    release(&ptable.lock);
    return -1;
  }

  *ppid = (target->parent) ? target->parent->pid : -1;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED) continue;
    if(p->parent == target){
      if(nc < max_child)
        child_pids[nc] = p->pid;
      nc++;
    }
  }

  if(target->parent){
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state == UNUSED) continue;
      if(p->parent == target->parent && p->pid != target->pid){
        if(ns < max_sib)
          sib_pids[ns] = p->pid;
        ns++;
      }
    }
  }

  release(&ptable.lock);

  if(num_child) *num_child = (nc < max_child ? nc : max_child);
  if(num_sib)   *num_sib   = (ns < max_sib ? ns : max_sib);
  return 0;
}


// proc.c

void
enqueue(struct cpu *c, struct proc *p)
{
  p->next_run = 0; 

  if (c->runq == 0) {
    c->runq = p;
  } else {
    struct proc *curr = c->runq;
    while (curr->next_run != 0)
      curr = curr->next_run;
    curr->next_run = p;
  }
  c->queue_count++; 
  // show head pid and length


}

// proc.c

int
dequeue(struct cpu *c, struct proc *p)
{
  if (c->runq == 0) return -1;

  if (c->runq == p) {
    c->runq = p->next_run;
    c->queue_count--;
    p->next_run = 0;
    return 0;
  }

  struct proc *curr = c->runq;
  while (curr->next_run != 0) {
    if (curr->next_run == p) {
      curr->next_run = p->next_run; 
      c->queue_count--;
      p->next_run = 0;

      return 0;
    }
    curr = curr->next_run;
  }

  return -1; 
}


void
load_balance(void)
{
  struct cpu *e_core = mycpu();
  
  // Safety check: Only E-cores push tasks
  if (e_core->core_type != CORE_E) return; 

  struct cpu *lightest_p_core = get_lightest_p_core();
  if (lightest_p_core == 0) return;

  // Requirement: Difference >= 3
  int min_p_load = lightest_p_core->queue_count;
  
  if (e_core->queue_count >= (min_p_load + 3)) {
      struct proc *victim = 0;
      struct proc *curr = e_core->runq;
      
      // Requirement: Do not move init (1) or sh (2)
      // Find the first movable candidate
      while (curr != 0) {
          if (curr->state == RUNNABLE && curr->pid > 2) { 
              victim = curr;
              break; 
          }
          curr = curr->next_run;
      }

      if (victim) {
          // Move the process
          dequeue(e_core, victim); 
          enqueue(lightest_p_core, victim);
          // Optional debug print
          if(CA3_TEST)
          cprintf("[LB] Pushed PID %d from E-Core to P-Core\n", victim->pid);
      }
  }
}

int
start_measure(void)
{
  acquire(&ptable.lock);
  metrics_active = 1;
  metrics_start_tick = ticks;
  metrics_proc_completed = 0;
  release(&ptable.lock);
  return 0;
}

int
end_measure(void)
{
  acquire(&ptable.lock);
  if(!metrics_active){
      release(&ptable.lock);
      return -1;
  }
  
  uint duration = ticks - metrics_start_tick;
  int count = metrics_proc_completed;
  metrics_active = 0;
  
  release(&ptable.lock);

  int throughput = 0;
  if(duration > 0)
    throughput = (count * 1000) / duration;

  cprintf("\n--- SCHEDULING METRICS ---\n");
  cprintf("Time: %d ticks\n", duration);
  cprintf("Completed: %d processes\n", count);
  cprintf("Throughput: %d (proc*1000/tick)\n", throughput);

  return 0;
}

int
print_info(void)
{
    struct proc *p;
    static char *states[] = {
    [UNUSED]    "UNUSED", [EMBRYO]    "EMBRYO", [SLEEPING]  "SLEEPING",
    [RUNNABLE]  "RUNBLE", [RUNNING]   "RUNNING", [ZOMBIE]    "ZOMBIE"
    };

    acquire(&ptable.lock);
    cprintf("\nPID\tName\tState\tLifetime\tAlgo\tQueueID\n");
    
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
        if(p->state == UNUSED)
            continue;

        uint lifetime = (p->state == ZOMBIE ? p->etime : ticks) - p->ctime;
        
        int queue_id = -1;
        char *algo = "GLOBAL"; // Default
        
        // 1. Check local run queues
        for(int i = 0; i < ncpu; i++){
            struct proc *curr = cpus[i].runq;
            while(curr){
                if (curr == p) {
                    queue_id = i;
                    algo = (cpus[i].core_type == CORE_E) ? "RR (E)" : "FCFS (P)";
                    goto found_queue;
                }
                curr = curr->next_run;
            }
        }
        
        // 2. Check if currently RUNNING on a CPU
        if(p->state == RUNNING){
             for(int i=0; i < ncpu; i++){
                 if(cpus[i].proc == p){
                     queue_id = i;
                     algo = (cpus[i].core_type == CORE_E) ? "RR (E)" : "FCFS (P)";
                     goto found_queue;
                 }
             }
        }

      found_queue:
        cprintf("%d\t%s\t%s\t%d\t\t%s\t%d\n", 
                p->pid, p->name, states[p->state], lifetime, algo, queue_id);
    }
    release(&ptable.lock);
    return 0;
}