# asm startup file
# very first module in load
.globl _startupc1, _print_return_of_main, _start, _wrapup

_start:  movl $0x3ffff0, %esp   # set user program stack
  movl $0, %ebp          # make debugger backtrace terminate here
  call _startupc1         # call C startup, which calls user main

_wrapup:
 # pushl %ebx
 # movl 8(%esp),%ebx
 # call _print_return_of_main     #for testing the return of test1#main all the way to this file
 # popl %ebx
  int $3                # return to Tutor
