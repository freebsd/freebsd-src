/*-
 * Copyright 2001 by Thomas Moestl <tmm@FreeBSD.org>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef	_MACHINE_FP_H_
#define	_MACHINE_FP_H_

/* A block of 8 double-precision (16 single-precision) FP registers. */
struct fpblock {
	u_long	fpq_l[8];
};

struct fpstate {
	struct	fpblock fp_fb[4];
	u_long	fp_fsr;
	u_long	fp_fprs;
};

#ifdef _KERNEL

struct pcb;
struct thread;

void	fp_init_thread(struct pcb *);
int	fp_enable_thread(struct thread *, struct trapframe *);
int	fp_exception_other(struct thread *, struct trapframe *);
/*
 * Note: The pointers passed to the next two functions must be aligned on
 * 64 byte boundaries.
 */
void	savefpctx(struct fpstate *);
void	restorefpctx(struct fpstate *);

#endif /* _KERNEL */
#endif /* !_MACHINE_FP_H_ */
