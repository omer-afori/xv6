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

#define hash(blockno) (blockno % NBUCKETS)

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  
  //struct buf head;
} bcache;

struct {
  struct spinlock lock;
  struct buf head;
} my_hash[NBUCKETS];

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");
  for (int i = 0; i < NBUCKETS; i++){
    initlock(&my_hash[i].lock, "my_hash");
    //initialize the hash table
    my_hash[i].head.next = &my_hash[i].head;
    my_hash[i].head.prev = &my_hash[i].head;
  }


  // Create linked list of buffers

  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = my_hash[0].head.next;
    b->prev = &my_hash[0].head;
    initsleeplock(&b->lock, "buffer");
    my_hash[0].head.next->prev = b;
    my_hash[0].head.next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int low_index;
  int high_index;
  acquire(&my_hash[hash(blockno)].lock);
  
  // Is the block already cached?
  for (b = my_hash[hash(blockno)].head.next; b != &my_hash[hash(blockno)].head; b = b->next) {
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&my_hash[hash(blockno)].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  //for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
  for(b = my_hash[hash(blockno)].head.prev; b != &my_hash[hash(blockno)].head; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&my_hash[hash(blockno)].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // No recyclable buffers - get LRU from other bucket

  for(int i = 0; i < NBUCKETS; i++){
    if(i!= hash(blockno)){

      release(&my_hash[hash(blockno)].lock);
      //for ordering - make sure locks are always in the same order, could lead to deadlock otherwise
      if (i < hash(blockno)){
        low_index = i;
        high_index = hash(blockno);
      }
      else{
        low_index = hash(blockno);
        high_index = i;
      }
      acquire(&my_hash[low_index].lock);
      acquire(&my_hash[high_index].lock);

      for(b = my_hash[i].head.prev; b != &my_hash[i].head; b = b->prev){
        if(b->refcnt == 0) {
          b->prev->next = b->next;
          b->next->prev = b->prev;
          my_hash[hash(blockno)].head.next->prev = b;
          b->next = my_hash[hash(blockno)].head.next;
          my_hash[hash(blockno)].head.next = b;
          b->prev = &my_hash[hash(blockno)].head;


          b->dev = dev;
          b->blockno = blockno;
          b->valid = 0;
          b->refcnt = 1;
          release(&my_hash[i].lock);
          release(&my_hash[hash(blockno)].lock);
          acquiresleep(&b->lock);
          return b;
        }
      }
      release(&my_hash[i].lock);
    } 
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

  acquire(&my_hash[hash(b->blockno)].lock);
  b->refcnt--;
  //no need for the code that was here anymore, although I dont think it would have mattered 
  release(&my_hash[hash(b->blockno)].lock);
}

void
bpin(struct buf *b) {
  acquire(&my_hash[hash(b->blockno)].lock);
  b->refcnt++;
  release(&my_hash[hash(b->blockno)].lock);
}

void
bunpin(struct buf *b) {
  acquire(&my_hash[hash(b->blockno)].lock);
  b->refcnt--;
  release(&my_hash[hash(b->blockno)].lock);
}


