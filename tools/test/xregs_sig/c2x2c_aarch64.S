/*
 * This file is in public domain.
 * Written by Dmitry Chagin <dchagin@FreeBSD.org>
 */

	.text

	.globl xregs_banks_max
	.type xregs_banks_max, @function
xregs_banks_max:
	mov  x0, #0
	ret

	.size xregs_banks_max, . - xregs_banks_max


	.globl cpu_to_vfp
	.type cpu_to_vfp, @function
cpu_to_vfp:
	stp  q0,  q1, [x0, #( 0 * 16)]
	stp  q2,  q3, [x0, #( 2 * 16)]
	stp  q4,  q5, [x0, #( 4 * 16)]
	stp  q6,  q7, [x0, #( 6 * 16)]
	stp  q8,  q9, [x0, #( 8 * 16)]
	stp q10, q11, [x0, #(10 * 16)]
	stp q12, q13, [x0, #(12 * 16)]
	stp q14, q15, [x0, #(14 * 16)]
	stp q16, q17, [x0, #(16 * 16)]
	stp q18, q19, [x0, #(18 * 16)]
	stp q20, q21, [x0, #(20 * 16)]
	stp q22, q23, [x0, #(22 * 16)]
	stp q24, q25, [x0, #(24 * 16)]
	stp q26, q27, [x0, #(26 * 16)]
	stp q28, q29, [x0, #(28 * 16)]
	stp q30, q31, [x0, #(30 * 16)]
	ret

	.size cpu_to_vfp, . - cpu_to_vfp


	.globl vfp_to_cpu
	.type vfp_to_cpu, @function
vfp_to_cpu:
	ldp  q0,  q1, [x0, #( 0 * 16)]
	ldp  q2,  q3, [x0, #( 2 * 16)]
	ldp  q4,  q5, [x0, #( 4 * 16)]
	ldp  q6,  q7, [x0, #( 6 * 16)]
	ldp  q8,  q9, [x0, #( 8 * 16)]
	ldp q10, q11, [x0, #(10 * 16)]
	ldp q12, q13, [x0, #(12 * 16)]
	ldp q14, q15, [x0, #(14 * 16)]
	ldp q16, q17, [x0, #(16 * 16)]
	ldp q18, q19, [x0, #(18 * 16)]
	ldp q20, q21, [x0, #(20 * 16)]
	ldp q22, q23, [x0, #(22 * 16)]
	ldp q24, q25, [x0, #(24 * 16)]
	ldp q26, q27, [x0, #(26 * 16)]
	ldp q28, q29, [x0, #(28 * 16)]
	ldp q30, q31, [x0, #(30 * 16)]
	ret

	.size vfp_to_cpu, . - vfp_to_cpu

	.section        .note.GNU-stack,"",@progbits
