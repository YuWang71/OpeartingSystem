/*********************************************************************
*
*       file:           io_public.h
*       author:         betty o'neil
*
*       API for device-independent i/o package for SAPC
*       hw2: phase this out as user-callable API, replace with syscalls
*       (except ioinit() becomes a kernel-internal function)
*/

#ifndef _TSYSTM_H_
#define _TSYSTM_H_

#include "tty_public.h"

/* initialize io package*/
void ioinit(void);
int syswrite(int dev, char *buf, int nchar);
int sysread(int dev, char *buf, int nchar);

#endif /* _TSYSTM_H_ */
