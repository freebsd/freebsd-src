/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2000 Assar Westerlund
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer 
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _LINUX_SOCKET_H_
#define _LINUX_SOCKET_H_

/* msg flags in recvfrom/recvmsg */

#define LINUX_MSG_OOB		0x01
#define LINUX_MSG_PEEK		0x02
#define LINUX_MSG_DONTROUTE	0x04
#define LINUX_MSG_CTRUNC	0x08
#define LINUX_MSG_PROXY		0x10
#define LINUX_MSG_TRUNC		0x20
#define LINUX_MSG_DONTWAIT	0x40
#define LINUX_MSG_EOR		0x80
#define LINUX_MSG_WAITALL	0x100
#define LINUX_MSG_FIN		0x200
#define LINUX_MSG_SYN		0x400
#define LINUX_MSG_CONFIRM	0x800
#define LINUX_MSG_RST		0x1000
#define LINUX_MSG_ERRQUEUE	0x2000
#define LINUX_MSG_NOSIGNAL	0x4000
#define LINUX_MSG_WAITFORONE	0x10000
#define LINUX_MSG_CMSG_CLOEXEC	0x40000000

struct l_msghdr {
	l_uintptr_t	msg_name;
	l_int		msg_namelen;
	l_uintptr_t	msg_iov;
	l_size_t	msg_iovlen;
	l_uintptr_t	msg_control;
	l_size_t	msg_controllen;
	l_uint		msg_flags;
};

struct l_mmsghdr {
	struct l_msghdr	msg_hdr;
	l_uint		msg_len;

};

struct l_cmsghdr {
	l_size_t	cmsg_len;
	l_int		cmsg_level;
	l_int		cmsg_type;
};

/* Ancillary data object information macros */

#define LINUX_CMSG_ALIGN(len)	roundup2(len, sizeof(l_ulong))
#define LINUX_CMSG_DATA(cmsg)	((void *)((char *)(cmsg) + \
				    LINUX_CMSG_ALIGN(sizeof(struct l_cmsghdr))))
#define LINUX_CMSG_SPACE(len)	(LINUX_CMSG_ALIGN(sizeof(struct l_cmsghdr)) + \
				    LINUX_CMSG_ALIGN(len))
#define LINUX_CMSG_LEN(len)	(LINUX_CMSG_ALIGN(sizeof(struct l_cmsghdr)) + \
				    (len))
#define LINUX_CMSG_FIRSTHDR(msg) \
				((msg)->msg_controllen >= \
				    sizeof(struct l_cmsghdr) ? \
				    (struct l_cmsghdr *) \
				        PTRIN((msg)->msg_control) : \
				    (struct l_cmsghdr *)(NULL))
#define LINUX_CMSG_NXTHDR(msg, cmsg) \
				((((char *)(cmsg) + \
				    LINUX_CMSG_ALIGN((cmsg)->cmsg_len) + \
				    sizeof(*(cmsg))) > \
				    (((char *)PTRIN((msg)->msg_control)) + \
				    (msg)->msg_controllen)) ? \
				    (struct l_cmsghdr *) NULL : \
				    (struct l_cmsghdr *)((char *)(cmsg) + \
				    LINUX_CMSG_ALIGN((cmsg)->cmsg_len)))

#define CMSG_HDRSZ		CMSG_LEN(0)
#define L_CMSG_HDRSZ		LINUX_CMSG_LEN(0)

/* Supported socket types */

#define	LINUX_SOCK_STREAM	1
#define	LINUX_SOCK_DGRAM	2
#define	LINUX_SOCK_RAW		3
#define	LINUX_SOCK_RDM		4
#define	LINUX_SOCK_SEQPACKET	5

#define	LINUX_SOCK_MAX		LINUX_SOCK_SEQPACKET

#define	LINUX_SOCK_TYPE_MASK	0xf

/* Flags for socket, socketpair, accept4 */

#define	LINUX_SOCK_CLOEXEC	LINUX_O_CLOEXEC
#define	LINUX_SOCK_NONBLOCK	LINUX_O_NONBLOCK

struct l_ucred {
	uint32_t	pid;
	uint32_t	uid;
	uint32_t	gid;
};

#if defined(__i386__) || defined(__arm__) || \
    (defined(__amd64__) && defined(COMPAT_LINUX32))

struct linux_accept_args {
	register_t s;
	register_t addr;
	register_t namelen;
};

int linux_accept(struct thread *td, struct linux_accept_args *args);

/* Operations for socketcall */
#define	LINUX_SOCKET		1
#define	LINUX_BIND		2
#define	LINUX_CONNECT		3
#define	LINUX_LISTEN		4
#define	LINUX_ACCEPT		5
#define	LINUX_GETSOCKNAME	6
#define	LINUX_GETPEERNAME	7
#define	LINUX_SOCKETPAIR	8
#define	LINUX_SEND		9
#define	LINUX_RECV		10
#define	LINUX_SENDTO		11
#define	LINUX_RECVFROM		12
#define	LINUX_SHUTDOWN		13
#define	LINUX_SETSOCKOPT	14
#define	LINUX_GETSOCKOPT	15
#define	LINUX_SENDMSG		16
#define	LINUX_RECVMSG		17
#define	LINUX_ACCEPT4		18
#define	LINUX_RECVMMSG		19
#define	LINUX_SENDMMSG		20
#define	LINUX_SENDFILE		21

#endif /* __i386__ || __arm__ || (__amd64__ && COMPAT_LINUX32) */

/* Socket defines */
#define	LINUX_SOL_SOCKET	1

#define	LINUX_SO_DEBUG		1
#define	LINUX_SO_REUSEADDR	2
#define	LINUX_SO_TYPE		3
#define	LINUX_SO_ERROR		4
#define	LINUX_SO_DONTROUTE	5
#define	LINUX_SO_BROADCAST	6
#define	LINUX_SO_SNDBUF		7
#define	LINUX_SO_RCVBUF		8
#define	LINUX_SO_KEEPALIVE	9
#define	LINUX_SO_OOBINLINE	10
#define	LINUX_SO_NO_CHECK	11
#define	LINUX_SO_PRIORITY	12
#define	LINUX_SO_LINGER		13
#define	LINUX_SO_REUSEPORT	15
#ifndef LINUX_SO_PASSCRED	/* powerpc differs */
#define	LINUX_SO_PASSCRED	16
#define	LINUX_SO_PEERCRED	17
#define	LINUX_SO_RCVLOWAT	18
#define	LINUX_SO_SNDLOWAT	19
#define	LINUX_SO_RCVTIMEO	20
#define	LINUX_SO_SNDTIMEO	21
#endif
#define	LINUX_SO_TIMESTAMPO	29
#define	LINUX_SO_TIMESTAMPN	63
#define	LINUX_SO_TIMESTAMPNSO	35
#define	LINUX_SO_TIMESTAMPNSN	64
#define	LINUX_SO_ACCEPTCONN	30
#define	LINUX_SO_PEERSEC	31
#define	LINUX_SO_SNDBUFFORCE	32
#define	LINUX_SO_RCVBUFFORCE	33
#define	LINUX_SO_PROTOCOL	38
#define	LINUX_SO_DOMAIN		39
#define	LINUX_SO_PEERGROUPS	59

/* Socket-level control message types */

#define LINUX_SCM_RIGHTS	0x01
#define LINUX_SCM_CREDENTIALS	0x02
#define LINUX_SCM_TIMESTAMPO	LINUX_SO_TIMESTAMPO
#define LINUX_SCM_TIMESTAMPN	LINUX_SO_TIMESTAMPN
#define LINUX_SCM_TIMESTAMPNSO	LINUX_SO_TIMESTAMPNSO
#define LINUX_SCM_TIMESTAMPNSN	LINUX_SO_TIMESTAMPNSN

/* Socket options */
#define	LINUX_IP_TOS		1
#define	LINUX_IP_TTL		2
#define	LINUX_IP_HDRINCL	3
#define	LINUX_IP_OPTIONS	4
#define	LINUX_IP_ROUTER_ALERT	5
#define	LINUX_IP_RECVOPTS	6
#define	LINUX_IP_RETOPTS	7
#define	LINUX_IP_PKTINFO	8
#define	LINUX_IP_PKTOPTIONS	9
#define	LINUX_IP_MTU_DISCOVER	10
#define	LINUX_IP_RECVERR	11
#define	LINUX_IP_RECVTTL	12
#define	LINUX_IP_RECVTOS	13
#define	LINUX_IP_MTU		14
#define	LINUX_IP_FREEBIND	15
#define	LINUX_IP_IPSEC_POLICY	16
#define	LINUX_IP_XFRM_POLICY	17
#define	LINUX_IP_PASSSEC	18
#define	LINUX_IP_TRANSPARENT	19

#define	LINUX_IP_ORIGDSTADDR		20
#define	LINUX_IP_RECVORIGDSTADDR	LINUX_IP_ORIGDSTADDR
#define	LINUX_IP_MINTTL		21
#define	LINUX_IP_NODEFRAG	22
#define	LINUX_IP_CHECKSUM	23
#define	LINUX_IP_BIND_ADDRESS_NO_PORT	24
#define	LINUX_IP_RECVFRAGSIZE	25

#define	LINUX_IP_MULTICAST_IF		32
#define	LINUX_IP_MULTICAST_TTL		33
#define	LINUX_IP_MULTICAST_LOOP		34
#define	LINUX_IP_ADD_MEMBERSHIP		35
#define	LINUX_IP_DROP_MEMBERSHIP	36
#define	LINUX_IP_UNBLOCK_SOURCE		37
#define	LINUX_IP_BLOCK_SOURCE		38
#define	LINUX_IP_ADD_SOURCE_MEMBERSHIP	39
#define	LINUX_IP_DROP_SOURCE_MEMBERSHIP	40
#define	LINUX_IP_MSFILTER		41

#define	LINUX_MCAST_JOIN_GROUP		42
#define	LINUX_MCAST_BLOCK_SOURCE	43
#define	LINUX_MCAST_UNBLOCK_SOURCE	44
#define	LINUX_MCAST_LEAVE_GROUP		45
#define	LINUX_MCAST_JOIN_SOURCE_GROUP	46
#define	LINUX_MCAST_LEAVE_SOURCE_GROUP	47
#define	LINUX_MCAST_MSFILTER		48
#define	LINUX_IP_MULTICAST_ALL		49
#define	LINUX_IP_UNICAST_IF		50

#define	LINUX_IPV6_ADDRFORM		1
#define	LINUX_IPV6_2292PKTINFO		2
#define	LINUX_IPV6_2292HOPOPTS		3
#define	LINUX_IPV6_2292DSTOPTS		4
#define	LINUX_IPV6_2292RTHDR		5
#define	LINUX_IPV6_2292PKTOPTIONS	6
#define	LINUX_IPV6_CHECKSUM		7
#define	LINUX_IPV6_2292HOPLIMIT		8
#define	LINUX_IPV6_NEXTHOP		9
#define	LINUX_IPV6_AUTHHDR		10
#define	LINUX_IPV6_FLOWINFO		11

#define	LINUX_IPV6_UNICAST_HOPS		16
#define	LINUX_IPV6_MULTICAST_IF		17
#define	LINUX_IPV6_MULTICAST_HOPS	18
#define	LINUX_IPV6_MULTICAST_LOOP	19
#define	LINUX_IPV6_ADD_MEMBERSHIP	20
#define	LINUX_IPV6_DROP_MEMBERSHIP	21
#define	LINUX_IPV6_ROUTER_ALERT		22
#define	LINUX_IPV6_MTU_DISCOVER		23
#define	LINUX_IPV6_MTU			24
#define	LINUX_IPV6_RECVERR		25
#define	LINUX_IPV6_V6ONLY		26
#define	LINUX_IPV6_JOIN_ANYCAST		27
#define	LINUX_IPV6_LEAVE_ANYCAST	28
#define	LINUX_IPV6_MULTICAST_ALL	29
#define	LINUX_IPV6_ROUTER_ALERT_ISOLATE	30

#define	LINUX_IPV6_FLOWLABEL_MGR	32
#define	LINUX_IPV6_FLOWINFO_SEND	33

#define	LINUX_IPV6_IPSEC_POLICY		34
#define	LINUX_IPV6_XFRM_POLICY		35
#define	LINUX_IPV6_HDRINCL		36

#define	LINUX_IPV6_RECVPKTINFO		49
#define	LINUX_IPV6_PKTINFO		50
#define	LINUX_IPV6_RECVHOPLIMIT		51
#define	LINUX_IPV6_HOPLIMIT		52
#define	LINUX_IPV6_RECVHOPOPTS		53
#define	LINUX_IPV6_HOPOPTS		54
#define	LINUX_IPV6_RTHDRDSTOPTS		55
#define	LINUX_IPV6_RECVRTHDR		56
#define	LINUX_IPV6_RTHDR		57
#define	LINUX_IPV6_RECVDSTOPTS		58
#define	LINUX_IPV6_DSTOPTS		59
#define	LINUX_IPV6_RECVPATHMTU		60
#define	LINUX_IPV6_PATHMTU		61
#define	LINUX_IPV6_DONTFRAG		62

#define	LINUX_IPV6_AUTOFLOWLABEL	70
#define	LINUX_IPV6_ADDR_PREFERENCES	72
#define	LINUX_IPV6_MINHOPCOUNT		73
#define	LINUX_IPV6_ORIGDSTADDR		74
#define	LINUX_IPV6_TRANSPARENT		75
#define	LINUX_IPV6_UNICAST_IF		76
#define	LINUX_IPV6_RECVFRAGSIZE		77
#define	LINUX_IPV6_FREEBIND		78

#define	LINUX_TCP_NODELAY	1
#define	LINUX_TCP_MAXSEG	2
#define	LINUX_TCP_CORK		3
#define	LINUX_TCP_KEEPIDLE	4
#define	LINUX_TCP_KEEPINTVL	5
#define	LINUX_TCP_KEEPCNT	6
#define	LINUX_TCP_INFO		11
#define	LINUX_TCP_MD5SIG	14

struct l_ifmap {
	l_ulong		mem_start;
	l_ulong		mem_end;
	l_ushort	base_addr;
	u_char		irq;
	u_char		dma;
	u_char		port;
	/* 3 bytes spare */
};

/*
 * Careful changing the declaration of this structure.
 * To use FreeBSD names to access the struct l_ifreq members the
 * member names of struct l_ifreq should be equal to the FreeBSD.
 */
struct l_ifreq {
	char	ifr_name[LINUX_IFNAMSIZ];
	union {
		struct l_sockaddr	ifru_addr;
		struct l_sockaddr	ifru_dstaddr;
		struct l_sockaddr	ifru_broadaddr;
		struct l_sockaddr	ifru_netmask;
		struct l_sockaddr	ifru_hwaddr;
		l_short		ifru_flags[1];
		l_int		ifru_index;
		l_int		ifru_mtu;
		struct l_ifmap	ifru_map;
		char		ifru_slave[LINUX_IFNAMSIZ];
		char		ifru_newname[LINUX_IFNAMSIZ];
		l_uintptr_t	ifru_data;
	} ifr_ifru;
};

/*
 * Define here members which are not exists in the FreeBSD struct ifreq.
 */
#define	ifr_hwaddr	ifr_ifru.ifru_hwaddr	/* MAC address */

#endif /* _LINUX_SOCKET_H_ */
