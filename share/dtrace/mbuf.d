/*
 * Copyright (c) 2016 George V. Neville-Neil
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 * $FreeBSD$
 *
 * Translators and flags for the mbuf structure.  FreeBSD specific code.
 *
 */

#pragma D depends_on module kernel

/*
 * mbuf flags of global significance and layer crossing.
 * Those of only protocol/layer specific significance are to be mapped
 * to M_PROTO[1-12] and cleared at layer handoff boundaries.
 * NB: Limited to the lower 24 bits.
 */

#pragma D binding "1.6.3" M_EXT
inline int M_EXT =	0x00000001; /* has associated external storage */
#pragma D binding "1.6.3" M_PKTHDR
inline int M_PKTHDR =	0x00000002; /* start of record */
#pragma D binding "1.6.3" M_EOR
inline int M_EOR =	0x00000004; /* end of record */
#pragma D binding "1.6.3" M_RDONLY
inline int M_RDONLY =	0x00000008; /* associated data is marked read-only */
#pragma D binding "1.6.3" M_BCAST
inline int M_BCAST =	0x00000010; /* send/received as link-level broadcast */
#pragma D binding "1.6.3" M_MCAST
inline int M_MCAST =	0x00000020; /* send/received as link-level multicast */
#pragma D binding "1.6.3" M_PROMISC
inline int M_PROMISC =	0x00000040; /* packet was not for us */
#pragma D binding "1.6.3" M_VLANTAG
inline int M_VLANTAG =	0x00000080; /* ether_vtag is valid */
#pragma D binding "1.6.3" M_UNUSED_8
inline int M_UNUSED_8 =	0x00000100; /* --available-- */
#pragma D binding "1.6.3" M_NOFREE
inline int M_NOFREE =	0x00000200; /* do not free mbuf, embedded in cluster */

#pragma D binding "1.6.3" M_PROTO1
inline int M_PROTO1 =	0x00001000; /* protocol-specific */
#pragma D binding "1.6.3" M_PROTO2
inline int M_PROTO2 =	0x00002000; /* protocol-specific */
#pragma D binding "1.6.3" M_PROTO3
inline int M_PROTO3 =	0x00004000; /* protocol-specific */
#pragma D binding "1.6.3" M_PROTO4
inline int M_PROTO4 =	0x00008000; /* protocol-specific */
#pragma D binding "1.6.3" M_PROTO5
inline int M_PROTO5 =	0x00010000; /* protocol-specific */
#pragma D binding "1.6.3" M_PROTO6
inline int M_PROTO6 =	0x00020000; /* protocol-specific */
#pragma D binding "1.6.3" M_PROTO7
inline int M_PROTO7 =	0x00040000; /* protocol-specific */
#pragma D binding "1.6.3" M_PROTO8
inline int M_PROTO8 =	0x00080000; /* protocol-specific */
#pragma D binding "1.6.3" M_PROTO9
inline int M_PROTO9 =	0x00100000; /* protocol-specific */
#pragma D binding "1.6.3" M_PROTO10
inline int M_PROTO10 =	0x00200000; /* protocol-specific */
#pragma D binding "1.6.3" M_PROTO11
inline int M_PROTO11 =	0x00400000; /* protocol-specific */
#pragma D binding "1.6.3" M_PROTO12
inline int M_PROTO12 =	0x00800000; /* protocol-specific */

#pragma D binding "1.6.3" mbufflags_string
inline string mbufflags_string[uint32_t flags] =
    flags & M_EXT ? "M_EXT" :
    flags & M_PKTHDR ? "M_PKTHDR" :
    flags & M_EOR  ? "M_EOR" :
    flags & M_RDONLY  ? "M_RDONLY" :
    flags & M_BCAST  ? "M_BCAST" :
    flags & M_MCAST 	? "M_MCAST" :
    flags & M_PROMISC 	? "M_PROMISC" :
    flags & M_VLANTAG 	? "M_VLANTAG" :
    flags & M_UNUSED_8 	? "M_UNUSED_8" :
    flags & M_NOFREE  ? "M_NOFREE" :
    flags & M_PROTO1  ? "M_PROTO1" :
    flags & M_PROTO2 ? "M_PROTO2" :
    flags & M_PROTO3 ? "M_PROTO3" :
    flags & M_PROTO4 ? "M_PROTO4" :
    flags & M_PROTO5 ? "M_PROTO5" :
    flags & M_PROTO6 ? "M_PROTO6" :
    flags & M_PROTO7 ? "M_PROTO7" :
    flags & M_PROTO8 ? "M_PROTO8" :
    flags & M_PROTO9 ? "M_PROTO9" :
    flags & M_PROTO10 ? "M_PROTO10" :
    flags & M_PROTO11 ? "M_PROTO11" :
    flags & M_PROTO12 ? "M_PROTO12" :
    "none" ;

/*
 * Packet tag structure (see below for details).
 */
typedef struct m_tag {
	u_int16_t		m_tag_id;	/* Tag ID */
	u_int16_t		m_tag_len;	/* Length of data */
	u_int32_t		m_tag_cookie;	/* ABI/Module ID */
} m_tag_t;

/*
 * Record/packet header in first mbuf of chain; valid only if M_PKTHDR is set.
 * Size ILP32: 48
 *	 LP64: 56
 * Compile-time assertions in uipc_mbuf.c test these values to ensure that
 * they are correct.
 */
typedef struct pkthdr {
/*	struct ifnet	*rcvif;	*/	/* rcv interface */
	int32_t		 len;		/* total packet length */

	/* Layer crossing persistent information. */
	uint32_t	 flowid;	/* packet's 4-tuple system */
	uint64_t	 csum_flags;	/* checksum and offload features */
	uint16_t	 fibnum;	/* this packet should use this fib */
	uint8_t		 cosqos;	/* class/quality of service */
	uint8_t		 rsstype;	/* hash type */
} pkthdr_t;

/*
 * Description of external storage mapped into mbuf; valid only if M_EXT is
 * set.
 * Size ILP32: 28
 *	 LP64: 48
 * Compile-time assertions in uipc_mbuf.c test these values to ensure that
 * they are correct.
 */
typedef struct m_ext {
	volatile u_int	*ext_cnt;	/* pointer to ref count info */
	caddr_t		 ext_buf;	/* start of buffer */
	uint32_t	 ext_size;	/* size of buffer, for ext_free */
	uint32_t	 ext_type:8,	/* type of external storage */
			 ext_flags:24;	/* external storage mbuf flags */
	void		*ext_arg1;	/* optional argument pointer */
	void		*ext_arg2;	/* optional argument pointer */
} m_ext_t;

/*
 * The core of the mbuf object along with some shortcut defines for practical
 * purposes.
 */
struct mbuf {
	uintptr_t mbuf_addr;
	/*
	 * Header present at the beginning of every mbuf.
	 * Size ILP32: 24
	 *      LP64: 32
	 * Compile-time assertions in uipc_mbuf.c test these values to ensure
	 * that they are correct.
	 */
	caddr_t		 m_data;	/* location of data */
	int32_t		 m_len;		/* amount of data in this mbuf */
	uint32_t	 m_type:8,	/* type of data in this mbuf */
			 m_flags:24;	/* flags; see below */
	/*
	 * A set of optional headers (packet header, external storage header)
	 * and internal data storage.  Historically, these arrays were sized
	 * to MHLEN (space left after a packet header) and MLEN (space left
	 * after only a regular mbuf header); they are now variable size in
	 * order to support future work on variable-size mbufs.
	 */
	/* union { */
 	/* 	struct { */
	/* 		struct pkthdr	m_pkthdr; */
	/* 		union { */
	/* 			struct m_ext	m_ext; */
	/* 			char		m_pktdat[0]; */
	/* 		}; */
	/* 	}; */
	/* 	char	m_dat[0];		 */
	/* }; */
	char *m_dat;
};

typedef struct mbufinfo {
	uintptr_t mbuf_addr;
	caddr_t m_data;
	int32_t m_len;
	uint8_t m_type;
	uint32_t m_flags;
} mbufinfo_t;

translator mbufinfo_t < struct mbuf *p > {
	mbuf_addr = (uintptr_t)p;
	m_data = p->m_data;
	m_len = p->m_len;
	m_type = p->m_type & 0xff000000;
	m_flags = p->m_type & 0x00ffffff;
};
