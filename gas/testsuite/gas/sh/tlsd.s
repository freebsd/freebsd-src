	.section	.tbss,"awT",@nobits
	.align 2
	.global	foo, bar
	.hidden bar
foo:	.long	25
bar:	.long	27
baz:	.long	29
	.text
	.align 1
	.global	fn
	.type	fn, @function
fn:
	mov.l	r12,@-r15
	mov.l	r14,@-r15
	sts.l	pr,@-r15
	mova	.L3,r0
	mov.l	.L3,r12
	add	r0,r12
	mov	r15,r14

	! Dynamic TLS model, foo not known to be in the current object
	mov.l	1f,r4
	mova	2f,r0
	mov.l	2f,r1
	add	r0,r1
	jsr	@r1
	add	r12,r4
	bra	3f
	nop
	.align	2
1:	.long	foo@TLSGD
2:	.long	__tls_get_addr@PLT
3:

	! Dynamic TLS model, bar and baz known to be in the current object
	mov.l	1f,r4
	mova	2f,r0
	mov.l	2f,r1
	add	r0,r1
	jsr	@r1
	add	r12,r4
	bra	3f
	nop
	.align	2
1:	.long	bar@TLSLDM
2:	.long	__tls_get_addr@PLT
3:
	! Just show that there can be arbitrary instructions here
	mov	#1,r2

	mov.l	.L4,r1
	add	r1,r0
	! r0 now contains &bar

	! Again, arbitrary instructions
	mov.l	r2,@r0

	mov.l	.L5,r1
	add	r1,r0
	! r0 now contains &baz

	mov	r14,r15
	lds.l	@r15+,pr
	mov.l	@r15+,r14
	rts	
	mov.l	@r15+,r12

	.align	2
.L3:	.long	_GLOBAL_OFFSET_TABLE_
.L4:	.long	bar@DTPOFF
.L5:	.long	baz@DTPOFF
