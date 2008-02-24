/*	$FreeBSD: src/sys/contrib/ipfilter/netinet/ip_nat.h,v 1.26.2.1 2007/10/31 05:00:38 darrenr Exp $	*/

/*
 * Copyright (C) 1995-2001, 2003 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * @(#)ip_nat.h	1.5 2/4/96
 * $FreeBSD: src/sys/contrib/ipfilter/netinet/ip_nat.h,v 1.26.2.1 2007/10/31 05:00:38 darrenr Exp $
 * Id: ip_nat.h,v 2.90.2.20 2007/09/25 08:27:32 darrenr Exp $
 */

#ifndef	__IP_NAT_H__
#define	__IP_NAT_H__

#ifndef SOLARIS
#define	SOLARIS	(defined(sun) && (defined(__svr4__) || defined(__SVR4)))
#endif

#if defined(__STDC__) || defined(__GNUC__) || defined(_AIX51)
#define	SIOCADNAT	_IOW('r', 60, struct ipfobj)
#define	SIOCRMNAT	_IOW('r', 61, struct ipfobj)
#define	SIOCGNATS	_IOWR('r', 62, struct ipfobj)
#define	SIOCGNATL	_IOWR('r', 63, struct ipfobj)
#else
#define	SIOCADNAT	_IOW(r, 60, struct ipfobj)
#define	SIOCRMNAT	_IOW(r, 61, struct ipfobj)
#define	SIOCGNATS	_IOWR(r, 62, struct ipfobj)
#define	SIOCGNATL	_IOWR(r, 63, struct ipfobj)
#endif

#undef	LARGE_NAT	/* define	this if you're setting up a system to NAT
			 * LARGE numbers of networks/hosts - i.e. in the
			 * hundreds or thousands.  In such a case, you should
			 * also change the RDR_SIZE and NAT_SIZE below to more
			 * appropriate sizes.  The figures below were used for
			 * a setup with 1000-2000 networks to NAT.
			 */
#ifndef NAT_SIZE
# ifdef LARGE_NAT
#  define	NAT_SIZE	2047
# else
#  define	NAT_SIZE	127
# endif
#endif
#ifndef RDR_SIZE
# ifdef LARGE_NAT
#  define	RDR_SIZE	2047
# else
#  define	RDR_SIZE	127
# endif
#endif
#ifndef HOSTMAP_SIZE
# ifdef LARGE_NAT
#  define	HOSTMAP_SIZE	8191
# else
#  define	HOSTMAP_SIZE	2047
# endif
#endif
#ifndef NAT_TABLE_MAX
/*
 * This is newly introduced and for the sake of "least surprise", the numbers
 * present aren't what we'd normally use for creating a proper hash table.
 */
# ifdef	LARGE_NAT
#  define	NAT_TABLE_MAX	180000
# else
#  define	NAT_TABLE_MAX	30000
# endif
#endif
#ifndef NAT_TABLE_SZ
# ifdef LARGE_NAT
#  define	NAT_TABLE_SZ	16383
# else
#  define	NAT_TABLE_SZ	2047
# endif
#endif
#ifndef	APR_LABELLEN
#define	APR_LABELLEN	16
#endif
#define	NAT_HW_CKSUM	0x80000000

#define	DEF_NAT_AGE	1200     /* 10 minutes (600 seconds) */

struct ipstate;
struct ap_session;

typedef	struct	nat	{
	ipfmutex_t	nat_lock;
	struct	nat	*nat_next;
	struct	nat	**nat_pnext;
	struct	nat	*nat_hnext[2];
	struct	nat	**nat_phnext[2];
	struct	hostmap	*nat_hm;
	void		*nat_data;
	struct	nat	**nat_me;
	struct	ipstate	*nat_state;
	struct	ap_session	*nat_aps;		/* proxy session */
	frentry_t	*nat_fr;	/* filter rule ptr if appropriate */
	struct	ipnat	*nat_ptr;	/* pointer back to the rule */
	void		*nat_ifps[2];
	void		*nat_sync;
	ipftqent_t	nat_tqe;
	u_32_t		nat_flags;
	u_32_t		nat_sumd[2];	/* ip checksum delta for data segment*/
	u_32_t		nat_ipsumd;	/* ip checksum delta for ip header */
	u_32_t		nat_mssclamp;	/* if != zero clamp MSS to this */
	i6addr_t	nat_inip6;
	i6addr_t	nat_outip6;
	i6addr_t	nat_oip6;		/* other ip */
	U_QUAD_T	nat_pkts[2];
	U_QUAD_T	nat_bytes[2];
	union	{
		udpinfo_t	nat_unu;
		tcpinfo_t	nat_unt;
		icmpinfo_t	nat_uni;
		greinfo_t	nat_ugre;
	} nat_un;
	u_short		nat_oport;		/* other port */
	u_short		nat_use;
	u_char		nat_p;			/* protocol for NAT */
	int		nat_dir;
	int		nat_ref;		/* reference count */
	int		nat_hv[2];
	char		nat_ifnames[2][LIFNAMSIZ];
	int		nat_rev;		/* 0 = forward, 1 = reverse */
	int		nat_redir;		/* copy of in_redir */
	u_32_t		nat_seqnext[2];
} nat_t;

#define	nat_inip	nat_inip6.in4
#define	nat_outip	nat_outip6.in4
#define	nat_oip		nat_oip6.in4
#define	nat_age		nat_tqe.tqe_die
#define	nat_inport	nat_un.nat_unt.ts_sport
#define	nat_outport	nat_un.nat_unt.ts_dport
#define	nat_type	nat_un.nat_uni.ici_type
#define	nat_seq		nat_un.nat_uni.ici_seq
#define	nat_id		nat_un.nat_uni.ici_id
#define	nat_tcpstate	nat_tqe.tqe_state
#define	nat_die		nat_tqe.tqe_die
#define	nat_touched	nat_tqe.tqe_touched

/*
 * Values for nat_dir
 */
#define	NAT_INBOUND	0
#define	NAT_OUTBOUND	1

/*
 * Definitions for nat_flags
 */
#define	NAT_TCP		0x0001	/* IPN_TCP */
#define	NAT_UDP		0x0002	/* IPN_UDP */
#define	NAT_ICMPERR	0x0004	/* IPN_ICMPERR */
#define	NAT_ICMPQUERY	0x0008	/* IPN_ICMPQUERY */
#define	NAT_SEARCH	0x0010
#define	NAT_SLAVE	0x0020	/* Slave connection for a proxy */
#define	NAT_NOTRULEPORT	0x0040	/* Don't use the port # in the NAT rule */

#define	NAT_TCPUDP	(NAT_TCP|NAT_UDP)
#define	NAT_TCPUDPICMP	(NAT_TCP|NAT_UDP|NAT_ICMPERR)
#define	NAT_TCPUDPICMPQ	(NAT_TCP|NAT_UDP|NAT_ICMPQUERY)
#define	NAT_FROMRULE	(NAT_TCP|NAT_UDP)

/* 0x0100 reserved for FI_W_SPORT */
/* 0x0200 reserved for FI_W_DPORT */
/* 0x0400 reserved for FI_W_SADDR */
/* 0x0800 reserved for FI_W_DADDR */
/* 0x1000 reserved for FI_W_NEWFR */
/* 0x2000 reserved for SI_CLONE */
/* 0x4000 reserved for SI_CLONED */
/* 0x8000 reserved for SI_IGNOREPKT */

#define	NAT_DEBUG	0x800000

typedef	struct	ipnat	{
	ipfmutex_t	in_lock;
	struct	ipnat	*in_next;		/* NAT rule list next */
	struct	ipnat	*in_rnext;		/* rdr rule hash next */
	struct	ipnat	**in_prnext;		/* prior rdr next ptr */
	struct	ipnat	*in_mnext;		/* map rule hash next */
	struct	ipnat	**in_pmnext;		/* prior map next ptr */
	struct	ipftq	*in_tqehead[2];
	void		*in_ifps[2];
	void		*in_apr;
	char		*in_comment;
	i6addr_t	in_next6;
	u_long		in_space;
	u_long		in_hits;
	u_int		in_use;
	u_int		in_hv;
	int		in_flineno;		/* conf. file line number */
	u_short		in_pnext;
	u_char		in_v;
	u_char		in_xxx;
	/* From here to the end is covered by IPN_CMPSIZ */
	u_32_t		in_flags;
	u_32_t		in_mssclamp;		/* if != 0 clamp MSS to this */
	u_int		in_age[2];
	int		in_redir;		/* see below for values */
	int		in_p;			/* protocol. */
	i6addr_t	in_in[2];
	i6addr_t	in_out[2];
	i6addr_t	in_src[2];
	frtuc_t		in_tuc;
	u_short		in_port[2];
	u_short		in_ppip;		/* ports per IP. */
	u_short		in_ippip;		/* IP #'s per IP# */
	char		in_ifnames[2][LIFNAMSIZ];
	char		in_plabel[APR_LABELLEN];	/* proxy label. */
	ipftag_t	in_tag;
} ipnat_t;

#define	in_pmin		in_port[0]	/* Also holds static redir port */
#define	in_pmax		in_port[1]
#define	in_nextip	in_next6.in4
#define	in_nip		in_next6.in4.s_addr
#define	in_inip		in_in[0].in4.s_addr
#define	in_inmsk	in_in[1].in4.s_addr
#define	in_outip	in_out[0].in4.s_addr
#define	in_outmsk	in_out[1].in4.s_addr
#define	in_srcip	in_src[0].in4.s_addr
#define	in_srcmsk	in_src[1].in4.s_addr
#define	in_scmp		in_tuc.ftu_scmp
#define	in_dcmp		in_tuc.ftu_dcmp
#define	in_stop		in_tuc.ftu_stop
#define	in_dtop		in_tuc.ftu_dtop
#define	in_sport	in_tuc.ftu_sport
#define	in_dport	in_tuc.ftu_dport

/*
 * Bit definitions for in_flags
 */
#define	IPN_ANY		0x00000
#define	IPN_TCP		0x00001
#define	IPN_UDP		0x00002
#define	IPN_TCPUDP	(IPN_TCP|IPN_UDP)
#define	IPN_ICMPERR	0x00004
#define	IPN_TCPUDPICMP	(IPN_TCP|IPN_UDP|IPN_ICMPERR)
#define	IPN_ICMPQUERY	0x00008
#define	IPN_TCPUDPICMPQ	(IPN_TCP|IPN_UDP|IPN_ICMPQUERY)
#define	IPN_RF		(IPN_TCPUDP|IPN_DELETE|IPN_ICMPERR)
#define	IPN_AUTOPORTMAP	0x00010
#define	IPN_IPRANGE	0x00020
#define	IPN_FILTER	0x00040
#define	IPN_SPLIT	0x00080
#define	IPN_ROUNDR	0x00100
#define	IPN_NOTSRC	0x04000
#define	IPN_NOTDST	0x08000
#define	IPN_DYNSRCIP	0x10000	/* dynamic src IP# */
#define	IPN_DYNDSTIP	0x20000	/* dynamic dst IP# */
#define	IPN_DELETE	0x40000
#define	IPN_STICKY	0x80000
#define	IPN_FRAG	0x100000
#define	IPN_FIXEDDPORT	0x200000
#define	IPN_FINDFORWARD	0x400000
#define	IPN_IN		0x800000
#define	IPN_USERFLAGS	(IPN_TCPUDP|IPN_AUTOPORTMAP|IPN_IPRANGE|IPN_SPLIT|\
			 IPN_ROUNDR|IPN_FILTER|IPN_NOTSRC|IPN_NOTDST|\
			 IPN_FRAG|IPN_STICKY|IPN_FIXEDDPORT|IPN_ICMPQUERY)

/*
 * Values for in_redir
 */
#define	NAT_MAP		0x01
#define	NAT_REDIRECT	0x02
#define	NAT_BIMAP	(NAT_MAP|NAT_REDIRECT)
#define	NAT_MAPBLK	0x04

#define	MAPBLK_MINPORT	1024	/* don't use reserved ports for src port */
#define	USABLE_PORTS	(65536 - MAPBLK_MINPORT)

#define	IPN_CMPSIZ	(sizeof(ipnat_t) - offsetof(ipnat_t, in_flags))

typedef	struct	natlookup {
	struct	in_addr	nl_inip;
	struct	in_addr	nl_outip;
	struct	in_addr	nl_realip;
	int	nl_flags;
	u_short	nl_inport;
	u_short	nl_outport;
	u_short	nl_realport;
} natlookup_t;


typedef struct  nat_save    {
	void	*ipn_next;
	struct	nat	ipn_nat;
	struct	ipnat	ipn_ipnat;
	struct	frentry ipn_fr;
	int	ipn_dsize;
	char	ipn_data[4];
} nat_save_t;

#define	ipn_rule	ipn_nat.nat_fr

typedef	struct	natget	{
	void	*ng_ptr;
	int	ng_sz;
} natget_t;


/*
 * This structure gets used to help NAT sessions keep the same NAT rule (and
 * thus translation for IP address) when:
 * (a) round-robin redirects are in use
 * (b) different IP add
 */
typedef	struct	hostmap	{
	struct	hostmap	*hm_hnext;
	struct	hostmap	**hm_phnext;
	struct	hostmap	*hm_next;
	struct	hostmap	**hm_pnext;
	struct	ipnat	*hm_ipnat;
	struct	in_addr	hm_srcip;
	struct	in_addr	hm_dstip;
	struct	in_addr	hm_mapip;
	u_32_t		hm_port;
	int		hm_ref;
} hostmap_t;


/*
 * Structure used to pass information in to nat_newmap and nat_newrdr.
 */
typedef struct	natinfo	{
	ipnat_t		*nai_np;
	u_32_t		nai_sum1;
	u_32_t		nai_sum2;
	u_32_t		nai_nflags;
	u_32_t		nai_flags;
	struct	in_addr	nai_ip;
	u_short		nai_port;
	u_short		nai_nport;
	u_short		nai_sport;
	u_short		nai_dport;
} natinfo_t;


typedef	struct	natstat	{
	u_long	ns_mapped[2];
	u_long	ns_rules;
	u_long	ns_added;
	u_long	ns_expire;
	u_long	ns_inuse;
	u_long	ns_logged;
	u_long	ns_logfail;
	u_long	ns_memfail;
	u_long	ns_badnat;
	u_long	ns_addtrpnt;
	nat_t	**ns_table[2];
	hostmap_t **ns_maptable;
	ipnat_t	*ns_list;
	void	*ns_apslist;
	u_int	ns_wilds;
	u_int	ns_nattab_sz;
	u_int	ns_nattab_max;
	u_int	ns_rultab_sz;
	u_int	ns_rdrtab_sz;
	u_int	ns_trpntab_sz;
	u_int	ns_hostmap_sz;
	nat_t	*ns_instances;
	hostmap_t *ns_maplist;
	u_long	*ns_bucketlen[2];
	u_long	ns_ticks;
	u_int	ns_orphans;
} natstat_t;

typedef	struct	natlog {
	struct	in_addr	nl_origip;
	struct	in_addr	nl_outip;
	struct	in_addr	nl_inip;
	u_short	nl_origport;
	u_short	nl_outport;
	u_short	nl_inport;
	u_short	nl_type;
	int	nl_rule;
	U_QUAD_T	nl_pkts[2];
	U_QUAD_T	nl_bytes[2];
	u_char	nl_p;
} natlog_t;


#define	NL_NEWMAP	NAT_MAP
#define	NL_NEWRDR	NAT_REDIRECT
#define	NL_NEWBIMAP	NAT_BIMAP
#define	NL_NEWBLOCK	NAT_MAPBLK
#define	NL_DESTROY	0xfffc
#define	NL_CLONE	0xfffd
#define	NL_FLUSH	0xfffe
#define	NL_EXPIRE	0xffff

#define	NAT_HASH_FN(k,l,m)	(((k) + ((k) >> 12) + l) % (m))

#define	LONG_SUM(in)	(((in) & 0xffff) + ((in) >> 16))

#define	CALC_SUMD(s1, s2, sd) { \
			    (s1) = ((s1) & 0xffff) + ((s1) >> 16); \
			    (s2) = ((s2) & 0xffff) + ((s2) >> 16); \
			    /* Do it twice */ \
			    (s1) = ((s1) & 0xffff) + ((s1) >> 16); \
			    (s2) = ((s2) & 0xffff) + ((s2) >> 16); \
			    /* Because ~1 == -2, We really need ~1 == -1 */ \
			    if ((s1) > (s2)) (s2)--; \
			    (sd) = (s2) - (s1); \
			    (sd) = ((sd) & 0xffff) + ((sd) >> 16); }

#define	NAT_SYSSPACE		0x80000000
#define	NAT_LOCKHELD		0x40000000


extern	u_int	ipf_nattable_sz;
extern	u_int	ipf_nattable_max;
extern	u_int	ipf_natrules_sz;
extern	u_int	ipf_rdrrules_sz;
extern	u_int	ipf_hostmap_sz;
extern	u_int	fr_nat_maxbucket;
extern	u_int	fr_nat_maxbucket_reset;
extern	int	fr_nat_lock;
extern	int	fr_nat_doflush;
extern	void	fr_natsync __P((void *));
extern	u_long	fr_defnatage;
extern	u_long	fr_defnaticmpage;
extern	u_long	fr_defnatipage;
	/* nat_table[0] -> hashed list sorted by inside (ip, port) */
	/* nat_table[1] -> hashed list sorted by outside (ip, port) */
extern	nat_t	**nat_table[2];
extern	nat_t	*nat_instances;
extern	ipnat_t	*nat_list;
extern	ipnat_t	**nat_rules;
extern	ipnat_t	**rdr_rules;
extern	ipftq_t	*nat_utqe;
extern	natstat_t	nat_stats;

#if defined(__OpenBSD__)
extern	void	nat_ifdetach __P((void *));
#endif
extern	int	fr_nat_ioctl __P((caddr_t, ioctlcmd_t, int, int, void *));
extern	int	fr_natinit __P((void));
extern	nat_t	*nat_new __P((fr_info_t *, ipnat_t *, nat_t **, u_int, int));
extern	nat_t	*nat_outlookup __P((fr_info_t *, u_int, u_int, struct in_addr,
				 struct in_addr));
extern	void	fix_datacksum __P((u_short *, u_32_t));
extern	nat_t	*nat_inlookup __P((fr_info_t *, u_int, u_int, struct in_addr,
				struct in_addr));
extern	nat_t	*nat_tnlookup __P((fr_info_t *, int));
extern	nat_t	*nat_maplookup __P((void *, u_int, struct in_addr,
				struct in_addr));
extern	nat_t	*nat_lookupredir __P((natlookup_t *));
extern	nat_t	*nat_icmperrorlookup __P((fr_info_t *, int));
extern	nat_t	*nat_icmperror __P((fr_info_t *, u_int *, int));
extern	void	nat_delete __P((struct nat *, int));
extern	int	nat_insert __P((nat_t *, int));

extern	int	fr_checknatout __P((fr_info_t *, u_32_t *));
extern	int	fr_natout __P((fr_info_t *, nat_t *, int, u_32_t));
extern	int	fr_checknatin __P((fr_info_t *, u_32_t *));
extern	int	fr_natin __P((fr_info_t *, nat_t *, int, u_32_t));
extern	void	fr_natunload __P((void));
extern	void	fr_natexpire __P((void));
extern	void	nat_log __P((struct nat *, u_int));
extern	void	fix_incksum __P((fr_info_t *, u_short *, u_32_t));
extern	void	fix_outcksum __P((fr_info_t *, u_short *, u_32_t));
extern	void	fr_ipnatderef __P((ipnat_t **));
extern	void	fr_natderef __P((nat_t **));
extern	u_short	*nat_proto __P((fr_info_t *, nat_t *, u_int));
extern	void	nat_update __P((fr_info_t *, nat_t *, ipnat_t *));
extern	void	fr_setnatqueue __P((nat_t *, int));
extern	void	fr_hostmapdel __P((hostmap_t **));

#endif /* __IP_NAT_H__ */
