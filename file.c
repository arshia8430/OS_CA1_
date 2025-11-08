//
// File descriptors
//

#include "types.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "stat.h"

struct devsw devsw[NDEV];
struct {
  struct spinlock lock;
  struct file file[NFILE];
} ftable;

void
fileinit(void)
{
  initlock(&ftable.lock, "ftable");
}

// Allocate a file structure.
struct file*
filealloc(void)
{
  struct file *f;

  acquire(&ftable.lock);
  for(f = ftable.file; f < ftable.file + NFILE; f++){
    if(f->ref == 0){
      f->ref = 1;
      release(&ftable.lock);
      return f;
    }
  }
  release(&ftable.lock);
  return 0;
}

// Increment ref count for file f.
struct file*
filedup(struct file *f)
{
  acquire(&ftable.lock);
  if(f->ref < 1)
    panic("filedup");
  f->ref++;
  release(&ftable.lock);
  return f;
}

// Close file f.  (Decrement ref count, close when reaches 0.)
void
fileclose(struct file *f)
{
  struct file ff;

  acquire(&ftable.lock);
  if(f->ref < 1)
    panic("fileclose");
  if(--f->ref > 0){
    release(&ftable.lock);
    return;
  }
  ff = *f;
  f->ref = 0;
  f->type = FD_NONE;
  release(&ftable.lock);

  if(ff.type == FD_PIPE)
    pipeclose(ff.pipe, ff.writable);
  else if(ff.type == FD_INODE){
    begin_op();
    iput(ff.ip);
    end_op();
  }
}

// Get metadata about file f.
int
filestat(struct file *f, struct stat *st)
{
  if(f->type == FD_INODE){
    ilock(f->ip);
    stati(f->ip, st);
    iunlock(f->ip);
    return 0;
  }
  return -1;
}

// Read from file f.
int
fileread(struct file *f, char *addr, int n)
{
  int r;

  if(f->readable == 0)
    return -1;
  if(f->type == FD_PIPE)
    return piperead(f->pipe, addr, n);
  if(f->type == FD_INODE){
    ilock(f->ip);
    if((r = readi(f->ip, addr, f->off, n)) > 0)
      f->off += r;
    iunlock(f->ip);
    return r;
  }
  panic("fileread");
}

//PAGEBREAK!
// Write to file f.
int
filewrite(struct file *f, char *addr, int n)
{
  int r;

  if(f->writable == 0)
    return -1;
  if(f->type == FD_PIPE)
    return pipewrite(f->pipe, addr, n);
  if(f->type == FD_INODE){
    // write a few blocks at a time to avoid exceeding
    // the maximum log transaction size, including
    // i-node, indirect block, allocation blocks,
    // and 2 blocks of slop for non-aligned writes.
    // this really belongs lower down, since writei()
    // might be writing a device like the console.
    int max = ((MAXOPBLOCKS-1-1-2) / 2) * 512;
    int i = 0;
    while(i < n){
      int n1 = n - i;
      if(n1 > max)
        n1 = max;

      begin_op();
      ilock(f->ip);
      if ((r = writei(f->ip, addr + i, f->off, n1)) > 0)
        f->off += r;
      iunlock(f->ip);
      end_op();

      if(r < 0)
        break;
      if(r != n1)
        panic("short filewrite");
      i += r;
    }
    return i == n ? n : -1;
  }
  panic("filewrite");
}

int
make_duplicate(const char *src)
{
  struct inode *sip, *dp, *dip, *tmp;
  char dst[128];
  char name[DIRSIZ];
  const char *suf = "_copy";
  uint off = 0, doff = 0;
  int r;

  if(src == 0)
    return 1;

  int srclen = strlen(src);
  int suflen = strlen(suf);
  if(srclen + suflen >= sizeof(dst))
    return 1;

  safestrcpy(dst, src, sizeof(dst));
  int l = strlen(dst);
  for(int i = 0; i < suflen; i++)
    dst[l + i] = suf[i];
  dst[l + suflen] = 0;

  begin_op();

  if((sip = namei((char*)src)) == 0){
    end_op();
    return -1;
  }
  ilock(sip);

  if(sip->type != T_FILE){
    iunlockput(sip);
    end_op();
    return 1;
  }

  if((dp = nameiparent(dst, name)) == 0){
    iunlockput(sip);
    end_op();
    return 1;
  }
  ilock(dp);

  if((tmp = dirlookup(dp, name, 0)) != 0){
    iunlockput(dp);
    iput(tmp);
    iunlockput(sip);
    end_op();
    return 1;
  }

  if((dip = ialloc(dp->dev, T_FILE)) == 0){
    iunlockput(dp);
    iunlockput(sip);
    end_op();
    return 1;
  }
  ilock(dip);
  dip->major = 0;
  dip->minor = 0;
  dip->nlink = 1;
  iupdate(dip);

  if(dirlink(dp, name, dip->inum) < 0)
    panic("make_duplicate: dirlink");

  iunlockput(dp);
  iunlock(dip);
  end_op();

  char buf[512];
  off = 0;
  doff = 0;

  while(off < sip->size){
    int n1 = sip->size - off;
    if(n1 > sizeof(buf))
      n1 = sizeof(buf);

    r = readi(sip, buf, off, n1);
    if(r <= 0){
      iunlockput(sip);
      iput(dip);
      return 1;
    }

    begin_op();
    ilock(dip);
    int w = writei(dip, buf, doff, r);
    iunlock(dip);
    end_op();

    if(w != r){
      iunlockput(sip);
      iput(dip);
      return 1;
    }

    off  += r;
    doff += w;
  }

  iunlockput(sip);
  iput(dip);
  return 0;
}
