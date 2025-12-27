#include "types.h"
#include "stat.h"
#include "user.h"

#include "param.h"

#define NCHILD 8
#define ITERATIONS 1000

int main()
{
  uint stats[2 * NCPU];
  int i, pid;
  
  printf(1, "=== Lock TEST===\n\n");
  
  printf(1, "1. Before load:\n");
  if(getlockstat(stats) < 0) {
    exit();
  }
  
  for(i = 0; i < NCPU; i++) {
    printf(1, "CPU%d: acq=%d, spins=%d\n", 
           i, stats[2*i], stats[2*i+1]);
  }
  
  printf(1, "\n2. Creating %d child processes...\n", NCHILD);
  for(i = 0; i < NCHILD; i++) {
    pid = fork();
    if(pid < 0) {
      exit();
    }
    if(pid == 0) {
      for(int j = 0; j < ITERATIONS; j++) {
        uptime();
      }
      exit();
    }
  }
  
  printf(1, "\nWAIT\n");
  for(i = 0; i < NCHILD; i++) {
    wait();
  }
  
  printf(1, "\nAfter load:\n");
  if(getlockstat(stats) < 0) {
    printf(1, "Error: getlockstat failed!\n");
    exit();
  }
  
  printf(1, "\nResults:\n");
  printf(1, "CPU | Acquisitions | Total Spins | Avg Spins/Acq\n");
  
  for(i = 0; i < NCPU; i++) {
    uint acq = stats[2*i];
    uint spins = stats[2*i+1];
    uint avg = 0;
    
    if(acq > 0) {
      avg = spins / acq;
    }
    
    printf(1, "%d  | %d | %d | %d\n", 
           i, acq, spins, avg);
  }
  
  exit();
}