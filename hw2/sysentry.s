
.globl _syscallc, _syscall

_syscall: pushl %edx
  pushl %ecx
  pushl %ebx
  pushl %eax
  call _syscallc
  popl %eax
  popl %ebx
  popl %ecx
  popl %edx
  iret
