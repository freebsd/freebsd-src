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
 * Macros for type conversion:
 * mtod(m, t)	-- Convert mbuf pointer to data pointer of correct type.
 * dtom(x)	-- Convert data pointer within mbuf to mbuf pointer (XXX).
 * mtocl(x) -	convert pointer within cluster to cluster index #
 * cltom(x) -	convert cluster # to ptr to beginning of cluster
 */
#define	mtod(m, t)	((t)((m)->m_data))
#define	dtom(x)		((struct mbuf *)((intptr_t)(x) & ~(MSIZE-1)))
#define	mtocl(x)	(((uintptr_t)(x) - (uintptr_t)mbutl) >> MCLSHIFT)
#define	cltom(x)	((caddr_t)((uintptr_t)mbutl + \
			    ((uintptr_t)(x) << MCLSHIFT)))

/*
 * Header present at the beginning of every mbuf.
 */
struct m_hdr {
	struct	mbuf *mh_next;		/* next buffer in chain */
	struct	mbuf *mh_nextpkt;	/* next chain in queue/record */
	caddr_t	mh_data;		/* location of data */
	int	mh_len;			/* amount of data in this mbuf */
	short	mh_type;		/* type of data in this mbuf */
	short	mh_flags;		/* flags; see below */
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
	struct	mbuf *aux;		/* extra data buffer; ipsec/others */
};

/*
 * Description of external storage mapped into mbuf; valid only if M_EXT is set.
 */
struct m_ext {
	caddr_t	ext_buf;		/* start of buffer */
	void	(*ext_free)		/* free routine if not the usual */
		    (caddr_t, u_int);
	u_int	ext_size;		/* size of buffer, for ext_free */
	void	(*ext_ref)		/* add a reference to the ext object */
		(caddr_t, u_int);
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
#define	M_PROTO1	0x0008	/* protocol-specific */
#define	M_PROTO2	0x0010	/* protocol-specific */
#define	M_PROTO3	0x0020	/* protocol-specific */
#define	M_PROTO4	0x0040	/* protocol-specific */
#define	M_PROTO5	0x0080	/* protocol-specific */

/*
 * mbuf pkthdr flags (also stored in m_flags).
 */
#define	M_BCAST		0x0100	/* send/received as link-level broadcast */
#define	M_MCAST		0x0200	/* send/received as link-level multicast */
#define	M_FRAG		0x0400	/* packet is a fragment of a larger packet */
#define	M_FIRSTFRAG	0x0800	/* packet is first fragment */
#define	M_LASTFRAG	0x1000	/* packet is last fragment */

/*
 * Flags copied when copying m_pkthdr.
 */
#define	M_COPYFLAGS	(M_PKTHDR|M_EOR|M_PROTO1|M_PROTO1|M_PROTO2|M_PROTO3 | \
			    M_PROTO4|M_PROTO5|M_BCAST|M_MCAST|M_FRAG)

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
#define	MT_TAG		13	/* volatile metadata associated to pkts */
#define	MT_CONTROL	14	/* extra-data protocol message */
#define	MT_OOBDATA	15	/* expedited data  */
#define	MT_NTYPES	16	/* number of mbuf types for mbtypes[] */

/*
 * General mbuf allocator statistics structure.
 */
struct mbstat {
	u_long	m_mbufs;	/* mbufs obtained from page pool */
	u_long	m_clusters;	/* clusters obtained from page pool */
	u_long	m_spare;	/* spare field */
	u_long	m_clfree;	/* free clusters */
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

/*
 * Flags specifying how an allocation should be made.
 */

#define	M_DONTWAIT	1
#define	M_WAIT		0

/* Freelists:
 *
 * Normal mbuf clusters are normally treated as character arrays
 * after allocation, but use the first word of the buffer as a free list
 * pointer while on the free list.
 */
union mcluster {
	union	mcluster *mcl_next;
	char	mcl_buf[MCLBYTES];
};


/*
 * These are identifying numbers passed to the m_mballoc_wait function,
 * allowing us to determine whether the call came from an MGETHDR or
 * an MGET.
 */
#define	MGETHDR_C      1
#define	MGET_C         2

/*
 * Wake up the next instance (if any) of m_mballoc_wait() which is
 * waiting for an mbuf to be freed.  This should be called at splimp().
 *
 * XXX: If there is another free mbuf, this routine will be called [again]
 * from the m_mballoc_wait routine in order to wake another sleep instance.
 */
#define	MMBWAKEUP() do {						\
	if (m_mballoc_wid) {						\
		m_mballoc_wid--;					\
		wakeup_one(&m_mballoc_wid); 				\
	}								\
} while (0)

/*
 * Same as above, but for mbuf cluster(s).
 */
#define	MCLWAKEUP() do {						\
	if (m_clalloc_wid) {						\
		m_clalloc_wid--;					\
		wakeup_one(&m_clalloc_wid);				\
	}								\
} while (0)

/*
 * mbuf utility macros:
 *
 *	MBUFLOCK(code)
 * prevents a section of code from from being interrupted by network
 * drivers.
 */
#define	MBUFLOCK(code) do {						\
	int _ms = splimp();						\
									\
	{ code }							\
	splx(_ms);							\
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
#define	MGET(m, how, type) do {						\
	struct mbuf *_mm;						\
	int _mhow = (how);						\
	int _mtype = (type);						\
	int _ms = splimp();						\
									\
	if (mmbfree == NULL)						\
		(void)m_mballoc(1, _mhow);				\
	_mm = mmbfree;							\
	if (_mm != NULL) {						\
		mmbfree = _mm->m_next;					\
		mbtypes[MT_FREE]--;					\
		_mm->m_type = _mtype;					\
		mbtypes[_mtype]++;					\
		_mm->m_next = NULL;					\
		_mm->m_nextpkt = NULL;					\
		_mm->m_data = _mm->m_dat;				\
		_mm->m_flags = 0;					\
		(m) = _mm;						\
		splx(_ms);						\
	} else {							\
		splx(_ms);						\
		_mm = m_retry(_mhow, _mtype);				\
		if (_mm == NULL && _mhow == M_WAIT)			\
			(m) = m_mballoc_wait(MGET_C, _mtype);		\
		else							\
			(m) = _mm;					\
	}								\
} while (0)

#define	MGETHDR(m, how, type) do {					\
	struct mbuf *_mm;						\
	int _mhow = (how);						\
	int _mtype = (type);						\
	int _ms = splimp();						\
									\
	if (mmbfree == NULL)						\
		(void)m_mballoc(1, _mhow);				\
	_mm = mmbfree;							\
	if (_mm != NULL) {						\
		mmbfree = _mm->m_next;					\
		mbtypes[MT_FREE]--;					\
		_mm->m_type = _mtype;					\
		mbtypes[_mtype]++;					\
		_mm->m_next = NULL;					\
		_mm->m_nextpkt = NULL;					\
		_mm->m_data = _mm->m_pktdat;				\
		_mm->m_flags = M_PKTHDR;				\
		_mm->m_pkthdr.rcvif = NULL;				\
		_mm->m_pkthdr.csum_flags = 0;				\
		_mm->m_pkthdr.aux = (struct mbuf *)NULL;		\
		(m) = _mm;						\
		splx(_ms);						\
	} else {							\
		splx(_ms);						\
		_mm = m_retryhdr(_mhow, _mtype);			\
		if (_mm == NULL && _mhow == M_WAIT)			\
			(m) = m_mballoc_wait(MGETHDR_C, _mtype);	\
		else							\
			(m) = _mm;					\
	}								\
} while (0)

/*
 * Mbuf cluster macros.
 * MCLALLOC(caddr_t p, int how) allocates an mbuf cluster.
 * MCLGET adds such clusters to a normal mbuf;
 * the flag M_EXT is set upon success.
 * MCLFREE releases a reference to a cluster allocated by MCLALLOC,
 * freeing the cluster if the reference count has reached 0.
 */
#define	MCLALLOC(p, how) do {						\
	caddr_t _mp;							\
	int _mhow = (how);						\
	int _ms = splimp();						\
									\
	if (mclfree == NULL)						\
		(void)m_clalloc(1, _mhow);				\
	_mp = (caddr_t)mclfree;						\
	if (_mp != NULL) {						\
		mclrefcnt[mtocl(_mp)]++;				\
		mbstat.m_clfree--;					\
		mclfree = ((union mcluster *)_mp)->mcl_next;		\
		(p) = _mp;						\
		splx(_ms);						\
	} else {							\
		splx(_ms);						\
		if (_mhow == M_WAIT)					\
			(p) = m_clalloc_wait();				\
		else							\
			(p) = NULL;					\
	}								\
} while (0)	

#define	MCLGET(m, how) do {						\
	struct mbuf *_mm = (m);						\
									\
	MCLALLOC(_mm->m_ext.ext_buf, (how));				\
	if (_mm->m_ext.ext_buf != NULL) {				\
		_mm->m_data = _mm->m_ext.ext_buf;			\
		_mm->m_flags |= M_EXT;					\
		_mm->m_ext.ext_free = NULL;				\
		_mm->m_ext.ext_ref = NULL;				\
		_mm->m_ext.ext_size = MCLBYTES;				\
	}								\
} while (0)

#define	MCLFREE1(p) do {						\
	union mcluster *_mp = (union mcluster *)(p);			\
									\
	KASSERT(mclrefcnt[mtocl(_mp)] > 0, ("freeing free cluster"));	\
	if (--mclrefcnt[mtocl(_mp)] == 0) {				\
		_mp->mcl_next = mclfree;				\
		mclfree = _mp;						\
		mbstat.m_clfree++;					\
		MCLWAKEUP();						\
	}								\
} while (0)

#define	MCLFREE(p) MBUFLOCK(						\
	MCLFREE1(p);							\
)

#define	MEXTFREE1(m) do {						\
		struct mbuf *_mm = (m);					\
									\
		if (_mm->m_ext.ext_free != NULL)			\
			(*_mm->m_ext.ext_free)(_mm->m_ext.ext_buf,	\
		    	    _mm->m_ext.ext_size);			\
		else							\
			MCLFREE1(_mm->m_ext.ext_buf);			\
} while (0)

#define	MEXTFREE(m) MBUFLOCK(						\
	MEXTFREE1(m);							\
)

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
	_mfrom->m_pkthdr.aux = (struct mbuf *)NULL;			\
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
 * Check if we can write to an mbuf.
 */
#define M_EXT_WRITABLE(m)	\
    ((m)->m_ext.ext_free == NULL && mclrefcnt[mtocl((m)->m_ext.ext_buf)] == 1)

#define M_WRITABLE(m) (!((m)->m_flags & M_EXT) || \
    M_EXT_WRITABLE(m) )

/*
 * Compute the amount of space available
 * before the current start of data in an mbuf.
 *
 * The M_WRITABLE() is a temporary, conservative safety measure: the burden
 * of checking writability of the mbuf data area rests solely with the caller.
 */
#define	M_LEADINGSPACE(m)						\
	((m)->m_flags & M_EXT ?						\
	    (M_EXT_WRITABLE(m) ? (m)->m_data - (m)->m_ext.ext_buf : 0):	\
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
 * If how is M_DONTWAIT and allocation fails, the original mbuf chain
 * is freed and m is set to NULL.
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

/* change mbuf to new type */
#define	MCHTYPE(m, t) do {						\
	struct mbuf *_mm = (m);						\
	int _mt = (t);							\
	int _ms = splimp();						\
									\
	mbtypes[_mm->m_type]--;						\
	mbtypes[_mt]++;							\
	splx(_ms);							\
	_mm->m_type = (_mt);						\
} while (0)

/* Length to m_copy to copy all. */
#define	M_COPYALL	1000000000

/* Compatibility with 4.3 */
#define	m_copy(m, o, l)	m_copym((m), (o), (l), M_DONTWAIT)

/*
 * pkthdr.aux type tags.
 */
struct mauxtag {
	int	af;
	int	type;
	void	*p;
};

/*
 * Some packet tags to identify different mbuf annotations.
 *
 * Eventually, these annotations will end up in an appropriate chain
 * (struct m_tag or similar, e.g. as in NetBSD) properly managed by
 * the mbuf handling routines.
 *
 * As a temporary and low impact solution to replace the even uglier
 * approach used so far in some parts of the network stack (which relies
 * on global variables), these annotations are stored in MT_TAG
 * mbufs (or lookalikes) prepended to the actual mbuf chain.
 *
 *	m_type	= MT_TAG
 *	m_flags	= m_tag_id
 *	m_next	= next buffer in chain.
 *
 * BE VERY CAREFUL not to pass these blocks to the mbuf handling routines.
 *
 */

#define	m_tag_id	m_hdr.mh_flags

/* Packet tag types -- first ones are from NetBSD */

#define	PACKET_TAG_NONE				0  /* Nadda */
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

/* Packet tags used in the FreeBSD network stack */
#define	PACKET_TAG_DUMMYNET			15 /* dummynet info */
#define	PACKET_TAG_IPFW				16 /* ipfw classification */
#define	PACKET_TAG_DIVERT			17 /* divert info */
#define	PACKET_TAG_IPFORWARD			18 /* ipforward info */

#define	PACKET_TAG_MAX				19

#ifdef _KERNEL
extern	u_int		 m_clalloc_wid;	/* mbuf cluster wait count */
extern	u_int		 m_mballoc_wid;	/* mbuf wait count */
extern	int		 max_linkhdr;	/* largest link-level header */
extern	int		 max_protohdr;	/* largest protocol header */
extern	int		 max_hdr;	/* largest link+protocol header */
extern	int		 max_datalen;	/* MHLEN - max_hdr */
extern	struct mbstat	 mbstat;
extern	u_long		 mbtypes[MT_NTYPES]; /* per-type mbuf allocations */
extern	int		 mbuf_wait;	/* mbuf sleep time */
extern	struct mbuf	*mbutl;		/* virtual address of mclusters */
extern	char		*mclrefcnt;	/* cluster reference counts */
extern	union mcluster	*mclfree;
extern	struct mbuf	*mmbfree;
extern	int		 nmbclusters;
extern	int		 nmbufs;
extern	int		 nsfbufs;

void		 m_adj(struct mbuf *, int);
struct	mbuf	*m_aux_add(struct mbuf *, int, int);
struct	mbuf	*m_aux_add2(struct mbuf *, int, int, void *);
void		 m_aux_delete(struct mbuf *, struct mbuf *);
struct	mbuf	*m_aux_find(struct mbuf *, int, int);
struct	mbuf	*m_aux_find2(struct mbuf *, int, int, void *);
void		 m_cat(struct mbuf *, struct mbuf *);
int		 m_clalloc(int, int);
caddr_t		 m_clalloc_wait(void);
void		 m_copyback(struct mbuf *, int, int, caddr_t);
void		 m_copydata(struct mbuf *, int, int, caddr_t);
struct	mbuf	*m_copym(struct mbuf *, int, int, int);
struct	mbuf	*m_copypacket(struct mbuf *, int);
struct	mbuf	*m_devget(char *, int, int, struct ifnet *,
		    void (*copy)(char *, caddr_t, u_int));
struct	mbuf	*m_dup(struct mbuf *, int);
struct	mbuf	*m_free(struct mbuf *);
void		 m_freem(struct mbuf *);
struct	mbuf	*m_get(int, int);
struct  mbuf	*m_getcl(int how, short type, int flags);
struct	mbuf	*m_getclr(int, int);
struct	mbuf	*m_gethdr(int, int);
struct	mbuf	*m_getm(struct mbuf *, int, int, int);
int		 m_mballoc(int, int);
struct	mbuf	*m_mballoc_wait(int, int);
struct	mbuf	*m_prepend(struct mbuf *, int, int);
void		 m_print(const struct mbuf *m);
struct	mbuf	*m_pulldown(struct mbuf *, int, int, int *);
struct	mbuf	*m_pullup(struct mbuf *, int);
struct	mbuf	*m_retry(int, int);
struct	mbuf	*m_retryhdr(int, int);
struct	mbuf	*m_split(struct mbuf *, int, int);
#endif /* _KERNEL */

#endif /* !_SYS_MBUF_H_ */
