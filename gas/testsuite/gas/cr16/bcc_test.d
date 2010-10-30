#as:
#objdump:  -dr
#name:  bcc_test

.*: +file format .*

Disassembly of section .text:

00000000 <main>:
   0:	01 11       	beq	\*\+0x22 <main\+0x22>:s
   2:	19 11       	bne	\*\+0x34 <main\+0x34>:s
   4:	32 12       	bcc	\*\+0x48 <main\+0x48>:s
   6:	3a 12       	bcc	\*\+0x5a <main\+0x5a>:s
   8:	43 13       	bhi	\*\+0x6e <main\+0x6e>:s
   a:	cb 13       	blt	\*\+0x80 <main\+0x80>:s
   c:	64 14       	bgt	\*\+0x94 <main\+0x94>:s
   e:	8d 14       	bfs	\*\+0xa8 <main\+0xa8>:s
  10:	95 15       	bfc	\*\+0xba <main\+0xba>:s
  12:	a0 18 bc 01 	blo	\*\+0x1ce <main\+0x1ce>:m
  16:	40 18 cc 01 	bhi	\*\+0x1e2 <main\+0x1e2>:m
  1a:	c0 18 d6 01 	blt	\*\+0x1f0 <main\+0x1f0>:m
  1e:	d0 18 e6 01 	bge	\*\+0x204 <main\+0x204>:m
  22:	eb 17       	br	\*\+0x118 <main\+0x118>:s
  24:	00 18 12 01 	beq	\*\+0x136 <main\+0x136>:m
  28:	00 18 12 1f 	beq	\*\+0x1f3a <main\+0x1f3a>:m
  2c:	00 18 22 0f 	beq	\*\+0xf4e <main\+0xf4e>:m
  30:	10 18 34 0f 	bne	\*\+0xf64 <main\+0xf64>:m
  34:	30 18 44 0f 	bcc	\*\+0xf78 <main\+0xf78>:m
  38:	30 18 56 0f 	bcc	\*\+0xf8e <main\+0xf8e>:m
  3c:	40 18 66 0f 	bhi	\*\+0xfa2 <main\+0xfa2>:m
  40:	c0 18 78 0f 	blt	\*\+0xfb8 <main\+0xfb8>:m
  44:	60 18 88 0f 	bgt	\*\+0xfcc <main\+0xfcc>:m
  48:	80 18 9a 0f 	bfs	\*\+0xfe2 <main\+0xfe2>:m
  4c:	90 18 aa 0f 	bfc	\*\+0xff6 <main\+0xff6>:m
  50:	a0 18 bc 1f 	blo	\*\+0x200c <main\+0x200c>:m
  54:	40 18 cc 1f 	bhi	\*\+0x2020 <main\+0x2020>:m
  58:	c0 18 da 1f 	blt	\*\+0x2032 <main\+0x2032>:m
  5c:	d0 18 ea 1f 	bge	\*\+0x2046 <main\+0x2046>:m
  60:	e0 18 fa ff 	br	\*\+0x1005a <main\+0x1005a>:m
  64:	10 00 0f 0f 	beq	\*\+0xff1f76 <main\+0xff1f76>:l
  68:	12 1f 
  6a:	10 00 0a 0a 	beq	\*\+0xaa0f8c <main\+0xaa0f8c>:l
  6e:	22 0f 
  70:	10 00 1b 0b 	bne	\*\+0xbb0fa4 <main\+0xbb0fa4>:l
  74:	34 0f 
  76:	10 00 3c 0c 	bcc	\*\+0xcc0fba <main\+0xcc0fba>:l
  7a:	44 0f 
  7c:	10 00 3d 0d 	bcc	\*\+0xdd0fd2 <main\+0xdd0fd2>:l
  80:	56 0f 
  82:	10 00 49 09 	bhi	\*\+0x990fe8 <main\+0x990fe8>:l
  86:	66 0f 
  88:	10 00 c8 08 	blt	\*\+0x881000 <main\+0x881000>:l
  8c:	78 0f 
  8e:	10 00 67 07 	bgt	\*\+0x771016 <main\+0x771016>:l
  92:	88 0f 
  94:	10 00 86 06 	bfs	\*\+0x66102e <main\+0x66102e>:l
  98:	9a 0f 
  9a:	10 00 95 05 	bfc	\*\+0x551044 <main\+0x551044>:l
  9e:	aa 0f 
  a0:	10 00 a4 04 	blo	\*\+0x44205c <main\+0x44205c>:l
  a4:	bc 1f 
  a6:	10 00 43 03 	bhi	\*\+0x332072 <main\+0x332072>:l
  aa:	cc 1f 
  ac:	10 00 c2 02 	blt	\*\+0x22208a <main\+0x22208a>:l
  b0:	de 1f 
  b2:	10 00 d1 01 	bge	\*\+0x1120a0 <main\+0x1120a0>:l
  b6:	ee 1f 
  b8:	10 00 e0 0f 	br	\*\+0x1000b6 <main\+0x1000b6>:l
  bc:	fe ff 
