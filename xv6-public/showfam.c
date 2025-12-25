#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  int pid;
  if(argc >= 2)
    pid = atoi(argv[1]);
  else
    pid = getpid();

  int r = show_process_family(pid);
  if(r == -1){
    printf(1, "ERR: process %d not found\n", pid);
  }
  exit();
}
