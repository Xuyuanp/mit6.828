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

#define NBUCKETS 17

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  struct buf heads[NBUCKETS];
  struct spinlock locks[NBUCKETS];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;
} bcache;

uint
hash(uint blockno)
{
  return blockno % NBUCKETS;
}

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  for (int i = 0; i < NBUCKETS; i++) {
    initlock(&bcache.locks[i], "bcache.bucket");
    bcache.heads[i].next = &bcache.heads[i];
    bcache.heads[i].prev = &bcache.heads[i];
  }

  // dispatch bufs to buckets
  for (int i = 0; i < NBUF; i++) {
    b = bcache.buf + i;
    uint bucket = hash(i);
    b->next = bcache.heads[bucket].next;
    b->prev = &bcache.heads[bucket];
    initsleeplock(&b->lock, "buffer");
    bcache.heads[bucket].next->prev = b;
    bcache.heads[bucket].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  uint nbucket;
  uint bucket = hash(blockno);

  acquire(&bcache.locks[bucket]);

  // Is the block already cached in bucket?
  for (b = bcache.heads[bucket].next; b != &bcache.heads[bucket]; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bcache.locks[bucket]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  release(&bcache.locks[bucket]);

  // Not cached.
  for (int i = 1; i < NBUCKETS; i++) {
    nbucket = (bucket + i) % NBUCKETS;
    acquire(&bcache.locks[nbucket]);
    for (b = bcache.heads[nbucket].prev; b != &bcache.heads[nbucket]; b = b->prev) {
      if (b->refcnt == 0) {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;

        // remove buf from nbucket
        b->next->prev = b->prev;
        b->prev->next = b->next;

        release(&bcache.locks[nbucket]);

        // move buc to bucket
        acquire(&bcache.locks[bucket]);

        b->next = bcache.heads[bucket].next;
        bcache.heads[bucket].next->prev = b;
        b->prev = &bcache.heads[bucket];
        bcache.heads[bucket].next = b;

        release(&bcache.locks[bucket]);

        acquiresleep(&b->lock);
        return b;
      }
    }
    release(&bcache.locks[nbucket]);
  }
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

  uint bucket = hash(b->blockno);

  acquire(&bcache.locks[bucket]);

  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.heads[bucket].next;
    b->prev = &bcache.heads[bucket];
    bcache.heads[bucket].next->prev = b;
    bcache.heads[bucket].next = b;
  }

  release(&bcache.locks[bucket]);
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}


