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
 *	@(#)socketvar.h	8.3 (Berkeley) 2/19/95
 * $FreeBSD$
 */

#ifndef _SYS_SOCKETVAR_H_
#define _SYS_SOCKETVAR_H_

#include <sys/queue.h>			/* for TAILQ macros */
#include <sys/select.h>			/* for struct selinfo */

/*
 * Kernel structure per socket.
 * Contains send and receive buffer queues,
 * handle on protocol and pointer to protocol
 * private data and error information.
 */
typedef	u_quad_t so_gen_t;

struct accept_filter;

struct socket {
	struct	vm_zone *so_zone;	/* zone we were allocated from */
	short	so_type;		/* generic type, see socket.h */
	short	so_options;		/* from socket call, see socket.h */
	short	so_linger;		/* time to linger while closing */
	short	so_state;		/* internal state flags SS_*, below */
	caddr_t	so_pcb;			/* protocol control block */
	struct	protosw *so_proto;	/* protocol handle */
/*
 * Variables for connection queuing.
 * Socket where accepts occur is so_head in all subsidiary sockets.
 * If so_head is 0, socket is not related to an accept.
 * For head socket so_q0 queues partially completed connections,
 * while so_q is a queue of connections ready to be accepted.
 * If a connection is aborted and it has so_head set, then
 * it has to be pulled out of either so_q0 or so_q.
 * We allow connections to queue up based on current queue lengths
 * and limit on number of queued connections for this socket.
 */
	struct	socket *so_head;	/* back pointer to accept socket */
	TAILQ_HEAD(, socket) so_incomp;	/* queue of partial unaccepted connections */
	TAILQ_HEAD(, socket) so_comp;	/* queue of complete unaccepted connections */
	TAILQ_ENTRY(socket) so_list;	/* list of unaccepted connections */
	short	so_qlen;		/* number of unaccepted connections */
	short	so_incqlen;		/* number of unaccepted incomplete
					   connections */
	short	so_qlimit;		/* max number queued connections */
	short	so_timeo;		/* connection timeout */
	u_short	so_error;		/* error affecting connection */
	struct  sigio *so_sigio;	/* information for async I/O or
					   out of band data (SIGURG) */
	u_long	so_oobmark;		/* chars to oob mark */
	TAILQ_HEAD(, aiocblist) so_aiojobq; /* AIO ops waiting on socket */
/*
 * Variables for socket buffering.
 */
	struct	sockbuf {
		u_long	sb_cc;		/* actual chars in buffer */
		u_long	sb_hiwat;	/* max actual char count */
		u_long	sb_mbcnt;	/* chars of mbufs used */
		u_long	sb_mbmax;	/* max chars of mbufs to use */
		long	sb_lowat;	/* low water mark */
		struct	mbuf *sb_mb;	/* the mbuf chain */
		struct	selinfo sb_sel;	/* process selecting read/write */
		short	sb_flags;	/* flags, see below */
		short	sb_timeo;	/* timeout for read/write */
	} so_rcv, so_snd;
#define	SB_MAX		(256*1024)	/* default for max chars in sockbuf */
#define	SB_LOCK		0x01		/* lock on data queue */
#define	SB_WANT		0x02		/* someone is waiting to lock */
#define	SB_WAIT		0x04		/* someone is waiting for data/space */
#define	SB_SEL		0x08		/* someone is selecting */
#define	SB_ASYNC	0x10		/* ASYNC I/O, need signals */
#define	SB_UPCALL	0x20		/* someone wants an upcall */
#define	SB_NOINTR	0x40		/* operations not interruptible */
#define SB_AIO		0x80		/* AIO operations queued */
#define SB_KNOTE	0x100		/* kernel note attached */

	void	(*so_upcall) __P((struct socket *, void *, int));
	void	*so_upcallarg;
	struct	ucred *so_cred;		/* user credentials */
	/* NB: generation count must not be first; easiest to make it last. */
	so_gen_t so_gencnt;		/* generation count */
	void	*so_emuldata;		/* private data for emulators */
	struct	so_accf { 
		struct	accept_filter *so_accept_filter;
		void	*so_accept_filter_arg;	/* saved filter args */
		char	*so_accept_filter_str;	/* saved user args */
	} *so_accf;
};

/*
 * Socket state bits.
 */
#define	SS_NOFDREF		0x0001	/* no file table ref any more */
#define	SS_ISCONNECTED		0x0002	/* socket connected to a peer */
#define	SS_ISCONNECTING		0x0004	/* in process of connecting to peer */
#define	SS_ISDISCONNECTING	0x0008	/* in process of disconnecting */
#define	SS_CANTSENDMORE		0x0010	/* can't send more data to peer */
#define	SS_CANTRCVMORE		0x0020	/* can't receive more data from peer */
#define	SS_RCVATMARK		0x0040	/* at mark on input */

#define	SS_NBIO			0x0100	/* non-blocking ops */
#define	SS_ASYNC		0x0200	/* async i/o notify */
#define	SS_ISCONFIRMING		0x0400	/* deciding to accept connection req */

#define	SS_INCOMP		0x0800	/* unaccepted, incomplete connection */
#define	SS_COMP			0x1000	/* unaccepted, complete connection */
#define	SS_ISDISCONNECTED	0x2000	/* socket disconnected from peer */

/*
 * Externalized form of struct socket used by the sysctl(3) interface.
 */
struct	xsocket {
	size_t	xso_len;	/* length of this structure */
	struct	socket *xso_so;	/* makes a convenient handle sometimes */
	short	so_type;
	short	so_options;
	short	so_linger;
	short	so_state;
	caddr_t	so_pcb;		/* another convenient handle */
	int	xso_protocol;
	int	xso_family;
	short	so_qlen;
	short	so_incqlen;
	short	so_qlimit;
	short	so_timeo;
	u_short	so_error;
	pid_t	so_pgid;
	u_long	so_oobmark;
	struct	xsockbuf {
		u_long	sb_cc;
		u_long	sb_hiwat;
		u_long	sb_mbcnt;
		u_long	sb_mbmax;
		long	sb_lowat;
		short	sb_flags;
		short	sb_timeo;
	} so_rcv, so_snd;
	uid_t	so_uid;		/* XXX */
};

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
	((so)->so_state & SS_CANTRCVMORE) || \
	(so)->so_comp.tqh_first || (so)->so_error)

/* can we write something to so? */
#define	sowriteable(so) \
    ((sbspace(&(so)->so_snd) >= (so)->so_snd.sb_lowat && \
	(((so)->so_state&SS_ISCONNECTED) || \
	  ((so)->so_proto->pr_flags&PR_CONNREQUIRED)==0)) || \
     ((so)->so_state & SS_CANTSENDMORE) || \
     (so)->so_error)

/* adjust counters in sb reflecting allocation of m */
#define	sballoc(sb, m) { \
	(sb)->sb_cc += (m)->m_len; \
	(sb)->sb_mbcnt += MSIZE; \
	if ((m)->m_flags & M_EXT) \
		(sb)->sb_mbcnt += (m)->m_ext.ext_size; \
}

/* adjust counters in sb reflecting freeing of m */
#define	sbfree(sb, m) { \
	(sb)->sb_cc -= (m)->m_len; \
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
#define	sbunlock(sb) { \
	(sb)->sb_flags &= ~SB_LOCK; \
	if ((sb)->sb_flags & SB_WANT) { \
		(sb)->sb_flags &= ~SB_WANT; \
		wakeup((caddr_t)&(sb)->sb_flags); \
	} \
}

#define	sorwakeup(so)	do { \
			  if (sb_notify(&(so)->so_rcv)) \
			    sowakeup((so), &(so)->so_rcv); \
			} while (0)

#define	sowwakeup(so)	do { \
			  if (sb_notify(&(so)->so_snd)) \
			    sowakeup((so), &(so)->so_snd); \
			} while (0)

#ifdef _KERNEL

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
	struct	proc *sopt_p;	/* calling process or null if kernel */
};

struct sf_buf {
	SLIST_ENTRY(sf_buf) free_list;	/* list of free buffer slots */
	struct		vm_page *m;	/* currently mapped page */
	vm_offset_t	kva;		/* va of mapping */
};

struct accept_filter {
	char	accf_name[16];
	void	(*accf_callback)
		__P((struct socket *so, void *arg, int waitflag));
	void *	(*accf_create)
		__P((struct socket *so, char *arg));
	void	(*accf_destroy)
		__P((struct socket *so));
	SLIST_ENTRY(accept_filter) accf_next;	/* next on the list */
};	

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_PCB);
MALLOC_DECLARE(M_SONAME);
MALLOC_DECLARE(M_ACCF);
#endif

extern int	maxsockets;
extern u_long	sb_max;
extern struct	vm_zone *socket_zone;
extern so_gen_t so_gencnt;

struct file;
struct filedesc;
struct mbuf;
struct sockaddr;
struct stat;
struct ucred;
struct uio;

/*
 * File operations on sockets.
 */
int	soo_read __P((struct file *fp, struct uio *uio, struct ucred *cred,
	    int flags, struct proc *p));
int	soo_write __P((struct file *fp, struct uio *uio, struct ucred *cred,
	    int flags, struct proc *p));
int	soo_close __P((struct file *fp, struct proc *p));
int	soo_ioctl __P((struct file *fp, u_long cmd, caddr_t data,
	    struct proc *p));
int	soo_poll __P((struct file *fp, int events, struct ucred *cred,
	    struct proc *p));
int	soo_stat __P((struct file *fp, struct stat *ub, struct proc *p));

/*
 * From uipc_socket and friends
 */
struct	sockaddr *dup_sockaddr __P((struct sockaddr *sa, int canwait));
int	holdsock __P((struct filedesc *fdp, int fdes, struct file **fpp));
int	sockargs __P((struct mbuf **mp, caddr_t buf, int buflen, int type));
int	getsockaddr __P((struct sockaddr **namp, caddr_t uaddr, size_t len));
void	sbappend __P((struct sockbuf *sb, struct mbuf *m));
int	sbappendaddr __P((struct sockbuf *sb, struct sockaddr *asa,
	    struct mbuf *m0, struct mbuf *control));
int	sbappendcontrol __P((struct sockbuf *sb, struct mbuf *m0,
	    struct mbuf *control));
void	sbappendrecord __P((struct sockbuf *sb, struct mbuf *m0));
void	sbcheck __P((struct sockbuf *sb));
void	sbcompress __P((struct sockbuf *sb, struct mbuf *m, struct mbuf *n));
struct mbuf *
	sbcreatecontrol __P((caddr_t p, int size, int type, int level));
void	sbdrop __P((struct sockbuf *sb, int len));
void	sbdroprecord __P((struct sockbuf *sb));
void	sbflush __P((struct sockbuf *sb));
void	sbinsertoob __P((struct sockbuf *sb, struct mbuf *m0));
void	sbrelease __P((struct sockbuf *sb, struct socket *so));
int	sbreserve __P((struct sockbuf *sb, u_long cc, struct socket *so,
		       struct proc *p));
void	sbtoxsockbuf __P((struct sockbuf *sb, struct xsockbuf *xsb));
int	sbwait __P((struct sockbuf *sb));
int	sb_lock __P((struct sockbuf *sb));
int	soabort __P((struct socket *so));
int	soaccept __P((struct socket *so, struct sockaddr **nam));
struct	socket *soalloc __P((int waitok));
int	sobind __P((struct socket *so, struct sockaddr *nam, struct proc *p));
void	socantrcvmore __P((struct socket *so));
void	socantsendmore __P((struct socket *so));
int	soclose __P((struct socket *so));
int	soconnect __P((struct socket *so, struct sockaddr *nam, struct proc *p));
int	soconnect2 __P((struct socket *so1, struct socket *so2));
int	socreate __P((int dom, struct socket **aso, int type, int proto,
	    struct proc *p));
void	sodealloc __P((struct socket *so));
int	sodisconnect __P((struct socket *so));
void	sofree __P((struct socket *so));
int	sogetopt __P((struct socket *so, struct sockopt *sopt));
void	sohasoutofband __P((struct socket *so));
void	soisconnected __P((struct socket *so));
void	soisconnecting __P((struct socket *so));
void	soisdisconnected __P((struct socket *so));
void	soisdisconnecting __P((struct socket *so));
int	solisten __P((struct socket *so, int backlog, struct proc *p));
struct socket *
	sodropablereq __P((struct socket *head));
struct socket *
	sonewconn __P((struct socket *head, int connstatus));
struct socket *
	sonewconn3 __P((struct socket *head, int connstatus, struct proc *p));
int	sooptcopyin __P((struct sockopt *sopt, void *buf, size_t len,
			 size_t minlen));
int	sooptcopyout __P((struct sockopt *sopt, void *buf, size_t len));

/* XXX; prepare mbuf for (__FreeBSD__ < 3) routines. */
int	soopt_getm __P((struct sockopt *sopt, struct mbuf **mp));
int	soopt_mcopyin __P((struct sockopt *sopt, struct mbuf *m));
int	soopt_mcopyout __P((struct sockopt *sopt, struct mbuf *m));

int	sopoll __P((struct socket *so, int events, struct ucred *cred,
		    struct proc *p));
int	soreceive __P((struct socket *so, struct sockaddr **paddr,
		       struct uio *uio, struct mbuf **mp0,
		       struct mbuf **controlp, int *flagsp));
int	soreserve __P((struct socket *so, u_long sndcc, u_long rcvcc));
void	sorflush __P((struct socket *so));
int	sosend __P((struct socket *so, struct sockaddr *addr, struct uio *uio,
		    struct mbuf *top, struct mbuf *control, int flags,
		    struct proc *p));
int	sosetopt __P((struct socket *so, struct sockopt *sopt));
int	soshutdown __P((struct socket *so, int how));
void	sotoxsocket __P((struct socket *so, struct xsocket *xso));
void	sowakeup __P((struct socket *so, struct sockbuf *sb));

/* accept filter functions */
int	accept_filt_add __P((struct accept_filter *filt));
int	accept_filt_del __P((char *name));
struct accept_filter *	accept_filt_get __P((char *name));
#ifdef ACCEPT_FILTER_MOD
int accept_filt_generic_mod_event __P((module_t mod, int event, void *data));
SYSCTL_DECL(_net_inet_accf);
#endif /* ACCEPT_FILTER_MOD */

#endif /* _KERNEL */

#endif /* !_SYS_SOCKETVAR_H_ */
