	.globl	f1
begin:
	nop
	nop
f1:
	nop
	nop
	nop
f2:
	call	f1
	bra	f1
	call	f2
	bra	f2

	call	f3
	bra	f3
	.space	16
	
	.section .gnu.linkonce.t.test
beginx:
	nop
	call	f1
	bra	f1
	call	f2
	bra	f2
	call	f3
	bra	f3
