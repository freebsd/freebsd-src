#as:
#objdump:  -dr
#name:  and_test

.*: +file format .*

Disassembly of section .text:

00000000 <main>:
   0:	f1 20       	andb	\$0xf:s,r1
   2:	b2 20 ff 00 	andb	\$0xff:m,r2
   6:	b1 20 ff 0f 	andb	\$0xfff:m,r1
   a:	b2 20 ff ff 	andb	\$0xffff:m,r2
   e:	b1 20 14 00 	andb	\$0x14:m,r1
  12:	a2 20       	andb	\$0xa:s,r2
  14:	12 21       	andb	r1,r2
  16:	23 21       	andb	r2,r3
  18:	34 21       	andb	r3,r4
  1a:	56 21       	andb	r5,r6
  1c:	67 21       	andb	r6,r7
  1e:	78 21       	andb	r7,r8
  20:	f1 22       	andw	\$0xf:s,r1
  22:	b2 22 ff 00 	andw	\$0xff:m,r2
  26:	b1 22 ff 0f 	andw	\$0xfff:m,r1
  2a:	b2 22 ff ff 	andw	\$0xffff:m,r2
  2e:	b1 22 14 00 	andw	\$0x14:m,r1
  32:	a2 22       	andw	\$0xa:s,r2
  34:	12 23       	andw	r1,r2
  36:	23 23       	andw	r2,r3
  38:	34 23       	andw	r3,r4
  3a:	56 23       	andw	r5,r6
  3c:	67 23       	andw	r6,r7
  3e:	78 23       	andw	r7,r8
  40:	41 00 00 00 	andd	\$0xf:l,\(r2,r1\)
  44:	0f 00 
  46:	41 00 00 00 	andd	\$0xff:l,\(r2,r1\)
  4a:	ff 00 
  4c:	41 00 00 00 	andd	\$0xfff:l,\(r2,r1\)
  50:	ff 0f 
  52:	41 00 00 00 	andd	\$0xffff:l,\(r2,r1\)
  56:	ff ff 
  58:	41 00 0f 00 	andd	\$0xfffff:l,\(r2,r1\)
  5c:	ff ff 
  5e:	41 00 ff 0f 	andd	\$0xfffffff:l,\(r2,r1\)
  62:	ff ff 
  64:	41 00 ff ff 	andd	\$0xffffffff:l,\(r2,r1\)
  68:	ff ff 
  6a:	14 00 31 b0 	andd	\(r4,r3\),\(r2,r1\)
  6e:	14 00 31 b0 	andd	\(r4,r3\),\(r2,r1\)
  72:	4f 00 00 00 	andd	\$0xa:l,\(sp\)
  76:	0a 00 
  78:	4f 00 00 00 	andd	\$0xe:l,\(sp\)
  7c:	0e 00 
  7e:	4f 00 00 00 	andd	\$0x8:l,\(sp\)
  82:	08 00 
