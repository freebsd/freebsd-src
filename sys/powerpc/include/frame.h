/*-
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$NetBSD: frame.h,v 1.2 1999/01/10 10:13:15 tsubai Exp $
 * $FreeBSD$
 */

#ifndef	_MACHINE_FRAME_H_
#define	_MACHINE_FRAME_H_

#include <sys/types.h>

/*
 * We have to save all registers on every trap, because
 *	1. user could attach this process every time
 *	2. we must be able to restore all user registers in case of fork
 * Actually, we do not save the fp registers on trap, since
 * these are not used by the kernel. They are saved only when switching
 * between processes using the FPU.
 *
 * Change ordering to cluster together these register_t's.		XXX
 */
struct trapframe {
	register_t fixreg[32];
	register_t lr;
	int cr;
	int xer;
	register_t ctr;
	register_t srr0;
	register_t srr1;
	register_t dar;		/* dar & dsisr are only filled on a DSI trap */
	int dsisr;
	int exc;
};
/*
 * This is to ensure alignment of the stackpointer
 */
#define	FRAMELEN	roundup(sizeof(struct trapframe) + 8, 16)
#define	trapframe(td)	((td)->td_frame)

struct switchframe {
	register_t sp;
	register_t fill;
	register_t user_sr;
	register_t cr;
	register_t fixreg2;
	register_t fixreg[19];		/* R13-R31 */
};

struct clockframe {
	register_t srr1;
	register_t srr0;
	int pri;
	int depth;
};

/*
 * Call frame for PowerPC used during fork.
 */
struct callframe {
	register_t	cf_dummy_fp;	/* dummy frame pointer */
	register_t	cf_lr;		/* space for link register save */
	register_t	cf_func;
	register_t	cf_arg0;
	register_t	cf_arg1;
};

#define	IFRAMELEN	sizeof(struct intrframe)
struct intrframe {
	register_t r1;			/*  0 */
	register_t _pad4;		/*  4 */
	/*
	 * The next 4 fields are "clockframe"
	 */
	register_t srr1;		/*  8 */
	register_t srr0;		/* 12 */
	int pri;			/* 16 */
	int intr_depth;			/* 20 */
	register_t vrsave;		/* 24 */
	register_t ctr;			/* 28 */
	register_t xer;			/* 32 */
	register_t cr;			/* 36 */
	register_t lr;			/* 40 */
	register_t r12;			/* 44 */
	register_t r11;			/* 48 */
	register_t r10;			/* 52 */
	register_t r9;			/* 56 */
	register_t r8;			/* 60 */
	register_t r7;			/* 64 */
	register_t r6;			/* 68 */
	register_t r5;			/* 72 */
	register_t r4;			/* 76 */
	register_t r3;			/* 80 */
	register_t r0;			/* 84 */
};

#define	SPFRAMELEN	sizeof(struct spillframe)
struct spillframe {
	register_t	r1;		/*  0 */
	register_t	_pad4;		/*  4 */
	register_t	r12;		/*  8 */
	register_t	r11;		/* 12 */
	register_t	r10;		/* 16 */
	register_t	r9;		/* 20 */
	register_t	r8;		/* 24 */
	register_t	r7;		/* 28 */
	register_t	r6;		/* 32 */
	register_t	r5;		/* 36 */
	register_t	r4;		/* 40 */
	register_t	r3;		/* 44 */
	register_t	r0;		/* 48 */
};

/* Definitions for syscalls */
#define	FIRSTARG	3				/* first arg in reg 3 */
#define	NARGREG		8				/* 8 args in regs */
#define	MOREARGS(sp)	((caddr_t)((int)(sp) + 8))	/* more args go here */

#endif	/* _MACHINE_FRAME_H_ */
