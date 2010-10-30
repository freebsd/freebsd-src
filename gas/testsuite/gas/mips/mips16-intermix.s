	.text
	.align	2
	.globl	m32_l
	.set	nomips16
	.ent	m32_l
m32_l:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, gp= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	.set	noreorder
	.set	nomacro
	
	j	$31
	move	$2,$4

	.set	macro
	.set	reorder
	.end	m32_l

	.align	2
	.globl	m16_l
	.set	mips16
	.ent	m16_l
m16_l:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, gp= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	.set	noreorder
	.set	nomacro
	j	$31
	move	$2,$4
	.set	macro
	.set	reorder

	.end	m16_l

	.align	2
	.set	nomips16
	.ent	m32_static_l
m32_static_l:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, gp= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	.set	noreorder
	.set	nomacro
	
	j	$31
	move	$2,$4

	.set	macro
	.set	reorder
	.end	m32_static_l

	.align	2
	.set	mips16
	.ent	m16_static_l
m16_static_l:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, gp= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	.set	noreorder
	.set	nomacro
	j	$31
	move	$2,$4
	.set	macro
	.set	reorder

	.end	m16_static_l

	.align	2
	.set	nomips16
	.ent	m32_static1_l
m32_static1_l:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, gp= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	.set	noreorder
	.set	nomacro
	
	j	$31
	move	$2,$4

	.set	macro
	.set	reorder
	.end	m32_static1_l

	.align	2
	.set	mips16
	.ent	m16_static1_l
m16_static1_l:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, gp= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	.set	noreorder
	.set	nomacro
	j	$31
	move	$2,$4
	.set	macro
	.set	reorder

	.end	m16_static1_l

	.align	2
	.set	nomips16
	.ent	m32_static32_l
m32_static32_l:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, gp= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	.set	noreorder
	.set	nomacro
	
	j	$31
	move	$2,$4

	.set	macro
	.set	reorder
	.end	m32_static32_l

	.align	2
	.set	mips16
	.ent	m16_static32_l
m16_static32_l:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, gp= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	.set	noreorder
	.set	nomacro
	j	$31
	move	$2,$4
	.set	macro
	.set	reorder

	.end	m16_static32_l

	.align	2
	.set	nomips16
	.ent	m32_static16_l
m32_static16_l:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, gp= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	.set	noreorder
	.set	nomacro
	
	j	$31
	move	$2,$4

	.set	macro
	.set	reorder
	.end	m32_static16_l

	.align	2
	.set	mips16
	.ent	m16_static16_l
m16_static16_l:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, gp= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	.set	noreorder
	.set	nomacro
	j	$31
	move	$2,$4
	.set	macro
	.set	reorder

	.end	m16_static16_l

	.align	2
	.globl	m32_d
	.set	nomips16
	.ent	m32_d
m32_d:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, gp= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	.set	noreorder
	.set	nomacro
	
	trunc.w.d $f12,$f12
	j	$31
	mfc1	$2,$f12

	.set	macro
	.set	reorder
	.end	m32_d

	.align	2
	.globl	m16_d
	.set	mips16
	.ent	m16_d
m16_d:
	.frame	$sp,24,$31		# vars= 0, regs= 1/0, args= 16, gp= 0
	.mask	0x80000000,-4
	.fmask	0x00000000,0
	save	24,$31
	jal	__mips16_fixdfsi
	restore	24,$31
	j	$31
	.end	m16_d
	# Stub function for m16_d (double)
	.set	nomips16
	.section	.mips16.fn.m16_d,"ax",@progbits
	.align	2
	.ent	__fn_stub_m16_d
__fn_stub_m16_d:
	.set	noreorder
	mfc1	$4,$f13
	mfc1	$5,$f12
	.set	noat
	la	$1,m16_d
	jr	$1
	.set	at
	nop
	.set	reorder
	.end	__fn_stub_m16_d
	.previous

	.align	2
	.set	nomips16
	.ent	m32_static_d
m32_static_d:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, gp= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	.set	noreorder
	.set	nomacro
	
	trunc.w.d $f12,$f12
	j	$31
	mfc1	$2,$f12

	.set	macro
	.set	reorder
	.end	m32_static_d

	.align	2
	.set	mips16
	.ent	m16_static_d
m16_static_d:
	.frame	$sp,24,$31		# vars= 0, regs= 1/0, args= 16, gp= 0
	.mask	0x80000000,-4
	.fmask	0x00000000,0
	save	24,$31
	jal	__mips16_fixdfsi
	restore	24,$31
	j	$31
	.end	m16_static_d
	# Stub function for m16_static_d (double)
	.set	nomips16
	.section	.mips16.fn.m16_static_d,"ax",@progbits
	.align	2
	.ent	__fn_stub_m16_static_d
__fn_stub_m16_static_d:
	.set	noreorder
	mfc1	$4,$f13
	mfc1	$5,$f12
	.set	noat
	la	$1,m16_static_d
	jr	$1
	.set	at
	nop
	.set	reorder
	.end	__fn_stub_m16_static_d
	.previous

	.align	2
	.set	nomips16
	.ent	m32_static1_d
m32_static1_d:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, gp= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	.set	noreorder
	.set	nomacro
	
	trunc.w.d $f12,$f12
	j	$31
	mfc1	$2,$f12

	.set	macro
	.set	reorder
	.end	m32_static1_d

	.align	2
	.set	mips16
	.ent	m16_static1_d
m16_static1_d:
	.frame	$sp,24,$31		# vars= 0, regs= 1/0, args= 16, gp= 0
	.mask	0x80000000,-4
	.fmask	0x00000000,0
	save	24,$31
	jal	__mips16_fixdfsi
	restore	24,$31
	j	$31
	.end	m16_static1_d
	# Stub function for m16_static1_d (double)
	.set	nomips16
	.section	.mips16.fn.m16_static1_d,"ax",@progbits
	.align	2
	.ent	__fn_stub_m16_static1_d
__fn_stub_m16_static1_d:
	.set	noreorder
	mfc1	$4,$f13
	mfc1	$5,$f12
	.set	noat
	la	$1,m16_static1_d
	jr	$1
	.set	at
	nop
	.set	reorder
	.end	__fn_stub_m16_static1_d
	.previous

	.align	2
	.set	nomips16
	.ent	m32_static32_d
m32_static32_d:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, gp= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	.set	noreorder
	.set	nomacro
	
	trunc.w.d $f12,$f12
	j	$31
	mfc1	$2,$f12

	.set	macro
	.set	reorder
	.end	m32_static32_d

	.align	2
	.set	mips16
	.ent	m16_static32_d
m16_static32_d:
	.frame	$sp,24,$31		# vars= 0, regs= 1/0, args= 16, gp= 0
	.mask	0x80000000,-4
	.fmask	0x00000000,0
	save	24,$31
	jal	__mips16_fixdfsi
	restore	24,$31
	j	$31
	.end	m16_static32_d
	# Stub function for m16_static32_d (double)
	.set	nomips16
	.section	.mips16.fn.m16_static32_d,"ax",@progbits
	.align	2
	.ent	__fn_stub_m16_static32_d
__fn_stub_m16_static32_d:
	.set	noreorder
	mfc1	$4,$f13
	mfc1	$5,$f12
	.set	noat
	la	$1,m16_static32_d
	jr	$1
	.set	at
	nop
	.set	reorder
	.end	__fn_stub_m16_static32_d
	.previous

	.align	2
	.set	nomips16
	.ent	m32_static16_d
m32_static16_d:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, gp= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	.set	noreorder
	.set	nomacro
	
	trunc.w.d $f12,$f12
	j	$31
	mfc1	$2,$f12

	.set	macro
	.set	reorder
	.end	m32_static16_d

	.align	2
	.set	mips16
	.ent	m16_static16_d
m16_static16_d:
	.frame	$sp,24,$31		# vars= 0, regs= 1/0, args= 16, gp= 0
	.mask	0x80000000,-4
	.fmask	0x00000000,0
	save	24,$31
	jal	__mips16_fixdfsi
	restore	24,$31
	j	$31
	.end	m16_static16_d
	# Stub function for m16_static16_d (double)
	.set	nomips16
	.section	.mips16.fn.m16_static16_d,"ax",@progbits
	.align	2
	.ent	__fn_stub_m16_static16_d
__fn_stub_m16_static16_d:
	.set	noreorder
	mfc1	$4,$f13
	mfc1	$5,$f12
	.set	noat
	la	$1,m16_static16_d
	jr	$1
	.set	at
	nop
	.set	reorder
	.end	__fn_stub_m16_static16_d
	.previous

	.align	2
	.globl	m32_ld
	.set	nomips16
	.ent	m32_ld
m32_ld:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, gp= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	.set	noreorder
	.set	nomacro
	
	mtc1	$7,$f2
	mtc1	$6,$f3
	trunc.w.d $f0,$f2
	mfc1	$24,$f0
	j	$31
	addu	$2,$24,$4

	.set	macro
	.set	reorder
	.end	m32_ld

	.align	2
	.globl	m16_ld
	.set	mips16
	.ent	m16_ld
m16_ld:
	.frame	$sp,24,$31		# vars= 0, regs= 2/0, args= 16, gp= 0
	.mask	0x80010000,-4
	.fmask	0x00000000,0
	save	24,$16,$31
	move	$16,$4
	move	$5,$7
	.set	noreorder
	.set	nomacro
	jal	__mips16_fixdfsi
	move	$4,$6
	.set	macro
	.set	reorder

	addu	$2,$16
	restore	24,$16,$31
	j	$31
	.end	m16_ld

	.align	2
	.set	nomips16
	.ent	m32_static_ld
m32_static_ld:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, gp= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	.set	noreorder
	.set	nomacro
	
	mtc1	$7,$f2
	mtc1	$6,$f3
	trunc.w.d $f0,$f2
	mfc1	$24,$f0
	j	$31
	addu	$2,$24,$4

	.set	macro
	.set	reorder
	.end	m32_static_ld

	.align	2
	.set	mips16
	.ent	m16_static_ld
m16_static_ld:
	.frame	$sp,24,$31		# vars= 0, regs= 2/0, args= 16, gp= 0
	.mask	0x80010000,-4
	.fmask	0x00000000,0
	save	24,$16,$31
	move	$16,$4
	move	$5,$7
	.set	noreorder
	.set	nomacro
	jal	__mips16_fixdfsi
	move	$4,$6
	.set	macro
	.set	reorder

	addu	$2,$16
	restore	24,$16,$31
	j	$31
	.end	m16_static_ld

	.align	2
	.set	nomips16
	.ent	m32_static1_ld
m32_static1_ld:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, gp= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	.set	noreorder
	.set	nomacro
	
	mtc1	$7,$f2
	mtc1	$6,$f3
	trunc.w.d $f0,$f2
	mfc1	$24,$f0
	j	$31
	addu	$2,$24,$4

	.set	macro
	.set	reorder
	.end	m32_static1_ld

	.align	2
	.set	mips16
	.ent	m16_static1_ld
m16_static1_ld:
	.frame	$sp,24,$31		# vars= 0, regs= 2/0, args= 16, gp= 0
	.mask	0x80010000,-4
	.fmask	0x00000000,0
	save	24,$16,$31
	move	$16,$4
	move	$5,$7
	.set	noreorder
	.set	nomacro
	jal	__mips16_fixdfsi
	move	$4,$6
	.set	macro
	.set	reorder

	addu	$2,$16
	restore	24,$16,$31
	j	$31
	.end	m16_static1_ld

	.align	2
	.set	nomips16
	.ent	m32_static32_ld
m32_static32_ld:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, gp= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	.set	noreorder
	.set	nomacro
	
	mtc1	$7,$f2
	mtc1	$6,$f3
	trunc.w.d $f0,$f2
	mfc1	$24,$f0
	j	$31
	addu	$2,$24,$4

	.set	macro
	.set	reorder
	.end	m32_static32_ld

	.align	2
	.set	mips16
	.ent	m16_static32_ld
m16_static32_ld:
	.frame	$sp,24,$31		# vars= 0, regs= 2/0, args= 16, gp= 0
	.mask	0x80010000,-4
	.fmask	0x00000000,0
	save	24,$16,$31
	move	$16,$4
	move	$5,$7
	.set	noreorder
	.set	nomacro
	jal	__mips16_fixdfsi
	move	$4,$6
	.set	macro
	.set	reorder

	addu	$2,$16
	restore	24,$16,$31
	j	$31
	.end	m16_static32_ld

	.align	2
	.set	nomips16
	.ent	m32_static16_ld
m32_static16_ld:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, gp= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	.set	noreorder
	.set	nomacro
	
	mtc1	$7,$f2
	mtc1	$6,$f3
	trunc.w.d $f0,$f2
	mfc1	$24,$f0
	j	$31
	addu	$2,$24,$4

	.set	macro
	.set	reorder
	.end	m32_static16_ld

	.align	2
	.set	mips16
	.ent	m16_static16_ld
m16_static16_ld:
	.frame	$sp,24,$31		# vars= 0, regs= 2/0, args= 16, gp= 0
	.mask	0x80010000,-4
	.fmask	0x00000000,0
	save	24,$16,$31
	move	$16,$4
	move	$5,$7
	.set	noreorder
	.set	nomacro
	jal	__mips16_fixdfsi
	move	$4,$6
	.set	macro
	.set	reorder

	addu	$2,$16
	restore	24,$16,$31
	j	$31
	.end	m16_static16_ld

	.align	2
	.globl	m32_dl
	.set	nomips16
	.ent	m32_dl
m32_dl:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, gp= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	.set	noreorder
	.set	nomacro
	
	trunc.w.d $f0,$f12
	mfc1	$24,$f0
	j	$31
	addu	$2,$24,$6

	.set	macro
	.set	reorder
	.end	m32_dl

	.align	2
	.globl	m16_dl
	.set	mips16
	.ent	m16_dl
m16_dl:
	.frame	$sp,24,$31		# vars= 0, regs= 2/0, args= 16, gp= 0
	.mask	0x80010000,-4
	.fmask	0x00000000,0
	save	24,$16,$31
	.set	noreorder
	.set	nomacro
	jal	__mips16_fixdfsi
	move	$16,$6
	.set	macro
	.set	reorder

	addu	$2,$16
	restore	24,$16,$31
	j	$31
	.end	m16_dl
	# Stub function for m16_dl (double)
	.set	nomips16
	.section	.mips16.fn.m16_dl,"ax",@progbits
	.align	2
	.ent	__fn_stub_m16_dl
__fn_stub_m16_dl:
	.set	noreorder
	mfc1	$4,$f13
	mfc1	$5,$f12
	.set	noat
	la	$1,m16_dl
	jr	$1
	.set	at
	nop
	.set	reorder
	.end	__fn_stub_m16_dl
	.previous

	.align	2
	.set	nomips16
	.ent	m32_static_dl
m32_static_dl:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, gp= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	.set	noreorder
	.set	nomacro
	
	trunc.w.d $f0,$f12
	mfc1	$24,$f0
	j	$31
	addu	$2,$24,$6

	.set	macro
	.set	reorder
	.end	m32_static_dl

	.align	2
	.set	mips16
	.ent	m16_static_dl
m16_static_dl:
	.frame	$sp,24,$31		# vars= 0, regs= 2/0, args= 16, gp= 0
	.mask	0x80010000,-4
	.fmask	0x00000000,0
	save	24,$16,$31
	.set	noreorder
	.set	nomacro
	jal	__mips16_fixdfsi
	move	$16,$6
	.set	macro
	.set	reorder

	addu	$2,$16
	restore	24,$16,$31
	j	$31
	.end	m16_static_dl
	# Stub function for m16_static_dl (double)
	.set	nomips16
	.section	.mips16.fn.m16_static_dl,"ax",@progbits
	.align	2
	.ent	__fn_stub_m16_static_dl
__fn_stub_m16_static_dl:
	.set	noreorder
	mfc1	$4,$f13
	mfc1	$5,$f12
	.set	noat
	la	$1,m16_static_dl
	jr	$1
	.set	at
	nop
	.set	reorder
	.end	__fn_stub_m16_static_dl
	.previous

	.align	2
	.set	nomips16
	.ent	m32_static1_dl
m32_static1_dl:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, gp= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	.set	noreorder
	.set	nomacro
	
	trunc.w.d $f0,$f12
	mfc1	$24,$f0
	j	$31
	addu	$2,$24,$6

	.set	macro
	.set	reorder
	.end	m32_static1_dl

	.align	2
	.set	mips16
	.ent	m16_static1_dl
m16_static1_dl:
	.frame	$sp,24,$31		# vars= 0, regs= 2/0, args= 16, gp= 0
	.mask	0x80010000,-4
	.fmask	0x00000000,0
	save	24,$16,$31
	.set	noreorder
	.set	nomacro
	jal	__mips16_fixdfsi
	move	$16,$6
	.set	macro
	.set	reorder

	addu	$2,$16
	restore	24,$16,$31
	j	$31
	.end	m16_static1_dl
	# Stub function for m16_static1_dl (double)
	.set	nomips16
	.section	.mips16.fn.m16_static1_dl,"ax",@progbits
	.align	2
	.ent	__fn_stub_m16_static1_dl
__fn_stub_m16_static1_dl:
	.set	noreorder
	mfc1	$4,$f13
	mfc1	$5,$f12
	.set	noat
	la	$1,m16_static1_dl
	jr	$1
	.set	at
	nop
	.set	reorder
	.end	__fn_stub_m16_static1_dl
	.previous

	.align	2
	.set	nomips16
	.ent	m32_static32_dl
m32_static32_dl:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, gp= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	.set	noreorder
	.set	nomacro
	
	trunc.w.d $f0,$f12
	mfc1	$24,$f0
	j	$31
	addu	$2,$24,$6

	.set	macro
	.set	reorder
	.end	m32_static32_dl

	.align	2
	.set	mips16
	.ent	m16_static32_dl
m16_static32_dl:
	.frame	$sp,24,$31		# vars= 0, regs= 2/0, args= 16, gp= 0
	.mask	0x80010000,-4
	.fmask	0x00000000,0
	save	24,$16,$31
	.set	noreorder
	.set	nomacro
	jal	__mips16_fixdfsi
	move	$16,$6
	.set	macro
	.set	reorder

	addu	$2,$16
	restore	24,$16,$31
	j	$31
	.end	m16_static32_dl
	# Stub function for m16_static32_dl (double)
	.set	nomips16
	.section	.mips16.fn.m16_static32_dl,"ax",@progbits
	.align	2
	.ent	__fn_stub_m16_static32_dl
__fn_stub_m16_static32_dl:
	.set	noreorder
	mfc1	$4,$f13
	mfc1	$5,$f12
	.set	noat
	la	$1,m16_static32_dl
	jr	$1
	.set	at
	nop
	.set	reorder
	.end	__fn_stub_m16_static32_dl
	.previous

	.align	2
	.set	nomips16
	.ent	m32_static16_dl
m32_static16_dl:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, gp= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	.set	noreorder
	.set	nomacro
	
	trunc.w.d $f0,$f12
	mfc1	$24,$f0
	j	$31
	addu	$2,$24,$6

	.set	macro
	.set	reorder
	.end	m32_static16_dl

	.align	2
	.set	mips16
	.ent	m16_static16_dl
m16_static16_dl:
	.frame	$sp,24,$31		# vars= 0, regs= 2/0, args= 16, gp= 0
	.mask	0x80010000,-4
	.fmask	0x00000000,0
	save	24,$16,$31
	.set	noreorder
	.set	nomacro
	jal	__mips16_fixdfsi
	move	$16,$6
	.set	macro
	.set	reorder

	addu	$2,$16
	restore	24,$16,$31
	j	$31
	.end	m16_static16_dl
	# Stub function for m16_static16_dl (double)
	.set	nomips16
	.section	.mips16.fn.m16_static16_dl,"ax",@progbits
	.align	2
	.ent	__fn_stub_m16_static16_dl
__fn_stub_m16_static16_dl:
	.set	noreorder
	mfc1	$4,$f13
	mfc1	$5,$f12
	.set	noat
	la	$1,m16_static16_dl
	jr	$1
	.set	at
	nop
	.set	reorder
	.end	__fn_stub_m16_static16_dl
	.previous

	.align	2
	.globl	m32_dlld
	.set	nomips16
	.ent	m32_dlld
m32_dlld:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, gp= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	.set	noreorder
	.set	nomacro
	
	trunc.w.d $f1,$f12
	mfc1	$4,$f1
	addu	$3,$4,$6
	addu	$2,$3,$7
	ldc1	$f0,16($sp)
	trunc.w.d $f2,$f0
	mfc1	$24,$f2
	j	$31
	addu	$2,$2,$24

	.set	macro
	.set	reorder
	.end	m32_dlld

	.align	2
	.globl	m16_dlld
	.set	mips16
	.ent	m16_dlld
m16_dlld:
	.frame	$sp,32,$31		# vars= 0, regs= 3/0, args= 16, gp= 0
	.mask	0x80030000,-4
	.fmask	0x00000000,0
	save	32,$16,$17,$31
	move	$16,$6
	.set	noreorder
	.set	nomacro
	jal	__mips16_fixdfsi
	move	$17,$7
	.set	macro
	.set	reorder

	lw	$5,52($sp)
	lw	$4,48($sp)
	addu	$16,$2,$16
	.set	noreorder
	.set	nomacro
	jal	__mips16_fixdfsi
	addu	$16,$17
	.set	macro
	.set	reorder

	addu	$2,$16,$2
	restore	32,$16,$17,$31
	j	$31
	.end	m16_dlld
	# Stub function for m16_dlld (double)
	.set	nomips16
	.section	.mips16.fn.m16_dlld,"ax",@progbits
	.align	2
	.ent	__fn_stub_m16_dlld
__fn_stub_m16_dlld:
	.set	noreorder
	mfc1	$4,$f13
	mfc1	$5,$f12
	.set	noat
	la	$1,m16_dlld
	jr	$1
	.set	at
	nop
	.set	reorder
	.end	__fn_stub_m16_dlld
	.previous

	.align	2
	.set	nomips16
	.ent	m32_static_dlld
m32_static_dlld:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, gp= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	.set	noreorder
	.set	nomacro
	
	trunc.w.d $f1,$f12
	mfc1	$4,$f1
	addu	$3,$4,$6
	addu	$2,$3,$7
	ldc1	$f0,16($sp)
	trunc.w.d $f2,$f0
	mfc1	$24,$f2
	j	$31
	addu	$2,$2,$24

	.set	macro
	.set	reorder
	.end	m32_static_dlld

	.align	2
	.set	mips16
	.ent	m16_static_dlld
m16_static_dlld:
	.frame	$sp,32,$31		# vars= 0, regs= 3/0, args= 16, gp= 0
	.mask	0x80030000,-4
	.fmask	0x00000000,0
	save	32,$16,$17,$31
	move	$16,$6
	.set	noreorder
	.set	nomacro
	jal	__mips16_fixdfsi
	move	$17,$7
	.set	macro
	.set	reorder

	lw	$5,52($sp)
	lw	$4,48($sp)
	addu	$16,$2,$16
	.set	noreorder
	.set	nomacro
	jal	__mips16_fixdfsi
	addu	$16,$17
	.set	macro
	.set	reorder

	addu	$2,$16,$2
	restore	32,$16,$17,$31
	j	$31
	.end	m16_static_dlld
	# Stub function for m16_static_dlld (double)
	.set	nomips16
	.section	.mips16.fn.m16_static_dlld,"ax",@progbits
	.align	2
	.ent	__fn_stub_m16_static_dlld
__fn_stub_m16_static_dlld:
	.set	noreorder
	mfc1	$4,$f13
	mfc1	$5,$f12
	.set	noat
	la	$1,m16_static_dlld
	jr	$1
	.set	at
	nop
	.set	reorder
	.end	__fn_stub_m16_static_dlld
	.previous

	.align	2
	.set	nomips16
	.ent	m32_static1_dlld
m32_static1_dlld:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, gp= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	.set	noreorder
	.set	nomacro
	
	trunc.w.d $f1,$f12
	mfc1	$4,$f1
	addu	$3,$4,$6
	addu	$2,$3,$7
	ldc1	$f0,16($sp)
	trunc.w.d $f2,$f0
	mfc1	$24,$f2
	j	$31
	addu	$2,$2,$24

	.set	macro
	.set	reorder
	.end	m32_static1_dlld

	.align	2
	.set	mips16
	.ent	m16_static1_dlld
m16_static1_dlld:
	.frame	$sp,32,$31		# vars= 0, regs= 3/0, args= 16, gp= 0
	.mask	0x80030000,-4
	.fmask	0x00000000,0
	save	32,$16,$17,$31
	move	$16,$6
	.set	noreorder
	.set	nomacro
	jal	__mips16_fixdfsi
	move	$17,$7
	.set	macro
	.set	reorder

	lw	$5,52($sp)
	lw	$4,48($sp)
	addu	$16,$2,$16
	.set	noreorder
	.set	nomacro
	jal	__mips16_fixdfsi
	addu	$16,$17
	.set	macro
	.set	reorder

	addu	$2,$16,$2
	restore	32,$16,$17,$31
	j	$31
	.end	m16_static1_dlld
	# Stub function for m16_static1_dlld (double)
	.set	nomips16
	.section	.mips16.fn.m16_static1_dlld,"ax",@progbits
	.align	2
	.ent	__fn_stub_m16_static1_dlld
__fn_stub_m16_static1_dlld:
	.set	noreorder
	mfc1	$4,$f13
	mfc1	$5,$f12
	.set	noat
	la	$1,m16_static1_dlld
	jr	$1
	.set	at
	nop
	.set	reorder
	.end	__fn_stub_m16_static1_dlld
	.previous

	.align	2
	.set	nomips16
	.ent	m32_static32_dlld
m32_static32_dlld:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, gp= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	.set	noreorder
	.set	nomacro
	
	trunc.w.d $f1,$f12
	mfc1	$4,$f1
	addu	$3,$4,$6
	addu	$2,$3,$7
	ldc1	$f0,16($sp)
	trunc.w.d $f2,$f0
	mfc1	$24,$f2
	j	$31
	addu	$2,$2,$24

	.set	macro
	.set	reorder
	.end	m32_static32_dlld

	.align	2
	.set	mips16
	.ent	m16_static32_dlld
m16_static32_dlld:
	.frame	$sp,32,$31		# vars= 0, regs= 3/0, args= 16, gp= 0
	.mask	0x80030000,-4
	.fmask	0x00000000,0
	save	32,$16,$17,$31
	move	$16,$6
	.set	noreorder
	.set	nomacro
	jal	__mips16_fixdfsi
	move	$17,$7
	.set	macro
	.set	reorder

	lw	$5,52($sp)
	lw	$4,48($sp)
	addu	$16,$2,$16
	.set	noreorder
	.set	nomacro
	jal	__mips16_fixdfsi
	addu	$16,$17
	.set	macro
	.set	reorder

	addu	$2,$16,$2
	restore	32,$16,$17,$31
	j	$31
	.end	m16_static32_dlld
	# Stub function for m16_static32_dlld (double)
	.set	nomips16
	.section	.mips16.fn.m16_static32_dlld,"ax",@progbits
	.align	2
	.ent	__fn_stub_m16_static32_dlld
__fn_stub_m16_static32_dlld:
	.set	noreorder
	mfc1	$4,$f13
	mfc1	$5,$f12
	.set	noat
	la	$1,m16_static32_dlld
	jr	$1
	.set	at
	nop
	.set	reorder
	.end	__fn_stub_m16_static32_dlld
	.previous

	.align	2
	.set	nomips16
	.ent	m32_static16_dlld
m32_static16_dlld:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, gp= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	.set	noreorder
	.set	nomacro
	
	trunc.w.d $f1,$f12
	mfc1	$4,$f1
	addu	$3,$4,$6
	addu	$2,$3,$7
	ldc1	$f0,16($sp)
	trunc.w.d $f2,$f0
	mfc1	$24,$f2
	j	$31
	addu	$2,$2,$24

	.set	macro
	.set	reorder
	.end	m32_static16_dlld

	.align	2
	.set	mips16
	.ent	m16_static16_dlld
m16_static16_dlld:
	.frame	$sp,32,$31		# vars= 0, regs= 3/0, args= 16, gp= 0
	.mask	0x80030000,-4
	.fmask	0x00000000,0
	save	32,$16,$17,$31
	move	$16,$6
	.set	noreorder
	.set	nomacro
	jal	__mips16_fixdfsi
	move	$17,$7
	.set	macro
	.set	reorder

	lw	$5,52($sp)
	lw	$4,48($sp)
	addu	$16,$2,$16
	.set	noreorder
	.set	nomacro
	jal	__mips16_fixdfsi
	addu	$16,$17
	.set	macro
	.set	reorder

	addu	$2,$16,$2
	restore	32,$16,$17,$31
	j	$31
	.end	m16_static16_dlld
	# Stub function for m16_static16_dlld (double)
	.set	nomips16
	.section	.mips16.fn.m16_static16_dlld,"ax",@progbits
	.align	2
	.ent	__fn_stub_m16_static16_dlld
__fn_stub_m16_static16_dlld:
	.set	noreorder
	mfc1	$4,$f13
	mfc1	$5,$f12
	.set	noat
	la	$1,m16_static16_dlld
	jr	$1
	.set	at
	nop
	.set	reorder
	.end	__fn_stub_m16_static16_dlld
	.previous

	.align	2
	.globl	m32_d_l
	.set	nomips16
	.ent	m32_d_l
m32_d_l:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, gp= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	.set	noreorder
	.set	nomacro
	
	mtc1	$4,$f2
	j	$31
	cvt.d.w	$f0,$f2

	.set	macro
	.set	reorder
	.end	m32_d_l

	.align	2
	.globl	m16_d_l
	.set	mips16
	.ent	m16_d_l
m16_d_l:
	.frame	$sp,24,$31		# vars= 0, regs= 1/0, args= 16, gp= 0
	.mask	0x80000000,-4
	.fmask	0x00000000,0
	save	24,$31
	jal	__mips16_floatsidf
	jal	__mips16_ret_df
	restore	24,$31
	j	$31
	.end	m16_d_l

	.align	2
	.set	nomips16
	.ent	m32_static_d_l
m32_static_d_l:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, gp= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	.set	noreorder
	.set	nomacro
	
	mtc1	$4,$f2
	j	$31
	cvt.d.w	$f0,$f2

	.set	macro
	.set	reorder
	.end	m32_static_d_l

	.align	2
	.set	mips16
	.ent	m16_static_d_l
m16_static_d_l:
	.frame	$sp,24,$31		# vars= 0, regs= 1/0, args= 16, gp= 0
	.mask	0x80000000,-4
	.fmask	0x00000000,0
	save	24,$31
	jal	__mips16_floatsidf
	jal	__mips16_ret_df
	restore	24,$31
	j	$31
	.end	m16_static_d_l

	.align	2
	.set	nomips16
	.ent	m32_static1_d_l
m32_static1_d_l:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, gp= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	.set	noreorder
	.set	nomacro
	
	mtc1	$4,$f2
	j	$31
	cvt.d.w	$f0,$f2

	.set	macro
	.set	reorder
	.end	m32_static1_d_l

	.align	2
	.set	mips16
	.ent	m16_static1_d_l
m16_static1_d_l:
	.frame	$sp,24,$31		# vars= 0, regs= 1/0, args= 16, gp= 0
	.mask	0x80000000,-4
	.fmask	0x00000000,0
	save	24,$31
	jal	__mips16_floatsidf
	jal	__mips16_ret_df
	restore	24,$31
	j	$31
	.end	m16_static1_d_l

	.align	2
	.set	nomips16
	.ent	m32_static32_d_l
m32_static32_d_l:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, gp= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	.set	noreorder
	.set	nomacro
	
	mtc1	$4,$f2
	j	$31
	cvt.d.w	$f0,$f2

	.set	macro
	.set	reorder
	.end	m32_static32_d_l

	.align	2
	.set	mips16
	.ent	m16_static32_d_l
m16_static32_d_l:
	.frame	$sp,24,$31		# vars= 0, regs= 1/0, args= 16, gp= 0
	.mask	0x80000000,-4
	.fmask	0x00000000,0
	save	24,$31
	jal	__mips16_floatsidf
	jal	__mips16_ret_df
	restore	24,$31
	j	$31
	.end	m16_static32_d_l

	.align	2
	.set	nomips16
	.ent	m32_static16_d_l
m32_static16_d_l:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, gp= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	.set	noreorder
	.set	nomacro
	
	mtc1	$4,$f2
	j	$31
	cvt.d.w	$f0,$f2

	.set	macro
	.set	reorder
	.end	m32_static16_d_l

	.align	2
	.set	mips16
	.ent	m16_static16_d_l
m16_static16_d_l:
	.frame	$sp,24,$31		# vars= 0, regs= 1/0, args= 16, gp= 0
	.mask	0x80000000,-4
	.fmask	0x00000000,0
	save	24,$31
	jal	__mips16_floatsidf
	jal	__mips16_ret_df
	restore	24,$31
	j	$31
	.end	m16_static16_d_l

	.align	2
	.globl	m32_d_d
	.set	nomips16
	.ent	m32_d_d
m32_d_d:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, gp= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	.set	noreorder
	.set	nomacro
	
	j	$31
	mov.d	$f0,$f12

	.set	macro
	.set	reorder
	.end	m32_d_d

	.align	2
	.globl	m16_d_d
	.set	mips16
	.ent	m16_d_d
m16_d_d:
	.frame	$sp,8,$31		# vars= 0, regs= 1/0, args= 0, gp= 0
	.mask	0x80000000,-4
	.fmask	0x00000000,0
	save	8,$31
	move	$3,$5
	.set	noreorder
	.set	nomacro
	jal	__mips16_ret_df
	move	$2,$4
	.set	macro
	.set	reorder

	restore	8,$31
	j	$31
	.end	m16_d_d
	# Stub function for m16_d_d (double)
	.set	nomips16
	.section	.mips16.fn.m16_d_d,"ax",@progbits
	.align	2
	.ent	__fn_stub_m16_d_d
__fn_stub_m16_d_d:
	.set	noreorder
	mfc1	$4,$f13
	mfc1	$5,$f12
	.set	noat
	la	$1,m16_d_d
	jr	$1
	.set	at
	nop
	.set	reorder
	.end	__fn_stub_m16_d_d
	.previous

	.align	2
	.set	nomips16
	.ent	m32_static_d_d
m32_static_d_d:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, gp= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	.set	noreorder
	.set	nomacro
	
	j	$31
	mov.d	$f0,$f12

	.set	macro
	.set	reorder
	.end	m32_static_d_d

	.align	2
	.set	mips16
	.ent	m16_static_d_d
m16_static_d_d:
	.frame	$sp,8,$31		# vars= 0, regs= 1/0, args= 0, gp= 0
	.mask	0x80000000,-4
	.fmask	0x00000000,0
	save	8,$31
	move	$3,$5
	.set	noreorder
	.set	nomacro
	jal	__mips16_ret_df
	move	$2,$4
	.set	macro
	.set	reorder

	restore	8,$31
	j	$31
	.end	m16_static_d_d
	# Stub function for m16_static_d_d (double)
	.set	nomips16
	.section	.mips16.fn.m16_static_d_d,"ax",@progbits
	.align	2
	.ent	__fn_stub_m16_static_d_d
__fn_stub_m16_static_d_d:
	.set	noreorder
	mfc1	$4,$f13
	mfc1	$5,$f12
	.set	noat
	la	$1,m16_static_d_d
	jr	$1
	.set	at
	nop
	.set	reorder
	.end	__fn_stub_m16_static_d_d
	.previous

	.align	2
	.set	nomips16
	.ent	m32_static1_d_d
m32_static1_d_d:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, gp= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	.set	noreorder
	.set	nomacro
	
	j	$31
	mov.d	$f0,$f12

	.set	macro
	.set	reorder
	.end	m32_static1_d_d

	.align	2
	.set	mips16
	.ent	m16_static1_d_d
m16_static1_d_d:
	.frame	$sp,8,$31		# vars= 0, regs= 1/0, args= 0, gp= 0
	.mask	0x80000000,-4
	.fmask	0x00000000,0
	save	8,$31
	move	$3,$5
	.set	noreorder
	.set	nomacro
	jal	__mips16_ret_df
	move	$2,$4
	.set	macro
	.set	reorder

	restore	8,$31
	j	$31
	.end	m16_static1_d_d
	# Stub function for m16_static1_d_d (double)
	.set	nomips16
	.section	.mips16.fn.m16_static1_d_d,"ax",@progbits
	.align	2
	.ent	__fn_stub_m16_static1_d_d
__fn_stub_m16_static1_d_d:
	.set	noreorder
	mfc1	$4,$f13
	mfc1	$5,$f12
	.set	noat
	la	$1,m16_static1_d_d
	jr	$1
	.set	at
	nop
	.set	reorder
	.end	__fn_stub_m16_static1_d_d
	.previous

	.align	2
	.set	nomips16
	.ent	m32_static32_d_d
m32_static32_d_d:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, gp= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	.set	noreorder
	.set	nomacro
	
	j	$31
	mov.d	$f0,$f12

	.set	macro
	.set	reorder
	.end	m32_static32_d_d

	.align	2
	.set	mips16
	.ent	m16_static32_d_d
m16_static32_d_d:
	.frame	$sp,8,$31		# vars= 0, regs= 1/0, args= 0, gp= 0
	.mask	0x80000000,-4
	.fmask	0x00000000,0
	save	8,$31
	move	$3,$5
	.set	noreorder
	.set	nomacro
	jal	__mips16_ret_df
	move	$2,$4
	.set	macro
	.set	reorder

	restore	8,$31
	j	$31
	.end	m16_static32_d_d
	# Stub function for m16_static32_d_d (double)
	.set	nomips16
	.section	.mips16.fn.m16_static32_d_d,"ax",@progbits
	.align	2
	.ent	__fn_stub_m16_static32_d_d
__fn_stub_m16_static32_d_d:
	.set	noreorder
	mfc1	$4,$f13
	mfc1	$5,$f12
	.set	noat
	la	$1,m16_static32_d_d
	jr	$1
	.set	at
	nop
	.set	reorder
	.end	__fn_stub_m16_static32_d_d
	.previous

	.align	2
	.set	nomips16
	.ent	m32_static16_d_d
m32_static16_d_d:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, gp= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	.set	noreorder
	.set	nomacro
	
	j	$31
	mov.d	$f0,$f12

	.set	macro
	.set	reorder
	.end	m32_static16_d_d

	.align	2
	.set	mips16
	.ent	m16_static16_d_d
m16_static16_d_d:
	.frame	$sp,8,$31		# vars= 0, regs= 1/0, args= 0, gp= 0
	.mask	0x80000000,-4
	.fmask	0x00000000,0
	save	8,$31
	move	$3,$5
	.set	noreorder
	.set	nomacro
	jal	__mips16_ret_df
	move	$2,$4
	.set	macro
	.set	reorder

	restore	8,$31
	j	$31
	.end	m16_static16_d_d
	# Stub function for m16_static16_d_d (double)
	.set	nomips16
	.section	.mips16.fn.m16_static16_d_d,"ax",@progbits
	.align	2
	.ent	__fn_stub_m16_static16_d_d
__fn_stub_m16_static16_d_d:
	.set	noreorder
	mfc1	$4,$f13
	mfc1	$5,$f12
	.set	noat
	la	$1,m16_static16_d_d
	jr	$1
	.set	at
	nop
	.set	reorder
	.end	__fn_stub_m16_static16_d_d
	.previous

	.align	2
	.globl	f32
	.set	nomips16
	.ent	f32
f32:
	.frame	$sp,64,$31		# vars= 0, regs= 3/3, args= 24, gp= 0
	.mask	0x80030000,-32
	.fmask	0x03f00000,-8
	.set	noreorder
	.set	nomacro
	
	addiu	$sp,$sp,-64
	sw	$17,28($sp)
	move	$17,$4
	sw	$31,32($sp)
	sdc1	$f24,56($sp)
	sw	$16,24($sp)
	sdc1	$f22,48($sp)
	sdc1	$f20,40($sp)
	mtc1	$7,$f22
	jal	m32_static1_l
	mtc1	$6,$f23

	move	$4,$17
	jal	m16_static1_l
	move	$16,$2

	addu	$16,$16,$2
	jal	m32_static1_d
	mov.d	$f12,$f22

	addu	$16,$16,$2
	jal	m16_static1_d
	mov.d	$f12,$f22

	move	$4,$17
	mfc1	$7,$f22
	mfc1	$6,$f23
	jal	m32_static1_ld
	addu	$16,$16,$2

	move	$4,$17
	mfc1	$7,$f22
	mfc1	$6,$f23
	jal	m16_static1_ld
	addu	$16,$16,$2

	move	$6,$17
	mov.d	$f12,$f22
	jal	m32_static1_dl
	addu	$16,$16,$2

	move	$6,$17
	mov.d	$f12,$f22
	jal	m16_static1_dl
	addu	$16,$16,$2

	move	$6,$17
	move	$7,$17
	sdc1	$f22,16($sp)
	mov.d	$f12,$f22
	jal	m32_static1_dlld
	addu	$16,$16,$2

	move	$6,$17
	move	$7,$17
	mov.d	$f12,$f22
	sdc1	$f22,16($sp)
	jal	m16_static1_dlld
	addu	$16,$16,$2

	move	$4,$17
	jal	m32_static1_d_l
	addu	$16,$16,$2

	move	$4,$17
	jal	m16_static1_d_l
	mov.d	$f20,$f0

	add.d	$f20,$f20,$f0
	jal	m32_static1_d_d
	mov.d	$f12,$f22

	add.d	$f20,$f20,$f0
	jal	m16_static1_d_d
	mov.d	$f12,$f22

	move	$4,$17
	jal	m32_static32_l
	add.d	$f20,$f20,$f0

	move	$4,$17
	jal	m16_static32_l
	addu	$16,$16,$2

	addu	$16,$16,$2
	jal	m32_static32_d
	mov.d	$f12,$f22

	addu	$16,$16,$2
	jal	m16_static32_d
	mov.d	$f12,$f22

	move	$4,$17
	mfc1	$7,$f22
	mfc1	$6,$f23
	jal	m32_static32_ld
	addu	$16,$16,$2

	move	$4,$17
	mfc1	$7,$f22
	mfc1	$6,$f23
	jal	m16_static32_ld
	addu	$16,$16,$2

	move	$6,$17
	mov.d	$f12,$f22
	jal	m32_static32_dl
	addu	$16,$16,$2

	move	$6,$17
	mov.d	$f12,$f22
	jal	m16_static32_dl
	addu	$16,$16,$2

	move	$6,$17
	move	$7,$17
	sdc1	$f22,16($sp)
	mov.d	$f12,$f22
	jal	m32_static32_dlld
	addu	$16,$16,$2

	move	$6,$17
	move	$7,$17
	mov.d	$f12,$f22
	sdc1	$f22,16($sp)
	jal	m16_static32_dlld
	addu	$16,$16,$2

	move	$4,$17
	jal	m32_static32_d_l
	addu	$16,$16,$2

	move	$4,$17
	jal	m16_static32_d_l
	add.d	$f20,$f20,$f0

	add.d	$f20,$f20,$f0
	jal	m32_static32_d_d
	mov.d	$f12,$f22

	mtc1	$16,$f24
	add.d	$f20,$f20,$f0
	jal	m16_static32_d_d
	mov.d	$f12,$f22

	lw	$31,32($sp)
	lw	$17,28($sp)
	lw	$16,24($sp)
	add.d	$f20,$f20,$f0
	ldc1	$f22,48($sp)
	cvt.d.w	$f0,$f24
	ldc1	$f24,56($sp)
	add.d	$f0,$f0,$f20
	ldc1	$f20,40($sp)
	j	$31
	addiu	$sp,$sp,64

	.set	macro
	.set	reorder
	.end	f32

	# Stub function to call m32_static1_d (double)
	.set	nomips16
	.section	.mips16.call.m32_static1_d,"ax",@progbits
	.align	2
	.ent	__call_stub_m32_static1_d
__call_stub_m32_static1_d:
	.set	noreorder
	mtc1	$4,$f13
	mtc1	$5,$f12
	.set	noat
	la	$1,m32_static1_d
	jr	$1
	.set	at
	nop
	.set	reorder
	.end	__call_stub_m32_static1_d
	.previous

	# Stub function to call m16_static1_d (double)
	.set	nomips16
	.section	.mips16.call.m16_static1_d,"ax",@progbits
	.align	2
	.ent	__call_stub_m16_static1_d
__call_stub_m16_static1_d:
	.set	noreorder
	mtc1	$4,$f13
	mtc1	$5,$f12
	.set	noat
	la	$1,m16_static1_d
	jr	$1
	.set	at
	nop
	.set	reorder
	.end	__call_stub_m16_static1_d
	.previous

	# Stub function to call m32_static1_dl (double)
	.set	nomips16
	.section	.mips16.call.m32_static1_dl,"ax",@progbits
	.align	2
	.ent	__call_stub_m32_static1_dl
__call_stub_m32_static1_dl:
	.set	noreorder
	mtc1	$4,$f13
	mtc1	$5,$f12
	.set	noat
	la	$1,m32_static1_dl
	jr	$1
	.set	at
	nop
	.set	reorder
	.end	__call_stub_m32_static1_dl
	.previous

	# Stub function to call m16_static1_dl (double)
	.set	nomips16
	.section	.mips16.call.m16_static1_dl,"ax",@progbits
	.align	2
	.ent	__call_stub_m16_static1_dl
__call_stub_m16_static1_dl:
	.set	noreorder
	mtc1	$4,$f13
	mtc1	$5,$f12
	.set	noat
	la	$1,m16_static1_dl
	jr	$1
	.set	at
	nop
	.set	reorder
	.end	__call_stub_m16_static1_dl
	.previous

	# Stub function to call m32_static1_dlld (double)
	.set	nomips16
	.section	.mips16.call.m32_static1_dlld,"ax",@progbits
	.align	2
	.ent	__call_stub_m32_static1_dlld
__call_stub_m32_static1_dlld:
	.set	noreorder
	mtc1	$4,$f13
	mtc1	$5,$f12
	.set	noat
	la	$1,m32_static1_dlld
	jr	$1
	.set	at
	nop
	.set	reorder
	.end	__call_stub_m32_static1_dlld
	.previous

	# Stub function to call m16_static1_dlld (double)
	.set	nomips16
	.section	.mips16.call.m16_static1_dlld,"ax",@progbits
	.align	2
	.ent	__call_stub_m16_static1_dlld
__call_stub_m16_static1_dlld:
	.set	noreorder
	mtc1	$4,$f13
	mtc1	$5,$f12
	.set	noat
	la	$1,m16_static1_dlld
	jr	$1
	.set	at
	nop
	.set	reorder
	.end	__call_stub_m16_static1_dlld
	.previous

	# Stub function to call double m32_static1_d_l ()
	.set	nomips16
	.section	.mips16.call.fp.m32_static1_d_l,"ax",@progbits
	.align	2
	.ent	__call_stub_fp_m32_static1_d_l
__call_stub_fp_m32_static1_d_l:
	.set	noreorder
	move	$18,$31
	jal	m32_static1_d_l
	nop
	mfc1	$2,$f1
	mfc1	$3,$f0
	j	$18
	nop
	.set	reorder
	.end	__call_stub_fp_m32_static1_d_l
	.previous

	# Stub function to call double m16_static1_d_l ()
	.set	nomips16
	.section	.mips16.call.fp.m16_static1_d_l,"ax",@progbits
	.align	2
	.ent	__call_stub_fp_m16_static1_d_l
__call_stub_fp_m16_static1_d_l:
	.set	noreorder
	move	$18,$31
	jal	m16_static1_d_l
	nop
	mfc1	$2,$f1
	mfc1	$3,$f0
	j	$18
	nop
	.set	reorder
	.end	__call_stub_fp_m16_static1_d_l
	.previous

	# Stub function to call double m32_static1_d_d (double)
	.set	nomips16
	.section	.mips16.call.fp.m32_static1_d_d,"ax",@progbits
	.align	2
	.ent	__call_stub_fp_m32_static1_d_d
__call_stub_fp_m32_static1_d_d:
	.set	noreorder
	mtc1	$4,$f13
	mtc1	$5,$f12
	move	$18,$31
	jal	m32_static1_d_d
	nop
	mfc1	$2,$f1
	mfc1	$3,$f0
	j	$18
	nop
	.set	reorder
	.end	__call_stub_fp_m32_static1_d_d
	.previous

	# Stub function to call double m16_static1_d_d (double)
	.set	nomips16
	.section	.mips16.call.fp.m16_static1_d_d,"ax",@progbits
	.align	2
	.ent	__call_stub_fp_m16_static1_d_d
__call_stub_fp_m16_static1_d_d:
	.set	noreorder
	mtc1	$4,$f13
	mtc1	$5,$f12
	move	$18,$31
	jal	m16_static1_d_d
	nop
	mfc1	$2,$f1
	mfc1	$3,$f0
	j	$18
	nop
	.set	reorder
	.end	__call_stub_fp_m16_static1_d_d
	.previous

	# Stub function to call m32_static16_d (double)
	.set	nomips16
	.section	.mips16.call.m32_static16_d,"ax",@progbits
	.align	2
	.ent	__call_stub_m32_static16_d
__call_stub_m32_static16_d:
	.set	noreorder
	mtc1	$4,$f13
	mtc1	$5,$f12
	.set	noat
	la	$1,m32_static16_d
	jr	$1
	.set	at
	nop
	.set	reorder
	.end	__call_stub_m32_static16_d
	.previous

	# Stub function to call m16_static16_d (double)
	.set	nomips16
	.section	.mips16.call.m16_static16_d,"ax",@progbits
	.align	2
	.ent	__call_stub_m16_static16_d
__call_stub_m16_static16_d:
	.set	noreorder
	mtc1	$4,$f13
	mtc1	$5,$f12
	.set	noat
	la	$1,m16_static16_d
	jr	$1
	.set	at
	nop
	.set	reorder
	.end	__call_stub_m16_static16_d
	.previous

	# Stub function to call m32_static16_dl (double)
	.set	nomips16
	.section	.mips16.call.m32_static16_dl,"ax",@progbits
	.align	2
	.ent	__call_stub_m32_static16_dl
__call_stub_m32_static16_dl:
	.set	noreorder
	mtc1	$4,$f13
	mtc1	$5,$f12
	.set	noat
	la	$1,m32_static16_dl
	jr	$1
	.set	at
	nop
	.set	reorder
	.end	__call_stub_m32_static16_dl
	.previous

	# Stub function to call m16_static16_dl (double)
	.set	nomips16
	.section	.mips16.call.m16_static16_dl,"ax",@progbits
	.align	2
	.ent	__call_stub_m16_static16_dl
__call_stub_m16_static16_dl:
	.set	noreorder
	mtc1	$4,$f13
	mtc1	$5,$f12
	.set	noat
	la	$1,m16_static16_dl
	jr	$1
	.set	at
	nop
	.set	reorder
	.end	__call_stub_m16_static16_dl
	.previous

	# Stub function to call m32_static16_dlld (double)
	.set	nomips16
	.section	.mips16.call.m32_static16_dlld,"ax",@progbits
	.align	2
	.ent	__call_stub_m32_static16_dlld
__call_stub_m32_static16_dlld:
	.set	noreorder
	mtc1	$4,$f13
	mtc1	$5,$f12
	.set	noat
	la	$1,m32_static16_dlld
	jr	$1
	.set	at
	nop
	.set	reorder
	.end	__call_stub_m32_static16_dlld
	.previous

	# Stub function to call m16_static16_dlld (double)
	.set	nomips16
	.section	.mips16.call.m16_static16_dlld,"ax",@progbits
	.align	2
	.ent	__call_stub_m16_static16_dlld
__call_stub_m16_static16_dlld:
	.set	noreorder
	mtc1	$4,$f13
	mtc1	$5,$f12
	.set	noat
	la	$1,m16_static16_dlld
	jr	$1
	.set	at
	nop
	.set	reorder
	.end	__call_stub_m16_static16_dlld
	.previous

	# Stub function to call double m32_static16_d_l ()
	.set	nomips16
	.section	.mips16.call.fp.m32_static16_d_l,"ax",@progbits
	.align	2
	.ent	__call_stub_fp_m32_static16_d_l
__call_stub_fp_m32_static16_d_l:
	.set	noreorder
	move	$18,$31
	jal	m32_static16_d_l
	nop
	mfc1	$2,$f1
	mfc1	$3,$f0
	j	$18
	nop
	.set	reorder
	.end	__call_stub_fp_m32_static16_d_l
	.previous

	# Stub function to call double m16_static16_d_l ()
	.set	nomips16
	.section	.mips16.call.fp.m16_static16_d_l,"ax",@progbits
	.align	2
	.ent	__call_stub_fp_m16_static16_d_l
__call_stub_fp_m16_static16_d_l:
	.set	noreorder
	move	$18,$31
	jal	m16_static16_d_l
	nop
	mfc1	$2,$f1
	mfc1	$3,$f0
	j	$18
	nop
	.set	reorder
	.end	__call_stub_fp_m16_static16_d_l
	.previous

	# Stub function to call double m32_static16_d_d (double)
	.set	nomips16
	.section	.mips16.call.fp.m32_static16_d_d,"ax",@progbits
	.align	2
	.ent	__call_stub_fp_m32_static16_d_d
__call_stub_fp_m32_static16_d_d:
	.set	noreorder
	mtc1	$4,$f13
	mtc1	$5,$f12
	move	$18,$31
	jal	m32_static16_d_d
	nop
	mfc1	$2,$f1
	mfc1	$3,$f0
	j	$18
	nop
	.set	reorder
	.end	__call_stub_fp_m32_static16_d_d
	.previous

	# Stub function to call double m16_static16_d_d (double)
	.set	nomips16
	.section	.mips16.call.fp.m16_static16_d_d,"ax",@progbits
	.align	2
	.ent	__call_stub_fp_m16_static16_d_d
__call_stub_fp_m16_static16_d_d:
	.set	noreorder
	mtc1	$4,$f13
	mtc1	$5,$f12
	move	$18,$31
	jal	m16_static16_d_d
	nop
	mfc1	$2,$f1
	mfc1	$3,$f0
	j	$18
	nop
	.set	reorder
	.end	__call_stub_fp_m16_static16_d_d
	.previous

	.align	2
	.globl	f16
	.set	mips16
	.ent	f16
f16:
	.frame	$sp,104,$31		# vars= 64, regs= 4/0, args= 24, gp= 0
	.mask	0x80070000,-4
	.fmask	0x00000000,0
	save	104,$16,$17,$18,$31
	move	$17,$4
	sw	$7,116($sp)
	.set	noreorder
	.set	nomacro
	jal	m32_static1_l
	sw	$6,112($sp)
	.set	macro
	.set	reorder

	move	$4,$17
	.set	noreorder
	.set	nomacro
	jal	m16_static1_l
	move	$16,$2
	.set	macro
	.set	reorder

	lw	$5,116($sp)
	lw	$4,112($sp)
	.set	noreorder
	.set	nomacro
	jal	m32_static1_d
	addu	$16,$2
	.set	macro
	.set	reorder

	lw	$5,116($sp)
	lw	$4,112($sp)
	.set	noreorder
	.set	nomacro
	jal	m16_static1_d
	addu	$16,$2
	.set	macro
	.set	reorder

	lw	$7,116($sp)
	lw	$6,112($sp)
	move	$4,$17
	.set	noreorder
	.set	nomacro
	jal	m32_static1_ld
	addu	$16,$2
	.set	macro
	.set	reorder

	lw	$7,116($sp)
	lw	$6,112($sp)
	move	$4,$17
	.set	noreorder
	.set	nomacro
	jal	m16_static1_ld
	addu	$16,$2
	.set	macro
	.set	reorder

	lw	$5,116($sp)
	lw	$4,112($sp)
	move	$6,$17
	.set	noreorder
	.set	nomacro
	jal	m32_static1_dl
	addu	$16,$2
	.set	macro
	.set	reorder

	lw	$5,116($sp)
	lw	$4,112($sp)
	move	$6,$17
	.set	noreorder
	.set	nomacro
	jal	m16_static1_dl
	addu	$16,$2
	.set	macro
	.set	reorder

	lw	$3,116($sp)
	lw	$6,112($sp)
	sw	$3,20($sp)
	move	$5,$3
	sw	$6,16($sp)
	move	$4,$6
	move	$7,$17
	move	$6,$17
	.set	noreorder
	.set	nomacro
	jal	m32_static1_dlld
	addu	$16,$2
	.set	macro
	.set	reorder

	addu	$16,$2
	lw	$7,112($sp)
	lw	$2,116($sp)
	move	$6,$17
	move	$5,$2
	sw	$7,16($sp)
	move	$4,$7
	sw	$2,20($sp)
	.set	noreorder
	.set	nomacro
	jal	m16_static1_dlld
	move	$7,$17
	.set	macro
	.set	reorder

	move	$4,$17
	.set	noreorder
	.set	nomacro
	jal	m32_static1_d_l
	addu	$16,$2
	.set	macro
	.set	reorder

	move	$4,$17
	sw	$3,28($sp)
	.set	noreorder
	.set	nomacro
	jal	m16_static1_d_l
	sw	$2,24($sp)
	.set	macro
	.set	reorder

	lw	$5,28($sp)
	lw	$4,24($sp)
	move	$7,$3
	.set	noreorder
	.set	nomacro
	jal	__mips16_adddf3
	move	$6,$2
	.set	macro
	.set	reorder

	lw	$5,116($sp)
	lw	$4,112($sp)
	sw	$3,36($sp)
	.set	noreorder
	.set	nomacro
	jal	m32_static1_d_d
	sw	$2,32($sp)
	.set	macro
	.set	reorder

	lw	$5,36($sp)
	lw	$4,32($sp)
	move	$7,$3
	.set	noreorder
	.set	nomacro
	jal	__mips16_adddf3
	move	$6,$2
	.set	macro
	.set	reorder

	lw	$5,116($sp)
	lw	$4,112($sp)
	sw	$3,44($sp)
	.set	noreorder
	.set	nomacro
	jal	m16_static1_d_d
	sw	$2,40($sp)
	.set	macro
	.set	reorder

	lw	$5,44($sp)
	lw	$4,40($sp)
	move	$7,$3
	.set	noreorder
	.set	nomacro
	jal	__mips16_adddf3
	move	$6,$2
	.set	macro
	.set	reorder

	move	$4,$17
	sw	$3,52($sp)
	.set	noreorder
	.set	nomacro
	jal	m32_static16_l
	sw	$2,48($sp)
	.set	macro
	.set	reorder

	move	$4,$17
	.set	noreorder
	.set	nomacro
	jal	m16_static16_l
	addu	$16,$2
	.set	macro
	.set	reorder

	lw	$5,116($sp)
	lw	$4,112($sp)
	.set	noreorder
	.set	nomacro
	jal	m32_static16_d
	addu	$16,$2
	.set	macro
	.set	reorder

	lw	$5,116($sp)
	lw	$4,112($sp)
	.set	noreorder
	.set	nomacro
	jal	m16_static16_d
	addu	$16,$2
	.set	macro
	.set	reorder

	lw	$7,116($sp)
	lw	$6,112($sp)
	move	$4,$17
	.set	noreorder
	.set	nomacro
	jal	m32_static16_ld
	addu	$16,$2
	.set	macro
	.set	reorder

	lw	$7,116($sp)
	lw	$6,112($sp)
	move	$4,$17
	.set	noreorder
	.set	nomacro
	jal	m16_static16_ld
	addu	$16,$2
	.set	macro
	.set	reorder

	lw	$5,116($sp)
	lw	$4,112($sp)
	move	$6,$17
	.set	noreorder
	.set	nomacro
	jal	m32_static16_dl
	addu	$16,$2
	.set	macro
	.set	reorder

	lw	$5,116($sp)
	lw	$4,112($sp)
	move	$6,$17
	.set	noreorder
	.set	nomacro
	jal	m16_static16_dl
	addu	$16,$2
	.set	macro
	.set	reorder

	lw	$4,116($sp)
	lw	$6,112($sp)
	sw	$4,20($sp)
	sw	$6,16($sp)
	move	$5,$4
	move	$7,$17
	move	$4,$6
	move	$6,$17
	.set	noreorder
	.set	nomacro
	jal	m32_static16_dlld
	addu	$16,$2
	.set	macro
	.set	reorder

	addu	$16,$2
	lw	$3,116($sp)
	lw	$2,112($sp)
	move	$6,$17
	move	$7,$17
	sw	$3,20($sp)
	move	$5,$3
	sw	$2,16($sp)
	.set	noreorder
	.set	nomacro
	jal	m16_static16_dlld
	move	$4,$2
	.set	macro
	.set	reorder

	move	$4,$17
	.set	noreorder
	.set	nomacro
	jal	m32_static16_d_l
	addu	$16,$2
	.set	macro
	.set	reorder

	lw	$5,52($sp)
	lw	$4,48($sp)
	move	$7,$3
	.set	noreorder
	.set	nomacro
	jal	__mips16_adddf3
	move	$6,$2
	.set	macro
	.set	reorder

	move	$4,$17
	sw	$3,60($sp)
	.set	noreorder
	.set	nomacro
	jal	m16_static16_d_l
	sw	$2,56($sp)
	.set	macro
	.set	reorder

	lw	$5,60($sp)
	lw	$4,56($sp)
	move	$7,$3
	.set	noreorder
	.set	nomacro
	jal	__mips16_adddf3
	move	$6,$2
	.set	macro
	.set	reorder

	lw	$5,116($sp)
	lw	$4,112($sp)
	sw	$3,68($sp)
	.set	noreorder
	.set	nomacro
	jal	m32_static16_d_d
	sw	$2,64($sp)
	.set	macro
	.set	reorder

	lw	$5,68($sp)
	lw	$4,64($sp)
	move	$7,$3
	.set	noreorder
	.set	nomacro
	jal	__mips16_adddf3
	move	$6,$2
	.set	macro
	.set	reorder

	lw	$5,116($sp)
	lw	$4,112($sp)
	sw	$3,76($sp)
	.set	noreorder
	.set	nomacro
	jal	m16_static16_d_d
	sw	$2,72($sp)
	.set	macro
	.set	reorder

	lw	$5,76($sp)
	lw	$4,72($sp)
	move	$7,$3
	.set	noreorder
	.set	nomacro
	jal	__mips16_adddf3
	move	$6,$2
	.set	macro
	.set	reorder

	move	$4,$16
	sw	$3,84($sp)
	.set	noreorder
	.set	nomacro
	jal	__mips16_floatsidf
	sw	$2,80($sp)
	.set	macro
	.set	reorder

	lw	$7,84($sp)
	lw	$6,80($sp)
	move	$5,$3
	.set	noreorder
	.set	nomacro
	jal	__mips16_adddf3
	move	$4,$2
	.set	macro
	.set	reorder

	jal	__mips16_ret_df
	restore	104,$16,$17,$18,$31
	j	$31
	.end	f16
