/*********************************************************************
*
*       file:           tty.h
*       author:         betty o'neil
*
*       private header file for tty drivers
*       apps should not include this header
*/

#ifndef TTY_H
#define TTY_H

#define MAXBUF 6
#include "queue/queue.h"
struct tty {
  int echoflag;			/* echo chars in read */
#if 0
  char rbuf[MAXBUF];            /* typeahead chars */
  int rin;                    /* index into receive buffer */
  int rout;                   /* index out of receive buffer */
  int rnum;                   /* number of characters in rcvr buffer */
  char tbuf[MAXBUF];            /* output chars (for transmit) */
  int tin;                    /* index into transmit buffer */
  int tout;                   /* index out of transmit buffer */
  int tnum;                   /* number of characters in xmit buffer */
#endif
  Queue rq;		/* readqueue */
  Queue wq;		/* writequeue */
  Queue eq;		/* echoqueue */
};

extern struct tty ttytab[];

/* tty-specific device functions */
void ttyinit(int dev);
int ttyread(int dev, char *buf, int nchar);
int ttywrite(int dev, char *buf, int nchar);
int ttycontrol(int dev, int fncode, int val);

#endif 
