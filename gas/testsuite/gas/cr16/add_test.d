#as:
#objdump:  -dr
#name:  add_test

.*: +file format .*

Disassembly of section .text:

00000000 <main>:
   0:	f1 30       	addb	\$0xf:s,r1
   2:	b2 30 ff 00 	addb	\$0xff:m,r2
   6:	b1 30 ff 0f 	addb	\$0xfff:m,r1
   a:	b1 30 14 00 	addb	\$0x14:m,r1
   e:	a2 30       	addb	\$0xa:s,r2
  10:	b2 30 0b 00 	addb	\$0xb:m,r2
  14:	12 31       	addb	r1,r2
  16:	23 31       	addb	r2,r3
  18:	34 31       	addb	r3,r4
  1a:	56 31       	addb	r5,r6
  1c:	67 31       	addb	r6,r7
  1e:	78 31       	addb	r7,r8
  20:	f1 34       	addcb	\$0xf:s,r1
  22:	b2 34 ff 00 	addcb	\$0xff:m,r2
  26:	b1 34 ff 0f 	addcb	\$0xfff:m,r1
  2a:	b1 34 14 00 	addcb	\$0x14:m,r1
  2e:	a2 34       	addcb	\$0xa:s,r2
  30:	b2 34 0b 00 	addcb	\$0xb:m,r2
  34:	12 35       	addcb	r1,r2
  36:	23 35       	addcb	r2,r3
  38:	34 35       	addcb	r3,r4
  3a:	56 35       	addcb	r5,r6
  3c:	67 35       	addcb	r6,r7
  3e:	78 35       	addcb	r7,r8
  40:	f1 36       	addcw	\$0xf:s,r1
  42:	b2 36 ff 00 	addcw	\$0xff:m,r2
  46:	b1 36 ff 0f 	addcw	\$0xfff:m,r1
  4a:	b1 36 14 00 	addcw	\$0x14:m,r1
  4e:	a2 36       	addcw	\$0xa:s,r2
  50:	b2 36 0b 00 	addcw	\$0xb:m,r2
  54:	12 37       	addcw	r1,r2
  56:	23 37       	addcw	r2,r3
  58:	34 37       	addcw	r3,r4
  5a:	56 37       	addcw	r5,r6
  5c:	67 37       	addcw	r6,r7
  5e:	78 37       	addcw	r7,r8
  60:	f1 32       	addw	\$0xf:s,r1
  62:	b2 32 ff 00 	addw	\$0xff:m,r2
  66:	b1 32 ff 0f 	addw	\$0xfff:m,r1
  6a:	b1 32 14 00 	addw	\$0x14:m,r1
  6e:	a2 32       	addw	\$0xa:s,r2
  70:	12 33       	addw	r1,r2
  72:	23 33       	addw	r2,r3
  74:	34 33       	addw	r3,r4
  76:	56 33       	addw	r5,r6
  78:	67 33       	addw	r6,r7
  7a:	78 33       	addw	r7,r8
  7c:	f1 60       	addd	\$0xf:s,\(r2,r1\)
  7e:	b1 60 0b 00 	addd	\$0xb:m,\(r2,r1\)
  82:	b1 60 ff 00 	addd	\$0xff:m,\(r2,r1\)
  86:	b1 60 ff 0f 	addd	\$0xfff:m,\(r2,r1\)
  8a:	10 04 ff ff 	addd	\$0xffff:m,\(r2,r1\)
  8e:	1f 04 ff ff 	addd	\$0xfffff:m,\(r2,r1\)
  92:	21 00 ff 0f 	addd	\$0xfffffff:l,\(r2,r1\)
  96:	ff ff 
  98:	91 60       	addd	\$-1:s,\(r2,r1\)
  9a:	31 61       	addd	\(r4,r3\),\(r2,r1\)
  9c:	31 61       	addd	\(r4,r3\),\(r2,r1\)
  9e:	af 60       	addd	\$0xa:s,\(sp\)
  a0:	ef 60       	addd	\$0xe:s,\(sp\)
  a2:	bf 60 0b 00 	addd	\$0xb:m,\(sp\)
  a6:	8f 60       	addd	\$0x8:s,\(sp\)
