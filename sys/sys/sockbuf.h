/*-
 * Copyright (c) 1982, 1986, 1990, 1993
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
 *	@(#)socketvar.h	8.3 (Berkeley) 2/19/95
 *
 * $FreeBSD$
 */
#ifndef _SYS_SOCKBUF_H_
#define _SYS_SOCKBUF_H_
#include <sys/selinfo.h>		/* for struct selinfo */
#include <sys/_lock.h>
#include <sys/_mutex.h>
#include <sys/_sx.h>

#define	SB_MAX		(2*1024*1024)	/* default for max chars in sockbuf */

/*
 * Constants for sb_flags field of struct sockbuf.
 */
#define	SB_WAIT		0x04		/* someone is waiting for data/space */
#define	SB_SEL		0x08		/* someone is selecting */
#define	SB_ASYNC	0x10		/* ASYNC I/O, need signals */
#define	SB_UPCALL	0x20		/* someone wants an upcall */
#define	SB_NOINTR	0x40		/* operations not interruptible */
#define	SB_AIO		0x80		/* AIO operations queued */
#define	SB_KNOTE	0x100		/* kernel note attached */
#define	SB_NOCOALESCE	0x200		/* don't coalesce new data into existing mbufs */
#define	SB_IN_TOE	0x400		/* socket buffer is in the middle of an operation */
#define	SB_AUTOSIZE	0x800		/* automatically size socket buffer */

#define	SBS_CANTSENDMORE	0x0010	/* can't send more data to peer */
#define	SBS_CANTRCVMORE		0x0020	/* can't receive more data from peer */
#define	SBS_RCVATMARK		0x0040	/* at mark on input */

struct mbuf;
struct sockaddr;
struct socket;
struct thread;

struct	xsockbuf {
	u_int	sb_cc;
	u_int	sb_hiwat;
	u_int	sb_mbcnt;
	u_int   sb_mcnt;
	u_int   sb_ccnt;
	u_int	sb_mbmax;
	int	sb_lowat;
	int	sb_timeo;
	short	sb_flags;
};

/*
 * Variables for socket buffering.
 */
struct	sockbuf {
	struct	selinfo sb_sel;	/* process selecting read/write */
	struct	mtx sb_mtx;	/* sockbuf lock */
	struct	sx sb_sx;	/* prevent I/O interlacing */
	short	sb_state;	/* (c/d) socket state on sockbuf */
#define	sb_startzero	sb_mb
	struct	mbuf *sb_mb;	/* (c/d) the mbuf chain */
	struct	mbuf *sb_mbtail; /* (c/d) the last mbuf in the chain */
	struct	mbuf *sb_lastrecord;	/* (c/d) first mbuf of last
					 * record in socket buffer */
	struct	mbuf *sb_sndptr; /* (c/d) pointer into mbuf chain */
	u_int	sb_sndptroff;	/* (c/d) byte offset of ptr into chain */
	u_int	sb_cc;		/* (c/d) actual chars in buffer */
	u_int	sb_hiwat;	/* (c/d) max actual char count */
	u_int	sb_mbcnt;	/* (c/d) chars of mbufs used */
	u_int   sb_mcnt;        /* (c/d) number of mbufs in buffer */
	u_int   sb_ccnt;        /* (c/d) number of clusters in buffer */
	u_int	sb_mbmax;	/* (c/d) max chars of mbufs to use */
	u_int	sb_ctl;		/* (c/d) non-data chars in buffer */
	int	sb_lowat;	/* (c/d) low water mark */
	int	sb_timeo;	/* (c/d) timeout for read/write */
	short	sb_flags;	/* (c/d) flags, see below */
	int	(*sb_upcall)(struct socket *, void *, int); /* (c/d) */
	void	*sb_upcallarg;	/* (c/d) */
};

#ifdef _KERNEL

/*
 * Per-socket buffer mutex used to protect most fields in the socket
 * buffer.
 */
#define	SOCKBUF_MTX(_sb)		(&(_sb)->sb_mtx)
#define	SOCKBUF_LOCK_INIT(_sb, _name) \
	mtx_init(SOCKBUF_MTX(_sb), _name, NULL, MTX_DEF)
#define	SOCKBUF_LOCK_DESTROY(_sb)	mtx_destroy(SOCKBUF_MTX(_sb))
#define	SOCKBUF_LOCK(_sb)		mtx_lock(SOCKBUF_MTX(_sb))
#define	SOCKBUF_OWNED(_sb)		mtx_owned(SOCKBUF_MTX(_sb))
#define	SOCKBUF_UNLOCK(_sb)		mtx_unlock(SOCKBUF_MTX(_sb))
#define	SOCKBUF_LOCK_ASSERT(_sb)	mtx_assert(SOCKBUF_MTX(_sb), MA_OWNED)
#define	SOCKBUF_UNLOCK_ASSERT(_sb)	mtx_assert(SOCKBUF_MTX(_sb), MA_NOTOWNED)

void	sbappend(struct sockbuf *sb, struct mbuf *m);
void	sbappend_locked(struct sockbuf *sb, struct mbuf *m);
void	sbappendstream(struct sockbuf *sb, struct mbuf *m);
void	sbappendstream_locked(struct sockbuf *sb, struct mbuf *m);
int	sbappendaddr(struct sockbuf *sb, const struct sockaddr *asa,
	    struct mbuf *m0, struct mbuf *control);
int	sbappendaddr_locked(struct sockbuf *sb, const struct sockaddr *asa,
	    struct mbuf *m0, struct mbuf *control);
int	sbappendcontrol(struct sockbuf *sb, struct mbuf *m0,
	    struct mbuf *control);
int	sbappendcontrol_locked(struct sockbuf *sb, struct mbuf *m0,
	    struct mbuf *control);
void	sbappendrecord(struct sockbuf *sb, struct mbuf *m0);
void	sbappendrecord_locked(struct sockbuf *sb, struct mbuf *m0);
void	sbcheck(struct sockbuf *sb);
void	sbcompress(struct sockbuf *sb, struct mbuf *m, struct mbuf *n);
struct mbuf *
	sbcreatecontrol(caddr_t p, int size, int type, int level);
void	sbdestroy(struct sockbuf *sb, struct socket *so);
void	sbdrop(struct sockbuf *sb, int len);
void	sbdrop_locked(struct sockbuf *sb, int len);
void	sbdroprecord(struct sockbuf *sb);
void	sbdroprecord_locked(struct sockbuf *sb);
void	sbflush(struct sockbuf *sb);
void	sbflush_locked(struct sockbuf *sb);
void	sbrelease(struct sockbuf *sb, struct socket *so);
void	sbrelease_internal(struct sockbuf *sb, struct socket *so);
void	sbrelease_locked(struct sockbuf *sb, struct socket *so);
int	sbreserve(struct sockbuf *sb, u_long cc, struct socket *so,
	    struct thread *td);
int	sbreserve_locked(struct sockbuf *sb, u_long cc, struct socket *so,
	    struct thread *td);
struct mbuf *
	sbsndptr(struct sockbuf *sb, u_int off, u_int len, u_int *moff);
void	sbtoxsockbuf(struct sockbuf *sb, struct xsockbuf *xsb);
int	sbwait(struct sockbuf *sb);
int	sblock(struct sockbuf *sb, int flags);
void	sbunlock(struct sockbuf *sb);

/*
 * How much space is there in a socket buffer (so->so_snd or so->so_rcv)?
 * This is problematical if the fields are unsigned, as the space might
 * still be negative (cc > hiwat or mbcnt > mbmax).  Should detect
 * overflow and return 0.  Should use "lmin" but it doesn't exist now.
 */
#define	sbspace(sb) \
    ((long) imin((int)((sb)->sb_hiwat - (sb)->sb_cc), \
	 (int)((sb)->sb_mbmax - (sb)->sb_mbcnt)))

/* adjust counters in sb reflecting allocation of m */
#define	sballoc(sb, m) { \
	(sb)->sb_cc += (m)->m_len; \
	if ((m)->m_type != MT_DATA && (m)->m_type != MT_OOBDATA) \
		(sb)->sb_ctl += (m)->m_len; \
	(sb)->sb_mbcnt += MSIZE; \
	(sb)->sb_mcnt += 1; \
	if ((m)->m_flags & M_EXT) { \
		(sb)->sb_mbcnt += (m)->m_ext.ext_size; \
		(sb)->sb_ccnt += 1; \
	} \
}

/* adjust counters in sb reflecting freeing of m */
#define	sbfree(sb, m) { \
	(sb)->sb_cc -= (m)->m_len; \
	if ((m)->m_type != MT_DATA && (m)->m_type != MT_OOBDATA) \
		(sb)->sb_ctl -= (m)->m_len; \
	(sb)->sb_mbcnt -= MSIZE; \
	(sb)->sb_mcnt -= 1; \
	if ((m)->m_flags & M_EXT) { \
		(sb)->sb_mbcnt -= (m)->m_ext.ext_size; \
		(sb)->sb_ccnt -= 1; \
	} \
	if ((sb)->sb_sndptr == (m)) { \
		(sb)->sb_sndptr = NULL; \
		(sb)->sb_sndptroff = 0; \
	} \
	if ((sb)->sb_sndptroff != 0) \
		(sb)->sb_sndptroff -= (m)->m_len; \
}

#define SB_EMPTY_FIXUP(sb) do {						\
	if ((sb)->sb_mb == NULL) {					\
		(sb)->sb_mbtail = NULL;					\
		(sb)->sb_lastrecord = NULL;				\
	}								\
} while (/*CONSTCOND*/0)

#ifdef SOCKBUF_DEBUG
void	sblastrecordchk(struct sockbuf *, const char *, int);
#define	SBLASTRECORDCHK(sb)	sblastrecordchk((sb), __FILE__, __LINE__)

void	sblastmbufchk(struct sockbuf *, const char *, int);
#define	SBLASTMBUFCHK(sb)	sblastmbufchk((sb), __FILE__, __LINE__)
#else
#define	SBLASTRECORDCHK(sb)      /* nothing */
#define	SBLASTMBUFCHK(sb)        /* nothing */
#endif /* SOCKBUF_DEBUG */

#endif /* _KERNEL */

#endif /* _SYS_SOCKBUF_H_ */
