# Test for correct generation of 68HC11 insns.
	
	.globl _start
	.sect .text

_start:
	lds #stack+1024
	ldx #1
Loop:	
	jsr test
	dex
	bne Loop
Stop:
	
	.byte 0xcd
	.byte 3	
	bra _start

test:
	ldd #2
	jsr test2
	rts

B_low = 12
A_low = 44
D_low = 50
value = 23
		
	.globl test2
test2:
	ldx value,y
	std value,x
	ldd ,x
	sty ,y
	stx ,y
	brclr 6,x,#4,test2
	brclr 12,x #8 test2
	ldd *ZD1
	ldx *ZD1+2
	clr *ZD2
	clr *ZD2+1
	bne .-4
	beq .+2
	bclr *ZD1+1, #32
	brclr *ZD2+2, #40, test2
	ldy #24+_start-44
	ldd B_low,y
	addd A_low,y
	addd D_low,y
	subd A_low
	subd #A_low
	jmp Stop
L1:	
	anda #%lo(test2)
	andb #%hi(test2)
	ldab #%page(test2)	; Check that the relocs are against symbol
	ldy  #%addr(test2)	; otherwise linker relaxation fails
	rts

	.sect .data

	.sect .bss
stack:
	.space	1024
stack_end:
