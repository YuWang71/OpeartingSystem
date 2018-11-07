#include <stdio.h>
#include <cpu.h>
#include <gates.h>
#include "tsystm.h"
#include "tsyscall.h"

extern void locate_idt(unsigned int *limitp, char ** idtp);
extern void ustart(void);
extern IntHandler syscall;
extern void wrapup(int exitcode);

void tiny_kernel_init(void);
void syscallc(int user_eax, int arg1, char * arg2, int arg3);

static struct sysent {
  char        sy_narg;
  int         (*sy_call)();
} sysent[TNSYSCALL];

/* write the nth idt descriptor as an interrupt gate to inthand_addr */
static void set_trap_gate(int n, IntHandler *inthand_addr)
{
  char *idt_LAaddr;		/* linear address of IDT */
  char *idt_VAaddr;		/* virtual address of IDT */
  Gate_descriptor *idt, *desc;
  unsigned int limit = 0;

  locate_idt(&limit,&idt_LAaddr);
  /* convert to virtual address, i.e., ordinary address */
  idt_VAaddr = idt_LAaddr - KERNEL_BASE_LA;  /* usable address of IDT */
  /* convert to a typed pointer, for an array of Gate_descriptor */
  idt = (Gate_descriptor *)idt_VAaddr;
  desc = &idt[n];		/* select nth descriptor in idt table */
  /* fill in descriptor */
  desc->selector = KERNEL_CS;	/* CS seg selector for int. handler */
  desc->addr_hi = ((unsigned int)inthand_addr)>>16; /* CS seg offset of inthand  */
  desc->addr_lo = ((unsigned int)inthand_addr)&0xffff;
  desc->flags = GATE_P|GATE_DPL_KERNEL|GATE_TRAPGATE; /* valid, interrupt */
  desc->zero = 0;
}

static int sysexit(int exitcode)
{
  kprintf("Error code from main returned to sysexit in tunix.c %d\n", exitcode);
  wrapup(exitcode);
  kprintf("Should not reach here\n");
  return 0;
}

void syscallc(int user_eax, int arg1, char * arg2, int arg3)
{
  char sy_narg = sysent[user_eax].sy_narg;
  switch(sy_narg) {
    case 1:
      user_eax = sysent[user_eax].sy_call(arg1);
      break;
    case 3:
      user_eax = sysent[user_eax].sy_call(arg1, arg2, arg3);
      break;
    default:
      kprintf("Not supported system call num :%d\n", user_eax);
  }
}

static void sysent_init(void)
{
  sysent[TSYS_EXIT].sy_narg = 1;
  sysent[TSYS_EXIT].sy_call = sysexit;
  sysent[TSYS_READ].sy_narg = 3;
  sysent[TSYS_READ].sy_call = sysread;
  sysent[TSYS_WRITE].sy_narg = 3;
  sysent[TSYS_WRITE].sy_call = syswrite;
}

void tiny_kernel_init(void)
{
  ioinit();
  set_trap_gate(0x80, &syscall);
  sysent_init();
  ustart();
}
