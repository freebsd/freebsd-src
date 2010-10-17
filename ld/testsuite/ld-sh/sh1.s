	.text
foo:
L1:	
	mov.l	L2,r0
	.uses	L1
	jsr	@r0
	rts
	.align	2
L2:
	.long	bar
bar:
	rts
	.align	4
