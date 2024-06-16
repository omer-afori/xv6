// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct {
  struct spinlock lock;
  int reference_count[REFERENCE_INDEX(PHYSTOP)];
} ref;

void reference_increase(uint64 addr){
  acquire(&ref.lock);
  ref.reference_count[REFERENCE_INDEX((uint64)addr)]++;
  release(&ref.lock);
}
void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&ref.lock,"ref");
  for (int i = 0; i < REFERENCE_INDEX(PHYSTOP); i++){
    ref.reference_count[i] = 1;
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  acquire(&ref.lock);
  ref.reference_count[REFERENCE_INDEX((uint64)pa)]--;
  if (ref.reference_count[REFERENCE_INDEX((uint64)pa)] > 0){
    release(&ref.lock);
    return;
  }
  release(&ref.lock);
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r){
    acquire(&ref.lock);
    ref.reference_count[REFERENCE_INDEX((uint64)r)] = 1;
    release(&ref.lock);
    memset((char*)r, 5, PGSIZE); // fill with junk
  }
  return (void*)r;
}
