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
/*static char *sccsid = "from: @(#)bindresvport.c 1.8 88/02/08 SMI";*/
/*static char *sccsid = "from: @(#)bindresvport.c	2.2 88/07/29 4.0 RPCSRC";*/
/*from: OpenBSD: bindresvport.c,v 1.7 1996/07/30 16:25:47 downsj Exp */
static char *rcsid = "$FreeBSD$";
#endif

/*
 * Copyright (c) 1987 by Sun Microsystems, Inc.
 *
 * Portions Copyright(C) 1996, Jason Downs.  All rights reserved.
 */

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>

/*
 * Bind a socket to a privileged IP port
 */
int
bindresvport(sd, sin)
	int sd;
	struct sockaddr_in *sin;
{
	struct sockaddr_in myaddr;
	int sinlen = sizeof(struct sockaddr_in);

	if (sin == (struct sockaddr_in *)0) {
		sin = &myaddr;
		memset(sin, 0, sinlen);
		sin->sin_len = sinlen;
		sin->sin_family = AF_INET;
	} else if (sin->sin_family != AF_INET) {
		errno = EPFNOSUPPORT;
		return (-1);
	}

	return (bindresvport2(sd, sin, sinlen));
}

int
bindresvport2(sd, sa, addrlen)
	int sd;
	struct sockaddr *sa;
	socklen_t addrlen;
{
	int on, old, error, level, optname;
	u_short port;

	if (sa == NULL) {
		errno = EINVAL;
		return (-1);
	}
	switch (sa->sa_family) {
	case AF_INET:
		port = ntohs(((struct sockaddr_in *)sa)->sin_port);
		level = IPPROTO_IP;
		optname = IP_PORTRANGE;
		on = IP_PORTRANGE_LOW;
		break;
#ifdef INET6
	case AF_INET6:
		port = ntohs(((struct sockaddr_in6 *)sa)->sin6_port);
		level = IPPROTO_IPV6;
		optname = IPV6_PORTRANGE;
		on = IPV6_PORTRANGE_LOW;
		break;
#endif
	default:
		errno = EAFNOSUPPORT;
		return (-1);
	}

	if (port == 0) {
		int oldlen = sizeof(old);
		error = getsockopt(sd, level, optname, &old, &oldlen);
		if (error < 0)
			return(error);

		error = setsockopt(sd, level, optname, &on, sizeof(on));
		if (error < 0)
			return(error);
	}

	error = bind(sd, sa, addrlen);

	switch (sa->sa_family) {
	case AF_INET:
		port = ntohs(((struct sockaddr_in *)sa)->sin_port);
		break;
#ifdef INET6
	case AF_INET6:
		port = ntohs(((struct sockaddr_in6 *)sa)->sin6_port);
		break;
#endif
	default: /* shoud not match here */
		errno = EAFNOSUPPORT;
		return (-1);
	}
	if (port == 0) {
		int saved_errno = errno;

		if (error) {
			if (setsockopt(sd, level, optname,
			    &old, sizeof(old)) < 0)
				errno = saved_errno;
			return (error);
		}

		/* Hmm, what did the kernel assign... */
		if (getsockname(sd, (struct sockaddr *)sa, &addrlen) < 0)
			errno = saved_errno;
		return (error);
	}
	return (error);
}
