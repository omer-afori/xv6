#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
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
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;


  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}


#ifdef LAB_PGTBL
//gets starting virtual address and number of pages, and a user address to store results in a bitmask (first page is LSB)
//needs to check which page has been accessed, and set the bitmask accordingly. also set the PTE to not accessed.
int
sys_pgaccess(void)
{

  uint64 start_addr;
  int pages;
  uint64 bitmask_addr;
  uint64 addr;
  argaddr(0, &start_addr);
  argint(1, &pages);
  argaddr(2, &bitmask_addr);
  uint64 mask_to_return = 0;

  for (int i = 0; i < pages; i++){
    addr = start_addr + (i * PGSIZE);
    uint64* pte = walk(myproc()->pagetable, addr, 0); //get the PTE
    if (pte == 0){
      return -1;
    }
    if (*pte & PTE_A){
      *pte = *pte & (~PTE_A); //remove accessed bit
      mask_to_return |= (1 << i);
    }
  }
  //copy the mask_to_return to the bitmask_addr:
  if (copyout(myproc()->pagetable, bitmask_addr, (char *)&mask_to_return, sizeof(mask_to_return)) < 0){
    return -1;
  }
  // lab pgtbl: your code here.
  return 0;
}
#endif

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
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
