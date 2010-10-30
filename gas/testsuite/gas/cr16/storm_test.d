#as:
#objdump:  -dr
#name:  storm_test

.*: +file format .*

Disassembly of section .text:

00000000 <main>:
   0:	b0 00       	storm	\$0x1,r0
   2:	b1 00       	storm	\$0x2,r0
   4:	b2 00       	storm	\$0x3,r0
   6:	b3 00       	storm	\$0x4,r0
   8:	b4 00       	storm	\$0x5,r0
   a:	b5 00       	storm	\$0x6,r0
   c:	b6 00       	storm	\$0x7,r0
   e:	b7 00       	storm	\$0x8,r0
  10:	b8 00       	stormp	\$0x1,r0
  12:	b9 00       	stormp	\$0x2,r0
  14:	ba 00       	stormp	\$0x3,r0
  16:	bb 00       	stormp	\$0x4,r0
  18:	bc 00       	stormp	\$0x5,r0
  1a:	bd 00       	stormp	\$0x6,r0
  1c:	be 00       	stormp	\$0x7,r0
  1e:	bf 00       	stormp	\$0x8,r0
