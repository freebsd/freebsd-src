/*	$OpenBSD: util.c,v 1.19 2004/07/06 19:49:11 dhartmei Exp $ */

/*
 * Copyright (c) 1996-2001
 *	Obtuse Systems Corporation.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Obtuse Systems nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE OBTUSE SYSTEMS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL OBTUSE
 * SYSTEMS CORPORATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <net/if.h>
#include <net/pfvar.h>

#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sysexits.h>
#include <syslog.h>
#include <unistd.h>

#include "util.h"

extern int ReverseMode;

int Debug_Level;
int Use_Rdns;
in_addr_t Bind_Addr = INADDR_NONE;

void		debuglog(int debug_level, const char *fmt, ...);

void
debuglog(int debug_level, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	if (Debug_Level >= debug_level)
		vsyslog(LOG_DEBUG, fmt, ap);
	va_end(ap);
}

int
get_proxy_env(int connected_fd, struct sockaddr_in *real_server_sa_ptr,
    struct sockaddr_in *client_sa_ptr, struct sockaddr_in *proxy_sa_ptr)
{
	struct pfioc_natlook natlook;
	socklen_t slen;
	int fd;

	slen = sizeof(*proxy_sa_ptr);
	if (getsockname(connected_fd, (struct sockaddr *)proxy_sa_ptr,
	    &slen) != 0) {
		syslog(LOG_ERR, "getsockname() failed (%m)");
		return(-1);
	}
	slen = sizeof(*client_sa_ptr);
	if (getpeername(connected_fd, (struct sockaddr *)client_sa_ptr,
	    &slen) != 0) {
		syslog(LOG_ERR, "getpeername() failed (%m)");
		return(-1);
	}

	if (ReverseMode)
		return(0);

	/*
	 * Build up the pf natlook structure.
	 * Just for IPv4 right now
	 */
	memset((void *)&natlook, 0, sizeof(natlook));
	natlook.af = AF_INET;
	natlook.saddr.addr32[0] = client_sa_ptr->sin_addr.s_addr;
	natlook.daddr.addr32[0] = proxy_sa_ptr->sin_addr.s_addr;
	natlook.proto = IPPROTO_TCP;
	natlook.sport = client_sa_ptr->sin_port;
	natlook.dport = proxy_sa_ptr->sin_port;
	natlook.direction = PF_OUT;

	/*
	 * Open the pf device and lookup the mapping pair to find
	 * the original address we were supposed to connect to.
	 */
	fd = open("/dev/pf", O_RDWR);
	if (fd == -1) {
		syslog(LOG_ERR, "cannot open /dev/pf (%m)");
		exit(EX_UNAVAILABLE);
	}

	if (ioctl(fd, DIOCNATLOOK, &natlook) == -1) {
		syslog(LOG_INFO,
		    "pf nat lookup failed %s:%hu (%m)",
		    inet_ntoa(client_sa_ptr->sin_addr),
		    ntohs(client_sa_ptr->sin_port));
		close(fd);
		return(-1);
	}
	close(fd);

	/*
	 * Now jam the original address and port back into the into
	 * destination sockaddr_in for the proxy to deal with.
	 */
	memset((void *)real_server_sa_ptr, 0, sizeof(struct sockaddr_in));
	real_server_sa_ptr->sin_port = natlook.rdport;
	real_server_sa_ptr->sin_addr.s_addr = natlook.rdaddr.addr32[0];
	real_server_sa_ptr->sin_len = sizeof(struct sockaddr_in);
	real_server_sa_ptr->sin_family = AF_INET;
	return(0);
}


/*
 * Transfer one unit of data across a pair of sockets
 *
 * A unit of data is as much as we get with a single read(2) call.
 */
int
xfer_data(const char *what_read,int from_fd, int to_fd, struct in_addr from,
    struct in_addr to)
{
	int rlen, offset, xerrno, mark, flags = 0;
	char tbuf[4096];

	/*
	 * Are we at the OOB mark?
	 */
	if (ioctl(from_fd, SIOCATMARK, &mark) < 0) {
		xerrno = errno;
		syslog(LOG_ERR, "cannot ioctl(SIOCATMARK) socket from %s (%m)",
		    what_read);
		errno = xerrno;
		return(-1);
	}
	if (mark)
		flags = MSG_OOB;	/* Yes - at the OOB mark */

snarf:
	rlen = recv(from_fd, tbuf, sizeof(tbuf), flags);
	if (rlen == -1 && flags == MSG_OOB && errno == EINVAL) {
		/* OOB didn't work */
		flags = 0;
		rlen = recv(from_fd, tbuf, sizeof(tbuf), flags);
	}
	if (rlen == 0) {
		debuglog(3, "EOF on read socket");
		return(0);
	} else if (rlen == -1) {
		if (errno == EAGAIN || errno == EINTR)
			goto snarf;
		xerrno = errno;
		syslog(LOG_ERR, "xfer_data (%s): failed (%m) with flags 0%o",
		    what_read, flags);
		errno = xerrno;
		return(-1);
	} else {
		offset = 0;
		debuglog(3, "got %d bytes from socket", rlen);

		while (offset < rlen) {
			int wlen;
		fling:
			wlen = send(to_fd, &tbuf[offset], rlen - offset,
			    flags);
			if (wlen == 0) {
				debuglog(3, "zero-length write");
				goto fling;
			} else if (wlen == -1) {
				if (errno == EAGAIN || errno == EINTR)
					goto fling;
				xerrno = errno;
				syslog(LOG_INFO, "write failed (%m)");
				errno = xerrno;
				return(-1);
			} else {
				debuglog(3, "wrote %d bytes to socket",wlen);
				offset += wlen;
			}
		}
		return(offset);
	}
}

/*
 * get_backchannel_socket gets us a socket bound somewhere in a
 * particular range of ports
 */
int
get_backchannel_socket(int type, int min_port, int max_port, int start_port,
    int direction, struct sockaddr_in *sap)
{
	int count;

	/*
	 * Make sure that direction is 'defined' and that min_port is not
	 * greater than max_port.
	 */
	if (direction != -1)
		direction = 1;

	/* by default we go up by one port until we find one */
	if (min_port > max_port) {
		errno = EINVAL;
		return(-1);
	}

	count = 1 + max_port - min_port;

	/*
	 * Pick a port we can bind to from within the range we want.
	 * If the caller specifies -1 as the starting port number then
	 * we pick one somewhere in the range to try.
	 * This is an optimization intended to speedup port selection and
	 * has NOTHING to do with security.
	 */
	if (start_port == -1)
		start_port = (arc4random() % count) + min_port;

	if (start_port < min_port || start_port > max_port) {
		errno = EINVAL;
		return(-1);
	}

	while (count-- > 0) {
		struct sockaddr_in sa;
		int one, fd;

		fd = socket(AF_INET, type, 0);

		bzero(&sa, sizeof sa);
		sa.sin_family = AF_INET;
		if (Bind_Addr == INADDR_NONE)
			if (sap == NULL)
				sa.sin_addr.s_addr = INADDR_ANY;
			else
				sa.sin_addr.s_addr = sap->sin_addr.s_addr;
		else
			sa.sin_addr.s_addr = Bind_Addr;

		/*
		 * Indicate that we want to reuse a port if it happens that the
		 * port in question was a listen port recently.
		 */
		one = 1;
		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one,
		    sizeof(one))  == -1)
			return(-1);

		sa.sin_port = htons(start_port);

		if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) == 0) {
			if (sap != NULL)
				*sap = sa;
			return(fd);
		}

		if (errno != EADDRINUSE)
			return(-1);

		/* if it's in use, try the next port */
		close(fd);

		start_port += direction;
		if (start_port < min_port)
			start_port = max_port;
		else if (start_port > max_port)
			start_port = min_port;
	}
	errno = EAGAIN;
	return(-1);
}
