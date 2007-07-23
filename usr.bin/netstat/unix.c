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

#if 0
#ifndef lint
static char sccsid[] = "@(#)unix.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */
#endif

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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
#include <strings.h>
#include <kvm.h>
#include "netstat.h"

static	void unixdomainpr (struct xunpcb *, struct xsocket *);

static	const char *const socktype[] =
    { "#0", "stream", "dgram", "raw", "rdm", "seqpacket" };

static int
pcblist_sysctl(int type, char **bufp)
{
	char 	*buf;
	size_t	len;
	char mibvar[sizeof "net.local.seqpacket.pcblist"];

	sprintf(mibvar, "net.local.%s.pcblist", socktype[type]);

	len = 0;
	if (sysctlbyname(mibvar, 0, &len, 0, 0) < 0) {
		if (errno != ENOENT)
			warn("sysctl: %s", mibvar);
		return (-1);
	}
	if ((buf = malloc(len)) == 0) {
		warnx("malloc %lu bytes", (u_long)len);
		return (-2);
	}
	if (sysctlbyname(mibvar, buf, &len, 0, 0) < 0) {
		warn("sysctl: %s", mibvar);
		free(buf);
		return (-2);
	}
	*bufp = buf;
	return (0);
}

static int
pcblist_kvm(u_long count_off, u_long gencnt_off, u_long head_off, char **bufp)
{
	struct unp_head head;
	struct unpcb *unp, unp_conn;
	u_char sun_len;
	struct socket so;
	struct xunpgen xug;
	struct xunpcb xu;
	unp_gen_t unp_gencnt;
	u_int	unp_count;
	char 	*buf, *p;
	size_t	len;

	if (count_off == 0 || gencnt_off == 0)
		return (-2);
	if (head_off == 0)
		return (-1);
	kread(count_off, &unp_count, sizeof(unp_count));
	len = 2 * sizeof(xug) + (unp_count + unp_count / 8) * sizeof(xu);
	if ((buf = malloc(len)) == 0) {
		warnx("malloc %lu bytes", (u_long)len);
		return (-2);
	}
	p = buf;

#define COPYOUT(obj, size) do {						\
	if (len < (size)) {						\
		warnx("buffer size exceeded");				\
		goto fail;						\
	}								\
	bcopy((obj), p, (size));					\
	len -= (size);							\
	p += (size);							\
} while (0)

#define KREAD(off, buf, len) do {					\
	if (kread((uintptr_t)(off), (buf), (len)) != 0)			\
		goto fail;						\
} while (0)

	/* Write out header. */
	kread(gencnt_off, &unp_gencnt, sizeof(unp_gencnt));
	xug.xug_len = sizeof xug;
	xug.xug_count = unp_count;
	xug.xug_gen = unp_gencnt;
	xug.xug_sogen = 0;
	COPYOUT(&xug, sizeof xug);

	/* Walk the PCB list. */
	xu.xu_len = sizeof xu;
	KREAD(head_off, &head, sizeof(head));
	LIST_FOREACH(unp, &head, unp_link) {
		xu.xu_unpp = unp;
		KREAD(unp, &xu.xu_unp, sizeof (*unp));
		unp = &xu.xu_unp;

		if (unp->unp_gencnt > unp_gencnt)
			continue;
		if (unp->unp_addr != NULL) {
			KREAD(unp->unp_addr, &sun_len, sizeof(sun_len));
			KREAD(unp->unp_addr, &xu.xu_addr, sun_len);
		}
		if (unp->unp_conn != NULL) {
			KREAD(unp->unp_conn, &unp_conn, sizeof(unp_conn));
			if (unp_conn.unp_addr != NULL) {
				KREAD(unp_conn.unp_addr, &sun_len,
				    sizeof(sun_len));
				KREAD(unp_conn.unp_addr, &xu.xu_caddr, sun_len);
			}
		}
		KREAD(unp->unp_socket, &so, sizeof(so));
		if (sotoxsocket(&so, &xu.xu_socket) != 0)
			goto fail;
		COPYOUT(&xu, sizeof(xu));
	}

	/* Reread the counts and write the footer. */
	kread(count_off, &unp_count, sizeof(unp_count));
	kread(gencnt_off, &unp_gencnt, sizeof(unp_gencnt));
	xug.xug_count = unp_count;
	xug.xug_gen = unp_gencnt;
	COPYOUT(&xug, sizeof xug);

	*bufp = buf;
	return (0);

fail:
	free(buf);
	return (-1);
#undef COPYOUT
#undef KREAD
}

void
unixpr(u_long count_off, u_long gencnt_off, u_long dhead_off, u_long shead_off)
{
	char 	*buf;
	int	ret, type;
	struct	xsocket *so;
	struct	xunpgen *xug, *oxug;
	struct	xunpcb *xunp;

	for (type = SOCK_STREAM; type <= SOCK_SEQPACKET; type++) {
		if (live)
			ret = pcblist_sysctl(type, &buf);
		else
			ret = pcblist_kvm(count_off, gencnt_off,
			    type == SOCK_STREAM ? shead_off :
			    (type == SOCK_DGRAM ? dhead_off : 0), &buf);
		if (ret == -1)
			continue;
		if (ret < 0)
			return;

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
unixdomainpr(struct xunpcb *xunp, struct xsocket *so)
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
	printf("%8lx %-6.6s %6u %6u %8lx %8lx %8lx %8lx",
	       (long)so->so_pcb, socktype[so->so_type], so->so_rcv.sb_cc,
	       so->so_snd.sb_cc,
	       (long)unp->unp_vnode, (long)unp->unp_conn,
	       (long)LIST_FIRST(&unp->unp_refs), (long)LIST_NEXT(unp, unp_reflink));
	if (sa)
		printf(" %.*s",
		    (int)(sa->sun_len - offsetof(struct sockaddr_un, sun_path)),
		    sa->sun_path);
	putchar('\n');
}
