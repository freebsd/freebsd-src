#as:
#objdump: -dr
#name: cop_insn

.*: +file format .*

Disassembly of section .text:

00000000 <cpi>:
   0:	12 30 34 12 	cpi	\$0x2, \$0x1234
   4:	13 31 65 87 	cpi	\$0x3, \$0x4321, \$0x8765
   8:	21 43 

0000000a <mtcr>:
   a:	1f 30 1e 30 	mtcr	\$0xf, r1, c14

0000000e <mfcr>:
   e:	13 30 72 31 	mfcr	\$0x3, c7, r2

00000012 <mtcsr>:
  12:	12 30 51 32 	mtcsr	\$0x2, r5, cs1

00000016 <mfcsr>:
  16:	11 30 ce 33 	mfcsr	\$0x1, cs12, r14

0000001a <ldcr>:
  1a:	11 30 38 34 	ldcr	\$0x1, r3, c8

0000001e <stcr>:
  1e:	12 30 4b 35 	stcr	\$0x2, c11, r4

00000022 <ldcsr>:
  22:	14 30 6c 36 	ldcsr	\$0x4, r6, cs12

00000026 <stcsr>:
  26:	17 30 da 37 	stcsr	\$0x7, cs10, r13

0000002a <loadmcr>:
  2a:	13 31 01 30 	loadmcr	\$0x3, r1, {c0,c12,c13}
  2e:	2c 00 

00000030 <stormcr>:
  30:	1f 31 1e 30 	stormcr	\$0xf, r14, {c1,c2,c3,c4,c12,c13}
  34:	90 06 

00000036 <loadmcsr>:
  36:	1c 31 28 30 	loadmcsr	\$0xc, r8, {cs3,cs5,cs12,cs13}
  3a:	80 0f 

0000003c <stormcsr>:
  3c:	19 31 39 30 	stormcsr	\$0x9, r9, {cs0,cs3,cs4,cs5,cs12,cs13}
  40:	90 04 

00000042 <bcop>:
  42:	13 30 48 77 	bcop	\$0x7, \$0x3, 0x[0-9a-f]* [-_<>+0-9a-z]*
  46:	1c 31 fa 76 	bcop	\$0x6, \$0xc, 0x[0-9a-f]* [-_<>+0-9a-z]*
  4a:	01 19 

0000004c <cpdop>:
  4c:	13 30 45 b2 	cpdop	\$0x3, \$0x2, r4, r5
  50:	17 31 12 ba 	cpdop	\$0x7, \$0xa, r1, r2, \$0xba12
  54:	34 12 

00000056 <mtpr>:
  56:	09 30 10 00 	mtpr	r0, hi

0000005a <mfpr>:
  5a:	0a 30 05 11 	mfpr	lo, r5
  5e:	0a 30 0a 90 	mfpr	uhi, r10

00000062 <cinv>:
  62:	10 30 0f 00 	cinv	\[b,d,i,u\]
