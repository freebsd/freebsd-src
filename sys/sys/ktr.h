/*-
 * Copyright (c) 1996 Berkeley Software Design, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Berkeley Software Design Inc's name may not be used to endorse or
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BERKELEY SOFTWARE DESIGN INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL BERKELEY SOFTWARE DESIGN INC BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from BSDI $Id: ktr.h,v 1.10.2.7 2000/03/16 21:44:42 cp Exp $
 * $FreeBSD$
 */

/*
 *	Wraparound kernel trace buffer support.
 */

#ifndef _SYS_KTR_H_
#define _SYS_KTR_H_

/* Requires sys/types.h, sys/time.h, machine/atomic.h, and machine/cpufunc.h */

#include <machine/atomic.h>
#include <machine/cpufunc.h>

/*
 * Trace classes
 */
#define	KTR_GEN		0x00000001		/* General (TR) */
#define	KTR_NET		0x00000002		/* Network */
#define	KTR_DEV		0x00000004		/* Device driver */
#define	KTR_LOCK	0x00000008		/* MP locking */
#define	KTR_SMP		0x00000010		/* MP general */
#define	KTR_FS		0x00000020		/* Filesystem */
#define KTR_PMAP	0x00000040		/* Pmap tracing */
#define KTR_MALLOC	0x00000080		/* Malloc tracing */
#define	KTR_TRAP	0x00000100		/* Trap processing */
#define	KTR_INTR	0x00000200		/* Interrupt tracing */
#define KTR_SIG		0x00000400		/* Signal processing */
#define	KTR_CLK		0x00000800		/* hardclock verbose */
#define	KTR_PROC	0x00001000		/* Process scheduling */
#define	KTR_SYSC	0x00002000		/* System call */
#define	KTR_INIT	0x00004000		/* System initialization */
#define KTR_KGDB	0x00008000		/* Trace kgdb internals */
#define	KTR_IO		0x00010000		/* Upper I/O  */
#define KTR_LOCKMGR	0x00020000
#define KTR_NFS		0x00040000		/* The obvious */
#define KTR_VOP		0x00080000		/* The obvious */
#define KTR_VM		0x00100000		/* The virtual memory system */
#define KTR_IDLELOOP	0x00200000		/* checks done in the idle process */
#define	KTR_RUNQ	0x00400000		/* Run queue */
#define	KTR_ALL		0x007fffff

/*
 * Trace classes which can be assigned to particular use at compile time
 * These must remain in high 22 as some assembly code counts on it
 */
#define KTR_CT1		0x010000000
#define KTR_CT2		0x020000000
#define KTR_CT3		0x040000000
#define KTR_CT4		0x080000000
#define KTR_CT5		0x100000000
#define KTR_CT6		0x200000000
#define KTR_CT7		0x400000000
#define KTR_CT8		0x800000000

/* Trace classes to compile in */
#ifndef KTR_COMPILE
#define	KTR_COMPILE	(KTR_GEN)
#endif

#ifndef LOCORE

#include <sys/time.h>

struct ktr_entry {
	struct	timespec ktr_tv;
#ifdef KTR_EXTEND
#ifndef KTRDESCSIZE
#define KTRDESCSIZE 80
#endif
	char	ktr_desc[KTRDESCSIZE];
	const	char *ktr_filename;
	int	ktr_line;
	int	ktr_cpu;
#else
	const	char *ktr_desc;
	u_long	ktr_parm1;
	u_long	ktr_parm2;
	u_long	ktr_parm3;
	u_long	ktr_parm4;
	u_long	ktr_parm5;
#endif
};

/* These variables are used by gdb to analyse the output */
extern int ktr_extend;

extern int ktr_cpumask;
extern int ktr_mask;
extern int ktr_entries;
extern int ktr_verbose;

extern volatile int ktr_idx;
extern struct ktr_entry ktr_buf[];

#endif /* !LOCORE */
#ifdef KTR

#ifndef KTR_ENTRIES
#define	KTR_ENTRIES	1024
#endif

#ifdef KTR_EXTEND
void	ktr_tracepoint(u_int mask, const char *filename, u_int line,
		       const char *format, ...) __printflike(4, 5);
#else
void	ktr_tracepoint(u_int mask, const char *format, u_long arg1, u_long arg2,
		       u_long arg3, u_long arg4, u_long arg5);
#endif

#ifdef KTR_EXTEND
#define CTR(m, format, args...) do {					\
	if (KTR_COMPILE & (m))						\
		ktr_tracepoint((m), __FILE__, __LINE__, format , ##args); \
	} while(0)

#define	CTR0(m, format)			CTR(m, format)
#define	CTR1(m, format, p1)		CTR(m, format, p1)
#define	CTR2(m, format, p1, p2)		CTR(m, format, p1, p2)
#define	CTR3(m, format, p1, p2, p3)	CTR(m, format, p1, p2, p3)
#define	CTR4(m, format, p1, p2, p3, p4)	CTR(m, format, p1, p2, p3, p4)
#define	CTR5(m, format, p1, p2, p3, p4, p5)				\
	CTR(m, format, p1, p2, p3, p4, p5)
#else							    /* not extended */
#define CTR5(m, format, p1, p2, p3, p4, p5) do {			\
	if (KTR_COMPILE & (m))						\
		ktr_tracepoint((m), format, (u_long)p1, (u_long)p2,	\
		    (u_long)p3, (u_long)p4, (u_long)p5);		\
	} while(0)
#define CTR0(m, format)			CTR5(m, format, 0, 0, 0, 0, 0)
#define CTR1(m, format, p1)		CTR5(m, format, p1, 0, 0, 0, 0)
#define	CTR2(m, format, p1, p2)		CTR5(m, format, p1, p2, 0, 0, 0)
#define	CTR3(m, format, p1, p2, p3)	CTR5(m, format, p1, p2, p3, 0, 0)
#define	CTR4(m, format, p1, p2, p3, p4)	CTR5(m, format, p1, p2, p3, p4, 0)
#endif	/* KTR_EXTEND */
#else	/* KTR */
#undef KTR_COMPILE
#define KTR_COMPILE 0
#define	CTR0(m, d)
#define	CTR1(m, d, p1)
#define	CTR2(m, d, p1, p2)
#define	CTR3(m, d, p1, p2, p3)
#define	CTR4(m, d, p1, p2, p3, p4)
#define	CTR5(m, d, p1, p2, p3, p4, p5)
#endif	/* KTR */

#define	TR0(d)				CTR0(KTR_GEN, d)
#define	TR1(d, p1)			CTR1(KTR_GEN, d, p1)
#define	TR2(d, p1, p2)			CTR2(KTR_GEN, d, p1, p2)
#define	TR3(d, p1, p2, p3)		CTR3(KTR_GEN, d, p1, p2, p3)
#define	TR4(d, p1, p2, p3, p4)		CTR4(KTR_GEN, d, p1, p2, p3, p4)
#define	TR5(d, p1, p2, p3, p4, p5)	CTR5(KTR_GEN, d, p1, p2, p3, p4, p5)

/*
 * Trace initialization events, similar to CTR with KTR_INIT, but
 * completely ifdef'ed out if KTR_INIT isn't in KTR_COMPILE (to
 * save string space, the compiler doesn't optimize out strings
 * for the conditional ones above).
 */
#if (KTR_COMPILE & KTR_INIT) != 0
#define	ITR0(d)				CTR0(KTR_INIT, d)
#define	ITR1(d, p1)			CTR1(KTR_INIT, d, p1)
#define	ITR2(d, p1, p2)			CTR2(KTR_INIT, d, p1, p2)
#define	ITR3(d, p1, p2, p3)		CTR3(KTR_INIT, d, p1, p2, p3)
#define	ITR4(d, p1, p2, p3, p4)		CTR4(KTR_INIT, d, p1, p2, p3, p4)
#define	ITR5(d, p1, p2, p3, p4, p5)	CTR5(KTR_INIT, d, p1, p2, p3, p4, p5)
#else
#define	ITR0(d)
#define	ITR1(d, p1)
#define	ITR2(d, p1, p2)
#define	ITR3(d, p1, p2, p3)
#define	ITR4(d, p1, p2, p3, p4)
#define	ITR5(d, p1, p2, p3, p4, p5)
#endif

#endif /* !_SYS_KTR_H_ */
