#as:
#objdump:  -dr
#name:  xor_test

.*: +file format .*

Disassembly of section .text:

00000000 <main>:
   0:	f1 28       	xorb	\$0xf:s,r1
   2:	b2 28 ff 00 	xorb	\$0xff:m,r2
   6:	b1 28 ff 0f 	xorb	\$0xfff:m,r1
   a:	b2 28 ff ff 	xorb	\$0xffff:m,r2
   e:	b1 28 14 00 	xorb	\$0x14:m,r1
  12:	a2 28       	xorb	\$0xa:s,r2
  14:	12 29       	xorb	r1,r2
  16:	23 29       	xorb	r2,r3
  18:	34 29       	xorb	r3,r4
  1a:	56 29       	xorb	r5,r6
  1c:	67 29       	xorb	r6,r7
  1e:	78 29       	xorb	r7,r8
  20:	f1 2a       	xorw	\$0xf:s,r1
  22:	b2 2a ff 00 	xorw	\$0xff:m,r2
  26:	b1 2a ff 0f 	xorw	\$0xfff:m,r1
  2a:	b2 2a ff ff 	xorw	\$0xffff:m,r2
  2e:	b1 2a 14 00 	xorw	\$0x14:m,r1
  32:	a2 2a       	xorw	\$0xa:s,r2
  34:	12 2b       	xorw	r1,r2
  36:	23 2b       	xorw	r2,r3
  38:	34 2b       	xorw	r3,r4
  3a:	56 2b       	xorw	r5,r6
  3c:	67 2b       	xorw	r6,r7
  3e:	78 2b       	xorw	r7,r8
  40:	61 00 00 00 	xord	\$0xf:l,\(r2,r1\)
  44:	0f 00 
  46:	61 00 00 00 	xord	\$0xff:l,\(r2,r1\)
  4a:	ff 00 
  4c:	61 00 00 00 	xord	\$0xfff:l,\(r2,r1\)
  50:	ff 0f 
  52:	61 00 00 00 	xord	\$0xffff:l,\(r2,r1\)
  56:	ff ff 
  58:	61 00 0f 00 	xord	\$0xfffff:l,\(r2,r1\)
  5c:	ff ff 
  5e:	61 00 ff 0f 	xord	\$0xfffffff:l,\(r2,r1\)
  62:	ff ff 
  64:	61 00 ff ff 	xord	\$0xffffffff:l,\(r2,r1\)
  68:	ff ff 
  6a:	14 00 31 a0 	xord	\(r4,r3\),\(r2,r1\)
  6e:	14 00 31 a0 	xord	\(r4,r3\),\(r2,r1\)
