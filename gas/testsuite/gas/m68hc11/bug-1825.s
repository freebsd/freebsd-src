;;; Bug #1825: gas assemble PC-relative indexed addressing modes incorrectly
;;; http://savannah.gnu.org/bugs/?func=detailbug&bug_id=1825&group_id=2424
;;; 
	.sect .text
	.globl _main
_main:
	nop
	ldx	L1,pc		; Assemble to 5-bit > 0 offset
	bra	L2
L1:
	.dc.w	0xaabb
L2:
	subd	L1,pc		; Assemble to 5-bit < 0 offset
L3:
	.ds.b	14, 0xA7
	ldab	L3,pc		; 5-bit < 0 offset
	ldab	L4,pc		; 5-bit > 0 offset
	.skip	15
L4:
	.skip	128
	subd	L4,pc		; 9-bit < 0 offset
	addd	L5,pc		; 9-bit > 0 offset
	.skip	128
L5:
	.skip	256-3
	orab	L5,pc		; 9 bit < 0 offset (min value)
	oraa	L6,pc		; 9 bit > 0 offset (max value)
	.skip	255
L6:
	.skip	256-2
	orab	L6,pc		; 16 bit < 0 offset
	anda	_main,pc	; 16 bit < 0 offset
	andb	L7,pc
	.skip	256
L7:
	stab	external,pc	; External 16-bit PCREL
	ldd	_table,pc
	addd	_table+2,pc
	subd	_table+4,pc
	addd	_table+8,pc
	addd	_table+12,pc
	addd	_table+16,pc
	rts
	nop
_table:
	.ds.b	16,0
	leax	_table,sp	; 16-bit absolute reloc
	leay	_table,x
	leax	_table,y
