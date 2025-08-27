// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define BUCKETCNT 13

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;
} bcache;

struct bucket {
  struct spinlock lock;
  struct buf head;
} hashtable[BUCKETCNT];

void
binit(void)
{
  int i;

  initlock(&bcache.lock, "bcache");

  for (i = 0; i < BUCKETCNT; ++i) {
    initlock(&hashtable[i].lock, "bcache_hash");
    hashtable[i].head.prev = &hashtable[i].head;
    hashtable[i].head.next = &hashtable[i].head;
  }
  i = 0;
  for (struct buf *b = bcache.buf, *end = b + NBUF;
       b < end; ++b, i = (i + 1) % BUCKETCNT) {
    b->next = hashtable[i].head.next;
    b->prev = &hashtable[i].head;
    initsleeplock(&b->lock, "buffer");
    hashtable[i].head.next->prev = b;
    hashtable[i].head.next = b;

    b->timestamp = 0;
    b->hash = i;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  struct buf *lru = 0;
  uint hash = blockno % BUCKETCNT;
  struct bucket *pbh = hashtable + hash;
  uint min_timestamp = ticks + 114514;

  acquire(&pbh->lock);

  for (b = pbh->head.next; b != &pbh->head; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      ++b->refcnt;
      release(&pbh->lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  for (int i = (hash + 1) % BUCKETCNT; i != hash; i = (i + 1) % BUCKETCNT) {
    struct bucket *pbi = hashtable + i;
    acquire(&pbi->lock);

    for (b = pbi->head.next; b != &pbi->head; b = b->next) {
      if (b->refcnt == 0 && b->timestamp < min_timestamp) {
        lru = b;
        min_timestamp = b->timestamp;
      }
    }

    if (!lru) {
      release(&pbi->lock);
      continue;
    }

    b = lru;
    b->next->prev = b->prev;
    b->prev->next = b->next;
    release(&pbi->lock);

    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    b->refcnt = 1;
    b->hash = hash;

    b->next = pbh->head.next;
    b->prev = &pbh->head;

    acquiresleep(&b->lock);

    pbh->head.next->prev = b;
    pbh->head.next = b;
    release(&pbh->lock);

    return b;
  }

  release(&pbh->lock);
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  struct bucket *pb = hashtable + b->hash;
  acquire(&pb->lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->timestamp = ticks;
  }
  release(&pb->lock);
}

void
bpin(struct buf *b) {
  struct bucket *pb = hashtable + b->hash;
  acquire(&pb->lock);
  ++b->refcnt;
  release(&pb->lock);
}

void
bunpin(struct buf *b) {
  struct bucket *pb = hashtable + b->hash;
  acquire(&pb->lock);
  --b->refcnt;
  release(&pb->lock);
}
