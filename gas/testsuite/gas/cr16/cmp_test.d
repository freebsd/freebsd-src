#as:
#objdump:  -dr
#name:  cmp_test

.*: +file format .*

Disassembly of section .text:

00000000 <main>:
   0:	f1 50       	cmpb	\$0xf:s,r1
   2:	b2 50 ff 00 	cmpb	\$0xff:m,r2
   6:	b1 50 ff 0f 	cmpb	\$0xfff:m,r1
   a:	b1 50 14 00 	cmpb	\$0x14:m,r1
   e:	a2 50       	cmpb	\$0xa:s,r2
  10:	b2 50 0b 00 	cmpb	\$0xb:m,r2
  14:	12 51       	cmpb	r1,r2
  16:	23 51       	cmpb	r2,r3
  18:	34 51       	cmpb	r3,r4
  1a:	56 51       	cmpb	r5,r6
  1c:	67 51       	cmpb	r6,r7
  1e:	78 51       	cmpb	r7,r8
  20:	f1 52       	cmpw	\$0xf:s,r1
  22:	b1 52 0b 00 	cmpw	\$0xb:m,r1
  26:	b2 52 ff 00 	cmpw	\$0xff:m,r2
  2a:	b1 52 ff 0f 	cmpw	\$0xfff:m,r1
  2e:	b1 52 14 00 	cmpw	\$0x14:m,r1
  32:	a2 52       	cmpw	\$0xa:s,r2
  34:	b2 52 0b 00 	cmpw	\$0xb:m,r2
  38:	12 53       	cmpw	r1,r2
  3a:	23 53       	cmpw	r2,r3
  3c:	34 53       	cmpw	r3,r4
  3e:	56 53       	cmpw	r5,r6
  40:	67 53       	cmpw	r6,r7
  42:	78 53       	cmpw	r7,r8
  44:	f1 56       	cmpd	\$0xf:s,\(r2,r1\)
  46:	b1 56 0b 00 	cmpd	\$0xb:m,\(r2,r1\)
  4a:	b1 56 ff 00 	cmpd	\$0xff:m,\(r2,r1\)
  4e:	b1 56 ff 0f 	cmpd	\$0xfff:m,\(r2,r1\)
  52:	91 00 00 00 	cmpd	\$0xffff:l,\(r2,r1\)
  56:	ff ff 
  58:	91 00 0f 00 	cmpd	\$0xfffff:l,\(r2,r1\)
  5c:	ff ff 
  5e:	91 00 ff 0f 	cmpd	\$0xfffffff:l,\(r2,r1\)
  62:	ff ff 
  64:	91 56       	cmpd	\$-1:s,\(r2,r1\)
  66:	31 57       	cmpd	\(r4,r3\),\(r2,r1\)
  68:	31 57       	cmpd	\(r4,r3\),\(r2,r1\)
  6a:	af 56       	cmpd	\$0xa:s,\(sp\)
  6c:	ef 56       	cmpd	\$0xe:s,\(sp\)
  6e:	bf 56 0b 00 	cmpd	\$0xb:m,\(sp\)
  72:	8f 56       	cmpd	\$0x8:s,\(sp\)
