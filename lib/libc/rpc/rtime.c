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

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.

 */

/*
 * rtime - get time from remote machine
 *
 * gets time, obtaining value from host
 * on the udp/time socket.  Since timeserver returns
 * with time of day in seconds since Jan 1, 1900,  must
 * subtract seconds before Jan 1, 1970 to get
 * what unix uses.
 */
#include "namespace.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <stdio.h>
#include <netdb.h>
#include "un-namespace.h"

#if defined(LIBC_SCCS) && !defined(lint)
/* from: static char sccsid[] = 	"@(#)rtime.c	2.2 88/08/10 4.0 RPCSRC; from 1.8 88/02/08 SMI"; */
static const char rcsid[] = "$FreeBSD$";
#endif

extern int _rpc_dtablesize( void );

#define NYEARS	(unsigned long)(1970 - 1900)
#define TOFFSET (unsigned long)(60*60*24*(365*NYEARS + (NYEARS/4)))

static void do_close( int );

int
rtime(addrp, timep, timeout)
	struct sockaddr_in *addrp;
	struct timeval *timep;
	struct timeval *timeout;
{
	int s;
	fd_set readfds;
	int res;
	unsigned long thetime;
	struct sockaddr_in from;
	int fromlen;
	int type;
	struct servent *serv;

	if (timeout == NULL) {
		type = SOCK_STREAM;
	} else {
		type = SOCK_DGRAM;
	}
	s = _socket(AF_INET, type, 0);
	if (s < 0) {
		return(-1);
	}
	addrp->sin_family = AF_INET;

	/* TCP and UDP port are the same in this case */
	if ((serv = getservbyname("time", "tcp")) == NULL) {
		return(-1);
	}

	addrp->sin_port = serv->s_port;

	if (type == SOCK_DGRAM) {
		res = _sendto(s, (char *)&thetime, sizeof(thetime), 0, 
			     (struct sockaddr *)addrp, sizeof(*addrp));
		if (res < 0) {
			do_close(s);
			return(-1);	
		}
		do {
			FD_ZERO(&readfds);
			FD_SET(s, &readfds);
			res = _select(_rpc_dtablesize(), &readfds,
				     (fd_set *)NULL, (fd_set *)NULL, timeout);
		} while (res < 0 && errno == EINTR);
		if (res <= 0) {
			if (res == 0) {
				errno = ETIMEDOUT;
			}
			do_close(s);
			return(-1);	
		}
		fromlen = sizeof(from);
		res = _recvfrom(s, (char *)&thetime, sizeof(thetime), 0, 
			       (struct sockaddr *)&from, &fromlen);
		do_close(s);
		if (res < 0) {
			return(-1);	
		}
	} else {
		if (_connect(s, (struct sockaddr *)addrp, sizeof(*addrp)) < 0) {
			do_close(s);
			return(-1);
		}
		res = _read(s, (char *)&thetime, sizeof(thetime));
		do_close(s);
		if (res < 0) {
			return(-1);
		}
	}
	if (res != sizeof(thetime)) {
		errno = EIO;
		return(-1);	
	}
	thetime = ntohl(thetime);
	timep->tv_sec = thetime - TOFFSET;
	timep->tv_usec = 0;
	return(0);
}

static void
do_close(s)
	int s;
{
	int save;

	save = errno;
	(void)_close(s);
	errno = save;
}
