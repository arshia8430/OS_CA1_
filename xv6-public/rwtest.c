#include "types.h"
#include "defs.h"
#include "rwlock.h"

static struct rwlock g_rw;

void
rwtestinit(void)
{
  rwlock_init(&g_rw, "rwtest");
}

void rwtest_rlock(void)   { rwlock_acquire_read(&g_rw); }
void rwtest_runlock(void) { rwlock_release_read(&g_rw); }
void rwtest_wlock(void)   { rwlock_acquire_write(&g_rw); }
void rwtest_wunlock(void) { rwlock_release_write(&g_rw); }
