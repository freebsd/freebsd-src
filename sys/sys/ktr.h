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

/*
 * Hack around due to egcs-1.1.2 not knowing what __func__ is.
 */
#ifdef __GNUC__
#if __GNUC__ == 2 && __GNUC_MINOR__ == 91      /* egcs 1.1.2 */
#define        __func__        __FUNCTION__
#endif
#endif

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
#define	KTR_EVH		0x00020000		/* Eventhandler */
#define KTR_NFS		0x00040000		/* The obvious */
#define KTR_VOP		0x00080000		/* The obvious */
#define KTR_VM		0x00100000		/* The virtual memory system */
#define KTR_WITNESS	0x00200000
#define	KTR_RUNQ	0x00400000		/* Run queue */
#define	KTR_CONTENTION	0x00800000		/* Lock contention */
#define	KTR_UMA		0x01000000		/* UMA slab allocator */
#define	KTR_CALLOUT	0x02000000		/* Callouts and timeouts */
#define	KTR_GEOM	0x04000000		/* GEOM I/O events */
#define	KTR_BUSDMA	0x08000000		/* busdma(9) events */
#define	KTR_CRITICAL	0x10000000		/* Critical sections */
#define	KTR_SCHED	0x20000000		/* Machine parsed sched info. */
#define	KTR_BUF		0x40000000		/* Buffer cache */
#define	KTR_ALL		0x7fffffff

/*
 * Trace classes which can be assigned to particular use at compile time
 * These must remain in high 22 as some assembly code counts on it
 */
#define KTR_CT1		0x01000000
#define KTR_CT2		0x02000000
#define KTR_CT3		0x04000000
#define KTR_CT4		0x08000000
#define KTR_CT5		0x10000000
#define KTR_CT6		0x20000000
#define KTR_CT7		0x40000000
#define KTR_CT8		0x80000000

/* Trace classes to compile in */
#ifndef KTR_COMPILE
#define	KTR_COMPILE	(KTR_GEN)
#endif

/* Trace classes that can not be used with KTR_ALQ */
#define	KTR_ALQ_MASK	(KTR_WITNESS)

/*
 * Version number for ktr_entry struct.  Increment this when you break binary
 * compatibility.
 */
#define	KTR_VERSION	1

#define	KTR_PARMS	6

#ifndef LOCORE

struct ktr_entry {
	u_int64_t ktr_timestamp;
	int	ktr_cpu;
	int	ktr_line;
	const	char *ktr_file;
	const	char *ktr_desc;
	u_long	ktr_parms[KTR_PARMS];
};

extern int ktr_cpumask;
extern int ktr_mask;
extern int ktr_entries;
extern int ktr_verbose;

extern volatile int ktr_idx;
extern struct ktr_entry ktr_buf[];

#endif /* !LOCORE */
#ifdef KTR

void	ktr_tracepoint(u_int mask, const char *file, int line,
	    const char *format, u_long arg1, u_long arg2, u_long arg3,
	    u_long arg4, u_long arg5, u_long arg6);

#define CTR6(m, format, p1, p2, p3, p4, p5, p6) do {			\
	if (KTR_COMPILE & (m))						\
		ktr_tracepoint((m), __FILE__, __LINE__, format,		\
		    (u_long)(p1), (u_long)(p2), (u_long)(p3),		\
		    (u_long)(p4), (u_long)(p5), (u_long)(p6));		\
	} while(0)
#define CTR0(m, format)			CTR6(m, format, 0, 0, 0, 0, 0, 0)
#define CTR1(m, format, p1)		CTR6(m, format, p1, 0, 0, 0, 0, 0)
#define	CTR2(m, format, p1, p2)		CTR6(m, format, p1, p2, 0, 0, 0, 0)
#define	CTR3(m, format, p1, p2, p3)	CTR6(m, format, p1, p2, p3, 0, 0, 0)
#define	CTR4(m, format, p1, p2, p3, p4)	CTR6(m, format, p1, p2, p3, p4, 0, 0)
#define	CTR5(m, format, p1, p2, p3, p4, p5)	CTR6(m, format, p1, p2, p3, p4, p5, 0)
#else	/* KTR */
#undef KTR_COMPILE
#define KTR_COMPILE 0
#define	CTR0(m, d)
#define	CTR1(m, d, p1)
#define	CTR2(m, d, p1, p2)
#define	CTR3(m, d, p1, p2, p3)
#define	CTR4(m, d, p1, p2, p3, p4)
#define	CTR5(m, d, p1, p2, p3, p4, p5)
#define	CTR6(m, d, p1, p2, p3, p4, p5, p6)
#endif	/* KTR */

#define	TR0(d)				CTR0(KTR_GEN, d)
#define	TR1(d, p1)			CTR1(KTR_GEN, d, p1)
#define	TR2(d, p1, p2)			CTR2(KTR_GEN, d, p1, p2)
#define	TR3(d, p1, p2, p3)		CTR3(KTR_GEN, d, p1, p2, p3)
#define	TR4(d, p1, p2, p3, p4)		CTR4(KTR_GEN, d, p1, p2, p3, p4)
#define	TR5(d, p1, p2, p3, p4, p5)	CTR5(KTR_GEN, d, p1, p2, p3, p4, p5)
#define	TR6(d, p1, p2, p3, p4, p5, p6)	CTR6(KTR_GEN, d, p1, p2, p3, p4, p5, p6)

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
#define	ITR6(d, p1, p2, p3, p4, p5, p6)	CTR6(KTR_INIT, d, p1, p2, p3, p4, p5, p6)
#else
#define	ITR0(d)
#define	ITR1(d, p1)
#define	ITR2(d, p1, p2)
#define	ITR3(d, p1, p2, p3)
#define	ITR4(d, p1, p2, p3, p4)
#define	ITR5(d, p1, p2, p3, p4, p5)
#define	ITR6(d, p1, p2, p3, p4, p5, p6)
#endif

#endif /* !_SYS_KTR_H_ */
