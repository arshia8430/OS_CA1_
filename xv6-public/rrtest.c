#include "types.h"
#include "stat.h"
#include "user.h"

#define NCHILD 3
#define INNER  100000000

void
busy(void)
{
  volatile int i;
  volatile int x = 0;

  for(;;){
    for(i = 0; i < INNER; i++)
      x = x + i;
  }
}

int
main(int argc, char *argv[])
{
  int i, pid;

  for(i = 0; i < NCHILD; i++){
    pid = fork();
    if(pid < 0){
      exit();
    }
    if(pid == 0){
      busy();
    }
  }

  for(;;)
    sleep(1000);
  exit();
}
