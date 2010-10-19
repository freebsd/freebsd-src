a:
 lapcq a,$r10
 lapcq x,$r11
x:
 lapcq xx,$r12
 nop
xx:
 lapcq xxx,$r13
 nop
 nop
xxx:
 nop
a00:
 nop
 lapc a00,$r9
a0:
 lapc a0,$r8
 lapc x0,$r7
x0:
 lapc xx0,$r6
 nop
xx0:
 nop
a11:
 nop
 lapc.d a11,$r10
a1:
 lapc.d a1,$r10
 lapc.d x1,$r11
x1:
 lapc.d xx1,$r12
 nop
xx1:
 lapc.d xxx1,$r13
 nop
 nop
xxx1:
 nop
 lapc y,$r3
 .space 28,0
y:

