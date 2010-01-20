/*-
 * Copyright (c) 1985, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)globals.h	8.1 (Berkeley) 6/6/93
 *	$FreeBSD$
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <protocols/timed.h>
#define	SECHR	(60*60)
#define	SECDAY	(24*SECHR)

/* Best expected round trip for a measurement.
 * This is essentially the number of milliseconds per CPU tick (CLK_TCK?).
 * All delays shorter than this are usually reported as 0.
 */
#define MIN_ROUND ((1000-1)/CLK_TCK)


#define SAMPLEINTVL	240		/* synch() freq for master in sec */
#define	MAXADJ		20		/* max adjtime() correction in sec */

#define MAX_TRIM	3000000		/* max drift in nsec/sec, 0.3% */
#define BIG_ADJ		(MAX_TRIM/1000*SAMPLEINTVL*2)	/* max good adj */

#define MINTOUT		360		/* election delays, 6-15 minutes */
#define MAXTOUT		900

#define BAD_STATUS	(-1)
#define GOOD		1
#define UNREACHABLE	2
#define NONSTDTIME	3
#define HOSTDOWN	0x7fffffff

#define OFF		0
#define ON		1

#define MAX_HOPCNT	10		/* max value for tsp_hpcnt */

#define LOSTHOST	3		/* forget after this many failures */

#define VALID_RANGE (MAXADJ*1000)	/* good times in milliseconds */
#define GOOD_RANGE (MIN_ROUND*2)
#define VGOOD_RANGE (MIN_ROUND-1)


/*
 * Global and per-network states.
 */
#define NOMASTER	0		/* no good master */
#define SLAVE		1
#define MASTER		2
#define IGNORE		4
#define ALL		(SLAVE|MASTER|IGNORE)
#define SUBMASTER	(SLAVE|MASTER)

#define NHOSTS		1013		/* max of hosts controlled by timed
					 * This must be a prime number.
					 */
struct hosttbl {
	struct	hosttbl *h_bak;		/* hash chain */
	struct	hosttbl *h_fwd;
	struct  hosttbl *l_bak;		/* "sequential" list */
	struct  hosttbl *l_fwd;
	struct	netinfo *ntp;
	struct	sockaddr_in addr;
	char	name[MAXHOSTNAMELEN];
	u_char	head;			/* 1=head of hash chain */
	u_char	good;			/* 0=trusted host, for averaging */
	u_char	noanswer;		/* count of failures to answer */
	u_char	need_set;		/* need a SETTIME */
	u_short seq;
	long	delta;
};

/* closed hash table with internal chaining */
extern struct hosttbl hosttbl[NHOSTS+1];
#define self hosttbl[0]
#define hostname (self.name)


struct netinfo {
	struct	netinfo *next;
	struct	in_addr net;
	u_int32_t	mask;
	struct	in_addr my_addr;
	struct	sockaddr_in dest_addr;	/* broadcast addr or point-point */
	long	status;
	struct timeval slvwait;		/* delay before sending our time */
	int	quit_count;		/* recent QUITs */
};

#include "extern.h"

#define tvtomsround(tv) ((tv).tv_sec*1000 + ((tv).tv_usec + 500)/1000)

extern struct netinfo *nettab;
extern int status;
extern int trace;
extern int sock;
extern struct sockaddr_in from;
extern struct timeval from_when;	/* when the last msg arrived */
extern u_short sequence;		/* TSP message sequence number */
extern struct netinfo *fromnet, *slavenet;
extern FILE *fd;
extern long delay1, delay2;
extern int nslavenets;			/* nets were I could be a slave */
extern int nmasternets;			/* nets were I could be a master */
extern int nignorednets;		/* ignored nets */
extern int nnets;			/* nets I am connected to */


#define trace_msg(msg)		{if (trace) fprintf(fd, msg);}

#define trace_sendto_err(addr) {					\
	int st_errno = errno;						\
	syslog(LOG_ERR, "%s %d: sendto %s: %m",				\
		__FILE__, __LINE__, inet_ntoa(addr));			\
	if (trace)							\
		fprintf(fd, "%s %d: sendto %s: %d", __FILE__, __LINE__,	\
			inet_ntoa(addr), st_errno);			\
}


# define max(a,b) 	(a<b ? b : a)
# define min(a,b) 	(a>b ? b : a)
# define abs(x)		(x>=0 ? x : -(x))
