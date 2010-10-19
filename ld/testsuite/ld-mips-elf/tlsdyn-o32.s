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
	addiu	$4,$28,%tlsgd(tlsbin_gd)
	jal	$25

	lw	$25,%call16(__tls_get_addr)($28)
	addiu	$4,$28,%tlsgd(tlsvar_gd)
	jal	$25

	# Local Dynamic
	lw	$25,%call16(__tls_get_addr)($28)
	addiu	$4,$28,%tlsldm(tlsbin_ld)
	jal	$25

	move	$2,$2		# Arbitrary instructions

	lui	$3,%dtprel_hi(tlsbin_ld)
	addiu	$3,$3,%dtprel_lo(tlsbin_ld)
	addu	$3,$3,$2

	# Initial Exec
	.set	push
	.set	mips32r2
	rdhwr	$2, $5
	.set	pop
	lw	$3,%gottprel(tlsbin_ie)($28)
	addu	$3,$3,$2

	lw	$3,%gottprel(tlsvar_ie)($28)
	addu	$3,$3,$2

	# Local Exec
	.set	push
	.set	mips32r2
	rdhwr	$2, $5
	.set	pop
	lui	$3,%tprel_hi(tlsbin_le)
	addiu	$3,$3,%tprel_lo(tlsbin_le)
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
	.global	tlsbin_gd
	.type	tlsbin_gd,@object
	.size	tlsbin_gd,4
tlsbin_gd:
	.space	4
	.global	tlsbin_ie
	.type	tlsbin_ie,@object
	.size	tlsbin_ie,4
tlsbin_ie:
	.space	4

	.section		.tdata,"awT"
	.align	2
	.global	tlsbin_ld
	.hidden	tlsbin_ld
	.type	tlsbin_ld,@object
	.size	tlsbin_ld,4
tlsbin_ld:
	.word	1
	.global	tlsbin_le
	.hidden	tlsbin_le
	.type	tlsbin_le,@object
	.size	tlsbin_le,4
tlsbin_le:
	.word	1
