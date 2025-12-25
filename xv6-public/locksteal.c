#include "types.h"
#include "user.h"

int
main(void)
{
  slacquire();

  int pid = fork();
  if(pid < 0){
    printf(1, "fork failed\n");
    slrelease();
    exit();
  }

  if(pid == 0){
    slrelease();
    printf(1, "ERROR: child released the lock!\n");
    exit();
  }

  wait();

  slrelease();
  exit();
}
