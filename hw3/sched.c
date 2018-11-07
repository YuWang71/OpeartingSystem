#include<stdio.h>
#include <cpu.h>
#include <pic.h>
#include "proc.h"
#include "sched.h"

#define BUFLEN 100

void run_scheduler(void);
void debug_report_process_switch(PEntry *oldprocess);
PEntry proctab[], *curproc;

extern void asmswtch(PEntry * oldproc, PEntry *curprocess);
extern void debug_log(char *msg);

void schedule(void){
  int i;
  PEntry *oldproc = NULL; 
  for (i = 1; i<=3; i++) {
    if(RUN != proctab[i].p_status) {
      if (3 != i)
        continue;

      oldproc = curproc;
      if (oldproc == &proctab[0])
        break;

      curproc = &proctab[0];
      debug_report_process_switch(oldproc);
      asmswtch(oldproc,curproc);
      //continue;
      break;
    }
    
    oldproc = curproc;
    switch(i) {
      case 1:
        curproc=&proctab[1];
        kprintf("\n Switching to proc 1 \n ");
        debug_report_process_switch(oldproc);
        asmswtch(oldproc,curproc);
        break;
      case 2:
        curproc=&proctab[2];
        kprintf("\n Switching to proc 2 \n ");
        debug_report_process_switch(oldproc);
        asmswtch(oldproc,curproc);
        break; 
      case 3:
        curproc=&proctab[3];
        kprintf("\n Switching to proc 3 \n ");
        debug_report_process_switch(oldproc);
        asmswtch(oldproc,curproc);
        break;
      default:
        kprintf("invalid Process\n");
    } 
  }
}

void wakeup(int event)
{
  int i;
  for(i=1; i< NPROC; i++) {  
    if(proctab[i].p_status == BLOCKED && proctab[i].p_waitcode == event) {
      proctab[i].p_status = RUN;
    }
  }
}

void sleep(int event)
{
  curproc->p_status=BLOCKED;
  curproc->p_waitcode=event;
  cli();
  schedule();
}

void debug_report_process_switch(PEntry *oldprocess)
{
  char buf[BUFLEN] = "";

  switch (oldprocess->p_status) {
    case ZOMBIE:
      sprintf(buf, "|(%dz-%d)", oldprocess - proctab, curproc - proctab);
      break;
    case BLOCKED:
      sprintf(buf, "|(%db-%d)", oldprocess - proctab, curproc - proctab);
      break;
    default:
      sprintf(buf, "|(%d-%d)", oldprocess - proctab, curproc - proctab);
  }
  if (strlen(buf)) debug_log(buf);
}


