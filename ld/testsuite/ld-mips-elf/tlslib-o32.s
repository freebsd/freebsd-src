	.file	1 "tlslib-o32.s"
	.abicalls
	.text
	.align	2
	.globl	fn
	.ent	fn
	.type	fn,@function
fn:
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

	move	$sp,$fp
	lw	$fp,8($sp)
	addiu	$sp,$sp,16
	j	$31
	.end	fn

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
