/*-
 * Copyright (c) 2003 Peter Wemm.
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)frame.h	5.2 (Berkeley) 1/18/91
 * $FreeBSD$
 */

#ifndef _MACHINE_FRAME_H_
#define _MACHINE_FRAME_H_ 1

/*
 * System stack frames.
 */

/*
 * Exception/Trap Stack Frame
 *
 * The ordering of this is specifically so that we can take first 6
 * the syscall arguments directly from the beginning of the frame.
 */

struct trapframe {
	register_t	tf_rdi;
	register_t	tf_rsi;
	register_t	tf_rdx;
	register_t	tf_rcx;
	register_t	tf_r8;
	register_t	tf_r9;
	register_t	tf_rax;
	register_t	tf_rbx;
	register_t	tf_rbp;
	register_t	tf_r10;
	register_t	tf_r11;
	register_t	tf_r12;
	register_t	tf_r13;
	register_t	tf_r14;
	register_t	tf_r15;
	register_t	tf_trapno;
	register_t	tf_addr;
	register_t	tf_flags;
	/* below portion defined in hardware */
	register_t	tf_err;
	register_t	tf_rip;
	register_t	tf_cs;
	register_t	tf_rflags;
	register_t	tf_rsp;
	register_t	tf_ss;
};

/* Interrupt stack frame */

struct intrframe {
	register_t	if_rdi;
	register_t	if_rsi;
	register_t	if_rdx;
	register_t	if_rcx;
	register_t	if_r8;
	register_t	if_r9;
	register_t	if_rax;
	register_t	if_rbx;
	register_t	if_rbp;
	register_t	if_r10;
	register_t	if_r11;
	register_t	if_r12;
	register_t	if_r13;
	register_t	if_r14;
	register_t	if_r15;
	register_t	:64;		/* compat with trap frame - trapno */
	register_t	:64;		/* compat with trap frame - addr */
	register_t	:64;		/* compat with trap frame - flags */
	register_t	:64;		/* compat with trap frame - err */
	/* below portion defined in hardware */
	register_t	if_rip;
	register_t	if_cs;
	register_t	if_rflags;
	register_t	if_rsp;
	register_t	if_ss;
};

/* frame of clock (same as interrupt frame) */

struct clockframe {
	register_t	cf_rdi;
	register_t	cf_rsi;
	register_t	cf_rdx;
	register_t	cf_rcx;
	register_t	cf_r8;
	register_t	cf_r9;
	register_t	cf_rax;
	register_t	cf_rbx;
	register_t	cf_rbp;
	register_t	cf_r10;
	register_t	cf_r11;
	register_t	cf_r12;
	register_t	cf_r13;
	register_t	cf_r14;
	register_t	cf_r15;
	register_t	:64;		/* compat with trap frame - trapno */
	register_t	:64;		/* compat with trap frame - addr */
	register_t	:64;		/* compat with trap frame - flags */
	register_t	:64;		/* compat with trap frame - err */
	/* below portion defined in hardware */
	register_t	cf_rip;
	register_t	cf_cs;
	register_t	cf_rflags;
	register_t	cf_rsp;
	register_t	cf_ss;
};

#endif /* _MACHINE_FRAME_H_ */
