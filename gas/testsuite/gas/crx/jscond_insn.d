#as:
#objdump: -dr
#name: jscond_insn

.*: +file format .*

Disassembly of section .text:

00000000 <jeq>:
   0:	01 ba       	jeq	r1

00000002 <jne>:
   2:	12 ba       	jne	r2

00000004 <jcs>:
   4:	23 ba       	jcs	r3

00000006 <jcc>:
   6:	34 ba       	jcc	r4

00000008 <jhi>:
   8:	45 ba       	jhi	r5

0000000a <jls>:
   a:	56 ba       	jls	r6

0000000c <jgt>:
   c:	67 ba       	jgt	r7

0000000e <jle>:
   e:	78 ba       	jle	r8

00000010 <jfs>:
  10:	89 ba       	jfs	r9

00000012 <jfc>:
  12:	9a ba       	jfc	r10

00000014 <jlo>:
  14:	ab ba       	jlo	r11

00000016 <jhs>:
  16:	bc ba       	jhs	r12

00000018 <jlt>:
  18:	cd ba       	jlt	r13

0000001a <jge>:
  1a:	de ba       	jge	r14

0000001c <jump>:
  1c:	ef ba       	jump	r15

0000001e <seq>:
  1e:	01 bb       	seq	r1

00000020 <sne>:
  20:	12 bb       	sne	r2

00000022 <scs>:
  22:	23 bb       	scs	r3

00000024 <scc>:
  24:	34 bb       	scc	r4

00000026 <shi>:
  26:	45 bb       	shi	r5

00000028 <sls>:
  28:	56 bb       	sls	r6

0000002a <sgt>:
  2a:	67 bb       	sgt	r7

0000002c <sle>:
  2c:	78 bb       	sle	r8

0000002e <sfs>:
  2e:	89 bb       	sfs	r9

00000030 <sfc>:
  30:	9a bb       	sfc	r10

00000032 <slo>:
  32:	ab bb       	slo	r11

00000034 <shs>:
  34:	bc bb       	shs	r12

00000036 <slt>:
  36:	cd bb       	slt	r13

00000038 <sge>:
  38:	de bb       	sge	r14
