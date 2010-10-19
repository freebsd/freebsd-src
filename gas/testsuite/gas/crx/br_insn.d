#as:
#objdump: -dr
#name: br_insn

.*: +file format .*

Disassembly of section .text:

00000000 <beq>:
   0:	08 70       	beq	0x[0-9a-f]* [-_<>+0-9a-z]*
   2:	7e 70 fd 07 	beq	0x[0-9a-f]* [-_<>+0-9a-z]*
   6:	7f 70 04 00 	beq	0x[0-9a-f]* [-_<>+0-9a-z]*
   a:	29 55 

0000000c <bne>:
   c:	fc 71       	bne	0x[0-9a-f]* [-_<>+0-9a-z]*
   e:	7e 71 a3 07 	bne	0x[0-9a-f]* [-_<>+0-9a-z]*
  12:	7f 71 f8 ff 	bne	0x[0-9a-f]* [-_<>+0-9a-z]*
  16:	f7 43 

00000018 <bcs>:
  18:	7d 72       	bcs	0x[0-9a-f]* [-_<>+0-9a-z]*
  1a:	7e 72 c6 ec 	bcs	0x[0-9a-f]* [-_<>+0-9a-z]*
  1e:	7f 72 04 00 	bcs	0x[0-9a-f]* [-_<>+0-9a-z]*
  22:	29 48 

00000024 <bcc>:
  24:	83 73       	bcc	0x[0-9a-f]* [-_<>+0-9a-z]*
  26:	7e 73 ff 7f 	bcc	0x[0-9a-f]* [-_<>+0-9a-z]*
  2a:	ff 73       	bcc	0x[0-9a-f]* [-_<>+0-9a-z]*

0000002c <bhi>:
  2c:	7e 74 7f 00 	bhi	0x[0-9a-f]* [-_<>+0-9a-z]*
  30:	7e 74 01 80 	bhi	0x[0-9a-f]* [-_<>+0-9a-z]*
  34:	01 74       	bhi	0x[0-9a-f]* [-_<>+0-9a-z]*

00000036 <bls>:
  36:	ff 75       	bls	0x[0-9a-f]* [-_<>+0-9a-z]*
  38:	7e 75 00 80 	bls	0x[0-9a-f]* [-_<>+0-9a-z]*
  3c:	7f 75 00 00 	bls	0x[0-9a-f]* [-_<>+0-9a-z]*
  40:	00 80 

00000042 <bgt>:
  42:	18 76       	bgt	0x[0-9a-f]* [-_<>+0-9a-z]*
  44:	7e 76 ff 07 	bgt	0x[0-9a-f]* [-_<>+0-9a-z]*
  48:	7f 76 ff ff 	bgt	0x[0-9a-f]* [-_<>+0-9a-z]*
  4c:	ff 7f 

0000004e <ble>:
  4e:	e0 77       	ble	0x[0-9a-f]* [-_<>+0-9a-z]*
  50:	7e 77 7f ff 	ble	0x[0-9a-f]* [-_<>+0-9a-z]*
  54:	7f 77 07 00 	ble	0x[0-9a-f]* [-_<>+0-9a-z]*
  58:	f9 7f 

0000005a <bfs>:
  5a:	01 78       	bfs	0x[0-9a-f]* [-_<>+0-9a-z]*
  5c:	7e 78 ff 7f 	bfs	0x[0-9a-f]* [-_<>+0-9a-z]*
  60:	7f 78 00 00 	bfs	0x[0-9a-f]* [-_<>+0-9a-z]*
  64:	00 80 

00000066 <bfc>:
  66:	7e 79 7f 00 	bfc	0x[0-9a-f]* [-_<>+0-9a-z]*
  6a:	7e 79 ff 7f 	bfc	0x[0-9a-f]* [-_<>+0-9a-z]*
  6e:	7f 79 04 00 	bfc	0x[0-9a-f]* [-_<>+0-9a-z]*
  72:	00 00 

00000074 <blo>:
  74:	81 7a       	blo	0x[0-9a-f]* [-_<>+0-9a-z]*
  76:	7e 7a 01 80 	blo	0x[0-9a-f]* [-_<>+0-9a-z]*
  7a:	ff 7a       	blo	0x[0-9a-f]* [-_<>+0-9a-z]*

0000007c <bhs>:
  7c:	80 7b       	bhs	0x[0-9a-f]* [-_<>+0-9a-z]*
  7e:	7e 7b 00 88 	bhs	0x[0-9a-f]* [-_<>+0-9a-z]*
  82:	7e 7b f9 07 	bhs	0x[0-9a-f]* [-_<>+0-9a-z]*

00000086 <blt>:
  86:	11 7c       	blt	0x[0-9a-f]* [-_<>+0-9a-z]*
  88:	7e 7c 69 02 	blt	0x[0-9a-f]* [-_<>+0-9a-z]*
  8c:	ff 7c       	blt	0x[0-9a-f]* [-_<>+0-9a-z]*

0000008e <bge>:
  8e:	1a 7d       	bge	0x[0-9a-f]* [-_<>+0-9a-z]*
  90:	7e 7d e6 f6 	bge	0x[0-9a-f]* [-_<>+0-9a-z]*
  94:	7f 7d 08 00 	bge	0x[0-9a-f]* [-_<>+0-9a-z]*
  98:	01 00 

0000009a <br>:
  9a:	0e 7e       	br	0x[0-9a-f]* [-_<>+0-9a-z]*
  9c:	7e 7e 4e 01 	br	0x[0-9a-f]* [-_<>+0-9a-z]*
  a0:	7f 7e f7 ff 	br	0x[0-9a-f]* [-_<>+0-9a-z]*
  a4:	ff ff 

000000a6 <dbnzb>:
  a6:	40 30 0e 00 	dbnzb	r0, 0x[0-9a-f]* [-_<>+0-9a-z]*
  aa:	41 30 97 53 	dbnzb	r1, 0x[0-9a-f]* [-_<>+0-9a-z]*

000000ae <dbnzw>:
  ae:	52 30 cc 0c 	dbnzw	r2, 0x[0-9a-f]* [-_<>+0-9a-z]*
  b2:	53 31 31 00 	dbnzw	r3, 0x[0-9a-f]* [-_<>+0-9a-z]*
  b6:	d8 ff 

000000b8 <dbnzd>:
  b8:	6e 30 f9 07 	dbnzd	r14, 0x[0-9a-f]* [-_<>+0-9a-z]*
  bc:	6f 31 97 00 	dbnzd	r15, 0x[0-9a-f]* [-_<>+0-9a-z]*
  c0:	fa ff 

000000c2 <bal>:
  c2:	71 30 01 00 	bal	r1, 0x[0-9a-f]* [-_<>+0-9a-z]*
  c6:	71 30 ff ff 	bal	r1, 0x[0-9a-f]* [-_<>+0-9a-z]*
  ca:	71 30 e7 55 	bal	r1, 0x[0-9a-f]* [-_<>+0-9a-z]*
  ce:	70 30 59 fa 	bal	r0, 0x[0-9a-f]* [-_<>+0-9a-z]*
  d2:	71 31 05 00 	bal	r1, 0x[0-9a-f]* [-_<>+0-9a-z]*
  d6:	6f 5e 
  d8:	71 31 fa ff 	bal	r1, 0x[0-9a-f]* [-_<>+0-9a-z]*
  dc:	91 a1 

000000de <jal>:
  de:	8e ff       	jal	r14
  e0:	08 30 1f 37 	jal	r1, r15

000000e4 <jalid>:
  e4:	08 30 ce 33 	jalid	r12, r14
