#as:
#objdump:  -dr
#name:  sub_test

.*: +file format .*

Disassembly of section .text:

00000000 <main>:
   0:	f1 38       	subb	\$0xf:s,r1
   2:	b2 38 ff 00 	subb	\$0xff:m,r2
   6:	b1 38 ff 0f 	subb	\$0xfff:m,r1
   a:	b1 38 14 00 	subb	\$0x14:m,r1
   e:	a2 38       	subb	\$0xa:s,r2
  10:	12 39       	subb	r1,r2
  12:	23 39       	subb	r2,r3
  14:	34 39       	subb	r3,r4
  16:	56 39       	subb	r5,r6
  18:	67 39       	subb	r6,r7
  1a:	78 39       	subb	r7,r8
  1c:	f1 3c       	subcb	\$0xf:s,r1
  1e:	b2 3c ff 00 	subcb	\$0xff:m,r2
  22:	b1 3c ff 0f 	subcb	\$0xfff:m,r1
  26:	b1 3c 14 00 	subcb	\$0x14:m,r1
  2a:	a2 3c       	subcb	\$0xa:s,r2
  2c:	12 3d       	subcb	r1,r2
  2e:	23 3d       	subcb	r2,r3
  30:	34 3d       	subcb	r3,r4
  32:	56 3d       	subcb	r5,r6
  34:	67 3d       	subcb	r6,r7
  36:	78 3d       	subcb	r7,r8
  38:	f1 3e       	subcw	\$0xf:s,r1
  3a:	b2 3e ff 00 	subcw	\$0xff:m,r2
  3e:	b1 3e ff 0f 	subcw	\$0xfff:m,r1
  42:	b1 3e 14 00 	subcw	\$0x14:m,r1
  46:	a2 3e       	subcw	\$0xa:s,r2
  48:	12 3f       	subcw	r1,r2
  4a:	23 3f       	subcw	r2,r3
  4c:	34 3f       	subcw	r3,r4
  4e:	56 3f       	subcw	r5,r6
  50:	67 3f       	subcw	r6,r7
  52:	78 3f       	subcw	r7,r8
  54:	f1 3a       	subw	\$0xf:s,r1
  56:	b2 3a ff 00 	subw	\$0xff:m,r2
  5a:	b1 3a ff 0f 	subw	\$0xfff:m,r1
  5e:	b1 3a 14 00 	subw	\$0x14:m,r1
  62:	a2 3a       	subw	\$0xa:s,r2
  64:	12 3b       	subw	r1,r2
  66:	23 3b       	subw	r2,r3
  68:	34 3b       	subw	r3,r4
  6a:	56 3b       	subw	r5,r6
  6c:	67 3b       	subw	r6,r7
  6e:	78 3b       	subw	r7,r8
  70:	31 00 00 00 	subd	\$0xf:l,\(r2,r1\)
  74:	0f 00 
  76:	31 00 00 00 	subd	\$0xff:l,\(r2,r1\)
  7a:	ff 00 
  7c:	31 00 00 00 	subd	\$0xfff:l,\(r2,r1\)
  80:	ff 0f 
  82:	31 00 00 00 	subd	\$0xffff:l,\(r2,r1\)
  86:	ff ff 
  88:	31 00 0f 00 	subd	\$0xfffff:l,\(r2,r1\)
  8c:	ff ff 
  8e:	31 00 ff 0f 	subd	\$0xfffffff:l,\(r2,r1\)
  92:	ff ff 
  94:	31 00 ff ff 	subd	\$0xffffffff:l,\(r2,r1\)
  98:	ff ff 
  9a:	14 00 31 c0 	subd	\(r4,r3\),\(r2,r1\)
  9e:	14 00 31 c0 	subd	\(r4,r3\),\(r2,r1\)
