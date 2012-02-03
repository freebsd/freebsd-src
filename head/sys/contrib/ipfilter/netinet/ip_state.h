/*	$FreeBSD$	*/

/*
 * Copyright (C) 1995-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * @(#)ip_state.h	1.3 1/12/96 (C) 1995 Darren Reed
 * $FreeBSD$
 * Id: ip_state.h,v 2.68.2.10 2007/10/16 09:33:24 darrenr Exp $
 */
#ifndef	__IP_STATE_H__
#define	__IP_STATE_H__

#if defined(__STDC__) || defined(__GNUC__) || defined(_AIX51)
# define	SIOCDELST	_IOW('r', 61, struct ipfobj)
#else
# define	SIOCDELST	_IOW(r, 61, struct ipfobj)
#endif

struct ipscan;

#ifndef	IPSTATE_SIZE
# define	IPSTATE_SIZE	5737
#endif
#ifndef	IPSTATE_MAX
# define	IPSTATE_MAX	4013	/* Maximum number of states held */
#endif

#define	SEQ_GE(a,b)	((int)((a) - (b)) >= 0)
#define	SEQ_GT(a,b)	((int)((a) - (b)) > 0)


typedef struct ipstate {
	ipfmutex_t	is_lock;
	struct	ipstate	*is_next;
	struct	ipstate	**is_pnext;
	struct	ipstate	*is_hnext;
	struct	ipstate	**is_phnext;
	struct	ipstate	**is_me;
	void		*is_ifp[4];
	void		*is_sync;
	frentry_t	*is_rule;
	struct	ipftq	*is_tqehead[2];
	struct	ipscan	*is_isc;
	U_QUAD_T	is_pkts[4];
	U_QUAD_T	is_bytes[4];
	U_QUAD_T	is_icmppkts[4];
	struct	ipftqent is_sti;
	u_int	is_frage[2];
	int	is_ref;			/* reference count */
	int	is_isninc[2];
	u_short	is_sumd[2];
	i6addr_t	is_src;
	i6addr_t	is_dst;
	u_int	is_pass;
	u_char	is_p;			/* Protocol */
	u_char	is_v;
	u_32_t	is_hv;
	u_32_t	is_tag;
	u_32_t	is_opt[2];		/* packet options set */
	u_32_t	is_optmsk[2];		/*    "      "    mask */
	u_short	is_sec;			/* security options set */
	u_short	is_secmsk;		/*    "        "    mask */
	u_short	is_auth;		/* authentication options set */
	u_short	is_authmsk;		/*    "              "    mask */
	union {
		icmpinfo_t	is_ics;
		tcpinfo_t	is_ts;
		udpinfo_t	is_us;
		greinfo_t	is_ug;
	} is_ps;
	u_32_t	is_flags;
	int	is_flx[2][2];
	u_32_t	is_rulen;		/* rule number when created */
	u_32_t	is_s0[2];
	u_short	is_smsk[2];
	char	is_group[FR_GROUPLEN];
	char	is_sbuf[2][16];
	char	is_ifname[4][LIFNAMSIZ];
} ipstate_t;

#define	is_die		is_sti.tqe_die
#define	is_state	is_sti.tqe_state
#define	is_touched	is_sti.tqe_touched
#define	is_saddr	is_src.in4.s_addr
#define	is_daddr	is_dst.in4.s_addr
#define	is_icmp		is_ps.is_ics
#define	is_type		is_icmp.ici_type
#define	is_code		is_icmp.ici_code
#define	is_tcp		is_ps.is_ts
#define	is_udp		is_ps.is_us
#define is_send		is_tcp.ts_data[0].td_end
#define is_dend		is_tcp.ts_data[1].td_end
#define is_maxswin	is_tcp.ts_data[0].td_maxwin
#define is_maxdwin	is_tcp.ts_data[1].td_maxwin
#define is_maxsend	is_tcp.ts_data[0].td_maxend
#define is_maxdend	is_tcp.ts_data[1].td_maxend
#define	is_swinscale	is_tcp.ts_data[0].td_winscale
#define	is_dwinscale	is_tcp.ts_data[1].td_winscale
#define	is_swinflags	is_tcp.ts_data[0].td_winflags
#define	is_dwinflags	is_tcp.ts_data[1].td_winflags
#define	is_sport	is_tcp.ts_sport
#define	is_dport	is_tcp.ts_dport
#define	is_ifpin	is_ifp[0]
#define	is_ifpout	is_ifp[2]
#define	is_gre		is_ps.is_ug
#define	is_call		is_gre.gs_call

#define	IS_WSPORT	SI_W_SPORT	/* 0x00100 */
#define	IS_WDPORT	SI_W_DPORT	/* 0x00200 */
#define	IS_WSADDR	SI_W_SADDR	/* 0x00400 */
#define	IS_WDADDR	SI_W_DADDR	/* 0x00800 */
#define	IS_NEWFR	SI_NEWFR	/* 0x01000 */
#define	IS_CLONE	SI_CLONE	/* 0x02000 */
#define	IS_CLONED	SI_CLONED	/* 0x04000 */
#define	IS_TCPFSM			   0x10000
#define	IS_STRICT			   0x20000
#define	IS_ISNSYN			   0x40000
#define	IS_ISNACK			   0x80000
#define	IS_STATESYNC			   0x100000
/*
 * IS_SC flags are for scan-operations that need to be recognised in state.
 */
#define	IS_SC_CLIENT			0x10000000
#define	IS_SC_SERVER			0x20000000
#define	IS_SC_MATCHC			0x40000000
#define	IS_SC_MATCHS			0x80000000
#define	IS_SC_MATCHALL	(IS_SC_MATCHC|IS_SC_MATCHC)
#define	IS_SC_ALL	(IS_SC_MATCHC|IS_SC_MATCHC|IS_SC_CLIENT|IS_SC_SERVER)

/*
 * Flags that can be passed into fr_addstate
 */
#define	IS_INHERITED			0x0fffff00

#define	TH_OPENING	(TH_SYN|TH_ACK)
/*
 * is_flags:
 * Bits 0 - 3 are use as a mask with the current packet's bits to check for
 * whether it is short, tcp/udp, a fragment or the presence of IP options.
 * Bits 4 - 7 are set from the initial packet and contain what the packet
 * anded with bits 0-3 must match.
 * Bits 8,9 are used to indicate wildcard source/destination port matching.
 * Bits 10,11 are reserved for other wildcard flag compatibility.
 * Bits 12,13 are for scaning.
 */

typedef	struct	ipstate_save	{
	void	*ips_next;
	struct	ipstate	ips_is;
	struct	frentry	ips_fr;
} ipstate_save_t;

#define	ips_rule	ips_is.is_rule


typedef	struct	ipslog	{
	U_QUAD_T	isl_pkts[4];
	U_QUAD_T	isl_bytes[4];
	i6addr_t	isl_src;
	i6addr_t	isl_dst;
	u_32_t	isl_tag;
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
	u_32_t	isl_rulen;
	char	isl_group[FR_GROUPLEN];
} ipslog_t;

#define	isl_sport	isl_ps.isl_ports[0]
#define	isl_dport	isl_ps.isl_ports[1]
#define	isl_itype	isl_ps.isl_icmp

#define	ISL_NEW			0
#define	ISL_CLONE		1
#define	ISL_EXPIRE		0xffff
#define	ISL_FLUSH		0xfffe
#define	ISL_REMOVE		0xfffd
#define	ISL_INTERMEDIATE	0xfffc
#define	ISL_KILLED		0xfffb
#define	ISL_ORPHAN		0xfffa
#define	ISL_UNLOAD		0xfff9


typedef	struct	ips_stat {
	u_long	iss_hits;
	u_long	iss_miss;
	u_long	iss_max;
	u_long	iss_maxref;
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
	u_long	iss_wild;
	u_long	iss_killed;
	u_long	iss_ticks;
	u_long	iss_bucketfull;
	int	iss_statesize;
	int	iss_statemax;
	ipstate_t **iss_table;
	ipstate_t *iss_list;
	u_long	*iss_bucketlen;
	ipftq_t	*iss_tcptab;
} ips_stat_t;


extern	u_long	fr_tcpidletimeout;
extern	u_long	fr_tcpclosewait;
extern	u_long	fr_tcplastack;
extern	u_long	fr_tcptimeout;
extern	u_long	fr_tcpclosed;
extern	u_long	fr_tcphalfclosed;
extern	u_long	fr_udptimeout;
extern	u_long	fr_udpacktimeout;
extern	u_long	fr_icmptimeout;
extern	u_long	fr_icmpacktimeout;
extern	u_long	fr_iptimeout;
extern	int	fr_statemax;
extern	int	fr_statesize;
extern	int	fr_state_lock;
extern	int	fr_state_maxbucket;
extern	int	fr_state_maxbucket_reset;
extern	ipstate_t	*ips_list;
extern	ipftq_t	*ips_utqe;
extern	ipftq_t	ips_tqtqb[IPF_TCP_NSTATES];

extern	int	fr_stateinit __P((void));
extern	ipstate_t *fr_addstate __P((fr_info_t *, ipstate_t **, u_int));
extern	frentry_t *fr_checkstate __P((struct fr_info *, u_32_t *));
extern	ipstate_t *fr_stlookup __P((fr_info_t *, tcphdr_t *, ipftq_t **));
extern	void	fr_statesync __P((void *));
extern	void	fr_timeoutstate __P((void));
extern	int	fr_tcp_age __P((struct ipftqent *, struct fr_info *,
				struct ipftq *, int));
extern	int	fr_tcpinwindow __P((struct fr_info *, struct tcpdata *,
				    struct tcpdata *, tcphdr_t *, int));
extern	void	fr_stateunload __P((void));
extern	void	ipstate_log __P((struct ipstate *, u_int));
extern	int	fr_state_ioctl __P((caddr_t, ioctlcmd_t, int, int, void *));
extern	void	fr_stinsert __P((struct ipstate *, int));
extern	void	fr_sttab_init __P((struct ipftq *));
extern	void	fr_sttab_destroy __P((struct ipftq *));
extern	void	fr_updatestate __P((fr_info_t *, ipstate_t *, ipftq_t *));
extern	void	fr_statederef __P((ipstate_t **));
extern	void	fr_setstatequeue __P((ipstate_t *, int));

#endif /* __IP_STATE_H__ */
