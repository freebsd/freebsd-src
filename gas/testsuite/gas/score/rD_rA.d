#as:
#objdump: -d
#source: rD_rA.s

.*: +file format .*

Disassembly of section \.text:

00000000 <\.text>:
   0:	2076      	not!		r0, r7
   2:	2076      	not!		r0, r7
   4:	2f46      	not!		r15, r4
   6:	2f46      	not!		r15, r4
   8:	2ff6      	not!		r15, r15
   a:	2ff6      	not!		r15, r15
   c:	2f36      	not!		r15, r3
   e:	2f36      	not!		r15, r3
  10:	2826      	not!		r8, r2
  12:	2826      	not!		r8, r2
  14:	81e58025 	not.c		r15, r5
  18:	83578025 	not.c		r26, r23
  1c:	0000      	nop!
  1e:	0000      	nop!
  20:	2072      	neg!		r0, r7
  22:	2072      	neg!		r0, r7
  24:	2f42      	neg!		r15, r4
  26:	2f42      	neg!		r15, r4
  28:	2ff2      	neg!		r15, r15
  2a:	2ff2      	neg!		r15, r15
  2c:	2f32      	neg!		r15, r3
  2e:	2f32      	neg!		r15, r3
  30:	2822      	neg!		r8, r2
  32:	2822      	neg!		r8, r2
  34:	81e0941f 	neg.c		r15, r5
  38:	8340dc1f 	neg.c		r26, r23
  3c:	0000      	nop!
  3e:	0000      	nop!
  40:	2073      	cmp!		r0, r7
  42:	2073      	cmp!		r0, r7
  44:	2f43      	cmp!		r15, r4
  46:	2f43      	cmp!		r15, r4
  48:	2ff3      	cmp!		r15, r15
  4a:	2ff3      	cmp!		r15, r15
  4c:	2f33      	cmp!		r15, r3
  4e:	2f33      	cmp!		r15, r3
  50:	2823      	cmp!		r8, r2
  52:	2823      	cmp!		r8, r2
  54:	806f9419 	cmp.c		r15, r5
  58:	807adc19 	cmp.c		r26, r23
  5c:	0000      	nop!
  5e:	0000      	nop!
  60:	80028025 	not.c		r0, r2
  64:	82958025 	not.c		r20, r21
  68:	81e48025 	not.c		r15, r4
  6c:	83358025 	not.c		r25, r21
  70:	81e38025 	not.c		r15, r3
  74:	83368025 	not.c		r25, r22
  78:	2836      	not!		r8, r3
  7a:	2836      	not!		r8, r3
  7c:	2626      	not!		r6, r2
  7e:	2626      	not!		r6, r2
  80:	2746      	not!		r7, r4
  82:	2746      	not!		r7, r4
	...
  90:	8000881f 	neg.c		r0, r2
  94:	8280d41f 	neg.c		r20, r21
  98:	81ef901f 	neg.c		r15, r4
  9c:	8320d41f 	neg.c		r25, r21
  a0:	81ef8c1f 	neg.c		r15, r3
  a4:	8320d81f 	neg.c		r25, r22
  a8:	2832      	neg!		r8, r3
  aa:	2832      	neg!		r8, r3
  ac:	2622      	neg!		r6, r2
  ae:	2622      	neg!		r6, r2
  b0:	2742      	neg!		r7, r4
  b2:	2742      	neg!		r7, r4
	...
  c0:	80608819 	cmp.c		r0, r2
  c4:	8074d419 	cmp.c		r20, r21
  c8:	806f9019 	cmp.c		r15, r4
  cc:	8079d419 	cmp.c		r25, r21
  d0:	806f8c19 	cmp.c		r15, r3
  d4:	8079d819 	cmp.c		r25, r22
  d8:	2833      	cmp!		r8, r3
  da:	2833      	cmp!		r8, r3
  dc:	2623      	cmp!		r6, r2
  de:	2623      	cmp!		r6, r2
  e0:	2743      	cmp!		r7, r4
  e2:	2743      	cmp!		r7, r4
#pass
