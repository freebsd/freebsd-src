/*
 * Copyright (C) 1995-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * @(#)ip_nat.h	1.5 2/4/96
 * $Id: ip_nat.h,v 2.17.2.27 2002/08/28 12:45:51 darrenr Exp $
 */

#ifndef	__IP_NAT_H__
#define	__IP_NAT_H__

#ifndef SOLARIS
#define SOLARIS (defined(sun) && (defined(__svr4__) || defined(__SVR4)))
#endif

#if defined(__STDC__) || defined(__GNUC__)
#define	SIOCADNAT	_IOW('r', 60, struct ipnat *)
#define	SIOCRMNAT	_IOW('r', 61, struct ipnat *)
#define	SIOCGNATS	_IOWR('r', 62, struct natstat *)
#define	SIOCGNATL	_IOWR('r', 63, struct natlookup *)
#else
#define	SIOCADNAT	_IOW(r, 60, struct ipnat *)
#define	SIOCRMNAT	_IOW(r, 61, struct ipnat *)
#define	SIOCGNATS	_IOWR(r, 62, struct natstat *)
#define	SIOCGNATL	_IOWR(r, 63, struct natlookup *)
#endif

#undef	LARGE_NAT	/* define this if you're setting up a system to NAT
			 * LARGE numbers of networks/hosts - i.e. in the
			 * hundreds or thousands.  In such a case, you should
			 * also change the RDR_SIZE and NAT_SIZE below to more
			 * appropriate sizes.  The figures below were used for
			 * a setup with 1000-2000 networks to NAT.
			 */
#ifndef	NAT_SIZE
# define	NAT_SIZE	127
#endif
#ifndef	RDR_SIZE
# define	RDR_SIZE	127
#endif
#ifndef	HOSTMAP_SIZE
# define	HOSTMAP_SIZE	127
#endif
#ifndef	NAT_TABLE_SZ
# define	NAT_TABLE_SZ	127
#endif
#ifdef	LARGE_NAT
#undef	NAT_SIZE
#undef	RDR_SIZE
#undef	NAT_TABLE_SZ
#undef	HOSTMAP_SIZE	127
#define	NAT_SIZE	2047
#define	RDR_SIZE	2047
#define	NAT_TABLE_SZ	16383
#define	HOSTMAP_SIZE	8191
#endif
#ifndef	APR_LABELLEN
#define	APR_LABELLEN	16
#endif
#define	NAT_HW_CKSUM	0x80000000

#define	DEF_NAT_AGE	1200     /* 10 minutes (600 seconds) */

struct ap_session;

typedef	struct	nat	{
	u_long	nat_age;
	int	nat_flags;
	u_32_t	nat_sumd[2];
	u_32_t	nat_ipsumd;
	void	*nat_data;
	struct	ap_session	*nat_aps;		/* proxy session */
	struct	frentry	*nat_fr;	/* filter rule ptr if appropriate */
	struct	in_addr	nat_inip;
	struct	in_addr	nat_outip;
	struct	in_addr	nat_oip;	/* other ip */
	U_QUAD_T	nat_pkts;
	U_QUAD_T	nat_bytes;
	u_int	nat_drop[2];
	u_short	nat_oport;		/* other port */
	u_short	nat_inport;
	u_short	nat_outport;
	u_short	nat_use;
	u_char	nat_tcpstate[2];
	u_char	nat_p;			/* protocol for NAT */
	struct	ipnat	*nat_ptr;	/* pointer back to the rule */
	struct	hostmap	*nat_hm;
	struct	nat	*nat_next;
	struct	nat	*nat_hnext[2];
	struct	nat	**nat_phnext[2];
	struct	nat	**nat_me;
	void	*nat_ifp;
	int	nat_dir;
	char	nat_ifname[IFNAMSIZ];
#if SOLARIS || defined(__sgi)
	kmutex_t	nat_lock;
#endif
} nat_t;

typedef	struct	ipnat	{
	struct	ipnat	*in_next;
	struct	ipnat	*in_rnext;
	struct	ipnat	**in_prnext;
	struct	ipnat	*in_mnext;
	struct	ipnat	**in_pmnext;
	void	*in_ifp;
	void	*in_apr;
	u_long	in_space;
	u_int	in_use;
	u_int	in_hits;
	struct	in_addr	in_nextip;
	u_short	in_pnext;
	u_short	in_ippip;	/* IP #'s per IP# */
	u_32_t	in_flags;	/* From here to in_dport must be reflected */
	u_short	in_spare;
	u_short	in_ppip;	/* ports per IP */
	u_short	in_port[2];	/* correctly in IPN_CMPSIZ */
	struct	in_addr	in_in[2];
	struct	in_addr	in_out[2];
	struct	in_addr	in_src[2];
	struct	frtuc	in_tuc;
	u_int	in_age[2];	/* Aging for NAT entries. Not for TCP */
	int	in_redir; /* 0 if it's a mapping, 1 if it's a hard redir */
	char	in_ifname[IFNAMSIZ];
	char	in_plabel[APR_LABELLEN];	/* proxy label */
	char	in_p;	/* protocol */
} ipnat_t;

#define	in_pmin		in_port[0]	/* Also holds static redir port */
#define	in_pmax		in_port[1]
#define	in_nip		in_nextip.s_addr
#define	in_inip		in_in[0].s_addr
#define	in_inmsk	in_in[1].s_addr
#define	in_outip	in_out[0].s_addr
#define	in_outmsk	in_out[1].s_addr
#define	in_srcip	in_src[0].s_addr
#define	in_srcmsk	in_src[1].s_addr
#define	in_scmp		in_tuc.ftu_scmp
#define	in_dcmp		in_tuc.ftu_dcmp
#define	in_stop		in_tuc.ftu_stop
#define	in_dtop		in_tuc.ftu_dtop
#define	in_sport	in_tuc.ftu_sport
#define	in_dport	in_tuc.ftu_dport

#define	NAT_OUTBOUND	0
#define	NAT_INBOUND	1

#define	NAT_MAP		0x01
#define	NAT_REDIRECT	0x02
#define	NAT_BIMAP	(NAT_MAP|NAT_REDIRECT)
#define	NAT_MAPBLK	0x04
/* 0x100 reserved for FI_W_SPORT */
/* 0x200 reserved for FI_W_DPORT */
/* 0x400 reserved for FI_W_SADDR */
/* 0x800 reserved for FI_W_DADDR */
/* 0x1000 reserved for FI_W_NEWFR */

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


typedef	struct	hostmap	{
	struct	hostmap	*hm_next;
	struct	hostmap	**hm_pnext;
	struct	ipnat	*hm_ipnat;
	struct	in_addr	hm_realip;
	struct	in_addr	hm_mapip;
	int	hm_ref;
} hostmap_t;


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
	nat_t	**ns_table[2];
	hostmap_t **ns_maptable;
	ipnat_t	*ns_list;
	void	*ns_apslist;
	u_int	ns_nattab_sz;
	u_int	ns_rultab_sz;
	u_int	ns_rdrtab_sz;
	u_int	ns_hostmap_sz;
	nat_t	*ns_instances;
	u_int	ns_wilds;
} natstat_t;

#define	IPN_ANY		0x000
#define	IPN_TCP		0x001
#define	IPN_UDP		0x002
#define	IPN_TCPUDP	(IPN_TCP|IPN_UDP)
#define	IPN_DELETE	0x004
#define	IPN_ICMPERR	0x008
#define	IPN_RF		(IPN_TCPUDP|IPN_DELETE|IPN_ICMPERR)
#define	IPN_AUTOPORTMAP	0x010
#define	IPN_IPRANGE	0x020
#define	IPN_USERFLAGS	(IPN_TCPUDP|IPN_AUTOPORTMAP|IPN_IPRANGE|IPN_SPLIT|\
			 IPN_ROUNDR|IPN_FILTER|IPN_NOTSRC|IPN_NOTDST|IPN_FRAG)
#define	IPN_FILTER	0x040
#define	IPN_SPLIT	0x080
#define	IPN_ROUNDR	0x100
#define	IPN_NOTSRC	0x080000
#define	IPN_NOTDST	0x100000
#define	IPN_FRAG	0x200000


typedef	struct	natlog {
	struct	in_addr	nl_origip;
	struct	in_addr	nl_outip;
	struct	in_addr	nl_inip;
	u_short	nl_origport;
	u_short	nl_outport;
	u_short	nl_inport;
	u_short	nl_type;
	int	nl_rule;
	U_QUAD_T	nl_pkts;
	U_QUAD_T	nl_bytes;
	u_char	nl_p;
} natlog_t;


#define	NL_NEWMAP	NAT_MAP
#define	NL_NEWRDR	NAT_REDIRECT
#define	NL_NEWBIMAP	NAT_BIMAP
#define	NL_NEWBLOCK	NAT_MAPBLK
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
extern	u_int	ipf_natrules_sz;
extern	u_int	ipf_rdrrules_sz;
extern	int	fr_nat_lock;
extern	void	ip_natsync __P((void *));
extern	u_long	fr_defnatage;
extern	u_long	fr_defnaticmpage;
extern	nat_t	**nat_table[2];
extern	nat_t	*nat_instances;
extern	ipnat_t	**nat_rules;
extern	ipnat_t	**rdr_rules;
extern	ipnat_t	*nat_list;
extern	natstat_t	nat_stats;
#if defined(__OpenBSD__)
extern	void	nat_ifdetach __P((void *));
#endif
#if defined(__NetBSD__) || defined(__OpenBSD__) || (__FreeBSD_version >= 300003)
extern	int	nat_ioctl __P((caddr_t, u_long, int));
#else
extern	int	nat_ioctl __P((caddr_t, int, int));
#endif
extern	int	nat_init __P((void));
extern	nat_t	*nat_new __P((fr_info_t *, ip_t *, ipnat_t *, nat_t **,
			      u_int, int));
extern	nat_t	*nat_outlookup __P((fr_info_t *, u_int, u_int, struct in_addr,
				 struct in_addr, int));
extern	nat_t	*nat_inlookup __P((fr_info_t *, u_int, u_int, struct in_addr,
				struct in_addr, int));
extern	nat_t	*nat_lookupredir __P((natlookup_t *));
extern	nat_t	*nat_icmplookup __P((ip_t *, fr_info_t *, int));
extern	nat_t	*nat_icmp __P((ip_t *, fr_info_t *, u_int *, int));
extern	int	nat_clearlist __P((void));
extern	void	nat_insert __P((nat_t *));

extern	int	ip_natout __P((ip_t *, fr_info_t *));
extern	int	ip_natin __P((ip_t *, fr_info_t *));
extern	void	ip_natunload __P((void)), ip_natexpire __P((void));
extern	void	nat_log __P((struct nat *, u_int));
extern	void	fix_incksum __P((fr_info_t *, u_short *, u_32_t));
extern	void	fix_outcksum __P((fr_info_t *, u_short *, u_32_t));
extern	void	fix_datacksum __P((u_short *, u_32_t));

#endif /* __IP_NAT_H__ */
