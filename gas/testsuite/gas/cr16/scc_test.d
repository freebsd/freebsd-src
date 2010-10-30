#as:
#objdump:  -dr
#name:  scc_test

.*: +file format .*

Disassembly of section .text:

00000000 <main>:
   0:	02 08       	seq	r2
   2:	13 08       	sne	r3
   4:	23 08       	scs	r3
   6:	34 08       	scc	r4
   8:	45 08       	shi	r5
   a:	56 08       	sls	r6
   c:	67 08       	sgt	r7
   e:	88 08       	sfs	r8
  10:	99 08       	sfc	r9
  12:	aa 08       	slo	r10
  14:	b1 08       	shs	r1
  16:	cb 08       	slt	r11
  18:	d0 08       	sge	r0
