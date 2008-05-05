/* $FreeBSD$ */

#ifndef CXGB_TCP_OFFLOAD_H_
#define CXGB_TCP_OFFLOAD_H_

struct tcpcb;
struct socket;
struct sockbuf;

void tcp_offload_twstart(struct tcpcb *tp);
void tcp_offload_twstart_disconnect(struct tcpcb *tp);
struct tcpcb *tcp_offload_close(struct tcpcb *tp);
struct tcpcb *tcp_offload_drop(struct tcpcb *tp, int error);

void inp_apply_all(void (*func)(struct inpcb *, void *), void *arg);
struct socket *inp_inpcbtosocket(struct inpcb *inp);
struct tcpcb *inp_inpcbtotcpcb(struct inpcb *inp);

int  inp_ip_tos_get(const struct inpcb *);
void inp_ip_tos_set(struct inpcb *, int);
void inp_4tuple_get(const struct inpcb *inp, uint32_t *, uint16_t *, uint32_t *, uint16_t *);

struct tcpcb *so_sototcpcb(struct socket *so);
struct inpcb *so_sotoinpcb(struct socket *so);
struct sockbuf *so_sockbuf_snd(struct socket *);
struct sockbuf *so_sockbuf_rcv(struct socket *);

int so_state_get(const struct socket *);
void so_state_set(struct socket *, int);

int so_options_get(const struct socket *);
void so_options_set(struct socket *, int);

int so_error_get(const struct socket *);
void so_error_set(struct socket *, int);

int so_linger_get(const struct socket *);
void so_linger_set(struct socket *, int);

struct protosw *so_protosw_get(const struct socket *);
void so_protosw_set(struct socket *, struct protosw *);

void so_sorwakeup_locked(struct socket *so);
void so_sowwakeup_locked(struct socket *so);

void so_sorwakeup(struct socket *so);
void so_sowwakeup(struct socket *so);

void so_lock(struct socket *so);
void so_unlock(struct socket *so);

void so_listeners_apply_all(struct socket *so, void (*func)(struct socket *, void *), void *arg);


void sockbuf_lock(struct sockbuf *);
void sockbuf_lock_assert(struct sockbuf *);
void sockbuf_unlock(struct sockbuf *);
int  sockbuf_sbspace(struct sockbuf *);

struct tcphdr;
struct tcpopt;

int	syncache_offload_expand(struct in_conninfo *, struct tcpopt *,
    struct tcphdr *, struct socket **, struct mbuf *);

#ifndef _SYS_SOCKETVAR_H_
#include <sys/selinfo.h>
#include <sys/sx.h>

/*
 * Constants for sb_flags field of struct sockbuf.
 */
#define	SB_MAX		(256*1024)	/* default for max chars in sockbuf */
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


struct sockbuf {
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
	u_int	sb_mbmax;	/* (c/d) max chars of mbufs to use */
	u_int	sb_ctl;		/* (c/d) non-data chars in buffer */
	int	sb_lowat;	/* (c/d) low water mark */
	int	sb_timeo;	/* (c/d) timeout for read/write */
	short	sb_flags;	/* (c/d) flags, see below */
};

void	sbappend(struct sockbuf *sb, struct mbuf *m);
void	sbappend_locked(struct sockbuf *sb, struct mbuf *m);
void	sbappendstream(struct sockbuf *sb, struct mbuf *m);
void	sbappendstream_locked(struct sockbuf *sb, struct mbuf *m);
void	sbdrop(struct sockbuf *sb, int len);
void	sbdrop_locked(struct sockbuf *sb, int len);
void	sbdroprecord(struct sockbuf *sb);
void	sbdroprecord_locked(struct sockbuf *sb);
void	sbflush(struct sockbuf *sb);
void	sbflush_locked(struct sockbuf *sb);
int	sbwait(struct sockbuf *sb);
int	sblock(struct sockbuf *, int);
void	sbunlock(struct sockbuf *);



/* adjust counters in sb reflecting allocation of m */
#define	sballoc(sb, m) { \
	(sb)->sb_cc += (m)->m_len; \
	if ((m)->m_type != MT_DATA && (m)->m_type != MT_OOBDATA) \
		(sb)->sb_ctl += (m)->m_len; \
	(sb)->sb_mbcnt += MSIZE; \
	if ((m)->m_flags & M_EXT) \
		(sb)->sb_mbcnt += (m)->m_ext.ext_size; \
}

/* adjust counters in sb reflecting freeing of m */
#define	sbfree(sb, m) { \
	(sb)->sb_cc -= (m)->m_len; \
	if ((m)->m_type != MT_DATA && (m)->m_type != MT_OOBDATA) \
		(sb)->sb_ctl -= (m)->m_len; \
	(sb)->sb_mbcnt -= MSIZE; \
	if ((m)->m_flags & M_EXT) \
		(sb)->sb_mbcnt -= (m)->m_ext.ext_size; \
	if ((sb)->sb_sndptr == (m)) { \
		(sb)->sb_sndptr = NULL; \
		(sb)->sb_sndptroff = 0; \
	} \
	if ((sb)->sb_sndptroff != 0) \
		(sb)->sb_sndptroff -= (m)->m_len; \
}

#define	SS_NOFDREF		0x0001	/* no file table ref any more */
#define	SS_ISCONNECTED		0x0002	/* socket connected to a peer */
#define	SS_ISCONNECTING		0x0004	/* in process of connecting to peer */
#define	SS_ISDISCONNECTING	0x0008	/* in process of disconnecting */
#define	SS_NBIO			0x0100	/* non-blocking ops */
#define	SS_ASYNC		0x0200	/* async i/o notify */
#define	SS_ISCONFIRMING		0x0400	/* deciding to accept connection req */
#define	SS_ISDISCONNECTED	0x2000	/* socket disconnected from peer */
/*
 * Protocols can mark a socket as SS_PROTOREF to indicate that, following
 * pru_detach, they still want the socket to persist, and will free it
 * themselves when they are done.  Protocols should only ever call sofree()
 * following setting this flag in pru_detach(), and never otherwise, as
 * sofree() bypasses socket reference counting.
 */
#define	SS_PROTOREF		0x4000	/* strong protocol reference */

/*
 * Socket state bits now stored in the socket buffer state field.
 */
#define	SBS_CANTSENDMORE	0x0010	/* can't send more data to peer */
#define	SBS_CANTRCVMORE		0x0020	/* can't receive more data from peer */
#define	SBS_RCVATMARK		0x0040	/* at mark on input */



enum sopt_dir { SOPT_GET, SOPT_SET };
struct sockopt {
	enum	sopt_dir sopt_dir; /* is this a get or a set? */
	int	sopt_level;	/* second arg of [gs]etsockopt */
	int	sopt_name;	/* third arg of [gs]etsockopt */
	void   *sopt_val;	/* fourth arg of [gs]etsockopt */
	size_t	sopt_valsize;	/* (almost) fifth arg of [gs]etsockopt */
	struct	thread *sopt_td; /* calling thread or null if kernel */
};


int	sooptcopyin(struct sockopt *sopt, void *buf, size_t len, size_t minlen);
int	sooptcopyout(struct sockopt *sopt, const void *buf, size_t len);


void	soisconnected(struct socket *so);
void	soisconnecting(struct socket *so);
void	soisdisconnected(struct socket *so);
void	soisdisconnecting(struct socket *so);
void	socantrcvmore(struct socket *so);
void	socantrcvmore_locked(struct socket *so);
void	socantsendmore(struct socket *so);
void	socantsendmore_locked(struct socket *so);

#endif /* !NET_CORE */


#endif /* CXGB_TCP_OFFLOAD_H_ */
