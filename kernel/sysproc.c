#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  // lab4: backtrace
  backtrace();

  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// lab4: alarm
uint64
sys_sigalarm(void)
{
   int ticks;
   void (*handler)();

   struct proc *p = myproc();

   if (argint(0, &ticks) < 0)
       return -1;
   if (ticks < 0)
       return -1;
   if (argaddr(1, (uint64*)&handler) < 0)
       return -1;

   if (ticks == 0) {
       p->alarm_ticks = -1;
       p->alarm_fired = 0;
       p->alarm_handler = 0;
   } else {
       p->alarm_ticks = ticks;
       p->alarm_fired = 0;
       p->alarm_handler = handler;
   }

   return 0;
}

uint64
sys_sigreturn(void)
{
  struct proc *p = myproc();
  p->trapframe->epc = p->alarm_epc;
  p->kstack = p->alarm_kstack;
  p->pagetable = p->alarm_pagetable;
  /* printf("sigreturn epc: %p\n", p->alarm_epc); */
  return 0;
}
