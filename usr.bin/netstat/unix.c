/*-
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
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)unix.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] =
  "$FreeBSD: src/usr.bin/netstat/unix.c,v 1.12 1999/08/28 01:04:31 peter Exp $";
#endif /* not lint */

/*
 * Display protocol blocks in the unix domain.
 */
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/mbuf.h>
#include <sys/sysctl.h>
#include <sys/un.h>
#include <sys/unpcb.h>

#include <netinet/in.h>

#include <errno.h>
#include <err.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <kvm.h>
#include "netstat.h"

static	void unixdomainpr __P((struct xunpcb *, struct xsocket *));

static	const char *const socktype[] =
    { "#0", "stream", "dgram", "raw", "rdm", "seqpacket" };

void
unixpr()
{
	char 	*buf;
	int	type;
	size_t	len;
	struct	xsocket *so;
	struct	xunpgen *xug, *oxug;
	struct	xunpcb *xunp;
	char mibvar[sizeof "net.local.seqpacket.pcblist"];

	for (type = SOCK_STREAM; type <= SOCK_SEQPACKET; type++) {
		sprintf(mibvar, "net.local.%s.pcblist", socktype[type]);

		len = 0;
		if (sysctlbyname(mibvar, 0, &len, 0, 0) < 0) {
			if (errno != ENOENT)
				warn("sysctl: %s", mibvar);
			continue;
		}
		if ((buf = malloc(len)) == 0) {
			warn("malloc %lu bytes", (u_long)len);
			return;
		}
		if (sysctlbyname(mibvar, buf, &len, 0, 0) < 0) {
			warn("sysctl: %s", mibvar);
			free(buf);
			return;
		}

		oxug = xug = (struct xunpgen *)buf;
		for (xug = (struct xunpgen *)((char *)xug + xug->xug_len);
		     xug->xug_len > sizeof(struct xunpgen);
		     xug = (struct xunpgen *)((char *)xug + xug->xug_len)) {
			xunp = (struct xunpcb *)xug;
			so = &xunp->xu_socket;

			/* Ignore PCBs which were freed during copyout. */
			if (xunp->xu_unp.unp_gencnt > oxug->xug_gen)
				continue;
			unixdomainpr(xunp, so);
		}
		if (xug != oxug && xug->xug_gen != oxug->xug_gen) {
			if (oxug->xug_count > xug->xug_count) {
				printf("Some %s sockets may have been deleted.\n",
				       socktype[type]);
			} else if (oxug->xug_count < xug->xug_count) {
				printf("Some %s sockets may have been created.\n",
			       socktype[type]);
			} else {
				printf("Some %s sockets may have been created or deleted",
			       socktype[type]);
			}
		}
		free(buf);
	}
}

static void
unixdomainpr(xunp, so)
	struct xunpcb *xunp;
	struct xsocket *so;
{
	struct unpcb *unp;
	struct sockaddr_un *sa;
	static int first = 1;

	unp = &xunp->xu_unp;
	if (unp->unp_addr)
		sa = &xunp->xu_addr;
	else
		sa = (struct sockaddr_un *)0;

	if (first) {
		printf("Active UNIX domain sockets\n");
		printf(
"%-8.8s %-6.6s %-6.6s %-6.6s %8.8s %8.8s %8.8s %8.8s Addr\n",
		    "Address", "Type", "Recv-Q", "Send-Q",
		    "Inode", "Conn", "Refs", "Nextref");
		first = 0;
	}
	printf("%8lx %-6.6s %6ld %6ld %8lx %8lx %8lx %8lx",
	       (long)so->so_pcb, socktype[so->so_type], so->so_rcv.sb_cc,
	       so->so_snd.sb_cc,
	       (long)unp->unp_vnode, (long)unp->unp_conn,
	       (long)unp->unp_refs.lh_first, (long)unp->unp_reflink.le_next);
	if (sa)
		printf(" %.*s",
		    (int)(sa->sun_len - offsetof(struct sockaddr_un, sun_path)),
		    sa->sun_path);
	putchar('\n');
}
