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
	.text
	.align	1
	.globl	fn1
	.type	fn1,@function
fn1:
	mov.l	r12,@-r15
	mov.l	r14,@-r15
	sts.l	pr,@-r15
	mova	.L3,r0
	mov.l	.L3,r12
	add	r0,r12
	mov	r15,r14
	nop;nop;nop;nop

	! GD
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

	! GD -> IE because variable is referenced through @GOTTPOFF too
	mov.l	1f,r4
	mova	2f,r0
	mov.l	2f,r1
	add	r0,r1
	jsr	@r1
	add	r12,r4
	bra	3f
	nop
	.align	2
1:	.long	sg2@TLSGD
2:	.long	__tls_get_addr@PLT
3:
	nop;nop;nop;nop

	! GD against local variable
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

	! GD -> IE against local variable referenced through @GOTTPOFF too
	mov.l	1f,r4
	mova	2f,r0
	mov.l	2f,r1
	add	r0,r1
	jsr	@r1
	add	r12,r4
	bra	3f
	nop
	.align	2
1:	.long	sl2@TLSGD
2:	.long	__tls_get_addr@PLT
3:
	nop;nop;nop;nop

	! GD against hidden and local variable
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

	! GD -> IE against hidden and local variable referenced through
	! @GOTTPOFF too
	mov.l	1f,r4
	mova	2f,r0
	mov.l	2f,r1
	add	r0,r1
	jsr	@r1
	add	r12,r4
	bra	3f
	nop
	.align	2
1:	.long	sh2@TLSGD
2:	.long	__tls_get_addr@PLT
3:
	nop;nop;nop;nop

	! GD against hidden but not local variable
	mov.l	1f,r4
	mova	2f,r0
	mov.l	2f,r1
	add	r0,r1
	jsr	@r1
	add	r12,r4
	bra	3f
	nop
	.align	2
1:	.long	sH1@TLSGD
2:	.long	__tls_get_addr@PLT
3:
	nop;nop;nop;nop

	! GD -> IE against hidden but not local variable referenced through
	! @GOTTPOFF too
	mov.l	1f,r4
	mova	2f,r0
	mov.l	2f,r1
	add	r0,r1
	jsr	@r1
	add	r12,r4
	bra	3f
	nop
	.align	2
1:	.long	sH2@TLSGD
2:	.long	__tls_get_addr@PLT
3:
	nop;nop;nop;nop

	! LD
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

	! LD against hidden and local variables
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
	mov.l	.L6,r1
	add	r0,r1
	nop;nop
	mov.l	.L7,r2
	add	r0,r2
	nop;nop;nop;nop

	! LD against hidden but not local variables
	mov.l	1f,r4
	mova	2f,r0
	mov.l	2f,r1
	add	r0,r1
	jsr	@r1
	add	r12,r4
	bra	3f
	nop
	.align	2
1:	.long	sH1@TLSLDM
2:	.long	__tls_get_addr@PLT
3:
	nop;nop
	mov.l	.L8,r1
	add	r0,r1
	nop;nop
	mov.l	.L9,r2
	add	r0,r2
	nop;nop;nop;nop

	! @GOTTPOFF IE against global var
	mov.l	1f,r0
	stc	gbr,r1
	mov.l	@(r0,r12),r0
	bra	2f
	add	r0,r1
	.align	2
1:	.long	sg2@GOTTPOFF
2:
	nop;nop;nop;nop

	! @GOTTPOFF IE against local var
	mov.l	1f,r0
	stc	gbr,r1
	mov.l	@(r0,r12),r0
	bra	2f
	add	r0,r1
	.align	2
1:	.long	sl2@GOTTPOFF
2:
	nop;nop;nop;nop

	! @GOTTPOFF IE against hidden and local var
	mov.l	1f,r0
	stc	gbr,r1
	mov.l	@(r0,r12),r0
	bra	2f
	add	r0,r1
	.align	2
1:	.long	sh2@GOTTPOFF
2:
	nop;nop;nop;nop

	! @GOTTPOFF IE against hidden but not local var
	mov.l	1f,r0
	stc	gbr,r1
	mov.l	@(r0,r12),r0
	bra	2f
	add	r0,r1
	.align	2
1:	.long	sH2@GOTTPOFF
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
.L5:	.long	sl1@DTPOFF + 4
.L6:	.long	sh1@DTPOFF
.L7:	.long	sh2@DTPOFF
.L8:	.long	sH1@DTPOFF
.L9:	.long	sH2@DTPOFF
