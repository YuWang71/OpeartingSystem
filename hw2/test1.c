#include <stdio.h>
#include "tty_public.h"
#include "tunistd.h"

#define MILLION 1000000
#define DELAYLOOPCOUNT (400 * MILLION)
#define BUFLEN 80

void print_return_of_main(int ret); 
/* the faster the machine you're on, the longer this loop must be */
static void delay(void)
{
  int i;

  kprintf("<doing delay>\n");
  for (i = 0; i < DELAYLOOPCOUNT; i++);
}

int main(void)
{
  char buf[BUFLEN];
  int got, lib_console_dev, ldev;

  /* Determine the SAPC's "console" device, the serial port for user i/o */
  lib_console_dev = sys_get_console_dev();  /* SAPC support lib fn */
  switch(lib_console_dev) {
    case COM1: ldev = TTY0;	/* convert to our dev #'s */
               break;
    case COM2: ldev = TTY1;
               break;
    default: printf("Unknown console device\n");
             return 1;
  }
  kprintf("Running with device TTY%d\n",ldev);
  /* Now have a usable device to talk to with i/o package-- */

  kprintf("\nTrying simple write(4 chars)...\n");
  got = write(ldev,"hi!\n",4);
  kprintf("write of 4 returned %d\n",got);
  got = write(ldev,"hi!\n",4);
  kprintf("write of 4 returned %d\n",got);
  kprintf("\nType 10 chars input to test typeahead while looping for delay...\n");
  got = read(ldev, buf, 10);	/* should wait for all 10 chars, once fixed */
  kprintf("\nGot %d chars into buf. Trying write of buf...\n", got);
  write(ldev, buf, got);
  kprintf("\nTrying another 10 chars read right away...\n");
  got = read(ldev, buf, 10);	/* should wait for input, once fixed */
  kprintf("\nGot %d chars on second read\n",got);
  if (got == 0) 
      kprintf("nothing in buffer\n");	/* expected result until fixed */
  else 
      write(ldev, buf, got);	/* should write 10 chars once fixed */

  return 0;
}

void print_return_of_main(int ret) 
{
  kprintf("Returned Value from main %d\n",ret);
}
