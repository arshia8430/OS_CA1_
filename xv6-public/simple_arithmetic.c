#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  if(argc != 3){
    printf(2, "usage: simple_arithmetic <a> <b>\n");
    exit();
  }

  int a = atoi(argv[1]);
  int b = atoi(argv[2]);

  int r = simple_arithmetic_syscall(a, b);
  printf(1, "simple_arithmetic_syscall(%d,%d) returned %d\n", a, b, r);
  exit();
}
