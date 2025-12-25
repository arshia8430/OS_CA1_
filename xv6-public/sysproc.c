#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}


extern void console_print_and_redraw(char *suggestions);
int
sys_redraw_console(void)
{
  char *suggestions;
  if(argstr(0, &suggestions) < 0)
    return -1; 
  console_print_and_redraw(suggestions); 
  
  return 0;
}

extern int simple_arithmetic_syscall(int, int);

int
sys_simple_arithmetic_syscall(void)
{
  struct proc *p = myproc();

  int a = p->tf->ebx;
  int b = p->tf->ecx;

  return simple_arithmetic_syscall(a, b);
}

int
sys_show_process_family(void)
{
  int pid;
  if(argint(0, &pid) < 0)
    return -1;

  int ppid = -1;
  int nchild = 0, nsib = 0, i;
  int children[NPROC];
  int siblings[NPROC];

  if(collect_process_family(pid, &ppid,
                            children, NPROC, &nchild,
                            siblings, NPROC, &nsib) < 0){
    return -1;
  }

  cprintf("My id: %d, My parent id: %d\n", pid, ppid);

  cprintf("Children of process %d:\n", pid);
  if(nchild == 0){
    cprintf("No children.\n");
  } else {
    for(i = 0; i < nchild; i++)
      cprintf("Child pid: %d\n", children[i]);
  }

  cprintf("Siblings of process %d:\n", pid);
  if(nsib == 0){
    cprintf("No siblings.\n");
  } else {
    for(i = 0; i < nsib; i++)
      cprintf("Sibling pid: %d\n", siblings[i]);
  }

  return 0;
}

// sysproc.c

extern int start_measure(void);
extern int end_measure(void);
extern int print_info(void);

int sys_start_measure(void) {
  return start_measure();
}

int sys_end_measure(void) {
  return end_measure();
}

int sys_print_info(void) {
  return print_info();
}

int
sys_slacquire(void)
{
  sltestacquire();
  return 0;
}

int
sys_slrelease(void)
{
  sltestrelease();
  return 0;
}

int sys_rwtest_rlock(void)   { rwtest_rlock();   return 0; }
int sys_rwtest_runlock(void) { rwtest_runlock(); return 0; }
int sys_rwtest_wlock(void)   { rwtest_wlock();   return 0; }
int sys_rwtest_wunlock(void) { rwtest_wunlock(); return 0; }