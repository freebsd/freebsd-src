/*	$FreeBSD$	*/
/*	$NetBSD: kttcp.c,v 1.3 2002/07/03 19:36:52 thorpej Exp $	*/

/*
 * Copyright (c) 2002 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Frank van der Linden and Jason R. Thorpe for
 * Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * kttcp.c --
 *
 *	This module provides kernel support for testing network
 *	throughput from the perspective of the kernel.  It is
 *	similar in spirit to the classic ttcp network benchmark
 *	program, the main difference being that with kttcp, the
 *	kernel is the source and sink of the data.
 *
 *	Testing like this is useful for a few reasons:
 *
 *	1. This allows us to know what kind of performance we can
 *	   expect from network applications that run in the kernel
 *	   space, such as the NFS server or the NFS client.  These
 *	   applications don't have to move the data to/from userspace,
 *	   and so benchmark programs which run in userspace don't
 *	   give us an accurate model.
 *
 *	2. Since data received is just thrown away, the receiver
 *	   is very fast.  This can provide better exercise for the
 *	   sender at the other end.
 *
 *	3. Since the NetBSD kernel currently uses a run-to-completion
 *	   scheduling model, kttcp provides a benchmark model where
 *	   preemption of the benchmark program is not an issue.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/sysctl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/errno.h>
#include <sys/uio.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/fcntl.h>
#include <sys/protosw.h>
#include <sys/socketvar.h>
#include <sys/socket.h>
#include <sys/mbuf.h>
#include <sys/resourcevar.h>
#include <sys/proc.h>

#include <dev/kttcp/kttcpio.h>

#ifndef timersub
#define timersub(tvp, uvp, vvp)						\
	do {								\
		(vvp)->tv_sec = (tvp)->tv_sec - (uvp)->tv_sec;		\
		(vvp)->tv_usec = (tvp)->tv_usec - (uvp)->tv_usec;	\
		if ((vvp)->tv_usec < 0) {				\
			(vvp)->tv_sec--;				\
			(vvp)->tv_usec += 1000000;			\
		}							\
	} while (0)
#endif

static int kttcp_send(struct thread *p, struct kttcp_io_args *);
static int kttcp_recv(struct thread *p, struct kttcp_io_args *);
static int kttcp_sosend(struct socket *, unsigned long long,
			unsigned long long *, struct thread *, int);
static int kttcp_soreceive(struct socket *, unsigned long long,
			   unsigned long long *, struct thread *, int *);

static d_open_t		kttcpopen;
static d_ioctl_t	kttcpioctl;

static struct cdevsw kttcp_cdevsw = {
	.d_open =	kttcpopen,
	.d_ioctl =	kttcpioctl,
	.d_name =	"kttcp",
	.d_maj =	MAJOR_AUTO,
};

static int
kttcpopen(dev_t dev, int flag, int mode, struct thread *td)
{
	/* Always succeeds. */
	return (0);
}

static int
kttcpioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct thread *td)
{
	int error;

	if ((flag & FWRITE) == 0)
		return EPERM;

	switch (cmd) {
	case KTTCP_IO_SEND:
		error = kttcp_send(td, (struct kttcp_io_args *) data);
		break;

	case KTTCP_IO_RECV:
		error = kttcp_recv(td, (struct kttcp_io_args *) data);
		break;

	default:
		return EINVAL;
	}

	return error;
}

static int
kttcp_send(struct thread *td, struct kttcp_io_args *kio)
{
	struct file *fp;
	int error;
	struct timeval t0, t1;
	unsigned long long len = 0;
	unsigned long long done;

	if (kio->kio_totalsize >= KTTCP_MAX_XMIT)
		return EINVAL;

	error = fget(td, kio->kio_socket, &fp);
	if (error != 0)
		return error;
	mtx_lock(&Giant);
	if ((fp->f_flag & FWRITE) == 0) {
		fdrop(fp, td);
		mtx_unlock(&Giant);
		return EBADF;
	}
	if (fp->f_type == DTYPE_SOCKET) {
		len = kio->kio_totalsize;
		microtime(&t0);
		do {
			error = kttcp_sosend((struct socket *)fp->f_data, len,
			    &done, td, 0);
			len -= done;
		} while (error == 0 && len > 0);
		microtime(&t1);
	} else
		error = EFTYPE;
	fdrop(fp, td);
	mtx_unlock(&Giant);
	if (error != 0)
		return error;
	timersub(&t1, &t0, &kio->kio_elapsed);

	kio->kio_bytesdone = kio->kio_totalsize - len;

	return 0;
}

static int
kttcp_recv(struct thread *td, struct kttcp_io_args *kio)
{
	struct file *fp;
	int error;
	struct timeval t0, t1;
	unsigned long long len = 0;
	unsigned long long done;

	if (kio->kio_totalsize > KTTCP_MAX_XMIT)
		return EINVAL;

	error = fget(td, kio->kio_socket, &fp);
	if (error != 0)
		return error;
	mtx_lock(&Giant);
	if ((fp->f_flag & FWRITE) == 0) {
		fdrop(fp, td);
		mtx_unlock(&Giant);
		return EBADF;
	}
	if (fp->f_type == DTYPE_SOCKET) {
		len = kio->kio_totalsize;
		microtime(&t0);
		do {
			error = kttcp_soreceive((struct socket *)fp->f_data,
			    len, &done, td, NULL);
			len -= done;
		} while (error == 0 && len > 0 && done > 0);
		microtime(&t1);
		if (error == EPIPE)
			error = 0;
	} else
		error = EFTYPE;
	fdrop(fp, td);
	mtx_unlock(&Giant);
	if (error != 0)
		return error;
	timersub(&t1, &t0, &kio->kio_elapsed);

	kio->kio_bytesdone = kio->kio_totalsize - len;

	return 0;
}

#define SBLOCKWAIT(f)   (((f) & MSG_DONTWAIT) ? M_NOWAIT : M_WAITOK)

/*
 * Slightly changed version of sosend()
 */
int
kttcp_sosend(struct socket *so, unsigned long long slen,
	     unsigned long long *done, struct thread *td, int flags)
{
	struct mbuf **mp, *m, *top;
	long space, len, mlen;
	int error, s, dontroute, atomic;
	long long resid;

	atomic = sosendallatonce(so);
	resid = slen;
	top = NULL;
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
				    !(resid == 0))
					snderr(ENOTCONN);
			} else
				snderr(EDESTADDRREQ);
		}
		space = sbspace(&so->so_snd);
		if (flags & MSG_OOB)
			space += 1024;
		if (atomic && resid > so->so_snd.sb_hiwat)
			snderr(EMSGSIZE);
		if (space < resid && (atomic || space < so->so_snd.sb_lowat)) {
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
		do {
			do {
				if (top == 0) {
					MGETHDR(m, M_WAIT, MT_DATA);
					if (m == NULL) {
						error = ENOBUFS;
						goto release;
					}
					mlen = MHLEN;
					m->m_pkthdr.len = 0;
					m->m_pkthdr.rcvif = (struct ifnet *)0;
				} else {
					MGET(m, M_WAIT, MT_DATA);
					if (m == NULL) {
						error = ENOBUFS;
						goto release;
					}
					mlen = MLEN;
				}
				if (resid >= MINCLSIZE) {
					MCLGET(m, M_WAIT);
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
				resid -= len;
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
			    top, NULL, NULL, td);
			splx(s);
			if (dontroute)
				so->so_options &= ~SO_DONTROUTE;
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
	*done = slen - resid;
	return (error);
}

int
kttcp_soreceive(struct socket *so, unsigned long long slen,
		unsigned long long *done, struct thread *td, int *flagsp)
{
	struct mbuf *m, **mp;
	int flags, len, error, s, offset;
	struct protosw *pr;
	struct mbuf *nextrecord;
	int moff, type;
	long long orig_resid, resid;

	pr = so->so_proto;
	mp = NULL;
	type = 0;
	resid = orig_resid = slen;
	if (flagsp)
		flags = *flagsp &~ MSG_EOR;
	else
		flags = 0;
	if (flags & MSG_OOB) {
		m = m_get(M_WAIT, MT_DATA);
		if (m == NULL)
			return (ENOBUFS);
		error = (*pr->pr_usrreqs->pru_rcvoob)(so, m, flags & MSG_PEEK);
		if (error)
			goto bad;
		do {
			resid -= min(resid, m->m_len);
			m = m_free(m);
		} while (resid && error == 0 && m);
bad:
		if (m)
			m_freem(m);
		return (error);
	}
	if (mp)
		*mp = (struct mbuf *)0;
	if (so->so_state & SS_ISCONFIRMING && resid)
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
	    so->so_rcv.sb_cc < resid) &&
	    (so->so_rcv.sb_cc < so->so_rcv.sb_lowat ||
	    ((flags & MSG_WAITALL) && resid <= so->so_rcv.sb_hiwat)) &&
	    m->m_nextpkt == 0 && (pr->pr_flags & PR_ATOMIC) == 0)) {
		KASSERT(m != 0 || !so->so_rcv.sb_cc, ("receive 1"));
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
		if (resid == 0)
			goto release;
		if ((so->so_state & SS_NBIO) || (flags & MSG_DONTWAIT)) {
			error = EWOULDBLOCK;
			goto release;
		}
		SBLASTRECORDCHK(&so->so_rcv);
		SBLASTMBUFCHK(&so->so_rcv);
		sbunlock(&so->so_rcv);
		error = sbwait(&so->so_rcv);
		splx(s);
		if (error)
			return (error);
		goto restart;
	}
dontblock:
	/*
	 * On entry here, m points to the first record of the socket buffer.
	 * While we process the initial mbufs containing address and control
	 * info, we save a copy of m->m_nextpkt into nextrecord.
	 */
	if (td)
		td->td_proc->p_stats->p_ru.ru_msgrcv++;
	KASSERT(m == so->so_rcv.sb_mb, ("receive 1b"));
	SBLASTRECORDCHK(&so->so_rcv);
	SBLASTMBUFCHK(&so->so_rcv);
	nextrecord = m->m_nextpkt;
	if (pr->pr_flags & PR_ADDR) {
		KASSERT(m->m_type == MT_SONAME, ("receive 1a"));
		orig_resid = 0;
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
			m = m->m_next;
		} else {
			sbfree(&so->so_rcv, m);
			so->so_rcv.sb_mb = m_free(m);
			m = so->so_rcv.sb_mb;
		}
	}

	/*
	 * If m is non-NULL, we have some data to read.  From now on,
	 * make sure to keep sb_lastrecord consistent when working on
	 * the last packet on the chain (nextrecord == NULL) and we
	 * change m->m_nextpkt.
	 */
	if (m) {
		if ((flags & MSG_PEEK) == 0) {
			m->m_nextpkt = nextrecord;
			/*
			 * If nextrecord == NULL (this is a single chain),
			 * then sb_lastrecord may not be valid here if m
			 * was changed earlier.
			 */
			if (nextrecord == NULL) {
				KASSERT(so->so_rcv.sb_mb == m, ("receive 1c"));
				so->so_rcv.sb_lastrecord = m;
			}
		}
		type = m->m_type;
		if (type == MT_OOBDATA)
			flags |= MSG_OOB;
	} else {
		if ((flags & MSG_PEEK) == 0) {
			KASSERT(so->so_rcv.sb_mb == m, ("receive 1d"));
			so->so_rcv.sb_mb = nextrecord;
			SB_EMPTY_FIXUP(&so->so_rcv);
		}
	}
	SBLASTRECORDCHK(&so->so_rcv);
	SBLASTMBUFCHK(&so->so_rcv);

	moff = 0;
	offset = 0;
	while (m && resid > 0 && error == 0) {
		if (m->m_type == MT_OOBDATA) {
			if (type != MT_OOBDATA)
				break;
		} else if (type == MT_OOBDATA)
			break;
		else
		    KASSERT(m->m_type == MT_DATA || m->m_type == MT_HEADER,
			("receive 3"));
		so->so_state &= ~SS_RCVATMARK;
		len = resid;
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
		resid -= len;
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
					so->so_rcv.sb_mb = m = m_free(m);
				}
				/*
				 * If m != NULL, we also know that
				 * so->so_rcv.sb_mb != NULL.
				 */
				KASSERT(so->so_rcv.sb_mb == m, ("receive 3a"));
				if (m) {
					m->m_nextpkt = nextrecord;
					if (nextrecord == NULL)
						so->so_rcv.sb_lastrecord = m;
				} else {
					so->so_rcv.sb_mb = nextrecord;
					SB_EMPTY_FIXUP(&so->so_rcv);
				}
				SBLASTRECORDCHK(&so->so_rcv);
				SBLASTMBUFCHK(&so->so_rcv);
			}
		} else {
			if (flags & MSG_PEEK)
				moff += len;
			else {
				if (mp)
					*mp = m_copym(m, 0, len, M_WAIT);
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
		while (flags & MSG_WAITALL && m == 0 && resid > 0 &&
		    !sosendallatonce(so) && !nextrecord) {
			if (so->so_error || so->so_state & SS_CANTRCVMORE)
				break;
			/*
			 * The window might have closed to zero, make
			 * sure we send an ack now that we've drained
			 * the buffer or we might end up blocking until
			 * the idle takes over (5 seconds).
			 */
			if (pr->pr_flags & PR_WANTRCVD && so->so_pcb)
				(*pr->pr_usrreqs->pru_rcvd)(so, flags);
			SBLASTRECORDCHK(&so->so_rcv);
			SBLASTMBUFCHK(&so->so_rcv);
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
		if (m == 0) {
			/*
			 * First part is an inline SB_EMPTY_FIXUP().  Second
			 * part makes sure sb_lastrecord is up-to-date if
			 * there is still data in the socket buffer.
			 */
			so->so_rcv.sb_mb = nextrecord;
			if (so->so_rcv.sb_mb == NULL) {
				so->so_rcv.sb_mbtail = NULL;
				so->so_rcv.sb_lastrecord = NULL;
			} else if (nextrecord->m_nextpkt == NULL)
				so->so_rcv.sb_lastrecord = nextrecord;
		}
		SBLASTRECORDCHK(&so->so_rcv);
		SBLASTMBUFCHK(&so->so_rcv);
		if (pr->pr_flags & PR_WANTRCVD && so->so_pcb)
			(*pr->pr_usrreqs->pru_rcvd)(so, flags);
	}
	if (orig_resid == resid && orig_resid &&
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
	*done = slen - resid;
#if 0
	printf("soreceive: error %d slen %llu resid %lld\n", error, slen, resid);
#endif
	return (error);
}

static dev_t kttcp_dev;

/*
 * Initialization code, both for static and dynamic loading.
 */
static int
kttcpdev_modevent(module_t mod, int type, void *unused)
{
	switch (type) {
	case MOD_LOAD:
		kttcp_dev = make_dev(&kttcp_cdevsw, 0,
				      UID_ROOT, GID_WHEEL, 0666,
				      "kttcp");
		return 0;
	case MOD_UNLOAD:
		/*XXX disallow if active sessions */
		destroy_dev(kttcp_dev);
		return 0;
	}
	return EINVAL;
}

static moduledata_t kttcpdev_mod = {
	"kttcpdev",
	kttcpdev_modevent,
	0
};
MODULE_VERSION(kttcpdev, 1);
DECLARE_MODULE(kttcpdev, kttcpdev_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);
