# D30V relocation test
	
	.text
start:
	add	r2, r0, hello
	add	r4, r0, bar
	add	r4, r0, bar2
	add	r4, r0, unk	
	bra	cont
hello:	.ascii "Hello World\n"
	.align 3
cont:	jmp	cont2
	abs	r2,r3
cont2:
	bra	start	||	nop
	bra.s	exit
	jmp	0	||	nop
	bsrtnz.s	r1,cont
	bsrtnz	r1,cont	
	bratnz.s	r1,cont
	bratnz	r1,cont
	jmptnz.s	r1,cont		
	bsrtnz.s	r1, foo
	jmptnz.s	r1, unk	
	bsrtnz.s	r1, unk
	jmptnz	r1, unk	
	bsrtnz	r1, unk
	bra.s	foo
	bra	foo	
	bra	start
	jmp	start
	jmp	start
	jmp.s	start
	jmp.s	foo
	bra	start
	bra	unknown
	jmp	unknown
	jmp.s	unknown	
	bra.s	unknown
	
	.data
bar:	.asciz	"XYZZY"
bar2:	.long	0xdeadbeef
	
	.text
	.space 0xF00,0

foo:
	add	r1,r0,r0
	ld2w	r60, @(r0,longzero)
	add	r62,r0,r0
	bsr.s	exit
	bsr.s	foo
	bra.s	cont2
	bra.s	cont2				
	bsr.s	exit	
	jmp.s	exit
	jmp.s	exit
	jmp.s	exit		
	bsr	exit
	jmp	exit

longzero:
	.quad	0

	.text
exit:	
	jmp	r62
