	! Force .got aligned to 4K, so it very likely gets at 0x413000
	.data
	.balign	4096
	.section ".tdata", "awT", @progbits
	.globl sg1, sg2
	.globl sh1, sh2
	.hidden sh1, sh2
sg1:	.long 17
sg2:	.long 18
sl1:	.long 65
sl2:	.long 66
sh1:	.long 257
sh2:	.long 258
	! Force .text aligned to 4K, so it very likely gets at 0x401000.
	.text
	.balign	4096
	.globl	fn2
	.type	fn2,@function
fn2:
	mov.l	r12,@-r15
	mov.l	r14,@-r15
	sts.l	pr,@-r15
	mova	.L3,r0
	mov.l	.L3,r12
	add	r0,r12
	mov	r15,r14
	nop;nop;nop;nop

	! GD -> IE because variable is not defined in executable
	mov.l	1f,r4
	mova	2f,r0
	mov.l	2f,r1
	add	r0,r1
	jsr	@r1
	add	r12,r4
	bra	3f
	nop
	.align	2
1:	.long	sG1@TLSGD
2:	.long	__tls_get_addr@PLT
3:
	nop;nop;nop;nop

	! GD -> IE because variable is not defined in executable where
	!   the variable is referenced through @gottpoff too
	mov.l	1f,r4
	mova	2f,r0
	mov.l	2f,r1
	add	r0,r1
	jsr	@r1
	add	r12,r4
	bra	3f
	nop
	.align	2
1:	.long	sG2@TLSGD
2:	.long	__tls_get_addr@PLT
3:
	nop;nop;nop;nop

	! GD -> LE with global variable defined in executable
	mov.l	1f,r4
	mova	2f,r0
	mov.l	2f,r1
	add	r0,r1
	jsr	@r1
	add	r12,r4
	bra	3f
	nop
	.align	2
1:	.long	sg1@TLSGD
2:	.long	__tls_get_addr@PLT
3:
	nop;nop;nop;nop

	! GD -> LE with local variable defined in executable
	mov.l	1f,r4
	mova	2f,r0
	mov.l	2f,r1
	add	r0,r1
	jsr	@r1
	add	r12,r4
	bra	3f
	nop
	.align	2
1:	.long	sl1@TLSGD
2:	.long	__tls_get_addr@PLT
3:
	nop;nop;nop;nop

	! GD -> LE with hidden variable defined in executable
	mov.l	1f,r4
	mova	2f,r0
	mov.l	2f,r1
	add	r0,r1
	jsr	@r1
	add	r12,r4
	bra	3f
	nop
	.align	2
1:	.long	sh1@TLSGD
2:	.long	__tls_get_addr@PLT
3:
	nop;nop;nop;nop

	! LD -> LE with local variable defined in executable
	mov.l	1f,r4
	mova	2f,r0
	mov.l	2f,r1
	add	r0,r1
	jsr	@r1
	add	r12,r4
	bra	3f
	nop
	.align	2
1:	.long	sl1@TLSLDM
2:	.long	__tls_get_addr@PLT
3:
	nop;nop
	mov.l	.L4,r1
	add	r0,r1
	nop;nop
	mov.l	.L5,r2
	add	r0,r2
	nop;nop;nop;nop

	! LD -> LE against hidden variables
	mov.l	1f,r4
	mova	2f,r0
	mov.l	2f,r1
	add	r0,r1
	jsr	@r1
	add	r12,r4
	bra	3f
	nop
	.align	2
1:	.long	sh1@TLSLDM
2:	.long	__tls_get_addr@PLT
3:
	nop;nop
	mov.l	.L6,r1
	add	r0,r1
	nop;nop
	mov.l	.L7,r2
	add	r0,r2
	nop;nop;nop;nop

	! @GOTTPOFF IE against global var
	mov.l	1f,r0
	stc	gbr,r1
	mov.l	@(r0,r12),r0
	bra	2f
	add	r0,r1
	.align	2
1:	.long	sG2@GOTTPOFF
2:
	nop;nop;nop;nop

	! @GOTTPOFF IE against global var
	mov.l	1f,r0
	stc	gbr,r1
	mov.l	@(r0,r12),r0
	bra	2f
	add	r1,r0
	.align	2
1:	.long	sG4@GOTTPOFF
2:
	nop;nop;nop;nop

	! @GOTTPOFF IE -> LE against global var defined in exec
	mov.l	1f,r0
	stc	gbr,r1
	mov.l	@(r0,r12),r0
	bra	2f
	add	r0,r1
	.align	2
1:	.long	sg1@GOTTPOFF
2:
	nop;nop;nop;nop

	! @GOTTPOFF IE -> LE against hidden var
	mov.l	1f,r0
	stc	gbr,r1
	mov.l	@(r0,r12),r0
	bra	2f
	add	r0,r1
	.align	2
1:	.long	sh1@GOTTPOFF
2:
	nop;nop;nop;nop

	mov	r14,r15
	lds.l	@r15+,pr
	mov.l	@r15+,r14
	rts	
	mov.l	@r15+,r12

	.align 2
.L3:	.long	_GLOBAL_OFFSET_TABLE_
.L4:	.long	sl1@DTPOFF
.L5:	.long	sl2@DTPOFF
.L6:	.long	sh1@DTPOFF
.L7:	.long	sh2@DTPOFF
	! Fill page with 0.
	.space	.L8-.
	.balign	4096
.L8:
