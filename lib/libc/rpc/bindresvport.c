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
static char *rcsid = "$Id: bindresvport.c,v 1.5 1996/08/12 14:09:46 peter Exp $";
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
	int on, old, error;
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

	if (sin->sin_port == 0) {
		int oldlen = sizeof(old);
		error = getsockopt(sd, IPPROTO_IP, IP_PORTRANGE,
				   &old, &oldlen);
		if (error < 0)
			return(error);

		on = IP_PORTRANGE_LOW;
		error = setsockopt(sd, IPPROTO_IP, IP_PORTRANGE,
		           	   &on, sizeof(on));
		if (error < 0)
			return(error);
	}

	error = bind(sd, (struct sockaddr *)sin, sinlen);

	if (sin->sin_port == 0) {
		int saved_errno = errno;

		if (error) {
			if (setsockopt(sd, IPPROTO_IP, IP_PORTRANGE,
			    &old, sizeof(old)) < 0)
				errno = saved_errno;
			return (error);
		}

		if (sin != &myaddr) {
			/* Hmm, what did the kernel assign... */
			if (getsockname(sd, (struct sockaddr *)sin,
			    &sinlen) < 0)
				errno = saved_errno;
			return (error);
		}
	}
	return (error);
}
