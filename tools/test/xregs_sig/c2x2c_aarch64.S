/*
 * This file is in public domain.
 * Written by Dmitry Chagin <dchagin@FreeBSD.org>
 *
 * $FreeBSD$
 */

	.global cpu_to_simd
	.type cpu_to_simd, @function
cpu_to_simd:
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

	.size cpu_to_simd, . - cpu_to_simd


	.global simd_to_cpu
	.type simd_to_cpu, @function
simd_to_cpu:
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

	.size simd_to_cpu, . - simd_to_cpu

	.section        .note.GNU-stack,"",@progbits
