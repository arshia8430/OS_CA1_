#include "types.h"
#include "defs.h"
#include "spinlock.h"
#include "sleeplock.h"

static struct sleeplock g_sl;

void
sltestinit(void)
{
  initsleeplock(&g_sl, "sltest");
}

void
sltestacquire(void)
{
  acquiresleep(&g_sl);
}

void
sltestrelease(void)
{
  releasesleep(&g_sl);
}
