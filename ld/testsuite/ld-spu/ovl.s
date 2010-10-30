 .text
 .p2align 2
 .globl _start
_start:
 ai sp,sp,-32
 xor lr,lr,lr
 stqd lr,0(sp)
 stqd lr,16(sp)
 brsl lr,f1_a1
 brsl lr,f2_a1
 brsl lr,f1_a2
 ila 9,f2_a2
 bisl lr,9
 ai sp,sp,32
 br _start

 .type f0,@function
f0:
 bi lr
 .size f0,.-f0

 .section .ov_a1,"ax",@progbits
 .p2align 2
 .global f1_a1
 .type f1_a1,@function
f1_a1:
 br f3_a1
 .size f1_a1,.-f1_a1

 .global f2_a1
 .type f2_a1,@function
f2_a1:
 ila 3,f4_a1
 bi lr
 .size f2_a1,.-f2_a1

 .global f3_a1
 .type f3_a1,@function
f3_a1:
 bi lr
 .size f3_a1,.-f3_a1

 .global f4_a1
 .type f4_a1,@function
f4_a1:
 bi lr
 .size f4_a1,.-f4_a1


 .section .ov_a2,"ax",@progbits
 .p2align 2
 .global f1_a2
 .type f1_a2,@function
f1_a2:
 stqd lr,16(sp)
 stqd sp,-32(sp)
 ai sp,sp,-32
 brsl lr,f0
 brsl lr,f1_a1
 brsl lr,f3_a2
 lqd lr,48(sp)
 ai sp,sp,32
 bi lr
 .size f1_a2,.-f1_a2

 .global f2_a2
 .type f2_a2,@function
f2_a2:
 ilhu 3,f4_a2@h
 iohl 3,f4_a2@l
 bi lr
 .size f2_a2,.-f2_a2

 .type f3_a2,@function
f3_a2:
 bi lr
 .size f3_a2,.-f3_a2

 .type f4_a2,@function
f4_a2:
 br f3_a2
 .size f4_a2,.-f4_a2
