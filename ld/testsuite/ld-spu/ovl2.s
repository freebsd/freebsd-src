 .text
 .p2align 2
 .global _start
_start:
 brsl lr,f1_a1
 brsl lr,setjmp
 br _start

 .type setjmp,@function
setjmp:
 bi lr
 .size setjmp,.-setjmp

 .type longjmp,@function
longjmp:
 bi lr
 .size longjmp,.-longjmp

 .section .ov_a1,"ax",@progbits
 .p2align 2
 .global f1_a1
 .type f1_a1,@function
f1_a1:
 bi lr
 .size f1_a1,.-f1_a1

 .section .ov_a2,"ax",@progbits
 .p2align 2
 .type f1_a2,@function
f1_a2:
 br longjmp
 .size f1_a2,.-f1_a2

_SPUEAR_f1_a2 = f1_a2
 .global _SPUEAR_f1_a2
