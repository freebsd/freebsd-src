	.file	1 "tlsbin-o32.s"
	.abicalls
	.text
	.align	2
	.globl	other
	.ent	other
	.type	other,@function
other:
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
	.end	other
