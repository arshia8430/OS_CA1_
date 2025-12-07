#include "types.h"
#include "stat.h"
#include "user.h"

#define INNER  10000000

void
spin(char c)
{
  volatile int i;
  for(;;){
    for(i = 0; i < INNER; i++)
    write(1, &c, 1);
  }
}

int
main(int argc, char *argv[])
{
  int pidA, pidB;

  pidA = fork();
  if(pidA < 0){
    exit();
  }
  if(pidA == 0){

    sleep(200);
    spin('A');
  }

  sleep(50);

  pidB = fork();
  if(pidB < 0){
    exit();
  }
  if(pidB == 0){
    spin('B');
  }

  for(;;)
    sleep(1000);

  exit();
}
