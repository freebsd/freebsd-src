/*
 * Copyright (C) 1995-2000 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 *
 * @(#)ip_state.h	1.3 1/12/96 (C) 1995 Darren Reed
 * $Id: ip_state.h,v 2.13.2.2 2000/08/23 11:01:31 darrenr Exp $
 */
#ifndef	__IP_STATE_H__
#define	__IP_STATE_H__

#if defined(__STDC__) || defined(__GNUC__)
# define	SIOCDELST	_IOW('r', 61, struct ipstate *)
#else
# define	SIOCDELST	_IOW(r, 61, struct ipstate *)
#endif

#define	IPSTATE_SIZE	5737
#define	IPSTATE_MAX	4013	/* Maximum number of states held */

#define	PAIRS(s1,d1,s2,d2)	((((s1) == (s2)) && ((d1) == (d2))) ||\
				 (((s1) == (d2)) && ((d1) == (s2))))
#define	IPPAIR(s1,d1,s2,d2)	PAIRS((s1).s_addr, (d1).s_addr, \
				      (s2).s_addr, (d2).s_addr)


typedef struct udpstate {
	u_short	us_sport;
	u_short	us_dport;
} udpstate_t;

typedef struct icmpstate {
	u_short	ics_id;
	u_short	ics_seq;
	u_char	ics_type;
} icmpstate_t;

typedef	struct	tcpdata	{
	u_32_t	td_end;
	u_32_t	td_maxend;
	u_short	td_maxwin;
} tcpdata_t;

typedef	struct tcpstate {
	u_short	ts_sport;
	u_short	ts_dport;
	tcpdata_t ts_data[2];
	u_char	ts_state[2];
} tcpstate_t;

typedef struct ipstate {
	struct	ipstate	*is_next;
	struct	ipstate	**is_pnext;
	struct	ipstate	*is_hnext;
	struct	ipstate	**is_phnext;
	u_long	is_age;
	u_int	is_pass;
	U_QUAD_T	is_pkts;
	U_QUAD_T	is_bytes;
	void	*is_ifp[2];
	frentry_t	*is_rule;
	union	i6addr	is_src;
	union	i6addr	is_dst;
	u_char	is_p;			/* Protocol */
	u_char	is_v;
	u_int	is_hv;
	u_32_t	is_flags;
	u_32_t	is_opt;			/* packet options set */
	u_32_t	is_optmsk;		/*    "      "    mask */
	u_short	is_sec;			/* security options set */
	u_short	is_secmsk;		/*    "        "    mask */
	u_short	is_auth;		/* authentication options set */
	u_short	is_authmsk;		/*    "              "    mask */
	union {
		icmpstate_t	is_ics;
		tcpstate_t	is_ts;
		udpstate_t	is_us;
	} is_ps;
	char	is_ifname[2][IFNAMSIZ];
#if SOLARIS || defined(__sgi)
	kmutex_t	is_lock;
#endif
} ipstate_t;

#define	is_saddr	is_src.in4.s_addr
#define	is_daddr	is_dst.in4.s_addr
#define	is_icmp		is_ps.is_ics
#define	is_type		is_icmp.ics_type
#define	is_code		is_icmp.ics_code
#define	is_tcp		is_ps.is_ts
#define	is_udp		is_ps.is_us
#define is_send		is_tcp.ts_data[0].td_end
#define is_dend		is_tcp.ts_data[1].td_end
#define is_maxswin	is_tcp.ts_data[0].td_maxwin
#define is_maxdwin	is_tcp.ts_data[1].td_maxwin
#define is_maxsend	is_tcp.ts_data[0].td_maxend
#define is_maxdend	is_tcp.ts_data[1].td_maxend
#define	is_sport	is_tcp.ts_sport
#define	is_dport	is_tcp.ts_dport
#define	is_state	is_tcp.ts_state
#define	is_ifpin	is_ifp[0]
#define	is_ifpout	is_ifp[1]

#define	TH_OPENING	(TH_SYN|TH_ACK)
/*
 * is_flags:
 * Bits 0 - 3 are use as a mask with the current packet's bits to check for
 * whether it is short, tcp/udp, a fragment or the presence of IP options.
 * Bits 4 - 7 are set from the initial packet and contain what the packet
 * anded with bits 0-3 must match.
 * Bits 8,9 are used to indicate wildcard source/destination port matching.
 */

typedef	struct	ipstate_save	{
	void	*ips_next;
	struct	ipstate	ips_is;
	struct	frentry	ips_fr;
} ipstate_save_t;

#define	ips_rule	ips_is.is_rule


typedef	struct	ipslog	{
	U_QUAD_T	isl_pkts;
	U_QUAD_T	isl_bytes;
	union	i6addr	isl_src;
	union	i6addr	isl_dst;
	u_short	isl_type;
	union {
		u_short	isl_filler[2];
		u_short	isl_ports[2];
		u_short	isl_icmp;
	} isl_ps;
	u_char	isl_v;
	u_char	isl_p;
	u_char	isl_flags;
	u_char	isl_state[2];
} ipslog_t;

#define	isl_sport	isl_ps.isl_ports[0]
#define	isl_dport	isl_ps.isl_ports[1]
#define	isl_itype	isl_ps.isl_icmp

#define	ISL_NEW		0
#define	ISL_EXPIRE	0xffff
#define	ISL_FLUSH	0xfffe
#define	ISL_REMOVE	0xfffd


typedef	struct	ips_stat {
	u_long	iss_hits;
	u_long	iss_miss;
	u_long	iss_max;
	u_long	iss_tcp;
	u_long	iss_udp;
	u_long	iss_icmp;
	u_long	iss_nomem;
	u_long	iss_expire;
	u_long	iss_fin;
	u_long	iss_active;
	u_long	iss_logged;
	u_long	iss_logfail;
	u_long	iss_inuse;
	ipstate_t **iss_table;
	ipstate_t *iss_list;
} ips_stat_t;


extern	u_long	fr_tcpidletimeout;
extern	u_long	fr_tcpclosewait;
extern	u_long	fr_tcplastack;
extern	u_long	fr_tcptimeout;
extern	u_long	fr_tcpclosed;
extern	u_long	fr_tcphalfclosed;
extern	u_long	fr_udptimeout;
extern	u_long	fr_icmptimeout;
extern	int	fr_state_lock;
extern	int	fr_stateinit __P((void));
extern	int	fr_tcpstate __P((ipstate_t *, fr_info_t *, ip_t *, tcphdr_t *));
extern	ipstate_t	*fr_addstate __P((ip_t *, fr_info_t *, u_int));
extern	frentry_t	*fr_checkstate __P((ip_t *, fr_info_t *));
extern	void	ip_statesync __P((void *));
extern	void	fr_timeoutstate __P((void));
extern	void	fr_tcp_age __P((u_long *, u_char *, fr_info_t *, int));
extern	void	fr_stateunload __P((void));
extern	void	ipstate_log __P((struct ipstate *, u_int));
#if defined(__NetBSD__) || defined(__OpenBSD__)
extern	int	fr_state_ioctl __P((caddr_t, u_long, int));
#else
extern	int	fr_state_ioctl __P((caddr_t, int, int));
#endif

#endif /* __IP_STATE_H__ */
