/*	$NetBSD$	*/

/*
 * Copyright (C) 1993-2001, 2003 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * @(#)ip_fil.h	1.35 6/5/96
 * Id: ip_fil.h,v 2.170.2.18 2005/03/28 10:47:52 darrenr Exp
 */

#ifndef	__IP_FIL_H__
#define	__IP_FIL_H__

#ifndef	SOLARIS
# define SOLARIS (defined(sun) && (defined(__svr4__) || defined(__SVR4)))
#endif

#ifndef	__P
# ifdef	__STDC__
#  define	__P(x)	x
# else
#  define	__P(x)	()
# endif
#endif

#if defined(__STDC__) || defined(__GNUC__)
# define	SIOCADAFR	_IOW('r', 60, struct ipfobj)
# define	SIOCRMAFR	_IOW('r', 61, struct ipfobj)
# define	SIOCSETFF	_IOW('r', 62, u_int)
# define	SIOCGETFF	_IOR('r', 63, u_int)
# define	SIOCGETFS	_IOWR('r', 64, struct ipfobj)
# define	SIOCIPFFL	_IOWR('r', 65, int)
# define	SIOCIPFFB	_IOR('r', 66, int)
# define	SIOCADIFR	_IOW('r', 67, struct ipfobj)
# define	SIOCRMIFR	_IOW('r', 68, struct ipfobj)
# define	SIOCSWAPA	_IOR('r', 69, u_int)
# define	SIOCINAFR	_IOW('r', 70, struct ipfobj)
# define	SIOCINIFR	_IOW('r', 71, struct ipfobj)
# define	SIOCFRENB	_IOW('r', 72, u_int)
# define	SIOCFRSYN	_IOW('r', 73, u_int)
# define	SIOCFRZST	_IOWR('r', 74, struct ipfobj)
# define	SIOCZRLST	_IOWR('r', 75, struct ipfobj)
# define	SIOCAUTHW	_IOWR('r', 76, struct ipfobj)
# define	SIOCAUTHR	_IOWR('r', 77, struct ipfobj)
# define	SIOCATHST	_IOWR('r', 78, struct ipfobj)
# define	SIOCSTLCK	_IOWR('r', 79, u_int)
# define	SIOCSTPUT	_IOWR('r', 80, struct ipfobj)
# define	SIOCSTGET	_IOWR('r', 81, struct ipfobj)
# define	SIOCSTGSZ	_IOWR('r', 82, struct ipfobj)
# define	SIOCGFRST	_IOWR('r', 83, struct ipfobj)
# define	SIOCSETLG	_IOWR('r', 84, int)
# define	SIOCGETLG	_IOWR('r', 85, int)
# define	SIOCFUNCL	_IOWR('r', 86, struct ipfunc_resolve)
# define	SIOCIPFGETNEXT	_IOWR('r', 87, struct ipfobj)
# define	SIOCIPFGET	_IOWR('r', 88, struct ipfobj)
# define	SIOCIPFSET	_IOWR('r', 89, struct ipfobj)
# define	SIOCIPFL6	_IOWR('r', 90, int)
#else
# define	SIOCADAFR	_IOW(r, 60, struct ipfobj)
# define	SIOCRMAFR	_IOW(r, 61, struct ipfobj)
# define	SIOCSETFF	_IOW(r, 62, u_int)
# define	SIOCGETFF	_IOR(r, 63, u_int)
# define	SIOCGETFS	_IOWR(r, 64, struct ipfobj)
# define	SIOCIPFFL	_IOWR(r, 65, int)
# define	SIOCIPFFB	_IOR(r, 66, int)
# define	SIOCADIFR	_IOW(r, 67, struct ipfobj)
# define	SIOCRMIFR	_IOW(r, 68, struct ipfobj)
# define	SIOCSWAPA	_IOR(r, 69, u_int)
# define	SIOCINAFR	_IOW(r, 70, struct ipfobj)
# define	SIOCINIFR	_IOW(r, 71, struct ipfobj)
# define	SIOCFRENB	_IOW(r, 72, u_int)
# define	SIOCFRSYN	_IOW(r, 73, u_int)
# define	SIOCFRZST	_IOWR(r, 74, struct ipfobj)
# define	SIOCZRLST	_IOWR(r, 75, struct ipfobj)
# define	SIOCAUTHW	_IOWR(r, 76, struct ipfobj)
# define	SIOCAUTHR	_IOWR(r, 77, struct ipfobj)
# define	SIOCATHST	_IOWR(r, 78, struct ipfobj)
# define	SIOCSTLCK	_IOWR(r, 79, u_int)
# define	SIOCSTPUT	_IOWR(r, 80, struct ipfobj)
# define	SIOCSTGET	_IOWR(r, 81, struct ipfobj)
# define	SIOCSTGSZ	_IOWR(r, 82, struct ipfobj)
# define	SIOCGFRST	_IOWR(r, 83, struct ipfobj)
# define	SIOCSETLG	_IOWR(r, 84, int)
# define	SIOCGETLG	_IOWR(r, 85, int)
# define	SIOCFUNCL	_IOWR(r, 86, struct ipfunc_resolve)
# define	SIOCIPFGETNEXT	_IOWR(r, 87, struct ipfobj)
# define	SIOCIPFGET	_IOWR(r, 88, struct ipfobj)
# define	SIOCIPFSET	_IOWR(r, 89, struct ipfobj)
# define	SIOCIPFL6	_IOWR(r, 90, int)
#endif
#define	SIOCADDFR	SIOCADAFR
#define	SIOCDELFR	SIOCRMAFR
#define	SIOCINSFR	SIOCINAFR


struct ipscan;
struct ifnet;


typedef	int	(* lookupfunc_t) __P((void *, int, void *));

/*
 * i6addr is used as a container for both IPv4 and IPv6 addresses, as well
 * as other types of objects, depending on its qualifier.
 */
#ifdef	USE_INET6
typedef	union	i6addr	{
	u_32_t	i6[4];
	struct	in_addr	in4;
	struct	in6_addr in6;
	void	*vptr[2];
	lookupfunc_t	lptr[2];
} i6addr_t;
#else
typedef	union	i6addr	{
	u_32_t	i6[4];
	struct	in_addr	in4;
	void	*vptr[2];
	lookupfunc_t	lptr[2];
} i6addr_t;
#endif

#define in4_addr	in4.s_addr
#define	iplookupnum	i6[0]
#define	iplookuptype	i6[1]
/*
 * NOTE: These DO overlap the above on 64bit systems and this IS recognised.
 */
#define	iplookupptr	vptr[0]
#define	iplookupfunc	lptr[1]

#define	I60(x)	(((i6addr_t *)(x))->i6[0])
#define	I61(x)	(((i6addr_t *)(x))->i6[1])
#define	I62(x)	(((i6addr_t *)(x))->i6[2])
#define	I63(x)	(((i6addr_t *)(x))->i6[3])
#define	HI60(x)	ntohl(((i6addr_t *)(x))->i6[0])
#define	HI61(x)	ntohl(((i6addr_t *)(x))->i6[1])
#define	HI62(x)	ntohl(((i6addr_t *)(x))->i6[2])
#define	HI63(x)	ntohl(((i6addr_t *)(x))->i6[3])

#define	IP6_EQ(a,b)	((I63(a) == I63(b)) && (I62(a) == I62(b)) && \
			 (I61(a) == I61(b)) && (I60(a) == I60(b)))
#define	IP6_NEQ(a,b)	((I63(a) != I63(b)) || (I62(a) != I62(b)) || \
			 (I61(a) != I61(b)) || (I60(a) != I60(b)))
#define IP6_ISZERO(a)   ((I60(a) | I61(a) | I62(a) | I63(a)) == 0)
#define IP6_NOTZERO(a)  ((I60(a) | I61(a) | I62(a) | I63(a)) != 0)
#define	IP6_GT(a,b)	(HI60(a) > HI60(b) || (HI60(a) == HI60(b) && \
			  (HI61(a) > HI61(b) || (HI61(a) == HI61(b) && \
			    (HI62(a) > HI62(b) || (HI62(a) == HI62(b) && \
			      HI63(a) > HI63(b)))))))
#define	IP6_LT(a,b)	(HI60(a) < HI60(b) || (HI60(a) == HI60(b) && \
			  (HI61(a) < HI61(b) || (HI61(a) == HI61(b) && \
			    (HI62(a) < HI62(b) || (HI62(a) == HI62(b) && \
			      HI63(a) < HI63(b)))))))
#define	NLADD(n,x)	htonl(ntohl(n) + (x))
#define	IP6_INC(a)	\
		{ i6addr_t *_i6 = (i6addr_t *)(a); \
		  _i6->i6[0] = NLADD(_i6->i6[0], 1); \
		  if (_i6->i6[0] == 0) { \
			_i6->i6[0] = NLADD(_i6->i6[1], 1); \
			if (_i6->i6[1] == 0) { \
				_i6->i6[0] = NLADD(_i6->i6[2], 1); \
				if (_i6->i6[2] == 0) { \
					_i6->i6[0] = NLADD(_i6->i6[3], 1); \
				} \
			} \
		  } \
		}
#define	IP6_ADD(a,x,d)	\
		{ i6addr_t *_s = (i6addr_t *)(a); \
		  i6addr_t *_d = (i6addr_t *)(d); \
		  _d->i6[0] = NLADD(_s->i6[0], x); \
		  if (ntohl(_d->i6[0]) < ntohl(_s->i6[0])) { \
			_d->i6[1] = NLADD(_d->i6[1], 1); \
			if (ntohl(_d->i6[1]) < ntohl(_s->i6[1])) { \
				_d->i6[2] = NLADD(_d->i6[2], 1); \
				if (ntohl(_d->i6[2]) < ntohl(_s->i6[2])) { \
					_d->i6[3] = NLADD(_d->i6[3], 1); \
				} \
			} \
		  } \
		}
#define	IP6_AND(a,b,d)	{ i6addr_t *_s1 = (i6addr_t *)(a); \
			  i6addr_t *_s2 = (i6addr_t *)(d); \
			  i6addr_t *_d = (i6addr_t *)(d); \
			  _d->i6[0] = _s1->i6[0] & _s2->i6[0]; \
			  _d->i6[1] = _s1->i6[1] & _s2->i6[1]; \
			  _d->i6[2] = _s1->i6[2] & _s2->i6[2]; \
			  _d->i6[3] = _s1->i6[3] & _s2->i6[3]; \
			}
#define	IP6_MERGE(a,b,c) \
			{ i6addr_t *_d, *_s1, *_s2; \
			  _d = (i6addr_t *)(a); \
			  _s1 = (i6addr_t *)(b); \
			  _s2 = (i6addr_t *)(c); \
			  _d->i6[0] |= _s1->i6[0] & ~_s2->i6[0]; \
			  _d->i6[1] |= _s1->i6[1] & ~_s2->i6[1]; \
			  _d->i6[2] |= _s1->i6[2] & ~_s2->i6[2]; \
			  _d->i6[2] |= _s1->i6[3] & ~_s2->i6[3]; \
			}


typedef	struct	fr_ip	{
	u_32_t	fi_v:4;		/* IP version */
	u_32_t	fi_xx:4;	/* spare */
	u_32_t	fi_tos:8;	/* IP packet TOS */
	u_32_t	fi_ttl:8;	/* IP packet TTL */
	u_32_t	fi_p:8;		/* IP packet protocol */
	u_32_t	fi_optmsk;	/* bitmask composed from IP options */
	i6addr_t fi_src;	/* source address from packet */
	i6addr_t fi_dst;	/* destination address from packet */
	u_short	fi_secmsk;	/* bitmask composed from IP security options */
	u_short	fi_auth;	/* authentication code from IP sec. options */
	u_32_t	fi_flx;		/* packet flags */
	u_32_t	fi_tcpmsk;	/* TCP options set/reset */
	u_32_t	fi_res1;	/* RESERVED */
} fr_ip_t;

/*
 * For use in fi_flx
 */
#define	FI_TCPUDP	0x0001	/* TCP/UCP implied comparison*/
#define	FI_OPTIONS	0x0002
#define	FI_FRAG		0x0004
#define	FI_SHORT	0x0008
#define	FI_NATED	0x0010
#define	FI_MULTICAST	0x0020
#define	FI_BROADCAST	0x0040
#define	FI_MBCAST	0x0080
#define	FI_STATE	0x0100
#define	FI_BADNAT	0x0200
#define	FI_BAD		0x0400
#define	FI_OOW		0x0800	/* Out of state window, else match */
#define	FI_ICMPERR	0x1000
#define	FI_FRAGBODY	0x2000
#define	FI_BADSRC	0x4000
#define	FI_LOWTTL	0x8000
#define	FI_CMP		0xcfe3	/* Not FI_FRAG,FI_NATED,FI_FRAGTAIL */
#define	FI_ICMPCMP	0x0003	/* Flags we can check for ICMP error packets */
#define	FI_WITH		0xeffe	/* Not FI_TCPUDP */
#define	FI_V6EXTHDR	0x10000
#define	FI_COALESCE	0x20000
#define	FI_NOCKSUM	0x20000000	/* don't do a L4 checksum validation */
#define	FI_DONTCACHE	0x40000000	/* don't cache the result */
#define	FI_IGNORE	0x80000000

#define	fi_saddr	fi_src.in4.s_addr
#define	fi_daddr	fi_dst.in4.s_addr
#define	fi_srcnum	fi_src.iplookupnum
#define	fi_dstnum	fi_dst.iplookupnum
#define	fi_srctype	fi_src.iplookuptype
#define	fi_dsttype	fi_dst.iplookuptype
#define	fi_srcptr	fi_src.iplookupptr
#define	fi_dstptr	fi_dst.iplookupptr
#define	fi_srcfunc	fi_src.iplookupfunc
#define	fi_dstfunc	fi_dst.iplookupfunc


/*
 * These are both used by the state and NAT code to indicate that one port or
 * the other should be treated as a wildcard.
 * NOTE: When updating, check bit masks in ip_state.h and update there too.
 */
#define	SI_W_SPORT	0x00000100
#define	SI_W_DPORT	0x00000200
#define	SI_WILDP	(SI_W_SPORT|SI_W_DPORT)
#define	SI_W_SADDR	0x00000400
#define	SI_W_DADDR	0x00000800
#define	SI_WILDA	(SI_W_SADDR|SI_W_DADDR)
#define	SI_NEWFR	0x00001000
#define	SI_CLONE	0x00002000
#define	SI_CLONED	0x00004000


typedef	struct	fr_info	{
	void	*fin_ifp;		/* interface packet is `on' */
	fr_ip_t	fin_fi;		/* IP Packet summary */
	union	{
		u_short	fid_16[2];	/* TCP/UDP ports, ICMP code/type */
		u_32_t	fid_32;
	} fin_dat;
	int	fin_out;		/* in or out ? 1 == out, 0 == in */
	int	fin_rev;		/* state only: 1 = reverse */
	u_short	fin_hlen;		/* length of IP header in bytes */
	u_char	fin_tcpf;		/* TCP header flags (SYN, ACK, etc) */
	u_char	fin_icode;		/* ICMP error to return */
	u_32_t	fin_rule;		/* rule # last matched */
	char	fin_group[FR_GROUPLEN];	/* group number, -1 for none */
	struct	frentry *fin_fr;	/* last matching rule */
	void	*fin_dp;		/* start of data past IP header */
	int	fin_dlen;		/* length of data portion of packet */
	int	fin_plen;
	int	fin_ipoff;		/* # bytes from buffer start to hdr */
	u_short	fin_id;			/* IP packet id field */
	u_short	fin_off;
	int	fin_depth;		/* Group nesting depth */
	int	fin_error;		/* Error code to return */
	void	*fin_nat;
	void	*fin_state;
	void	*fin_nattag;
	ip_t	*fin_ip;
	mb_t	**fin_mp;		/* pointer to pointer to mbuf */
	mb_t	*fin_m;			/* pointer to mbuf */
#ifdef	MENTAT
	mb_t	*fin_qfm;		/* pointer to mblk where pkt starts */
	void	*fin_qpi;
#endif
#ifdef	__sgi
	void	*fin_hbuf;
#endif
} fr_info_t;

#define	fin_v		fin_fi.fi_v
#define	fin_p		fin_fi.fi_p
#define	fin_flx		fin_fi.fi_flx
#define	fin_optmsk	fin_fi.fi_optmsk
#define	fin_secmsk	fin_fi.fi_secmsk
#define	fin_auth	fin_fi.fi_auth
#define	fin_src		fin_fi.fi_src.in4
#define	fin_src6	fin_fi.fi_src.in6
#define	fin_saddr	fin_fi.fi_saddr
#define	fin_dst		fin_fi.fi_dst.in4
#define	fin_dst6	fin_fi.fi_dst.in6
#define	fin_daddr	fin_fi.fi_daddr
#define	fin_data	fin_dat.fid_16
#define	fin_sport	fin_dat.fid_16[0]
#define	fin_dport	fin_dat.fid_16[1]
#define	fin_ports	fin_dat.fid_32

#define	IPF_IN	0
#define	IPF_OUT	1

typedef	struct frentry	*(*ipfunc_t) __P((fr_info_t *, u_32_t *));
typedef	int		(*ipfuncinit_t) __P((struct frentry *));

typedef	struct	ipfunc_resolve	{
	char		ipfu_name[32];
	ipfunc_t	ipfu_addr;
	ipfuncinit_t	ipfu_init;
} ipfunc_resolve_t;

/*
 * Size for compares on fr_info structures
 */
#define	FI_CSIZE	offsetof(fr_info_t, fin_icode)
#define	FI_LCSIZE	offsetof(fr_info_t, fin_dp)

/*
 * Size for copying cache fr_info structure
 */
#define	FI_COPYSIZE	offsetof(fr_info_t, fin_dp)

/*
 * Structure for holding IPFilter's tag information
 */
#define	IPFTAG_LEN	16
typedef	struct	{
	union	{
		u_32_t	iptu_num[4];
		char	iptu_tag[IPFTAG_LEN];
	} ipt_un;
	int	ipt_not;
} ipftag_t;

#define	ipt_tag	ipt_un.iptu_tag
#define	ipt_num	ipt_un.iptu_num


/*
 * This structure is used to hold information about the next hop for where
 * to forward a packet.
 */
typedef	struct	frdest	{
	void	*fd_ifp;
	i6addr_t	fd_ip6;
	char	fd_ifname[LIFNAMSIZ];
} frdest_t;

#define	fd_ip	fd_ip6.in4


/*
 * This structure holds information about a port comparison.
 */
typedef	struct	frpcmp	{
	int	frp_cmp;	/* data for port comparisons */
	u_short	frp_port;	/* top port for <> and >< */
	u_short	frp_top;	/* top port for <> and >< */
} frpcmp_t;

#define FR_NONE 0
#define FR_EQUAL 1
#define FR_NEQUAL 2
#define FR_LESST 3
#define FR_GREATERT 4
#define FR_LESSTE 5
#define FR_GREATERTE 6
#define FR_OUTRANGE 7
#define FR_INRANGE 8
#define FR_INCRANGE 9

/*
 * Structure containing all the relevant TCP things that can be checked in
 * a filter rule.
 */
typedef	struct	frtuc	{
	u_char		ftu_tcpfm;	/* tcp flags mask */
	u_char		ftu_tcpf;	/* tcp flags */
	frpcmp_t	ftu_src;
	frpcmp_t	ftu_dst;
} frtuc_t;

#define	ftu_scmp	ftu_src.frp_cmp
#define	ftu_dcmp	ftu_dst.frp_cmp
#define	ftu_sport	ftu_src.frp_port
#define	ftu_dport	ftu_dst.frp_port
#define	ftu_stop	ftu_src.frp_top
#define	ftu_dtop	ftu_dst.frp_top

#define	FR_TCPFMAX	0x3f

/*
 * This structure makes up what is considered to be the IPFilter specific
 * matching components of a filter rule, as opposed to the data structures
 * used to define the result which are in frentry_t and not here.
 */
typedef	struct	fripf	{
	fr_ip_t	fri_ip;
	fr_ip_t	fri_mip;	/* mask structure */

	u_short	fri_icmpm;		/* data for ICMP packets (mask) */
	u_short	fri_icmp;

	frtuc_t	fri_tuc;
	int	fri_satype;		/* addres type */
	int	fri_datype;		/* addres type */
	int	fri_sifpidx;		/* doing dynamic addressing */
	int	fri_difpidx;		/* index into fr_ifps[] to use when */
} fripf_t;

#define	fri_dstnum	fri_ip.fi_dstnum
#define	fri_srcnum	fri_mip.fi_srcnum
#define	fri_dstptr	fri_ip.fi_dstptr
#define	fri_srcptr	fri_mip.fi_srcptr

#define	FRI_NORMAL	0	/* Normal address */
#define	FRI_DYNAMIC	1	/* dynamic address */
#define	FRI_LOOKUP	2	/* address is a pool # */
#define	FRI_RANGE	3	/* address/mask is a range */
#define	FRI_NETWORK	4	/* network address from if */
#define	FRI_BROADCAST	5	/* broadcast address from if */
#define	FRI_PEERADDR	6	/* Peer address for P-to-P */
#define	FRI_NETMASKED	7	/* network address with netmask from if */


typedef	struct	frentry	* (* frentfunc_t) __P((fr_info_t *));

typedef	struct	frentry {
	ipfmutex_t	fr_lock;
	struct	frentry	*fr_next;
	struct	frentry	**fr_grp;
	struct	ipscan	*fr_isc;
	void	*fr_ifas[4];
	void	*fr_ptr;	/* for use with fr_arg */
	char	*fr_comment;	/* text comment for rule */
	int	fr_ref;		/* reference count - for grouping */
	int	fr_statecnt;	/* state count - for limit rules */
	/*
	 * These are only incremented when a packet  matches this rule and
	 * it is the last match
	 */
	U_QUAD_T	fr_hits;
	U_QUAD_T	fr_bytes;

	/*
	 * For PPS rate limiting
	 */
	struct timeval	fr_lastpkt;
	int		fr_curpps;

	union	{
		void		*fru_data;
		caddr_t		fru_caddr;
		fripf_t		*fru_ipf;
		frentfunc_t	fru_func;
	} fr_dun;

	/*
	 * Fields after this may not change whilst in the kernel.
	 */
	ipfunc_t fr_func; 	/* call this function */
	int	fr_dsize;
	int	fr_pps;
	int	fr_statemax;	/* max reference count */
	int	fr_flineno;	/* line number from conf file */
	u_32_t	fr_type;
	u_32_t	fr_flags;	/* per-rule flags && options (see below) */
	u_32_t	fr_logtag;	/* user defined log tag # */
	u_32_t	fr_collect;	/* collection number */
	u_int	fr_arg;		/* misc. numeric arg for rule */ 
	u_int	fr_loglevel;	/* syslog log facility + priority */
	u_int	fr_age[2];	/* non-TCP timeouts */
	u_char	fr_v;
	u_char	fr_icode;	/* return ICMP code */
	char	fr_group[FR_GROUPLEN];	/* group to which this rule belongs */
	char	fr_grhead[FR_GROUPLEN];	/* group # which this rule starts */
	ipftag_t fr_nattag;
	char	fr_ifnames[4][LIFNAMSIZ];
	char	fr_isctag[16];
	frdest_t fr_tifs[2];	/* "to"/"reply-to" interface */
	frdest_t fr_dif;	/* duplicate packet interface */
	/*
	 * This must be last and will change after loaded into the kernel.
	 */
	u_int	fr_cksum;	/* checksum on filter rules for performance */
} frentry_t;

#define	fr_caddr	fr_dun.fru_caddr
#define	fr_data		fr_dun.fru_data
#define	fr_dfunc	fr_dun.fru_func
#define	fr_ipf		fr_dun.fru_ipf
#define	fr_ip		fr_ipf->fri_ip
#define	fr_mip		fr_ipf->fri_mip
#define	fr_icmpm	fr_ipf->fri_icmpm
#define	fr_icmp		fr_ipf->fri_icmp
#define	fr_tuc		fr_ipf->fri_tuc
#define	fr_satype	fr_ipf->fri_satype
#define	fr_datype	fr_ipf->fri_datype
#define	fr_sifpidx	fr_ipf->fri_sifpidx
#define	fr_difpidx	fr_ipf->fri_difpidx
#define	fr_proto	fr_ip.fi_p
#define	fr_mproto	fr_mip.fi_p
#define	fr_ttl		fr_ip.fi_ttl
#define	fr_mttl		fr_mip.fi_ttl
#define	fr_tos		fr_ip.fi_tos
#define	fr_mtos		fr_mip.fi_tos
#define	fr_tcpfm	fr_tuc.ftu_tcpfm
#define	fr_tcpf		fr_tuc.ftu_tcpf
#define	fr_scmp		fr_tuc.ftu_scmp
#define	fr_dcmp		fr_tuc.ftu_dcmp
#define	fr_dport	fr_tuc.ftu_dport
#define	fr_sport	fr_tuc.ftu_sport
#define	fr_stop		fr_tuc.ftu_stop
#define	fr_dtop		fr_tuc.ftu_dtop
#define	fr_dst		fr_ip.fi_dst.in4
#define	fr_daddr	fr_ip.fi_dst.in4.s_addr
#define	fr_src		fr_ip.fi_src.in4
#define	fr_saddr	fr_ip.fi_src.in4.s_addr
#define	fr_dmsk		fr_mip.fi_dst.in4
#define	fr_dmask	fr_mip.fi_dst.in4.s_addr
#define	fr_smsk		fr_mip.fi_src.in4
#define	fr_smask	fr_mip.fi_src.in4.s_addr
#define	fr_dstnum	fr_ip.fi_dstnum
#define	fr_srcnum	fr_ip.fi_srcnum
#define	fr_dsttype	fr_ip.fi_dsttype
#define	fr_srctype	fr_ip.fi_srctype
#define	fr_dstptr	fr_mip.fi_dstptr
#define	fr_srcptr	fr_mip.fi_srcptr
#define	fr_dstfunc	fr_mip.fi_dstfunc
#define	fr_srcfunc	fr_mip.fi_srcfunc
#define	fr_optbits	fr_ip.fi_optmsk
#define	fr_optmask	fr_mip.fi_optmsk
#define	fr_secbits	fr_ip.fi_secmsk
#define	fr_secmask	fr_mip.fi_secmsk
#define	fr_authbits	fr_ip.fi_auth
#define	fr_authmask	fr_mip.fi_auth
#define	fr_flx		fr_ip.fi_flx
#define	fr_mflx		fr_mip.fi_flx
#define	fr_ifname	fr_ifnames[0]
#define	fr_oifname	fr_ifnames[2]
#define	fr_ifa		fr_ifas[0]
#define	fr_oifa		fr_ifas[2]
#define	fr_tif		fr_tifs[0]
#define	fr_rif		fr_tifs[1]

#define	FR_NOLOGTAG	0

#ifndef	offsetof
#define	offsetof(t,m)	(int)((&((t *)0L)->m))
#endif
#define	FR_CMPSIZ	(sizeof(struct frentry) - \
			 offsetof(struct frentry, fr_func))

/*
 * fr_type
 */
#define	FR_T_NONE	0
#define	FR_T_IPF	1	/* IPF structures */
#define	FR_T_BPFOPC	2	/* BPF opcode */
#define	FR_T_CALLFUNC	3	/* callout to function in fr_func only */
#define	FR_T_COMPIPF	4	/* compiled C code */
#define	FR_T_BUILTIN	0x80000000	/* rule is in kernel space */

/*
 * fr_flags
 */
#define	FR_CALL		0x00000	/* call rule */
#define	FR_BLOCK	0x00001	/* do not allow packet to pass */
#define	FR_PASS		0x00002	/* allow packet to pass */
#define	FR_AUTH		0x00003	/* use authentication */
#define	FR_PREAUTH	0x00004	/* require preauthentication */
#define	FR_ACCOUNT	0x00005	/* Accounting rule */
#define	FR_SKIP		0x00006	/* skip rule */
#define	FR_DIVERT	0x00007	/* divert rule */
#define	FR_CMDMASK	0x0000f
#define	FR_LOG		0x00010	/* Log */
#define	FR_LOGB		0x00011	/* Log-fail */
#define	FR_LOGP		0x00012	/* Log-pass */
#define	FR_LOGMASK	(FR_LOG|FR_CMDMASK)
#define	FR_CALLNOW	0x00020	/* call another function (fr_func) if matches */
#define	FR_NOTSRCIP	0x00040
#define	FR_NOTDSTIP	0x00080
#define	FR_QUICK	0x00100	/* match & stop processing list */
#define	FR_KEEPFRAG	0x00200	/* keep fragment information */
#define	FR_KEEPSTATE	0x00400	/* keep `connection' state information */
#define	FR_FASTROUTE	0x00800	/* bypass normal routing */
#define	FR_RETRST	0x01000	/* Return TCP RST packet - reset connection */
#define	FR_RETICMP	0x02000	/* Return ICMP unreachable packet */
#define	FR_FAKEICMP	0x03000	/* Return ICMP unreachable with fake source */
#define	FR_OUTQUE	0x04000	/* outgoing packets */
#define	FR_INQUE	0x08000	/* ingoing packets */
#define	FR_LOGBODY	0x10000	/* Log the body */
#define	FR_LOGFIRST	0x20000	/* Log the first byte if state held */
#define	FR_LOGORBLOCK	0x40000	/* block the packet if it can't be logged */
#define	FR_DUP		0x80000	/* duplicate packet */
#define	FR_FRSTRICT	0x100000	/* strict frag. cache */
#define	FR_STSTRICT	0x200000	/* strict keep state */
#define	FR_NEWISN	0x400000	/* new ISN for outgoing TCP */
#define	FR_NOICMPERR	0x800000	/* do not match ICMP errors in state */
#define	FR_STATESYNC	0x1000000	/* synchronize state to slave */
#define	FR_NOMATCH	0x8000000	/* no match occured */
		/*	0x10000000 	FF_LOGPASS */
		/*	0x20000000 	FF_LOGBLOCK */
		/*	0x40000000 	FF_LOGNOMATCH */
		/*	0x80000000 	FF_BLOCKNONIP */
#define	FR_COPIED	0x40000000	/* copied from user space */
#define	FR_INACTIVE	0x80000000	/* only used when flush'ing rules */

#define	FR_RETMASK	(FR_RETICMP|FR_RETRST|FR_FAKEICMP)
#define	FR_ISBLOCK(x)	(((x) & FR_CMDMASK) == FR_BLOCK)
#define	FR_ISPASS(x)	(((x) & FR_CMDMASK) == FR_PASS)
#define	FR_ISAUTH(x)	(((x) & FR_CMDMASK) == FR_AUTH)
#define	FR_ISPREAUTH(x)	(((x) & FR_CMDMASK) == FR_PREAUTH)
#define	FR_ISACCOUNT(x)	(((x) & FR_CMDMASK) == FR_ACCOUNT)
#define	FR_ISSKIP(x)	(((x) & FR_CMDMASK) == FR_SKIP)
#define	FR_ISNOMATCH(x)	((x) & FR_NOMATCH)
#define	FR_INOUT	(FR_INQUE|FR_OUTQUE)

/*
 * recognized flags for SIOCGETFF and SIOCSETFF, and get put in fr_flags
 */
#define	FF_LOGPASS	0x10000000
#define	FF_LOGBLOCK	0x20000000
#define	FF_LOGNOMATCH	0x40000000
#define	FF_LOGGING	(FF_LOGPASS|FF_LOGBLOCK|FF_LOGNOMATCH)
#define	FF_BLOCKNONIP	0x80000000	/* Solaris2 Only */


/*
 * Structure that passes information on what/how to flush to the kernel.
 */
typedef	struct	ipfflush	{
	int	ipflu_how;
	int	ipflu_arg;
} ipfflush_t;


/*
 *
 */
typedef	struct	ipfgetctl	{
	u_int	ipfg_min;	/* min value */
	u_int	ipfg_current;	/* current value */
	u_int	ipfg_max;	/* max value */
	u_int	ipfg_default;	/* default value */
	u_int	ipfg_steps;	/* value increments */
	char	ipfg_name[40];	/* tag name for this control */
} ipfgetctl_t;

typedef	struct	ipfsetctl	{
	int	ipfs_which;	/* 0 = min 1 = current 2 = max 3 = default */
	u_int	ipfs_value;	/* min value */
	char	ipfs_name[40];	/* tag name for this control */
} ipfsetctl_t;


/*
 * Some of the statistics below are in their own counters, but most are kept
 * in this single structure so that they can all easily be collected and
 * copied back as required.
 */
typedef	struct	filterstats {
	u_long	fr_pass;	/* packets allowed */
	u_long	fr_block;	/* packets denied */
	u_long	fr_nom;		/* packets which don't match any rule */
	u_long	fr_short;	/* packets which are short */
	u_long	fr_ppkl;	/* packets allowed and logged */
	u_long	fr_bpkl;	/* packets denied and logged */
	u_long	fr_npkl;	/* packets unmatched and logged */
	u_long	fr_pkl;		/* packets logged */
	u_long	fr_skip;	/* packets to be logged but buffer full */
	u_long	fr_ret;		/* packets for which a return is sent */
	u_long	fr_acct;	/* packets for which counting was performed */
	u_long	fr_bnfr;	/* bad attempts to allocate fragment state */
	u_long	fr_nfr;		/* new fragment state kept */
	u_long	fr_cfr;		/* add new fragment state but complete pkt */
	u_long	fr_bads;	/* bad attempts to allocate packet state */
	u_long	fr_ads;		/* new packet state kept */
	u_long	fr_chit;	/* cached hit */
	u_long	fr_tcpbad;	/* TCP checksum check failures */
	u_long	fr_pull[2];	/* good and bad pullup attempts */
	u_long	fr_badsrc;	/* source received doesn't match route */
	u_long	fr_badttl;	/* TTL in packet doesn't reach minimum */
	u_long	fr_bad;		/* bad IP packets to the filter */
	u_long	fr_ipv6;	/* IPv6 packets in/out */
	u_long	fr_ppshit;	/* dropped because of pps ceiling */
	u_long	fr_ipud;	/* IP id update failures */
} filterstats_t;

/*
 * Log structure.  Each packet header logged is prepended by one of these.
 * Following this in the log records read from the device will be an ipflog
 * structure which is then followed by any packet data.
 */
typedef	struct	iplog	{
	u_32_t		ipl_magic;
	u_int		ipl_count;
	struct	timeval	ipl_time;
	size_t		ipl_dsize;
	struct	iplog	*ipl_next;
} iplog_t;

#define	ipl_sec		ipl_time.tv_sec
#define	ipl_usec	ipl_time.tv_usec

#define IPL_MAGIC	0x49504c4d	/* 'IPLM' */
#define IPL_MAGIC_NAT	0x49504c4e	/* 'IPLN' */
#define IPL_MAGIC_STATE	0x49504c53	/* 'IPLS' */
#define	IPLOG_SIZE	sizeof(iplog_t)

typedef	struct	ipflog	{
#if (defined(NetBSD) && (NetBSD <= 1991011) && (NetBSD >= 199603)) || \
        (defined(OpenBSD) && (OpenBSD >= 199603))
#else
	u_int	fl_unit;
#endif
	u_32_t	fl_rule;
	u_32_t	fl_flags;
	u_32_t	fl_lflags;
	u_32_t	fl_logtag;
	ipftag_t	fl_nattag;
	u_short	fl_plen;	/* extra data after hlen */
	u_short	fl_loglevel;	/* syslog log level */
	char	fl_group[FR_GROUPLEN];
	u_char	fl_hlen;	/* length of IP headers saved */
	u_char	fl_dir;
	u_char	fl_xxx[2];	/* pad */
	char	fl_ifname[LIFNAMSIZ];
} ipflog_t;

#ifndef	IPF_LOGGING
# define	IPF_LOGGING	0
#endif
#ifndef	IPF_DEFAULT_PASS
# define	IPF_DEFAULT_PASS	FR_PASS
#endif

#define	DEFAULT_IPFLOGSIZE	8192
#ifndef	IPFILTER_LOGSIZE
# define	IPFILTER_LOGSIZE	DEFAULT_IPFLOGSIZE
#else
# if IPFILTER_LOGSIZE < DEFAULT_IPFLOGSIZE
#  error IPFILTER_LOGSIZE too small.  Must be >= DEFAULT_IPFLOGSIZE
# endif
#endif

#define	IPF_OPTCOPY	0x07ff00	/* bit mask of copied options */

/*
 * Device filenames for reading log information.  Use ipf on Solaris2 because
 * ipl is already a name used by something else.
 */
#ifndef	IPL_NAME
# if	SOLARIS
#  define	IPL_NAME	"/dev/ipf"
# else
#  define	IPL_NAME	"/dev/ipl"
# endif
#endif
/*
 * Pathnames for various IP Filter control devices.  Used by LKM
 * and userland, so defined here.
 */
#define	IPNAT_NAME	"/dev/ipnat"
#define	IPSTATE_NAME	"/dev/ipstate"
#define	IPAUTH_NAME	"/dev/ipauth"
#define	IPSYNC_NAME	"/dev/ipsync"
#define	IPSCAN_NAME	"/dev/ipscan"
#define	IPLOOKUP_NAME	"/dev/iplookup"

#define	IPL_LOGIPF	0	/* Minor device #'s for accessing logs */
#define	IPL_LOGNAT	1
#define	IPL_LOGSTATE	2
#define	IPL_LOGAUTH	3
#define	IPL_LOGSYNC	4
#define	IPL_LOGSCAN	5
#define	IPL_LOGLOOKUP	6
#define	IPL_LOGCOUNT	7
#define	IPL_LOGMAX	7
#define	IPL_LOGSIZE	IPL_LOGMAX + 1
#define	IPL_LOGALL	-1
#define	IPL_LOGNONE	-2

/*
 * For SIOCGETFS
 */
typedef	struct	friostat	{
	struct	filterstats	f_st[2];
	struct	frentry		*f_ipf[2][2];
	struct	frentry		*f_acct[2][2];
	struct	frentry		*f_ipf6[2][2];
	struct	frentry		*f_acct6[2][2];
	struct	frentry		*f_auth;
	struct	frgroup		*f_groups[IPL_LOGSIZE][2];
	u_long	f_froute[2];
	u_long	f_ticks;
	int	f_locks[IPL_LOGMAX];
	size_t	f_kmutex_sz;
	size_t	f_krwlock_sz;
	int	f_defpass;	/* default pass - from fr_pass */
	int	f_active;	/* 1 or 0 - active rule set */
	int	f_running;	/* 1 if running, else 0 */
	int	f_logging;	/* 1 if enabled, else 0 */
	int	f_features;
	char	f_version[32];	/* version string */
} friostat_t;

#define	f_fin		f_ipf[0]
#define	f_fin6		f_ipf6[0]
#define	f_fout		f_ipf[1]
#define	f_fout6		f_ipf6[1]
#define	f_acctin	f_acct[0]
#define	f_acctin6	f_acct6[0]
#define	f_acctout	f_acct[1]
#define	f_acctout6	f_acct6[1]

#define	IPF_FEAT_LKM		0x001
#define	IPF_FEAT_LOG		0x002
#define	IPF_FEAT_LOOKUP		0x004
#define	IPF_FEAT_BPF		0x008
#define	IPF_FEAT_COMPILED	0x010
#define	IPF_FEAT_CKSUM		0x020
#define	IPF_FEAT_SYNC		0x040
#define	IPF_FEAT_SCAN		0x080
#define	IPF_FEAT_IPV6		0x100

typedef struct	optlist {
	u_short ol_val;
	int	ol_bit;
} optlist_t;


/*
 * Group list structure.
 */
typedef	struct frgroup {
	struct	frgroup	*fg_next;
	struct	frentry	*fg_head;
	struct	frentry	*fg_start;
	u_32_t	fg_flags;
	int	fg_ref;
	char	fg_name[FR_GROUPLEN];
} frgroup_t;

#define	FG_NAME(g)	(*(g)->fg_name == '\0' ? "" : (g)->fg_name)


/*
 * Used by state and NAT tables
 */
typedef struct icmpinfo {
	u_short	ici_id;
	u_short	ici_seq;
	u_char	ici_type;
} icmpinfo_t;

typedef struct udpinfo {
	u_short	us_sport;
	u_short	us_dport;
} udpinfo_t;


typedef	struct	tcpdata	{
	u_32_t	td_end;
	u_32_t	td_maxend;
	u_32_t	td_maxwin;
	u_32_t	td_winscale;
	u_32_t	td_maxseg;
	int	td_winflags;
} tcpdata_t;

#define	TCP_WSCALE_MAX		14

#define	TCP_WSCALE_SEEN		0x00000001
#define	TCP_WSCALE_FIRST	0x00000002


typedef	struct tcpinfo {
	u_short	ts_sport;
	u_short	ts_dport;
	tcpdata_t ts_data[2];
} tcpinfo_t;


struct	grebits	{
	u_32_t	grb_C:1;
	u_32_t	grb_R:1;
	u_32_t	grb_K:1;
	u_32_t	grb_S:1;
	u_32_t	grb_s:1;
	u_32_t	grb_recur:1;
	u_32_t	grb_A:1;
	u_32_t	grb_flags:3;
	u_32_t	grb_ver:3;
	u_short	grb_ptype;
};

typedef	struct	grehdr	{
	union	{
		struct	grebits	gru_bits;
		u_short	gru_flags;
	} gr_un;
	u_short	gr_len;
	u_short	gr_call;
} grehdr_t;

#define	gr_flags	gr_un.gru_flags
#define	gr_bits		gr_un.gru_bits
#define	gr_ptype	gr_bits.grb_ptype
#define	gr_C		gr_bits.grb_C
#define	gr_R		gr_bits.grb_R
#define	gr_K		gr_bits.grb_K
#define	gr_S		gr_bits.grb_S
#define	gr_s		gr_bits.grb_s
#define	gr_recur	gr_bits.grb_recur
#define	gr_A		gr_bits.grb_A
#define	gr_ver		gr_bits.grb_ver


typedef	struct	greinfo	{
	u_short	gs_call[2];
	u_short	gs_flags;
	u_short	gs_ptype;
} greinfo_t;

#define	GRE_REV(x)	((ntohs(x) >> 13) & 7)


/*
 * Timeout tail queue list member
 */
typedef	struct	ipftqent	{
	struct ipftqent **tqe_pnext;
	struct ipftqent *tqe_next;
	struct	ipftq	*tqe_ifq;
	void	*tqe_parent;		/* pointer back to NAT/state struct */
	u_long	tqe_die;		/* when this entriy is to die */
	u_long	tqe_touched;
	int	tqe_flags;
	int	tqe_state[2];		/* current state of this entry */
} ipftqent_t;

#define	TQE_RULEBASED	0x00000001


/*
 * Timeout tail queue head for IPFilter
 */
typedef struct  ipftq   {
	ipfmutex_t	ifq_lock;
	u_int	ifq_ttl;
	ipftqent_t	*ifq_head;
	ipftqent_t	**ifq_tail;
	struct	ipftq	*ifq_next;
	struct	ipftq	**ifq_pnext;
	int	ifq_ref;
	u_int	ifq_flags;
} ipftq_t;

#define	IFQF_USER	0x01		/* User defined aging */
#define	IFQF_DELETE	0x02		/* Marked for deletion */
#define	IFQF_PROXY	0x04		/* Timeout queue in use by a proxy */

#define	IPF_HZ_MULT	1
#define	IPF_HZ_DIVIDE	2		/* How many times a second ipfilter */
					/* checks its timeout queues.       */
#define	IPF_TTLVAL(x)	(((x) / IPF_HZ_MULT) * IPF_HZ_DIVIDE)

/*
 * Structure to define address for pool lookups.
 */
typedef	struct	{
	u_char		adf_len;
	i6addr_t	adf_addr;
} addrfamily_t;


/*
 * Object structure description.  For passing through in ioctls.
 */
typedef	struct	ipfobj	{
	u_32_t	ipfo_rev;		/* IPFilter version number */
	u_32_t	ipfo_size;		/* size of object at ipfo_ptr */
	void	*ipfo_ptr;		/* pointer to object */
	int	ipfo_type;		/* type of object being pointed to */
	int	ipfo_offset;		/* bytes from ipfo_ptr where to start */
	u_char	ipfo_xxxpad[32];	/* reserved for future use */
} ipfobj_t;

#define	IPFOBJ_FRENTRY		0	/* struct frentry */
#define	IPFOBJ_IPFSTAT		1	/* struct friostat */
#define	IPFOBJ_IPFINFO		2	/* struct fr_info */
#define	IPFOBJ_AUTHSTAT		3	/* struct fr_authstat */
#define	IPFOBJ_FRAGSTAT		4	/* struct ipfrstat */
#define	IPFOBJ_IPNAT		5	/* struct ipnat */
#define	IPFOBJ_NATSTAT		6	/* struct natstat */
#define	IPFOBJ_STATESAVE	7	/* struct ipstate_save */
#define	IPFOBJ_NATSAVE		8	/* struct nat_save */
#define	IPFOBJ_NATLOOKUP	9	/* struct natlookup */
#define	IPFOBJ_IPSTATE		10	/* struct ipstate */
#define	IPFOBJ_STATESTAT	11	/* struct ips_stat */
#define	IPFOBJ_FRAUTH		12	/* struct frauth */
#define	IPFOBJ_TUNEABLE		13	/* struct ipftune */


typedef	union	ipftunevalptr	{
	void	*ipftp_void;
	u_long	*ipftp_long;
	u_int	*ipftp_int;
	u_short	*ipftp_short;
	u_char	*ipftp_char;
} ipftunevalptr_t;

typedef	struct	ipftuneable	{
	ipftunevalptr_t	ipft_una;
	char		*ipft_name;
	u_long		ipft_min;
	u_long		ipft_max;
	int		ipft_sz;
	int		ipft_flags;
	struct ipftuneable *ipft_next;
} ipftuneable_t;

#define	ipft_addr	ipft_una.ipftp_void
#define	ipft_plong	ipft_una.ipftp_long
#define	ipft_pint	ipft_una.ipftp_int
#define	ipft_pshort	ipft_una.ipftp_short
#define	ipft_pchar	ipft_una.ipftp_char

#define	IPFT_RDONLY	1	/* read-only */
#define	IPFT_WRDISABLED	2	/* write when disabled only */

typedef	union	ipftuneval	{
	u_long	ipftu_long;
	u_int	ipftu_int;
	u_short	ipftu_short;
	u_char	ipftu_char;
} ipftuneval_t;

typedef	struct	ipftune	{
	void    	*ipft_cookie;
	ipftuneval_t	ipft_un;
	u_long  	ipft_min;
	u_long  	ipft_max;
	int		ipft_sz;
	int		ipft_flags;
	char		ipft_name[80];
} ipftune_t;

#define	ipft_vlong	ipft_un.ipftu_long
#define	ipft_vint	ipft_un.ipftu_int
#define	ipft_vshort	ipft_un.ipftu_short
#define	ipft_vchar	ipft_un.ipftu_char


/*
** HPUX Port
*/
#ifdef __hpux
/* HP-UX locking sequence deadlock detection module lock MAJOR ID */
# define	IPF_SMAJ	0	/* temp assignment XXX, not critical */
#endif

#if !defined(CDEV_MAJOR) && defined (__FreeBSD_version) && \
    (__FreeBSD_version >= 220000)
# define	CDEV_MAJOR	79
#endif

/*
 * Post NetBSD 1.2 has the PFIL interface for packet filters.  This turns
 * on those hooks.  We don't need any special mods in non-IP Filter code
 * with this!
 */
#if (defined(NetBSD) && (NetBSD > 199609) && (NetBSD <= 1991011)) || \
    (defined(NetBSD1_2) && NetBSD1_2 > 1) || \
    (defined(__FreeBSD__) && (__FreeBSD_version >= 500043))
# if (NetBSD >= 199905)
#  define PFIL_HOOKS
# endif
# ifdef PFIL_HOOKS
#  define NETBSD_PF
# endif
#endif

#ifndef	_KERNEL
extern	int	fr_check __P((struct ip *, int, void *, int, mb_t **));
extern	int	(*fr_checkp) __P((ip_t *, int, void *, int, mb_t **));
extern	int	ipf_log __P((void));
extern	struct	ifnet *get_unit __P((char *, int));
extern	char	*get_ifname __P((struct ifnet *));
# if defined(__NetBSD__) || defined(__OpenBSD__) || \
	  (_BSDI_VERSION >= 199701) || (__FreeBSD_version >= 300000)
extern	int	iplioctl __P((int, ioctlcmd_t, caddr_t, int));
# else
extern	int	iplioctl __P((int, ioctlcmd_t, caddr_t, int));
# endif
extern	int	iplopen __P((dev_t, int));
extern	int	iplclose __P((dev_t, int));
extern	void	m_freem __P((mb_t *));
#else /* #ifndef _KERNEL */
# if defined(__NetBSD__) && defined(PFIL_HOOKS)
extern	void	ipfilterattach __P((int));
# endif
extern	int	ipl_enable __P((void));
extern	int	ipl_disable __P((void));
# ifdef MENTAT
extern	int	fr_check __P((struct ip *, int, void *, int, void *,
			      mblk_t **));
#  if SOLARIS
#   if SOLARIS2 >= 7
extern	int	iplioctl __P((dev_t, int, intptr_t, int, cred_t *, int *));
#   else
extern	int	iplioctl __P((dev_t, int, int *, int, cred_t *, int *));
#   endif
extern	int	iplopen __P((dev_t *, int, int, cred_t *));
extern	int	iplclose __P((dev_t, int, int, cred_t *));
extern	int	iplread __P((dev_t, uio_t *, cred_t *));
extern	int	iplwrite __P((dev_t, uio_t *, cred_t *));
#  endif
#  ifdef __hpux
extern	int	iplopen __P((dev_t, int, intptr_t, int));
extern	int	iplclose __P((dev_t, int, int));
extern	int	iplioctl __P((dev_t, int, caddr_t, int));
extern	int	iplread __P((dev_t, uio_t *));
extern	int	iplwrite __P((dev_t, uio_t *));
extern	int	iplselect __P((dev_t, int));
#  endif
extern	int	ipfsync __P((void));
extern	int	fr_qout __P((queue_t *, mblk_t *));
# else /* MENTAT */
extern	int	fr_check __P((struct ip *, int, void *, int, mb_t **));
extern	int	(*fr_checkp) __P((ip_t *, int, void *, int, mb_t **));
extern	size_t	mbufchainlen __P((mb_t *));
#  ifdef	__sgi
#   include <sys/cred.h>
extern	int	iplioctl __P((dev_t, int, caddr_t, int, cred_t *, int *));
extern	int	iplopen __P((dev_t *, int, int, cred_t *));
extern	int	iplclose __P((dev_t, int, int, cred_t *));
extern	int	iplread __P((dev_t, uio_t *, cred_t *));
extern	int	iplwrite __P((dev_t, uio_t *, cred_t *));
extern	int	ipfsync __P((void));
extern	int	ipfilter_sgi_attach __P((void));
extern	void	ipfilter_sgi_detach __P((void));
extern	void	ipfilter_sgi_intfsync __P((void));
#  else
#   ifdef	IPFILTER_LKM
extern	int	iplidentify __P((char *));
#   endif
#   if (_BSDI_VERSION >= 199510) || (__FreeBSD_version >= 220000) || \
      (NetBSD >= 199511) || defined(__OpenBSD__)
#    if defined(__NetBSD__) || (_BSDI_VERSION >= 199701) || \
       defined(__OpenBSD__) || (__FreeBSD_version >= 300000)
#     if (__FreeBSD_version >= 500024)
#      if (__FreeBSD_version >= 502116)
extern	int	iplioctl __P((struct cdev*, u_long, caddr_t, int, struct thread *));
#      else
extern	int	iplioctl __P((dev_t, u_long, caddr_t, int, struct thread *));
#      endif /* __FreeBSD_version >= 502116 */
#     else
extern	int	iplioctl __P((dev_t, u_long, caddr_t, int, struct proc *));
#     endif /* __FreeBSD_version >= 500024 */
#    else
extern	int	iplioctl __P((dev_t, int, caddr_t, int, struct proc *));
#    endif
#    if (__FreeBSD_version >= 500024)
#      if (__FreeBSD_version >= 502116)
extern	int	iplopen __P((struct cdev*, int, int, struct thread *));
extern	int	iplclose __P((struct cdev*, int, int, struct thread *));
#      else
extern	int	iplopen __P((dev_t, int, int, struct thread *));
extern	int	iplclose __P((dev_t, int, int, struct thread *));
#      endif /* __FreeBSD_version >= 502116 */
#    else
extern	int	iplopen __P((dev_t, int, int, struct proc *));
extern	int	iplclose __P((dev_t, int, int, struct proc *));
#    endif /* __FreeBSD_version >= 500024 */
#   else
#    ifdef linux
extern	int	iplioctl __P((struct inode *, struct file *, u_int, u_long));
#    else
extern	int	iplopen __P((dev_t, int));
extern	int	iplclose __P((dev_t, int));
extern	int	iplioctl __P((dev_t, int, caddr_t, int));
#    endif
#   endif /* (_BSDI_VERSION >= 199510) */
#   if	BSD >= 199306
#      if (__FreeBSD_version >= 502116)
extern	int	iplread __P((struct cdev*, struct uio *, int));
extern	int	iplwrite __P((struct cdev*, struct uio *, int));
#      else
extern	int	iplread __P((dev_t, struct uio *, int));
extern	int	iplwrite __P((dev_t, struct uio *, int));
#      endif /* __FreeBSD_version >= 502116 */
#   else
#    ifndef linux
extern	int	iplread __P((dev_t, struct uio *));
extern	int	iplwrite __P((dev_t, struct uio *));
#    endif
#   endif /* BSD >= 199306 */
#  endif /* __ sgi */
# endif /* MENTAT */

#endif /* #ifndef _KERNEL */

extern	ipfmutex_t	ipl_mutex, ipf_authmx, ipf_rw, ipf_hostmap;
extern	ipfmutex_t	ipf_timeoutlock, ipf_stinsert, ipf_natio, ipf_nat_new;
extern	ipfrwlock_t	ipf_mutex, ipf_global, ip_poolrw, ipf_ipidfrag;
extern	ipfrwlock_t	ipf_frag, ipf_state, ipf_nat, ipf_natfrag, ipf_auth;

extern	char	*memstr __P((char *, char *, int, int));
extern	int	count4bits __P((u_32_t));
extern	int	frrequest __P((int, ioctlcmd_t, caddr_t, int, int));
extern	char	*getifname __P((struct ifnet *));
extern	int	iplattach __P((void));
extern	int	ipldetach __P((void));
extern	u_short	ipf_cksum __P((u_short *, int));
extern	int	copyinptr __P((void *, void *, size_t));
extern	int	copyoutptr __P((void *, void *, size_t));
extern	int	fr_fastroute __P((mb_t *, mb_t **, fr_info_t *, frdest_t *));
extern	int	fr_inobj __P((void *, void *, int));
extern	int	fr_inobjsz __P((void *, void *, int, int));
extern	int	fr_ioctlswitch __P((int, void *, ioctlcmd_t, int));
extern	int	fr_ipftune __P((ioctlcmd_t, void *));
extern	int	fr_outobj __P((void *, void *, int));
extern	int	fr_outobjsz __P((void *, void *, int, int));
extern	void	*fr_pullup __P((mb_t *, fr_info_t *, int));
extern	void	fr_resolvedest __P((struct frdest *, int));
extern	int	fr_resolvefunc __P((void *));
extern	void	*fr_resolvenic __P((char *, int));
extern	int	fr_send_icmp_err __P((int, fr_info_t *, int));
extern	int	fr_send_reset __P((fr_info_t *));
#if  (__FreeBSD_version < 490000) || !defined(_KERNEL)
extern	int	ppsratecheck __P((struct timeval *, int *, int));
#endif
extern	ipftq_t	*fr_addtimeoutqueue __P((ipftq_t **, u_int));
extern	void	fr_deletequeueentry __P((ipftqent_t *));
extern	int	fr_deletetimeoutqueue __P((ipftq_t *));
extern	void	fr_freetimeoutqueue __P((ipftq_t *));
extern	void	fr_movequeue __P((ipftqent_t *, ipftq_t *, ipftq_t *));
extern	void	fr_queueappend __P((ipftqent_t *, ipftq_t *, void *));
extern	void	fr_queueback __P((ipftqent_t *));
extern	void	fr_queuefront __P((ipftqent_t *));
extern	void	fr_checkv4sum __P((fr_info_t *));
extern	int	fr_checkl4sum __P((fr_info_t *));
extern	int	fr_ifpfillv4addr __P((int, struct sockaddr_in *,
				      struct sockaddr_in *, struct in_addr *,
				      struct in_addr *));
extern	int	fr_coalesce __P((fr_info_t *));
#ifdef	USE_INET6
extern	void	fr_checkv6sum __P((fr_info_t *));
extern	int	fr_ifpfillv6addr __P((int, struct sockaddr_in6 *,
				      struct sockaddr_in6 *, struct in_addr *,
				      struct in_addr *));
#endif

extern	int		fr_addipftune __P((ipftuneable_t *));
extern	int		fr_delipftune __P((ipftuneable_t *));

extern	int	frflush __P((minor_t, int, int));
extern	void	frsync __P((void *));
extern	frgroup_t *fr_addgroup __P((char *, void *, u_32_t, minor_t, int));
extern	int	fr_derefrule __P((frentry_t **));
extern	void	fr_delgroup __P((char *, minor_t, int));
extern	frgroup_t *fr_findgroup __P((char *, minor_t, int, frgroup_t ***));

extern	int	fr_loginit __P((void));
extern	int	ipflog_clear __P((minor_t));
extern	int	ipflog_read __P((minor_t, uio_t *));
extern	int	ipflog __P((fr_info_t *, u_int));
extern	int	ipllog __P((int, fr_info_t *, void **, size_t *, int *, int));
extern	void	fr_logunload __P((void));

extern	frentry_t	*fr_acctpkt __P((fr_info_t *, u_32_t *));
extern	int		fr_copytolog __P((int, char *, int));
extern	u_short		fr_cksum __P((mb_t *, ip_t *, int, void *));
extern	void		fr_deinitialise __P((void));
extern	frentry_t 	*fr_dolog __P((fr_info_t *, u_32_t *));
extern	frentry_t 	*fr_dstgrpmap __P((fr_info_t *, u_32_t *));
extern	void		fr_fixskip __P((frentry_t **, frentry_t *, int));
extern	void		fr_forgetifp __P((void *));
extern	frentry_t 	*fr_getrulen __P((int, char *, u_32_t));
extern	void		fr_getstat __P((struct friostat *));
extern	int		fr_icmp4errortype __P((int));
extern	int		fr_ifpaddr __P((int, int, void *,
				struct in_addr *, struct in_addr *));
extern	int		fr_initialise __P((void));
extern	void		fr_lock __P((caddr_t, int *));
extern  int		fr_makefrip __P((int, ip_t *, fr_info_t *));
extern	int		fr_matchtag __P((ipftag_t *, ipftag_t *));
extern	int		fr_matchicmpqueryreply __P((int, icmpinfo_t *,
					    struct icmp *, int));
extern	u_32_t		fr_newisn __P((fr_info_t *));
extern	u_short		fr_nextipid __P((fr_info_t *));
extern	int		fr_rulen __P((int, frentry_t *));
extern	int		fr_scanlist __P((fr_info_t *, u_32_t));
extern	frentry_t 	*fr_srcgrpmap __P((fr_info_t *, u_32_t *));
extern	int		fr_tcpudpchk __P((fr_info_t *, frtuc_t *));
extern	int		fr_verifysrc __P((fr_info_t *fin));
extern	int		fr_zerostats __P((char *));

extern	int	fr_running;
extern	u_long	fr_frouteok[2];
extern	int	fr_pass;
extern	int	fr_flags;
extern	int	fr_active;
extern	int	fr_chksrc;
extern	int	fr_minttl;
extern	int	fr_refcnt;
extern	int	fr_control_forwarding;
extern	int	fr_update_ipid;
extern	int	nat_logging;
extern	int	ipstate_logging;
extern	int	ipl_suppress;
extern	int	ipl_buffer_sz;
extern	int	ipl_logmax;
extern	int	ipl_logall;
extern	int	ipl_logsize;
extern	u_long	fr_ticks;
extern	fr_info_t	frcache[2][8];
extern	char	ipfilter_version[];
extern	iplog_t	**iplh[IPL_LOGMAX+1], *iplt[IPL_LOGMAX+1];
extern	int	iplused[IPL_LOGMAX + 1];
extern	struct frentry *ipfilter[2][2], *ipacct[2][2];
#ifdef	USE_INET6
extern	struct frentry *ipfilter6[2][2], *ipacct6[2][2];
extern	int	icmptoicmp6types[ICMP_MAXTYPE+1];
extern	int	icmptoicmp6unreach[ICMP_MAX_UNREACH];
extern	int	icmpreplytype6[ICMP6_MAXTYPE + 1];
#endif
extern	int	icmpreplytype4[ICMP_MAXTYPE + 1];
extern	struct frgroup *ipfgroups[IPL_LOGSIZE][2];
extern	struct filterstats frstats[];
extern	frentry_t *ipfrule_match __P((fr_info_t *));
extern	u_char	ipf_iss_secret[32];
extern	ipftuneable_t ipf_tuneables[];

#endif	/* __IP_FIL_H__ */
