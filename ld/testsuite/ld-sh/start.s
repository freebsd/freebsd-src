	.section .text
	.global	start
start:

	mov.l	stack_k,r15

	! call the mainline	
L1:	
	mov.l	main_k,r0
	.uses	L1
	jsr	@r0
	nop

	.align 2
stack_k:
	.long	_stack	
main_k:
	.long	_main

	.global _trap
_trap:	
	trapa #3
	rts
	nop

	.section .stack
_stack:	.long	0xdeaddead
