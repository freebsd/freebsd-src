#as:
#objdump:  -dr
#name:  loadm_test

.*: +file format .*

Disassembly of section .text:

00000000 <main>:
   0:	a0 00       	loadm	\$0x1,r0
   2:	a1 00       	loadm	\$0x2,r0
   4:	a2 00       	loadm	\$0x3,r0
   6:	a3 00       	loadm	\$0x4,r0
   8:	a4 00       	loadm	\$0x5,r0
   a:	a5 00       	loadm	\$0x6,r0
   c:	a6 00       	loadm	\$0x7,r0
   e:	a7 00       	loadm	\$0x8,r0
  10:	a8 00       	loadmp	\$0x1,r0
  12:	a9 00       	loadmp	\$0x2,r0
  14:	aa 00       	loadmp	\$0x3,r0
  16:	ab 00       	loadmp	\$0x4,r0
  18:	ac 00       	loadmp	\$0x5,r0
  1a:	ad 00       	loadmp	\$0x6,r0
  1c:	ae 00       	loadmp	\$0x7,r0
  1e:	af 00       	loadmp	\$0x8,r0
