#as: -m32r2
#objdump: -dr
#name: m32r2

.*: +file format .*

Disassembly of section .text:

0+0000 <setpsw>:
   0:	71 c1 71 ff 	setpsw #0xc1 -> setpsw #0xff

0+0004 <clrpsw>:
   4:	72 c1 72 ff 	clrpsw #0xc1 -> clrpsw #0xff

0+0008 <bset>:
   8:	a0 61 00 04 	bset #0x0,@\(4,r1\)
   c:	a1 61 00 04 	bset #0x1,@\(4,r1\)
  10:	a7 61 00 04 	bset #0x7,@\(4,r1\)

0+0014 <bclr>:
  14:	a0 71 00 04 	bclr #0x0,@\(4,r1\)
  18:	a1 71 00 04 	bclr #0x1,@\(4,r1\)
  1c:	a7 71 00 04 	bclr #0x7,@\(4,r1\)

0+0020 <btst>:
  20:	00 fd 01 fd 	btst #0x0,fp -> btst #0x1,fp
  24:	07 fd f0 00 	btst #0x7,fp \|\| nop
  28:	01 fd 90 82 	btst #0x1,fp \|\| mv r0,r2
  2c:	01 fd 90 82 	btst #0x1,fp \|\| mv r0,r2

0+0030 <divuh>:
  30:	9d 1d 00 10 	divuh fp,fp

0+0034 <divb>:
  34:	9d 0d 00 18 	divb fp,fp

0+0038 <divub>:
  38:	9d 1d 00 18 	divub fp,fp

0+003c <remh>:
  3c:	9d 2d 00 10 	remh fp,fp

0+0040 <remuh>:
  40:	9d 3d 00 10 	remuh fp,fp

0+0044 <remb>:
  44:	9d 2d 00 18 	remb fp,fp

0+0048 <remub>:
  48:	9d 3d 00 18 	remub fp,fp

0+004c <sll>:
  4c:	10 41 92 43 	sll r0,r1 \|\| sll r2,r3
  50:	12 43 90 61 	sll r2,r3 \|\| mul r0,r1
  54:	10 41 92 63 	sll r0,r1 \|\| mul r2,r3
  58:	60 01 92 43 	ldi r0,#1 \|\| sll r2,r3
  5c:	10 41 e2 01 	sll r0,r1 \|\| ldi r2,#1

0+0060 <slli>:
  60:	50 41 d2 5f 	slli r0,#0x1 \|\| slli r2,#0x1f
  64:	52 5f 90 61 	slli r2,#0x1f \|\| mul r0,r1
  68:	50 41 92 63 	slli r0,#0x1 \|\| mul r2,r3
  6c:	60 01 d2 5f 	ldi r0,#1 \|\| slli r2,#0x1f
  70:	50 41 e2 01 	slli r0,#0x1 \|\| ldi r2,#1

0+0074 <sra>:
  74:	10 21 92 23 	sra r0,r1 \|\| sra r2,r3
  78:	12 23 90 61 	sra r2,r3 \|\| mul r0,r1
  7c:	10 21 92 63 	sra r0,r1 \|\| mul r2,r3
  80:	60 01 92 23 	ldi r0,#1 \|\| sra r2,r3
  84:	10 21 e2 01 	sra r0,r1 \|\| ldi r2,#1

0+0088 <srai>:
  88:	50 21 d2 3f 	srai r0,#0x1 \|\| srai r2,#0x1f
  8c:	52 3f 90 61 	srai r2,#0x1f \|\| mul r0,r1
  90:	50 21 92 63 	srai r0,#0x1 \|\| mul r2,r3
  94:	60 01 d2 3f 	ldi r0,#1 \|\| srai r2,#0x1f
  98:	50 21 e2 01 	srai r0,#0x1 \|\| ldi r2,#1

0+009c <srl>:
  9c:	10 01 92 03 	srl r0,r1 \|\| srl r2,r3
  a0:	12 03 90 61 	srl r2,r3 \|\| mul r0,r1
  a4:	10 01 92 63 	srl r0,r1 \|\| mul r2,r3
  a8:	60 01 92 03 	ldi r0,#1 \|\| srl r2,r3
  ac:	10 01 e2 01 	srl r0,r1 \|\| ldi r2,#1

0+00b0 <srli>:
  b0:	50 01 d2 1f 	srli r0,#0x1 \|\| srli r2,#0x1f
  b4:	52 1f 90 61 	srli r2,#0x1f \|\| mul r0,r1
  b8:	50 01 92 63 	srli r0,#0x1 \|\| mul r2,r3
  bc:	60 01 d2 1f 	ldi r0,#1 \|\| srli r2,#0x1f
  c0:	50 01 e2 01 	srli r0,#0x1 \|\| ldi r2,#1
