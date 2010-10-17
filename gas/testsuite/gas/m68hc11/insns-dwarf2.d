#objdump: -S
#as: -m68hc11 -gdwarf2
#name: Dwarf2 test on insns.s
#source: insns.s

# Test handling of basic instructions.

.*: +file format elf32\-m68hc11

Disassembly of section .text:

00000000 <_start>:
	.globl _start
	.sect .text

_start:
	lds #stack\+1024
   0:	8e 04 00    	lds	#400 <stack_end>
	ldx #1
   3:	ce 00 01    	ldx	#1 <_start\+0x1>

0+06 <Loop>:
Loop:	
	jsr test
   6:	bd 00 00    	jsr	0 <_start>
	dex
   9:	09          	dex
	bne Loop
   a:	26 fa       	bne	6 <Loop>

0000000c <Stop>:
   c:	cd 03       	.byte	0xcd, 0x03
Stop:
	
	.byte 0xcd
	.byte 3	
	bra _start
   e:	20 f0       	bra	0 <_start>

00000010 <test>:

test:
	ldd #2
  10:	cc 00 02    	ldd	#2 <_start\+0x2>
	jsr test2
  13:	bd 00 00    	jsr	0 <_start>
	rts
  16:	39          	rts

00000017 <test2>:

B_low = 12
A_low = 44
D_low = 50
value = 23
		
	.globl test2
test2:
	ldx value,y
  17:	cd ee 17    	ldx	23,y
	std value,x
  1a:	ed 17       	std	23,x
	ldd ,x
  1c:	ec 00       	ldd	0,x
	sty ,y
  1e:	18 ef 00    	sty	0,y
	stx ,y
  21:	cd ef 00    	stx	0,y
	brclr 6,x,#4,test2
  24:	1f 06 04 ef 	brclr	6,x #\$04 17 <test2>
	brclr 12,x #8 test2
  28:	1f 0c 08 eb 	brclr	12,x #\$08 17 <test2>
	ldd \*ZD1
  2c:	dc 00       	ldd	\*0 <_start>
	ldx \*ZD1\+2
  2e:	de 02       	ldx	\*2 <_start\+0x2>
	clr \*ZD2
  30:	7f 00 00    	clr	0 <_start>
	clr \*ZD2\+1
  33:	7f 00 01    	clr	1 <_start\+0x1>
	bne .-4
  36:	26 fc       	bne	34 <test2\+0x1d>
	beq .\+2
  38:	27 02       	beq	3c <test2\+0x25>
	bclr \*ZD1\+1, #32
  3a:	15 01 20    	bclr	\*1 <_start\+0x1> #\$20
	brclr \*ZD2\+2, #40, test2
  3d:	13 02 28 d6 	brclr	\*2 <_start\+0x2> #\$28 17 <test2>
	ldy #24\+_start-44
  41:	18 ce ff ec 	ldy	#ffec <stack_end\+0xfbec>
	ldd B_low,y
  45:	18 ec 0c    	ldd	12,y
	addd A_low,y
  48:	18 e3 2c    	addd	44,y
	addd D_low,y
  4b:	18 e3 32    	addd	50,y
	subd A_low
  4e:	b3 00 2c    	subd	2c <test2\+0x15>
	subd #A_low
  51:	83 00 2c    	subd	#2c <test2\+0x15>
	jmp Stop
  54:	7e 00 00    	jmp	0 <_start>

00000057 <L1>:
L1:	
	anda #%lo\(test2\)
  57:	84 17       	anda	#23
	andb #%hi\(test2\)
  59:	c4 00       	andb	#0
	ldab #%page\(test2\)	; Check that the relocs are against symbol
  5b:	c6 00       	ldab	#0
	ldy  #%addr\(test2\)	; otherwise linker relaxation fails
  5d:	18 ce 00 00 	ldy	#0 <_start>
	rts
  61:	39          	rts
