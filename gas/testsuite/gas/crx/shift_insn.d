#as:
#objdump: -dr
#name: shift_insn

.*: +file format .*

Disassembly of section .text:

00000000 <sllb>:
   0:	71 fc       	sllb	\$0x7, r1
   2:	23 4d       	sllb	r2, r3

00000004 <srlb>:
   4:	d4 fc       	srlb	\$0x5, r4
   6:	56 4e       	srlb	r5, r6

00000008 <srab>:
   8:	47 fd       	srab	\$0x4, r7
   a:	89 4f       	srab	r8, r9

0000000c <sllw>:
   c:	fa b6       	sllw	\$0xf, r10
   e:	bc 5d       	sllw	r11, r12

00000010 <srlw>:
  10:	ed b7       	srlw	\$0xe, r13
  12:	ef 5e       	srlw	r14, r15

00000014 <sraw>:
  14:	de b8       	sraw	\$0xd, r14
  16:	f1 5f       	sraw	r15, r1

00000018 <slld>:
  18:	f2 f1       	slld	\$0x1f, r2
  1a:	34 6d       	slld	r3, r4

0000001c <srld>:
  1c:	f5 f3       	srld	\$0x1f, r5
  1e:	67 6e       	srld	r6, r7

00000020 <srad>:
  20:	28 f5       	srad	\$0x12, r8
  22:	9a 6f       	srad	r9, r10
