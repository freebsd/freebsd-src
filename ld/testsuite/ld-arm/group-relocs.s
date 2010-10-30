@ Tests for group relocations.
@
@ Beware when editing this file: it is carefully crafted so that
@ specific PC- and SB-relative offsets arise.
@
@ Note that the gas tests have already checked that group relocations are
@ handled in the same way for local and external symbols.

@ We will place .text at 0x8000.

	.text
	.globl _start

_start:
	@ ALU, PC-relative

	@ Instructions start at .text + 0x0
	add	r0, r15, #:pc_g0:(one_group_needed_alu_pc)

	@ Instructions start at .text + 0x4
	add	r0, r15, #:pc_g0_nc:(two_groups_needed_alu_pc)
	add	r0, r0, #:pc_g1:(two_groups_needed_alu_pc + 4)

	@ Instructions start at .text + 0xc
	add	r0, r15, #:pc_g0_nc:(three_groups_needed_alu_pc)
	add	r0, r0, #:pc_g1_nc:(three_groups_needed_alu_pc + 4)
	add	r0, r0, #:pc_g2:(three_groups_needed_alu_pc + 8)

	@ ALU, SB-relative

	add	r0, r0, #:sb_g0:(one_group_needed_alu_sb)

	add	r0, r15, #:sb_g0_nc:(two_groups_needed_alu_sb)
	add	r0, r0, #:sb_g1:(two_groups_needed_alu_sb)

	add	r0, r0, #:sb_g0_nc:(three_groups_needed_alu_sb)
	add	r0, r0, #:sb_g1_nc:(three_groups_needed_alu_sb)
	add	r0, r0, #:sb_g2:(three_groups_needed_alu_sb)

	@ LDR, PC-relative

	@ Instructions start at .text + 0x30
	add	r0, r0, #:pc_g0_nc:(two_groups_needed_ldr_pc)
	ldr	r1, [r0, #:pc_g1:(two_groups_needed_ldr_pc + 4)]

	@ Instructions start at .text + 0x38
	add	r0, r0, #:pc_g0_nc:(three_groups_needed_ldr_pc)
	add	r0, r0, #:pc_g1_nc:(three_groups_needed_ldr_pc + 4)
	ldr	r1, [r0, #:pc_g2:(three_groups_needed_ldr_pc + 8)]

	@ LDR, SB-relative

	ldr	r1, [r0, #:sb_g0:(one_group_needed_ldr_sb)]

	add	r0, r0, #:sb_g0_nc:(two_groups_needed_ldr_sb)
	ldr	r1, [r0, #:sb_g1:(two_groups_needed_ldr_sb)]

	add	r0, r0, #:sb_g0_nc:(three_groups_needed_ldr_sb)
	add	r0, r0, #:sb_g1_nc:(three_groups_needed_ldr_sb)
	ldr	r1, [r0, #:sb_g2:(three_groups_needed_ldr_sb)]

	@ LDRS, PC-relative

	@ Instructions start at .text + 0x5c
	ldrd	r2, [r0, #:pc_g0:(one_group_needed_ldrs_pc)]

	@ Instructions start at .text + 0x60
	add	r0, r0, #:pc_g0_nc:(two_groups_needed_ldrs_pc)
	ldrd	r2, [r0, #:pc_g1:(two_groups_needed_ldrs_pc + 4)]

	@ Instructions start at .text + 0x68
	add	r0, r0, #:pc_g0_nc:(three_groups_needed_ldrs_pc)
	add	r0, r0, #:pc_g1_nc:(three_groups_needed_ldrs_pc + 4)
	ldrd	r2, [r0, #:pc_g2:(three_groups_needed_ldrs_pc + 8)]

	@ LDRS, SB-relative

	ldrd	r2, [r0, #:sb_g0:(one_group_needed_ldrs_sb)]

	add	r0, r0, #:sb_g0_nc:(two_groups_needed_ldrs_sb)
	ldrd	r2, [r0, #:sb_g1:(two_groups_needed_ldrs_sb)]

	add	r0, r0, #:sb_g0_nc:(three_groups_needed_ldrs_sb)
	add	r0, r0, #:sb_g1_nc:(three_groups_needed_ldrs_sb)
	ldrd	r2, [r0, #:sb_g2:(three_groups_needed_ldrs_sb)]

	@ LDC, PC-relative

	@ Instructions start at .text + 0x8c
	ldc	0, c0, [r0, #:pc_g0:(one_group_needed_ldc_pc)]

	@ Instructions start at .text + 0x90
	add	r0, r0, #:pc_g0_nc:(two_groups_needed_ldc_pc)
	ldc	0, c0, [r0, #:pc_g1:(two_groups_needed_ldc_pc + 4)]

	@ Instructions start at .text + 0x98
	add	r0, r0, #:pc_g0_nc:(three_groups_needed_ldc_pc)
	add	r0, r0, #:pc_g1_nc:(three_groups_needed_ldc_pc + 4)
	ldc	0, c0, [r0, #:pc_g2:(three_groups_needed_ldc_pc + 8)]

	@ LDC, SB-relative

	ldc	0, c0, [r0, #:sb_g0:(one_group_needed_ldc_sb)]

	add	r0, r0, #:sb_g0_nc:(two_groups_needed_ldc_sb)
	ldc	0, c0, [r0, #:sb_g1:(two_groups_needed_ldc_sb)]

	add	r0, r0, #:sb_g0_nc:(three_groups_needed_ldc_sb)
	add	r0, r0, #:sb_g1_nc:(three_groups_needed_ldc_sb)
	ldc	0, c0, [r0, #:sb_g2:(three_groups_needed_ldc_sb)]

@ This point in the file is .text + 0xbc.

one_group_needed_alu_pc:
one_group_needed_ldrs_pc:
one_group_needed_ldc_pc:
	mov	r0, #0

@ We will place the section zero at 0x0.

	.section zero

one_group_needed_alu_sb:
one_group_needed_ldr_sb:
one_group_needed_ldrs_sb:
one_group_needed_ldc_sb:
	mov	r0, #0

@ We will place the section alpha at 0xeef0.

	.section alpha

two_groups_needed_alu_sb:
two_groups_needed_ldr_sb:
two_groups_needed_ldrs_sb:
two_groups_needed_ldc_sb:
two_groups_needed_alu_pc:
two_groups_needed_ldr_pc:
two_groups_needed_ldrs_pc:
two_groups_needed_ldc_pc:
	mov	r0, #0

@ We will place the section beta at 0xffeef0.

	.section beta

three_groups_needed_alu_sb:
three_groups_needed_ldr_sb:
three_groups_needed_ldrs_sb:
three_groups_needed_ldc_sb:
three_groups_needed_alu_pc:
three_groups_needed_ldr_pc:
three_groups_needed_ldrs_pc:
three_groups_needed_ldc_pc:
	mov	r0, #0

