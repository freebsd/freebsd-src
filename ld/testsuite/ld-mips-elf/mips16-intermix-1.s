	.text
	.align	2
	.globl	__start
	.set	nomips16
	.ent	__start
__start:
	.frame	$sp,56,$31		# vars= 0, regs= 3/2, args= 24, gp= 0
	.mask	0x80030000,-24
	.fmask	0x00f00000,-8
	.set	noreorder
	.set	nomacro
	
	addiu	$sp,$sp,-56
	sw	$31,32($sp)
	sw	$17,28($sp)
	sw	$16,24($sp)
	sdc1	$f22,48($sp)
	sdc1	$f20,40($sp)
	jal	m32_l
	move	$4,$17

	move	$4,$17
	jal	m16_l
	move	$16,$2

	addu	$16,$16,$2
	jal	m32_d
	mov.d	$f12,$f22

	addu	$16,$16,$2
	jal	m16_d
	mov.d	$f12,$f22

	move	$4,$17
	mfc1	$7,$f22
	mfc1	$6,$f23
	jal	m32_ld
	addu	$16,$16,$2

	move	$4,$17
	mfc1	$7,$f22
	mfc1	$6,$f23
	jal	m16_ld
	addu	$16,$16,$2

	move	$6,$17
	mov.d	$f12,$f22
	jal	m32_dl
	addu	$16,$16,$2

	move	$6,$17
	mov.d	$f12,$f22
	jal	m16_dl
	addu	$16,$16,$2

	move	$6,$17
	move	$7,$17
	sdc1	$f22,16($sp)
	mov.d	$f12,$f22
	jal	m32_dlld
	addu	$16,$16,$2

	move	$6,$17
	move	$7,$17
	mov.d	$f12,$f22
	sdc1	$f22,16($sp)
	jal	m16_dlld
	addu	$16,$16,$2

	move	$4,$17
	jal	m32_d_l
	addu	$16,$16,$2

	move	$4,$17
	jal	m16_d_l
	mov.d	$f20,$f0

	move	$4,$17
	mfc1	$7,$f22
	mfc1	$6,$f23
	jal	f32
	add.d	$f20,$f20,$f0

	move	$4,$17
	add.d	$f20,$f20,$f0
	mfc1	$7,$f22
	jal	f16
	mfc1	$6,$f23

	add.d	$f20,$f20,$f0
	lw	$31,32($sp)
	trunc.w.d $f0,$f20
	lw	$17,28($sp)
	mfc1	$3,$f0
	addu	$2,$3,$16
	lw	$16,24($sp)
	ldc1	$f22,48($sp)
	ldc1	$f20,40($sp)
	j	$31
	addiu	$sp,$sp,56

	.set	macro
	.set	reorder
	.end	__start
