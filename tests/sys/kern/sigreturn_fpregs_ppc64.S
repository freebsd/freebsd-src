/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Piotr Kubaj <pkubaj@FreeBSD.org>
 */

/*
 * Helpers for sigreturn_fpregs.c, kept in assembly so that nothing touches
 * the FP registers between the sigreturn and the stores that take the lazy
 * reload trap.
 *
 * fp_roundtrip(in, out, tid, sig, wfd, rfd): load f14-f31 from in, raise
 * sig on tid with thr_kill(2), write a byte to wfd and read one from rfd
 * (both raw system calls: they let the helper on this CPU run), then store
 * f14-f31 and the FPSCR to out.
 *
 * fp_load_all(pat): load f0-f31 from pat.
 */

	.abiversion 2
	.text
/* void fp_roundtrip(const double *in, double *out, long tid, long sig, long wfd, long rfd) */
	.globl fp_roundtrip
	.type fp_roundtrip,@function
fp_roundtrip:
	mflr 0
	std 0,16(1)
	stdu 1,-320(1)
	stfd 14,32(1)
	stfd 15,40(1)
	stfd 16,48(1)
	stfd 17,56(1)
	stfd 18,64(1)
	stfd 19,72(1)
	stfd 20,80(1)
	stfd 21,88(1)
	stfd 22,96(1)
	stfd 23,104(1)
	stfd 24,112(1)
	stfd 25,120(1)
	stfd 26,128(1)
	stfd 27,136(1)
	stfd 28,144(1)
	stfd 29,152(1)
	stfd 30,160(1)
	stfd 31,168(1)
	std 28,192(1)
	std 29,200(1)
	std 30,176(1)
	std 31,184(1)
	mr 30,3
	mr 31,4
	mr 28,7
	mr 29,8
	lfd 14,0(30)
	lfd 15,8(30)
	lfd 16,16(30)
	lfd 17,24(30)
	lfd 18,32(30)
	lfd 19,40(30)
	lfd 20,48(30)
	lfd 21,56(30)
	lfd 22,64(30)
	lfd 23,72(30)
	lfd 24,80(30)
	lfd 25,88(30)
	lfd 26,96(30)
	lfd 27,104(30)
	lfd 28,112(30)
	lfd 29,120(30)
	lfd 30,128(30)
	lfd 31,136(30)
	li 0,433		/* SYS_thr_kill */
	mr 3,5
	mr 4,6
	sc
	/* ping the helper on this CPU: it leaves ITS registers in the FPU */
	li 0,4		/* SYS_write(wfd, buf, 1) */
	mr 3,28
	addi 4,1,208
	li 5,1
	sc
	li 0,3		/* SYS_read(rfd, buf, 1) */
	mr 3,29
	addi 4,1,208
	li 5,1
	sc
	/* first FP instruction since sigreturn: lazy reload happens here */
	stfd 14,0(31)
	stfd 15,8(31)
	stfd 16,16(31)
	stfd 17,24(31)
	stfd 18,32(31)
	stfd 19,40(31)
	stfd 20,48(31)
	stfd 21,56(31)
	stfd 22,64(31)
	stfd 23,72(31)
	stfd 24,80(31)
	stfd 25,88(31)
	stfd 26,96(31)
	stfd 27,104(31)
	stfd 28,112(31)
	stfd 29,120(31)
	stfd 30,128(31)
	stfd 31,136(31)
	mffs 0		/* FPSCR, for diagnosis */
	stfd 0,144(31)
	lfd 14,32(1)
	lfd 15,40(1)
	lfd 16,48(1)
	lfd 17,56(1)
	lfd 18,64(1)
	lfd 19,72(1)
	lfd 20,80(1)
	lfd 21,88(1)
	lfd 22,96(1)
	lfd 23,104(1)
	lfd 24,112(1)
	lfd 25,120(1)
	lfd 26,128(1)
	lfd 27,136(1)
	lfd 28,144(1)
	lfd 29,152(1)
	lfd 30,160(1)
	lfd 31,168(1)
	ld 28,192(1)
	ld 29,200(1)
	ld 30,176(1)
	ld 31,184(1)
	addi 1,1,320
	ld 0,16(1)
	mtlr 0
	blr
	.size fp_roundtrip,.-fp_roundtrip

/* void fp_load_all(const double *pat): leave pat[0..31] live in f0-f31 */
	.globl fp_load_all
	.type fp_load_all,@function
fp_load_all:
	lfd 0,0(3)
	lfd 1,8(3)
	lfd 2,16(3)
	lfd 3,24(3)
	lfd 4,32(3)
	lfd 5,40(3)
	lfd 6,48(3)
	lfd 7,56(3)
	lfd 8,64(3)
	lfd 9,72(3)
	lfd 10,80(3)
	lfd 11,88(3)
	lfd 12,96(3)
	lfd 13,104(3)
	lfd 14,112(3)
	lfd 15,120(3)
	lfd 16,128(3)
	lfd 17,136(3)
	lfd 18,144(3)
	lfd 19,152(3)
	lfd 20,160(3)
	lfd 21,168(3)
	lfd 22,176(3)
	lfd 23,184(3)
	lfd 24,192(3)
	lfd 25,200(3)
	lfd 26,208(3)
	lfd 27,216(3)
	lfd 28,224(3)
	lfd 29,232(3)
	lfd 30,240(3)
	lfd 31,248(3)
	blr
	.size fp_load_all,.-fp_load_all
	.section .note.GNU-stack,"",@progbits
