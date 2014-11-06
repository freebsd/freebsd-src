/*-
 * Copyright (c) 2014
 * 	Alexander V. Chernikov <melifaro@FreeBSD.org>
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
 */

#ifndef _NET_RT_NHOPS_H_
#define	_NET_RT_NHOPS_H_



#define	NH_TYPE_DIRECT		1	/* Directly reachable, no data */
#define	NH_TYPE_BLACKHOLE	2	/* Blackhole route */
#define	NH_TYPE_REJECT		3	/* Send reject  */
#define	NH_TYPE_L2		4	/* Provides full prepend header */
#define	NH_TYPE_MUTATOR		5	/* NH+callback function  */
#define	NH_TYPE_MULTIPATH	6	/* Multipath route */

struct nhop_ctl_info {
	uint64_t	refcnt;		/* Use references */
	uint64_t	flags;		/* Options */

};

/* Multipath nhop info */
struct nhop_mpath_info {
	uint16_t	nhop;		/* Netxthop id */
};

/* mutator info */
struct nhop_mutator_info;
struct nhop_prepend;

typedef int nhop_mutate_t(struct mbuf **, struct nhop_prepend *nd, void *storage);
struct nhop_mutator_info {
	nhop_mutate_t	*func;
	char		data[];
};

/* Structures used for forwarding purposes */
#define	MAX_PREPEND_LEN		48	/* Max data that can be prepended */

/* Non-recursive nexthop */
struct nhop_prepend {
	uint16_t	nh_flags;	/* NH flags */
	uint8_t		nh_count;	/* Number of nexthops or data length */
	uint8_t		spare0;
	uint16_t	nh_mtu;		/* given nhop MTU */
	uint16_t	lifp_idx;	/* Logical interface index */
	union {
		uint16_t	ifp_idx;	/* Transmit interface index */
		uint16_t	nhop_idx;	/* L2 multipath nhop index */
	} i;
	uint16_t	aifp_idx;	/* Interface address index */
	uint16_t	spare1[2];
	union {
		char	data[MAX_PREPEND_LEN];	/* data to prepend */
#ifdef INET
		struct in_addr	gw4;		/* IPv4 gw address */
#endif
#ifdef INET6
		struct in6_addr	gw6;		/* IPv4 gw address */
#endif
	} d;
};

/* Internal flags */
#define	NHF_RECURSE		0x0001	/* Nexthop structure is recursive */
#define	NHF_L2_NHOP		0x0002	/* L2 interface has to be selected */
#define	NHF_L2_ME		0x0004	/* dst L2 address is our address */
#define	NHF_L2_INCOMPLETE 	0x0008	/* L2 header not prepended */

/* External flags */
#define	NHF_REJECT		0x0010	/* RTF_REJECT */
#define	NHF_BLACKHOLE		0x0020	/* RTF_BLACKHOLE */
#define	NHF_REDIRECT		0x0040	/* RTF_DYNAMIC|RTF_MODIFIED */
#define	NHF_DEFAULT		0x0080	/* Default route */
#define	NHF_BROADCAST		0x0100	/* RTF_BROADCAST */
#define	NHF_GATEWAY		0x0200	/* RTF_GATEWAY */

#define	NH_LIFP(nh)	ifnet_byindex_locked((nh)->lifp_idx)
#define	NH_TIFP(nh)	ifnet_byindex_locked((nh)->i.ifp_idx)
#define	NH_AIFP(nh)	ifnet_byindex_locked((nh)->aifp_idx)

/* L2/L3 recursive nexthop */
struct nhop_multi {
	uint8_t		nh_flags;	/* NH flags */
	uint8_t		nh_count;	/* Number of nexthops or data length */
	uint8_t		spare[2];
	uint16_t	nh_nhops[30];	/* Nexthop indexes */
};

/* Control plane nexthop data */
struct nhop_info {
};

/* Per-AF per-fib nhop table */
struct nhops_descr {
	uint32_t	nhop_size;	/* Nehthop data size */
	uint32_t	nhops_max;	/* Max number of nhops */
	void		*nhops_data;	/* Pointer to nhop data table */
	void		*nhops_info;	/* Pointer to nhop info table */
};


#if 0
typedef int nhop_resolve_t(struct sockaddr *dst, u_int fib, struct nhop_prepend *nd, struct nhop_info *nf);



int
lla_create_notify(struct sockaddr *dst, u_int fib, lla_notify_t *func, void *state, int flags);
#endif

/* Basic nexthop info used for uRPF/mtu checks */
struct nhop4_basic {
	struct ifnet	*nh_ifp;	/* Logical egress interface */
	uint16_t	nh_mtu;		/* nexthop mtu */
	uint16_t	nh_flags;	/* nhop flags */
	struct in_addr	nh_addr;	/* GW/DST IPv4 address */
};

struct nhop6_basic {
	struct ifnet	*nh_ifp;	/* Logical egress interface */
	uint16_t	nh_mtu;		/* nexthop mtu */
	uint16_t	nh_flags;	/* nhop flags */
	uint8_t		spare[4];
	struct in6_addr	nh_addr;	/* GW/DST IPv4 address */
};

struct nhopu_basic {
	union {
		struct nhop4_basic	nh4;
		struct nhop6_basic	nh6;
	} u;
};

/* Extended nexthop info used for control protocols */
struct nhop4_extended {
	struct ifnet	*nh_ifp;	/* Logical egress interface */
	uint16_t	nh_mtu;		/* nexthop mtu */
	uint16_t	nh_flags;	/* nhop flags */
	uint8_t		spare[4];
	struct in_addr	nh_addr;	/* GW/DST IPv4 address */
	struct in_addr	nh_src;		/* default source IPv4 address */
	uint64_t	spare2[2];
};

/* Does not differ from nhop6_basic */
struct nhop6_extended {
	struct ifnet	*nh_ifp;	/* Logical egress interface */
	uint16_t	nh_mtu;		/* nexthop mtu */
	uint16_t	nh_flags;	/* nhop flags */
	uint8_t		spare[4];
	struct in6_addr	nh_addr;	/* GW/DST IPv6 address */
	uint64_t	spare2[2];
};

/* route info used for control plane purposes */
struct rt4_basic {
	struct in_addr	rt_addr;	/* route prefix */
	struct in_addr	rt_gateway;	/* GW used */
	int		rt_flags;	/* Copy of rte flags */
	uint16_t	rt_mtu;
	uint16_t	rt_nhop;	/* nexthop id (might bi mpath) */
	struct in_addr	rt_mask;	/* route mask */
	uint16_t	spare[2];
};

struct rt6_basic {
	struct in6_addr	rt_addr;
	struct in6_addr	rt_gateway;
	int		rt_flags;
	uint16_t	rt_mtu;
	uint16_t	rt_nhop;
	uint8_t		rt_mask;
	uint8_t		spare[7];
};

struct nhopu_extended {
	union {
		struct nhop4_extended	nh4;
		struct nhop6_extended	nh6;
	} u;
};

struct route_info {
	struct nhop_prepend	*ri_nh;		/* Desired nexthop to use */
	struct nhopu_basic	*ri_nh_info;	/* Get selected route info */
	uint16_t		ri_mtu;		/* Get selected route MTU */
	uint16_t		spare;
	uint32_t		scopeid;	/* Desired scope id to use */
};

struct route_compat {
	struct nhop_prepend	*ro_nh;
	void			*spare0;
	void			*spare1;
	int			ro_flags;
};

int fib4_lookup_nh_ifp(uint32_t fibnum, struct in_addr dst, uint32_t flowid,
    struct nhop4_basic *pnh4);
int fib4_lookup_nh_basic(uint32_t fibnum, struct in_addr dst, uint32_t flowid,
    struct nhop4_basic *pnh4);
int fib4_lookup_nh_ext(uint32_t fibnum, struct in_addr dst,
    uint32_t flowid, uint32_t flags, struct nhop4_extended *pnh4);
void fib4_free_nh_ext(uint32_t fibnum, struct nhop4_extended *pnh4);
#define	NHOP_LOOKUP_REF	0x01


int fib6_lookup_nh_ifp(uint32_t fibnum, struct in6_addr *dst, uint32_t scopeid,
    uint32_t flowid, struct nhop6_basic *pnh6);
int fib6_lookup_nh_basic(uint32_t fibnum, struct in6_addr *dst,
    uint32_t scopeid, uint32_t flowid, struct nhop6_basic *pnh6);
int fib6_lookup_nh_ext(uint32_t fibnum, struct in6_addr *dst,
    uint32_t scopeid, uint32_t flowid, uint32_t flags,
    struct nhop6_extended *pnh6);
void fib6_free_nh_ext(uint32_t fibnum, struct nhop6_extended *pnh6);

void fib_free_nh_ext(uint32_t fibnum, struct nhopu_extended *pnhu);


void fib4_free_nh_prepend(uint32_t fibnum, struct nhop_prepend *nh);
void fib4_choose_prepend(uint32_t fibnum, struct nhop_prepend *nh_src,
    uint32_t flowid, struct nhop_prepend *nh, struct nhop4_extended *nh_ext);
int fib4_lookup_prepend(uint32_t fibnum, struct in_addr dst, struct mbuf *m,
    struct nhop_prepend *nh, struct nhop4_extended *nh_ext);

int fib4_sendmbuf(struct ifnet *ifp, struct mbuf *m, struct nhop_prepend *nh,
    struct in_addr dst);

void fib6_free_nh_prepend(uint32_t fibnum, struct nhop_prepend *nh);
void fib6_choose_prepend(uint32_t fibnum, struct nhop_prepend *nh_src,
    uint32_t flowid, struct nhop_prepend *nh, struct nhop6_extended *nh_ext);
int fib6_lookup_prepend(uint32_t fibnum, struct in6_addr *dst, uint32_t scopeid,
    struct mbuf *m, struct nhop_prepend *nh, struct nhop6_extended *nh_ext);

int fib6_sendmbuf(struct ifnet *ifp, struct ifnet *origifp, struct mbuf *m,
    struct nhop_prepend *nh);

#define	FWD_INET	0
#define	FWD_INET6	1

#define	FWD_SIZE	2

#define	FWD_NAME_MAX	15

#define	FWD_MULTIPATH	0x0001	/* has multipath support */
#define	FWD_OLDMASKS	0x0002	/* has support for non-contig masks */
#define	FWD_DEFAULT	0x0004	/* installs as default fib mechanism */
#define	FWD_MANAGELOCK	0x0004	/* manage its own locking */

typedef void *fib_init_t(u_int fibnum);
typedef void fib_destroy_t(void *state);
typedef int fib_dump_t(void *state, struct radix_node_head *rnh);
typedef	int fib_change_t(void *state, int req, struct rtentry *rte,
    struct rt_addrinfo *info);
typedef int fib_lookup_t(void *state, void *key, uint64_t *attr, u_int flowid,
    void *nhop);

/* Structure used by external module */
struct fwd_module_info {
	uint8_t		fwd_family;	/* family we're registering to */
	char		name[FWD_NAME_MAX];	/* fwd module name */
	uint32_t	capabilities;
	fib_init_t	*fib_init;
	fib_destroy_t	*fib_destroy;
	fib_dump_t	*fib_dump;
	fib_change_t	*fib_change;
	fib_lookup_t	*fib_lookup;
};

/* Internal version of previous structure */
struct fwd_module {
	TAILQ_ENTRY(fwd_module)	list;
	uint8_t		fwd_family;
	char		name[FWD_NAME_MAX];
	uint32_t	capabilities;
	fib_init_t	*fib_init;
	fib_destroy_t	*fib_destroy;
	fib_dump_t	*fib_dump;
	fib_change_t	*fib_change;
	fib_lookup_t	*fib_lookup;
};

int fwd_attach_module(struct fwd_module_info *m, void **);
int fwd_destroy_module(void *state);
int fwd_change_fib(struct radix_node_head *rnh, int req, struct rtentry *rte,
    struct rt_addrinfo *info);

#endif

