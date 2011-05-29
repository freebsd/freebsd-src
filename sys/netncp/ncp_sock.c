/*-
 * Copyright (c) 1999, 2001 Boris Popov
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Low level socket routines
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/kernel.h>
#include <sys/uio.h>
#include <sys/syslog.h>
#include <sys/mbuf.h>
#include <sys/condvar.h>
#include <net/route.h>

#include <netipx/ipx.h>
#include <netipx/ipx_pcb.h>

#include <netncp/ncp.h>
#include <netncp/ncp_conn.h>
#include <netncp/ncp_sock.h>
#include <netncp/ncp_subr.h>
#include <netncp/ncp_rq.h>

#define ipx_setnullnet(x) ((x).x_net.s_net[0]=0); ((x).x_net.s_net[1]=0);
#define ipx_setnullhost(x) ((x).x_host.s_host[0] = 0); \
	((x).x_host.s_host[1] = 0); ((x).x_host.s_host[2] = 0);

/*static int ncp_getsockname(struct socket *so, caddr_t asa, int *alen);*/
static int ncp_soconnect(struct socket *so, struct sockaddr *target,
			 struct thread *td);


/* This will need only if native IP used, or (unlikely) NCP will be
 * implemented on the socket level
 */
static int
ncp_soconnect(struct socket *so, struct sockaddr *target, struct thread *td)
{
	int error, s;

	error = soconnect(so, (struct sockaddr*)target, td);
	if (error)
		return error;
	/*
	 * Wait for the connection to complete. Cribbed from the
	 * connect system call but with the wait timing out so
	 * that interruptible mounts don't hang here for a long time.
	 */
	error = EIO;
	s = splnet();
	while ((so->so_state & SS_ISCONNECTING) && so->so_error == 0) {
		(void) tsleep((caddr_t)&so->so_timeo, PSOCK, "ncpcon", 2 * hz);
		if ((so->so_state & SS_ISCONNECTING) &&
		    so->so_error == 0 /*&& rep &&*/) {
			so->so_state &= ~SS_ISCONNECTING;
			splx(s);
			goto bad;
		}
	}
	if (so->so_error) {
		error = so->so_error;
		so->so_error = 0;
		splx(s);
		goto bad;
	}
		splx(s);
	error=0;
bad:
	return error;
}
#ifdef notyet
static int
ncp_getsockname(struct socket *so, caddr_t asa, int *alen) {
	struct sockaddr *sa;
	int len=0, error;

	sa = 0;
	error = (*so->so_proto->pr_usrreqs->pru_sockaddr)(so, &sa);
	if (error==0) {
		if (sa) {
			len = min(len, sa->sa_len);
			bcopy(sa, (caddr_t)asa, (u_int)len);
		}
		*alen=len;
	}
	if (sa)
		free(sa, M_SONAME);
	return (error);
}
#endif
int ncp_sock_recv(struct socket *so, struct mbuf **mp, int *rlen)
{
	struct uio auio;
	struct thread *td = curthread; /* XXX */
	int error,flags,len;

	auio.uio_resid = len = 1000000;
	auio.uio_td = td;
	flags = MSG_DONTWAIT;

/*	error = soreceive(so, 0, &auio, (struct mbuf **)0, (struct mbuf **)0,
	    &flags);*/
	error = soreceive(so, 0, &auio, mp, (struct mbuf **)0, &flags);
	*rlen = len - auio.uio_resid;
/*	if (!error) {
	    *rlen=iov.iov_len;
	} else
	    *rlen=0;*/
#ifdef NCP_SOCKET_DEBUG
	if (error)
		printf("ncp_recv: err=%d\n", error);
#endif
	return (error);
}

int
ncp_sock_send(struct socket *so, struct mbuf *top, struct ncp_rq *rqp)
{
	struct thread *td = curthread; /* XXX */
	struct sockaddr *to = 0;
	struct ncp_conn *conn = rqp->nr_conn;
	struct mbuf *m;
	int error, flags=0;

	for (;;) {
		m = m_copym(top, 0, M_COPYALL, M_WAIT);
/*		NCPDDEBUG(m);*/
		error = sosend(so, to, 0, m, 0, flags, td);
		if (error == 0 || error == EINTR || error == ENETDOWN)
			break;
		if (rqp->rexmit == 0) break;
		rqp->rexmit--;
		pause("ncprsn", conn->li.timeout * hz);
		error = ncp_chkintr(conn, td);
		if (error == EINTR) break;
	}
	if (error) {
		log(LOG_INFO, "ncp_send: error %d for server %s", error, conn->li.server);
	}
	return error;
}

/*
 * Connect to specified server via IPX
 */
static int
ncp_sock_connect_ipx(struct ncp_conn *conn)
{
	struct sockaddr_ipx sipx;
	struct ipxpcb *npcb;
	struct thread *td = conn->td;
	int addrlen, error, count;

	sipx.sipx_port = htons(0);

	for (count = 0;;count++) {
		if (count > (IPXPORT_WELLKNOWN-IPXPORT_RESERVED)*2) {
			error = EADDRINUSE;
			goto bad;
		}
		conn->ncp_so = conn->wdg_so = NULL;
		checkbad(socreate(AF_IPX, &conn->ncp_so, SOCK_DGRAM, 0, td->td_ucred, td));
		if (conn->li.opt & NCP_OPT_WDOG)
			checkbad(socreate(AF_IPX, &conn->wdg_so, SOCK_DGRAM, 0, td->td_ucred, td));
		addrlen = sizeof(sipx);
		sipx.sipx_family = AF_IPX;
		ipx_setnullnet(sipx.sipx_addr);
		ipx_setnullhost(sipx.sipx_addr);
		sipx.sipx_len = addrlen;
		error = sobind(conn->ncp_so, (struct sockaddr *)&sipx, td);
		if (error == 0) {
			if ((conn->li.opt & NCP_OPT_WDOG) == 0)
				break;
			sipx.sipx_addr = sotoipxpcb(conn->ncp_so)->ipxp_laddr;
			sipx.sipx_port = htons(ntohs(sipx.sipx_port) + 1);
			ipx_setnullnet(sipx.sipx_addr);
			ipx_setnullhost(sipx.sipx_addr);
			error = sobind(conn->wdg_so, (struct sockaddr *)&sipx, td);
		}
		if (!error) break;
		if (error != EADDRINUSE) goto bad;
		sipx.sipx_port = htons((ntohs(sipx.sipx_port)+4) & 0xfff8);
		soclose(conn->ncp_so);
		if (conn->wdg_so)
			soclose(conn->wdg_so);
	}
	npcb = sotoipxpcb(conn->ncp_so);
	npcb->ipxp_dpt = IPXPROTO_NCP;
	/* IPXrouted must be running, i.e. route must be presented */
	conn->li.ipxaddr.sipx_len = sizeof(struct sockaddr_ipx);
	checkbad(ncp_soconnect(conn->ncp_so, &conn->li.saddr, td));
	if (conn->wdg_so) {
		sotoipxpcb(conn->wdg_so)->ipxp_laddr.x_net = npcb->ipxp_laddr.x_net;
		sotoipxpcb(conn->wdg_so)->ipxp_laddr.x_host= npcb->ipxp_laddr.x_host;
	}
	if (!error) {
		conn->flags |= NCPFL_SOCONN;
	}
#ifdef NCPBURST
	if (ncp_burst_enabled) {
		checkbad(socreate(AF_IPX, &conn->bc_so, SOCK_DGRAM, 0, td));
		bzero(&sipx, sizeof(sipx));
		sipx.sipx_len = sizeof(sipx);
		checkbad(sobind(conn->bc_so, (struct sockaddr *)&sipx, td));
		checkbad(ncp_soconnect(conn->bc_so, &conn->li.saddr, td));
	}
#endif
	if (!error) {
		conn->flags |= NCPFL_SOCONN;
		ncp_sock_checksum(conn, 0);
	}
	return error;
bad:
	ncp_sock_disconnect(conn);
	return (error);
}

int
ncp_sock_checksum(struct ncp_conn *conn, int enable)
{

	if (enable) {
		sotoipxpcb(conn->ncp_so)->ipxp_flags |= IPXP_CHECKSUM;
	} else {
		sotoipxpcb(conn->ncp_so)->ipxp_flags &= ~IPXP_CHECKSUM;
	}
	return 0;
}

/*
 * Connect to specified server via IP
 */
static int
ncp_sock_connect_in(struct ncp_conn *conn)
{
	struct sockaddr_in sin;
	struct thread *td = conn->td;
	int addrlen = sizeof(sin), error;

	conn->flags = 0;
	bzero(&sin,addrlen);
	conn->ncp_so = conn->wdg_so = NULL;
	checkbad(socreate(AF_INET, &conn->ncp_so, SOCK_DGRAM, IPPROTO_UDP, td->td_ucred, td));
	sin.sin_family = AF_INET;
	sin.sin_len = addrlen;
	checkbad(sobind(conn->ncp_so, (struct sockaddr *)&sin, td));
	checkbad(ncp_soconnect(conn->ncp_so,(struct sockaddr*)&conn->li.addr, td));
	if  (!error)
		conn->flags |= NCPFL_SOCONN;
	return error;
bad:
	ncp_sock_disconnect(conn);
	return (error);
}

int
ncp_sock_connect(struct ncp_conn *ncp)
{
	int error;

	switch (ncp->li.saddr.sa_family) {
	    case AF_IPX:
		error = ncp_sock_connect_ipx(ncp);
		break;
	    case AF_INET:
		error = ncp_sock_connect_in(ncp);
		break;
	    default:
		return EPROTONOSUPPORT;
	}
	return error;
}

/*
 * Connection expected to be locked
 */
int
ncp_sock_disconnect(struct ncp_conn *conn) {
	register struct socket *so;
	conn->flags &= ~(NCPFL_SOCONN | NCPFL_ATTACHED | NCPFL_LOGGED);
	if (conn->ncp_so) {
		so = conn->ncp_so;
		conn->ncp_so = (struct socket *)0;
		soshutdown(so, 2);
		soclose(so);
	}
	if (conn->wdg_so) {
		so = conn->wdg_so;
		conn->wdg_so = (struct socket *)0;
		soshutdown(so, 2);
		soclose(so);
	}
#ifdef NCPBURST
	if (conn->bc_so) {
		so = conn->bc_so;
		conn->bc_so = (struct socket *)NULL;
		soshutdown(so, 2);
		soclose(so);
	}
#endif
	return 0;
}

static void
ncp_watchdog(struct ncp_conn *conn) {
	char *buf;
	struct mbuf *m;
	int error, len, flags;
	struct socket *so;
	struct sockaddr *sa;
	struct uio auio;

	sa = NULL;
	while (conn->wdg_so) { /* not a loop */
		so = conn->wdg_so;
		auio.uio_resid = len = 1000000;
		auio.uio_td = curthread;
		flags = MSG_DONTWAIT;
		error = soreceive(so, (struct sockaddr**)&sa, &auio, &m,
		    (struct mbuf**)0, &flags);
		if (error) break;
		len -= auio.uio_resid;
		NCPSDEBUG("got watch dog %d\n",len);
		if (len != 2) break;
		buf = mtod(m, char*);
		if (buf[1] != '?') break;
		buf[1] = 'Y';
		error = sosend(so, (struct sockaddr*)sa, 0, m, 0, 0, curthread);
		NCPSDEBUG("send watch dog %d\n",error);
		break;
	}
	if (sa) free(sa, M_SONAME);
	return;
}

void
ncp_check_conn(struct ncp_conn *conn) {
	int s;

	if (conn == NULL || !(conn->flags & NCPFL_ATTACHED))
	        return;
	s = splnet();
	ncp_check_rq(conn);
	splx(s);
	if (conn->li.saddr.sa_family == AF_IPX)
		ncp_watchdog(conn);
}
