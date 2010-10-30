#as:
#objdump:  -dr
#name:  tbit_test

.*: +file format .*

Disassembly of section .text:

00000000 <main>:
   0:	00 06       	tbit	\$0x0:s,r0
   2:	11 06       	tbit	\$0x1:s,r1
   4:	22 06       	tbit	\$0x2:s,r2
   6:	33 06       	tbit	\$0x3:s,r3
   8:	44 06       	tbit	\$0x4:s,r4
   a:	55 06       	tbit	\$0x5:s,r5
   c:	66 06       	tbit	\$0x6:s,r6
   e:	77 06       	tbit	\$0x7:s,r7
  10:	88 06       	tbit	\$0x8:s,r8
  12:	99 06       	tbit	\$0x9:s,r9
  14:	aa 06       	tbit	\$0xa:s,r10
  16:	bb 06       	tbit	\$0xb:s,r11
  18:	cc 06       	tbit	\$0xc:s,r12
  1a:	dd 06       	tbit	\$0xd:s,r13
  1c:	00 07       	tbit	r0,r0
  1e:	11 07       	tbit	r1,r1
  20:	22 07       	tbit	r2,r2
  22:	33 07       	tbit	r3,r3
  24:	44 07       	tbit	r4,r4
  26:	55 07       	tbit	r5,r5
  28:	66 07       	tbit	r6,r6
  2a:	77 07       	tbit	r7,r7
  2c:	88 07       	tbit	r8,r8
  2e:	99 07       	tbit	r9,r9
  30:	aa 07       	tbit	r10,r10
  32:	bb 07       	tbit	r11,r11
  34:	cc 07       	tbit	r12,r12
  36:	dd 07       	tbit	r13,r13
