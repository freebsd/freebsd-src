/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Copyright (c) 1995 John Hay.  All rights reserved.
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
 *	@(#)defs.h	8.1 (Berkeley) 6/5/93
 *
 *	$FreeBSD$
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <net/route.h>
#include <netipx/ipx.h>
#if defined(vax) || defined(pdp11)
#define xnnet(x) ((u_long) (x)->rip_dst[1] << 16 | (u_long) (x)->rip_dst[0] )
#else
#define xnnet(x) ((u_long) (x)->rip_dst[0] << 16 | (u_long) (x)->rip_dst[1] )
#endif

#include <stdio.h>
#include <string.h>
#include <syslog.h>

#include "protocol.h"
#include "sap.h"
#include "table.h"
#include "trace.h"
#include "interface.h"
#include "af.h"


/*
 * When we find any interfaces marked down we rescan the
 * kernel every CHECK_INTERVAL seconds to see if they've
 * come up.
 */
#define	CHECK_INTERVAL	(5*60)

#define equal(a1, a2) \
	(bcmp((caddr_t)(a1), (caddr_t)(a2), sizeof (struct sockaddr)) == 0)
#define	min(a,b)	((a)>(b)?(b):(a))
#define	max(a,b)	((a)<(b)?(b):(a))

extern int	ripsock;		/* Socket to listen on */
extern int	sapsock;		/* Socket to listen on */
extern int	kmem;
extern int	supplier;		/* process should supply updates */
extern int	dosap;			/* SAP is enabled */
extern int	install;		/* if 1 call kernel */
extern int	lookforinterfaces;	/* if 1 probe kernel for new up ifs */
extern int	performnlist;		/* if 1 check if /kernel has changed */
extern int	externalinterfaces;	/* # of remote and local interfaces */
extern int	timeval;		/* local idea of time */
extern int	noteremoterequests;	/* squawk on requests from non-local nets */
extern int	r;			/* Routing socket to install updates with */
extern int gateway;
extern struct	sockaddr_ipx ipx_netmask;	/* Used in installing routes */

extern char	packet[MAXRXPACKETSIZE+1];
extern struct	rip *msg;

extern char	**argv0;

#define	ADD	1
#define	DELETE	2
#define CHANGE	3

void	sndmsg(struct sockaddr *, int, struct interface *);
void	supply(struct sockaddr *, int, struct interface *);
void	addrouteforif(struct interface *);
void	ifinit(void);
void	toall(void (*f)(struct sockaddr *, int, struct interface *),
	      struct rt_entry *);
void	rip_input(struct sockaddr *, int);

