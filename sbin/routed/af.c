/*
 * Copyright (c) 1983 Regents of the University of California.
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
 */

#ifndef lint
static char sccsid[] = "@(#)af.c	5.11 (Berkeley) 2/28/91";
#endif /* not lint */

#include "defs.h"

/*
 * Address family support routines
 */
int	inet_hash(), inet_netmatch(), inet_output(),
	inet_portmatch(), inet_portcheck(),
	inet_checkhost(), inet_rtflags(), inet_sendroute(), inet_canon();
char	*inet_format();

#define NIL	{ 0 }
#define	INET \
	{ inet_hash,		inet_netmatch,		inet_output, \
	  inet_portmatch,	inet_portcheck,		inet_checkhost, \
	  inet_rtflags,		inet_sendroute,		inet_canon, \
	  inet_format \
	}

struct afswitch afswitch[AF_MAX] = {
	NIL,		/* 0- unused */
	NIL,		/* 1- Unix domain, unused */
	INET,		/* Internet */
};

int af_max = sizeof(afswitch) / sizeof(afswitch[0]);

struct sockaddr_in inet_default = {
#ifdef RTM_ADD
	sizeof (inet_default),
#endif
	AF_INET, INADDR_ANY };

inet_hash(sin, hp)
	register struct sockaddr_in *sin;
	struct afhash *hp;
{
	register u_long n;

	n = inet_netof(sin->sin_addr);
	if (n)
	    while ((n & 0xff) == 0)
		n >>= 8;
	hp->afh_nethash = n;
	hp->afh_hosthash = ntohl(sin->sin_addr.s_addr);
	hp->afh_hosthash &= 0x7fffffff;
}

inet_netmatch(sin1, sin2)
	struct sockaddr_in *sin1, *sin2;
{

	return (inet_netof(sin1->sin_addr) == inet_netof(sin2->sin_addr));
}

/*
 * Verify the message is from the right port.
 */
inet_portmatch(sin)
	register struct sockaddr_in *sin;
{
	
	return (sin->sin_port == sp->s_port);
}

/*
 * Verify the message is from a "trusted" port.
 */
inet_portcheck(sin)
	struct sockaddr_in *sin;
{

	return (ntohs(sin->sin_port) <= IPPORT_RESERVED);
}

/*
 * Internet output routine.
 */
inet_output(s, flags, sin, size)
	int s, flags;
	struct sockaddr_in *sin;
	int size;
{
	struct sockaddr_in dst;

	dst = *sin;
	sin = &dst;
	if (sin->sin_port == 0)
		sin->sin_port = sp->s_port;
	if (sin->sin_len == 0)
		sin->sin_len = sizeof (*sin);
	if (sendto(s, packet, size, flags,
	    (struct sockaddr *)sin, sizeof (*sin)) < 0)
		perror("sendto");
}

/*
 * Return 1 if the address is believed
 * for an Internet host -- THIS IS A KLUDGE.
 */
inet_checkhost(sin)
	struct sockaddr_in *sin;
{
	u_long i = ntohl(sin->sin_addr.s_addr);

#ifndef IN_EXPERIMENTAL
#define	IN_EXPERIMENTAL(i)	(((long) (i) & 0xe0000000) == 0xe0000000)
#endif

	if (IN_EXPERIMENTAL(i) || sin->sin_port != 0)
		return (0);
	if (i != 0 && (i & 0xff000000) == 0)
		return (0);
	for (i = 0; i < sizeof(sin->sin_zero)/sizeof(sin->sin_zero[0]); i++)
		if (sin->sin_zero[i])
			return (0);
	return (1);
}

inet_canon(sin)
	struct sockaddr_in *sin;
{

	sin->sin_port = 0;
	sin->sin_len = sizeof(*sin);
}

char *
inet_format(sin)
	struct sockaddr_in *sin;
{
	char *inet_ntoa();

	return (inet_ntoa(sin->sin_addr));
}
