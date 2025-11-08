#include "types.h"
#include "stat.h"
#include "user.h"

void busy_work(int loops, char *name) {
  volatile int i;
  for(i = 0; i < loops; i++) {
    if(i % 10000000 == 0)
      printf(1, "%s running, iteration %d\n", name, i / 10000000);
    asm("nop");
  }
}

int main(int argc, char *argv[]) {
  int pid1, pid2;
  int p1[2], p2[2];
  char buf = 'x';

  pipe(p1);
  pipe(p2);

  pid1 = fork();
  if(pid1 == 0) {
    close(p1[1]);
    read(p1[0], &buf, 1);
    close(p1[0]);

    busy_work(100000000, "Child 1 (HIGH)");
    printf(1, "Child 1 (HIGH) finished. pid=%d\n", getpid());
    exit();
  }

  close(p1[0]);
  set_priority(pid1, 0);
  write(p1[1], &buf, 1);
  close(p1[1]);

  pid2 = fork();
  if(pid2 == 0) {
    close(p2[1]);
    read(p2[0], &buf, 1);
    close(p2[0]);

    busy_work(100000000, "Child 2 (LOW)");
    printf(1, "Child 2 (LOW) finished. pid=%d\n", getpid());
    exit();
  }

  close(p2[0]);
  set_priority(pid2, 2);
  write(p2[1], &buf, 1);
  close(p2[1]);

  wait();
  wait();

  printf(1, "Parent done\n");
  exit();
}
