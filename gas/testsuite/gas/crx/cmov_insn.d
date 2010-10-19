#as:
#objdump: -dr
#name: cmov_insn

.*: +file format .*

Disassembly of section .text:

00000000 <cmoveqd>:
   0:	08 30 01 70 	cmoveqd	r0, r1

00000004 <cmovned>:
   4:	08 30 23 71 	cmovned	r2, r3

00000008 <cmovcsd>:
   8:	08 30 45 72 	cmovcsd	r4, r5

0000000c <cmovccd>:
   c:	08 30 67 73 	cmovccd	r6, r7

00000010 <cmovhid>:
  10:	08 30 89 74 	cmovhid	r8, r9

00000014 <cmovlsd>:
  14:	08 30 ab 75 	cmovlsd	r10, r11

00000018 <cmovgtd>:
  18:	08 30 cd 76 	cmovgtd	r12, r13

0000001c <cmovled>:
  1c:	08 30 ef 77 	cmovled	r14, r15

00000020 <cmovfsd>:
  20:	08 30 fe 78 	cmovfsd	r15, r14

00000024 <cmovfcd>:
  24:	08 30 fe 79 	cmovfcd	r15, r14

00000028 <cmovlod>:
  28:	08 30 f0 7a 	cmovlod	r15, r0

0000002c <cmovhsd>:
  2c:	08 30 23 7b 	cmovhsd	r2, r3

00000030 <cmovltd>:
  30:	08 30 75 7c 	cmovltd	r7, r5

00000034 <cmovged>:
  34:	08 30 34 7d 	cmovged	r3, r4
