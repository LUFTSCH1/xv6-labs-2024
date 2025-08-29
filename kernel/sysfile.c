//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "spinlock.h"
#include "proc.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "fcntl.h"

// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
static int
argfd(int n, int *pfd, struct file **pf)
{
  int fd;
  struct file *f;

  argint(n, &fd);
  if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
    return -1;
  if(pfd)
    *pfd = fd;
  if(pf)
    *pf = f;
  return 0;
}

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
static int
fdalloc(struct file *f)
{
  int fd;
  struct proc *p = myproc();

  for(fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd] == 0){
      p->ofile[fd] = f;
      return fd;
    }
  }
  return -1;
}

uint64
sys_dup(void)
{
  struct file *f;
  int fd;

  if(argfd(0, 0, &f) < 0)
    return -1;
  if((fd=fdalloc(f)) < 0)
    return -1;
  filedup(f);
  return fd;
}

uint64
sys_read(void)
{
  struct file *f;
  int n;
  uint64 p;

  argaddr(1, &p);
  argint(2, &n);
  if(argfd(0, 0, &f) < 0)
    return -1;
  return fileread(f, p, n);
}

uint64
sys_write(void)
{
  struct file *f;
  int n;
  uint64 p;
  
  argaddr(1, &p);
  argint(2, &n);
  if(argfd(0, 0, &f) < 0)
    return -1;

  return filewrite(f, p, n);
}

uint64
sys_close(void)
{
  int fd;
  struct file *f;

  if(argfd(0, &fd, &f) < 0)
    return -1;
  myproc()->ofile[fd] = 0;
  fileclose(f);
  return 0;
}

uint64
sys_fstat(void)
{
  struct file *f;
  uint64 st; // user pointer to struct stat

  argaddr(1, &st);
  if(argfd(0, 0, &f) < 0)
    return -1;
  return filestat(f, st);
}

// Create the path new as a link to the same inode as old.
uint64
sys_link(void)
{
  char name[DIRSIZ], new[MAXPATH], old[MAXPATH];
  struct inode *dp, *ip;

  if(argstr(0, old, MAXPATH) < 0 || argstr(1, new, MAXPATH) < 0)
    return -1;

  begin_op();
  if((ip = namei(old)) == 0){
    end_op();
    return -1;
  }

  ilock(ip);
  if(ip->type == T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }

  ip->nlink++;
  iupdate(ip);
  iunlock(ip);

  if((dp = nameiparent(new, name)) == 0)
    goto bad;
  ilock(dp);
  if(dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0){
    iunlockput(dp);
    goto bad;
  }
  iunlockput(dp);
  iput(ip);

  end_op();

  return 0;

bad:
  ilock(ip);
  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);
  end_op();
  return -1;
}

// Is the directory dp empty except for "." and ".." ?
static int
isdirempty(struct inode *dp)
{
  int off;
  struct dirent de;

  for(off=2*sizeof(de); off<dp->size; off+=sizeof(de)){
    if(readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
      panic("isdirempty: readi");
    if(de.inum != 0)
      return 0;
  }
  return 1;
}

uint64
sys_unlink(void)
{
  struct inode *ip, *dp;
  struct dirent de;
  char name[DIRSIZ], path[MAXPATH];
  uint off;

  if(argstr(0, path, MAXPATH) < 0)
    return -1;

  begin_op();
  if((dp = nameiparent(path, name)) == 0){
    end_op();
    return -1;
  }

  ilock(dp);

  // Cannot unlink "." or "..".
  if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
    goto bad;

  if((ip = dirlookup(dp, name, &off)) == 0)
    goto bad;
  ilock(ip);

  if(ip->nlink < 1)
    panic("unlink: nlink < 1");
  if(ip->type == T_DIR && !isdirempty(ip)){
    iunlockput(ip);
    goto bad;
  }

  memset(&de, 0, sizeof(de));
  if(writei(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
    panic("unlink: writei");
  if(ip->type == T_DIR){
    dp->nlink--;
    iupdate(dp);
  }
  iunlockput(dp);

  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);

  end_op();

  return 0;

bad:
  iunlockput(dp);
  end_op();
  return -1;
}

static struct inode*
create(char *path, short type, short major, short minor)
{
  struct inode *ip, *dp;
  char name[DIRSIZ];

  if((dp = nameiparent(path, name)) == 0)
    return 0;

  ilock(dp);

  if((ip = dirlookup(dp, name, 0)) != 0){
    iunlockput(dp);
    ilock(ip);
    if(type == T_FILE && (ip->type == T_FILE || ip->type == T_DEVICE))
      return ip;
    iunlockput(ip);
    return 0;
  }

  if((ip = ialloc(dp->dev, type)) == 0){
    iunlockput(dp);
    return 0;
  }

  ilock(ip);
  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;
  iupdate(ip);

  if(type == T_DIR){  // Create . and .. entries.
    // No ip->nlink++ for ".": avoid cyclic ref count.
    if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
      goto fail;
  }

  if(dirlink(dp, name, ip->inum) < 0)
    goto fail;

  if(type == T_DIR){
    // now that success is guaranteed:
    dp->nlink++;  // for ".."
    iupdate(dp);
  }

  iunlockput(dp);

  return ip;

 fail:
  // something went wrong. de-allocate ip.
  ip->nlink = 0;
  iupdate(ip);
  iunlockput(ip);
  iunlockput(dp);
  return 0;
}

uint64
sys_open(void)
{
  char path[MAXPATH];
  int fd, omode;
  struct file *f;
  struct inode *ip;
  int n;

  argint(1, &omode);
  if((n = argstr(0, path, MAXPATH)) < 0)
    return -1;

  begin_op();

  if(omode & O_CREATE){
    ip = create(path, T_FILE, 0, 0);
    if(ip == 0){
      end_op();
      return -1;
    }
  } else {
    if((ip = namei(path)) == 0){
      end_op();
      return -1;
    }
    ilock(ip);
    if(ip->type == T_DIR && omode != O_RDONLY){
      iunlockput(ip);
      end_op();
      return -1;
    }
  }

  if(ip->type == T_DEVICE && (ip->major < 0 || ip->major >= NDEV)){
    iunlockput(ip);
    end_op();
    return -1;
  }

  if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
    if(f)
      fileclose(f);
    iunlockput(ip);
    end_op();
    return -1;
  }

  if(ip->type == T_DEVICE){
    f->type = FD_DEVICE;
    f->major = ip->major;
  } else {
    f->type = FD_INODE;
    f->off = 0;
  }
  f->ip = ip;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);

  if((omode & O_TRUNC) && ip->type == T_FILE){
    itrunc(ip);
  }

  iunlock(ip);
  end_op();

  return fd;
}

uint64
sys_mkdir(void)
{
  char path[MAXPATH];
  struct inode *ip;

  begin_op();
  if(argstr(0, path, MAXPATH) < 0 || (ip = create(path, T_DIR, 0, 0)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

uint64
sys_mknod(void)
{
  struct inode *ip;
  char path[MAXPATH];
  int major, minor;

  begin_op();
  argint(1, &major);
  argint(2, &minor);
  if((argstr(0, path, MAXPATH)) < 0 ||
     (ip = create(path, T_DEVICE, major, minor)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

uint64
sys_chdir(void)
{
  char path[MAXPATH];
  struct inode *ip;
  struct proc *p = myproc();
  
  begin_op();
  if(argstr(0, path, MAXPATH) < 0 || (ip = namei(path)) == 0){
    end_op();
    return -1;
  }
  ilock(ip);
  if(ip->type != T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  iput(p->cwd);
  end_op();
  p->cwd = ip;
  return 0;
}

uint64
sys_exec(void)
{
  char path[MAXPATH], *argv[MAXARG];
  int i;
  uint64 uargv, uarg;

  argaddr(1, &uargv);
  if(argstr(0, path, MAXPATH) < 0) {
    return -1;
  }
  memset(argv, 0, sizeof(argv));
  for(i=0;; i++){
    if(i >= NELEM(argv)){
      goto bad;
    }
    if(fetchaddr(uargv+sizeof(uint64)*i, (uint64*)&uarg) < 0){
      goto bad;
    }
    if(uarg == 0){
      argv[i] = 0;
      break;
    }
    argv[i] = kalloc();
    if(argv[i] == 0)
      goto bad;
    if(fetchstr(uarg, argv[i], PGSIZE) < 0)
      goto bad;
  }

  int ret = exec(path, argv);

  for(i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);

  return ret;

 bad:
  for(i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);
  return -1;
}

uint64
sys_pipe(void)
{
  uint64 fdarray; // user pointer to array of two integers
  struct file *rf, *wf;
  int fd0, fd1;
  struct proc *p = myproc();

  argaddr(0, &fdarray);
  if(pipealloc(&rf, &wf) < 0)
    return -1;
  fd0 = -1;
  if((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0){
    if(fd0 >= 0)
      p->ofile[fd0] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  if(copyout(p->pagetable, fdarray, (char*)&fd0, sizeof(fd0)) < 0 ||
     copyout(p->pagetable, fdarray+sizeof(fd0), (char *)&fd1, sizeof(fd1)) < 0){
    p->ofile[fd0] = 0;
    p->ofile[fd1] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  return 0;
}

#ifdef LAB_MMAP
uint64
sys_mmap(void)
{
  struct proc *p = myproc();
  struct vma *ptvma = (void *)0;
  for (int i = 0; i < NVMA; ++i) {
    struct vma *vp = p->pvma + i;
    if (vp->npages == 0) {
      ptvma = vp;
      break;
    }
  }
  if (!ptvma) {
    return -1;
  }
  argaddr(0, &ptvma->addr);
  argsize_t(1, &ptvma->len);
  argint(2, &ptvma->prot);
  argint(3, &ptvma->flags);
  argint(4, &ptvma->fd);
  argoff_t(5, &ptvma->offset);
  if (ptvma->addr || ptvma->offset) { // 题目假定为0，只处理为0的情况
    panic("mmap: <addr> or <offset> is not 0");
  }

  p->sz = PGROUNDUP(p->sz);
  ptvma->addr = p->sz;
  ptvma->vfile = p->ofile[ptvma->fd];
  if (((ptvma->prot & PROT_READ) && !((ptvma->vfile)->readable)) ||
      ((ptvma->flags & MAP_SHARED) && (ptvma->prot & PROT_WRITE) && !((ptvma->vfile)->writable))) {
    return ~(uint64)0;
  }

  filedup(ptvma->vfile);
  ptvma->npages = PGROUNDUP(ptvma->len) / PGSIZE;
  p->sz += ptvma->npages * PGSIZE;
  return ptvma->addr;
}
#endif

#ifdef LAB_MMAP
uint64
sys_munmap(void)
{
  uint64 addr;
  size_t len;
  argaddr(0, &addr);
  argsize_t(1, &len);
  struct proc *p = myproc();
  struct vma *ptvma;
  int i = 0;
  for( ; i < NVMA; ++i) {
    ptvma = p->pvma + i;
    if (addr >= ptvma->addr && addr < ptvma->addr + ptvma->len) {
      munmap(i, p, addr, len);
      break;
    }
  }
  return i < NVMA;
}
#endif

#ifdef LAB_MMAP
static int
munmapfilewrite(struct file *f, uint64 addr, off_t off, int n)
{
  int max = ((MAXOPBLOCKS - 1 - 1 - 2) / 2) * BSIZE;
  int i = 0;
  for (int r = 0; i < n; i += r) {
    int n1 = n - i;
    if (n1 > max) {
      n1 = max;
    }

    begin_op();
    ilock(f->ip);
    if ((r = writei(f->ip, 1, addr + i, off, n1)) > 0) {
      off += r;
    }
    iunlock(f->ip);
    end_op();

    if (r != n1) { // writei错误
      break;
    }
  }
  return (i == n) ? n : -1;
}
#endif

#ifdef LAB_MMAP
int
munmap(int i, struct proc *p, uint64 addr, size_t len)
{
  int do_free = 0;
  struct vma *ptvma = p->pvma + i;
  struct file *vfile = ptvma->vfile;
  uint fizesize = vfile->ip->size;
  uint64 va = addr;
  int npages = PGROUNDUP(len) / PGSIZE;
  int can_write = (ptvma->flags & MAP_SHARED) && (ptvma->prot & PROT_WRITE) && (vfile->writable);
  for (int j = 0; j < npages; ++j) {
    if (walkaddr(p->pagetable, va) != 0) {
      if (can_write) {
        int off = va - addr;
        int n = (off + PGSIZE + ptvma->offset > fizesize) ? fizesize % PGSIZE : PGSIZE;
        if (munmapfilewrite(vfile, va, ptvma->offset + off, n) == -1) {
          return -1;
        }
      }
      uvmunmap(p->pagetable, va, 1, do_free);
    }
    va += PGSIZE;
  }
  if (addr == ptvma->addr) {
    ptvma->addr = va;
    ptvma->offset += npages * PGSIZE;
  }
  ptvma->npages -= npages;
  ptvma->len  -=  len;
  if (ptvma->npages == 0) {
    fileclose(ptvma->vfile);
    ptvma->vfile = 0;
  }
  return 0;
}
#endif

#ifdef LAB_MMAP
int
mmapfaulthandler(struct proc *p, uint64 scause)
{
  if (scause != 13 && scause != 15) {
    return 0; // 不是需要处理的情况
  }
  uint64 va = r_stval();
  va = PGROUNDDOWN(va);
  if (va >= MAXVA) {
    return 0; // 非法
  }
  int i = 0;
  for ( ; i < NVMA; ++i) {
    struct vma trapvma = p->pvma[i];
    if (va >= trapvma.addr && va < trapvma.addr + trapvma.len) {
      int perm = PTE_U | ((trapvma.prot & PROT_READ) ? PTE_R : 0);
      if (trapvma.prot & PROT_WRITE) {
        perm |= PTE_W;
      } else if (scause == 15) {
        return 0; // failed
      }
      uint off = va - trapvma.addr + trapvma.offset;
      char *mem = (char *)kalloc();
      if (!mem) {
        panic("mmapfaulthandler: kalloc");
      }
      memset(mem, 0, PGSIZE);
      struct inode *ip = trapvma.vfile->ip;
      ilock(ip);
      readi(ip, 0, (uint64)mem, off, PGSIZE);
      iunlock(ip);
      if (mappages(p->pagetable, va, PGSIZE, (uint64)mem, perm) != 0) {
        panic("mmapfaulthandler: mappages");
      }
      break;
    }
  }
  return i < NVMA;
}
#endif
