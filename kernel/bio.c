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

#define NBUCKET 13
#define hash(blockno) (blockno % NBUCKET) 

struct {
  struct spinlock lock;
  struct buf buf[NBUF];
  int cntbuf;
  struct buf buckets[NBUCKET];
  struct spinlock bcklocks[NBUCKET];
  struct spinlock hashlock;

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  // struct buf head;
} bcache;

void
binit(void)
{
  // struct buf *b;

  initlock(&bcache.lock, "bcache");
  initlock(&bcache.hashlock, "hashlock");
  for(int i = 0; i < NBUCKET; i++) 
    initlock(&bcache.bcklocks[i], "bucketlock");
  for (int i = 0; i < NBUF; i++) 
    initsleeplock(&bcache.buf[i].lock, "buffer");
  bcache.cntbuf = 0;
  // Create linked list of buffers
  // bcache.head.prev = &bcache.head;
  // bcache.head.next = &bcache.head;
  // for(b = bcache.buf; b < bcache.buf+NBUF; b++){
  //   b->next = bcache.head.next;
  //   b->prev = &bcache.head;
  //   initsleeplock(&b->lock, "buffer");
  //   bcache.head.next->prev = b;
  //   bcache.head.next = b;
  // }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  // acquire(&bcache.lock);

  // Is the block already cached?
  int index = hash(blockno);
  acquire(&bcache.bcklocks[index]);
  for (b = bcache.buckets[index].next; b; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bcache.bcklocks[index]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.bcklocks[index]);
  // for(b = bcache.head.next; b != &bcache.head; b = b->next){
  //   if(b->dev == dev && b->blockno == blockno){
  //     b->refcnt++;
  //     release(&bcache.lock);
  //     acquiresleep(&b->lock);
  //     return b;
  //   }
  // }

  // Not cached.
  // check if there is some buf not in buckets
  acquire(&bcache.lock);
  if (bcache.cntbuf < NBUF) {
    b = &bcache.buf[bcache.cntbuf++];
    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    b->refcnt = 1;
    acquire(&bcache.bcklocks[index]);
    b->next = bcache.buckets[index].next;
    bcache.buckets[index].next = b;
    release(&bcache.bcklocks[index]);
    release(&bcache.lock);
    acquiresleep(&b->lock);
    return b;
  }
  release(&bcache.lock);

  // // Recycle the least recently used (LRU) unused buffer.
  struct buf *min_block = 0, *min_block_pre = 0, *pre = 0;
  int idx = index, i;
  uint min_ticks;
  acquire(&bcache.hashlock);
  for(i = 0; i < NBUCKET; ++i) {
      min_ticks = -1;
      acquire(&bcache.bcklocks[idx]);
      for(pre = &bcache.buckets[idx], b = pre->next; b; pre = b, b = b->next) {
          if(idx == hash(blockno) && b->dev == dev && b->blockno == blockno){
              b->refcnt++;
              release(&bcache.bcklocks[idx]);
              release(&bcache.hashlock);
              acquiresleep(&b->lock);
              return b;
          }
          if(b->refcnt == 0 && b->ticks < min_ticks) {
              min_block = b;
              min_block_pre = pre;
              min_ticks = b->ticks;
          }
      }

      if(min_block) {
          min_block->dev = dev;
          min_block->blockno = blockno;
          min_block->valid = 0;
          min_block->refcnt = 1;
          if(idx != hash(blockno)) {
              min_block_pre->next = min_block->next;    
              release(&bcache.bcklocks[idx]);
              idx = hash(blockno);  
              acquire(&bcache.bcklocks[idx]);
              min_block->next = bcache.buckets[idx].next;    
              bcache.buckets[idx].next = min_block;
          }
          release(&bcache.bcklocks[idx]);
          release(&bcache.hashlock);
          acquiresleep(&min_block->lock);
          return min_block;
      }
      release(&bcache.bcklocks[idx]);
      if(++idx == NBUCKET) {
          idx = 0;
      }
  }

  // uint min_ticks = -1, min_blockno = -1;
  // struct buf *min_block = 0, *min_block_pre = 0;
  // int pre_idx = -1;
  // acquire(&bcache.hashlock);
  // for (int i = 0; i < NBUCKET; i++) {
  //   int idx = (index + i) % NBUCKET;
  //   ushort flag = 1;
  //   acquire(&bcache.bcklocks[idx]);
  //   b = &bcache.buckets[idx]; 
  //   while (b->next) {
  //     if (b->next->refcnt == 0 && b->next->ticks < min_ticks) {
  //       min_block_pre = b;
  //       min_block = b->next;
  //       min_ticks = b->next->ticks;
  //       min_blockno = b->next->blockno;
  //       flag = 0;
  //     }
  //     b = b->next;
  //   }
  //   if (flag)
  //     release(&bcache.bcklocks[idx]);
  //   else if (pre_idx != -1) {
  //     release(&bcache.bcklocks[pre_idx]);
  //   } else {
  //     pre_idx = idx;
  //   }
  // }
  // if (min_blockno == -1) 
  //   panic("bget: no buffers");
  // int idx_min = hash(min_blockno);
  // if (!holding(&bcache.bcklocks[idx_min]))
  //   panic("bget: not holding idx_min");

  // if (index != idx_min) {
  //   // 一定要放在里面，因为 idx_min此时正锁着
  //   acquire(&bcache.bcklocks[index]);
  //   min_block_pre->next = 0;
  //   min_block->next = bcache.buckets[index].next;
  //   bcache.buckets[index].next = min_block;
  //   min_block->blockno = blockno;
  //   min_block->dev = dev;
  //   min_block->valid = 0;
  //   min_block->refcnt = 1;
  //   release(&bcache.bcklocks[index]);
  //   release(&bcache.bcklocks[idx_min]);
  // } else {
  //   min_block->blockno = blockno;
  //   min_block->dev = dev;
  //   min_block->valid = 0;
  //   min_block->refcnt = 1;
  //   release(&bcache.bcklocks[idx_min]);
  // }
  // release(&bcache.hashlock);
  // acquiresleep(&min_block->lock);
  // return min_block;


  // for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
  //   if(b->refcnt == 0) {
  //     b->dev = dev;
  //     b->blockno = blockno;
  //     b->valid = 0;
  //     b->refcnt = 1;
  //     release(&bcache.lock);
  //     acquiresleep(&b->lock);
  //     return b;
  //   }
  // }

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

  int i = hash(b->blockno);
  acquire(&bcache.bcklocks[i]);
  // acquire(&bcache.lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    // b->next->prev = b->prev;
    // b->prev->next = b->next;
    // b->next = bcache.head.next;
    // b->prev = &bcache.head;
    // bcache.head.next->prev = b;
    // bcache.head.next = b;
    b->ticks = ticks;
  }
  
  release(&bcache.bcklocks[i]);
}

void
bpin(struct buf *b) {
  int i = hash(b->blockno);
  acquire(&bcache.bcklocks[i]);
  b->refcnt++;
  release(&bcache.bcklocks[i]);
}

void
bunpin(struct buf *b) {
  int i = hash(b->blockno);
  acquire(&bcache.bcklocks[i]);
  b->refcnt--;
  release(&bcache.bcklocks[i]);
}


