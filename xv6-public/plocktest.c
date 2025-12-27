#include "types.h"
#include "stat.h"
#include "user.h"

void child(int prio)
{
  printf(1, "[PID %d] Priority %d: trying to acquire\n", getpid(), prio);
  plock_acquire(prio);
  printf(1, "[PID %d] Priority %d: LOCK ACQUIRED\n", getpid(), prio);
  sleep(50);
  printf(1, "[PID %d] Priority %d: releasing\n", getpid(), prio);
  plock_release();
  exit();
}

int main()
{
  int i, pid;
  
  printf(1, "\n=== Plock Test ===\n");
  
  if(fork() == 0) {
    printf(1, "\n[LOW] PID %d (prio=5) takes lock first\n", getpid());
    plock_acquire(5);
    
    sleep(100);
    
    printf(1, "\n[LOW] PID %d releasing lock\n", getpid());
    plock_release();
    exit();
  }
  
  sleep(20); 
  
  int priorities[] = {10, 20, 30, 40, 50};
  
  for(i = 0; i < 5; i++) {
    pid = fork();
    if(pid == 0) {
      child(priorities[i]);
    }
    sleep(5);
  }
  for(i = 0; i < 6; i++) {
    wait();
  }  
  exit();
}