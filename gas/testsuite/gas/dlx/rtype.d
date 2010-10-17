#as:
#objdump: -dr
#name: rtype

.*: +file format .*

Disassembly of section .text:

00000000 <.text>:
   0:	00 22 18 20 	add     r3,r1,r2
   4:	00 22 18 20 	add     r3,r1,r2
   8:	00 22 18 21 	addu    r3,r1,r2
   c:	00 43 20 22 	sub     r4,r2,r3
  10:	00 43 20 23 	subu    r4,r2,r3
  14:	00 c7 28 05 	mult    r5,r6,r7
  18:	01 4b 60 06 	multu   r12,r10,r11
  1c:	01 ae 78 07 	div     r15,r13,r14
  20:	02 32 80 08 	divu    r16,r17,r18
  24:	02 95 98 24 	and     r19,r20,r21
  28:	02 e0 b0 25 	or      r22,r23,r0
  2c:	03 19 78 26 	xor     r15,r24,r25
  30:	03 60 d0 04 	sll     r26,r27,r0
  34:	03 be e0 07 	div     r28,r29,r30
  38:	01 bf 78 06 	multu   r15,r13,r31
  3c:	00 43 08 28 	seq     r1,r2,r3
  40:	03 e0 20 29 	sne     r4,r31,r0
  44:	01 2a 40 2a 	slt     r8,r9,r10
  48:	00 a6 38 2b 	sgt     r7,r5,r6
  4c:	00 a6 38 2c 	sle     r7,r5,r6
  50:	00 a6 38 2d 	sge     r7,r5,r6
  54:	00 43 08 28 	seq     r1,r2,r3
  58:	03 e0 20 29 	sne     r4,r31,r0
  5c:	01 2a 40 2a 	slt     r8,r9,r10
  60:	00 a6 38 2b 	sgt     r7,r5,r6
  64:	00 a6 38 2c 	sle     r7,r5,r6
  68:	00 a6 38 2d 	sge     r7,r5,r6
  6c:	00 a0 50 30 	mvts    r10,r5
  70:	00 a0 50 31 	mvfs    r10,r5
