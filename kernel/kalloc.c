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
void cpu_freerange(int cpu_id, void *pa_start, void *pa_end);
void cpu_kfree(int cpu_id, void *pa);
struct run *kstealfree(int cpu_id, uint64 want, uint64 *got);

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
  struct run *freelist;
} kmems[NCPU];

void
kinit()
{
  uint64 step = (PHYSTOP - (uint64)end) / NCPU;
  for (int i = 0; i < NCPU; i++) {
    char buf[32];
    snprintf(buf, 32, "kmem-%d", i);
    initlock(&kmems[i].lock, buf);
    cpu_freerange(i, (void*)(end + i * step), (void*)(end + (i + 1) * step));
  }
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

void
cpu_freerange(int cpu_id, void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    cpu_kfree(cpu_id, p);
}

void
cpu_kfree(int cpu_id, void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmems[cpu_id].lock);
  r->next = kmems[cpu_id].freelist;
  kmems[cpu_id].freelist = r;
  release(&kmems[cpu_id].lock);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  push_off();
  int cpu_id = cpuid();
  pop_off();

  cpu_kfree(cpu_id, pa);
}

struct run *
kstealfree(int cpu_id, uint64 want, uint64 *got)
{
  struct run *r;
  struct run *tail;
  *got = 0;
  acquire(&kmems[cpu_id].lock);

  r = kmems[cpu_id].freelist;
  tail = r;

  for (int i = 0; i < want && tail != 0; i++) {
    tail = tail->next;
    (*got)++;
  }

  if (tail) {
    kmems[cpu_id].freelist = tail->next;
    tail->next = 0;
  } else {
    kmems[cpu_id].freelist = 0;
  }

  release(&kmems[cpu_id].lock);

  return r;
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  push_off();
  int cpu_id = cpuid();
  pop_off();
  struct run *r;

  acquire(&kmems[cpu_id].lock);
  r = kmems[cpu_id].freelist;
  if(r)
    kmems[cpu_id].freelist = r->next;
  release(&kmems[cpu_id].lock);

  if(r) {
    memset((char*)r, 5, PGSIZE); // fill with junk
    return (void*)r;
  }

  uint64 got = 0;
  uint64 want = 32;
  for (int i = 1; i < NCPU; i++) {
    int next_cpu = (cpu_id + i) % NCPU;
    r = kstealfree(next_cpu, want, &got);
    if (r) {
      acquire(&kmems[cpu_id].lock);

      if (kmems[cpu_id].freelist == 0) {
        kmems[cpu_id].freelist = r->next;
      } else {
        struct run *tmp = kmems[cpu_id].freelist;
        for (; tmp->next != 0; tmp = tmp->next);
        tmp->next = r->next;
      }

      release(&kmems[cpu_id].lock);

      memset((char*)r, 5, PGSIZE); // fill with junk
      return (void*)r;
    }
  }
  return 0;
}
