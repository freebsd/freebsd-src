/*
 * Copyright (c) 1989 The Regents of the University of California.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	from: @(#)kinfo.h	7.9 (Berkeley) 6/26/91
 *	$Id: kinfo.h,v 1.3 1993/11/07 17:52:40 wollman Exp $
 */

#ifndef _SYS_KINFO_H_
#define _SYS_KINFO_H_ 1

/*
 * Get kernel info
 */
#define ki_op(x)		((x)&0x0000ffff)
#define ki_type(x)		((x)&0x0000ff00)

/* 
 * proc info 
 */
#define	KINFO_PROC		(0<<8)
#define KINFO_PROC_ALL	(KINFO_PROC|0)	/* everything */
#define	KINFO_PROC_PID	(KINFO_PROC|1)	/* by process id */
#define	KINFO_PROC_PGRP	(KINFO_PROC|2)	/* by process group id */
#define	KINFO_PROC_SESSION (KINFO_PROC|3)	/* by session of pid */
#define	KINFO_PROC_TTY	(KINFO_PROC|4)	/* by controlling tty */
#define	KINFO_PROC_UID	(KINFO_PROC|5)	/* by effective uid */
#define	KINFO_PROC_RUID	(KINFO_PROC|6)	/* by real uid */

/*
 * Routing table
 */
#define ki_af(x)		(((x)&0x00ff0000) >> 16)
#define KINFO_RT		(1<<8)
#define KINFO_RT_DUMP	(KINFO_RT|1)	/* dump; may limit to a.f. */
#define KINFO_RT_FLAGS	(KINFO_RT|2)	/* by flags, e.g. RESOLVING */

/*
 * vnodes
 */
#define	KINFO_VNODE		(2<<8)

/*
 * file structures
 */
#define KINFO_FILE		(3<<8)

/*
 * Locking and stats
 */

struct kinfo_lock {
	int	kl_lock;
	int	kl_want;
	int	kl_locked;
};

#ifdef KERNEL
extern struct kinfo_lock kinfo_lock;
#else

#include <sys/cdefs.h>

__BEGIN_DECLS
int	getkerninfo __P((int, void *, int *, int));
__END_DECLS
#endif
#endif /* _SYS_KINFO_H_ */
