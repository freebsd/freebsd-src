/*	$NetBSD: bindresvport.c,v 1.19 2000/07/06 03:03:59 christos Exp $	*/

/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 *
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 *
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 *
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 *
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char *sccsid2 = "from: @(#)bindresvport.c 1.8 88/02/08 SMI";
static char *sccsid = "from: @(#)bindresvport.c	2.2 88/07/29 4.0 RPCSRC";
#endif
/* from: $OpenBSD: bindresvport.c,v 1.7 1996/07/30 16:25:47 downsj Exp $ */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Copyright (c) 1987 by Sun Microsystems, Inc.
 *
 * Portions Copyright(C) 1996, Jason Downs.  All rights reserved.
 */

#include "namespace.h"
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <rpc/rpc.h>

#include <string.h>
#include "un-namespace.h"

/*
 * Bind a socket to a privileged IP port
 */
int
bindresvport(sd, sin)
	int sd;
	struct sockaddr_in *sin;
{
	return bindresvport_sa(sd, (struct sockaddr *)sin);
}

/*
 * Bind a socket to a privileged IP port
 */
int
bindresvport_sa(sd, sa)
	int sd;
	struct sockaddr *sa;
{
	int old, error, af;
	struct sockaddr_storage myaddr;
	struct sockaddr_in *sin;
#ifdef INET6
	struct sockaddr_in6 *sin6;
#endif
	int proto, portrange, portlow;
	u_int16_t *portp;
	socklen_t salen;

	if (sa == NULL) {
		salen = sizeof(myaddr);
		sa = (struct sockaddr *)&myaddr;

		if (_getsockname(sd, sa, &salen) == -1)
			return -1;	/* errno is correctly set */

		af = sa->sa_family;
		memset(sa, 0, salen);
	} else
		af = sa->sa_family;

	switch (af) {
	case AF_INET:
		proto = IPPROTO_IP;
		portrange = IP_PORTRANGE;
		portlow = IP_PORTRANGE_LOW;
		sin = (struct sockaddr_in *)sa;
		salen = sizeof(struct sockaddr_in);
		portp = &sin->sin_port;
		break;
#ifdef INET6
	case AF_INET6:
		proto = IPPROTO_IPV6;
		portrange = IPV6_PORTRANGE;
		portlow = IPV6_PORTRANGE_LOW;
		sin6 = (struct sockaddr_in6 *)sa;
		salen = sizeof(struct sockaddr_in6);
		portp = &sin6->sin6_port;
		break;
#endif
	default:
		errno = EPFNOSUPPORT;
		return (-1);
	}
	sa->sa_family = af;
	sa->sa_len = salen;

	if (*portp == 0) {
		socklen_t oldlen = sizeof(old);

		error = _getsockopt(sd, proto, portrange, &old, &oldlen);
		if (error < 0)
			return (error);

		error = _setsockopt(sd, proto, portrange, &portlow,
		    sizeof(portlow));
		if (error < 0)
			return (error);
	}

	error = _bind(sd, sa, salen);

	if (*portp == 0) {
		int saved_errno = errno;

		if (error < 0) {
			if (_setsockopt(sd, proto, portrange, &old,
			    sizeof(old)) < 0)
				errno = saved_errno;
			return (error);
		}

		if (sa != (struct sockaddr *)&myaddr) {
			/* Hmm, what did the kernel assign? */
			if (_getsockname(sd, sa, &salen) < 0)
				errno = saved_errno;
			return (error);
		}
	}
	return (error);
}
