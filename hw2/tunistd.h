#ifndef _TUNISTD_H_
#define _TUNISTD_H_

extern int read(int dev, char *buf, int nchar);
extern int write(int dev, char *buf, int nchar);
extern int exit(int exitcode);

#endif /*_TUNISTD_H_*/
