# asm startup file
# very first module in load
.globl _main, _exit, _ustart
_ustart:
  call _main            # call user main
  pushl %eax
  call _exit            # call sysexit
