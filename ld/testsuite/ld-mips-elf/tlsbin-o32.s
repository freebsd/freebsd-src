	.file	1 "tlsbin-o32.s"
	.abicalls
	.text
	.align	2
	.globl	__start
	.ent	__start
	.type	__start,@function
__start:
	.frame	$fp,16,$31
	.mask	0x40000000,-8
	.fmask	0x00000000,0
	.set	noreorder
	.cpload $25
	.set	reorder
	addiu	$sp,$sp,-16
	sw	$fp,8($sp)
	move	$fp,$sp
	.cprestore	0

	# General Dynamic
	lw	$25,%call16(__tls_get_addr)($28)
	addiu	$4,$28,%tlsgd(tlsvar_gd)
	jal	$25

	# Local Dynamic
	lw	$25,%call16(__tls_get_addr)($28)
	addiu	$4,$28,%tlsldm(tlsvar_ld)
	jal	$25

	move	$2,$2		# Arbitrary instructions

	lui	$3,%dtprel_hi(tlsvar_ld)
	addiu	$3,$3,%dtprel_lo(tlsvar_ld)
	addu	$3,$3,$2

	# Initial Exec
	.set	push
	.set	mips32r2
	rdhwr	$2, $5
	.set	pop
	lw	$3,%gottprel(tlsvar_ie)($28)
	addu	$3,$3,$2

	# Local Exec
	.set	push
	.set	mips32r2
	rdhwr	$2, $5
	.set	pop
	lui	$3,%tprel_hi(tlsvar_le)
	addiu	$3,$3,%tprel_lo(tlsvar_le)
	addu	$3,$3,$2

	move	$sp,$fp
	lw	$fp,8($sp)
	addiu	$sp,$sp,16
	j	$31
	.end	__start

	.globl __tls_get_addr
__tls_get_addr:
	j $31

	.section		.tbss,"awT",@nobits
	.align	2
	.global	tlsvar_gd
	.type	tlsvar_gd,@object
	.size	tlsvar_gd,4
tlsvar_gd:
	.space	4
	.global	tlsvar_ie
	.type	tlsvar_ie,@object
	.size	tlsvar_ie,4
tlsvar_ie:
	.space	4

	.section		.tdata,"awT"
	.align	2
	.global	tlsvar_ld
	.hidden	tlsvar_ld
	.type	tlsvar_ld,@object
	.size	tlsvar_ld,4
tlsvar_ld:
	.word	1
	.global	tlsvar_le
	.hidden	tlsvar_le
	.type	tlsvar_le,@object
	.size	tlsvar_le,4
tlsvar_le:
	.word	1
