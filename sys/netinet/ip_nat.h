/*
 * Copyright (C) 1995-1998 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 *
 * @(#)ip_nat.h	1.5 2/4/96
 * $Id: ip_nat.h,v 2.1.2.3 2000/01/24 12:44:24 darrenr Exp $
 * $FreeBSD: src/sys/netinet/ip_nat.h,v 1.8 2000/02/10 21:29:10 guido Exp $
 */

#ifndef	__IP_NAT_H__
#define	__IP_NAT_H__

#ifndef SOLARIS
#define SOLARIS (defined(sun) && (defined(__svr4__) || defined(__SVR4)))
#endif

#if defined(__STDC__) || defined(__GNUC__)
#define	SIOCADNAT	_IOW('r', 80, struct ipnat)
#define	SIOCRMNAT	_IOW('r', 81, struct ipnat)
#define	SIOCGNATS	_IOR('r', 82, struct natstat)
#define	SIOCGNATL	_IOWR('r', 83, struct natlookup)
#define SIOCGFRST	_IOR('r', 84, struct ipfrstat)
#define SIOCGIPST	_IOR('r', 85, struct ips_stat)
#define	SIOCFLNAT	_IOWR('r', 86, int)
#define	SIOCCNATL	_IOWR('r', 87, int)
#else
#define	SIOCADNAT	_IOW(r, 80, struct ipnat)
#define	SIOCRMNAT	_IOW(r, 81, struct ipnat)
#define	SIOCGNATS	_IOR(r, 82, struct natstat)
#define	SIOCGNATL	_IOWR(r, 83, struct natlookup)
#define SIOCGFRST	_IOR(r, 84, struct ipfrstat)
#define SIOCGIPST	_IOR(r, 85, struct ips_stat)
#define	SIOCFLNAT	_IOWR(r, 86, int)
#define	SIOCCNATL	_IOWR(r, 87, int)
#endif

#undef	LARGE_NAT	/* define this if you're setting up a system to NAT
			 * LARGE numbers of networks/hosts - i.e. in the
			 * hundreds or thousands.  In such a case, you should
			 * also change the RDR_SIZE and NAT_SIZE below to more
			 * appropriate sizes.  The figures below were used for
			 * a setup with 1000-2000 networks to NAT.
			 */
#define	NAT_SIZE	127
#define	RDR_SIZE	127
#define	NAT_TABLE_SZ	127
#ifdef	LARGE_NAT
#undef	NAT_SIZE
#undef	RDR_SIZE
#undef	NAT_TABLE_SZ
#define	NAT_SIZE	2047
#define	RDR_SIZE	2047
#define	NAT_TABLE_SZ	16383
#endif
#ifndef	APR_LABELLEN
#define	APR_LABELLEN	16
#endif
#define	NAT_HW_CKSUM	0x80000000

#define	DEF_NAT_AGE	1200     /* 10 minutes (600 seconds) */

typedef	struct	nat	{
	u_long	nat_age;
	int	nat_flags;
	u_32_t	nat_sumd[2];
	u_32_t	nat_ipsumd;
	void	*nat_data;
	void	*nat_aps;		/* proxy session */
	frentry_t	*nat_fr;	/* filter rule ptr if appropriate */
	struct	in_addr	nat_inip;
	struct	in_addr	nat_outip;
	struct	in_addr	nat_oip;	/* other ip */
	U_QUAD_T	nat_pkts;
	U_QUAD_T	nat_bytes;
	u_short	nat_oport;		/* other port */
	u_short	nat_inport;
	u_short	nat_outport;
	u_short	nat_use;
	u_char	nat_tcpstate[2];
	u_char	nat_p;			/* protocol for NAT */
	struct	ipnat	*nat_ptr;	/* pointer back to the rule */
	struct	nat	*nat_next;
	struct	nat	*nat_hnext[2];
	struct	nat	**nat_hstart[2];
	void	*nat_ifp;
	int	nat_dir;
} nat_t;

typedef	struct	ipnat	{
	struct	ipnat	*in_next;
	struct	ipnat	*in_rnext;
	struct	ipnat	*in_mnext;
	void	*in_ifp;
	void	*in_apr;
	u_long	in_space;
	u_int	in_use;
	u_int	in_hits;
	struct	in_addr	in_nextip;
	u_short	in_pnext;
	u_short	in_ppip;	/* ports per IP */
	u_short	in_ippip;	/* IP #'s per IP# */
	u_short	in_flags;	/* From here to in_dport must be reflected */
	u_short	in_port[2];	/* correctly in IPN_CMPSIZ */
	struct	in_addr	in_in[2];
	struct	in_addr	in_out[2];
	struct	in_addr	in_src[2];
	int	in_redir; /* 0 if it's a mapping, 1 if it's a hard redir */
	char	in_ifname[IFNAMSIZ];
	char	in_plabel[APR_LABELLEN];	/* proxy label */
	char	in_p;	/* protocol */
	u_short	in_dport;
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

#define	NAT_OUTBOUND	0
#define	NAT_INBOUND	1

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

typedef	struct	natstat	{
	u_long	ns_mapped[2];
	u_long	ns_rules;
	u_long	ns_added;
	u_long	ns_expire;
	u_long	ns_inuse;
	u_long	ns_logged;
	u_long	ns_logfail;
	nat_t	**ns_table[2];
	ipnat_t	*ns_list;
	void	*ns_apslist;
	u_int	ns_nattab_sz;
	u_int	ns_rultab_sz;
	u_int	ns_rdrtab_sz;
	nat_t	*ns_instances;
} natstat_t;

#define	IPN_ANY		0x00
#define	IPN_TCP		0x01
#define	IPN_UDP		0x02
#define	IPN_TCPUDP	(IPN_TCP|IPN_UDP)
#define	IPN_DELETE	0x04
#define	IPN_ICMPERR	0x08
#define	IPN_RF		(IPN_TCPUDP|IPN_DELETE|IPN_ICMPERR)
#define	IPN_AUTOPORTMAP	0x10
#define	IPN_RANGE	0x20
#define	IPN_USERFLAGS	(IPN_TCPUDP|IPN_AUTOPORTMAP|IPN_RANGE)


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
#define	NL_EXPIRE	0xffff

#define	NAT_HASH_FN(k,m)	(((k) + ((k) >> 12)) % (m))

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


extern	u_int	ipf_nattable_sz;
extern	u_int	ipf_natrules_sz;
extern	u_int	ipf_rdrrules_sz;
extern	void	ip_natsync __P((void *));
extern	u_long	fr_defnatage;
extern	u_long	fr_defnaticmpage;
extern	nat_t	**nat_table[2];
extern	nat_t	*nat_instances;
extern	ipnat_t	**nat_rules;
extern	ipnat_t	**rdr_rules;
extern	natstat_t	nat_stats;
#if defined(__NetBSD__) || defined(__OpenBSD__)
extern	int	nat_ioctl __P((caddr_t, u_long, int));
#else
extern	int	nat_ioctl __P((caddr_t, int, int));
#endif
extern	int	nat_init __P((void));
extern	nat_t	*nat_new __P((ipnat_t *, ip_t *, fr_info_t *, u_int, int));
extern	nat_t	*nat_outlookup __P((void *, u_int, u_int, struct in_addr,
				 struct in_addr, u_32_t));
extern	nat_t	*nat_inlookup __P((void *, u_int, u_int, struct in_addr,
				struct in_addr, u_32_t));
extern	nat_t	*nat_maplookup __P((void *, u_int, struct in_addr,
				struct in_addr));
extern	nat_t	*nat_lookupredir __P((natlookup_t *));
extern	nat_t	*nat_icmpinlookup __P((ip_t *, fr_info_t *));
extern	nat_t	*nat_icmpin __P((ip_t *, fr_info_t *, u_int *));

extern	int	ip_natout __P((ip_t *, fr_info_t *));
extern	int	ip_natin __P((ip_t *, fr_info_t *));
extern	void	ip_natunload __P((void)), ip_natexpire __P((void));
extern	void	nat_log __P((struct nat *, u_int));
extern	void	fix_incksum __P((u_short *, u_32_t, int));
extern	void	fix_outcksum __P((u_short *, u_32_t, int));

#endif /* __IP_NAT_H__ */
