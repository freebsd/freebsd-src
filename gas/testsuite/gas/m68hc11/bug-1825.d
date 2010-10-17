#objdump: -S -r
#as: -m68hc12 -gdwarf2
#name: 68HC12 PC-relative addressing modes (bug-1825)

.*:     file format elf32\-m68hc12

Disassembly of section \.text:

0+ <_main>:
;;; 
	\.sect \.text
	\.globl _main
_main:
	nop
   0:	a7          	nop
	ldx	L1,pc		; Assemble to 5\-bit > 0 offset
   1:	ee c2       	ldx	2,PC \{5 <L1>\}
	bra	L2
   3:	20 02       	bra	7 <L2>
			3: R_M68HC12_RL_JUMP	\*ABS\*

0+5 <L1>:
   5:	aa bb       	oraa	5,SP\-

0+7 <L2>:
L1:
	.dc.w	0xaabb
L2:
	subd	L1,pc		; Assemble to 5\-bit < 0 offset
   7:	a3 dc       	subd	\-4,PC \{5 <L1>\}

0+9 <L3>:
   9:	a7          	nop
   a:	a7          	nop
   b:	a7          	nop
   c:	a7          	nop
   d:	a7          	nop
   e:	a7          	nop
   f:	a7          	nop
  10:	a7          	nop
  11:	a7          	nop
  12:	a7          	nop
  13:	a7          	nop
  14:	a7          	nop
  15:	a7          	nop
  16:	a7          	nop
L3:
	.ds.b	14, 0xA7
	ldab	L3,pc		; 5\-bit < 0 offset
  17:	e6 d0       	ldab	\-16,PC \{9 <L3>\}
	ldab	L4,pc		; 5\-bit > 0 offset
  19:	e6 cf       	ldab	15,PC \{2a <L4>\}
	...

0+2a <L4>:
	...
	.skip	15
L4:
	.skip	128
	subd	L4,pc		; 9\-bit < 0 offset
  aa:	a3 f9 7d    	subd	\-131,PC \{2a <L4>\}
	addd	L5,pc		; 9\-bit > 0 offset
  ad:	e3 f8 80    	addd	128,PC \{130 <L5>\}
	...

0+130 <L5>:
	...
 22c:	00          	bgnd
	.skip	128
L5:
	.skip	256\-3
	orab	L5,pc		; 9 bit < 0 offset \(min value\)
 22d:	ea f9 00    	orab	\-256,PC \{130 <L5>\}
	oraa	L6,pc		; 9 bit > 0 offset \(max value\)
 230:	aa f8 ff    	oraa	255,PC \{332 <L6>\}
	...

0+332 <L6>:
	...
 42e:	00          	bgnd
 42f:	00          	bgnd
	.skip	255
L6:
	.skip	256\-2
	orab	L6,pc		; 16 bit < 0 offset
 430:	ea fa fe fe 	orab	\-258,PC \{332 <L6>\}
	anda	_main,pc	; 16 bit < 0 offset
 434:	a4 fa fb c8 	anda	\-1080,PC \{0 <_main>\}
	andb	L7,pc
 438:	e4 fa 01 00 	andb	256,PC \{53c <L7>\}
	...

0+53c <L7>:
	.skip	256
L7:
	stab	external,pc	; External 16\-bit PCREL
 53c:	6b fa fa c0 	stab	\-1344,PC \{0 <_main>\}
			53e: R_M68HC12_PCREL_16	external
	ldd	_table,pc
 540:	ec cf       	ldd	15,PC \{551 <_table>\}
	addd	_table\+2,pc
 542:	e3 cf       	addd	15,PC \{553 <_table\+0x2>\}
	subd	_table\+4,pc
 544:	a3 cf       	subd	15,PC \{555 <_table\+0x4>\}
	addd	_table\+8,pc
 546:	e3 f8 10    	addd	16,PC \{559 <_table\+0x8>\}
	addd	_table\+12,pc
 549:	e3 f8 11    	addd	17,PC \{55d <_table\+0xc>\}
	addd	_table\+16,pc
 54c:	e3 f8 12    	addd	18,PC \{561 <_table\+0x10>\}
	rts
 54f:	3d          	rts
	nop
 550:	a7          	nop

0+551 <_table>:
	...
_table:
	.ds.b	16,0
	leax	_table,sp	; 16\-bit absolute reloc
 561:	1a f2 00 00 	leax	0,SP
			563: R_M68HC12_16	_table
	leay	_table,x
 565:	19 e2 00 00 	leay	0,X
			567: R_M68HC12_16	_table
	leax	_table,y
 569:	1a ea 00 00 	leax	0,Y
			56b: R_M68HC12_16	_table
