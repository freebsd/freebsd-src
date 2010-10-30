#as:
#objdump:  -dr
#name:  or_test

.*: +file format .*

Disassembly of section .text:

00000000 <main>:
   0:	f1 24       	orb	\$0xf:s,r1
   2:	b2 24 ff 00 	orb	\$0xff:m,r2
   6:	b1 24 ff 0f 	orb	\$0xfff:m,r1
   a:	b2 24 ff ff 	orb	\$0xffff:m,r2
   e:	b1 24 14 00 	orb	\$0x14:m,r1
  12:	a2 24       	orb	\$0xa:s,r2
  14:	12 25       	orb	r1,r2
  16:	23 25       	orb	r2,r3
  18:	34 25       	orb	r3,r4
  1a:	56 25       	orb	r5,r6
  1c:	67 25       	orb	r6,r7
  1e:	78 25       	orb	r7,r8
  20:	f1 26       	orw	\$0xf:s,r1
  22:	b2 26 ff 00 	orw	\$0xff:m,r2
  26:	b1 26 ff 0f 	orw	\$0xfff:m,r1
  2a:	b2 26 ff ff 	orw	\$0xffff:m,r2
  2e:	b1 26 14 00 	orw	\$0x14:m,r1
  32:	a2 26       	orw	\$0xa:s,r2
  34:	12 27       	orw	r1,r2
  36:	23 27       	orw	r2,r3
  38:	34 27       	orw	r3,r4
  3a:	56 27       	orw	r5,r6
  3c:	67 27       	orw	r6,r7
  3e:	78 27       	orw	r7,r8
  40:	51 00 00 00 	ord	\$0xf:l,\(r2,r1\)
  44:	0f 00 
  46:	51 00 00 00 	ord	\$0xff:l,\(r2,r1\)
  4a:	ff 00 
  4c:	51 00 00 00 	ord	\$0xfff:l,\(r2,r1\)
  50:	ff 0f 
  52:	51 00 00 00 	ord	\$0xffff:l,\(r2,r1\)
  56:	ff ff 
  58:	51 00 0f 00 	ord	\$0xfffff:l,\(r2,r1\)
  5c:	ff ff 
  5e:	51 00 ff 0f 	ord	\$0xfffffff:l,\(r2,r1\)
  62:	ff ff 
  64:	51 00 ff ff 	ord	\$0xffffffff:l,\(r2,r1\)
  68:	ff ff 
  6a:	14 00 31 90 	ord	\(r4,r3\),\(r2,r1\)
  6e:	14 00 31 90 	ord	\(r4,r3\),\(r2,r1\)
