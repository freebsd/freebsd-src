/*-
 * Copyright (c) 1982, 1986, 1988, 1993
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
 *
 *	@(#)mbuf.h	8.5 (Berkeley) 2/19/95
 * $FreeBSD$
 */

#ifndef _SYS_MBUF_H_
#define	_SYS_MBUF_H_

/* XXX: These includes suck. Sorry! */
#include <sys/queue.h>
#ifdef _KERNEL
#include <sys/systm.h>
#include <vm/uma.h>
#ifdef WITNESS
#include <sys/lock.h>
#endif
#endif

/*
 * Mbufs are of a single size, MSIZE (sys/param.h), which
 * includes overhead.  An mbuf may add a single "mbuf cluster" of size
 * MCLBYTES (also in sys/param.h), which has no additional overhead
 * and is used instead of the internal data area; this is done when
 * at least MINCLSIZE of data must be stored.  Additionally, it is possible
 * to allocate a separate buffer externally and attach it to the mbuf in
 * a way similar to that of mbuf clusters.
 */
#define	MLEN		(MSIZE - sizeof(struct m_hdr))	/* normal data len */
#define	MHLEN		(MLEN - sizeof(struct pkthdr))	/* data len w/pkthdr */
#define	MINCLSIZE	(MHLEN + 1)	/* smallest amount to put in cluster */
#define	M_MAXCOMPRESS	(MHLEN / 2)	/* max amount to copy for compression */

#ifdef _KERNEL
/*-
 * Macros for type conversion:
 * mtod(m, t)	-- Convert mbuf pointer to data pointer of correct type.
 * dtom(x)	-- Convert data pointer within mbuf to mbuf pointer (XXX).
 */
#define	mtod(m, t)	((t)((m)->m_data))
#define	dtom(x)		((struct mbuf *)((intptr_t)(x) & ~(MSIZE-1)))

/*
 * Argument structure passed to UMA routines during mbuf and packet
 * allocations.
 */
struct mb_args {
	int	flags;	/* Flags for mbuf being allocated */
	short	type;	/* Type of mbuf being allocated */
};
#endif /* _KERNEL */

/*
 * Header present at the beginning of every mbuf.
 */
struct m_hdr {
	struct	mbuf *mh_next;		/* next buffer in chain */
	struct	mbuf *mh_nextpkt;	/* next chain in queue/record */
	caddr_t	mh_data;		/* location of data */
	int	mh_len;			/* amount of data in this mbuf */
	int	mh_flags;		/* flags; see below */
	short	mh_type;		/* type of data in this mbuf */
};

/*
 * Packet tag structure (see below for details).
 */
struct m_tag {
	SLIST_ENTRY(m_tag)	m_tag_link;	/* List of packet tags */
	u_int16_t		m_tag_id;	/* Tag ID */
	u_int16_t		m_tag_len;	/* Length of data */
	u_int32_t		m_tag_cookie;	/* ABI/Module ID */
	void			(*m_tag_free)(struct m_tag *);
};

/*
 * Record/packet header in first mbuf of chain; valid only if M_PKTHDR is set.
 */
struct pkthdr {
	struct	ifnet *rcvif;		/* rcv interface */
	int	len;			/* total packet length */
	/* variables for ip and tcp reassembly */
	void	*header;		/* pointer to packet header */
	/* variables for hardware checksum */
	int	csum_flags;		/* flags regarding checksum */
	int	csum_data;		/* data field used by csum routines */
	SLIST_HEAD(packet_tags, m_tag) tags; /* list of packet tags */
};

/*
 * Description of external storage mapped into mbuf; valid only if M_EXT is set.
 */
struct m_ext {
	caddr_t	ext_buf;		/* start of buffer */
	void	(*ext_free)		/* free routine if not the usual */
		    (void *, void *);
	void	*ext_args;		/* optional argument pointer */
	u_int	ext_size;		/* size of buffer, for ext_free */
	volatile u_int *ref_cnt;	/* pointer to ref count info */
	int	ext_type;		/* type of external storage */
};

/*
 * The core of the mbuf object along with some shortcut defines for
 * practical purposes.
 */
struct mbuf {
	struct	m_hdr m_hdr;
	union {
		struct {
			struct	pkthdr MH_pkthdr;	/* M_PKTHDR set */
			union {
				struct	m_ext MH_ext;	/* M_EXT set */
				char	MH_databuf[MHLEN];
			} MH_dat;
		} MH;
		char	M_databuf[MLEN];		/* !M_PKTHDR, !M_EXT */
	} M_dat;
};
#define	m_next		m_hdr.mh_next
#define	m_len		m_hdr.mh_len
#define	m_data		m_hdr.mh_data
#define	m_type		m_hdr.mh_type
#define	m_flags		m_hdr.mh_flags
#define	m_nextpkt	m_hdr.mh_nextpkt
#define	m_act		m_nextpkt
#define	m_pkthdr	M_dat.MH.MH_pkthdr
#define	m_ext		M_dat.MH.MH_dat.MH_ext
#define	m_pktdat	M_dat.MH.MH_dat.MH_databuf
#define	m_dat		M_dat.M_databuf

/*
 * mbuf flags.
 */
#define	M_EXT		0x0001	/* has associated external storage */
#define	M_PKTHDR	0x0002	/* start of record */
#define	M_EOR		0x0004	/* end of record */
#define	M_RDONLY	0x0008	/* associated data is marked read-only */
#define	M_PROTO1	0x0010	/* protocol-specific */
#define	M_PROTO2	0x0020	/* protocol-specific */
#define	M_PROTO3	0x0040	/* protocol-specific */
#define	M_PROTO4	0x0080	/* protocol-specific */
#define	M_PROTO5	0x0100	/* protocol-specific */
#define	M_SKIP_FIREWALL	0x4000	/* skip firewall processing */
#define	M_FREELIST	0x8000	/* mbuf is on the free list */

/*
 * mbuf pkthdr flags (also stored in m_flags).
 */
#define	M_BCAST		0x0200	/* send/received as link-level broadcast */
#define	M_MCAST		0x0400	/* send/received as link-level multicast */
#define	M_FRAG		0x0800	/* packet is a fragment of a larger packet */
#define	M_FIRSTFRAG	0x1000	/* packet is first fragment */
#define	M_LASTFRAG	0x2000	/* packet is last fragment */
#define	M_VLANTAG	0x10000	/* packet has VLAN tag attached */

/*
 * External buffer types: identify ext_buf type.
 */
#define	EXT_CLUSTER	1	/* mbuf cluster */
#define	EXT_SFBUF	2	/* sendfile(2)'s sf_bufs */
#define	EXT_PACKET	3	/* came out of Packet zone */
#define	EXT_NET_DRV	100	/* custom ext_buf provided by net driver(s) */
#define	EXT_MOD_TYPE	200	/* custom module's ext_buf type */
#define	EXT_DISPOSABLE	300	/* can throw this buffer away w/page flipping */
#define	EXT_EXTREF	400	/* has externally maintained ref_cnt ptr */

/*
 * Flags copied when copying m_pkthdr.
 */
#define	M_COPYFLAGS	(M_PKTHDR|M_EOR|M_RDONLY|M_PROTO1|M_PROTO1|M_PROTO2|\
			    M_PROTO3|M_PROTO4|M_PROTO5|M_SKIP_FIREWALL|\
			    M_BCAST|M_MCAST|M_FRAG|M_FIRSTFRAG|M_LASTFRAG|\
			    M_VLANTAG)

/*
 * Flags indicating hw checksum support and sw checksum requirements.
 */
#define	CSUM_IP			0x0001		/* will csum IP */
#define	CSUM_TCP		0x0002		/* will csum TCP */
#define	CSUM_UDP		0x0004		/* will csum UDP */
#define	CSUM_IP_FRAGS		0x0008		/* will csum IP fragments */
#define	CSUM_FRAGMENT		0x0010		/* will do IP fragmentation */

#define	CSUM_IP_CHECKED		0x0100		/* did csum IP */
#define	CSUM_IP_VALID		0x0200		/*   ... the csum is valid */
#define	CSUM_DATA_VALID		0x0400		/* csum_data field is valid */
#define	CSUM_PSEUDO_HDR		0x0800		/* csum_data has pseudo hdr */

#define	CSUM_DELAY_DATA		(CSUM_TCP | CSUM_UDP)
#define	CSUM_DELAY_IP		(CSUM_IP)	/* XXX add ipv6 here too? */

/*
 * mbuf types.
 */
#define	MT_NOTMBUF	0	/* USED INTERNALLY ONLY! Object is not mbuf */
#define	MT_DATA		1	/* dynamic (data) allocation */
#define	MT_HEADER	2	/* packet header */
#if 0
#define	MT_SOCKET	3	/* socket structure */
#define	MT_PCB		4	/* protocol control block */
#define	MT_RTABLE	5	/* routing tables */
#define	MT_HTABLE	6	/* IMP host tables */
#define	MT_ATABLE	7	/* address resolution tables */
#endif
#define	MT_SONAME	8	/* socket name */
#if 0
#define	MT_SOOPTS	10	/* socket options */
#endif
#define	MT_FTABLE	11	/* fragment reassembly header */
#if 0
#define	MT_RIGHTS	12	/* access rights */
#define	MT_IFADDR	13	/* interface address */
#endif
#define	MT_CONTROL	14	/* extra-data protocol message */
#define	MT_OOBDATA	15	/* expedited data  */
#define	MT_NTYPES	16	/* number of mbuf types for mbtypes[] */

/*
 * General mbuf allocator statistics structure.
 */
struct mbstat {
	u_long	m_mbufs;	/* XXX */
	u_long	m_mclusts;	/* XXX */

	u_long	m_drain;	/* times drained protocols for space */
	u_long	m_mcfail;	/* XXX: times m_copym failed */
	u_long	m_mpfail;	/* XXX: times m_pullup failed */
	u_long	m_msize;	/* length of an mbuf */
	u_long	m_mclbytes;	/* length of an mbuf cluster */
	u_long	m_minclsize;	/* min length of data to allocate a cluster */
	u_long	m_mlen;		/* length of data in an mbuf */
	u_long	m_mhlen;	/* length of data in a header mbuf */

	/* Number of mbtypes (gives # elems in mbtypes[] array: */
	short	m_numtypes;

	/* XXX: Sendfile stats should eventually move to their own struct */
	u_long	sf_iocnt;	/* times sendfile had to do disk I/O */
	u_long	sf_allocfail;	/* times sfbuf allocation failed */
	u_long	sf_allocwait;	/* times sfbuf allocation had to wait */
};

/*
 * Flags specifying how an allocation should be made.
 *
 * The flag to use is as follows:
 * - M_DONTWAIT or M_NOWAIT from an interrupt handler to not block allocation.
 * - M_WAIT or M_WAITOK or M_TRYWAIT from wherever it is safe to block.
 *
 * M_DONTWAIT/M_NOWAIT means that we will not block the thread explicitly
 * and if we cannot allocate immediately we may return NULL,
 * whereas M_WAIT/M_WAITOK/M_TRYWAIT means that if we cannot allocate
 * resources we will block until they are available, and thus never
 * return NULL.
 *
 * XXX Eventually just phase this out to use M_WAITOK/M_NOWAIT.
 */
#define	MBTOM(how)	(how)
#define	M_DONTWAIT	M_NOWAIT
#define	M_TRYWAIT	M_WAITOK
#define	M_WAIT		M_WAITOK

#ifdef _KERNEL
/*-
 * mbuf external reference count management macros.
 *
 * MEXT_IS_REF(m): true if (m) is not the only mbuf referencing
 *     the external buffer ext_buf.
 *
 * MEXT_REM_REF(m): remove reference to m_ext object.
 *
 * MEXT_ADD_REF(m): add reference to m_ext object already
 *     referred to by (m).  XXX Note that it is VERY important that you
 *     always set the second mbuf's m_ext.ref_cnt to point to the first
 *     one's (i.e., n->m_ext.ref_cnt = m->m_ext.ref_cnt) AFTER you run
 *     MEXT_ADD_REF(m).  This is because m might have a lazy initialized
 *     ref_cnt (NULL) before this is run and it will only be looked up
 *     from here.  We should make MEXT_ADD_REF() always take two mbufs
 *     as arguments so that it can take care of this itself.
 */
#define	MEXT_IS_REF(m)	(((m)->m_ext.ref_cnt != NULL)			\
    && (*((m)->m_ext.ref_cnt) > 1))

#define	MEXT_REM_REF(m) do {						\
	KASSERT((m)->m_ext.ref_cnt != NULL, ("m_ext refcnt lazy NULL")); \
	KASSERT(*((m)->m_ext.ref_cnt) > 0, ("m_ext refcnt < 0"));	\
	atomic_subtract_int((m)->m_ext.ref_cnt, 1);			\
} while(0)

#define	MEXT_ADD_REF(m)	do {						\
	if ((m)->m_ext.ref_cnt == NULL) {				\
		KASSERT((m)->m_ext.ext_type == EXT_CLUSTER ||		\
		    (m)->m_ext.ext_type == EXT_PACKET,			\
		    ("Unexpected mbuf type has lazy refcnt"));		\
		(m)->m_ext.ref_cnt = (u_int *)uma_find_refcnt(		\
		    zone_clust, (m)->m_ext.ext_buf);			\
		*((m)->m_ext.ref_cnt) = 2;				\
	} else								\
		atomic_add_int((m)->m_ext.ref_cnt, 1);			\
} while (0)

#ifdef WITNESS
#define MBUF_CHECKSLEEP(how) do {					\
	if (how == M_WAITOK)						\
		WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,		\
		    "Sleeping in \"%s\"", __func__);			\
} while(0)
#else
#define MBUF_CHECKSLEEP(how)
#endif

/*
 * Network buffer allocation API
 *
 * The rest of it is defined in kern/kern_mbuf.c
 */

extern uma_zone_t	zone_mbuf;
extern uma_zone_t	zone_clust;
extern uma_zone_t	zone_pack;

static __inline struct mbuf	*m_get(int how, short type);
static __inline struct mbuf	*m_gethdr(int how, short type);
static __inline struct mbuf	*m_getcl(int how, short type, int flags);
static __inline struct mbuf	*m_getclr(int how, short type);	/* XXX */
static __inline struct mbuf	*m_free(struct mbuf *m);
static __inline void		 m_clget(struct mbuf *m, int how);
static __inline void		 m_chtype(struct mbuf *m, short new_type);
void				 mb_free_ext(struct mbuf *);

static __inline
struct mbuf *
m_get(int how, short type)
{
	struct mb_args args;

	args.flags = 0;
	args.type = type;
	return (uma_zalloc_arg(zone_mbuf, &args, how));
}

/* XXX This should be depracated, very little use */
static __inline
struct mbuf *
m_getclr(int how, short type)
{
	struct mbuf *m;
	struct mb_args args;

	args.flags = 0;
	args.type = type;
	m = uma_zalloc_arg(zone_mbuf, &args, how);
	if (m != NULL)
		bzero(m->m_data, MLEN);
	return m;
}

static __inline
struct mbuf *
m_gethdr(int how, short type)
{
	struct mb_args args;

	args.flags = M_PKTHDR;
	args.type = type;
	return (uma_zalloc_arg(zone_mbuf, &args, how));
}

static __inline
struct mbuf *
m_getcl(int how, short type, int flags)
{
	struct mb_args args;

	args.flags = flags;
	args.type = type;
	return (uma_zalloc_arg(zone_pack, &args, how));
}

static __inline
struct mbuf *
m_free(struct mbuf *m)
{
	struct mbuf *n = m->m_next;

#ifdef INVARIANTS
	m->m_flags |= M_FREELIST;
#endif
	if (m->m_flags & M_EXT)
		mb_free_ext(m);
	else
		uma_zfree(zone_mbuf, m);
	return n;
}

static __inline
void
m_clget(struct mbuf *m, int how)
{

	m->m_ext.ext_buf = NULL;
	uma_zalloc_arg(zone_clust, m, how);
}

static __inline
void
m_chtype(struct mbuf *m, short new_type)
{
	m->m_type = new_type;
}

/*
 * mbuf, cluster, and external object allocation macros
 * (for compatibility purposes).
 */
/* NB: M_COPY_PKTHDR is deprecated.  Use M_MOVE_PKTHDR or m_dup_pktdr. */
#define	M_MOVE_PKTHDR(to, from)	m_move_pkthdr((to), (from))
#define	MGET(m, how, type)	((m) = m_get((how), (type)))
#define	MGETHDR(m, how, type)	((m) = m_gethdr((how), (type)))
#define	MCLGET(m, how)		m_clget((m), (how))
#define	MEXTADD(m, buf, size, free, args, flags, type) 			\
    m_extadd((m), (caddr_t)(buf), (size), (free), (args), (flags), (type))

/*
 * Evaluate TRUE if it's safe to write to the mbuf m's data region (this
 * can be both the local data payload, or an external buffer area,
 * depending on whether M_EXT is set).
 */
#define	M_WRITABLE(m)	(!((m)->m_flags & M_RDONLY) && (!((m)->m_flags  \
			    & M_EXT) || !MEXT_IS_REF(m)))

/* Check if the supplied mbuf has a packet header, or else panic. */
#define	M_ASSERTPKTHDR(m)						\
	KASSERT(m != NULL && m->m_flags & M_PKTHDR,			\
	    ("%s: no mbuf packet header!", __func__))

/* Ensure that the supplied mbuf is a valid, non-free mbuf. */
#define	M_ASSERTVALID(m)						\
	KASSERT((((struct mbuf *)m)->m_flags & M_FREELIST) == 0,	\
	    ("%s: attempted use of a free mbuf!", __func__))

/*
 * Set the m_data pointer of a newly-allocated mbuf (m_get/MGET) to place
 * an object of the specified size at the end of the mbuf, longword aligned.
 */
#define	M_ALIGN(m, len) do {						\
	(m)->m_data += (MLEN - (len)) & ~(sizeof(long) - 1);		\
} while (0)

/*
 * As above, for mbufs allocated with m_gethdr/MGETHDR
 * or initialized by M_COPY_PKTHDR.
 */
#define	MH_ALIGN(m, len) do {						\
	(m)->m_data += (MHLEN - (len)) & ~(sizeof(long) - 1);		\
} while (0)

/*
 * Compute the amount of space available
 * before the current start of data in an mbuf.
 *
 * The M_WRITABLE() is a temporary, conservative safety measure: the burden
 * of checking writability of the mbuf data area rests solely with the caller.
 */
#define	M_LEADINGSPACE(m)						\
	((m)->m_flags & M_EXT ?						\
	    (M_WRITABLE(m) ? (m)->m_data - (m)->m_ext.ext_buf : 0):	\
	    (m)->m_flags & M_PKTHDR ? (m)->m_data - (m)->m_pktdat :	\
	    (m)->m_data - (m)->m_dat)

/*
 * Compute the amount of space available
 * after the end of data in an mbuf.
 *
 * The M_WRITABLE() is a temporary, conservative safety measure: the burden
 * of checking writability of the mbuf data area rests solely with the caller.
 */
#define	M_TRAILINGSPACE(m)						\
	((m)->m_flags & M_EXT ?						\
	    (M_WRITABLE(m) ? (m)->m_ext.ext_buf + (m)->m_ext.ext_size	\
		- ((m)->m_data + (m)->m_len) : 0) :			\
	    &(m)->m_dat[MLEN] - ((m)->m_data + (m)->m_len))

/*
 * Arrange to prepend space of size plen to mbuf m.
 * If a new mbuf must be allocated, how specifies whether to wait.
 * If the allocation fails, the original mbuf chain is freed and m is
 * set to NULL.
 */
#define	M_PREPEND(m, plen, how) do {					\
	struct mbuf **_mmp = &(m);					\
	struct mbuf *_mm = *_mmp;					\
	int _mplen = (plen);						\
	int __mhow = (how);						\
									\
	MBUF_CHECKSLEEP(how);						\
	if (M_LEADINGSPACE(_mm) >= _mplen) {				\
		_mm->m_data -= _mplen;					\
		_mm->m_len += _mplen;					\
	} else								\
		_mm = m_prepend(_mm, _mplen, __mhow);			\
	if (_mm != NULL && _mm->m_flags & M_PKTHDR)			\
		_mm->m_pkthdr.len += _mplen;				\
	*_mmp = _mm;							\
} while (0)

/*
 * Change mbuf to new type.
 * This is a relatively expensive operation and should be avoided.
 */
#define	MCHTYPE(m, t)	m_chtype((m), (t))

/* Length to m_copy to copy all. */
#define	M_COPYALL	1000000000

/* Compatibility with 4.3. */
#define	m_copy(m, o, l)	m_copym((m), (o), (l), M_DONTWAIT)

extern	int max_datalen;		/* MHLEN - max_hdr */
extern	int max_hdr;			/* Largest link + protocol header */
extern	int max_linkhdr;		/* Largest link-level header */
extern	int max_protohdr;		/* Largest protocol header */
extern	struct mbstat mbstat;		/* General mbuf stats/infos */
extern	int nmbclusters;		/* Maximum number of clusters */

struct uio;

void		 m_adj(struct mbuf *, int);
int		 m_apply(struct mbuf *, int, int,
		    int (*)(void *, void *, u_int), void *);
int		 m_append(struct mbuf *, int, c_caddr_t);
void		 m_cat(struct mbuf *, struct mbuf *);
void		 m_extadd(struct mbuf *, caddr_t, u_int,
		    void (*)(void *, void *), void *, int, int);
void		 m_copyback(struct mbuf *, int, int, c_caddr_t);
void		 m_copydata(const struct mbuf *, int, int, caddr_t);
struct	mbuf	*m_copym(struct mbuf *, int, int, int);
struct	mbuf	*m_copypacket(struct mbuf *, int);
void		 m_copy_pkthdr(struct mbuf *, struct mbuf *);
struct	mbuf	*m_defrag(struct mbuf *, int);
struct	mbuf	*m_devget(char *, int, int, struct ifnet *,
		    void (*)(char *, caddr_t, u_int));
struct	mbuf	*m_dup(struct mbuf *, int);
int		 m_dup_pkthdr(struct mbuf *, struct mbuf *, int);
u_int		 m_fixhdr(struct mbuf *);
struct	mbuf	*m_fragment(struct mbuf *, int, int);
void		 m_freem(struct mbuf *);
struct	mbuf	*m_getm(struct mbuf *, int, int, short);
struct	mbuf	*m_getptr(struct mbuf *, int, int *);
u_int		 m_length(struct mbuf *, struct mbuf **);
void		 m_move_pkthdr(struct mbuf *, struct mbuf *);
struct	mbuf	*m_prepend(struct mbuf *, int, int);
void		 m_print(const struct mbuf *, int);
struct	mbuf	*m_pulldown(struct mbuf *, int, int, int *);
struct	mbuf	*m_pullup(struct mbuf *, int);
struct	mbuf	*m_split(struct mbuf *, int, int);
struct	mbuf	*m_uiotombuf(struct uio *, int, int);

/*-
 * Network packets may have annotations attached by affixing a list
 * of "packet tags" to the pkthdr structure.  Packet tags are
 * dynamically allocated semi-opaque data structures that have
 * a fixed header (struct m_tag) that specifies the size of the
 * memory block and a <cookie,type> pair that identifies it.
 * The cookie is a 32-bit unique unsigned value used to identify
 * a module or ABI.  By convention this value is chose as the
 * date+time that the module is created, expressed as the number of
 * seconds since the epoch (e.g., using date -u +'%s').  The type value
 * is an ABI/module-specific value that identifies a particular annotation
 * and is private to the module.  For compatibility with systems
 * like OpenBSD that define packet tags w/o an ABI/module cookie,
 * the value PACKET_ABI_COMPAT is used to implement m_tag_get and
 * m_tag_find compatibility shim functions and several tag types are
 * defined below.  Users that do not require compatibility should use
 * a private cookie value so that packet tag-related definitions
 * can be maintained privately.
 *
 * Note that the packet tag returned by m_tag_alloc has the default
 * memory alignment implemented by malloc.  To reference private data
 * one can use a construct like:
 *
 *	struct m_tag *mtag = m_tag_alloc(...);
 *	struct foo *p = (struct foo *)(mtag+1);
 *
 * if the alignment of struct m_tag is sufficient for referencing members
 * of struct foo.  Otherwise it is necessary to embed struct m_tag within
 * the private data structure to insure proper alignment; e.g.,
 *
 *	struct foo {
 *		struct m_tag	tag;
 *		...
 *	};
 *	struct foo *p = (struct foo *) m_tag_alloc(...);
 *	struct m_tag *mtag = &p->tag;
 */

/*
 * Persistent tags stay with an mbuf until the mbuf is reclaimed.
 * Otherwise tags are expected to ``vanish'' when they pass through
 * a network interface.  For most interfaces this happens normally
 * as the tags are reclaimed when the mbuf is free'd.  However in
 * some special cases reclaiming must be done manually.  An example
 * is packets that pass through the loopback interface.  Also, one
 * must be careful to do this when ``turning around'' packets (e.g.,
 * icmp_reflect).
 *
 * To mark a tag persistent bit-or this flag in when defining the
 * tag id.  The tag will then be treated as described above.
 */
#define	MTAG_PERSISTENT				0x800

#define	PACKET_TAG_NONE				0  /* Nadda */

/* Packet tags for use with PACKET_ABI_COMPAT. */
#define	PACKET_TAG_IPSEC_IN_DONE		1  /* IPsec applied, in */
#define	PACKET_TAG_IPSEC_OUT_DONE		2  /* IPsec applied, out */
#define	PACKET_TAG_IPSEC_IN_CRYPTO_DONE		3  /* NIC IPsec crypto done */
#define	PACKET_TAG_IPSEC_OUT_CRYPTO_NEEDED	4  /* NIC IPsec crypto req'ed */
#define	PACKET_TAG_IPSEC_IN_COULD_DO_CRYPTO	5  /* NIC notifies IPsec */
#define	PACKET_TAG_IPSEC_PENDING_TDB		6  /* Reminder to do IPsec */
#define	PACKET_TAG_BRIDGE			7  /* Bridge processing done */
#define	PACKET_TAG_GIF				8  /* GIF processing done */
#define	PACKET_TAG_GRE				9  /* GRE processing done */
#define	PACKET_TAG_IN_PACKET_CHECKSUM		10 /* NIC checksumming done */
#define	PACKET_TAG_ENCAP			11 /* Encap.  processing */
#define	PACKET_TAG_IPSEC_SOCKET			12 /* IPSEC socket ref */
#define	PACKET_TAG_IPSEC_HISTORY		13 /* IPSEC history */
#define	PACKET_TAG_IPV6_INPUT			14 /* IPV6 input processing */
#define	PACKET_TAG_DUMMYNET			15 /* dummynet info */
#define	PACKET_TAG_DIVERT			17 /* divert info */
#define	PACKET_TAG_IPFORWARD			18 /* ipforward info */
#define	PACKET_TAG_MACLABEL	(19 | MTAG_PERSISTENT) /* MAC label */
#define	PACKET_TAG_PF_ROUTED			21 /* PF routed, avoid loops */
#define	PACKET_TAG_PF_FRAGCACHE			22 /* PF fragment cached */
#define	PACKET_TAG_PF_QID			23 /* PF ALTQ queue id */
#define	PACKET_TAG_PF_TAG			24 /* PF tagged */
#define	PACKET_TAG_RTSOCKFAM			25 /* rtsock sa family */
#define	PACKET_TAG_PF_TRANSLATE_LOCALHOST	26 /* PF translate localhost */
#define	PACKET_TAG_IPOPTIONS			27 /* Saved IP options */
#define	PACKET_TAG_CARP                         28 /* CARP info */

/* Packet tag routines. */
struct	m_tag	*m_tag_alloc(u_int32_t, int, int, int);
void		 m_tag_delete(struct mbuf *, struct m_tag *);
void		 m_tag_delete_chain(struct mbuf *, struct m_tag *);
void		 m_tag_free_default(struct m_tag *);
struct	m_tag	*m_tag_locate(struct mbuf *, u_int32_t, int, struct m_tag *);
struct	m_tag	*m_tag_copy(struct m_tag *, int);
int		 m_tag_copy_chain(struct mbuf *, struct mbuf *, int);
void		 m_tag_delete_nonpersistent(struct mbuf *);

/*
 * Initialize the list of tags associated with an mbuf.
 */
static __inline void
m_tag_init(struct mbuf *m)
{
	SLIST_INIT(&m->m_pkthdr.tags);
}

/*
 * Set up the contents of a tag.  Note that this does not
 * fill in the free method; the caller is expected to do that.
 *
 * XXX probably should be called m_tag_init, but that was
 * already taken.
 */
static __inline void
m_tag_setup(struct m_tag *t, u_int32_t cookie, int type, int len)
{
	t->m_tag_id = type;
	t->m_tag_len = len;
	t->m_tag_cookie = cookie;
}

/*
 * Reclaim resources associated with a tag.
 */
static __inline void
m_tag_free(struct m_tag *t)
{
	(*t->m_tag_free)(t);
}

/*
 * Return the first tag associated with an mbuf.
 */
static __inline struct m_tag *
m_tag_first(struct mbuf *m)
{
	return (SLIST_FIRST(&m->m_pkthdr.tags));
}

/*
 * Return the next tag in the list of tags associated with an mbuf.
 */
static __inline struct m_tag *
m_tag_next(struct mbuf *m, struct m_tag *t)
{
	return (SLIST_NEXT(t, m_tag_link));
}

/*
 * Prepend a tag to the list of tags associated with an mbuf.
 */
static __inline void
m_tag_prepend(struct mbuf *m, struct m_tag *t)
{
	SLIST_INSERT_HEAD(&m->m_pkthdr.tags, t, m_tag_link);
}

/*
 * Unlink a tag from the list of tags associated with an mbuf.
 */
static __inline void
m_tag_unlink(struct mbuf *m, struct m_tag *t)
{
	SLIST_REMOVE(&m->m_pkthdr.tags, t, m_tag, m_tag_link);
}

/* These are for OpenBSD compatibility. */
#define	MTAG_ABI_COMPAT		0		/* compatibility ABI */

static __inline struct m_tag *
m_tag_get(int type, int length, int wait)
{
	return (m_tag_alloc(MTAG_ABI_COMPAT, type, length, wait));
}

static __inline struct m_tag *
m_tag_find(struct mbuf *m, int type, struct m_tag *start)
{
	return (SLIST_EMPTY(&m->m_pkthdr.tags) ?
	    NULL : m_tag_locate(m, MTAG_ABI_COMPAT, type, start));
}

#endif /* _KERNEL */

#endif /* !_SYS_MBUF_H_ */
