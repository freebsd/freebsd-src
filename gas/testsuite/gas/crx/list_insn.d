#as:
#objdump: -dr
#name: list_insn

.*: +file format .*

Disassembly of section .text:

00000000 <push>:
   0:	6e 34 18 00 	push	r14, {r3,r4}
   4:	b2 ff       	push	r2

00000006 <pushx>:
   6:	7f 34 ff 00 	pushx	r15, {r0,r1,r2,r3,r4,r5,r6,r7}
   a:	76 34 00 00 	pushx	r6, {lo,hi}

0000000e <pop>:
   e:	40 32 00 04 	loadm	r0, {r10}
  12:	c2 ff       	pop	r2

00000014 <popx>:
  14:	7f 32 fb 00 	popx	r15, {r0,r1,r3,r4,r5,r6,r7}
  18:	77 32 00 00 	popx	r7, {lo,hi}

0000001c <popret>:
  1c:	6d 32 02 40 	popret	r13, {r1,r14}
  20:	de ff       	popret	r14

00000022 <loadm>:
  22:	40 32 42 00 	loadm	r0, {r1,r6}

00000026 <loadma>:
  26:	5d 32 14 10 	loadma	r13, {u2,u4,u12}

0000002a <storm>:
  2a:	4f 34 00 40 	storm	r15, {r14}

0000002e <storma>:
  2e:	53 34 05 00 	storma	r3, {u0,u2}
