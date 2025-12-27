#ifndef _SPINLOCK_H_
#define _SPINLOCK_H_

// Mutual exclusion lock.
#include "param.h"
struct spinlock {
  uint locked;       // Is the lock held?

  // For debugging:
  char *name;        // Name of lock.
  struct cpu *cpu;   // The cpu holding the lock.
  uint pcs[10];      // The call stack (an array of program counters)
                     // that locked the lock.


  uint acq_count[NCPU];
  uint total_spins[NCPU];
};


struct plock_node {
  struct proc *proc;      
  int priority;         
  struct plock_node *next; 
};

struct plock {
  struct spinlock lock;  
  int locked;           
  struct plock_node *head; 
  char *name;           
  struct proc *owner;
};

void plock_init(struct plock *pl, char *name);
void plock_acquire(struct plock *pl, int priority);
void plock_release(struct plock *pl);

extern struct plock global_plock;


#endif