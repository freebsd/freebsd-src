/*
 * Copyright (c) 1982, 1986, 1988, 1990, 1993
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
 *	@(#)uipc_socket.c	8.3 (Berkeley) 4/15/94
 * $FreeBSD$
 */

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/fcntl.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/domain.h>
#include <sys/file.h>			/* for struct knote */
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/event.h>
#include <sys/poll.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <sys/sysctl.h>
#include <sys/uio.h>
#include <sys/jail.h>

#include <vm/vm_zone.h>

#include <machine/limits.h>

#ifdef INET
static int	 do_setopt_accept_filter(struct socket *so, struct sockopt *sopt);
#endif

static void 	filt_sordetach(struct knote *kn);
static int 	filt_soread(struct knote *kn, long hint);
static void 	filt_sowdetach(struct knote *kn);
static int	filt_sowrite(struct knote *kn, long hint);
static int	filt_solisten(struct knote *kn, long hint);

static struct filterops solisten_filtops =
	{ 1, NULL, filt_sordetach, filt_solisten };
static struct filterops soread_filtops =
	{ 1, NULL, filt_sordetach, filt_soread };
static struct filterops sowrite_filtops =
	{ 1, NULL, filt_sowdetach, filt_sowrite };

struct	vm_zone *socket_zone;
so_gen_t	so_gencnt;	/* generation count for sockets */

MALLOC_DEFINE(M_SONAME, "soname", "socket name");
MALLOC_DEFINE(M_PCB, "pcb", "protocol control block");

SYSCTL_DECL(_kern_ipc);

static int somaxconn = SOMAXCONN;
SYSCTL_INT(_kern_ipc, KIPC_SOMAXCONN, somaxconn, CTLFLAG_RW,
    &somaxconn, 0, "Maximum pending socket connection queue size");
static int numopensockets;
SYSCTL_INT(_kern_ipc, OID_AUTO, numopensockets, CTLFLAG_RD,
    &numopensockets, 0, "Number of open sockets");


/*
 * Socket operation routines.
 * These routines are called by the routines in
 * sys_socket.c or from a system process, and
 * implement the semantics of socket operations by
 * switching out to the protocol specific routines.
 */

/*
 * Get a socket structure from our zone, and initialize it.
 * We don't implement `waitok' yet (see comments in uipc_domain.c).
 * Note that it would probably be better to allocate socket
 * and PCB at the same time, but I'm not convinced that all
 * the protocols can be easily modified to do this.
 *
 * soalloc() returns a socket with a ref count of 0.
 */
struct socket *
soalloc(waitok)
	int waitok;
{
	struct socket *so;

	so = zalloc(socket_zone);
	if (so) {
		/* XXX race condition for reentrant kernel */
		bzero(so, sizeof *so);
		so->so_gencnt = ++so_gencnt;
		so->so_zone = socket_zone;
		/* sx_init(&so->so_sxlock, "socket sxlock"); */
		TAILQ_INIT(&so->so_aiojobq);
		++numopensockets;
	}
	return so;
}

/*
 * socreate returns a socket with a ref count of 1.  The socket should be
 * closed with soclose().
 */
int
socreate(dom, aso, type, proto, cred, td)
	int dom;
	struct socket **aso;
	register int type;
	int proto;
	struct ucred *cred;
	struct thread *td;
{
	register struct protosw *prp;
	register struct socket *so;
	register int error;

	if (proto)
		prp = pffindproto(dom, proto, type);
	else
		prp = pffindtype(dom, type);

	if (prp == 0 || prp->pr_usrreqs->pru_attach == 0)
		return (EPROTONOSUPPORT);

	if (jailed(td->td_proc->p_ucred) && jail_socket_unixiproute_only &&
	    prp->pr_domain->dom_family != PF_LOCAL &&
	    prp->pr_domain->dom_family != PF_INET &&
	    prp->pr_domain->dom_family != PF_ROUTE) {
		return (EPROTONOSUPPORT);
	}

	if (prp->pr_type != type)
		return (EPROTOTYPE);
	so = soalloc(td != 0);
	if (so == 0)
		return (ENOBUFS);

	TAILQ_INIT(&so->so_incomp);
	TAILQ_INIT(&so->so_comp);
	so->so_type = type;
	so->so_cred = crhold(cred);
	so->so_proto = prp;
	soref(so);
	error = (*prp->pr_usrreqs->pru_attach)(so, proto, td);
	if (error) {
		so->so_state |= SS_NOFDREF;
		sorele(so);
		return (error);
	}
	*aso = so;
	return (0);
}

int
sobind(so, nam, td)
	struct socket *so;
	struct sockaddr *nam;
	struct thread *td;
{
	int s = splnet();
	int error;

	error = (*so->so_proto->pr_usrreqs->pru_bind)(so, nam, td);
	splx(s);
	return (error);
}

static void
sodealloc(struct socket *so)
{

	KASSERT(so->so_count == 0, ("sodealloc(): so_count %d", so->so_count));
	so->so_gencnt = ++so_gencnt;
	if (so->so_rcv.sb_hiwat)
		(void)chgsbsize(so->so_cred->cr_uidinfo,
		    &so->so_rcv.sb_hiwat, 0, RLIM_INFINITY);
	if (so->so_snd.sb_hiwat)
		(void)chgsbsize(so->so_cred->cr_uidinfo,
		    &so->so_snd.sb_hiwat, 0, RLIM_INFINITY);
#ifdef INET
	if (so->so_accf != NULL) {
		if (so->so_accf->so_accept_filter != NULL &&
			so->so_accf->so_accept_filter->accf_destroy != NULL) {
			so->so_accf->so_accept_filter->accf_destroy(so);
		}
		if (so->so_accf->so_accept_filter_str != NULL)
			FREE(so->so_accf->so_accept_filter_str, M_ACCF);
		FREE(so->so_accf, M_ACCF);
	}
#endif
	crfree(so->so_cred);
	/* sx_destroy(&so->so_sxlock); */
	zfree(so->so_zone, so);
	--numopensockets;
}

int
solisten(so, backlog, td)
	register struct socket *so;
	int backlog;
	struct thread *td;
{
	int s, error;

	s = splnet();
	error = (*so->so_proto->pr_usrreqs->pru_listen)(so, td);
	if (error) {
		splx(s);
		return (error);
	}
	if (TAILQ_EMPTY(&so->so_comp))
		so->so_options |= SO_ACCEPTCONN;
	if (backlog < 0 || backlog > somaxconn)
		backlog = somaxconn;
	so->so_qlimit = backlog;
	splx(s);
	return (0);
}

void
sofree(so)
	register struct socket *so;
{
	struct socket *head = so->so_head;

	KASSERT(so->so_count == 0, ("socket %p so_count not 0", so));

	if (so->so_pcb || (so->so_state & SS_NOFDREF) == 0)
		return;
	if (head != NULL) {
		if (so->so_state & SS_INCOMP) {
			TAILQ_REMOVE(&head->so_incomp, so, so_list);
			head->so_incqlen--;
		} else if (so->so_state & SS_COMP) {
			/*
			 * We must not decommission a socket that's
			 * on the accept(2) queue.  If we do, then
			 * accept(2) may hang after select(2) indicated
			 * that the listening socket was ready.
			 */
			return;
		} else {
			panic("sofree: not queued");
		}
		head->so_qlen--;
		so->so_state &= ~SS_INCOMP;
		so->so_head = NULL;
	}
	sbrelease(&so->so_snd, so);
	sorflush(so);
	sodealloc(so);
}

/*
 * Close a socket on last file table reference removal.
 * Initiate disconnect if connected.
 * Free socket when disconnect complete.
 *
 * This function will sorele() the socket.  Note that soclose() may be
 * called prior to the ref count reaching zero.  The actual socket
 * structure will not be freed until the ref count reaches zero.
 */
int
soclose(so)
	register struct socket *so;
{
	int s = splnet();		/* conservative */
	int error = 0;

	funsetown(so->so_sigio);
	if (so->so_options & SO_ACCEPTCONN) {
		struct socket *sp, *sonext;

		sp = TAILQ_FIRST(&so->so_incomp);
		for (; sp != NULL; sp = sonext) {
			sonext = TAILQ_NEXT(sp, so_list);
			(void) soabort(sp);
		}
		for (sp = TAILQ_FIRST(&so->so_comp); sp != NULL; sp = sonext) {
			sonext = TAILQ_NEXT(sp, so_list);
			/* Dequeue from so_comp since sofree() won't do it */
			TAILQ_REMOVE(&so->so_comp, sp, so_list);
			so->so_qlen--;
			sp->so_state &= ~SS_COMP;
			sp->so_head = NULL;
			(void) soabort(sp);
		}
	}
	if (so->so_pcb == 0)
		goto discard;
	if (so->so_state & SS_ISCONNECTED) {
		if ((so->so_state & SS_ISDISCONNECTING) == 0) {
			error = sodisconnect(so);
			if (error)
				goto drop;
		}
		if (so->so_options & SO_LINGER) {
			if ((so->so_state & SS_ISDISCONNECTING) &&
			    (so->so_state & SS_NBIO))
				goto drop;
			while (so->so_state & SS_ISCONNECTED) {
				error = tsleep((caddr_t)&so->so_timeo,
				    PSOCK | PCATCH, "soclos", so->so_linger * hz);
				if (error)
					break;
			}
		}
	}
drop:
	if (so->so_pcb) {
		int error2 = (*so->so_proto->pr_usrreqs->pru_detach)(so);
		if (error == 0)
			error = error2;
	}
discard:
	if (so->so_state & SS_NOFDREF)
		panic("soclose: NOFDREF");
	so->so_state |= SS_NOFDREF;
	sorele(so);
	splx(s);
	return (error);
}

/*
 * Must be called at splnet...
 */
int
soabort(so)
	struct socket *so;
{
	int error;

	error = (*so->so_proto->pr_usrreqs->pru_abort)(so);
	if (error) {
		sotryfree(so);	/* note: does not decrement the ref count */
		return error;
	}
	return (0);
}

int
soaccept(so, nam)
	register struct socket *so;
	struct sockaddr **nam;
{
	int s = splnet();
	int error;

	if ((so->so_state & SS_NOFDREF) == 0)
		panic("soaccept: !NOFDREF");
	so->so_state &= ~SS_NOFDREF;
	error = (*so->so_proto->pr_usrreqs->pru_accept)(so, nam);
	splx(s);
	return (error);
}

int
soconnect(so, nam, td)
	register struct socket *so;
	struct sockaddr *nam;
	struct thread *td;
{
	int s;
	int error;

	if (so->so_options & SO_ACCEPTCONN)
		return (EOPNOTSUPP);
	s = splnet();
	/*
	 * If protocol is connection-based, can only connect once.
	 * Otherwise, if connected, try to disconnect first.
	 * This allows user to disconnect by connecting to, e.g.,
	 * a null address.
	 */
	if (so->so_state & (SS_ISCONNECTED|SS_ISCONNECTING) &&
	    ((so->so_proto->pr_flags & PR_CONNREQUIRED) ||
	    (error = sodisconnect(so))))
		error = EISCONN;
	else
		error = (*so->so_proto->pr_usrreqs->pru_connect)(so, nam, td);
	splx(s);
	return (error);
}

int
soconnect2(so1, so2)
	register struct socket *so1;
	struct socket *so2;
{
	int s = splnet();
	int error;

	error = (*so1->so_proto->pr_usrreqs->pru_connect2)(so1, so2);
	splx(s);
	return (error);
}

int
sodisconnect(so)
	register struct socket *so;
{
	int s = splnet();
	int error;

	if ((so->so_state & SS_ISCONNECTED) == 0) {
		error = ENOTCONN;
		goto bad;
	}
	if (so->so_state & SS_ISDISCONNECTING) {
		error = EALREADY;
		goto bad;
	}
	error = (*so->so_proto->pr_usrreqs->pru_disconnect)(so);
bad:
	splx(s);
	return (error);
}

#define	SBLOCKWAIT(f)	(((f) & MSG_DONTWAIT) ? M_NOWAIT : M_WAITOK)
/*
 * Send on a socket.
 * If send must go all at once and message is larger than
 * send buffering, then hard error.
 * Lock against other senders.
 * If must go all at once and not enough room now, then
 * inform user that this would block and do nothing.
 * Otherwise, if nonblocking, send as much as possible.
 * The data to be sent is described by "uio" if nonzero,
 * otherwise by the mbuf chain "top" (which must be null
 * if uio is not).  Data provided in mbuf chain must be small
 * enough to send all at once.
 *
 * Returns nonzero on error, timeout or signal; callers
 * must check for short counts if EINTR/ERESTART are returned.
 * Data and control buffers are freed on return.
 */
int
sosend(so, addr, uio, top, control, flags, td)
	register struct socket *so;
	struct sockaddr *addr;
	struct uio *uio;
	struct mbuf *top;
	struct mbuf *control;
	int flags;
	struct thread *td;
{
	struct mbuf **mp;
	register struct mbuf *m;
	register long space, len, resid;
	int clen = 0, error, s, dontroute, mlen;
	int atomic = sosendallatonce(so) || top;

	if (uio)
		resid = uio->uio_resid;
	else
		resid = top->m_pkthdr.len;
	/*
	 * In theory resid should be unsigned.
	 * However, space must be signed, as it might be less than 0
	 * if we over-committed, and we must use a signed comparison
	 * of space and resid.  On the other hand, a negative resid
	 * causes us to loop sending 0-length segments to the protocol.
	 *
	 * Also check to make sure that MSG_EOR isn't used on SOCK_STREAM
	 * type sockets since that's an error.
	 */
	if (resid < 0 || (so->so_type == SOCK_STREAM && (flags & MSG_EOR))) {
		error = EINVAL;
		goto out;
	}

	dontroute =
	    (flags & MSG_DONTROUTE) && (so->so_options & SO_DONTROUTE) == 0 &&
	    (so->so_proto->pr_flags & PR_ATOMIC);
	if (td)
		td->td_proc->p_stats->p_ru.ru_msgsnd++;
	if (control)
		clen = control->m_len;
#define	snderr(errno)	{ error = errno; splx(s); goto release; }

restart:
	error = sblock(&so->so_snd, SBLOCKWAIT(flags));
	if (error)
		goto out;
	do {
		s = splnet();
		if (so->so_state & SS_CANTSENDMORE)
			snderr(EPIPE);
		if (so->so_error) {
			error = so->so_error;
			so->so_error = 0;
			splx(s);
			goto release;
		}
		if ((so->so_state & SS_ISCONNECTED) == 0) {
			/*
			 * `sendto' and `sendmsg' is allowed on a connection-
			 * based socket if it supports implied connect.
			 * Return ENOTCONN if not connected and no address is
			 * supplied.
			 */
			if ((so->so_proto->pr_flags & PR_CONNREQUIRED) &&
			    (so->so_proto->pr_flags & PR_IMPLOPCL) == 0) {
				if ((so->so_state & SS_ISCONFIRMING) == 0 &&
				    !(resid == 0 && clen != 0))
					snderr(ENOTCONN);
			} else if (addr == 0)
			    snderr(so->so_proto->pr_flags & PR_CONNREQUIRED ?
				   ENOTCONN : EDESTADDRREQ);
		}
		space = sbspace(&so->so_snd);
		if (flags & MSG_OOB)
			space += 1024;
		if ((atomic && resid > so->so_snd.sb_hiwat) ||
		    clen > so->so_snd.sb_hiwat)
			snderr(EMSGSIZE);
		if (space < resid + clen && uio &&
		    (atomic || space < so->so_snd.sb_lowat || space < clen)) {
			if (so->so_state & SS_NBIO)
				snderr(EWOULDBLOCK);
			sbunlock(&so->so_snd);
			error = sbwait(&so->so_snd);
			splx(s);
			if (error)
				goto out;
			goto restart;
		}
		splx(s);
		mp = &top;
		space -= clen;
		do {
		    if (uio == NULL) {
			/*
			 * Data is prepackaged in "top".
			 */
			resid = 0;
			if (flags & MSG_EOR)
				top->m_flags |= M_EOR;
		    } else do {
			if (top == 0) {
				MGETHDR(m, M_TRYWAIT, MT_DATA);
				if (m == NULL) {
					error = ENOBUFS;
					goto release;
				}
				mlen = MHLEN;
				m->m_pkthdr.len = 0;
				m->m_pkthdr.rcvif = (struct ifnet *)0;
			} else {
				MGET(m, M_TRYWAIT, MT_DATA);
				if (m == NULL) {
					error = ENOBUFS;
					goto release;
				}
				mlen = MLEN;
			}
			if (resid >= MINCLSIZE) {
				MCLGET(m, M_TRYWAIT);
				if ((m->m_flags & M_EXT) == 0)
					goto nopages;
				mlen = MCLBYTES;
				len = min(min(mlen, resid), space);
			} else {
nopages:
				len = min(min(mlen, resid), space);
				/*
				 * For datagram protocols, leave room
				 * for protocol headers in first mbuf.
				 */
				if (atomic && top == 0 && len < mlen)
					MH_ALIGN(m, len);
			}
			space -= len;
			error = uiomove(mtod(m, caddr_t), (int)len, uio);
			resid = uio->uio_resid;
			m->m_len = len;
			*mp = m;
			top->m_pkthdr.len += len;
			if (error)
				goto release;
			mp = &m->m_next;
			if (resid <= 0) {
				if (flags & MSG_EOR)
					top->m_flags |= M_EOR;
				break;
			}
		    } while (space > 0 && atomic);
		    if (dontroute)
			    so->so_options |= SO_DONTROUTE;
		    s = splnet();				/* XXX */
		    /*
		     * XXX all the SS_CANTSENDMORE checks previously
		     * done could be out of date.  We could have recieved
		     * a reset packet in an interrupt or maybe we slept
		     * while doing page faults in uiomove() etc. We could
		     * probably recheck again inside the splnet() protection
		     * here, but there are probably other places that this
		     * also happens.  We must rethink this.
		     */
		    error = (*so->so_proto->pr_usrreqs->pru_send)(so,
			(flags & MSG_OOB) ? PRUS_OOB :
			/*
			 * If the user set MSG_EOF, the protocol
			 * understands this flag and nothing left to
			 * send then use PRU_SEND_EOF instead of PRU_SEND.
			 */
			((flags & MSG_EOF) &&
			 (so->so_proto->pr_flags & PR_IMPLOPCL) &&
			 (resid <= 0)) ?
				PRUS_EOF :
			/* If there is more to send set PRUS_MORETOCOME */
			(resid > 0 && space > 0) ? PRUS_MORETOCOME : 0,
			top, addr, control, td);
		    splx(s);
		    if (dontroute)
			    so->so_options &= ~SO_DONTROUTE;
		    clen = 0;
		    control = 0;
		    top = 0;
		    mp = &top;
		    if (error)
			goto release;
		} while (resid && space > 0);
	} while (resid);

release:
	sbunlock(&so->so_snd);
out:
	if (top)
		m_freem(top);
	if (control)
		m_freem(control);
	return (error);
}

/*
 * Implement receive operations on a socket.
 * We depend on the way that records are added to the sockbuf
 * by sbappend*.  In particular, each record (mbufs linked through m_next)
 * must begin with an address if the protocol so specifies,
 * followed by an optional mbuf or mbufs containing ancillary data,
 * and then zero or more mbufs of data.
 * In order to avoid blocking network interrupts for the entire time here,
 * we splx() while doing the actual copy to user space.
 * Although the sockbuf is locked, new data may still be appended,
 * and thus we must maintain consistency of the sockbuf during that time.
 *
 * The caller may receive the data as a single mbuf chain by supplying
 * an mbuf **mp0 for use in returning the chain.  The uio is then used
 * only for the count in uio_resid.
 */
int
soreceive(so, psa, uio, mp0, controlp, flagsp)
	register struct socket *so;
	struct sockaddr **psa;
	struct uio *uio;
	struct mbuf **mp0;
	struct mbuf **controlp;
	int *flagsp;
{
	struct mbuf *m, **mp;
	register int flags, len, error, s, offset;
	struct protosw *pr = so->so_proto;
	struct mbuf *nextrecord;
	int moff, type = 0;
	int orig_resid = uio->uio_resid;

	mp = mp0;
	if (psa)
		*psa = 0;
	if (controlp)
		*controlp = 0;
	if (flagsp)
		flags = *flagsp &~ MSG_EOR;
	else
		flags = 0;
	if (flags & MSG_OOB) {
		m = m_get(M_TRYWAIT, MT_DATA);
		if (m == NULL)
			return (ENOBUFS);
		error = (*pr->pr_usrreqs->pru_rcvoob)(so, m, flags & MSG_PEEK);
		if (error)
			goto bad;
		do {
			error = uiomove(mtod(m, caddr_t),
			    (int) min(uio->uio_resid, m->m_len), uio);
			m = m_free(m);
		} while (uio->uio_resid && error == 0 && m);
bad:
		if (m)
			m_freem(m);
		return (error);
	}
	if (mp)
		*mp = (struct mbuf *)0;
	if (so->so_state & SS_ISCONFIRMING && uio->uio_resid)
		(*pr->pr_usrreqs->pru_rcvd)(so, 0);

restart:
	error = sblock(&so->so_rcv, SBLOCKWAIT(flags));
	if (error)
		return (error);
	s = splnet();

	m = so->so_rcv.sb_mb;
	/*
	 * If we have less data than requested, block awaiting more
	 * (subject to any timeout) if:
	 *   1. the current count is less than the low water mark, or
	 *   2. MSG_WAITALL is set, and it is possible to do the entire
	 *	receive operation at once if we block (resid <= hiwat).
	 *   3. MSG_DONTWAIT is not set
	 * If MSG_WAITALL is set but resid is larger than the receive buffer,
	 * we have to do the receive in sections, and thus risk returning
	 * a short count if a timeout or signal occurs after we start.
	 */
	if (m == 0 || (((flags & MSG_DONTWAIT) == 0 &&
	    so->so_rcv.sb_cc < uio->uio_resid) &&
	    (so->so_rcv.sb_cc < so->so_rcv.sb_lowat ||
	    ((flags & MSG_WAITALL) && uio->uio_resid <= so->so_rcv.sb_hiwat)) &&
	    m->m_nextpkt == 0 && (pr->pr_flags & PR_ATOMIC) == 0)) {
		KASSERT(m != 0 || !so->so_rcv.sb_cc,
		    ("receive: m == %p so->so_rcv.sb_cc == %lu",
		    m, so->so_rcv.sb_cc));
		if (so->so_error) {
			if (m)
				goto dontblock;
			error = so->so_error;
			if ((flags & MSG_PEEK) == 0)
				so->so_error = 0;
			goto release;
		}
		if (so->so_state & SS_CANTRCVMORE) {
			if (m)
				goto dontblock;
			else
				goto release;
		}
		for (; m; m = m->m_next)
			if (m->m_type == MT_OOBDATA  || (m->m_flags & M_EOR)) {
				m = so->so_rcv.sb_mb;
				goto dontblock;
			}
		if ((so->so_state & (SS_ISCONNECTED|SS_ISCONNECTING)) == 0 &&
		    (so->so_proto->pr_flags & PR_CONNREQUIRED)) {
			error = ENOTCONN;
			goto release;
		}
		if (uio->uio_resid == 0)
			goto release;
		if ((so->so_state & SS_NBIO) || (flags & MSG_DONTWAIT)) {
			error = EWOULDBLOCK;
			goto release;
		}
		sbunlock(&so->so_rcv);
		error = sbwait(&so->so_rcv);
		splx(s);
		if (error)
			return (error);
		goto restart;
	}
dontblock:
	if (uio->uio_td)
		uio->uio_td->td_proc->p_stats->p_ru.ru_msgrcv++;
	nextrecord = m->m_nextpkt;
	if (pr->pr_flags & PR_ADDR) {
		KASSERT(m->m_type == MT_SONAME,
		    ("m->m_type == %d", m->m_type));
		orig_resid = 0;
		if (psa)
			*psa = dup_sockaddr(mtod(m, struct sockaddr *),
					    mp0 == 0);
		if (flags & MSG_PEEK) {
			m = m->m_next;
		} else {
			sbfree(&so->so_rcv, m);
			so->so_rcv.sb_mb = m_free(m);
			m = so->so_rcv.sb_mb;
		}
	}
	while (m && m->m_type == MT_CONTROL && error == 0) {
		if (flags & MSG_PEEK) {
			if (controlp)
				*controlp = m_copy(m, 0, m->m_len);
			m = m->m_next;
		} else {
			sbfree(&so->so_rcv, m);
			so->so_rcv.sb_mb = m->m_next;
			m->m_next = NULL;
			if (pr->pr_domain->dom_externalize)
				error =
				(*pr->pr_domain->dom_externalize)(m, controlp);
			else if (controlp)
				*controlp = m;
			else
				m_freem(m);
			m = so->so_rcv.sb_mb;
		}
		if (controlp) {
			orig_resid = 0;
			do
				controlp = &(*controlp)->m_next;
			while (*controlp != NULL);
		}
	}
	if (m) {
		if ((flags & MSG_PEEK) == 0)
			m->m_nextpkt = nextrecord;
		type = m->m_type;
		if (type == MT_OOBDATA)
			flags |= MSG_OOB;
	}
	moff = 0;
	offset = 0;
	while (m && uio->uio_resid > 0 && error == 0) {
		if (m->m_type == MT_OOBDATA) {
			if (type != MT_OOBDATA)
				break;
		} else if (type == MT_OOBDATA)
			break;
		else
		    KASSERT(m->m_type == MT_DATA || m->m_type == MT_HEADER,
			("m->m_type == %d", m->m_type));
		so->so_state &= ~SS_RCVATMARK;
		len = uio->uio_resid;
		if (so->so_oobmark && len > so->so_oobmark - offset)
			len = so->so_oobmark - offset;
		if (len > m->m_len - moff)
			len = m->m_len - moff;
		/*
		 * If mp is set, just pass back the mbufs.
		 * Otherwise copy them out via the uio, then free.
		 * Sockbuf must be consistent here (points to current mbuf,
		 * it points to next record) when we drop priority;
		 * we must note any additions to the sockbuf when we
		 * block interrupts again.
		 */
		if (mp == 0) {
			splx(s);
			error = uiomove(mtod(m, caddr_t) + moff, (int)len, uio);
			s = splnet();
			if (error)
				goto release;
		} else
			uio->uio_resid -= len;
		if (len == m->m_len - moff) {
			if (m->m_flags & M_EOR)
				flags |= MSG_EOR;
			if (flags & MSG_PEEK) {
				m = m->m_next;
				moff = 0;
			} else {
				nextrecord = m->m_nextpkt;
				sbfree(&so->so_rcv, m);
				if (mp) {
					*mp = m;
					mp = &m->m_next;
					so->so_rcv.sb_mb = m = m->m_next;
					*mp = (struct mbuf *)0;
				} else {
					so->so_rcv.sb_mb = m_free(m);
					m = so->so_rcv.sb_mb;
				}
				if (m)
					m->m_nextpkt = nextrecord;
			}
		} else {
			if (flags & MSG_PEEK)
				moff += len;
			else {
				if (mp)
					*mp = m_copym(m, 0, len, M_TRYWAIT);
				m->m_data += len;
				m->m_len -= len;
				so->so_rcv.sb_cc -= len;
			}
		}
		if (so->so_oobmark) {
			if ((flags & MSG_PEEK) == 0) {
				so->so_oobmark -= len;
				if (so->so_oobmark == 0) {
					so->so_state |= SS_RCVATMARK;
					break;
				}
			} else {
				offset += len;
				if (offset == so->so_oobmark)
					break;
			}
		}
		if (flags & MSG_EOR)
			break;
		/*
		 * If the MSG_WAITALL flag is set (for non-atomic socket),
		 * we must not quit until "uio->uio_resid == 0" or an error
		 * termination.  If a signal/timeout occurs, return
		 * with a short count but without error.
		 * Keep sockbuf locked against other readers.
		 */
		while (flags & MSG_WAITALL && m == 0 && uio->uio_resid > 0 &&
		    !sosendallatonce(so) && !nextrecord) {
			if (so->so_error || so->so_state & SS_CANTRCVMORE)
				break;
			/*
			 * Notify the protocol that some data has been
			 * drained before blocking.
			 */
			if (pr->pr_flags & PR_WANTRCVD && so->so_pcb)
				(*pr->pr_usrreqs->pru_rcvd)(so, flags);
			error = sbwait(&so->so_rcv);
			if (error) {
				sbunlock(&so->so_rcv);
				splx(s);
				return (0);
			}
			m = so->so_rcv.sb_mb;
			if (m)
				nextrecord = m->m_nextpkt;
		}
	}

	if (m && pr->pr_flags & PR_ATOMIC) {
		flags |= MSG_TRUNC;
		if ((flags & MSG_PEEK) == 0)
			(void) sbdroprecord(&so->so_rcv);
	}
	if ((flags & MSG_PEEK) == 0) {
		if (m == 0)
			so->so_rcv.sb_mb = nextrecord;
		if (pr->pr_flags & PR_WANTRCVD && so->so_pcb)
			(*pr->pr_usrreqs->pru_rcvd)(so, flags);
	}
	if (orig_resid == uio->uio_resid && orig_resid &&
	    (flags & MSG_EOR) == 0 && (so->so_state & SS_CANTRCVMORE) == 0) {
		sbunlock(&so->so_rcv);
		splx(s);
		goto restart;
	}

	if (flagsp)
		*flagsp |= flags;
release:
	sbunlock(&so->so_rcv);
	splx(s);
	return (error);
}

int
soshutdown(so, how)
	register struct socket *so;
	register int how;
{
	register struct protosw *pr = so->so_proto;

	if (!(how == SHUT_RD || how == SHUT_WR || how == SHUT_RDWR))
		return (EINVAL);

	if (how != SHUT_WR)
		sorflush(so);
	if (how != SHUT_RD)
		return ((*pr->pr_usrreqs->pru_shutdown)(so));
	return (0);
}

void
sorflush(so)
	register struct socket *so;
{
	register struct sockbuf *sb = &so->so_rcv;
	register struct protosw *pr = so->so_proto;
	register int s;
	struct sockbuf asb;

	sb->sb_flags |= SB_NOINTR;
	(void) sblock(sb, M_WAITOK);
	s = splimp();
	socantrcvmore(so);
	sbunlock(sb);
	asb = *sb;
	bzero((caddr_t)sb, sizeof (*sb));
	splx(s);
	if (pr->pr_flags & PR_RIGHTS && pr->pr_domain->dom_dispose)
		(*pr->pr_domain->dom_dispose)(asb.sb_mb);
	sbrelease(&asb, so);
}

#ifdef INET
static int
do_setopt_accept_filter(so, sopt)
	struct	socket *so;
	struct	sockopt *sopt;
{
	struct accept_filter_arg	*afap = NULL;
	struct accept_filter	*afp;
	struct so_accf	*af = so->so_accf;
	int	error = 0;

	/* do not set/remove accept filters on non listen sockets */
	if ((so->so_options & SO_ACCEPTCONN) == 0) {
		error = EINVAL;
		goto out;
	}

	/* removing the filter */
	if (sopt == NULL) {
		if (af != NULL) {
			if (af->so_accept_filter != NULL &&
				af->so_accept_filter->accf_destroy != NULL) {
				af->so_accept_filter->accf_destroy(so);
			}
			if (af->so_accept_filter_str != NULL) {
				FREE(af->so_accept_filter_str, M_ACCF);
			}
			FREE(af, M_ACCF);
			so->so_accf = NULL;
		}
		so->so_options &= ~SO_ACCEPTFILTER;
		return (0);
	}
	/* adding a filter */
	/* must remove previous filter first */
	if (af != NULL) {
		error = EINVAL;
		goto out;
	}
	/* don't put large objects on the kernel stack */
	MALLOC(afap, struct accept_filter_arg *, sizeof(*afap), M_TEMP, M_WAITOK);
	error = sooptcopyin(sopt, afap, sizeof *afap, sizeof *afap);
	afap->af_name[sizeof(afap->af_name)-1] = '\0';
	afap->af_arg[sizeof(afap->af_arg)-1] = '\0';
	if (error)
		goto out;
	afp = accept_filt_get(afap->af_name);
	if (afp == NULL) {
		error = ENOENT;
		goto out;
	}
	MALLOC(af, struct so_accf *, sizeof(*af), M_ACCF, M_WAITOK | M_ZERO);
	if (afp->accf_create != NULL) {
		if (afap->af_name[0] != '\0') {
			int len = strlen(afap->af_name) + 1;

			MALLOC(af->so_accept_filter_str, char *, len, M_ACCF, M_WAITOK);
			strcpy(af->so_accept_filter_str, afap->af_name);
		}
		af->so_accept_filter_arg = afp->accf_create(so, afap->af_arg);
		if (af->so_accept_filter_arg == NULL) {
			FREE(af->so_accept_filter_str, M_ACCF);
			FREE(af, M_ACCF);
			so->so_accf = NULL;
			error = EINVAL;
			goto out;
		}
	}
	af->so_accept_filter = afp;
	so->so_accf = af;
	so->so_options |= SO_ACCEPTFILTER;
out:
	if (afap != NULL)
		FREE(afap, M_TEMP);
	return (error);
}
#endif /* INET */

/*
 * Perhaps this routine, and sooptcopyout(), below, ought to come in
 * an additional variant to handle the case where the option value needs
 * to be some kind of integer, but not a specific size.
 * In addition to their use here, these functions are also called by the
 * protocol-level pr_ctloutput() routines.
 */
int
sooptcopyin(sopt, buf, len, minlen)
	struct	sockopt *sopt;
	void	*buf;
	size_t	len;
	size_t	minlen;
{
	size_t	valsize;

	/*
	 * If the user gives us more than we wanted, we ignore it,
	 * but if we don't get the minimum length the caller
	 * wants, we return EINVAL.  On success, sopt->sopt_valsize
	 * is set to however much we actually retrieved.
	 */
	if ((valsize = sopt->sopt_valsize) < minlen)
		return EINVAL;
	if (valsize > len)
		sopt->sopt_valsize = valsize = len;

	if (sopt->sopt_td != 0)
		return (copyin(sopt->sopt_val, buf, valsize));

	bcopy(sopt->sopt_val, buf, valsize);
	return 0;
}

int
sosetopt(so, sopt)
	struct socket *so;
	struct sockopt *sopt;
{
	int	error, optval;
	struct	linger l;
	struct	timeval tv;
	u_long  val;

	error = 0;
	if (sopt->sopt_level != SOL_SOCKET) {
		if (so->so_proto && so->so_proto->pr_ctloutput)
			return ((*so->so_proto->pr_ctloutput)
				  (so, sopt));
		error = ENOPROTOOPT;
	} else {
		switch (sopt->sopt_name) {
#ifdef INET
		case SO_ACCEPTFILTER:
			error = do_setopt_accept_filter(so, sopt);
			if (error)
				goto bad;
			break;
#endif
		case SO_LINGER:
			error = sooptcopyin(sopt, &l, sizeof l, sizeof l);
			if (error)
				goto bad;

			so->so_linger = l.l_linger;
			if (l.l_onoff)
				so->so_options |= SO_LINGER;
			else
				so->so_options &= ~SO_LINGER;
			break;

		case SO_DEBUG:
		case SO_KEEPALIVE:
		case SO_DONTROUTE:
		case SO_USELOOPBACK:
		case SO_BROADCAST:
		case SO_REUSEADDR:
		case SO_REUSEPORT:
		case SO_OOBINLINE:
		case SO_TIMESTAMP:
			error = sooptcopyin(sopt, &optval, sizeof optval,
					    sizeof optval);
			if (error)
				goto bad;
			if (optval)
				so->so_options |= sopt->sopt_name;
			else
				so->so_options &= ~sopt->sopt_name;
			break;

		case SO_SNDBUF:
		case SO_RCVBUF:
		case SO_SNDLOWAT:
		case SO_RCVLOWAT:
			error = sooptcopyin(sopt, &optval, sizeof optval,
					    sizeof optval);
			if (error)
				goto bad;

			/*
			 * Values < 1 make no sense for any of these
			 * options, so disallow them.
			 */
			if (optval < 1) {
				error = EINVAL;
				goto bad;
			}

			switch (sopt->sopt_name) {
			case SO_SNDBUF:
			case SO_RCVBUF:
				if (sbreserve(sopt->sopt_name == SO_SNDBUF ?
				    &so->so_snd : &so->so_rcv, (u_long)optval,
				    so, curthread) == 0) {
					error = ENOBUFS;
					goto bad;
				}
				break;

			/*
			 * Make sure the low-water is never greater than
			 * the high-water.
			 */
			case SO_SNDLOWAT:
				so->so_snd.sb_lowat =
				    (optval > so->so_snd.sb_hiwat) ?
				    so->so_snd.sb_hiwat : optval;
				break;
			case SO_RCVLOWAT:
				so->so_rcv.sb_lowat =
				    (optval > so->so_rcv.sb_hiwat) ?
				    so->so_rcv.sb_hiwat : optval;
				break;
			}
			break;

		case SO_SNDTIMEO:
		case SO_RCVTIMEO:
			error = sooptcopyin(sopt, &tv, sizeof tv,
					    sizeof tv);
			if (error)
				goto bad;

			/* assert(hz > 0); */
			if (tv.tv_sec < 0 || tv.tv_sec > SHRT_MAX / hz ||
			    tv.tv_usec < 0 || tv.tv_usec >= 1000000) {
				error = EDOM;
				goto bad;
			}
			/* assert(tick > 0); */
			/* assert(ULONG_MAX - SHRT_MAX >= 1000000); */
			val = (u_long)(tv.tv_sec * hz) + tv.tv_usec / tick;
			if (val > SHRT_MAX) {
				error = EDOM;
				goto bad;
			}

			switch (sopt->sopt_name) {
			case SO_SNDTIMEO:
				so->so_snd.sb_timeo = val;
				break;
			case SO_RCVTIMEO:
				so->so_rcv.sb_timeo = val;
				break;
			}
			break;
		default:
			error = ENOPROTOOPT;
			break;
		}
		if (error == 0 && so->so_proto && so->so_proto->pr_ctloutput) {
			(void) ((*so->so_proto->pr_ctloutput)
				  (so, sopt));
		}
	}
bad:
	return (error);
}

/* Helper routine for getsockopt */
int
sooptcopyout(sopt, buf, len)
	struct	sockopt *sopt;
	void	*buf;
	size_t	len;
{
	int	error;
	size_t	valsize;

	error = 0;

	/*
	 * Documented get behavior is that we always return a value,
	 * possibly truncated to fit in the user's buffer.
	 * Traditional behavior is that we always tell the user
	 * precisely how much we copied, rather than something useful
	 * like the total amount we had available for her.
	 * Note that this interface is not idempotent; the entire answer must
	 * generated ahead of time.
	 */
	valsize = min(len, sopt->sopt_valsize);
	sopt->sopt_valsize = valsize;
	if (sopt->sopt_val != 0) {
		if (sopt->sopt_td != 0)
			error = copyout(buf, sopt->sopt_val, valsize);
		else
			bcopy(buf, sopt->sopt_val, valsize);
	}
	return error;
}

int
sogetopt(so, sopt)
	struct socket *so;
	struct sockopt *sopt;
{
	int	error, optval;
	struct	linger l;
	struct	timeval tv;
#ifdef INET
	struct accept_filter_arg *afap;
#endif

	error = 0;
	if (sopt->sopt_level != SOL_SOCKET) {
		if (so->so_proto && so->so_proto->pr_ctloutput) {
			return ((*so->so_proto->pr_ctloutput)
				  (so, sopt));
		} else
			return (ENOPROTOOPT);
	} else {
		switch (sopt->sopt_name) {
#ifdef INET
		case SO_ACCEPTFILTER:
			if ((so->so_options & SO_ACCEPTCONN) == 0)
				return (EINVAL);
			MALLOC(afap, struct accept_filter_arg *, sizeof(*afap),
				M_TEMP, M_WAITOK | M_ZERO);
			if ((so->so_options & SO_ACCEPTFILTER) != 0) {
				strcpy(afap->af_name, so->so_accf->so_accept_filter->accf_name);
				if (so->so_accf->so_accept_filter_str != NULL)
					strcpy(afap->af_arg, so->so_accf->so_accept_filter_str);
			}
			error = sooptcopyout(sopt, afap, sizeof(*afap));
			FREE(afap, M_TEMP);
			break;
#endif

		case SO_LINGER:
			l.l_onoff = so->so_options & SO_LINGER;
			l.l_linger = so->so_linger;
			error = sooptcopyout(sopt, &l, sizeof l);
			break;

		case SO_USELOOPBACK:
		case SO_DONTROUTE:
		case SO_DEBUG:
		case SO_KEEPALIVE:
		case SO_REUSEADDR:
		case SO_REUSEPORT:
		case SO_BROADCAST:
		case SO_OOBINLINE:
		case SO_TIMESTAMP:
			optval = so->so_options & sopt->sopt_name;
integer:
			error = sooptcopyout(sopt, &optval, sizeof optval);
			break;

		case SO_TYPE:
			optval = so->so_type;
			goto integer;

		case SO_ERROR:
			optval = so->so_error;
			so->so_error = 0;
			goto integer;

		case SO_SNDBUF:
			optval = so->so_snd.sb_hiwat;
			goto integer;

		case SO_RCVBUF:
			optval = so->so_rcv.sb_hiwat;
			goto integer;

		case SO_SNDLOWAT:
			optval = so->so_snd.sb_lowat;
			goto integer;

		case SO_RCVLOWAT:
			optval = so->so_rcv.sb_lowat;
			goto integer;

		case SO_SNDTIMEO:
		case SO_RCVTIMEO:
			optval = (sopt->sopt_name == SO_SNDTIMEO ?
				  so->so_snd.sb_timeo : so->so_rcv.sb_timeo);

			tv.tv_sec = optval / hz;
			tv.tv_usec = (optval % hz) * tick;
			error = sooptcopyout(sopt, &tv, sizeof tv);
			break;

		default:
			error = ENOPROTOOPT;
			break;
		}
		return (error);
	}
}

/* XXX; prepare mbuf for (__FreeBSD__ < 3) routines. */
int
soopt_getm(struct sockopt *sopt, struct mbuf **mp)
{
	struct mbuf *m, *m_prev;
	int sopt_size = sopt->sopt_valsize;

	MGET(m, sopt->sopt_td ? M_TRYWAIT : M_DONTWAIT, MT_DATA);
	if (m == 0)
		return ENOBUFS;
	if (sopt_size > MLEN) {
		MCLGET(m, sopt->sopt_td ? M_TRYWAIT : M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			m_free(m);
			return ENOBUFS;
		}
		m->m_len = min(MCLBYTES, sopt_size);
	} else {
		m->m_len = min(MLEN, sopt_size);
	}
	sopt_size -= m->m_len;
	*mp = m;
	m_prev = m;

	while (sopt_size) {
		MGET(m, sopt->sopt_td ? M_TRYWAIT : M_DONTWAIT, MT_DATA);
		if (m == 0) {
			m_freem(*mp);
			return ENOBUFS;
		}
		if (sopt_size > MLEN) {
			MCLGET(m, sopt->sopt_td ? M_TRYWAIT : M_DONTWAIT);
			if ((m->m_flags & M_EXT) == 0) {
				m_freem(*mp);
				return ENOBUFS;
			}
			m->m_len = min(MCLBYTES, sopt_size);
		} else {
			m->m_len = min(MLEN, sopt_size);
		}
		sopt_size -= m->m_len;
		m_prev->m_next = m;
		m_prev = m;
	}
	return 0;
}

/* XXX; copyin sopt data into mbuf chain for (__FreeBSD__ < 3) routines. */
int
soopt_mcopyin(struct sockopt *sopt, struct mbuf *m)
{
	struct mbuf *m0 = m;

	if (sopt->sopt_val == NULL)
		return 0;
	while (m != NULL && sopt->sopt_valsize >= m->m_len) {
		if (sopt->sopt_td != NULL) {
			int error;

			error = copyin(sopt->sopt_val, mtod(m, char *),
				       m->m_len);
			if (error != 0) {
				m_freem(m0);
				return(error);
			}
		} else
			bcopy(sopt->sopt_val, mtod(m, char *), m->m_len);
		sopt->sopt_valsize -= m->m_len;
		(caddr_t)sopt->sopt_val += m->m_len;
		m = m->m_next;
	}
	if (m != NULL) /* should be allocated enoughly at ip6_sooptmcopyin() */
		panic("ip6_sooptmcopyin");
	return 0;
}

/* XXX; copyout mbuf chain data into soopt for (__FreeBSD__ < 3) routines. */
int
soopt_mcopyout(struct sockopt *sopt, struct mbuf *m)
{
	struct mbuf *m0 = m;
	size_t valsize = 0;

	if (sopt->sopt_val == NULL)
		return 0;
	while (m != NULL && sopt->sopt_valsize >= m->m_len) {
		if (sopt->sopt_td != NULL) {
			int error;

			error = copyout(mtod(m, char *), sopt->sopt_val,
				       m->m_len);
			if (error != 0) {
				m_freem(m0);
				return(error);
			}
		} else
			bcopy(mtod(m, char *), sopt->sopt_val, m->m_len);
	       sopt->sopt_valsize -= m->m_len;
	       (caddr_t)sopt->sopt_val += m->m_len;
	       valsize += m->m_len;
	       m = m->m_next;
	}
	if (m != NULL) {
		/* enough soopt buffer should be given from user-land */
		m_freem(m0);
		return(EINVAL);
	}
	sopt->sopt_valsize = valsize;
	return 0;
}

void
sohasoutofband(so)
	register struct socket *so;
{
	if (so->so_sigio != NULL)
		pgsigio(so->so_sigio, SIGURG, 0);
	selwakeup(&so->so_rcv.sb_sel);
}

int
sopoll(struct socket *so, int events, struct ucred *cred, struct thread *td)
{
	int revents = 0;
	int s = splnet();

	if (events & (POLLIN | POLLRDNORM))
		if (soreadable(so))
			revents |= events & (POLLIN | POLLRDNORM);

	if (events & POLLINIGNEOF)
		if (so->so_rcv.sb_cc >= so->so_rcv.sb_lowat ||
		    !TAILQ_EMPTY(&so->so_comp) || so->so_error)
			revents |= POLLINIGNEOF;

	if (events & (POLLOUT | POLLWRNORM))
		if (sowriteable(so))
			revents |= events & (POLLOUT | POLLWRNORM);

	if (events & (POLLPRI | POLLRDBAND))
		if (so->so_oobmark || (so->so_state & SS_RCVATMARK))
			revents |= events & (POLLPRI | POLLRDBAND);

	if (revents == 0) {
		if (events &
		    (POLLIN | POLLINIGNEOF | POLLPRI | POLLRDNORM |
		     POLLRDBAND)) {
			selrecord(td, &so->so_rcv.sb_sel);
			so->so_rcv.sb_flags |= SB_SEL;
		}

		if (events & (POLLOUT | POLLWRNORM)) {
			selrecord(td, &so->so_snd.sb_sel);
			so->so_snd.sb_flags |= SB_SEL;
		}
	}

	splx(s);
	return (revents);
}

int
sokqfilter(struct file *fp, struct knote *kn)
{
	struct socket *so = (struct socket *)kn->kn_fp->f_data;
	struct sockbuf *sb;
	int s;

	switch (kn->kn_filter) {
	case EVFILT_READ:
		if (so->so_options & SO_ACCEPTCONN)
			kn->kn_fop = &solisten_filtops;
		else
			kn->kn_fop = &soread_filtops;
		sb = &so->so_rcv;
		break;
	case EVFILT_WRITE:
		kn->kn_fop = &sowrite_filtops;
		sb = &so->so_snd;
		break;
	default:
		return (1);
	}

	s = splnet();
	SLIST_INSERT_HEAD(&sb->sb_sel.si_note, kn, kn_selnext);
	sb->sb_flags |= SB_KNOTE;
	splx(s);
	return (0);
}

static void
filt_sordetach(struct knote *kn)
{
	struct socket *so = (struct socket *)kn->kn_fp->f_data;
	int s = splnet();

	SLIST_REMOVE(&so->so_rcv.sb_sel.si_note, kn, knote, kn_selnext);
	if (SLIST_EMPTY(&so->so_rcv.sb_sel.si_note))
		so->so_rcv.sb_flags &= ~SB_KNOTE;
	splx(s);
}

/*ARGSUSED*/
static int
filt_soread(struct knote *kn, long hint)
{
	struct socket *so = (struct socket *)kn->kn_fp->f_data;

	kn->kn_data = so->so_rcv.sb_cc;
	if (so->so_state & SS_CANTRCVMORE) {
		kn->kn_flags |= EV_EOF;
		kn->kn_fflags = so->so_error;
		return (1);
	}
	if (so->so_error)	/* temporary udp error */
		return (1);
	if (kn->kn_sfflags & NOTE_LOWAT)
		return (kn->kn_data >= kn->kn_sdata);
	return (kn->kn_data >= so->so_rcv.sb_lowat);
}

static void
filt_sowdetach(struct knote *kn)
{
	struct socket *so = (struct socket *)kn->kn_fp->f_data;
	int s = splnet();

	SLIST_REMOVE(&so->so_snd.sb_sel.si_note, kn, knote, kn_selnext);
	if (SLIST_EMPTY(&so->so_snd.sb_sel.si_note))
		so->so_snd.sb_flags &= ~SB_KNOTE;
	splx(s);
}

/*ARGSUSED*/
static int
filt_sowrite(struct knote *kn, long hint)
{
	struct socket *so = (struct socket *)kn->kn_fp->f_data;

	kn->kn_data = sbspace(&so->so_snd);
	if (so->so_state & SS_CANTSENDMORE) {
		kn->kn_flags |= EV_EOF;
		kn->kn_fflags = so->so_error;
		return (1);
	}
	if (so->so_error)	/* temporary udp error */
		return (1);
	if (((so->so_state & SS_ISCONNECTED) == 0) &&
	    (so->so_proto->pr_flags & PR_CONNREQUIRED))
		return (0);
	if (kn->kn_sfflags & NOTE_LOWAT)
		return (kn->kn_data >= kn->kn_sdata);
	return (kn->kn_data >= so->so_snd.sb_lowat);
}

/*ARGSUSED*/
static int
filt_solisten(struct knote *kn, long hint)
{
	struct socket *so = (struct socket *)kn->kn_fp->f_data;

	kn->kn_data = so->so_qlen - so->so_incqlen;
	return (! TAILQ_EMPTY(&so->so_comp));
}

int
socheckuid(struct socket *so, uid_t uid)
{

	if (so == NULL)
		return (EPERM);
	if (so->so_cred->cr_uid == uid)
		return (0);
	return (EPERM);
}
