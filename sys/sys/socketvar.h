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
 * $FreeBSD$
 */

#ifndef _SYS_SOCKETVAR_H_
#define _SYS_SOCKETVAR_H_

#include <sys/queue.h>			/* for TAILQ macros */
#include <sys/selinfo.h>		/* for struct selinfo */
#include <sys/_lock.h>
#include <sys/_mutex.h>

/*
 * Kernel structure per socket.
 * Contains send and receive buffer queues,
 * handle on protocol and pointer to protocol
 * private data and error information.
 */
typedef	u_quad_t so_gen_t;

/*-
 * Locking key to struct socket:
 * (a) constant after allocation, no locking required.
 * (b) locked by SOCK_LOCK(so).
 * (c) locked by SOCKBUF_LOCK(&so->so_rcv).
 * (d) locked by SOCKBUF_LOCK(&so->so_snd).
 * (e) locked by ACCEPT_LOCK().
 * (f) not locked since integer reads/writes are atomic.
 * (g) used only as a sleep/wakeup address, no value.
 * (h) locked by global mutex so_global_mtx.
 */
struct socket {
	int	so_count;		/* (b) reference count */
	short	so_type;		/* (a) generic type, see socket.h */
	short	so_options;		/* from socket call, see socket.h */
	short	so_linger;		/* time to linger while closing */
	short	so_state;		/* (b) internal state flags SS_* */
	int	so_qstate;		/* (e) internal state flags SQ_* */
	void	*so_pcb;		/* protocol control block */
	struct	protosw *so_proto;	/* (a) protocol handle */
/*
 * Variables for connection queuing.
 * Socket where accepts occur is so_head in all subsidiary sockets.
 * If so_head is 0, socket is not related to an accept.
 * For head socket so_incomp queues partially completed connections,
 * while so_comp is a queue of connections ready to be accepted.
 * If a connection is aborted and it has so_head set, then
 * it has to be pulled out of either so_incomp or so_comp.
 * We allow connections to queue up based on current queue lengths
 * and limit on number of queued connections for this socket.
 */
	struct	socket *so_head;	/* (e) back pointer to accept socket */
	TAILQ_HEAD(, socket) so_incomp;	/* (e) queue of partial unaccepted connections */
	TAILQ_HEAD(, socket) so_comp;	/* (e) queue of complete unaccepted connections */
	TAILQ_ENTRY(socket) so_list;	/* (e) list of unaccepted connections */
	u_short	so_qlen;		/* (e) number of unaccepted connections */
	u_short	so_incqlen;		/* (e) number of unaccepted incomplete
					   connections */
	u_short	so_qlimit;		/* (e) max number queued connections */
	short	so_timeo;		/* (g) connection timeout */
	u_short	so_error;		/* (f) error affecting connection */
	struct	sigio *so_sigio;	/* [sg] information for async I/O or
					   out of band data (SIGURG) */
	u_long	so_oobmark;		/* (c) chars to oob mark */
	TAILQ_HEAD(, aiocblist) so_aiojobq; /* AIO ops waiting on socket */
/*
 * Variables for socket buffering.
 */
	struct sockbuf {
		struct	selinfo sb_sel;	/* process selecting read/write */
		struct	mtx sb_mtx;	/* sockbuf lock */
#define	sb_startzero	sb_mb
		struct	mbuf *sb_mb;	/* (c/d) the mbuf chain */
		struct	mbuf *sb_mbtail; /* (c/d) the last mbuf in the chain */
		struct	mbuf *sb_lastrecord;	/* (c/d) first mbuf of last
						 * record in socket buffer */
		u_int	sb_cc;		/* (c/d) actual chars in buffer */
		u_int	sb_hiwat;	/* (c/d) max actual char count */
		u_int	sb_mbcnt;	/* (c/d) chars of mbufs used */
		u_int	sb_mbmax;	/* (c/d) max chars of mbufs to use */
		u_int	sb_ctl;		/* (c/d) non-data chars in buffer */
		int	sb_lowat;	/* (c/d) low water mark */
		int	sb_timeo;	/* (c/d) timeout for read/write */
		short	sb_flags;	/* (c/d) flags, see below */
		short	sb_state;	/* (c/d) socket state on sockbuf */
	} so_rcv, so_snd;
/*
 * Constants for sb_flags field of struct sockbuf.
 */
#define	SB_MAX		(256*1024)	/* default for max chars in sockbuf */
/*
 * Constants for sb_flags field of struct sockbuf.
 */
#define	SB_LOCK		0x01		/* lock on data queue */
#define	SB_WANT		0x02		/* someone is waiting to lock */
#define	SB_WAIT		0x04		/* someone is waiting for data/space */
#define	SB_SEL		0x08		/* someone is selecting */
#define	SB_ASYNC	0x10		/* ASYNC I/O, need signals */
#define	SB_UPCALL	0x20		/* someone wants an upcall */
#define	SB_NOINTR	0x40		/* operations not interruptible */
#define SB_AIO		0x80		/* AIO operations queued */
#define SB_KNOTE	0x100		/* kernel note attached */

	void	(*so_upcall)(struct socket *, void *, int);
	void	*so_upcallarg;
	struct	ucred *so_cred;		/* (a) user credentials */
	struct	label *so_label;	/* (b) MAC label for socket */
	struct	label *so_peerlabel;	/* (b) cached MAC label for peer */
	/* NB: generation count must not be first; easiest to make it last. */
	so_gen_t so_gencnt;		/* (h) generation count */
	void	*so_emuldata;		/* private data for emulators */
 	struct so_accf {
		struct	accept_filter *so_accept_filter;
		void	*so_accept_filter_arg;	/* saved filter args */
		char	*so_accept_filter_str;	/* saved user args */
	} *so_accf;
};

#define SB_EMPTY_FIXUP(sb) do {						\
	if ((sb)->sb_mb == NULL) {					\
		(sb)->sb_mbtail = NULL;					\
		(sb)->sb_lastrecord = NULL;				\
	}								\
} while (/*CONSTCOND*/0)

/*
 * Global accept mutex to serialize access to accept queues and
 * fields associated with multiple sockets.  This allows us to
 * avoid defining a lock order between listen and accept sockets
 * until such time as it proves to be a good idea.
 */
extern struct mtx accept_mtx;
#define	ACCEPT_LOCK_ASSERT()		mtx_assert(&accept_mtx, MA_OWNED)
#define	ACCEPT_UNLOCK_ASSERT()		mtx_assert(&accept_mtx, MA_NOTOWNED)
#define	ACCEPT_LOCK()			mtx_lock(&accept_mtx)
#define	ACCEPT_UNLOCK()			mtx_unlock(&accept_mtx)

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

/*
 * Per-socket mutex: we reuse the receive socket buffer mutex for space
 * efficiency.  This decision should probably be revisited as we optimize
 * locking for the socket code.
 */
#define	SOCK_MTX(_so)			SOCKBUF_MTX(&(_so)->so_rcv)
#define	SOCK_LOCK(_so)			SOCKBUF_LOCK(&(_so)->so_rcv)
#define	SOCK_OWNED(_so)			SOCKBUF_OWNED(&(_so)->so_rcv)
#define	SOCK_UNLOCK(_so)		SOCKBUF_UNLOCK(&(_so)->so_rcv)
#define	SOCK_LOCK_ASSERT(_so)		SOCKBUF_LOCK_ASSERT(&(_so)->so_rcv)

/*
 * Socket state bits.
 *
 * Historically, this bits were all kept in the so_state field.  For
 * locking reasons, they are now in multiple fields, as they are
 * locked differently.  so_state maintains basic socket state protected
 * by the socket lock.  so_qstate holds information about the socket
 * accept queues.  Each socket buffer also has a state field holding
 * information relevant to that socket buffer (can't send, rcv).  Many
 * fields will be read without locks to improve performance and avoid
 * lock order issues.  However, this approach must be used with caution.
 */
#define	SS_NOFDREF		0x0001	/* no file table ref any more */
#define	SS_ISCONNECTED		0x0002	/* socket connected to a peer */
#define	SS_ISCONNECTING		0x0004	/* in process of connecting to peer */
#define	SS_ISDISCONNECTING	0x0008	/* in process of disconnecting */
#define	SS_NBIO			0x0100	/* non-blocking ops */
#define	SS_ASYNC		0x0200	/* async i/o notify */
#define	SS_ISCONFIRMING		0x0400	/* deciding to accept connection req */
#define	SS_ISDISCONNECTED	0x2000	/* socket disconnected from peer */

/*
 * Socket state bits now stored in the socket buffer state field.
 */
#define	SBS_CANTSENDMORE	0x0010	/* can't send more data to peer */
#define	SBS_CANTRCVMORE		0x0020	/* can't receive more data from peer */
#define	SBS_RCVATMARK		0x0040	/* at mark on input */

/*
 * Socket state bits stored in so_qstate.
 */
#define	SQ_INCOMP		0x0800	/* unaccepted, incomplete connection */
#define	SQ_COMP			0x1000	/* unaccepted, complete connection */

/*
 * Externalized form of struct socket used by the sysctl(3) interface.
 */
struct xsocket {
	size_t	xso_len;	/* length of this structure */
	struct	socket *xso_so;	/* makes a convenient handle sometimes */
	short	so_type;
	short	so_options;
	short	so_linger;
	short	so_state;
	caddr_t	so_pcb;		/* another convenient handle */
	int	xso_protocol;
	int	xso_family;
	u_short	so_qlen;
	u_short	so_incqlen;
	u_short	so_qlimit;
	short	so_timeo;
	u_short	so_error;
	pid_t	so_pgid;
	u_long	so_oobmark;
	struct xsockbuf {
		u_int	sb_cc;
		u_int	sb_hiwat;
		u_int	sb_mbcnt;
		u_int	sb_mbmax;
		int	sb_lowat;
		int	sb_timeo;
		short	sb_flags;
	} so_rcv, so_snd;
	uid_t	so_uid;		/* XXX */
};

#ifdef _KERNEL

/*
 * Macros for sockets and socket buffering.
 */

/*
 * Do we need to notify the other side when I/O is possible?
 */
#define	sb_notify(sb)	(((sb)->sb_flags & (SB_WAIT | SB_SEL | SB_ASYNC | \
    SB_UPCALL | SB_AIO | SB_KNOTE)) != 0)

/*
 * How much space is there in a socket buffer (so->so_snd or so->so_rcv)?
 * This is problematical if the fields are unsigned, as the space might
 * still be negative (cc > hiwat or mbcnt > mbmax).  Should detect
 * overflow and return 0.  Should use "lmin" but it doesn't exist now.
 */
#define	sbspace(sb) \
    ((long) imin((int)((sb)->sb_hiwat - (sb)->sb_cc), \
	 (int)((sb)->sb_mbmax - (sb)->sb_mbcnt)))

/* do we have to send all at once on a socket? */
#define	sosendallatonce(so) \
    ((so)->so_proto->pr_flags & PR_ATOMIC)

/* can we read something from so? */
#define	soreadable(so) \
    ((so)->so_rcv.sb_cc >= (so)->so_rcv.sb_lowat || \
	((so)->so_rcv.sb_state & SBS_CANTRCVMORE) || \
	!TAILQ_EMPTY(&(so)->so_comp) || (so)->so_error)

/* can we write something to so? */
#define	sowriteable(so) \
    ((sbspace(&(so)->so_snd) >= (so)->so_snd.sb_lowat && \
	(((so)->so_state&SS_ISCONNECTED) || \
	  ((so)->so_proto->pr_flags&PR_CONNREQUIRED)==0)) || \
     ((so)->so_snd.sb_state & SBS_CANTSENDMORE) || \
     (so)->so_error)

/* adjust counters in sb reflecting allocation of m */
#define	sballoc(sb, m) { \
	(sb)->sb_cc += (m)->m_len; \
	if ((m)->m_type != MT_DATA && (m)->m_type != MT_HEADER && \
	    (m)->m_type != MT_OOBDATA) \
		(sb)->sb_ctl += (m)->m_len; \
	(sb)->sb_mbcnt += MSIZE; \
	if ((m)->m_flags & M_EXT) \
		(sb)->sb_mbcnt += (m)->m_ext.ext_size; \
}

/* adjust counters in sb reflecting freeing of m */
#define	sbfree(sb, m) { \
	(sb)->sb_cc -= (m)->m_len; \
	if ((m)->m_type != MT_DATA && (m)->m_type != MT_HEADER && \
	    (m)->m_type != MT_OOBDATA) \
		(sb)->sb_ctl -= (m)->m_len; \
	(sb)->sb_mbcnt -= MSIZE; \
	if ((m)->m_flags & M_EXT) \
		(sb)->sb_mbcnt -= (m)->m_ext.ext_size; \
}

/*
 * Set lock on sockbuf sb; sleep if lock is already held.
 * Unless SB_NOINTR is set on sockbuf, sleep is interruptible.
 * Returns error without lock if sleep is interrupted.
 */
#define sblock(sb, wf) ((sb)->sb_flags & SB_LOCK ? \
		(((wf) == M_WAITOK) ? sb_lock(sb) : EWOULDBLOCK) : \
		((sb)->sb_flags |= SB_LOCK), 0)

/* release lock on sockbuf sb */
#define	sbunlock(sb) do { \
	SOCKBUF_LOCK_ASSERT(sb); \
	(sb)->sb_flags &= ~SB_LOCK; \
	if ((sb)->sb_flags & SB_WANT) { \
		(sb)->sb_flags &= ~SB_WANT; \
		wakeup(&(sb)->sb_flags); \
	} \
} while (0)

/*
 * soref()/sorele() ref-count the socket structure.  Note that you must
 * still explicitly close the socket, but the last ref count will free
 * the structure.
 */
#define	soref(so) do {							\
	SOCK_LOCK_ASSERT(so);						\
	++(so)->so_count;						\
} while (0)

#define	sorele(so) do {							\
	ACCEPT_LOCK_ASSERT();						\
	SOCK_LOCK_ASSERT(so);						\
	if ((so)->so_count <= 0)					\
		panic("sorele");					\
	if (--(so)->so_count == 0)					\
		sofree(so);						\
	else {								\
		SOCK_UNLOCK(so);					\
		ACCEPT_UNLOCK();					\
	}								\
} while (0)

#define	sotryfree(so) do {						\
	ACCEPT_LOCK_ASSERT();						\
	SOCK_LOCK_ASSERT(so);						\
	if ((so)->so_count == 0)					\
		sofree(so);						\
	else {								\
		SOCK_UNLOCK(so);					\
		ACCEPT_UNLOCK();					\
	}								\
} while(0)

/*
 * In sorwakeup() and sowwakeup(), acquire the socket buffer lock to
 * avoid a non-atomic test-and-wakeup.  However, sowakeup is
 * responsible for releasing the lock if it is called.  We unlock only
 * if we don't call into sowakeup.  If any code is introduced that
 * directly invokes the underlying sowakeup() primitives, it must
 * maintain the same semantics.
 */
#define	sorwakeup_locked(so) do {					\
	SOCKBUF_LOCK_ASSERT(&(so)->so_rcv);				\
	if (sb_notify(&(so)->so_rcv))					\
		sowakeup((so), &(so)->so_rcv);	 			\
	else								\
		SOCKBUF_UNLOCK(&(so)->so_rcv);				\
} while (0)

#define	sorwakeup(so) do {						\
	SOCKBUF_LOCK(&(so)->so_rcv);					\
	sorwakeup_locked(so);						\
} while (0)

#define	sowwakeup_locked(so) do {					\
	SOCKBUF_LOCK_ASSERT(&(so)->so_snd);				\
	if (sb_notify(&(so)->so_snd))					\
		sowakeup((so), &(so)->so_snd); 				\
	else								\
		SOCKBUF_UNLOCK(&(so)->so_snd);				\
} while (0)

#define	sowwakeup(so) do {						\
	SOCKBUF_LOCK(&(so)->so_snd);					\
	sowwakeup_locked(so);						\
} while (0)

/*
 * Argument structure for sosetopt et seq.  This is in the KERNEL
 * section because it will never be visible to user code.
 */
enum sopt_dir { SOPT_GET, SOPT_SET };
struct sockopt {
	enum	sopt_dir sopt_dir; /* is this a get or a set? */
	int	sopt_level;	/* second arg of [gs]etsockopt */
	int	sopt_name;	/* third arg of [gs]etsockopt */
	void   *sopt_val;	/* fourth arg of [gs]etsockopt */
	size_t	sopt_valsize;	/* (almost) fifth arg of [gs]etsockopt */
	struct	thread *sopt_td; /* calling thread or null if kernel */
};

struct accept_filter {
	char	accf_name[16];
	void	(*accf_callback)
		(struct socket *so, void *arg, int waitflag);
	void *	(*accf_create)
		(struct socket *so, char *arg);
	void	(*accf_destroy)
		(struct socket *so);
	SLIST_ENTRY(accept_filter) accf_next;
};

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_ACCF);
MALLOC_DECLARE(M_PCB);
MALLOC_DECLARE(M_SONAME);
#endif

extern int	maxsockets;
extern u_long	sb_max;
extern struct uma_zone *socket_zone;
extern so_gen_t so_gencnt;

struct mbuf;
struct sockaddr;
struct ucred;
struct uio;

/*
 * From uipc_socket and friends
 */
int	so_setsockopt(struct socket *so, int level, int optname,
	    void *optval, size_t optlen);
int	sockargs(struct mbuf **mp, caddr_t buf, int buflen, int type);
int	getsockaddr(struct sockaddr **namp, caddr_t uaddr, size_t len);
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
void	sbdrop(struct sockbuf *sb, int len);
void	sbdrop_locked(struct sockbuf *sb, int len);
void	sbdroprecord(struct sockbuf *sb);
void	sbdroprecord_locked(struct sockbuf *sb);
void	sbflush(struct sockbuf *sb);
void	sbflush_locked(struct sockbuf *sb);
void	sbinsertoob(struct sockbuf *sb, struct mbuf *m0);
void	sbinsertoob_locked(struct sockbuf *sb, struct mbuf *m0);
void	sbrelease(struct sockbuf *sb, struct socket *so);
void	sbrelease_locked(struct sockbuf *sb, struct socket *so);
int	sbreserve(struct sockbuf *sb, u_long cc, struct socket *so,
	    struct thread *td);
int	sbreserve_locked(struct sockbuf *sb, u_long cc, struct socket *so,
	    struct thread *td);
void	sbtoxsockbuf(struct sockbuf *sb, struct xsockbuf *xsb);
int	sbwait(struct sockbuf *sb);
int	sb_lock(struct sockbuf *sb);
int	soabort(struct socket *so);
int	soaccept(struct socket *so, struct sockaddr **nam);
struct	socket *soalloc(int mflags);
int	socheckuid(struct socket *so, uid_t uid);
int	sobind(struct socket *so, struct sockaddr *nam, struct thread *td);
void	socantrcvmore(struct socket *so);
void	socantrcvmore_locked(struct socket *so);
void	socantsendmore(struct socket *so);
void	socantsendmore_locked(struct socket *so);
int	soclose(struct socket *so);
int	soconnect(struct socket *so, struct sockaddr *nam, struct thread *td);
int	soconnect2(struct socket *so1, struct socket *so2);
int	socow_setup(struct mbuf *m0, struct uio *uio);
int	socreate(int dom, struct socket **aso, int type, int proto,
	    struct ucred *cred, struct thread *td);
void	sodealloc(struct socket *so);
int	sodisconnect(struct socket *so);
struct	sockaddr *sodupsockaddr(const struct sockaddr *sa, int mflags);
void	sofree(struct socket *so);
int	sogetopt(struct socket *so, struct sockopt *sopt);
void	sohasoutofband(struct socket *so);
void	soisconnected(struct socket *so);
void	soisconnecting(struct socket *so);
void	soisdisconnected(struct socket *so);
void	soisdisconnecting(struct socket *so);
int	solisten(struct socket *so, int backlog, struct thread *td);
struct socket *
	sonewconn(struct socket *head, int connstatus);
int	sooptcopyin(struct sockopt *sopt, void *buf, size_t len, size_t minlen);
int	sooptcopyout(struct sockopt *sopt, const void *buf, size_t len);

/* XXX; prepare mbuf for (__FreeBSD__ < 3) routines. */
int	soopt_getm(struct sockopt *sopt, struct mbuf **mp);
int	soopt_mcopyin(struct sockopt *sopt, struct mbuf *m);
int	soopt_mcopyout(struct sockopt *sopt, struct mbuf *m);

int	sopoll(struct socket *so, int events, struct ucred *active_cred,
	    struct thread *td);
int	soreceive(struct socket *so, struct sockaddr **paddr, struct uio *uio,
	    struct mbuf **mp0, struct mbuf **controlp, int *flagsp);
int	soreserve(struct socket *so, u_long sndcc, u_long rcvcc);
void	sorflush(struct socket *so);
int	sosend(struct socket *so, struct sockaddr *addr, struct uio *uio,
	    struct mbuf *top, struct mbuf *control, int flags,
	    struct thread *td);
int	sosetopt(struct socket *so, struct sockopt *sopt);
int	soshutdown(struct socket *so, int how);
void	sotoxsocket(struct socket *so, struct xsocket *xso);
void	sowakeup(struct socket *so, struct sockbuf *sb);

#ifdef SOCKBUF_DEBUG
void	sblastrecordchk(struct sockbuf *, const char *, int);
#define	SBLASTRECORDCHK(sb)	sblastrecordchk((sb), __FILE__, __LINE__)

void	sblastmbufchk(struct sockbuf *, const char *, int);
#define	SBLASTMBUFCHK(sb)	sblastmbufchk((sb), __FILE__, __LINE__)
#else
#define	SBLASTRECORDCHK(sb)      /* nothing */
#define	SBLASTMBUFCHK(sb)        /* nothing */
#endif /* SOCKBUF_DEBUG */

/*
 * Accept filter functions (duh).
 */
int	accept_filt_add(struct accept_filter *filt);
int	accept_filt_del(char *name);
struct	accept_filter *accept_filt_get(char *name);
#ifdef ACCEPT_FILTER_MOD
#ifdef SYSCTL_DECL
SYSCTL_DECL(_net_inet_accf);
#endif
int	accept_filt_generic_mod_event(module_t mod, int event, void *data);
#endif

#endif /* _KERNEL */

#endif /* !_SYS_SOCKETVAR_H_ */
