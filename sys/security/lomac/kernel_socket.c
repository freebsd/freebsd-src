/*-
 * Copyright (c) 2001 Networks Associates Technology, Inc.
 *
 * This software was developed for the FreeBSD Project by NAI Labs, the
 * Security Research Division of Network Associates, Inc. under
 * DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the DARPA
 * CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the the above entities nor the names of any
 *    contributors of those entities may be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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
 * $Id$
 */
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

/*
 * This file implements LOMAC controls over socket operations.  LOMAC
 * gains control of socket operations by interposing on the `struct
 * pr_usrreqs' operations vectors of each `struct protosw'.  This code
 * replaces each `struct pr_usrreqs' with an instance of `struct
 * lomac_pr_usrreqs' containing LOMAC socket control functions.  These
 * socket control functions implement LOMAC's socket controls, and then
 * call the corresponding socket operations from the original `struct
 * pr_usrreqs'.  Each instance of `struct lomac_pr_usrreqs' ends with
 * a pointer to the `struct pr_usrreqs' it replaces.  These pointers
 * allow the LOMAC socket control functions to find their corresponding
 * original `struct pr_usrreqs' functions.
 *
 * This file provides the function lomac_initialize_sockets() to turn
 * socket interposition on.  Once socket iterposition is turned on,
 * the kernel will begin to call LOMAC's socket control functions.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>

#include <sys/domain.h>
#include <sys/mbuf.h>
#include <sys/namei.h>
#include <sys/protosw.h>
#include <sys/socketvar.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/unpcb.h>
#include <sys/uio.h>
#include <sys/vnode.h>

#include "kernel_interface.h"
#include "kernel_socket.h"
#include "kernel_mediate.h"
#include "kernel_monitor.h"
#include "lomacfs.h"

MALLOC_DEFINE(M_LOMAC_USRREQS, "LOMAC-UR", "LOMAC usrreqs");

struct lomac_pr_usrreqs {
	struct pr_usrreqs lomac_pr_usrreqs; /* LOMAC socket control fxns */
	struct pr_usrreqs *orig_pr_usrreqs; /* original socket op vector */
};

int lomac_local_accept(struct socket *, struct sockaddr **);
int lomac_local_connect(struct socket *, struct sockaddr *, struct thread *);
int lomac_local_connect2(struct socket *, struct socket *);
int lomac_local_detach(struct socket *);
int lomac_local_send( struct socket *, int, struct mbuf *, struct sockaddr *,
    struct mbuf *, struct thread * );
int lomac_soreceive( struct socket *, struct sockaddr **, struct uio *,
    struct mbuf **, struct mbuf **, int * );
int lomac_local_soreceive( struct socket *, struct sockaddr **, struct uio *,
    struct mbuf **, struct mbuf **, int * );
static int monitored_soreceive( struct socket *, struct sockaddr **,
    struct uio *, struct mbuf **, struct mbuf **, int * );

/* This usrreqs structure implements LOMAC's controls on local sockets */
struct pr_usrreqs lomac_local_usrreqs = {
	NULL,
	lomac_local_accept,
	NULL,
	NULL,
	lomac_local_connect,
	lomac_local_connect2,
	NULL,
	lomac_local_detach,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	lomac_local_send,
	NULL,
	NULL,
	NULL,
	NULL,
	lomac_local_soreceive,
	NULL
};

/* This usrreqs structure implements LOMAC's controls on network sockets */
struct pr_usrreqs lomac_net_usrreqs = {
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	lomac_soreceive,
	NULL
};

static __inline struct pr_usrreqs *
orig_pr_usrreqs( struct socket *so ) {
	return (((struct lomac_pr_usrreqs *)(so->so_proto->pr_usrreqs))->
	    orig_pr_usrreqs);
}

int
lomac_local_accept( struct socket *so, struct sockaddr **nam ) {
	struct vnode *vp;
	struct unpcb *unp;
	int ret_val;     /* value to return to caller */
	
	unp = sotounpcb(so);
	if (unp == NULL)
		return (EINVAL);
	if (unp->unp_conn != NULL) {
		vp = unp->unp_vnode = unp->unp_conn->unp_vnode;
		if (vp != NULL)
			vref(vp);
	}
	ret_val = (*orig_pr_usrreqs(so)->pru_accept)(so, nam);
	return (ret_val);
}

int
lomac_local_connect(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	register struct sockaddr_un *soun = (struct sockaddr_un *)nam;
	register struct vnode *vp;
	register struct socket *so2, *so3;
	struct unpcb *unp, *unp2, *unp3;
	int error, len;
	struct nameidata nd;
	char buf[SOCK_MAXADDRLEN];

	len = nam->sa_len - offsetof(struct sockaddr_un, sun_path);
	if (len <= 0)
		return EINVAL;
	strncpy(buf, soun->sun_path, len);
	buf[len] = 0;

	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_SYSSPACE, buf, td);
	error = namei(&nd);
	if (error)
		goto bad2;
	vp = nd.ni_vp;
	NDFREE(&nd, NDF_ONLY_PNBUF);
	if (vp->v_type != VSOCK) {
		error = ENOTSOCK;
		goto bad;
	}
	error = VOP_ACCESS(vp, VWRITE, td->td_ucred, td);
	if (error)
		goto bad;
	so2 = vp->v_socket;
	if (so2 == 0) {
		error = ECONNREFUSED;
		goto bad;
	}
	if (so->so_type != so2->so_type) {
		error = EPROTOTYPE;
		goto bad;
	}
	if (so->so_proto->pr_flags & PR_CONNREQUIRED) {
		if ((so2->so_options & SO_ACCEPTCONN) == 0 ||
		    (so3 = sonewconn(so2, 0)) == 0) {
			error = ECONNREFUSED;
			goto bad;
		}
		unp = sotounpcb(so);
		unp2 = sotounpcb(so2);
		unp3 = sotounpcb(so3);
		if (unp2->unp_addr)
			unp3->unp_addr = (struct sockaddr_un *)
				dup_sockaddr((struct sockaddr *)
					     unp2->unp_addr, 1);

		/*
		 * unp_peercred management:
		 *
		 * The connecter's (client's) credentials are copied
		 * from its process structure at the time of connect()
		 * (which is now).
		 */
		cru2x(td->td_ucred, &unp3->unp_peercred);
		unp3->unp_flags |= UNP_HAVEPC;
		/*
		 * The receiver's (server's) credentials are copied
		 * from the unp_peercred member of socket on which the
		 * former called listen(); unp_listen() cached that
		 * process's credentials at that time so we can use
		 * them now.
		 */
		KASSERT(unp2->unp_flags & UNP_HAVEPCCACHED,
		    ("unp_connect: listener without cached peercred"));
		memcpy(&unp->unp_peercred, &unp2->unp_peercred,
		    sizeof(unp->unp_peercred));
		unp->unp_flags |= UNP_HAVEPC;

		so2 = so3;
	}
	error = lomac_local_connect2(so, so2);
bad:
	vput(vp);
bad2:
	return (error);
}

int
lomac_local_connect2( struct socket *so1, struct socket *so2 ) {
	struct vnode *vp;
	int ret_val;     /* value to return to caller */
	
	if (so2->so_head != NULL) {
		vp = sotounpcb(so2->so_head)->unp_vnode;
		if (vp != NULL) {
			sotounpcb(so1)->unp_vnode = vp;
			vref(vp);
		}
	}
	ret_val = (*orig_pr_usrreqs(so1)->pru_connect2)(so1, so2);
	return (ret_val);
}

int
lomac_local_detach( struct socket *so ) {
	int ret_val;     /* value to return to caller */
	struct unpcb *unp = sotounpcb(so);
	
	if (unp == NULL)
		return (EINVAL);
	if (unp->unp_vnode != NULL && unp->unp_vnode->v_socket != so) {
		vrele(unp->unp_vnode);
		unp->unp_vnode = NULL;
	}
	ret_val = (*orig_pr_usrreqs(so)->pru_detach)(so);
	return (ret_val);
}

int
lomac_local_send( struct socket *so, int flags, struct mbuf *m, 
    struct sockaddr *addr, struct mbuf *control, struct thread *td ) {
	struct vnode *vp;
	struct unpcb *unp = sotounpcb(so);
	int error;

	/* printf( "pid %d local send\n", p->p_pid ); */
	if (unp == NULL) {
		error = EINVAL;
		goto out;
	}
	if (so->so_type == SOCK_DGRAM) {
		if (addr != NULL) {
			if (unp->unp_conn != NULL) {
				error = EISCONN;
				goto out;
			}
			error = lomac_local_connect(so, addr, td);
			if (error)
				goto out;
		} else if (unp->unp_conn == NULL) {
			error = ENOTCONN;
			goto out;
		}
	} else if ((so->so_state & SS_ISCONNECTED) == 0) {
		if (addr != NULL) {
			error = lomac_local_connect(so, addr, td);
			if (error)
				goto out;	/* XXX */
		} else {
			error = ENOTCONN;
			goto out;
		}
	}
	vp = unp->unp_vnode;
	if (vp != NULL) {
		lomac_object_t lobj;

		lobj.lo_type = VISLOMAC(vp) ? LO_TYPE_LVNODE : LO_TYPE_UVNODE;
		lobj.lo_object.vnode = vp;
		if (!mediate_subject_object("send", td->td_proc, &lobj)) {
			error = EPERM;
			goto out;
		}
	} else {
		/*
		 * This is a send to a socket in a socketpair() pair.
		 * Mark both sockets in pair with the appropriate level.
		 */
		lomac_object_t lobj1, lobj2;
		lattr_t lattr;

		lobj1.lo_type = LO_TYPE_SOCKETPAIR;
		lobj1.lo_object.socket = so;
		if ((error = monitor_pipe_write(td->td_proc, &lobj1)) != 0)
			goto out;
		lobj2.lo_type = LO_TYPE_SOCKETPAIR;
		lobj2.lo_object.socket = unp->unp_conn->unp_socket;
		get_object_lattr(&lobj1, &lattr);
		set_object_lattr(&lobj2, lattr);
	}
	error = (*orig_pr_usrreqs(so)->pru_send)( so, flags, m, NULL,
            control, td );
	if (addr != NULL && so->so_type == SOCK_DGRAM)
		(*orig_pr_usrreqs(so)->pru_disconnect)(so);
out:
	return (error);
}



int
lomac_local_soreceive(struct socket *so, struct sockaddr **paddr,
    struct uio *uio, struct mbuf **mp0, struct mbuf **controlp, int *flagsp) {
	lomac_object_t lobj;
	struct vnode *vp;
	struct unpcb *unp = sotounpcb(so);
	int ret_val;     /* value to return to caller */

	if (unp == NULL)
		return (EINVAL);
	vp = unp->unp_vnode;
	if (vp != NULL) {
		lobj.lo_type = VISLOMAC(vp) ? LO_TYPE_LVNODE : LO_TYPE_UVNODE;
		lobj.lo_object.vnode = vp;
		ret_val = monitor_read_object(uio->uio_td->td_proc, &lobj);
		if (ret_val == 0)
			ret_val = (*orig_pr_usrreqs(so)->pru_soreceive)(so,
		 	    paddr, uio, mp0, controlp, flagsp);
	} else {
		/*
		 * This is a receive from a socket in a pair created by
		 * socketpair().  Monitor it as we would a pipe read,
		 * except for allowing for arbitrary numbers of sleeps.
		 */
		ret_val = monitored_soreceive(so, paddr, uio, mp0, controlp,
		   flagsp);
	}
	return (ret_val);
}

int
lomac_soreceive(struct socket *so, struct sockaddr **paddr, struct uio *uio,
    struct mbuf **mp0, struct mbuf **controlp, int *flagsp) {
	int ret_val;     /* value to return to caller */
	
	(void)monitor_read_net_socket(uio->uio_td->td_proc);
	ret_val = (*orig_pr_usrreqs(so)->pru_soreceive)(so, paddr, uio, mp0,
            controlp, flagsp);
	return (ret_val);
}

int
lomac_initialize_sockets(void) {
	struct domain *dp;         /* used to traverse global `domains' list */
	struct protosw *pr;   /* used to traverse each domain's protosw list */
	struct lomac_pr_usrreqs *lomac_pr_usrreqs;  /* lomac usrreqs vectors */
	void (**lfuncp)(void), (**funcp)(void);
	int n, nreq;

	nreq = sizeof(struct pr_usrreqs) / sizeof(void (*)(void));
	for (dp = domains; dp; dp = dp->dom_next) {
		for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++) {

			lomac_pr_usrreqs = (struct lomac_pr_usrreqs *)malloc(
			    sizeof(struct lomac_pr_usrreqs), M_LOMAC_USRREQS,
			    M_WAITOK);

			if (dp->dom_family == AF_LOCAL)
				memcpy(lomac_pr_usrreqs, &lomac_local_usrreqs, 
				    sizeof(struct pr_usrreqs));
			else
				memcpy(lomac_pr_usrreqs, &lomac_net_usrreqs, 
				    sizeof(struct pr_usrreqs));
			/*
			 * Do sparse allocation of user requests and only
			 * override the ones we need to (to reduce overhead).
			 */
			lfuncp = (void (**)(void))lomac_pr_usrreqs;
			funcp = (void (**)(void))pr->pr_usrreqs;
			for (n = 0; n < nreq; n++) {
				if (lfuncp[n] == NULL)
					lfuncp[n] = funcp[n];
			}
			lomac_pr_usrreqs->orig_pr_usrreqs = pr->pr_usrreqs;
			pr->pr_usrreqs = (struct pr_usrreqs *)lomac_pr_usrreqs;
		}
	}
	return (0);
}


int
lomac_uninitialize_sockets(void) {
	struct domain *dp;         /* used to traverse global `domains' list */
	struct protosw *pr;   /* used to traverse each domain's protosw list */
	struct lomac_pr_usrreqs *lomac_pr_usrreqs;  /* lomac usrreqs vectors */

	for (dp = domains; dp; dp = dp->dom_next) {
		for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++) {
			lomac_pr_usrreqs = (struct lomac_pr_usrreqs *)
			    pr->pr_usrreqs;
			pr->pr_usrreqs = lomac_pr_usrreqs->orig_pr_usrreqs;
			free(lomac_pr_usrreqs, M_LOMAC_USRREQS);
		}
	}
	return (0);
}

#define	SBLOCKWAIT(f)	(((f) & MSG_DONTWAIT) ? M_NOWAIT : M_WAITOK)
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
static int
monitored_soreceive(so, psa, uio, mp0, controlp, flagsp)
	register struct socket *so;
	struct sockaddr **psa;
	struct uio *uio;
	struct mbuf **mp0;
	struct mbuf **controlp;
	int *flagsp;
{
	lomac_object_t lobj;
	register struct mbuf *m, **mp;
	register int flags, len, error, s, offset;
	struct protosw *pr = so->so_proto;
	struct mbuf *nextrecord;
	struct proc *p;
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
	lobj.lo_type = LO_TYPE_SOCKETPAIR;
	lobj.lo_object.socket = so;
	if (uio->uio_td != NULL)		/* XXX */
		p = uio->uio_td->td_proc;
	else
		p = curthread->td_proc;
	if (flags & MSG_OOB) {
		m = m_get(M_TRYWAIT, MT_DATA);
		if (m == NULL)
			return (ENOBUFS);
		error = (*pr->pr_usrreqs->pru_rcvoob)(so, m, flags & MSG_PEEK);
		if (error)
			goto bad;
		do {
			monitor_read_object(p, &lobj);
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
		p->p_stats->p_ru.ru_msgrcv++;
	nextrecord = m->m_nextpkt;
	if (pr->pr_flags & PR_ADDR) {
		KASSERT(m->m_type == MT_SONAME, ("receive 1a"));
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
			("receive 3"));
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
			monitor_read_object(p, &lobj);
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
