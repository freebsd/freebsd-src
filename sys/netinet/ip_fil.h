/*
 * (C)opyright 1993-1996 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 *
 * @(#)ip_fil.h	1.35 6/5/96
 * $Id: ip_fil.h,v 2.0.1.2 1997/01/10 00:28:15 darrenr Exp $
 */

#ifndef	__IP_FIL_H__
#define	__IP_FIL_H__

#ifndef	SOLARIS
#define	SOLARIS	(defined(sun) && (defined(__svr4__) || defined(__SVR4)))
#endif

#if defined(KERNEL) && !defined(_KERNEL)
#define	_KERNEL
#endif
#if SOLARIS
# include <sys/ioccom.h>
# include <sys/sysmacros.h>
# ifdef	_KERNEL
#  include <inet/common.h>
/*
 * because Solaris 2 defines these in two places :-/
 */
#undef	IPOPT_EOL
#undef	IPOPT_NOP
#undef	IPOPT_LSRR
#undef	IPOPT_RR
#undef	IPOPT_SSRR
#  include <inet/ip.h>
# endif
#endif

#if defined(__STDC__) || defined(__GNUC__)
#define	SIOCADAFR	_IOW('r', 60, struct frentry)
#define	SIOCRMAFR	_IOW('r', 61, struct frentry)
#define	SIOCSETFF	_IOW('r', 62, u_int)
#define	SIOCGETFF	_IOR('r', 63, u_int)
#define	SIOCGETFS	_IOR('r', 64, struct friostat)
#define	SIOCIPFFL	_IOWR('r', 65, int)
#define	SIOCIPFFB	_IOR('r', 66, int)
#define	SIOCADIFR	_IOW('r', 67, struct frentry)
#define	SIOCRMIFR	_IOW('r', 68, struct frentry)
#define	SIOCSWAPA	_IOR('r', 69, u_int)
#define	SIOCINAFR	_IOW('r', 70, struct frentry)
#define	SIOCINIFR	_IOW('r', 71, struct frentry)
#define	SIOCFRENB	_IOW('r', 72, u_int)
#define	SIOCFRSYN	_IOW('r', 73, u_int)
#define	SIOCFRZST	_IOWR('r', 74, struct friostat)
#define	SIOCZRLST	_IOWR('r', 75, struct frentry)
#else
#define	SIOCADAFR	_IOW(r, 60, struct frentry)
#define	SIOCRMAFR	_IOW(r, 61, struct frentry)
#define	SIOCSETFF	_IOW(r, 62, u_int)
#define	SIOCGETFF	_IOR(r, 63, u_int)
#define	SIOCGETFS	_IOR(r, 64, struct friostat)
#define	SIOCIPFFL	_IOWR(r, 65, int)
#define	SIOCIPFFB	_IOR(r, 66, int)
#define	SIOCADIFR	_IOW(r, 67, struct frentry)
#define	SIOCRMIFR	_IOW(r, 68, struct frentry)
#define	SIOCSWAPA	_IOR(r, 69, u_int)
#define	SIOCINAFR	_IOW(r, 70, struct frentry)
#define	SIOCINIFR	_IOW(r, 71, struct frentry)
#define SIOCFRENB	_IOW(r, 72, u_int)
#define	SIOCFRSYN	_IOW(r, 73, u_int)
#define	SIOCFRZST	_IOWR(r, 74, struct friostat)
#define	SIOCZRLST	_IOWR(r, 75, struct frentry)
#endif
#define	SIOCADDFR	SIOCADAFR
#define	SIOCDELFR	SIOCRMAFR
#define	SIOCINSFR	SIOCINAFR

typedef	struct	fr_ip	{
	u_char	fi_v:4;		/* IP version */
	u_char	fi_fl:4;	/* packet flags */
	u_char	fi_tos;
	u_char	fi_ttl;
	u_char	fi_p;
	struct	in_addr	fi_src;
	struct	in_addr	fi_dst;
	u_long	fi_optmsk;	/* bitmask composed from IP options */
	u_short	fi_secmsk;	/* bitmask composed from IP security options */
	u_short	fi_auth;
} fr_ip_t;

#define	FI_OPTIONS	0x01
#define	FI_TCPUDP	0x02	/* TCP/UCP implied comparison involved */
#define	FI_FRAG		0x04
#define	FI_SHORT	0x08

typedef	struct	fr_info	{
	struct	fr_ip	fin_fi;
	void	*fin_ifp;
	u_short	fin_data[2];
	u_short	fin_out;
	u_char	fin_tcpf;
	u_char	fin_icode;
	u_short	fin_rule;
	u_short	fin_hlen;
	u_short	fin_dlen;
	char	*fin_dp;		/* start of data past IP header */
	struct	frentry *fin_fr;
} fr_info_t;

#define	FI_CSIZE	(sizeof(struct fr_ip) + 11)

typedef	struct	frdest	{
	void	*fd_ifp;
	struct	in_addr	fd_ip;
	char	fd_ifname[IFNAMSIZ];
} frdest_t;

typedef	struct	frentry {
	struct	frentry	*fr_next;
	struct	ifnet	*fr_ifa;
	u_long	fr_hits;
	u_long	fr_bytes;	/* this is only incremented when a packet */
				/* matches this rule and it is the last match*/
	/*
	 * Fields after this may not change whilst in the kernel.
	 */
	struct	fr_ip	fr_ip;
	struct	fr_ip	fr_mip;	/* mask structure */

	u_char	fr_tcpfm;	/* tcp flags mask */
	u_char	fr_tcpf;	/* tcp flags */

	u_short	fr_icmpm;	/* data for ICMP packets (mask) */
	u_short	fr_icmp;

	u_char	fr_scmp;	/* data for port comparisons */
	u_char	fr_dcmp;
	u_short	fr_dport;
	u_short	fr_sport;
	u_short	fr_stop;	/* top port for <> and >< */
	u_short	fr_dtop;	/* top port for <> and >< */
	u_long	fr_flags;	/* per-rule flags && options (see below) */
	int	(*fr_func)();	/* call this function */
	char	fr_icode;	/* return ICMP code */
	char	fr_ifname[IFNAMSIZ];
	struct	frdest	fr_tif;	/* "to" interface */
	struct	frdest	fr_dif;	/* duplicate packet interfaces */
} frentry_t;

#define	fr_proto	fr_ip.fi_p
#define	fr_ttl		fr_ip.fi_ttl
#define	fr_tos		fr_ip.fi_tos
#define	fr_dst		fr_ip.fi_dst
#define	fr_src		fr_ip.fi_src
#define	fr_dmsk		fr_mip.fi_dst
#define	fr_smsk		fr_mip.fi_src

#ifndef	offsetof
#define	offsetof(t,m)	(int)((&((t *)0L)->m))
#endif
#define	FR_CMPSIZ	(sizeof(struct frentry) - offsetof(frentry_t, fr_ip))

/*
 * fr_flags
*/
#define	FR_BLOCK	0x00001
#define	FR_PASS		0x00002
#define	FR_OUTQUE	0x00004
#define	FR_INQUE	0x00008
#define	FR_LOG		0x00010	/* Log */
#define	FR_LOGB		0x00011	/* Log-fail */
#define	FR_LOGP		0x00012	/* Log-pass */
#define	FR_LOGBODY	0x00020	/* Log the body */
#define	FR_LOGFIRST	0x00040	/* Log the first byte if state held */
#define	FR_RETRST	0x00080	/* Return TCP RST packet - reset connection */
#define	FR_RETICMP	0x00100	/* Return ICMP unreachable packet */
#define	FR_NOMATCH	0x00200
#define	FR_ACCOUNT	0x00400	/* count packet bytes */
#define	FR_KEEPFRAG	0x00800	/* keep fragment information */
#define	FR_KEEPSTATE	0x01000	/* keep `connection' state information */
#define	FR_INACTIVE	0x02000
#define	FR_QUICK	0x04000	/* match & stop processing list */
#define	FR_FASTROUTE	0x08000	/* bypass normal routing */
#define	FR_CALLNOW	0x10000	/* call another function (fr_func) if matches */
#define	FR_DUP		0x20000	/* duplicate packet */
#define	FR_LOGORBLOCK	0x40000	/* block the packet if it can't be logged */

#define	FR_LOGMASK	(FR_LOG|FR_LOGP|FR_LOGB)
/*
 * recognized flags for SIOCGETFF and SIOCSETFF
 */
#define	FF_LOGPASS	0x100000
#define	FF_LOGBLOCK	0x200000
#define	FF_LOGNOMATCH	0x400000
#define	FF_LOGGING	(FF_LOGPASS|FF_LOGBLOCK|FF_LOGNOMATCH)
#define	FF_BLOCKNONIP	0x800000	/* Solaris2 Only */

#define	FR_NONE 0
#define	FR_EQUAL 1
#define	FR_NEQUAL 2
#define FR_LESST 3
#define FR_GREATERT 4
#define FR_LESSTE 5
#define FR_GREATERTE 6
#define	FR_OUTRANGE 7
#define	FR_INRANGE 8

typedef	struct	filterstats {
	u_long	fr_pass;	/* packets allowed */
	u_long	fr_block;	/* packets denied */
	u_long	fr_nom;		/* packets which don't match any rule */
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
	u_long	fr_pull[2];	/* good and bad pullup attempts */
#if SOLARIS
	u_long	fr_bad;		/* bad IP packets to the filter */
	u_long	fr_notip;	/* packets passed through no on ip queue */
	u_long	fr_drop;	/* packets dropped - no info for them! */
#endif
} filterstats_t;

/*
 * For SIOCGETFS
 */
typedef	struct	friostat	{
	struct	filterstats	f_st[2];
	struct	frentry		*f_fin[2];
	struct	frentry		*f_fout[2];
	struct	frentry		*f_acctin[2];
	struct	frentry		*f_acctout[2];
	int	f_active;
} friostat_t;

typedef struct  optlist {
	u_short ol_val;
	int     ol_bit;
} optlist_t;

/*
 * Log structure.  Each packet header logged is prepended by one of these,
 * minimize size to make most effective use of log space which should
 * (ideally) be a muliple of the most common log entry size.
 */
typedef	struct ipl_ci	{
	u_long	sec;
	u_long	usec;
	u_char	hlen;
	u_char	plen;
	u_short	rule;		/* assume never more than 64k rules, total */
#if (defined(NetBSD) && (NetBSD <= 1991011) && (NetBSD >= 199603))
	u_long	flags;
	u_char	ifname[IFNAMSIZ];	/* = 32 bytes */
#else
	u_long	flags:24;
	u_long	unit:8;
	u_char	ifname[4];	/* = 20 bytes */
#endif
} ipl_ci_t;


#ifndef	ICMP_UNREACH_FILTER
#define	ICMP_UNREACH_FILTER	13
#endif

#define	IPMINLEN(i, h)	((i)->ip_len >= ((i)->ip_hl * 4 + sizeof(struct h)))
#define	IPLLOGSIZE	8192

extern	int	fr_check();
extern	int	fr_copytolog();
extern	fr_info_t	frcache[];
extern	char	*iplh, *iplt;
extern	char	iplbuf[IPLLOGSIZE];

#ifdef _KERNEL

extern struct frentry *ipfilter[2][2], *ipacct[2][2];
extern struct filterstats frstats[];
# if	SOLARIS
extern	int	ipfsync();
# endif
#endif /* _KERNEL */
#endif	/* __IP_FIL_H__ */
