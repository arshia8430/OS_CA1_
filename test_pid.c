#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  int pid;
  
  pid = getpid(); 
  
  printf(1, "pid: %d\n", pid);
  
  exit();
}