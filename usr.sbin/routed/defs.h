/*
 * Copyright (c) 1983, 1988, 1993
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
 *	@(#)defs.h	8.1 (Berkeley) 6/5/93
 */

/*
 * Internal data structure definitions for
 * user routing process.  Based on Xerox NS
 * protocol specs with mods relevant to more
 * general addressing scheme.
 */
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <net/route.h>
#include <netinet/in.h>
#include <protocols/routed.h>

#include <stdio.h>
#include <netdb.h>

#include "trace.h"
#include "interface.h"
#include "table.h"
#include "af.h"

/*
 * When we find any interfaces marked down we rescan the
 * kernel every CHECK_INTERVAL seconds to see if they've
 * come up.
 */
#define	CHECK_INTERVAL	(1*60)

#define equal(a1, a2) \
	(bcmp((caddr_t)(a1), (caddr_t)(a2), sizeof (struct sockaddr)) == 0)

struct	sockaddr_in addr;	/* address of daemon's socket */

int	s;			/* source and sink of all data */
int	r;			/* routing socket */
pid_t	pid;			/* process id for identifying messages */
uid_t	uid;			/* user id for identifying messages */
int	seqno;			/* sequence number for identifying messages */
int	kmem;
int	supplier;		/* process should supply updates */
int	install;		/* if 1 call kernel */
int	lookforinterfaces;	/* if 1 probe kernel for new up interfaces */
int	performnlist;		/* if 1 check if /vmunix has changed */
int	externalinterfaces;	/* # of remote and local interfaces */
struct	timeval now;		/* current idea of time */
struct	timeval lastbcast;	/* last time all/changes broadcast */
struct	timeval lastfullupdate;	/* last time full table broadcast */
struct	timeval nextbcast;	/* time to wait before changes broadcast */
int	needupdate;		/* true if we need update at nextbcast */

char	packet[MAXPACKETSIZE+1];
struct	rip *msg;

char	**argv0;
struct	servent *sp;

struct	in_addr inet_makeaddr();
int	inet_addr();
int	inet_maskof();
int	sndmsg();
int	supply();
int	cleanup();

int	rtioctl();
#define ADD 1
#define DELETE 2
#define CHANGE 3
