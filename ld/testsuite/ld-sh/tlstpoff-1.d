#source: tlstpoff1.s
#source: tlstpoff2.s
#as: -little
#ld: -EL -e foo
#objdump: -drj.text
#target: sh*-*-linux* sh*-*-netbsd*

.*: +file format elf32-sh.*

Disassembly of section \.text:

[0-9a-f]+ <foo>:
  [0-9a-f]+:	c6 2f       	mov.l	r12,@-r15
  [0-9a-f]+:	07 c7       	mova	[0-9a-f]+ <foo\+0x20>,r0
  [0-9a-f]+:	06 dc       	mov.l	[0-9a-f]+ <foo\+0x20>,r12	! 0x[0-9a-f]+
  [0-9a-f]+:	0c 3c       	add	r0,r12
  [0-9a-f]+:	02 d0       	mov.l	[0-9a-f]+ <foo\+0x14>,r0	! 0xc
  [0-9a-f]+:	12 01       	stc	gbr,r1
  [0-9a-f]+:	09 00       	nop	
  [0-9a-f]+:	03 a0       	bra	[0-9a-f]+ <foo\+0x18>
  [0-9a-f]+:	0c 31       	add	r0,r1
  [0-9a-f]+:	09 00       	nop	
  [0-9a-f]+:	0c 00       	.*[	]*.*
  [0-9a-f]+:	00 00       	.*[	]*.*
  [0-9a-f]+:	12 60       	mov.l	@r1,r0
  [0-9a-f]+:	0b 00       	rts	
  [0-9a-f]+:	f6 6c       	mov.l	@r15\+,r12
  [0-9a-f]+:	09 00       	nop	
  [0-9a-f]+:	[0-9a-f]+ [0-9a-f]+       	.*[	]*.*
  [0-9a-f]+:	[0-9a-f]+ [0-9a-f]+       	.*[	]*.*
#pass
