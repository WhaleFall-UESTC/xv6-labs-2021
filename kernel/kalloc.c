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

// typedef struct {
//   uint16 index;
//   uint16 cnt;
// } pgcnt;
// pgcnt *pgcnts;
// int pgcnts_sz = 0;

// inline void resize(int newsz) {
//   pgcnt *newpgcnts = (pgcnt *) malloc (newsz * sizeof(pgcnt));
//   for (int i = 0; i < pgcnts; i++)
//     newpgcnts[i] = pgcnts[i];
//   free(pgcnts);
//   pgcnts = newpgcnts;
// }

// inline int search(int pgindex) {
//   for (int i = 0; i < pgcnts_sz; i++)
//     if (pgcnts[i].index == pgindex)
//       return i;
//   return -1;
// }

// inline void add(int pgindex) {
//   int index = search(pgindex);
//   if (index < 0) {
//     pgcnts[pgcnts_sz++] = (pgcnt) {.index = pgindex, .cnt = 1};
//     int len = sizeof(pgcnts) / sizeof(pgcnt);
//     if (pgcnts_sz >= len)
//       resize(2 * len);
//   } else {
//     pgcnts[index].cnt++;
//   }
// }

// inline int sub(int pgindex) {
//   int index = search(pgindex);
//   if (index < 0) return -1;
//   pgcnts[index].cnt--;
//   if (pgcnts[index].cnt == 0) {
//     int ret = pgcnts[index].index;
//     pgcnts[index] = pgcnts[--pgcnts_sz];
//     int len = sizeof(pgcnts) / sizeof(pgcnt);
//     if (pgcnts_sz <= len / 4)
//       resize(len / 2);
//     return ret;
//   }
//   return 0;
// }

// #define pa2pgindex(pa) ((PGROUNDDOWN(va)) / PGSIZE)
// #define pgindex2pa(pgindex) (pgindex << 12)

char pgcnt[32750];
inline uint64 index(void* PA) {
  return ((PGROUNDDOWN((uint64) PA) - ((uint64) end)) / PGSIZE);
}
void addpgcnt(uint64 pa) { pgcnt[(PGROUNDDOWN(pa) - ((uint64) end))/ PGSIZE]++; }

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    kfree(p);
    pgcnt[index(p)] = 0;
  }
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  if (pgcnt[index(pa)] != 0)
    pgcnt[index(pa)]--;

  if (pgcnt[index(pa)] != 0) 
    return;
  // Fill with junk to catch dangling refs.
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

  if(r) {
    memset((char*)r, 5, PGSIZE); // fill with junk
    pgcnt[index(r)] = 1;
  }
  return (void*)r;
}
