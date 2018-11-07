/* file: tunix.c core kernel code */

#include <stdio.h>
#include <cpu.h>
#include <gates.h>
#include <timer.h>
#include <pic.h>
#include <cpu.h>
#include "tsyscall.h"
#include "tsystm.h"
#include "proc.h"
#include "sched.h"

/* for saved eflags register initialization */
#define EFLAGS_IF 0x200
/* for saved esp register--stack bases for user programs */
/* could use enum here */
#define STACKBASE1 0x3f0000
#define STACKBASE2 0x3e0000
#define STACKBASE3 0x3d0000

extern IntHandler syscall; /* the assembler envelope routine    */
extern void ustart1(void),ustart2(void), ustart3(void);
extern void finale(void);

void k_init(void);
void syscallc( int user_eax, int devcode, char *buff , int bufflen);
void debug_set_trap_gate(int n, IntHandler *inthand_addr, int debug);
void set_trap_gate(int n, IntHandler *inthand_addr);
void process0(void), init_proctab(void), shutdown(void);
int sysexit(int);
 
/* Record debug info in otherwise free memory between program and stack */
/* 0x300000 = 3M, the start of the last M of user memory on the SAPC */
#define DEBUG_AREA 0x300000

char *debug_log_area = (char *)DEBUG_AREA;
char *debug_record;  /* current pointer into log area */ 

/* kernel globals added for scheduler */
PEntry proctab[NPROC],*curproc;
int scheduler_call_site_index[NSCHED_CALL_SITE];
int number_of_zombie;

#define MAX_CALL 6

/* syscall dispatch table */
static  struct sysent {
       short   sy_narg;        /* total number of arguments */
       int     (*sy_call)(int, ...);   /* handler */
} sysent[MAX_CALL];

/* end of kernel globals */

/* TIMER CODE Start here ............. */

extern IntHandler irq0inthand; /* assembler envelope */
void irq0inthandc(void);	/* this is called from irq0inthand */
#define TIMER_COUNT   11932

/* Set up timer to count down from given count, then pulse, repeatedly */
/* These output pulses follow wires to the PIC to provide timer interrupts */
/* count of 0 sets max count, 65536 = 2**16 */
static void set_timer_count(int count)
{
  outpt(TIMER_CNTRL_PORT, TIMER0|TIMER_SET_ALL|TIMER_MODE_RATEGEN);
  outpt(TIMER0_COUNT_PORT,count&0xff); /* set LSB here */
  outpt(TIMER0_COUNT_PORT,count>>8); /* and MSB here */
}

static void tick_handler()
{
  curproc->cpu_field++;
  
  if (curproc == &proctab[0]) {
    debug_log("*0");
  } else if (curproc == &proctab[1]) {
    debug_log("*1");
  } else if (curproc == &proctab[2]) {
    debug_log("*2");
  }

  if (--curproc->p_quantum == 0) {
    /* preempting, later start over with full quantum */
      curproc->p_quantum = QUANTUM;
      cli();
      scheduler(0);  /* find another process to run */
  }
}

/* timer interrupt handler */
void irq0inthandc(void)
{
  pic_end_int();	/* notify PIC with EOI command that its part is done */
  tick_handler();
}

/* Timer init .... */
static void tick_init(int count)
{
 kprintf("Inittializing timer\n");
 // cli();			/* disable ints while setting them up */
 kprintf("Setting IDT interrupt gate descriptor for irq 0 (int n = 0x20)\n");
  set_intr_gate(TIMER0_IRQ+IRQ_TO_INT_N_SHIFT, &irq0inthand);
 kprintf("Commanding PIC to interrupt CPU for irq 0 (TIMER0_IRQ)\n");
  pic_enable_irq(TIMER0_IRQ);
 kprintf("Commanding timer to generate pulse train using this count\n");
  set_timer_count(count);	
 kprintf("Enabling interrupts in CPU\n");
  //icount = 0;
 // sti();
}
/* Timer init .... */

/* Timer shutdown .... */
static void tick_deinit()
{
  int saved_eflags;
  kprintf("Shutting down timer\n");
  saved_eflags = get_eflags();
  cli();
  pic_disable_irq(TIMER0_IRQ);
  set_eflags(saved_eflags);
}
/* Timer shutdown .... */


/* TIMER CODE END here ............. */

/****************************************************************************/
/* k_init: this function for the initialize  of the kernel system*/
/****************************************************************************/

void k_init(void){
  int i = 0;
  debug_record = debug_log_area; /* clear debug log */
  strcpy(debug_record,"");

  cli();
  ioinit();            /* initialize the deivce */ 
  set_trap_gate(0x80, &syscall);   /* SET THE TRAP GATE*/

  /* Note: Could set these with initializers */
  /* Need to cast function pointer type to keep ANSI C happy */
  sysent[TREAD].sy_call = (int (*)(int, ...))sysread;
  sysent[TWRITE].sy_call = (int (*)(int, ...))syswrite;
  sysent[TEXIT].sy_call = (int (*)(int, ...))sysexit;
 
  sysent[TEXIT].sy_narg = 1;    /* set the arg number of function */
  sysent[TREAD].sy_narg = 3;
  sysent[TIOCTL].sy_narg = 3;
  sysent[TWRITE].sy_narg = 3;

  init_proctab();
  for(i = 0; i<NSCHED_CALL_SITE; i++)
    scheduler_call_site_index[i] = 0;

  tick_init(TIMER_COUNT);
  process0();			/* rest of kernel operation (non-init) */
}

/****************************************************************************/
/* process0:  code for process 0: runs when necessary, shuts down            */
/****************************************************************************/
void process0()
{
  int i;
  kprintf("Starting proc 0\n");
  /* execution will come back here when restore process 0*/
  while (number_of_zombie < NPROC-1) { /* proc 0 can't be zombie */  
    sti();			/* let proc 0 take interrupts (important!) */
    cli();
  }
  kprintf("SHUTTING DOWN\n");
  sti();
  for (i=0; i< 1000000; i++)
    ;				/* let output finish (kludge) */
  
  for(i=1;i<NPROC;i++)
    kprintf("\nEXIT CODE OF PROCESS %d: %d\n",i,proctab[i].p_exitval);
  
  shutdown();
  /* note that we can return, in process0, to the startup module
     with its int $3.  It's OK to jump to finale, but not necessary */
}
/****************************************************************************/
/* init_proctab: this function for setting init_sp, init_pc              */
/* zeroing out savededp, and set to RUN                                     */
/****************************************************************************/
void init_proctab()
{
  int i;

  proctab[1].p_savedregs[SAVED_PC] = (int)&ustart1;
  proctab[2].p_savedregs[SAVED_PC] = (int)&ustart2;
  proctab[3].p_savedregs[SAVED_PC] = (int)&ustart3;
   
  proctab[1].p_savedregs[SAVED_ESP] = STACKBASE1;
  proctab[2].p_savedregs[SAVED_ESP] = STACKBASE2;
  proctab[3].p_savedregs[SAVED_ESP] = STACKBASE3;

  for(i=0;i<NPROC;i++){
    proctab[i].p_savedregs[SAVED_EBP] = 0;
    proctab[i].p_savedregs[SAVED_EFLAGS] = EFLAGS_IF; /* make IF=1 */
    proctab[i].p_status=RUN;
    proctab[i].p_quantum = QUANTUM;
    proctab[i].cpu_field = 0;
    proctab[i].num_outchar = 0;
  }

  curproc=&proctab[0];
 
  number_of_zombie=0;
}

/* shut the system down */
void shutdown()
{
  int i = 0;

  tick_deinit();

 for(i=0;i<NPROC;i++){
    kprintf("Proc: %d cpu: %d\n", i, proctab[i].cpu_field);
    kprintf("Proc: %d outchar: %d\n", i, proctab[i].num_outchar);
  }
 kprintf("Scheduler Call Site 0:tick_handler,\n\
     Call Site 1:sysexit\n Call Site 2:sleep\n");

  for(i = 0; i<NSCHED_CALL_SITE; i++)
    kprintf("Call Site-%d called schedule %d times\n", i, scheduler_call_site_index[i]);

  kprintf("SHUTTING THE SYSTEM DOWN!\n");
  kprintf("Debug log from run:\n");
  kprintf("Marking kernel events as follows:\n");
  kprintf("  ^a   COM2 input interrupt, a received\n");
  kprintf("  ~d   COM2 output interrupt, ordinary char output\n");
  kprintf("  ~e   COM2 output interrupt, echo output\n");
  kprintf("  ~s   COM2 output interrupt, shutdown TX ints\n");
  kprintf("  |(1z-2) process switch from 1, now a zombie, to 2\n");
  kprintf("  |(1b-2) process switch from 1, now blocked, to 2\n");
  kprintf("  |(2-1) process switch (by preemption) from 2 to 1\n");
  kprintf("%s", debug_log_area);	/* the debug log from memory */
  kprintf("\nLEAVE KERNEL!\n\n");
  finale();		/* trap to Tutor */
}

/****************************************************************************/
/* syscallc: this function for the C part of the 0x80 trap handler          */
/* OK to just switch on the system call number here                         */
/* By putting the return value of syswrite, etc. in user_eax, it gets       */
/* popped back in sysentry.s and returned to user in eax                    */
/****************************************************************************/

void syscallc( int user_eax, int devcode, char *buff , int bufflen){
  int nargs;
  int syscall_no = user_eax;

  switch(nargs = sysent[syscall_no].sy_narg)
    {
    case 1:         /* 1-argument system call */
	user_eax = sysent[syscall_no].sy_call(devcode);   /* sysexit */
	break;
    case 3:         /* 3-arg system call: calls sysread or syswrite */
	user_eax = sysent[syscall_no].sy_call(devcode,buff,bufflen);
	  if (3 == syscall_no)
      curproc->num_outchar += user_eax;
  break;
    default: kprintf("bad # syscall args %d, syscall #%d\n",
		     nargs, syscall_no);
    }
} 

/****************************************************************************/
/* sysexit: this function for the exit syscall function */
/****************************************************************************/

int sysexit(int exit_code)
{
  int saved_eflags; 
  kprintf("Exit code %d\n", exit_code);
  saved_eflags = get_eflags();
  cli();
  curproc->p_exitval = exit_code;
  curproc->p_status = ZOMBIE;
  number_of_zombie++;
  set_eflags(saved_eflags);
  cli();
  scheduler(1);
  return 0;    /* never happens, but avoids warning about no return value */
}

/****************************************************************************/
/* set_trap_gate: this function for setting the trap gate */
/****************************************************************************/

void set_trap_gate(int n, IntHandler *inthand_addr)
{
  debug_set_trap_gate(n, inthand_addr, 0);
}

/* write the nth idt descriptor as a trap gate to inthand_addr */
void debug_set_trap_gate(int n, IntHandler *inthand_addr, int debug)
{
  char *idt_addr;
  Gate_descriptor *idt, *desc;
  unsigned int limit = 0;
  extern void locate_idt(unsigned int *, char **);

  if (debug)
    kprintf("Calling locate_idt to do sidt instruction...\n");
  locate_idt(&limit,&idt_addr);
  /* convert to CS seg offset, i.e., ordinary address, then to typed pointer */
  idt = (Gate_descriptor *)(idt_addr - KERNEL_BASE_LA);
  if (debug)
    kprintf("Found idt at %x, lim %x\n",idt, limit);
  desc = &idt[n];               /* select nth descriptor in idt table */
  /* fill in descriptor */
  if (debug)
    kprintf("Filling in desc at %x with addr %x\n",(unsigned int)desc,
           (unsigned int)inthand_addr);
  desc->selector = KERNEL_CS;   /* CS seg selector for int. handler */
  desc->addr_hi = ((unsigned int)inthand_addr)>>16; /* CS seg offset of inthand */
  desc->addr_lo = ((unsigned int)inthand_addr)&0xffff;
  desc->flags = GATE_P|GATE_DPL_KERNEL|GATE_TRAPGATE; /* valid, trap */
  desc->zero = 0;
}

/* append msg to memory debugging log */
void debug_log(char *msg)
{
  strcpy(debug_record, msg);
  debug_record +=strlen(msg);
}
