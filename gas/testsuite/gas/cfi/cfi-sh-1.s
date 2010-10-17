	.file	"a.c"
	.text
	.align	1
	.globl foo
	.type	foo, @function
foo:
	.cfi_startproc
	mov.l	r14,@-r15
	.cfi_adjust_cfa_offset 4
	sts.l	pr,@-r15
	.cfi_adjust_cfa_offset 4
	.cfi_offset r15,-4
	.cfi_offset pr,-8
	tst	r4,r4
	bt/s	.L2
	mov	r15,r14
	.cfi_def_cfa_register r14
	add	#-32,r15
	.cfi_adjust_cfa_offset 32
	mov.l	.L3,r0
	jsr	@r0
	mov	r15,r4
.L2:
	mov.l	.L4,r0
	jsr	@r0
	nop
	mov	#0,r0
	mov	r14,r15
	lds.l	@r15+,pr
	rts	
	mov.l	@r15+,r14
.L5:
	.align 2
.L3:
	.long	bar
.L4:
	.long	baz
	.cfi_endproc
	.size	foo, .-foo
