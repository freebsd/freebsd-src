/*
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
 *	@(#)mbuf.h	8.5 (Berkeley) 2/19/95
 * $FreeBSD$
 */

#ifndef _SYS_MBUF_H_
#define	_SYS_MBUF_H_

#ifdef _KERNEL
#include <sys/condvar.h>	/* XXX */
#include <sys/_lock.h>
#include <sys/_mutex.h>
#endif /* _KERNEL */

/*
 * Mbufs are of a single size, MSIZE (machine/param.h), which
 * includes overhead.  An mbuf may add a single "mbuf cluster" of size
 * MCLBYTES (also in machine/param.h), which has no additional overhead
 * and is used instead of the internal data area; this is done when
 * at least MINCLSIZE of data must be stored.
 */

#define	MLEN		(MSIZE - sizeof(struct m_hdr))	/* normal data len */
#define	MHLEN		(MLEN - sizeof(struct pkthdr))	/* data len w/pkthdr */

#define	MINCLSIZE	(MHLEN + 1)	/* smallest amount to put in cluster */
#define	M_MAXCOMPRESS	(MHLEN / 2)	/* max amount to copy for compression */

/*
 * Maximum number of allocatable counters for external buffers. This
 * ensures enough VM address space for the allocation of counters
 * in the extreme case where all possible external buffers are allocated.
 *
 * Note: When new types of external storage are allocated, EXT_COUNTERS
 * 	 must be tuned accordingly. Practically, this isn't a big deal
 *	 as each counter is only a word long, so we can fit
 *	 (PAGE_SIZE / length of word) counters in a single page.
 *
 * XXX: Must increase this if using any of if_ti, if_wb, if_sk drivers,
 *	or any other drivers which may manage their own buffers and
 *	eventually attach them to mbufs. 
 */
#define EXT_COUNTERS (nmbclusters + nsfbufs)

#ifdef _KERNEL
/*
 * Macros for type conversion
 * mtod(m, t)	- convert mbuf pointer to data pointer of correct type
 * dtom(x)	- convert data pointer within mbuf to mbuf pointer (XXX)
 */
#define	mtod(m, t)	((t)((m)->m_data))
#define	dtom(x)		((struct mbuf *)((intptr_t)(x) & ~(MSIZE-1)))
#endif /* _KERNEL */

/* header at beginning of each mbuf: */
struct m_hdr {
	struct	mbuf *mh_next;		/* next buffer in chain */
	struct	mbuf *mh_nextpkt;	/* next chain in queue/record */
	caddr_t	mh_data;		/* location of data */
	int	mh_len;			/* amount of data in this mbuf */
	short	mh_type;		/* type of data in this mbuf */
	short	mh_flags;		/* flags; see below */
};

/* record/packet header in first mbuf of chain; valid if M_PKTHDR set */
struct pkthdr {
	struct	ifnet *rcvif;		/* rcv interface */
	int	len;			/* total packet length */
	/* variables for ip and tcp reassembly */
	void	*header;		/* pointer to packet header */
	/* variables for hardware checksum */
	int	csum_flags;		/* flags regarding checksum */
	int	csum_data;		/* data field used by csum routines */
	struct	mbuf *aux;		/* extra data buffer; ipsec/others */
};

/* description of external storage mapped into mbuf, valid if M_EXT set */
struct m_ext {
	caddr_t	ext_buf;		/* start of buffer */
	void	(*ext_free)		/* free routine if not the usual */
		    (caddr_t, void *);
	void	*ext_args;		/* optional argument pointer */
	u_int	ext_size;		/* size of buffer, for ext_free */
	union	mext_refcnt *ref_cnt;	/* pointer to ref count info */
	int	ext_type;		/* type of external storage */
};

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

/* mbuf flags */
#define	M_EXT		0x0001	/* has associated external storage */
#define	M_PKTHDR	0x0002	/* start of record */
#define	M_EOR		0x0004	/* end of record */
#define M_RDONLY	0x0008	/* associated data is marked read-only */
#define	M_PROTO1	0x0010	/* protocol-specific */
#define	M_PROTO2	0x0020	/* protocol-specific */
#define	M_PROTO3	0x0040	/* protocol-specific */
#define	M_PROTO4	0x0080	/* protocol-specific */
#define	M_PROTO5	0x0100	/* protocol-specific */

/* mbuf pkthdr flags, also in m_flags */
#define	M_BCAST		0x0200	/* send/received as link-level broadcast */
#define	M_MCAST		0x0400	/* send/received as link-level multicast */
#define	M_FRAG		0x0800	/* packet is a fragment of a larger packet */
#define	M_FIRSTFRAG	0x1000	/* packet is first fragment */
#define	M_LASTFRAG	0x2000	/* packet is last fragment */

/* external buffer types: identify ext_buf type */
#define	EXT_CLUSTER	1	/* mbuf cluster */
#define	EXT_SFBUF	2	/* sendfile(2)'s sf_bufs */
#define	EXT_NET_DRV	100	/* custom ext_buf provided by net driver(s) */
#define	EXT_MOD_TYPE	200	/* custom module's ext_buf type */

/* flags copied when copying m_pkthdr */
#define	M_COPYFLAGS	(M_PKTHDR|M_EOR|M_PROTO1|M_PROTO1|M_PROTO2|M_PROTO3 | \
			    M_PROTO4|M_PROTO5|M_BCAST|M_MCAST|M_FRAG|M_RDONLY)

/* flags indicating hw checksum support and sw checksum requirements */
#define CSUM_IP			0x0001		/* will csum IP */
#define CSUM_TCP		0x0002		/* will csum TCP */
#define CSUM_UDP		0x0004		/* will csum UDP */
#define CSUM_IP_FRAGS		0x0008		/* will csum IP fragments */
#define CSUM_FRAGMENT		0x0010		/* will do IP fragmentation */

#define CSUM_IP_CHECKED		0x0100		/* did csum IP */
#define CSUM_IP_VALID		0x0200		/*   ... the csum is valid */
#define CSUM_DATA_VALID		0x0400		/* csum_data field is valid */
#define CSUM_PSEUDO_HDR		0x0800		/* csum_data has pseudo hdr */

#define CSUM_DELAY_DATA		(CSUM_TCP | CSUM_UDP)
#define CSUM_DELAY_IP		(CSUM_IP)	/* XXX add ipv6 here too? */

/* mbuf types */
#define	MT_FREE		0	/* should be on free list */
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
 * mbuf statistics
 */
struct mbstat {
	u_long	m_mbufs;	/* # mbufs obtained from page pool */
	u_long	m_clusters;	/* # clusters obtained from page pool */
	u_long	m_clfree;	/* # clusters on freelist (cache) */
	u_long	m_refcnt;	/* # ref counters obtained from page pool */
	u_long	m_refree;	/* # ref counters on freelist (cache) */
	u_long	m_spare;	/* spare field */
	u_long	m_drops;	/* times failed to find space */
	u_long	m_wait;		/* times waited for space */
	u_long	m_drain;	/* times drained protocols for space */
	u_long	m_mcfail;	/* times m_copym failed */
	u_long	m_mpfail;	/* times m_pullup failed */
	u_long	m_msize;	/* length of an mbuf */
	u_long	m_mclbytes;	/* length of an mbuf cluster */
	u_long	m_minclsize;	/* min length of data to allocate a cluster */
	u_long	m_mlen;		/* length of data in an mbuf */
	u_long	m_mhlen;	/* length of data in a header mbuf */
};

/* flags to m_get/MGET */
#define	M_DONTWAIT	1
#define	M_TRYWAIT	0
#define	M_WAIT		M_TRYWAIT	/* XXX: Deprecated. */

/*
 * Normal mbuf clusters are normally treated as character arrays
 * after allocation, but use the first word of the buffer as a free list
 * pointer while on the free list.
 */
union mcluster {
	union	mcluster *mcl_next;
	char	mcl_buf[MCLBYTES];
};

/*
 * The m_ext object reference counter structure.
 */
union mext_refcnt {
	union	mext_refcnt *next_ref;
	u_int	refcnt;
};

#ifdef _KERNEL
/*
 * The freelists for mbufs and mbuf clusters include condition variables
 * that are used in cases of depletion/starvation.
 * The counter freelist does not require a condition variable as we never
 * expect to consume more than the reserved address space for counters.
 * All are presently protected by the mbuf_mtx lock.
 */
struct mbffree_lst {
	struct	mbuf *m_head;
	struct	cv m_starved;
};

struct mclfree_lst {
	union	mcluster *m_head;
	struct	cv m_starved;
};
  
struct mcntfree_lst {
	union	mext_refcnt *m_head;
};

/*
 * Signal a single instance (if any) blocked on a m_starved cv (i.e. an
 * instance waiting for an {mbuf, cluster} to be freed to the global
 * cache lists).
 *
 * Must be called with mbuf_mtx held.
 */
#define	MBWAKEUP(m_wid, m_cv) do {					\
	if ((m_wid) > 0)						\
		cv_signal((m_cv));					\
} while (0)

/*
 * mbuf external reference count management macros:
 *
 * MEXT_IS_REF(m): true if (m) is not the only mbuf referencing
 *     the external buffer ext_buf
 * MEXT_REM_REF(m): remove reference to m_ext object
 * MEXT_ADD_REF(m): add reference to m_ext object already
 *     referred to by (m)
 * MEXT_INIT_REF(m): allocate and initialize an external
 *     object reference counter for (m)
 */
#define MEXT_IS_REF(m) ((m)->m_ext.ref_cnt->refcnt > 1)

#define MEXT_REM_REF(m) do {						\
	KASSERT((m)->m_ext.ref_cnt->refcnt > 0, ("m_ext refcnt < 0"));	\
	atomic_subtract_int(&((m)->m_ext.ref_cnt->refcnt), 1);		\
} while(0)

#define MEXT_ADD_REF(m) atomic_add_int(&((m)->m_ext.ref_cnt->refcnt), 1)

#define _MEXT_ALLOC_CNT(m_cnt, how) do {				\
	union mext_refcnt *__mcnt;					\
									\
	mtx_lock(&mbuf_mtx);						\
	if (mcntfree.m_head == NULL)					\
		m_alloc_ref(1, (how));					\
	__mcnt = mcntfree.m_head;					\
	if (__mcnt != NULL) {						\
		mcntfree.m_head = __mcnt->next_ref;			\
		mbstat.m_refree--;					\
		__mcnt->refcnt = 0;					\
	}								\
	mtx_unlock(&mbuf_mtx);						\
	(m_cnt) = __mcnt;						\
} while (0)

#define _MEXT_DEALLOC_CNT(m_cnt) do {					\
	union mext_refcnt *__mcnt = (m_cnt);				\
									\
	mtx_lock(&mbuf_mtx);						\
	__mcnt->next_ref = mcntfree.m_head;				\
	mcntfree.m_head = __mcnt;					\
	mbstat.m_refree++;						\
	mtx_unlock(&mbuf_mtx);						\
} while (0)

#define MEXT_INIT_REF(m, how) do {					\
	struct mbuf *__mmm = (m);					\
									\
	_MEXT_ALLOC_CNT(__mmm->m_ext.ref_cnt, (how));			\
	if (__mmm->m_ext.ref_cnt != NULL)				\
		MEXT_ADD_REF(__mmm);					\
} while (0)

/*
 * mbuf allocation/deallocation macros:
 *
 *	MGET(struct mbuf *m, int how, int type)
 * allocates an mbuf and initializes it to contain internal data.
 *
 *	MGETHDR(struct mbuf *m, int how, int type)
 * allocates an mbuf and initializes it to contain a packet header
 * and internal data.
 */
/*
 * Lower-level macros for MGET(HDR)... Not to be used outside the
 * subsystem ("non-exportable" macro names are prepended with "_").
 */
#define _MGET_SETUP(m_set, m_set_type) do {				\
	(m_set)->m_type = (m_set_type);					\
	(m_set)->m_next = NULL;						\
	(m_set)->m_nextpkt = NULL;					\
	(m_set)->m_data = (m_set)->m_dat;				\
	(m_set)->m_flags = 0;						\
} while (0)

#define	_MGET(m_mget, m_get_how) do {					\
	if (mmbfree.m_head == NULL)					\
		m_mballoc(1, (m_get_how));				\
	(m_mget) = mmbfree.m_head;					\
	if ((m_mget) != NULL) {						\
		mmbfree.m_head = (m_mget)->m_next;			\
		mbtypes[MT_FREE]--;					\
	} else {							\
		if ((m_get_how) == M_TRYWAIT)				\
			(m_mget) = m_mballoc_wait();			\
	}								\
} while (0)

#define MGET(m, how, type) do {						\
	struct mbuf *_mm;						\
	int _mhow = (how);						\
	int _mtype = (type);						\
									\
	mtx_lock(&mbuf_mtx);						\
	_MGET(_mm, _mhow);						\
	if (_mm != NULL) {						\
		mbtypes[_mtype]++;					\
		mtx_unlock(&mbuf_mtx);					\
		_MGET_SETUP(_mm, _mtype);				\
	} else {							\
		mbstat.m_drops++;					\
		mtx_unlock(&mbuf_mtx);					\
	}								\
	(m) = _mm;							\
} while (0)

#define _MGETHDR_SETUP(m_set, m_set_type) do {				\
	(m_set)->m_type = (m_set_type);					\
	(m_set)->m_next = NULL;						\
	(m_set)->m_nextpkt = NULL;					\
	(m_set)->m_data = (m_set)->m_pktdat;				\
	(m_set)->m_flags = M_PKTHDR;					\
	(m_set)->m_pkthdr.rcvif = NULL;					\
	(m_set)->m_pkthdr.csum_flags = 0;				\
	(m_set)->m_pkthdr.aux = NULL;					\
} while (0)

#define MGETHDR(m, how, type) do {					\
	struct mbuf *_mm;						\
	int _mhow = (how);						\
	int _mtype = (type);						\
									\
	mtx_lock(&mbuf_mtx);						\
	_MGET(_mm, _mhow);						\
	if (_mm != NULL) {						\
		mbtypes[_mtype]++;					\
		mtx_unlock(&mbuf_mtx);					\
		_MGETHDR_SETUP(_mm, _mtype);				\
	} else {							\
		mbstat.m_drops++;					\
		mtx_unlock(&mbuf_mtx);					\
	}								\
	(m) = _mm;							\
} while (0)

/*
 * mbuf external storage macros:
 *
 *   MCLGET allocates and refers an mcluster to an mbuf
 *   MEXTADD sets up pre-allocated external storage and refers to mbuf
 *   MEXTFREE removes reference to external object and frees it if
 *       necessary
 */
#define	_MCLALLOC(p, how) do {						\
	caddr_t _mp;							\
	int _mhow = (how);						\
									\
	if (mclfree.m_head == NULL)					\
		m_clalloc(1, _mhow);					\
	_mp = (caddr_t)mclfree.m_head;					\
	if (_mp != NULL) {						\
		mbstat.m_clfree--;					\
		mclfree.m_head = ((union mcluster *)_mp)->mcl_next;	\
	} else {							\
		if (_mhow == M_TRYWAIT)					\
			_mp = m_clalloc_wait();				\
	}								\
	(p) = _mp;							\
} while (0)

#define	MCLGET(m, how) do {						\
	struct mbuf *_mm = (m);						\
									\
	mtx_lock(&mbuf_mtx);						\
	_MCLALLOC(_mm->m_ext.ext_buf, (how));				\
	if (_mm->m_ext.ext_buf != NULL) {				\
		mtx_unlock(&mbuf_mtx);					\
		MEXT_INIT_REF(_mm, (how));				\
		if (_mm->m_ext.ref_cnt == NULL) {			\
			_MCLFREE(_mm->m_ext.ext_buf);			\
			_mm->m_ext.ext_buf = NULL;			\
		} else {						\
			_mm->m_data = _mm->m_ext.ext_buf;		\
			_mm->m_flags |= M_EXT;				\
			_mm->m_ext.ext_free = NULL;			\
			_mm->m_ext.ext_args = NULL;			\
			_mm->m_ext.ext_size = MCLBYTES;			\
			_mm->m_ext.ext_type = EXT_CLUSTER;		\
		}							\
	} else {							\
		mbstat.m_drops++;					\
		mtx_unlock(&mbuf_mtx);					\
	}								\
} while (0)

#define MEXTADD(m, buf, size, free, args, flags, type) do {		\
	struct mbuf *_mm = (m);						\
									\
	MEXT_INIT_REF(_mm, M_TRYWAIT);					\
	if (_mm->m_ext.ref_cnt != NULL) {				\
		_mm->m_flags |= (M_EXT | (flags));			\
		_mm->m_ext.ext_buf = (caddr_t)(buf);			\
		_mm->m_data = _mm->m_ext.ext_buf;			\
		_mm->m_ext.ext_size = (size);				\
		_mm->m_ext.ext_free = (free);				\
		_mm->m_ext.ext_args = (args);				\
		_mm->m_ext.ext_type = (type);				\
	}								\
} while (0)

#define	_MCLFREE(p) do {						\
	union mcluster *_mp = (union mcluster *)(p);			\
									\
	mtx_lock(&mbuf_mtx);						\
	_mp->mcl_next = mclfree.m_head;					\
	mclfree.m_head = _mp;						\
	mbstat.m_clfree++;						\
	MBWAKEUP(m_clalloc_wid, &mclfree.m_starved);			\
	mtx_unlock(&mbuf_mtx); 						\
} while (0)

/* MEXTFREE:
 * If the atomic_cmpset_int() returns 0, then we effectively do nothing
 * in terms of "cleaning up" (freeing the ext buf and ref. counter) as
 * this means that either there are still references, or another thread
 * is taking care of the clean-up.
 */
#define	MEXTFREE(m) do {						\
	struct mbuf *_mmm = (m);					\
									\
	MEXT_REM_REF(_mmm);						\
	if (atomic_cmpset_int(&_mmm->m_ext.ref_cnt->refcnt, 0, 1)) {	\
		if (_mmm->m_ext.ext_type != EXT_CLUSTER) {		\
			(*(_mmm->m_ext.ext_free))(_mmm->m_ext.ext_buf,	\
			    _mmm->m_ext.ext_args);			\
		} else							\
			_MCLFREE(_mmm->m_ext.ext_buf);			\
		_MEXT_DEALLOC_CNT(_mmm->m_ext.ref_cnt);			\
	}								\
	_mmm->m_flags &= ~M_EXT;					\
} while (0)

/*
 * MFREE(struct mbuf *m, struct mbuf *n)
 * Free a single mbuf and associated external storage.
 * Place the successor, if any, in n.
 */
#define	MFREE(m, n) do {						\
	struct mbuf *_mm = (m);						\
									\
	KASSERT(_mm->m_type != MT_FREE, ("freeing free mbuf"));		\
	if (_mm->m_flags & M_EXT)					\
		MEXTFREE(_mm);						\
	mtx_lock(&mbuf_mtx);						\
	mbtypes[_mm->m_type]--;						\
	_mm->m_type = MT_FREE;						\
	mbtypes[MT_FREE]++;						\
	(n) = _mm->m_next;						\
	_mm->m_next = mmbfree.m_head;					\
	mmbfree.m_head = _mm;						\
	MBWAKEUP(m_mballoc_wid, &mmbfree.m_starved);			\
	mtx_unlock(&mbuf_mtx); 						\
} while (0)

/*
 * M_WRITABLE(m)
 * Evaluate TRUE if it's safe to write to the mbuf m's data region (this
 * can be both the local data payload, or an external buffer area,
 * depending on whether M_EXT is set).
 */
#define M_WRITABLE(m)	(!((m)->m_flags & M_RDONLY) && (!((m)->m_flags  \
			    & M_EXT) || !MEXT_IS_REF(m)))

/*
 * Copy mbuf pkthdr from "from" to "to".
 * from must have M_PKTHDR set, and to must be empty.
 * aux pointer will be moved to `to'.
 */
#define	M_COPY_PKTHDR(to, from) do {					\
	struct mbuf *_mfrom = (from);					\
	struct mbuf *_mto = (to);					\
									\
	_mto->m_data = _mto->m_pktdat;					\
	_mto->m_flags = _mfrom->m_flags & M_COPYFLAGS;			\
	_mto->m_pkthdr = _mfrom->m_pkthdr;				\
	_mfrom->m_pkthdr.aux = NULL;					\
} while (0)

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
 */
#define	M_LEADINGSPACE(m)						\
	((m)->m_flags & M_EXT ?						\
	    /* (m)->m_data - (m)->m_ext.ext_buf */ 0 :			\
	    (m)->m_flags & M_PKTHDR ? (m)->m_data - (m)->m_pktdat :	\
	    (m)->m_data - (m)->m_dat)

/*
 * Compute the amount of space available
 * after the end of data in an mbuf.
 */
#define	M_TRAILINGSPACE(m)						\
	((m)->m_flags & M_EXT ? (m)->m_ext.ext_buf +			\
	    (m)->m_ext.ext_size - ((m)->m_data + (m)->m_len) :		\
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
 * change mbuf to new type
 */
#define	MCHTYPE(m, t) do {						\
	struct mbuf *_mm = (m);						\
	int _mt = (t);							\
									\
	mtx_lock(&mbuf_mtx);						\
	mbtypes[_mm->m_type]--;						\
	mbtypes[_mt]++;							\
	mtx_unlock(&mbuf_mtx);						\
	_mm->m_type = (_mt);						\
} while (0)

/* length to m_copy to copy all */
#define	M_COPYALL	1000000000

/* compatibility with 4.3 */
#define	m_copy(m, o, l)	m_copym((m), (o), (l), M_DONTWAIT)

/*
 * pkthdr.aux type tags.
 */
struct mauxtag {
	int	af;
	int	type;
};

extern	u_long		 m_clalloc_wid;	/* mbuf cluster wait count */
extern	u_long		 m_mballoc_wid;	/* mbuf wait count */
extern	int		 max_datalen;	/* MHLEN - max_hdr */
extern	int		 max_hdr;	/* largest link+protocol header */
extern	int		 max_linkhdr;	/* largest link-level header */
extern	int		 max_protohdr;	/* largest protocol header */
extern	struct mbstat	 mbstat;
extern	u_long		 mbtypes[MT_NTYPES]; /* per-type mbuf allocations */
extern	int		 mbuf_wait;	/* mbuf sleep time */
extern	struct mtx	 mbuf_mtx;
extern	struct mbuf	*mbutl;		/* virtual address of mclusters */
extern	struct mclfree_lst	mclfree;
extern	struct mcntfree_lst	mcntfree;
extern	struct mbffree_lst	mmbfree;
extern	int		 nmbclusters;
extern	int		 nmbcnt;
extern	int		 nmbufs;
extern	int		 nsfbufs;

void	m_adj(struct mbuf *, int);
int	m_alloc_ref(u_int, int);
struct	mbuf *m_aux_add(struct mbuf *, int, int);
void	m_aux_delete(struct mbuf *, struct mbuf *);
struct	mbuf *m_aux_find(struct mbuf *, int, int);
void	m_cat(struct mbuf *, struct mbuf *);
int	m_clalloc(int, int);
caddr_t	m_clalloc_wait(void);
void	m_copyback(struct mbuf *, int, int, caddr_t);
void	m_copydata(struct mbuf *, int, int, caddr_t);
struct	mbuf *m_copym(struct mbuf *, int, int, int);
struct	mbuf *m_copypacket(struct mbuf *, int);
struct	mbuf *m_devget(char *, int, int, struct ifnet *,
    void (*copy)(char *, caddr_t, u_int));
struct	mbuf *m_dup(struct mbuf *, int);
struct	mbuf *m_free(struct mbuf *);
void	m_freem(struct mbuf *);
struct	mbuf *m_get(int, int);
struct	mbuf *m_getclr(int, int);
struct	mbuf *m_gethdr(int, int);
struct	mbuf *m_getm(struct mbuf *, int, int, int);
int	m_mballoc(int, int);
struct	mbuf *m_mballoc_wait(void);
struct	mbuf *m_prepend(struct mbuf *, int, int);
void	m_print(const struct mbuf *m);
struct	mbuf *m_pulldown(struct mbuf *, int, int, int *);
struct	mbuf *m_pullup(struct mbuf *, int);
struct	mbuf *m_split(struct mbuf *, int, int);
#endif /* _KERNEL */

#endif /* !_SYS_MBUF_H_ */
