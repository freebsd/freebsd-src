#objdump: -S
#as: -m68hc11 -gdwarf2
#name: Dwarf2 test on lbranch.s
#source: lbranch.s

# Test handling of basic instructions.

.*: +file format elf32\-m68hc11

Disassembly of section .text:

0+00 <_rcall>:
	.globl	_rcall
	.globl _start
_start:
_rcall:
	ldaa	#0x10		;86 10
   0:	86 10       	ldaa	#16
	jbra	Lend		; Must be switched to a jmp
   2:	7e 00 00    	jmp	0 <_rcall>
	jbsr	toto		; -> to a jsr
   5:	bd 00 00    	jsr	0 <_rcall>
	jbne	toto		; -> to a beq\+jmp
   8:	27 03       	beq	d <_rcall\+0xd>
   a:	7e 00 00    	jmp	0 <_rcall>
	jbeq	toto		; -> to a bne\+jmp
   d:	26 03       	bne	12 <_rcall\+0x12>
   f:	7e 00 00    	jmp	0 <_rcall>
	jbcs	toto		; -> to a bcc\+jmp
  12:	24 03       	bcc	17 <_rcall\+0x17>
  14:	7e 00 00    	jmp	0 <_rcall>
	jbcc	toto		; -> to a bcs\+jmp
  17:	25 03       	bcs	1c <_rcall\+0x1c>
  19:	7e 00 00    	jmp	0 <_rcall>
	xgdx
  1c:	8f          	xgdx
	xgdx
  1d:	8f          	xgdx
	beq	bidule		; -> to a bne\+jmp
  1e:	26 03       	bne	23 <_rcall\+0x23>
  20:	7e 00 00    	jmp	0 <_rcall>
	bcs	bidule		; -> to a bcc\+jmp
  23:	24 03       	bcc	28 <_rcall\+0x28>
  25:	7e 00 00    	jmp	0 <_rcall>
	bcc	bidule		; -> to a bcs\+jmp
  28:	25 03       	bcs	2d <_rcall\+0x2d>
  2a:	7e 00 00    	jmp	0 <_rcall>
	xgdx
  2d:	8f          	xgdx
	jbra	200
  2e:	7e 00 c8    	jmp	c8 <_rcall\+0xc8>
	jbsr	1923
  31:	bd 07 83    	jsr	783 <L0\+0x602>
	bne	Lend		; -> to a beq\+jmp
  34:	27 03       	beq	39 <_rcall\+0x39>
  36:	7e 00 00    	jmp	0 <_rcall>
	jbsr	toto
  39:	bd 00 00    	jsr	0 <_rcall>
	jbeq	toto
  3c:	26 03       	bne	41 <_rcall\+0x41>
  3e:	7e 00 00    	jmp	0 <_rcall>
	...
	.skip 200
	ldaa	\*dir		;96 33
 109:	96 00       	ldaa	\*0 <_rcall>

0000010b <Lend>:
Lend:
	bhi	external_op
 10b:	23 03       	bls	110 <Lend\+0x5>
 10d:	7e 00 00    	jmp	0 <_rcall>
	bls	external_op
 110:	22 03       	bhi	115 <Lend\+0xa>
 112:	7e 00 00    	jmp	0 <_rcall>
	bsr	out
 115:	bd 00 00    	jsr	0 <_rcall>
	ldx	#12
 118:	ce 00 0c    	ldx	#c <_rcall\+0xc>

0000011b <toto>:
toto:	
	rts
 11b:	39          	rts
	...

00000180 <bidule>:
	.skip 100
bidule:
	rts
 180:	39          	rts
