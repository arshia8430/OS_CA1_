#include "types.h"
#include "user.h"

static int
ustrlen(const char *s)
{
  int n = 0;
  while(s[n]) n++;
  return n;
}

static int
uitoa(int x, char *out)
{
  char buf[16];
  int i = 0;
  if(x == 0){
    out[0] = '0';
    return 1;
  }
  while(x > 0 && i < (int)sizeof(buf)){
    buf[i++] = '0' + (x % 10);
    x /= 10;
  }
  int j;
  for(j = 0; j < i; j++)
    out[j] = buf[i - 1 - j];
  return i;
}

static void
print_line(char who, int pid, const char *msg)
{
  char line[80];
  int k = 0;

  line[k++] = who;
  line[k++] = ' ';
  k += uitoa(pid, &line[k]);
  line[k++] = ':';
  line[k++] = ' ';

  int mlen = ustrlen(msg);
  int i;
  for(i = 0; i < mlen && k < (int)sizeof(line) - 2; i++)
    line[k++] = msg[i];

  line[k++] = '\n';
  write(1, line, k);
}


static void
reader(int hold_ticks)
{
  int pid = getpid();
  print_line('R', pid, "trying");
  rwtest_rlock();
  print_line('R', pid, "entered");

  sleep(hold_ticks);

  print_line('R', pid, "leaving");
  rwtest_runlock();
  exit();
}


static void
writer(int hold_ticks)
{
  int pid = getpid();
  print_line('W', pid, "trying");
  rwtest_wlock();
  print_line('W', pid, "entered");

  sleep(hold_ticks);

  print_line('W', pid, "leaving");
  rwtest_wunlock();
  exit();
}


int
main(void)
{
  int i;

  for(i = 0; i < 3; i++){
    if(fork() == 0)
      reader(300);
  }

  sleep(20);

  if(fork() == 0)
    writer(200);

  sleep(20);
  for(i = 0; i < 2; i++){
    if(fork() == 0)
      reader(300);
  }

  sleep(380);
  if(fork() == 0)
    reader(50);

  while(wait() >= 0)
    ;

  exit();
}
