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

#endif