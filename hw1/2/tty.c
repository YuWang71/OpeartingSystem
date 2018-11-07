/*********************************************************************
*
*       file:           tty.c
*       author:         betty o'neil
*
*       tty driver--device-specific routines for ttys 
*
*/
#include <stdio.h>  /* for kprintf prototype */
#include <serial.h>
#include <cpu.h>
#include <pic.h>
#include "ioconf.h"
#include "tty_public.h"
#include "tty.h"
//#include "queue/queue.h"

struct tty ttytab[NTTYS];        /* software params/data for each SLU dev */

/* tell C about the assembler shell routines */
extern void irq3inthand(void), irq4inthand(void);

/* C part of interrupt handlers--specific names called by the assembler code */
extern void irq3inthandc(void), irq4inthandc(void); 

/* the common code for the two interrupt handlers */
static void irqinthandc(int dev); 

static void kickout(int baseport)
{
outpt(baseport+UART_IER, UART_IER_RDI);
outpt(baseport+UART_IER, UART_IER_RDI|UART_IER_THRI);
}


/*====================================================================
*
*       tty specific initialization routine for COM devices
*
*/

void ttyinit(int dev)
{
  int baseport;
  struct tty *tty;		/* ptr to tty software params/data block */

  baseport = devtab[dev].dvbaseport; /* pick up hardware addr */
  tty = (struct tty *)devtab[dev].dvdata; /* and software params struct */

  if (baseport == COM1_BASE) {
      /* arm interrupts by installing int vec */
      set_intr_gate(COM1_IRQ+IRQ_TO_INT_N_SHIFT, &irq4inthand);
      pic_enable_irq(COM1_IRQ);
  } else if (baseport == COM2_BASE) {
      /* arm interrupts by installing int vec */
      set_intr_gate(COM2_IRQ+IRQ_TO_INT_N_SHIFT, &irq3inthand);
      pic_enable_irq(COM2_IRQ);
  } else {
      kprintf("Bad TTY device table entry, dev %d\n", dev);
      return;			/* give up */
  }
  tty->echoflag = 1;		/* default to echoing */
  init_queue(&(tty->rq), MAXBUF); /* initialize readqueue */
  init_queue(&(tty->wq), MAXBUF); /* initialize writequeue */
  init_queue(&(tty->eq), MAXBUF); /* initialize echoqueue */

  /* enable interrupts on receiver */
  outpt(baseport+UART_IER, UART_IER_RDI); /* RDI = receiver data int */
}


/*====================================================================
*
*       Useful function when emptying/filling the read/write buffers
*
*/

#define min(x,y) (x < y ? x : y)


/*====================================================================
*
*       tty-specific read routine for TTY devices
*
*/

int ttyread(int dev, char *buf, int nchar)
{
  int baseport;
  struct tty *tty;
  int i;
  int saved_eflags;        /* old cpu control/status reg, so can restore it */

  baseport = devtab[dev].dvbaseport; /* hardware addr from devtab */
  tty = (struct tty *)devtab[dev].dvdata;   /* software data for line */

  for (i = 0; i < nchar; i++) {
    saved_eflags = get_eflags();
    cli();			/* disable ints in CPU */
    if (0 == emptyqueue(&(tty->rq))) {
	    buf[i] = dequeue(&(tty->rq));      /* copy from readqueue to user buf */
     } else i--; 
    set_eflags(saved_eflags);     /* back to previous CPU int. status */
  }
  return nchar;       /* but should wait for rest of nchar chars if nec. */
  /* this is something for you to correct */
}


/*====================================================================
*
*       tty-specific write routine for SAPC devices
*       (cs444: note that the buffer tbuf is superfluous in this code, but
*        it still gives you a hint as to what needs to be done for
*        the interrupt-driven case)
*
*/
int ttywrite(int dev, char *buf, int nchar)
{
  int baseport;
  struct tty *tty;
  int i, saved_eflags;

  baseport = devtab[dev].dvbaseport; /* hardware addr from devtab */
  tty = (struct tty *)devtab[dev].dvdata;   /* software data for line */

  for (i = 0; i < nchar; i++) {
    saved_eflags = get_eflags();
    cli();
    if (FULLQUE == enqueue(&(tty->wq), buf[i])) {
    	outpt(baseport+UART_IER, UART_IER_RDI | UART_IER_THRI);
	i--;
    }
    set_eflags(saved_eflags);
  }
  if (0 == emptyqueue(&(tty->wq))) {
    	outpt(baseport+UART_IER, UART_IER_RDI | UART_IER_THRI);
  }
  // No need to set shutdown Tx since ISR is shutting Tx if wq is empty
  return nchar;
}

/*====================================================================
*
*       tty-specific control routine for TTY devices
*
*/

int ttycontrol(int dev, int fncode, int val)
{
  struct tty *this_tty = (struct tty *)(devtab[dev].dvdata);

  if (fncode == ECHOCONTROL)
    this_tty->echoflag = val;
  else return -1;
  return 0;
}



/*====================================================================
*
*       tty-specific interrupt routine for COM ports
*
*   Since interrupt handlers don't have parameters, we have two different
*   handlers.  However, all the common code has been placed in a helper 
*   function.
*/
  
void irq4inthandc()
{
  irqinthandc(TTY0);
}                              
  
void irq3inthandc()
{
  irqinthandc(TTY1);
}                              

void irqinthandc(int dev) {  
  int ch;
  struct tty *tty = (struct tty *)(devtab[dev].dvdata);
  int baseport = devtab[dev].dvbaseport; /* hardware i/o port */

  pic_end_int();                /* notify PIC that its part is done */
  switch (inpt(baseport + UART_IIR) & UART_IIR_ID) {
   case UART_IIR_RDI:
  	ch = inpt(baseport+UART_RX);	/* read char, ack the device */
  	/* TBD: Characters are being dropped if queue is full. Is this expected behaviour? */
	enqueue(&(tty->rq), ch);
	if (tty->echoflag) {            /* if echoing wanted */
    	//	outpt(baseport+UART_TX,ch);   /* echo char: see note above */
		enqueue(&(tty->eq), ch);
		kickout(baseport);
	}
	break;
    case UART_IIR_THRI:
//	Prefentially transmit from echo que what does that mean?
	if (0 == emptyqueue(&(tty->eq))) {
		outpt(baseport+UART_TX,dequeue(&(tty->eq)));   /* echo char: see note above */
		kickout(baseport);
	} else if (0 == emptyqueue(&(tty->wq))) {
		outpt(baseport+UART_TX,dequeue(&(tty->wq)));   /* echo char: see note above */
		kickout(baseport);
	} else outpt( baseport+UART_IER, UART_IER_RDI); /* shut down tx ints */
#if 0
	Prefentially transmit from echo que what does that mean?
	if (0 == emptyqueue(&(tty->eq))) {
		outpt(baseport+UART_TX,dequeue(&(tty->eq)));   /* echo char: see note above */
		kickout(baseport);
	} else outpt( baseport+UART_IER, UART_IER_RDI); /* shut down tx ints */
#endif
	break;
     default:
	kprintf("Invalid ISR\n irqinthandc");
	break;
   }
}
