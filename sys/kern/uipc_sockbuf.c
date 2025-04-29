/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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
 * 3. Neither the name of the University nor the names of its contributors
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

#include <sys/cdefs.h>
#include "opt_kern_tls.h"
#include "opt_param.h"

#include <sys/param.h>
#include <sys/aio.h> /* for aio_swake proto */
#include <sys/kernel.h>
#include <sys/ktls.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/msan.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sx.h>
#include <sys/sysctl.h>

#include <netinet/in.h>

/*
 * Function pointer set by the AIO routines so that the socket buffer code
 * can call back into the AIO module if it is loaded.
 */
void	(*aio_swake)(struct socket *, struct sockbuf *);

/*
 * Primitive routines for operating on socket buffers
 */

#define	BUF_MAX_ADJ(_sz)	(((u_quad_t)(_sz)) * MCLBYTES / (MSIZE + MCLBYTES))

u_long	sb_max = SB_MAX;
u_long sb_max_adj = BUF_MAX_ADJ(SB_MAX);

static	u_long sb_efficiency = 8;	/* parameter for sbreserve() */

#ifdef KERN_TLS
static void	sbcompress_ktls_rx(struct sockbuf *sb, struct mbuf *m,
    struct mbuf *n);
#endif
static struct mbuf	*sbcut_internal(struct sockbuf *sb, int len);
static void		sbunreserve_locked(struct socket *so, sb_which which);

/*
 * Our own version of m_clrprotoflags(), that can preserve M_NOTREADY.
 */
static void
sbm_clrprotoflags(struct mbuf *m, int flags)
{
	int mask;

	mask = ~M_PROTOFLAGS;
	if (flags & PRUS_NOTREADY)
		mask |= M_NOTREADY;
	while (m) {
		m->m_flags &= mask;
		m = m->m_next;
	}
}

/*
 * Compress M_NOTREADY mbufs after they have been readied by sbready().
 *
 * sbcompress() skips M_NOTREADY mbufs since the data is not available to
 * be copied at the time of sbcompress().  This function combines small
 * mbufs similar to sbcompress() once mbufs are ready.  'm0' is the first
 * mbuf sbready() marked ready, and 'end' is the first mbuf still not
 * ready.
 */
static void
sbready_compress(struct sockbuf *sb, struct mbuf *m0, struct mbuf *end)
{
	struct mbuf *m, *n;
	int ext_size;

	SOCKBUF_LOCK_ASSERT(sb);

	if ((sb->sb_flags & SB_NOCOALESCE) != 0)
		return;

	for (m = m0; m != end; m = m->m_next) {
		MPASS((m->m_flags & M_NOTREADY) == 0);
		/*
		 * NB: In sbcompress(), 'n' is the last mbuf in the
		 * socket buffer and 'm' is the new mbuf being copied
		 * into the trailing space of 'n'.  Here, the roles
		 * are reversed and 'n' is the next mbuf after 'm'
		 * that is being copied into the trailing space of
		 * 'm'.
		 */
		n = m->m_next;
#ifdef KERN_TLS
		/* Try to coalesce adjacent ktls mbuf hdr/trailers. */
		if ((n != NULL) && (n != end) && (m->m_flags & M_EOR) == 0 &&
		    (m->m_flags & M_EXTPG) &&
		    (n->m_flags & M_EXTPG) &&
		    !mbuf_has_tls_session(m) &&
		    !mbuf_has_tls_session(n)) {
			int hdr_len, trail_len;

			hdr_len = n->m_epg_hdrlen;
			trail_len = m->m_epg_trllen;
			if (trail_len != 0 && hdr_len != 0 &&
			    trail_len + hdr_len <= MBUF_PEXT_TRAIL_LEN) {
				/* copy n's header to m's trailer */
				memcpy(&m->m_epg_trail[trail_len],
				    n->m_epg_hdr, hdr_len);
				m->m_epg_trllen += hdr_len;
				m->m_len += hdr_len;
				n->m_epg_hdrlen = 0;
				n->m_len -= hdr_len;
			}
		}
#endif

		/* Compress small unmapped mbufs into plain mbufs. */
		if ((m->m_flags & M_EXTPG) && m->m_len <= MLEN &&
		    !mbuf_has_tls_session(m)) {
			ext_size = m->m_ext.ext_size;
			if (mb_unmapped_compress(m) == 0)
				sb->sb_mbcnt -= ext_size;
		}

		while ((n != NULL) && (n != end) && (m->m_flags & M_EOR) == 0 &&
		    M_WRITABLE(m) &&
		    (m->m_flags & M_EXTPG) == 0 &&
		    !mbuf_has_tls_session(n) &&
		    !mbuf_has_tls_session(m) &&
		    n->m_len <= MCLBYTES / 4 && /* XXX: Don't copy too much */
		    n->m_len <= M_TRAILINGSPACE(m) &&
		    m->m_type == n->m_type) {
			KASSERT(sb->sb_lastrecord != n,
		    ("%s: merging start of record (%p) into previous mbuf (%p)",
			    __func__, n, m));
			m_copydata(n, 0, n->m_len, mtodo(m, m->m_len));
			m->m_len += n->m_len;
			m->m_next = n->m_next;
			m->m_flags |= n->m_flags & M_EOR;
			if (sb->sb_mbtail == n)
				sb->sb_mbtail = m;

			sb->sb_mbcnt -= MSIZE;
			if (n->m_flags & M_EXT)
				sb->sb_mbcnt -= n->m_ext.ext_size;
			m_free(n);
			n = m->m_next;
		}
	}
	SBLASTRECORDCHK(sb);
	SBLASTMBUFCHK(sb);
}

/*
 * Mark ready "count" units of I/O starting with "m".  Most mbufs
 * count as a single unit of I/O except for M_EXTPG mbufs which
 * are backed by multiple pages.
 */
int
sbready(struct sockbuf *sb, struct mbuf *m0, int count)
{
	struct mbuf *m;
	u_int blocker;

	SOCKBUF_LOCK_ASSERT(sb);
	KASSERT(sb->sb_fnrdy != NULL, ("%s: sb %p NULL fnrdy", __func__, sb));
	KASSERT(count > 0, ("%s: invalid count %d", __func__, count));

	m = m0;
	blocker = (sb->sb_fnrdy == m) ? M_BLOCKED : 0;

	while (count > 0) {
		KASSERT(m->m_flags & M_NOTREADY,
		    ("%s: m %p !M_NOTREADY", __func__, m));
		if ((m->m_flags & M_EXTPG) != 0 && m->m_epg_npgs != 0) {
			if (count < m->m_epg_nrdy) {
				m->m_epg_nrdy -= count;
				count = 0;
				break;
			}
			count -= m->m_epg_nrdy;
			m->m_epg_nrdy = 0;
		} else
			count--;

		m->m_flags &= ~(M_NOTREADY | blocker);
		if (blocker)
			sb->sb_acc += m->m_len;
		m = m->m_next;
	}

	/*
	 * If the first mbuf is still not fully ready because only
	 * some of its backing pages were readied, no further progress
	 * can be made.
	 */
	if (m0 == m) {
		MPASS(m->m_flags & M_NOTREADY);
		return (EINPROGRESS);
	}

	if (!blocker) {
		sbready_compress(sb, m0, m);
		return (EINPROGRESS);
	}

	/* This one was blocking all the queue. */
	for (; m && (m->m_flags & M_NOTREADY) == 0; m = m->m_next) {
		KASSERT(m->m_flags & M_BLOCKED,
		    ("%s: m %p !M_BLOCKED", __func__, m));
		m->m_flags &= ~M_BLOCKED;
		sb->sb_acc += m->m_len;
	}

	sb->sb_fnrdy = m;
	sbready_compress(sb, m0, m);

	return (0);
}

/*
 * Adjust sockbuf state reflecting allocation of m.
 */
void
sballoc(struct sockbuf *sb, struct mbuf *m)
{

	SOCKBUF_LOCK_ASSERT(sb);

	sb->sb_ccc += m->m_len;

	if (sb->sb_fnrdy == NULL) {
		if (m->m_flags & M_NOTREADY)
			sb->sb_fnrdy = m;
		else
			sb->sb_acc += m->m_len;
	} else
		m->m_flags |= M_BLOCKED;

	if (m->m_type != MT_DATA && m->m_type != MT_OOBDATA)
		sb->sb_ctl += m->m_len;

	sb->sb_mbcnt += MSIZE;

	if (m->m_flags & M_EXT)
		sb->sb_mbcnt += m->m_ext.ext_size;
}

/*
 * Adjust sockbuf state reflecting freeing of m.
 */
void
sbfree(struct sockbuf *sb, struct mbuf *m)
{

#if 0	/* XXX: not yet: soclose() call path comes here w/o lock. */
	SOCKBUF_LOCK_ASSERT(sb);
#endif

	sb->sb_ccc -= m->m_len;

	if (!(m->m_flags & M_NOTAVAIL))
		sb->sb_acc -= m->m_len;

	if (m == sb->sb_fnrdy) {
		struct mbuf *n;

		KASSERT(m->m_flags & M_NOTREADY,
		    ("%s: m %p !M_NOTREADY", __func__, m));

		n = m->m_next;
		while (n != NULL && !(n->m_flags & M_NOTREADY)) {
			n->m_flags &= ~M_BLOCKED;
			sb->sb_acc += n->m_len;
			n = n->m_next;
		}
		sb->sb_fnrdy = n;
	}

	if (m->m_type != MT_DATA && m->m_type != MT_OOBDATA)
		sb->sb_ctl -= m->m_len;

	sb->sb_mbcnt -= MSIZE;
	if (m->m_flags & M_EXT)
		sb->sb_mbcnt -= m->m_ext.ext_size;

	if (sb->sb_sndptr == m) {
		sb->sb_sndptr = NULL;
		sb->sb_sndptroff = 0;
	}
	if (sb->sb_sndptroff != 0)
		sb->sb_sndptroff -= m->m_len;
}

#ifdef KERN_TLS
/*
 * Similar to sballoc/sbfree but does not adjust state associated with
 * the sb_mb chain such as sb_fnrdy or sb_sndptr*.  Also assumes mbufs
 * are not ready.
 */
void
sballoc_ktls_rx(struct sockbuf *sb, struct mbuf *m)
{

	SOCKBUF_LOCK_ASSERT(sb);

	sb->sb_ccc += m->m_len;
	sb->sb_tlscc += m->m_len;

	sb->sb_mbcnt += MSIZE;

	if (m->m_flags & M_EXT)
		sb->sb_mbcnt += m->m_ext.ext_size;
}

void
sbfree_ktls_rx(struct sockbuf *sb, struct mbuf *m)
{

#if 0	/* XXX: not yet: soclose() call path comes here w/o lock. */
	SOCKBUF_LOCK_ASSERT(sb);
#endif

	sb->sb_ccc -= m->m_len;
	sb->sb_tlscc -= m->m_len;

	sb->sb_mbcnt -= MSIZE;

	if (m->m_flags & M_EXT)
		sb->sb_mbcnt -= m->m_ext.ext_size;
}
#endif

/*
 * Socantsendmore indicates that no more data will be sent on the socket; it
 * would normally be applied to a socket when the user informs the system
 * that no more data is to be sent, by the protocol code (in case
 * PRU_SHUTDOWN).  Socantrcvmore indicates that no more data will be
 * received, and will normally be applied to the socket by a protocol when it
 * detects that the peer will send no more data.  Data queued for reading in
 * the socket may yet be read.
 */
void
socantsendmore_locked(struct socket *so)
{

	SOCK_SENDBUF_LOCK_ASSERT(so);

	so->so_snd.sb_state |= SBS_CANTSENDMORE;
	sowwakeup_locked(so);
	SOCK_SENDBUF_UNLOCK_ASSERT(so);
}

void
socantsendmore(struct socket *so)
{

	SOCK_SENDBUF_LOCK(so);
	socantsendmore_locked(so);
	SOCK_SENDBUF_UNLOCK_ASSERT(so);
}

void
socantrcvmore_locked(struct socket *so)
{

	SOCK_RECVBUF_LOCK_ASSERT(so);

	so->so_rcv.sb_state |= SBS_CANTRCVMORE;
#ifdef KERN_TLS
	if (so->so_rcv.sb_flags & SB_TLS_RX)
		ktls_check_rx(&so->so_rcv);
#endif
	sorwakeup_locked(so);
	SOCK_RECVBUF_UNLOCK_ASSERT(so);
}

void
socantrcvmore(struct socket *so)
{

	SOCK_RECVBUF_LOCK(so);
	socantrcvmore_locked(so);
	SOCK_RECVBUF_UNLOCK_ASSERT(so);
}

void
soroverflow_locked(struct socket *so)
{

	SOCK_RECVBUF_LOCK_ASSERT(so);

	if (so->so_options & SO_RERROR) {
		so->so_rerror = ENOBUFS;
		sorwakeup_locked(so);
	} else
		SOCK_RECVBUF_UNLOCK(so);

	SOCK_RECVBUF_UNLOCK_ASSERT(so);
}

void
soroverflow(struct socket *so)
{

	SOCK_RECVBUF_LOCK(so);
	soroverflow_locked(so);
	SOCK_RECVBUF_UNLOCK_ASSERT(so);
}

/*
 * Wait for data to arrive at/drain from a socket buffer.
 */
int
sbwait(struct socket *so, sb_which which)
{
	struct sockbuf *sb;

	SOCK_BUF_LOCK_ASSERT(so, which);

	sb = sobuf(so, which);
	sb->sb_flags |= SB_WAIT;
	return (msleep_sbt(&sb->sb_acc, soeventmtx(so, which),
	    PSOCK | PCATCH, "sbwait", sb->sb_timeo, 0, 0));
}

/*
 * Wakeup processes waiting on a socket buffer.  Do asynchronous notification
 * via SIGIO if the socket has the SS_ASYNC flag set.
 *
 * Called with the socket buffer lock held; will release the lock by the end
 * of the function.  This allows the caller to acquire the socket buffer lock
 * while testing for the need for various sorts of wakeup and hold it through
 * to the point where it's no longer required.  We currently hold the lock
 * through calls out to other subsystems (with the exception of kqueue), and
 * then release it to avoid lock order issues.  It's not clear that's
 * correct.
 */
static __always_inline void
sowakeup(struct socket *so, const sb_which which)
{
	struct sockbuf *sb;
	int ret;

	SOCK_BUF_LOCK_ASSERT(so, which);

	sb = sobuf(so, which);
	selwakeuppri(sb->sb_sel, PSOCK);
	if (!SEL_WAITING(sb->sb_sel))
		sb->sb_flags &= ~SB_SEL;
	if (sb->sb_flags & SB_WAIT) {
		sb->sb_flags &= ~SB_WAIT;
		wakeup(&sb->sb_acc);
	}
	KNOTE_LOCKED(&sb->sb_sel->si_note, 0);
	if (sb->sb_upcall != NULL) {
		ret = sb->sb_upcall(so, sb->sb_upcallarg, M_NOWAIT);
		if (ret == SU_ISCONNECTED) {
			KASSERT(sb == &so->so_rcv,
			    ("SO_SND upcall returned SU_ISCONNECTED"));
			soupcall_clear(so, SO_RCV);
		}
	} else
		ret = SU_OK;
	if (sb->sb_flags & SB_AIO)
		sowakeup_aio(so, which);
	SOCK_BUF_UNLOCK(so, which);
	if (ret == SU_ISCONNECTED)
		soisconnected(so);
	if ((so->so_state & SS_ASYNC) && so->so_sigio != NULL)
		pgsigio(&so->so_sigio, SIGIO, 0);
	SOCK_BUF_UNLOCK_ASSERT(so, which);
}

static void
splice_push(struct socket *so)
{
	struct so_splice *sp;

	SOCK_RECVBUF_LOCK_ASSERT(so);

	sp = so->so_splice;
	mtx_lock(&sp->mtx);
	SOCK_RECVBUF_UNLOCK(so);
	so_splice_dispatch(sp);
}

static void
splice_pull(struct socket *so)
{
	struct so_splice *sp;

	SOCK_SENDBUF_LOCK_ASSERT(so);

	sp = so->so_splice_back;
	mtx_lock(&sp->mtx);
	SOCK_SENDBUF_UNLOCK(so);
	so_splice_dispatch(sp);
}

/*
 * Do we need to notify the other side when I/O is possible?
 */
static __always_inline bool
sb_notify(const struct sockbuf *sb)
{
	return ((sb->sb_flags & (SB_WAIT | SB_SEL | SB_ASYNC |
	    SB_UPCALL | SB_AIO | SB_KNOTE)) != 0);
}

void
sorwakeup_locked(struct socket *so)
{
	SOCK_RECVBUF_LOCK_ASSERT(so);
	if (so->so_rcv.sb_flags & SB_SPLICED)
		splice_push(so);
	else if (sb_notify(&so->so_rcv))
		sowakeup(so, SO_RCV);
	else
		SOCK_RECVBUF_UNLOCK(so);
}

void
sowwakeup_locked(struct socket *so)
{
	SOCK_SENDBUF_LOCK_ASSERT(so);
	if (so->so_snd.sb_flags & SB_SPLICED)
		splice_pull(so);
	else if (sb_notify(&so->so_snd))
		sowakeup(so, SO_SND);
	else
		SOCK_SENDBUF_UNLOCK(so);
}

/*
 * Socket buffer (struct sockbuf) utility routines.
 *
 * Each socket contains two socket buffers: one for sending data and one for
 * receiving data.  Each buffer contains a queue of mbufs, information about
 * the number of mbufs and amount of data in the queue, and other fields
 * allowing select() statements and notification on data availability to be
 * implemented.
 *
 * Data stored in a socket buffer is maintained as a list of records.  Each
 * record is a list of mbufs chained together with the m_next field.  Records
 * are chained together with the m_nextpkt field. The upper level routine
 * soreceive() expects the following conventions to be observed when placing
 * information in the receive buffer:
 *
 * 1. If the protocol requires each message be preceded by the sender's name,
 *    then a record containing that name must be present before any
 *    associated data (mbuf's must be of type MT_SONAME).
 * 2. If the protocol supports the exchange of ``access rights'' (really just
 *    additional data associated with the message), and there are ``rights''
 *    to be received, then a record containing this data should be present
 *    (mbuf's must be of type MT_RIGHTS).
 * 3. If a name or rights record exists, then it must be followed by a data
 *    record, perhaps of zero length.
 *
 * Before using a new socket structure it is first necessary to reserve
 * buffer space to the socket, by calling sbreserve().  This should commit
 * some of the available buffer space in the system buffer pool for the
 * socket (currently, it does nothing but enforce limits).  The space should
 * be released by calling sbrelease() when the socket is destroyed.
 */
int
soreserve(struct socket *so, u_long sndcc, u_long rcvcc)
{
	struct thread *td = curthread;

	SOCK_SENDBUF_LOCK(so);
	SOCK_RECVBUF_LOCK(so);
	if (sbreserve_locked(so, SO_SND, sndcc, td) == 0)
		goto bad;
	if (sbreserve_locked(so, SO_RCV, rcvcc, td) == 0)
		goto bad2;
	if (so->so_rcv.sb_lowat == 0)
		so->so_rcv.sb_lowat = 1;
	if (so->so_snd.sb_lowat == 0)
		so->so_snd.sb_lowat = MCLBYTES;
	if (so->so_snd.sb_lowat > so->so_snd.sb_hiwat)
		so->so_snd.sb_lowat = so->so_snd.sb_hiwat;
	SOCK_RECVBUF_UNLOCK(so);
	SOCK_SENDBUF_UNLOCK(so);
	return (0);
bad2:
	sbunreserve_locked(so, SO_SND);
bad:
	SOCK_RECVBUF_UNLOCK(so);
	SOCK_SENDBUF_UNLOCK(so);
	return (ENOBUFS);
}

static int
sysctl_handle_sb_max(SYSCTL_HANDLER_ARGS)
{
	int error = 0;
	u_long tmp_sb_max = sb_max;

	error = sysctl_handle_long(oidp, &tmp_sb_max, arg2, req);
	if (error || !req->newptr)
		return (error);
	if (tmp_sb_max < MSIZE + MCLBYTES)
		return (EINVAL);
	sb_max = tmp_sb_max;
	sb_max_adj = BUF_MAX_ADJ(sb_max);
	return (0);
}

/*
 * Allot mbufs to a sockbuf.  Attempt to scale mbmax so that mbcnt doesn't
 * become limiting if buffering efficiency is near the normal case.
 */
bool
sbreserve_locked_limit(struct socket *so, sb_which which, u_long cc,
    u_long buf_max, struct thread *td)
{
	struct sockbuf *sb = sobuf(so, which);
	rlim_t sbsize_limit;

	SOCK_BUF_LOCK_ASSERT(so, which);

	/*
	 * When a thread is passed, we take into account the thread's socket
	 * buffer size limit.  The caller will generally pass curthread, but
	 * in the TCP input path, NULL will be passed to indicate that no
	 * appropriate thread resource limits are available.  In that case,
	 * we don't apply a process limit.
	 */
	if (cc > BUF_MAX_ADJ(buf_max))
		return (false);
	if (td != NULL) {
		sbsize_limit = lim_cur(td, RLIMIT_SBSIZE);
	} else
		sbsize_limit = RLIM_INFINITY;
	if (!chgsbsize(so->so_cred->cr_uidinfo, &sb->sb_hiwat, cc,
	    sbsize_limit))
		return (false);
	sb->sb_mbmax = min(cc * sb_efficiency, buf_max);
	if (sb->sb_lowat > sb->sb_hiwat)
		sb->sb_lowat = sb->sb_hiwat;
	return (true);
}

bool
sbreserve_locked(struct socket *so, sb_which which, u_long cc,
    struct thread *td)
{
	return (sbreserve_locked_limit(so, which, cc, sb_max, td));
}

static void
sbunreserve_locked(struct socket *so, sb_which which)
{
	struct sockbuf *sb = sobuf(so, which);

	SOCK_BUF_LOCK_ASSERT(so, which);

	(void)chgsbsize(so->so_cred->cr_uidinfo, &sb->sb_hiwat, 0,
	    RLIM_INFINITY);
	sb->sb_mbmax = 0;
}

int
sbsetopt(struct socket *so, struct sockopt *sopt)
{
	struct sockbuf *sb;
	sb_which wh;
	short *flags;
	u_int cc, *hiwat, *lowat;
	int error, optval;

	error = sooptcopyin(sopt, &optval, sizeof optval, sizeof optval);
	if (error != 0)
		return (error);

	/*
	 * Values < 1 make no sense for any of these options,
	 * so disallow them.
	 */
	if (optval < 1)
		return (EINVAL);
	cc = optval;

	sb = NULL;
	SOCK_LOCK(so);
	if (SOLISTENING(so)) {
		switch (sopt->sopt_name) {
			case SO_SNDLOWAT:
			case SO_SNDBUF:
				lowat = &so->sol_sbsnd_lowat;
				hiwat = &so->sol_sbsnd_hiwat;
				flags = &so->sol_sbsnd_flags;
				break;
			case SO_RCVLOWAT:
			case SO_RCVBUF:
				lowat = &so->sol_sbrcv_lowat;
				hiwat = &so->sol_sbrcv_hiwat;
				flags = &so->sol_sbrcv_flags;
				break;
		}
	} else {
		switch (sopt->sopt_name) {
			case SO_SNDLOWAT:
			case SO_SNDBUF:
				sb = &so->so_snd;
				wh = SO_SND;
				break;
			case SO_RCVLOWAT:
			case SO_RCVBUF:
				sb = &so->so_rcv;
				wh = SO_RCV;
				break;
		}
		flags = &sb->sb_flags;
		hiwat = &sb->sb_hiwat;
		lowat = &sb->sb_lowat;
		SOCK_BUF_LOCK(so, wh);
	}

	error = 0;
	switch (sopt->sopt_name) {
	case SO_SNDBUF:
	case SO_RCVBUF:
		if (SOLISTENING(so)) {
			if (cc > sb_max_adj) {
				error = ENOBUFS;
				break;
			}
			*hiwat = cc;
			if (*lowat > *hiwat)
				*lowat = *hiwat;
		} else {
			if (!sbreserve_locked(so, wh, cc, curthread))
				error = ENOBUFS;
		}
		if (error == 0)
			*flags &= ~SB_AUTOSIZE;
		break;
	case SO_SNDLOWAT:
	case SO_RCVLOWAT:
		/*
		 * Make sure the low-water is never greater than the
		 * high-water.
		 */
		*lowat = (cc > *hiwat) ? *hiwat : cc;
		break;
	}

	if (!SOLISTENING(so))
		SOCK_BUF_UNLOCK(so, wh);
	SOCK_UNLOCK(so);
	return (error);
}

/*
 * Free mbufs held by a socket, and reserved mbuf space.
 */
void
sbrelease_locked(struct socket *so, sb_which which)
{
	struct sockbuf *sb = sobuf(so, which);

	SOCK_BUF_LOCK_ASSERT(so, which);

	sbflush_locked(sb);
	sbunreserve_locked(so, which);
}

void
sbrelease(struct socket *so, sb_which which)
{

	SOCK_BUF_LOCK(so, which);
	sbrelease_locked(so, which);
	SOCK_BUF_UNLOCK(so, which);
}

void
sbdestroy(struct socket *so, sb_which which)
{
#ifdef KERN_TLS
	struct sockbuf *sb = sobuf(so, which);

	if (sb->sb_tls_info != NULL)
		ktls_free(sb->sb_tls_info);
	sb->sb_tls_info = NULL;
#endif
	sbrelease_locked(so, which);
}

/*
 * Routines to add and remove data from an mbuf queue.
 *
 * The routines sbappend() or sbappendrecord() are normally called to append
 * new mbufs to a socket buffer, after checking that adequate space is
 * available, comparing the function sbspace() with the amount of data to be
 * added.  sbappendrecord() differs from sbappend() in that data supplied is
 * treated as the beginning of a new record.  To place a sender's address,
 * optional access rights, and data in a socket receive buffer,
 * sbappendaddr() should be used.  To place access rights and data in a
 * socket receive buffer, sbappendrights() should be used.  In either case,
 * the new data begins a new record.  Note that unlike sbappend() and
 * sbappendrecord(), these routines check for the caller that there will be
 * enough space to store the data.  Each fails if there is not enough space,
 * or if it cannot find mbufs to store additional information in.
 *
 * Reliable protocols may use the socket send buffer to hold data awaiting
 * acknowledgement.  Data is normally copied from a socket send buffer in a
 * protocol with m_copy for output to a peer, and then removing the data from
 * the socket buffer with sbdrop() or sbdroprecord() when the data is
 * acknowledged by the peer.
 */
#ifdef SOCKBUF_DEBUG
void
sblastrecordchk(struct sockbuf *sb, const char *file, int line)
{
	struct mbuf *m = sb->sb_mb;

	SOCKBUF_LOCK_ASSERT(sb);

	while (m && m->m_nextpkt)
		m = m->m_nextpkt;

	if (m != sb->sb_lastrecord) {
		printf("%s: sb_mb %p sb_lastrecord %p last %p\n",
			__func__, sb->sb_mb, sb->sb_lastrecord, m);
		printf("packet chain:\n");
		for (m = sb->sb_mb; m != NULL; m = m->m_nextpkt)
			printf("\t%p\n", m);
		panic("%s from %s:%u", __func__, file, line);
	}
}

void
sblastmbufchk(struct sockbuf *sb, const char *file, int line)
{
	struct mbuf *m = sb->sb_mb;
	struct mbuf *n;

	SOCKBUF_LOCK_ASSERT(sb);

	while (m && m->m_nextpkt)
		m = m->m_nextpkt;

	while (m && m->m_next)
		m = m->m_next;

	if (m != sb->sb_mbtail) {
		printf("%s: sb_mb %p sb_mbtail %p last %p\n",
			__func__, sb->sb_mb, sb->sb_mbtail, m);
		printf("packet tree:\n");
		for (m = sb->sb_mb; m != NULL; m = m->m_nextpkt) {
			printf("\t");
			for (n = m; n != NULL; n = n->m_next)
				printf("%p ", n);
			printf("\n");
		}
		panic("%s from %s:%u", __func__, file, line);
	}

#ifdef KERN_TLS
	m = sb->sb_mtls;
	while (m && m->m_next)
		m = m->m_next;

	if (m != sb->sb_mtlstail) {
		printf("%s: sb_mtls %p sb_mtlstail %p last %p\n",
			__func__, sb->sb_mtls, sb->sb_mtlstail, m);
		printf("TLS packet tree:\n");
		printf("\t");
		for (m = sb->sb_mtls; m != NULL; m = m->m_next) {
			printf("%p ", m);
		}
		printf("\n");
		panic("%s from %s:%u", __func__, file, line);
	}
#endif
}
#endif /* SOCKBUF_DEBUG */

#define SBLINKRECORD(sb, m0) do {					\
	SOCKBUF_LOCK_ASSERT(sb);					\
	if ((sb)->sb_lastrecord != NULL)				\
		(sb)->sb_lastrecord->m_nextpkt = (m0);			\
	else								\
		(sb)->sb_mb = (m0);					\
	(sb)->sb_lastrecord = (m0);					\
} while (/*CONSTCOND*/0)

/*
 * Append mbuf chain m to the last record in the socket buffer sb.  The
 * additional space associated the mbuf chain is recorded in sb.  Empty mbufs
 * are discarded and mbufs are compacted where possible.
 */
void
sbappend_locked(struct sockbuf *sb, struct mbuf *m, int flags)
{
	struct mbuf *n;

	SOCKBUF_LOCK_ASSERT(sb);

	if (m == NULL)
		return;
	kmsan_check_mbuf(m, "sbappend");
	sbm_clrprotoflags(m, flags);
	SBLASTRECORDCHK(sb);
	n = sb->sb_mb;
	if (n) {
		while (n->m_nextpkt)
			n = n->m_nextpkt;
		do {
			if (n->m_flags & M_EOR) {
				sbappendrecord_locked(sb, m); /* XXXXXX!!!! */
				return;
			}
		} while (n->m_next && (n = n->m_next));
	} else {
		/*
		 * XXX Would like to simply use sb_mbtail here, but
		 * XXX I need to verify that I won't miss an EOR that
		 * XXX way.
		 */
		if ((n = sb->sb_lastrecord) != NULL) {
			do {
				if (n->m_flags & M_EOR) {
					sbappendrecord_locked(sb, m); /* XXXXXX!!!! */
					return;
				}
			} while (n->m_next && (n = n->m_next));
		} else {
			/*
			 * If this is the first record in the socket buffer,
			 * it's also the last record.
			 */
			sb->sb_lastrecord = m;
		}
	}
	sbcompress(sb, m, n);
	SBLASTRECORDCHK(sb);
}

/*
 * Append mbuf chain m to the last record in the socket buffer sb.  The
 * additional space associated the mbuf chain is recorded in sb.  Empty mbufs
 * are discarded and mbufs are compacted where possible.
 */
void
sbappend(struct sockbuf *sb, struct mbuf *m, int flags)
{

	SOCKBUF_LOCK(sb);
	sbappend_locked(sb, m, flags);
	SOCKBUF_UNLOCK(sb);
}

#ifdef KERN_TLS
/*
 * Append an mbuf containing encrypted TLS data.  The data
 * is marked M_NOTREADY until it has been decrypted and
 * stored as a TLS record.
 */
static void
sbappend_ktls_rx(struct sockbuf *sb, struct mbuf *m)
{
	struct ifnet *ifp;
	struct mbuf *n;
	int flags;

	ifp = NULL;
	flags = M_NOTREADY;

	SBLASTMBUFCHK(sb);

	/* Mbuf chain must start with a packet header. */
	MPASS((m->m_flags & M_PKTHDR) != 0);

	/* Remove all packet headers and mbuf tags to get a pure data chain. */
	for (n = m; n != NULL; n = n->m_next) {
		if (n->m_flags & M_PKTHDR) {
			ifp = m->m_pkthdr.leaf_rcvif;
			if ((n->m_pkthdr.csum_flags & CSUM_TLS_MASK) ==
			    CSUM_TLS_DECRYPTED) {
				/* Mark all mbufs in this packet decrypted. */
				flags = M_NOTREADY | M_DECRYPTED;
			} else {
				flags = M_NOTREADY;
			}
			m_demote_pkthdr(n);
		}

		n->m_flags &= M_DEMOTEFLAGS;
		n->m_flags |= flags;

		MPASS((n->m_flags & M_NOTREADY) != 0);
	}

	sbcompress_ktls_rx(sb, m, sb->sb_mtlstail);
	ktls_check_rx(sb);

	/* Check for incoming packet route changes: */
	if (ifp != NULL && sb->sb_tls_info->rx_ifp != NULL &&
	    sb->sb_tls_info->rx_ifp != ifp)
		ktls_input_ifp_mismatch(sb, ifp);
}
#endif

/*
 * This version of sbappend() should only be used when the caller absolutely
 * knows that there will never be more than one record in the socket buffer,
 * that is, a stream protocol (such as TCP).
 */
void
sbappendstream_locked(struct sockbuf *sb, struct mbuf *m, int flags)
{
	SOCKBUF_LOCK_ASSERT(sb);

	KASSERT(m->m_nextpkt == NULL,("sbappendstream 0"));

	kmsan_check_mbuf(m, "sbappend");

#ifdef KERN_TLS
	/*
	 * Decrypted TLS records are appended as records via
	 * sbappendrecord().  TCP passes encrypted TLS records to this
	 * function which must be scheduled for decryption.
	 */
	if (sb->sb_flags & SB_TLS_RX) {
		sbappend_ktls_rx(sb, m);
		return;
	}
#endif

	KASSERT(sb->sb_mb == sb->sb_lastrecord,("sbappendstream 1"));

	SBLASTMBUFCHK(sb);

#ifdef KERN_TLS
	if (sb->sb_tls_info != NULL)
		ktls_seq(sb, m);
#endif

	/* Remove all packet headers and mbuf tags to get a pure data chain. */
	m_demote(m, 1, flags & PRUS_NOTREADY ? M_NOTREADY : 0);

	sbcompress(sb, m, sb->sb_mbtail);

	sb->sb_lastrecord = sb->sb_mb;
	SBLASTRECORDCHK(sb);
}

/*
 * This version of sbappend() should only be used when the caller absolutely
 * knows that there will never be more than one record in the socket buffer,
 * that is, a stream protocol (such as TCP).
 */
void
sbappendstream(struct sockbuf *sb, struct mbuf *m, int flags)
{

	SOCKBUF_LOCK(sb);
	sbappendstream_locked(sb, m, flags);
	SOCKBUF_UNLOCK(sb);
}

#ifdef SOCKBUF_DEBUG
void
sbcheck(struct sockbuf *sb, const char *file, int line)
{
	struct mbuf *m, *n, *fnrdy;
	u_long acc, ccc, mbcnt;
#ifdef KERN_TLS
	u_long tlscc;
#endif

	SOCKBUF_LOCK_ASSERT(sb);

	acc = ccc = mbcnt = 0;
	fnrdy = NULL;

	for (m = sb->sb_mb; m; m = n) {
	    n = m->m_nextpkt;
	    for (; m; m = m->m_next) {
		if (m->m_len == 0) {
			printf("sb %p empty mbuf %p\n", sb, m);
			goto fail;
		}
		if ((m->m_flags & M_NOTREADY) && fnrdy == NULL) {
			if (m != sb->sb_fnrdy) {
				printf("sb %p: fnrdy %p != m %p\n",
				    sb, sb->sb_fnrdy, m);
				goto fail;
			}
			fnrdy = m;
		}
		if (fnrdy) {
			if (!(m->m_flags & M_NOTAVAIL)) {
				printf("sb %p: fnrdy %p, m %p is avail\n",
				    sb, sb->sb_fnrdy, m);
				goto fail;
			}
		} else
			acc += m->m_len;
		ccc += m->m_len;
		mbcnt += MSIZE;
		if (m->m_flags & M_EXT) /*XXX*/ /* pretty sure this is bogus */
			mbcnt += m->m_ext.ext_size;
	    }
	}
#ifdef KERN_TLS
	/*
	 * Account for mbufs "detached" by ktls_detach_record() while
	 * they are decrypted by ktls_decrypt().  tlsdcc gives a count
	 * of the detached bytes that are included in ccc.  The mbufs
	 * and clusters are not included in the socket buffer
	 * accounting.
	 */
	ccc += sb->sb_tlsdcc;

	tlscc = 0;
	for (m = sb->sb_mtls; m; m = m->m_next) {
		if (m->m_nextpkt != NULL) {
			printf("sb %p TLS mbuf %p with nextpkt\n", sb, m);
			goto fail;
		}
		if ((m->m_flags & M_NOTREADY) == 0) {
			printf("sb %p TLS mbuf %p ready\n", sb, m);
			goto fail;
		}
		tlscc += m->m_len;
		ccc += m->m_len;
		mbcnt += MSIZE;
		if (m->m_flags & M_EXT) /*XXX*/ /* pretty sure this is bogus */
			mbcnt += m->m_ext.ext_size;
	}

	if (sb->sb_tlscc != tlscc) {
		printf("tlscc %ld/%u dcc %u\n", tlscc, sb->sb_tlscc,
		    sb->sb_tlsdcc);
		goto fail;
	}
#endif
	if (acc != sb->sb_acc || ccc != sb->sb_ccc || mbcnt != sb->sb_mbcnt) {
		printf("acc %ld/%u ccc %ld/%u mbcnt %ld/%u\n",
		    acc, sb->sb_acc, ccc, sb->sb_ccc, mbcnt, sb->sb_mbcnt);
#ifdef KERN_TLS
		printf("tlscc %ld/%u dcc %u\n", tlscc, sb->sb_tlscc,
		    sb->sb_tlsdcc);
#endif
		goto fail;
	}
	return;
fail:
	panic("%s from %s:%u", __func__, file, line);
}
#endif

/*
 * As above, except the mbuf chain begins a new record.
 */
void
sbappendrecord_locked(struct sockbuf *sb, struct mbuf *m0)
{
	struct mbuf *m;

	SOCKBUF_LOCK_ASSERT(sb);

	if (m0 == NULL)
		return;

	kmsan_check_mbuf(m0, "sbappend");
	m_clrprotoflags(m0);

	/*
	 * Put the first mbuf on the queue.  Note this permits zero length
	 * records.
	 */
	sballoc(sb, m0);
	SBLASTRECORDCHK(sb);
	SBLINKRECORD(sb, m0);
	sb->sb_mbtail = m0;
	m = m0->m_next;
	m0->m_next = 0;
	if (m && (m0->m_flags & M_EOR)) {
		m0->m_flags &= ~M_EOR;
		m->m_flags |= M_EOR;
	}
	/* always call sbcompress() so it can do SBLASTMBUFCHK() */
	sbcompress(sb, m, m0);
}

/*
 * As above, except the mbuf chain begins a new record.
 */
void
sbappendrecord(struct sockbuf *sb, struct mbuf *m0)
{

	SOCKBUF_LOCK(sb);
	sbappendrecord_locked(sb, m0);
	SOCKBUF_UNLOCK(sb);
}

/* Helper routine that appends data, control, and address to a sockbuf. */
static int
sbappendaddr_locked_internal(struct sockbuf *sb, const struct sockaddr *asa,
    struct mbuf *m0, struct mbuf *control, struct mbuf *ctrl_last)
{
	struct mbuf *m, *n, *nlast;

	if (m0 != NULL)
		kmsan_check_mbuf(m0, "sbappend");
	if (control != NULL)
		kmsan_check_mbuf(control, "sbappend");

#if MSIZE <= 256
	if (asa->sa_len > MLEN)
		return (0);
#endif
	m = m_get(M_NOWAIT, MT_SONAME);
	if (m == NULL)
		return (0);
	m->m_len = asa->sa_len;
	bcopy(asa, mtod(m, caddr_t), asa->sa_len);
	if (m0) {
		M_ASSERT_NO_SND_TAG(m0);
		m_clrprotoflags(m0);
		m_tag_delete_chain(m0, NULL);
		/*
		 * Clear some persistent info from pkthdr.
		 * We don't use m_demote(), because some netgraph consumers
		 * expect M_PKTHDR presence.
		 */
		m0->m_pkthdr.rcvif = NULL;
		m0->m_pkthdr.flowid = 0;
		m0->m_pkthdr.csum_flags = 0;
		m0->m_pkthdr.fibnum = 0;
		m0->m_pkthdr.rsstype = 0;
	}
	if (ctrl_last)
		ctrl_last->m_next = m0;	/* concatenate data to control */
	else
		control = m0;
	m->m_next = control;
	for (n = m; n->m_next != NULL; n = n->m_next)
		sballoc(sb, n);
	sballoc(sb, n);
	nlast = n;
	SBLINKRECORD(sb, m);

	sb->sb_mbtail = nlast;
	SBLASTMBUFCHK(sb);

	SBLASTRECORDCHK(sb);
	return (1);
}

/*
 * Append address and data, and optionally, control (ancillary) data to the
 * receive queue of a socket.  If present, m0 must include a packet header
 * with total length.  Returns 0 if no space in sockbuf or insufficient
 * mbufs.
 */
int
sbappendaddr_locked(struct sockbuf *sb, const struct sockaddr *asa,
    struct mbuf *m0, struct mbuf *control)
{
	struct mbuf *ctrl_last;
	int space = asa->sa_len;

	SOCKBUF_LOCK_ASSERT(sb);

	if (m0 && (m0->m_flags & M_PKTHDR) == 0)
		panic("sbappendaddr_locked");
	if (m0)
		space += m0->m_pkthdr.len;
	space += m_length(control, &ctrl_last);

	if (space > sbspace(sb))
		return (0);
	return (sbappendaddr_locked_internal(sb, asa, m0, control, ctrl_last));
}

/*
 * Append address and data, and optionally, control (ancillary) data to the
 * receive queue of a socket.  If present, m0 must include a packet header
 * with total length.  Returns 0 if insufficient mbufs.  Does not validate space
 * on the receiving sockbuf.
 */
int
sbappendaddr_nospacecheck_locked(struct sockbuf *sb, const struct sockaddr *asa,
    struct mbuf *m0, struct mbuf *control)
{
	struct mbuf *ctrl_last;

	SOCKBUF_LOCK_ASSERT(sb);

	ctrl_last = (control == NULL) ? NULL : m_last(control);
	return (sbappendaddr_locked_internal(sb, asa, m0, control, ctrl_last));
}

/*
 * Append address and data, and optionally, control (ancillary) data to the
 * receive queue of a socket.  If present, m0 must include a packet header
 * with total length.  Returns 0 if no space in sockbuf or insufficient
 * mbufs.
 */
int
sbappendaddr(struct sockbuf *sb, const struct sockaddr *asa,
    struct mbuf *m0, struct mbuf *control)
{
	int retval;

	SOCKBUF_LOCK(sb);
	retval = sbappendaddr_locked(sb, asa, m0, control);
	SOCKBUF_UNLOCK(sb);
	return (retval);
}

void
sbappendcontrol_locked(struct sockbuf *sb, struct mbuf *m0,
    struct mbuf *control, int flags)
{
	struct mbuf *m, *mlast;

	if (m0 != NULL)
		kmsan_check_mbuf(m0, "sbappend");
	kmsan_check_mbuf(control, "sbappend");

	sbm_clrprotoflags(m0, flags);
	m_last(control)->m_next = m0;

	SBLASTRECORDCHK(sb);

	for (m = control; m->m_next; m = m->m_next)
		sballoc(sb, m);
	sballoc(sb, m);
	mlast = m;
	SBLINKRECORD(sb, control);

	sb->sb_mbtail = mlast;
	SBLASTMBUFCHK(sb);

	SBLASTRECORDCHK(sb);
}

void
sbappendcontrol(struct sockbuf *sb, struct mbuf *m0, struct mbuf *control,
    int flags)
{

	SOCKBUF_LOCK(sb);
	sbappendcontrol_locked(sb, m0, control, flags);
	SOCKBUF_UNLOCK(sb);
}

/*
 * Append the data in mbuf chain (m) into the socket buffer sb following mbuf
 * (n).  If (n) is NULL, the buffer is presumed empty.
 *
 * When the data is compressed, mbufs in the chain may be handled in one of
 * three ways:
 *
 * (1) The mbuf may simply be dropped, if it contributes nothing (no data, no
 *     record boundary, and no change in data type).
 *
 * (2) The mbuf may be coalesced -- i.e., data in the mbuf may be copied into
 *     an mbuf already in the socket buffer.  This can occur if an
 *     appropriate mbuf exists, there is room, both mbufs are not marked as
 *     not ready, and no merging of data types will occur.
 *
 * (3) The mbuf may be appended to the end of the existing mbuf chain.
 *
 * If any of the new mbufs is marked as M_EOR, mark the last mbuf appended as
 * end-of-record.
 */
void
sbcompress(struct sockbuf *sb, struct mbuf *m, struct mbuf *n)
{
	int eor = 0;
	struct mbuf *o;

	SOCKBUF_LOCK_ASSERT(sb);

	while (m) {
		eor |= m->m_flags & M_EOR;
		if (m->m_len == 0 &&
		    (eor == 0 ||
		     (((o = m->m_next) || (o = n)) &&
		      o->m_type == m->m_type))) {
			if (sb->sb_lastrecord == m)
				sb->sb_lastrecord = m->m_next;
			m = m_free(m);
			continue;
		}
		if (n && (n->m_flags & M_EOR) == 0 &&
		    M_WRITABLE(n) &&
		    ((sb->sb_flags & SB_NOCOALESCE) == 0) &&
		    !(m->m_flags & M_NOTREADY) &&
		    !(n->m_flags & (M_NOTREADY | M_EXTPG)) &&
		    !mbuf_has_tls_session(m) &&
		    !mbuf_has_tls_session(n) &&
		    m->m_len <= MCLBYTES / 4 && /* XXX: Don't copy too much */
		    m->m_len <= M_TRAILINGSPACE(n) &&
		    n->m_type == m->m_type) {
			m_copydata(m, 0, m->m_len, mtodo(n, n->m_len));
			n->m_len += m->m_len;
			sb->sb_ccc += m->m_len;
			if (sb->sb_fnrdy == NULL)
				sb->sb_acc += m->m_len;
			if (m->m_type != MT_DATA && m->m_type != MT_OOBDATA)
				/* XXX: Probably don't need.*/
				sb->sb_ctl += m->m_len;
			m = m_free(m);
			continue;
		}
		if (m->m_len <= MLEN && (m->m_flags & M_EXTPG) &&
		    (m->m_flags & M_NOTREADY) == 0 &&
		    !mbuf_has_tls_session(m))
			(void)mb_unmapped_compress(m);
		if (n)
			n->m_next = m;
		else
			sb->sb_mb = m;
		sb->sb_mbtail = m;
		sballoc(sb, m);
		n = m;
		m->m_flags &= ~M_EOR;
		m = m->m_next;
		n->m_next = 0;
	}
	if (eor) {
		KASSERT(n != NULL, ("sbcompress: eor && n == NULL"));
		n->m_flags |= eor;
	}
	SBLASTMBUFCHK(sb);
}

#ifdef KERN_TLS
/*
 * A version of sbcompress() for encrypted TLS RX mbufs.  These mbufs
 * are appended to the 'sb_mtls' chain instead of 'sb_mb' and are also
 * a bit simpler (no EOR markers, always MT_DATA, etc.).
 */
static void
sbcompress_ktls_rx(struct sockbuf *sb, struct mbuf *m, struct mbuf *n)
{

	SOCKBUF_LOCK_ASSERT(sb);

	while (m) {
		KASSERT((m->m_flags & M_EOR) == 0,
		    ("TLS RX mbuf %p with EOR", m));
		KASSERT(m->m_type == MT_DATA,
		    ("TLS RX mbuf %p is not MT_DATA", m));
		KASSERT((m->m_flags & M_NOTREADY) != 0,
		    ("TLS RX mbuf %p ready", m));
		KASSERT((m->m_flags & M_EXTPG) == 0,
		    ("TLS RX mbuf %p unmapped", m));

		if (m->m_len == 0) {
			m = m_free(m);
			continue;
		}

		/*
		 * Even though both 'n' and 'm' are NOTREADY, it's ok
		 * to coalesce the data.
		 */
		if (n &&
		    M_WRITABLE(n) &&
		    ((sb->sb_flags & SB_NOCOALESCE) == 0) &&
		    !((m->m_flags ^ n->m_flags) & M_DECRYPTED) &&
		    !(n->m_flags & M_EXTPG) &&
		    m->m_len <= MCLBYTES / 4 && /* XXX: Don't copy too much */
		    m->m_len <= M_TRAILINGSPACE(n)) {
			m_copydata(m, 0, m->m_len, mtodo(n, n->m_len));
			n->m_len += m->m_len;
			sb->sb_ccc += m->m_len;
			sb->sb_tlscc += m->m_len;
			m = m_free(m);
			continue;
		}
		if (n)
			n->m_next = m;
		else
			sb->sb_mtls = m;
		sb->sb_mtlstail = m;
		sballoc_ktls_rx(sb, m);
		n = m;
		m = m->m_next;
		n->m_next = NULL;
	}
	SBLASTMBUFCHK(sb);
}
#endif

/*
 * Free all mbufs in a sockbuf.  Check that all resources are reclaimed.
 */
void
sbflush_locked(struct sockbuf *sb)
{

	SOCKBUF_LOCK_ASSERT(sb);

	while (sb->sb_mbcnt || sb->sb_tlsdcc) {
		/*
		 * Don't call sbcut(sb, 0) if the leading mbuf is non-empty:
		 * we would loop forever. Panic instead.
		 */
		if (sb->sb_ccc == 0 && (sb->sb_mb == NULL || sb->sb_mb->m_len))
			break;
		m_freem(sbcut_internal(sb, (int)sb->sb_ccc));
	}
	KASSERT(sb->sb_ccc == 0 && sb->sb_mb == 0 && sb->sb_mbcnt == 0,
	    ("%s: ccc %u mb %p mbcnt %u", __func__,
	    sb->sb_ccc, (void *)sb->sb_mb, sb->sb_mbcnt));
}

void
sbflush(struct sockbuf *sb)
{

	SOCKBUF_LOCK(sb);
	sbflush_locked(sb);
	SOCKBUF_UNLOCK(sb);
}

/*
 * Cut data from (the front of) a sockbuf.
 */
static struct mbuf *
sbcut_internal(struct sockbuf *sb, int len)
{
	struct mbuf *m, *next, *mfree;
	bool is_tls;

	KASSERT(len >= 0, ("%s: len is %d but it is supposed to be >= 0",
	    __func__, len));
	KASSERT(len <= sb->sb_ccc, ("%s: len: %d is > ccc: %u",
	    __func__, len, sb->sb_ccc));

	next = (m = sb->sb_mb) ? m->m_nextpkt : 0;
	is_tls = false;
	mfree = NULL;

	while (len > 0) {
		if (m == NULL) {
#ifdef KERN_TLS
			if (next == NULL && !is_tls) {
				if (sb->sb_tlsdcc != 0) {
					MPASS(len >= sb->sb_tlsdcc);
					len -= sb->sb_tlsdcc;
					sb->sb_ccc -= sb->sb_tlsdcc;
					sb->sb_tlsdcc = 0;
					if (len == 0)
						break;
				}
				next = sb->sb_mtls;
				is_tls = true;
			}
#endif
			KASSERT(next, ("%s: no next, len %d", __func__, len));
			m = next;
			next = m->m_nextpkt;
		}
		if (m->m_len > len) {
			KASSERT(!(m->m_flags & M_NOTAVAIL),
			    ("%s: m %p M_NOTAVAIL", __func__, m));
			m->m_len -= len;
			m->m_data += len;
			sb->sb_ccc -= len;
			sb->sb_acc -= len;
			if (sb->sb_sndptroff != 0)
				sb->sb_sndptroff -= len;
			if (m->m_type != MT_DATA && m->m_type != MT_OOBDATA)
				sb->sb_ctl -= len;
			break;
		}
		len -= m->m_len;
#ifdef KERN_TLS
		if (is_tls)
			sbfree_ktls_rx(sb, m);
		else
#endif
			sbfree(sb, m);
		/*
		 * Do not put M_NOTREADY buffers to the free list, they
		 * are referenced from outside.
		 */
		if (m->m_flags & M_NOTREADY && !is_tls)
			m = m->m_next;
		else {
			struct mbuf *n;

			n = m->m_next;
			m->m_next = mfree;
			mfree = m;
			m = n;
		}
	}
	/*
	 * Free any zero-length mbufs from the buffer.
	 * For SOCK_DGRAM sockets such mbufs represent empty records.
	 * XXX: For SOCK_STREAM sockets such mbufs can appear in the buffer,
	 * when sosend_generic() needs to send only control data.
	 */
	while (m && m->m_len == 0) {
		struct mbuf *n;

		sbfree(sb, m);
		n = m->m_next;
		m->m_next = mfree;
		mfree = m;
		m = n;
	}
#ifdef KERN_TLS
	if (is_tls) {
		sb->sb_mb = NULL;
		sb->sb_mtls = m;
		if (m == NULL)
			sb->sb_mtlstail = NULL;
	} else
#endif
	if (m) {
		sb->sb_mb = m;
		m->m_nextpkt = next;
	} else
		sb->sb_mb = next;
	/*
	 * First part is an inline SB_EMPTY_FIXUP().  Second part makes sure
	 * sb_lastrecord is up-to-date if we dropped part of the last record.
	 */
	m = sb->sb_mb;
	if (m == NULL) {
		sb->sb_mbtail = NULL;
		sb->sb_lastrecord = NULL;
	} else if (m->m_nextpkt == NULL) {
		sb->sb_lastrecord = m;
	}

	return (mfree);
}

/*
 * Drop data from (the front of) a sockbuf.
 */
void
sbdrop_locked(struct sockbuf *sb, int len)
{

	SOCKBUF_LOCK_ASSERT(sb);
	m_freem(sbcut_internal(sb, len));
}

/*
 * Drop data from (the front of) a sockbuf,
 * and return it to caller.
 */
struct mbuf *
sbcut_locked(struct sockbuf *sb, int len)
{

	SOCKBUF_LOCK_ASSERT(sb);
	return (sbcut_internal(sb, len));
}

void
sbdrop(struct sockbuf *sb, int len)
{
	struct mbuf *mfree;

	SOCKBUF_LOCK(sb);
	mfree = sbcut_internal(sb, len);
	SOCKBUF_UNLOCK(sb);

	m_freem(mfree);
}

struct mbuf *
sbsndptr_noadv(struct sockbuf *sb, uint32_t off, uint32_t *moff)
{
	struct mbuf *m;

	KASSERT(sb->sb_mb != NULL, ("%s: sb_mb is NULL", __func__));
	if (sb->sb_sndptr == NULL || sb->sb_sndptroff > off) {
		*moff = off;
		if (sb->sb_sndptr == NULL) {
			sb->sb_sndptr = sb->sb_mb;
			sb->sb_sndptroff = 0;
		}
		return (sb->sb_mb);
	} else {
		m = sb->sb_sndptr;
		off -= sb->sb_sndptroff;
	}
	*moff = off;
	return (m);
}

void
sbsndptr_adv(struct sockbuf *sb, struct mbuf *mb, uint32_t len)
{
	/*
	 * A small copy was done, advance forward the sb_sbsndptr to cover
	 * it.
	 */
	struct mbuf *m;

	if (mb != sb->sb_sndptr) {
		/* Did not copyout at the same mbuf */
		return;
	}
	m = mb;
	while (m && (len > 0)) {
		if (len >= m->m_len) {
			len -= m->m_len;
			if (m->m_next) {
				sb->sb_sndptroff += m->m_len;
				sb->sb_sndptr = m->m_next;
			}
			m = m->m_next;
		} else {
			len = 0;
		}
	}
}

/*
 * Return the first mbuf and the mbuf data offset for the provided
 * send offset without changing the "sb_sndptroff" field.
 */
struct mbuf *
sbsndmbuf(struct sockbuf *sb, u_int off, u_int *moff)
{
	struct mbuf *m;

	KASSERT(sb->sb_mb != NULL, ("%s: sb_mb is NULL", __func__));

	/*
	 * If the "off" is below the stored offset, which happens on
	 * retransmits, just use "sb_mb":
	 */
	if (sb->sb_sndptr == NULL || sb->sb_sndptroff > off) {
		m = sb->sb_mb;
	} else {
		m = sb->sb_sndptr;
		off -= sb->sb_sndptroff;
	}
	while (off > 0 && m != NULL) {
		if (off < m->m_len)
			break;
		off -= m->m_len;
		m = m->m_next;
	}
	*moff = off;
	return (m);
}

/*
 * Drop a record off the front of a sockbuf and move the next record to the
 * front.
 */
void
sbdroprecord_locked(struct sockbuf *sb)
{
	struct mbuf *m;

	SOCKBUF_LOCK_ASSERT(sb);

	m = sb->sb_mb;
	if (m) {
		sb->sb_mb = m->m_nextpkt;
		do {
			sbfree(sb, m);
			m = m_free(m);
		} while (m);
	}
	SB_EMPTY_FIXUP(sb);
}

/*
 * Drop a record off the front of a sockbuf and move the next record to the
 * front.
 */
void
sbdroprecord(struct sockbuf *sb)
{

	SOCKBUF_LOCK(sb);
	sbdroprecord_locked(sb);
	SOCKBUF_UNLOCK(sb);
}

/*
 * Create a "control" mbuf containing the specified data with the specified
 * type for presentation on a socket buffer.
 */
struct mbuf *
sbcreatecontrol(const void *p, u_int size, int type, int level, int wait)
{
	struct cmsghdr *cp;
	struct mbuf *m;

	MBUF_CHECKSLEEP(wait);

	if (wait == M_NOWAIT) {
		if (CMSG_SPACE(size) > MCLBYTES)
			return (NULL);
	} else
		KASSERT(CMSG_SPACE(size) <= MCLBYTES,
		    ("%s: passed CMSG_SPACE(%u) > MCLBYTES", __func__, size));

	if (CMSG_SPACE(size) > MLEN)
		m = m_getcl(wait, MT_CONTROL, 0);
	else
		m = m_get(wait, MT_CONTROL);
	if (m == NULL)
		return (NULL);

	KASSERT(CMSG_SPACE(size) <= M_TRAILINGSPACE(m),
	    ("sbcreatecontrol: short mbuf"));
	/*
	 * Don't leave the padding between the msg header and the
	 * cmsg data and the padding after the cmsg data un-initialized.
	 */
	cp = mtod(m, struct cmsghdr *);
	bzero(cp, CMSG_SPACE(size));
	if (p != NULL)
		(void)memcpy(CMSG_DATA(cp), p, size);
	m->m_len = CMSG_SPACE(size);
	cp->cmsg_len = CMSG_LEN(size);
	cp->cmsg_level = level;
	cp->cmsg_type = type;
	return (m);
}

/*
 * This does the same for socket buffers that sotoxsocket does for sockets:
 * generate an user-format data structure describing the socket buffer.  Note
 * that the xsockbuf structure, since it is always embedded in a socket, does
 * not include a self pointer nor a length.  We make this entry point public
 * in case some other mechanism needs it.
 */
void
sbtoxsockbuf(struct sockbuf *sb, struct xsockbuf *xsb)
{

	xsb->sb_cc = sb->sb_ccc;
	xsb->sb_hiwat = sb->sb_hiwat;
	xsb->sb_mbcnt = sb->sb_mbcnt;
	xsb->sb_mbmax = sb->sb_mbmax;
	xsb->sb_lowat = sb->sb_lowat;
	xsb->sb_flags = sb->sb_flags;
	xsb->sb_timeo = sb->sb_timeo;
}

/* This takes the place of kern.maxsockbuf, which moved to kern.ipc. */
static int dummy;
SYSCTL_INT(_kern, KERN_DUMMY, dummy, CTLFLAG_RW | CTLFLAG_SKIP, &dummy, 0, "");
SYSCTL_OID(_kern_ipc, KIPC_MAXSOCKBUF, maxsockbuf,
    CTLTYPE_ULONG | CTLFLAG_RW | CTLFLAG_MPSAFE, &sb_max, 0,
    sysctl_handle_sb_max, "LU",
    "Maximum socket buffer size");
SYSCTL_ULONG(_kern_ipc, KIPC_SOCKBUF_WASTE, sockbuf_waste_factor, CTLFLAG_RW,
    &sb_efficiency, 0, "Socket buffer size waste factor");
