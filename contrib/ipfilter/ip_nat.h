/*
 * (C)opyright 1995-1997 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 *
 * @(#)ip_nat.h	1.5 2/4/96
 * $Id: ip_nat.h,v 2.0.2.12 1997/05/24 07:35:20 darrenr Exp $
 */

#ifndef	__IP_NAT_H__
#define	__IP_NAT_H__

#ifndef	__IP_PROXY_H__
#include "netinet/ip_proxy.h"
#endif

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

#define	NAT_SIZE	367

typedef	struct	nat	{
	u_long	nat_age;
	int	nat_flags;
	u_long	nat_sumd;
	u_long	nat_ipsumd;
	struct	ipfr	*nat_frag;
	struct	in_addr	nat_inip;
	struct	in_addr	nat_outip;
	struct	in_addr	nat_oip;	/* other ip */
	U_QUAD_T	nat_pkts;
	U_QUAD_T	nat_bytes;
	u_short	nat_oport;	/* other port */
	u_short	nat_inport;
	u_short	nat_outport;
	u_short	nat_use;
	u_char	nat_state[2];
	struct	ipnat	*nat_ptr;
	struct	nat	*nat_next;
	struct	nat	*nat_hnext[2];
	struct	nat	**nat_hstart[2];
	void	*nat_ifp;
	int	nat_dir;
} nat_t;

typedef	struct	ipnat	{
	struct	ipnat	*in_next;
	void	*in_ifp;
	u_int	in_space;
	u_int	in_use;
	struct	in_addr	in_nextip;
	u_short	in_pnext;
	u_short	in_flags;
	u_short	in_port[2];
	struct	in_addr	in_in[2];
	struct	in_addr	in_out[2];
	struct	aproxy	*in_apr;
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

#define	NAT_OUTBOUND	0
#define	NAT_INBOUND	1

#define	NAT_MAP		0x01
#define	NAT_REDIRECT	0x02
#define	NAT_BIMAP	(NAT_MAP|NAT_REDIRECT)

#define	IPN_CMPSIZ	(sizeof(struct in_addr) * 4 + sizeof(u_short) * 3 + \
			 sizeof(int))

typedef	struct	natlookup {
	struct	in_addr	nl_inip;
	struct	in_addr	nl_outip;
	u_short	nl_inport;
	u_short	nl_outport;
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
} natstat_t;

#define	IPN_ANY		0x00
#define	IPN_TCP		0x01
#define	IPN_UDP		0x02
#define	IPN_TCPUDP	0x03
#define	IPN_DELETE	0x04


typedef	struct	natlog {
	struct	timeval	nl_tv;
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
} natlog_t;


#define	NL_NEWMAP	NAT_MAP
#define	NL_NEWRDR	NAT_REDIRECT
#define	NL_EXPIRE	0xffff


extern u_long fr_defnatage;
extern nat_t *nat_table[2][NAT_SIZE];
extern int nat_ioctl __P((caddr_t, int, int));
extern nat_t *nat_new __P((ipnat_t *, ip_t *, fr_info_t *, u_short, int));
extern nat_t *nat_outlookup __P((void *, int, struct in_addr, u_short,
				 struct in_addr, u_short));
extern nat_t *nat_inlookup __P((void *, int, struct in_addr, u_short,
				struct in_addr, u_short));
extern nat_t *nat_lookupredir __P((natlookup_t *));
extern nat_t *nat_lookupmapip __P((void *, int, struct in_addr, u_short,
				   struct in_addr, u_short));

extern int ip_natout __P((ip_t *, int, fr_info_t *));
extern int ip_natin __P((ip_t *, int, fr_info_t *));
extern void ip_natunload __P((void)), ip_natexpire __P((void));
extern void nat_log __P((struct nat *, u_short));
extern void fix_incksum __P((u_short *, u_long));
extern void fix_outcksum __P((u_short *, u_long));
#endif /* __IP_NAT_H__ */
