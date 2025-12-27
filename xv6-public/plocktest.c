#include "types.h"
#include "stat.h"
#include "user.h"

#define TEST_OWNER 0
void child(int prio)
{
  printf(1, "[PID %d] Priority %d: trying to acquire\n", getpid(), prio);
  plock_acquire(prio);
  printf(1, "[PID %d] Priority %d: LOCK ACQUIRED \n", getpid(), prio);
  sleep(50);
  
  printf(1, "[PID %d] Priority %d: releasing \n", getpid(), prio);
  plock_release();
  
  exit();
}

void test_wrong_release()
{
  plock_release();  
  exit();
}

int main()
{
  int i, pid;
  
  
  if(fork() == 0) {
    printf(1, "\n[OWNER-1] PID %d takes lock \n", getpid());
    plock_acquire(5);
    
    printf(1, "[OWNER-1] PID %d now owns \n", getpid());
    sleep(50);
    
    printf(1, "[OWNER-1] PID %d releasing \n", getpid());
    plock_release();
    exit();
  }
  
  sleep(10);
  if(TEST_OWNER){
    if(fork() == 0) {
      test_wrong_release();
    }
}
  
  sleep(10);
  
  int priorities[] = {2,3,1,5,4};
  
  for(i = 0; i < 5; i++) {
    pid = fork();
    if(pid == 0) {
      child(priorities[i]);
    }
    sleep(5);
  }
  
  for(i = 0; i < 7; i++) {
    wait();
  }
  
 
  exit();
}