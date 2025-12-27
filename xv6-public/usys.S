#include "syscall.h"
#include "traps.h"

#define SYSCALL(name) \
  .globl name; \
  name: \
    movl $SYS_ ## name, %eax; \
    int $T_SYSCALL; \
    ret

SYSCALL(fork)
SYSCALL(exit)
SYSCALL(wait)
SYSCALL(pipe)
SYSCALL(read)
SYSCALL(write)
SYSCALL(close)
SYSCALL(kill)
SYSCALL(exec)
SYSCALL(open)
SYSCALL(mknod)
SYSCALL(unlink)
SYSCALL(fstat)
SYSCALL(link)
SYSCALL(mkdir)
SYSCALL(chdir)
SYSCALL(dup)
SYSCALL(getpid)
SYSCALL(sbrk)
SYSCALL(sleep)
SYSCALL(uptime)
SYSCALL(redraw_console)
SYSCALL(start_measure)
SYSCALL(end_measure)
SYSCALL(print_info)

.globl simple_arithmetic_syscall
simple_arithmetic_syscall:

    movl 4(%esp), %ebx
    movl 8(%esp), %ecx

    movl $SYS_simple_arithmetic_syscall, %eax
    int $T_SYSCALL
    ret

SYSCALL(make_duplicate)
SYSCALL(show_process_family)
SYSCALL(slacquire)
SYSCALL(slrelease)
SYSCALL(rwtest_rlock)
SYSCALL(rwtest_runlock)
SYSCALL(rwtest_wlock)
SYSCALL(rwtest_wunlock)
SYSCALL(getlockstat)
SYSCALL(plock_acquire)
SYSCALL(plock_release)