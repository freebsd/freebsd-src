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
 *	$Id: rtprio.h,v 1.4 1997/02/22 09:45:48 peter Exp $
 */

#ifndef _SYS_RTPRIO_H_
#define _SYS_RTPRIO_H_

/*
 * Process realtime-priority specifications to rtprio.
 */

/* priority types */

#define RTP_PRIO_REALTIME	0
#define RTP_PRIO_NORMAL		1
#define RTP_PRIO_IDLE		2

/* RTP_PRIO_FIFO is Posix 4 SCHED_FIFO.
 * Careful: These are based on the kernel config POSIX4 and not
 * the compile time test _POSIX_PRIORITY_SCHEDULING since they
 * set the behavior of the system.
 */

#ifdef POSIX4
#define RTP_PRIO_FIFO_BIT	4
#define RTP_PRIO_FIFO		(RTP_PRIO_REALTIME | RTP_PRIO_FIFO_BIT)
#define RTP_PRIO_BASE(P)	((P) & ~RTP_PRIO_FIFO_BIT)
#define RTP_PRIO_IS_REALTIME(P) (RTP_PRIO_BASE(P) == RTP_PRIO_REALTIME)
#define RTP_PRIO_NEED_RR(P)	((P) != RTP_PRIO_FIFO)
#else
#define RTP_PRIO_BASE(P)	(P)
#define RTP_PRIO_IS_REALTIME(P)	(P == RTP_PRIO_REALTIME)
#define RTP_PRIO_NEED_RR(P)	(1)
#endif

/* priority range */
#define RTP_PRIO_MIN		0	/* Highest priority */
#define RTP_PRIO_MAX		31	/* Lowest priority */

/*
 * rtprio() syscall functions
 */
#define RTP_LOOKUP		0
#define RTP_SET			1

#ifndef LOCORE
struct rtprio {
	u_short type;
	u_short prio;
};
#endif

#ifndef KERNEL
#include <sys/cdefs.h>

__BEGIN_DECLS
int	rtprio __P((int, pid_t, struct rtprio *));
__END_DECLS
#endif	/* !KERNEL */
#endif	/* !_SYS_RTPRIO_H_ */
