// test_security.c
#include "types.h"
#include "stat.h"
#include "user.h"

int main() {
  int pid;
  
  printf(1, "\n=== Testing plock security ===\n");
  
  pid = fork();
  if(pid == 0) {
    printf(1, "Child PID %d trying to release lock (should panic)...\n", getpid());
    plock_release(); 
    printf(1, "ERROR: Should not reach here!\n");
    exit();
  } else {
    printf(1, "Parent PID %d acquiring lock...\n", getpid());
    plock_acquire(10);
    sleep(100);
    printf(1, "Parent releasing lock...\n");
    plock_release();
    wait();
  }
  
  exit();
}