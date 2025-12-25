#include "types.h"
#include "defs.h"
#include "param.h"
#include "x86.h"
#include "mmu.h"
#include "proc.h"
#include "rwlock.h"


void
rwlock_init(struct rwlock *rw, char *name)
{
  initlock(&rw->lk, "rwlock");
  rw->name = name;
  rw->read_count = 0;
  rw->writer = 0;
  rw->writer_pid = 0;
}

void
rwlock_acquire_read(struct rwlock *rw)
{
  acquire(&rw->lk);

  while(rw->writer){
    sleep(rw, &rw->lk);
  }

  rw->read_count++;
  release(&rw->lk);
}

void
rwlock_release_read(struct rwlock *rw)
{
  acquire(&rw->lk);

  if(rw->read_count < 1){
    panic("rwlock_release_read: no readers");
  }

  rw->read_count--;

  if(rw->read_count == 0){
    wakeup(rw);
  }

  release(&rw->lk);
}

void
rwlock_acquire_write(struct rwlock *rw)
{
  acquire(&rw->lk);

  while(rw->writer || rw->read_count > 0){
    sleep(rw, &rw->lk);
  }

  rw->writer = 1;
  rw->writer_pid = myproc()->pid;
  release(&rw->lk);
}

void
rwlock_release_write(struct rwlock *rw)
{
  acquire(&rw->lk);

  if(rw->writer == 0){
    panic("rwlock_release_write: no writer");
  }

  if(rw->writer_pid != myproc()->pid){
    panic("rwlock_release_write: not owner");
  }

  rw->writer = 0;
  rw->writer_pid = 0;

  wakeup(rw);

  release(&rw->lk);
}
