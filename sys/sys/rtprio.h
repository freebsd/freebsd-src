/*
 * Copyright (c) 1994, Henrik Vestergaard Draboel
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
 *	This product includes software developed by (name).
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _SYS_RTPRIO_H_
#define _SYS_RTPRIO_H_

/*
 * Process realtime-priority specifications to rtprio.
 */

/* priority types.  Start at 1 to catch uninitialized fields. */

#define RTP_PRIO_ITHREAD	1	/* interrupt thread */
#define RTP_PRIO_REALTIME	2	/* real time process */
#define RTP_PRIO_NORMAL		3	/* time sharing process */
#define RTP_PRIO_IDLE		4	/* idle process */

/* RTP_PRIO_FIFO is POSIX.1B SCHED_FIFO.
 */

#define RTP_PRIO_FIFO_BIT	4
#define RTP_PRIO_FIFO		(RTP_PRIO_REALTIME | RTP_PRIO_FIFO_BIT)
#define RTP_PRIO_BASE(P)	((P) & ~RTP_PRIO_FIFO_BIT)
#define RTP_PRIO_IS_REALTIME(P) (RTP_PRIO_BASE(P) == RTP_PRIO_REALTIME)
#define RTP_PRIO_NEED_RR(P)	((P) != RTP_PRIO_FIFO)

/* priority range */
#define RTP_PRIO_MIN		0	/* Highest priority */
#define RTP_PRIO_MAX		31	/* Lowest priority */

/*
 * rtprio() syscall functions
 */
#define RTP_LOOKUP		0
#define RTP_SET			1

#ifndef LOCORE
/*
 * Scheduling class information.  This is strictly speaking not only
 * for real-time processes.  We should replace it with two variables:
 * class and priority.  At the moment we use prio here for real-time
 * and interrupt processes, and for others we use proc.p_pri.  FIXME.
 */
struct rtprio {
	u_short type;			/* scheduling class */
	u_short prio;
};
#endif

/*
 * Interrupt thread priorities, after BSD/OS.
 */
#define	PI_REALTIME	 1		/* very high priority (clock) */
#define	PI_AV		 2		/* Audio/video devices */
#define	PI_TTYHIGH	 3		/* High priority tty's (small FIFOs) */
#define	PI_TAPE		 4		/* Tape devices (high for streaming) */
#define	PI_NET		 5		/* Network interfaces */
#define	PI_DISK		 6		/* Disks and SCSI */
#define	PI_TTYLOW	 7		/* Ttys with big buffers */
#define	PI_DISKLOW	 8		/* Disks that do programmed I/O */
#define	PI_DULL		 9		/* We don't know or care */

/* Soft interrupt threads */
#define	PI_SOFT  	15		/* All soft interrupts */

#ifndef _KERNEL
#include <sys/cdefs.h>

__BEGIN_DECLS
int	rtprio __P((int, pid_t, struct rtprio *));
__END_DECLS
#endif	/* !_KERNEL */
#endif	/* !_SYS_RTPRIO_H_ */
