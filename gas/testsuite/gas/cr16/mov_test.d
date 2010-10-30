#as:
#objdump:  -dr
#name:  mov_test

.*: +file format .*

Disassembly of section .text:

00000000 <main>:
   0:	f1 58       	movb	\$0xf:s,r1
   2:	b2 58 ff 00 	movb	\$0xff:m,r2
   6:	b1 58 ff 0f 	movb	\$0xfff:m,r1
   a:	b1 58 14 00 	movb	\$0x14:m,r1
   e:	a2 58       	movb	\$0xa:s,r2
  10:	b2 58 0b 00 	movb	\$0xb:m,r2
  14:	12 59       	movb	r1,r2
  16:	23 59       	movb	r2,r3
  18:	34 59       	movb	r3,r4
  1a:	56 59       	movb	r5,r6
  1c:	67 59       	movb	r6,r7
  1e:	78 59       	movb	r7,r8
  20:	f1 5a       	movw	\$0xf:s,r1
  22:	b1 5a 0b 00 	movw	\$0xb:m,r1
  26:	b2 5a ff 00 	movw	\$0xff:m,r2
  2a:	b1 5a ff 0f 	movw	\$0xfff:m,r1
  2e:	b1 5a 14 00 	movw	\$0x14:m,r1
  32:	a2 5a       	movw	\$0xa:s,r2
  34:	b2 5a 0b 00 	movw	\$0xb:m,r2
  38:	12 5b       	movw	r1,r2
  3a:	23 5b       	movw	r2,r3
  3c:	34 5b       	movw	r3,r4
  3e:	56 5b       	movw	r5,r6
  40:	67 5b       	movw	r6,r7
  42:	78 5b       	movw	r7,r8
  44:	f1 54       	movd	\$0xf:s,\(r2,r1\)
  46:	b1 54 0b 00 	movd	\$0xb:m,\(r2,r1\)
  4a:	b1 54 ff 00 	movd	\$0xff:m,\(r2,r1\)
  4e:	b1 54 ff 0f 	movd	\$0xfff:m,\(r2,r1\)
  52:	10 05 ff ff 	movd	\$0xffff:m,\(r2,r1\)
  56:	1f 05 ff ff 	movd	\$0xfffff:m,\(r2,r1\)
  5a:	71 00 ff 0f 	movd	\$0xfffffff:l,\(r2,r1\)
  5e:	ff ff 
  60:	91 54       	movd	\$-1:s,\(r2,r1\)
  62:	31 55       	movd	\(r4,r3\),\(r2,r1\)
  64:	31 55       	movd	\(r4,r3\),\(r2,r1\)
  66:	af 54       	movd	\$0xa:s,\(sp\)
  68:	ef 54       	movd	\$0xe:s,\(sp\)
  6a:	bf 54 0b 00 	movd	\$0xb:m,\(sp\)
  6e:	8f 54       	movd	\$0x8:s,\(sp\)
  70:	12 5c       	movxb	r1,r2
  72:	34 5c       	movxb	r3,r4
  74:	56 5c       	movxb	r5,r6
  76:	78 5c       	movxb	r7,r8
  78:	9a 5c       	movxb	r9,r10
  7a:	12 5e       	movxw	r1,\(r3,r2\)
  7c:	33 5e       	movxw	r3,\(r4,r3\)
  7e:	55 5e       	movxw	r5,\(r6,r5\)
  80:	77 5e       	movxw	r7,\(r8,r7\)
  82:	98 5e       	movxw	r9,\(r9,r8\)
  84:	12 5d       	movzb	r1,r2
  86:	34 5d       	movzb	r3,r4
  88:	56 5d       	movzb	r5,r6
  8a:	78 5d       	movzb	r7,r8
  8c:	9a 5d       	movzb	r9,r10
  8e:	12 5f       	movzw	r1,\(r3,r2\)
  90:	33 5f       	movzw	r3,\(r4,r3\)
  92:	55 5f       	movzw	r5,\(r6,r5\)
  94:	77 5f       	movzw	r7,\(r8,r7\)
  96:	98 5f       	movzw	r9,\(r9,r8\)
