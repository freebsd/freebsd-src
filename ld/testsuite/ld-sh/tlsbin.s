	.section ".tbss", "awT", @nobits
	.globl bg1, bg2
bg1:	.space 4
bg2:	.space 4
bl1:	.space 4
bl2:	.space 4
	.text
	.globl	_start
	.type	_start,@function
_start:
	mov.l	r12,@-r15
	mov.l	r14,@-r15
	mov	r15,r14
	! Set up .GOT pointer for non-pic @gottpoff sequences
	mova	.L3,r0
	mov.l	.L3,r12
	add	r0,r12
	nop;nop;nop;nop

	! @GOTTPOFF IE against global var
	mov.l	1f,r0
	stc	gbr,r1
	mov.l	@(r0,r12),r0
	bra	2f
	add	r0,r1
	.align	2
1:	.long	sG3@GOTTPOFF
2:
	nop;nop;nop;nop

	! @GOTTPOFF IE -> LE against global var defined in exec
	mov.l	1f,r0
	stc	gbr,r1
	mov.l	@(r0,r12),r0
	bra	2f
	add	r0,r1
	.align	2
1:	.long	bg1@GOTTPOFF
2:
	nop;nop;nop;nop

	! @GOTTPOFF IE -> LE against local var
	mov.l	1f,r0
	stc	gbr,r1
	mov.l	@(r0,r12),r0
	bra	2f
	add	r0,r1
	.align	2
1:	.long	bl2@GOTTPOFF
2:
	nop;nop;nop;nop

	! @GOTTPOFF IE -> LE against hidden but not local var
	mov.l	1f,r0
	stc	gbr,r1
	mov.l	@(r0,r12),r0
	bra	2f
	add	r0,r1
	.align	2
1:	.long	sh2@GOTTPOFF
2:
	nop;nop;nop;nop

	! LE @TPOFF, global var defined in exec
	stc	gbr,r1
	mov.l	.L4,r0
	add	r1,r0
	nop;nop;nop;nop

	! LE @TPOFF, local var
	stc	gbr,r1
	mov.l	.L5,r0
	add	r1,r0
	nop;nop;nop;nop

	! LE @TPOFF, hidden var defined in exec
	stc	gbr,r1
	mov.l	.L6,r0
	add	r1,r0
	nop;nop;nop;nop

	mov	r14,r15
	rts	
	mov.l	@r15+,r14

	.align	2
.L3:	.long	_GLOBAL_OFFSET_TABLE_
.L4:	.long	sg1@TPOFF
.L5:	.long	bl1@TPOFF
.L6:	.long	sh1@TPOFF
