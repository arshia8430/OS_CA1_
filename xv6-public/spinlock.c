// Mutual exclusion spin locks.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"

void
initlock(struct spinlock *lk, char *name)
{
  lk->name = name;
  lk->locked = 0;
  lk->cpu = 0;
  for(int i = 0; i < NCPU; i++) {
    lk->acq_count[i] = 0;
    lk->total_spins[i] = 0;
  }
}

// Acquire the lock.
// Loops (spins) until the lock is acquired.
// Holding a lock for a long time may cause
// other CPUs to waste time spinning to acquire it.
void
acquire(struct spinlock *lk)
{
  pushcli(); // disable interrupts to avoid deadlock.
  if(holding(lk))
    panic("acquire");

  // The xchg is atomic.
  int spins=0;
  while(xchg(&lk->locked, 1) != 0)
    spins++;

  // Tell the C compiler and the processor to not move loads or stores
  // past this point, to ensure that the critical section's memory
  // references happen after the lock is acquired.
  __sync_synchronize();

  // Record info about lock acquisition for debugging.
  lk->cpu = mycpu();
  getcallerpcs(&lk, lk->pcs);



  if(lk==&tickslock){
    int id = mycpu() - cpus;
    lk->acq_count[id]++;
    lk->total_spins[id] += spins;
  }
  
}

// Release the lock.
void
release(struct spinlock *lk)
{
  if(!holding(lk))
    panic("release");

  lk->pcs[0] = 0;
  lk->cpu = 0;

  // Tell the C compiler and the processor to not move loads or stores
  // past this point, to ensure that all the stores in the critical
  // section are visible to other cores before the lock is released.
  // Both the C compiler and the hardware may re-order loads and
  // stores; __sync_synchronize() tells them both not to.
  __sync_synchronize();

  // Release the lock, equivalent to lk->locked = 0.
  // This code can't use a C assignment, since it might
  // not be atomic. A real OS would use C atomics here.
  asm volatile("movl $0, %0" : "+m" (lk->locked) : );

  popcli();
}

// Record the current call stack in pcs[] by following the %ebp chain.
void
getcallerpcs(void *v, uint pcs[])
{
  uint *ebp;
  int i;

  ebp = (uint*)v - 2;
  for(i = 0; i < 10; i++){
    if(ebp == 0 || ebp < (uint*)KERNBASE || ebp == (uint*)0xffffffff)
      break;
    pcs[i] = ebp[1];     // saved %eip
    ebp = (uint*)ebp[0]; // saved %ebp
  }
  for(; i < 10; i++)
    pcs[i] = 0;
}

// Check whether this cpu is holding the lock.
int
holding(struct spinlock *lock)
{
  int r;
  pushcli();
  r = lock->locked && lock->cpu == mycpu();
  popcli();
  return r;
}


// Pushcli/popcli are like cli/sti except that they are matched:
// it takes two popcli to undo two pushcli.  Also, if interrupts
// are off, then pushcli, popcli leaves them off.

void
pushcli(void)
{
  int eflags;

  eflags = readeflags();
  cli();
  if(mycpu()->ncli == 0)
    mycpu()->intena = eflags & FL_IF;
  mycpu()->ncli += 1;
}

void
popcli(void)
{
  if(readeflags()&FL_IF)
    panic("popcli - interruptible");
  if(--mycpu()->ncli < 0)
    panic("popcli");
  if(mycpu()->ncli == 0 && mycpu()->intena)
    sti();
}

struct plock global_plock;

void
plock_init(struct plock *pl, char *name)
{
  initlock(&pl->lock, "plock");
  pl->locked = 0;
  pl->head = 0;
  pl->name = name;
}

void
plock_acquire(struct plock *pl, int priority)
{
  struct plock_node *node;
  struct proc *p = myproc();
  
  acquire(&pl->lock);
  
  if (xchg(&pl->locked, 1)== 0) {
    pl->locked = 1;
    release(&pl->lock);
    return;
  }  
  node = (struct plock_node*)kalloc();
  if(node == 0)
    panic("plock_acquire: kalloc failed");
    
  node->proc = p;
  node->priority = priority;
  node->next = pl->head;
  pl->head = node;
  
  sleep(p, &pl->lock);
  
  release(&pl->lock);
}

void
plock_release(struct plock *pl)
{
  struct plock_node **prev, *current, *highest, **highest_prev;
  int max_priority;
  
  acquire(&pl->lock);
  
  if (pl->head == 0) {
    pl->locked = 0;
    release(&pl->lock);
    return;
  }
  
  highest_prev = &pl->head;
  highest = pl->head;
  max_priority = highest->priority;
  
  for (prev = &pl->head, current = pl->head; 
       current != 0; 
       prev = &current->next, current = current->next) {
    if (current->priority > max_priority) {
      highest_prev = prev;
      highest = current;
      max_priority = current->priority;
    }
  }
  
  *highest_prev = highest->next;
  
  wakeup(highest->proc);
  
  kfree((char*)highest);
  
  release(&pl->lock);
}