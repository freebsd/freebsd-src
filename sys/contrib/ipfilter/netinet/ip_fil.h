/*
 * Copyright (C) 1993-2002 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * @(#)ip_fil.h	1.35 6/5/96
 * $Id: ip_fil.h,v 2.29.2.4 2000/11/12 11:54:53 darrenr Exp $
 * $FreeBSD$
 */

#ifndef	__IP_FIL_H__
#define	__IP_FIL_H__

/*
 * Pathnames for various IP Filter control devices.  Used by LKM
 * and userland, so defined here.
 */
#define	IPNAT_NAME	"/dev/ipnat"
#define	IPSTATE_NAME	"/dev/ipstate"
#define	IPAUTH_NAME	"/dev/ipauth"

#ifndef	SOLARIS
# define SOLARIS (defined(sun) && (defined(__svr4__) || defined(__SVR4)))
#endif

#if defined(KERNEL) && !defined(_KERNEL)
# define	_KERNEL
#endif

#ifndef	__P
# ifdef	__STDC__
#  define	__P(x)	x
# else
#  define	__P(x)	()
# endif
#endif

#ifndef	offsetof
# define	offsetof(t,m)	(int)((&((t *)0L)->m))
#endif

#if defined(__STDC__) || defined(__GNUC__)
# define	SIOCADAFR	_IOW('r', 60, struct frentry *)
# define	SIOCRMAFR	_IOW('r', 61, struct frentry *)
# define	SIOCSETFF	_IOW('r', 62, u_int)
# define	SIOCGETFF	_IOR('r', 63, u_int)
# define	SIOCGETFS	_IOWR('r', 64, struct friostat *)
# define	SIOCIPFFL	_IOWR('r', 65, int)
# define	SIOCIPFFB	_IOR('r', 66, int)
# define	SIOCADIFR	_IOW('r', 67, struct frentry *)
# define	SIOCRMIFR	_IOW('r', 68, struct frentry *)
# define	SIOCSWAPA	_IOR('r', 69, u_int)
# define	SIOCINAFR	_IOW('r', 70, struct frentry *)
# define	SIOCINIFR	_IOW('r', 71, struct frentry *)
# define	SIOCFRENB	_IOW('r', 72, u_int)
# define	SIOCFRSYN	_IOW('r', 73, u_int)
# define	SIOCFRZST	_IOWR('r', 74, struct friostat *)
# define	SIOCZRLST	_IOWR('r', 75, struct frentry *)
# define	SIOCAUTHW	_IOWR('r', 76, struct frauth *)
# define	SIOCAUTHR	_IOWR('r', 77, struct frauth *)
# define	SIOCATHST	_IOWR('r', 78, struct fr_authstat *)
# define	SIOCSTLCK	_IOWR('r', 79, u_int)
# define	SIOCSTPUT	_IOWR('r', 80, struct ipstate_save *)
# define	SIOCSTGET	_IOWR('r', 81, struct ipstate_save *)
# define	SIOCSTGSZ	_IOWR('r', 82, struct natget)
# define	SIOCGFRST	_IOWR('r', 83, struct ipfrstat *)
# define	SIOCIPFL6	_IOWR('r', 84, int)
#else
# define	SIOCADAFR	_IOW(r, 60, struct frentry *)
# define	SIOCRMAFR	_IOW(r, 61, struct frentry *)
# define	SIOCSETFF	_IOW(r, 62, u_int)
# define	SIOCGETFF	_IOR(r, 63, u_int)
# define	SIOCGETFS	_IOWR(r, 64, struct friostat *)
# define	SIOCIPFFL	_IOWR(r, 65, int)
# define	SIOCIPFFB	_IOR(r, 66, int)
# define	SIOCADIFR	_IOW(r, 67, struct frentry *)
# define	SIOCRMIFR	_IOW(r, 68, struct frentry *)
# define	SIOCSWAPA	_IOR(r, 69, u_int)
# define	SIOCINAFR	_IOW(r, 70, struct frentry *)
# define	SIOCINIFR	_IOW(r, 71, struct frentry *)
# define	SIOCFRENB	_IOW(r, 72, u_int)
# define	SIOCFRSYN	_IOW(r, 73, u_int)
# define	SIOCFRZST	_IOWR(r, 74, struct friostat *)
# define	SIOCZRLST	_IOWR(r, 75, struct frentry *)
# define	SIOCAUTHW	_IOWR(r, 76, struct frauth *)
# define	SIOCAUTHR	_IOWR(r, 77, struct frauth *)
# define	SIOCATHST	_IOWR(r, 78, struct fr_authstat *)
# define	SIOCSTLCK	_IOWR(r, 79, u_int)
# define	SIOCSTPUT	_IOWR(r, 80, struct ipstate_save *)
# define	SIOCSTGET	_IOWR(r, 81, struct ipstate_save *)
# define	SIOCSTGSZ	_IOWR(r, 82, struct natget)
# define	SIOCGFRST	_IOWR(r, 83, struct ipfrstat *)
# define	SIOCIPFL6	_IOWR(r, 84, int)
#endif
#define	SIOCADDFR	SIOCADAFR
#define	SIOCDELFR	SIOCRMAFR
#define	SIOCINSFR	SIOCINAFR


typedef	struct	fr_ip	{
	u_32_t	fi_v:4;		/* IP version */
	u_32_t	fi_fl:4;	/* packet flags */
	u_32_t	fi_tos:8;	/* IP packet TOS */
	u_32_t	fi_ttl:8;	/* IP packet TTL */
	u_32_t	fi_p:8;		/* IP packet protocol */
	union	i6addr fi_src;	/* source address from packet */
	union	i6addr fi_dst;	/* destination address from packet */
	u_32_t	fi_optmsk;	/* bitmask composed from IP options */
	u_short	fi_secmsk;	/* bitmask composed from IP security options */
	u_short	fi_auth;	/* authentication code from IP sec. options */
} fr_ip_t;

#define	FI_OPTIONS	(FF_OPTIONS >> 24)
#define	FI_TCPUDP	(FF_TCPUDP >> 24)	/* TCP/UCP implied comparison*/
#define	FI_FRAG		(FF_FRAG >> 24)
#define	FI_SHORT	(FF_SHORT >> 24)
#define	FI_CMP		(FI_OPTIONS|FI_TCPUDP|FI_SHORT)

#define	fi_saddr	fi_src.in4.s_addr
#define	fi_daddr	fi_dst.in4.s_addr


/*
 * These are both used by the state and NAT code to indicate that one port or
 * the other should be treated as a wildcard.
 */
#define	FI_W_SPORT	0x00000100
#define	FI_W_DPORT	0x00000200
#define	FI_WILDP	(FI_W_SPORT|FI_W_DPORT)
#define	FI_W_SADDR	0x00000400
#define	FI_W_DADDR	0x00000800
#define	FI_WILDA	(FI_W_SADDR|FI_W_DADDR)
#define	FI_NEWFR	0x00001000	/* Create a filter rule */
#define	FI_IGNOREPKT	0x00002000	/* Do not treat as a real packet */
#define	FI_NORULE	0x00004000	/* Not direct a result of a rule */

typedef	struct	fr_info	{
	void	*fin_ifp;		/* interface packet is `on' */
	struct	fr_ip	fin_fi;		/* IP Packet summary */
	u_short	fin_data[2];		/* TCP/UDP ports, ICMP code/type */
	u_int	fin_out;		/* in or out ? 1 == out, 0 == in */
	u_short	fin_hlen;		/* length of IP header in bytes */
	u_char	fin_rev;		/* state only: 1 = reverse */
	u_char	fin_tcpf;		/* TCP header flags (SYN, ACK, etc) */
	u_int	fin_icode;		/* ICMP error to return */
	u_32_t	fin_rule;		/* rule # last matched */
	u_32_t	fin_group;		/* group number, -1 for none */
	struct	frentry *fin_fr;	/* last matching rule */
	char	*fin_dp;		/* start of data past IP header */
	u_short	fin_plen;
	u_short	fin_off;
	u_short	fin_dlen;		/* length of data portion of packet */
	u_short	fin_id;			/* IP packet id field */
	u_int	fin_misc;
	mb_t	**fin_mp;		/* pointer to pointer to mbuf */
#if SOLARIS
	void	*fin_qfm;		/* pointer to mblk where pkt starts */
	void	*fin_qif;
#endif
} fr_info_t;

#define	fin_v		fin_fi.fi_v
#define	fin_p		fin_fi.fi_p
#define	fin_saddr	fin_fi.fi_saddr
#define	fin_src		fin_fi.fi_src.in4
#define	fin_daddr	fin_fi.fi_daddr
#define	fin_dst		fin_fi.fi_dst.in4
#define	fin_fl		fin_fi.fi_fl

/*
 * Size for compares on fr_info structures
 */
#define	FI_CSIZE	offsetof(fr_info_t, fin_icode)
#define	FI_LCSIZE	offsetof(fr_info_t, fin_dp)

/*
 * For fin_misc
 */
#define	FM_BADSTATE	0x00000001

/*
 * Size for copying cache fr_info structure
 */
#define	FI_COPYSIZE	offsetof(fr_info_t, fin_dp)

typedef	struct	frdest	{
	void	*fd_ifp;
	union	i6addr	fd_ip6;
	char	fd_ifname[LIFNAMSIZ];
#if SOLARIS
	mb_t	*fd_mp;			/* cache resolver for to/dup-to */
#endif
} frdest_t;

#define	fd_ip	fd_ip6.in4


typedef	struct	frpcmp	{
	int	frp_cmp;	/* data for port comparisons */
	u_short	frp_port;	/* top port for <> and >< */
	u_short	frp_top;	/* top port for <> and >< */
} frpcmp_t;

typedef	struct	frtuc	{
	u_char	ftu_tcpfm;	/* tcp flags mask */
	u_char	ftu_tcpf;	/* tcp flags */
	frpcmp_t	ftu_src;
	frpcmp_t	ftu_dst;
} frtuc_t;

#define	ftu_scmp	ftu_src.frp_cmp
#define	ftu_dcmp	ftu_dst.frp_cmp
#define	ftu_sport	ftu_src.frp_port
#define	ftu_dport	ftu_dst.frp_port
#define	ftu_stop	ftu_src.frp_top
#define	ftu_dtop	ftu_dst.frp_top

typedef	struct	frentry {
	struct	frentry	*fr_next;
	struct	frentry	*fr_grp;
	int	fr_ref;		/* reference count - for grouping */
	void	*fr_ifas[4];
	/*
	 * These are only incremented when a packet  matches this rule and
	 * it is the last match
	 */
	U_QUAD_T	fr_hits;
	U_QUAD_T	fr_bytes;
	/*
	 * Fields after this may not change whilst in the kernel.
	 */
	struct	fr_ip	fr_ip;
	struct	fr_ip	fr_mip;	/* mask structure */


	u_short	fr_icmpm;	/* data for ICMP packets (mask) */
	u_short	fr_icmp;

	u_int	fr_age[2];	/* aging for state */
	frtuc_t	fr_tuc;
	u_32_t	fr_group;	/* group to which this rule belongs */
	u_32_t	fr_grhead;	/* group # which this rule starts */
	u_32_t	fr_flags;	/* per-rule flags && options (see below) */
	u_int	fr_skip;	/* # of rules to skip */
	u_int	fr_loglevel;	/* syslog log facility + priority */
	int	(*fr_func) __P((int, ip_t *, fr_info_t *));	/* call this function */
	int	fr_sap;		/* For solaris only */
	u_char	fr_icode;	/* return ICMP code */
	char	fr_ifnames[4][LIFNAMSIZ];
	struct	frdest	fr_tif;	/* "to" interface */
	struct	frdest	fr_dif;	/* duplicate packet interfaces */
	u_int	fr_cksum;	/* checksum on filter rules for performance */
} frentry_t;

#define	fr_v		fr_ip.fi_v
#define	fr_proto	fr_ip.fi_p
#define	fr_ttl		fr_ip.fi_ttl
#define	fr_tos		fr_ip.fi_tos
#define	fr_tcpfm	fr_tuc.ftu_tcpfm
#define	fr_tcpf		fr_tuc.ftu_tcpf
#define	fr_scmp		fr_tuc.ftu_scmp
#define	fr_dcmp		fr_tuc.ftu_dcmp
#define	fr_dport	fr_tuc.ftu_dport
#define	fr_sport	fr_tuc.ftu_sport
#define	fr_stop		fr_tuc.ftu_stop
#define	fr_dtop		fr_tuc.ftu_dtop
#define	fr_dst		fr_ip.fi_dst.in4
#define	fr_src		fr_ip.fi_src.in4
#define	fr_dmsk		fr_mip.fi_dst.in4
#define	fr_smsk		fr_mip.fi_src.in4
#define	fr_ifname	fr_ifnames[0]
#define	fr_oifname	fr_ifnames[2]
#define	fr_ifa		fr_ifas[0]
#define	fr_oifa		fr_ifas[2]

#define	FR_CMPSIZ	(sizeof(struct frentry) - offsetof(frentry_t, fr_ip))

/*
 * fr_flags
 */
#define	FR_BLOCK	0x00001	/* do not allow packet to pass */
#define	FR_PASS		0x00002	/* allow packet to pass */
#define	FR_OUTQUE	0x00004	/* outgoing packets */
#define	FR_INQUE	0x00008	/* ingoing packets */
#define	FR_LOG		0x00010	/* Log */
#define	FR_LOGB		0x00011	/* Log-fail */
#define	FR_LOGP		0x00012	/* Log-pass */
#define	FR_NOTSRCIP	0x00020	/* not the src IP# */
#define	FR_NOTDSTIP	0x00040	/* not the dst IP# */
#define	FR_RETRST	0x00080	/* Return TCP RST packet - reset connection */
#define	FR_RETICMP	0x00100	/* Return ICMP unreachable packet */
#define	FR_FAKEICMP	0x00180	/* Return ICMP unreachable with fake source */
#define	FR_NOMATCH	0x00200	/* no match occured */
#define	FR_ACCOUNT	0x00400	/* count packet bytes */
#define	FR_KEEPFRAG	0x00800	/* keep fragment information */
#define	FR_KEEPSTATE	0x01000	/* keep `connection' state information */
#define	FR_INACTIVE	0x02000
#define	FR_QUICK	0x04000	/* match & stop processing list */
#define	FR_FASTROUTE	0x08000	/* bypass normal routing */
#define	FR_CALLNOW	0x10000	/* call another function (fr_func) if matches */
#define	FR_DUP		0x20000	/* duplicate packet */
#define	FR_LOGORBLOCK	0x40000	/* block the packet if it can't be logged */
#define	FR_LOGBODY	0x80000	/* Log the body */
#define	FR_LOGFIRST	0x100000	/* Log the first byte if state held */
#define	FR_AUTH		0x200000	/* use authentication */
#define	FR_PREAUTH	0x400000	/* require preauthentication */
#define	FR_DONTCACHE	0x800000	/* don't cache the result */

#define	FR_LOGMASK	(FR_LOG|FR_LOGP|FR_LOGB)
#define	FR_RETMASK	(FR_RETICMP|FR_RETRST|FR_FAKEICMP)

/*
 * These correspond to #define's for FI_* and are stored in fr_flags
 */
#define	FF_OPTIONS	0x01000000
#define	FF_TCPUDP	0x02000000
#define	FF_FRAG		0x04000000
#define	FF_SHORT	0x08000000
/*
 * recognized flags for SIOCGETFF and SIOCSETFF, and get put in fr_flags
 */
#define	FF_LOGPASS	0x10000000
#define	FF_LOGBLOCK	0x20000000
#define	FF_LOGNOMATCH	0x40000000
#define	FF_LOGGING	(FF_LOGPASS|FF_LOGBLOCK|FF_LOGNOMATCH)
#define	FF_BLOCKNONIP	0x80000000	/* Solaris2 Only */

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
#if SOLARIS
	u_long	fr_notdata;	/* PROTO/PCPROTO that have no data */
	u_long	fr_nodata;	/* mblks that have no data */
	u_long	fr_bad;		/* bad IP packets to the filter */
	u_long	fr_notip;	/* packets passed through no on ip queue */
	u_long	fr_drop;	/* packets dropped - no info for them! */
	u_long	fr_copy;	/* messages copied due to db_ref > 1 */
#endif
	u_long	fr_ipv6[2];	/* IPv6 packets in/out */
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
	struct	frentry		*f_fin6[2];
	struct	frentry		*f_fout6[2];
	struct	frentry		*f_acctin6[2];
	struct	frentry		*f_acctout6[2];
	struct	frentry		*f_auth;
	struct	frgroup		*f_groups[3][2];
	u_long	f_froute[2];
	int	f_defpass;	/* default pass - from fr_pass */
	char	f_active;	/* 1 or 0 - active rule set */
	char	f_running;	/* 1 if running, else 0 */
	char	f_logging;	/* 1 if enabled, else 0 */
	char	f_version[32];	/* version string */
	int	f_locks[4];
} friostat_t;

typedef struct	optlist {
	u_short ol_val;
	int	ol_bit;
} optlist_t;


/*
 * Group list structure.
 */
typedef	struct frgroup {
	u_32_t	fg_num;
	struct	frgroup	*fg_next;
	struct	frentry	*fg_head;
	struct	frentry	**fg_start;
} frgroup_t;


/*
 * Log structure.  Each packet header logged is prepended by one of these.
 * Following this in the log records read from the device will be an ipflog
 * structure which is then followed by any packet data.
 */
typedef	struct	iplog	{
	u_32_t		ipl_magic;
	u_int		ipl_count;
	struct	timeval	ipl_tv;
	size_t		ipl_dsize;
	struct	iplog	*ipl_next;
} iplog_t;

#define	ipl_sec		ipl_tv.tv_sec
#define	ipl_usec	ipl_tv.tv_usec

#define IPL_MAGIC	0x49504c4d /* 'IPLM' */
#define	IPLOG_SIZE	sizeof(iplog_t)

typedef	struct	ipflog	{
#if (defined(NetBSD) && (NetBSD <= 1991011) && (NetBSD >= 199603)) || \
        (defined(OpenBSD) && (OpenBSD >= 199603))
	char	fl_ifname[LIFNAMSIZ];
#else
	u_int	fl_unit;
	char	fl_ifname[LIFNAMSIZ];
#endif
	u_char	fl_plen;	/* extra data after hlen */
	u_char	fl_hlen;	/* length of IP headers saved */
	u_short	fl_loglevel;	/* syslog log level */
	u_32_t	fl_rule;
	u_32_t	fl_group;
	u_32_t	fl_flags;
	u_char	fl_dir;
	u_char	fl_pad[3];
} ipflog_t;


#ifndef	ICMP_UNREACH_FILTER
# define	ICMP_UNREACH_FILTER	13
#endif

#ifndef	IPF_LOGGING
# define	IPF_LOGGING	0
#endif
#ifndef	IPF_DEFAULT_PASS
# define	IPF_DEFAULT_PASS	FR_PASS
#endif

#define	IPMINLEN(i, h)	((i)->ip_len >= ((i)->ip_hl * 4 + sizeof(struct h)))
#define	IPLLOGSIZE	8192

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
#define	IPL_NAT		IPNAT_NAME
#define	IPL_STATE	IPSTATE_NAME
#define	IPL_AUTH	IPAUTH_NAME

#define	IPL_LOGIPF	0	/* Minor device #'s for accessing logs */
#define	IPL_LOGNAT	1
#define	IPL_LOGSTATE	2
#define	IPL_LOGAUTH	3
#define	IPL_LOGMAX	3

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
    (defined(NetBSD1_2) && NetBSD1_2 > 1) || (defined(__FreeBSD_version) && \
     (__FreeBSD_version >= 500011))
# if (NetBSD >= 199905) || (__FreeBSD_version >= 503000)
#  define PFIL_HOOKS
# endif
# ifdef PFIL_HOOKS
#  define NETBSD_PF
# endif
#endif


#ifndef	_KERNEL
extern	char	*get_ifname __P((struct ifnet *));
extern	int	fr_check __P((ip_t *, int, void *, int, mb_t **));
extern	int	(*fr_checkp) __P((ip_t *, int, void *, int, mb_t **));
extern	int	send_reset __P((ip_t *, fr_info_t *));
extern	int	send_icmp_err __P((ip_t *, int, fr_info_t *, int));
extern	int	ipf_log __P((void));
extern	struct	ifnet *get_unit __P((char *, int));
extern	int	mbuflen __P((mb_t *));
# if defined(__NetBSD__) || defined(__OpenBSD__) || \
	  (_BSDI_VERSION >= 199701) || (__FreeBSD_version >= 300000)
extern	int	iplioctl __P((dev_t, u_long, caddr_t, int));
# else
extern	int	iplioctl __P((dev_t, int, caddr_t, int));
# endif
extern	int	iplopen __P((dev_t, int));
extern	int	iplclose __P((dev_t, int));
#else /* #ifndef _KERNEL */
# if defined(__NetBSD__) && defined(PFIL_HOOKS)
extern	void	ipfilterattach __P((int));
# endif
extern	int	iplattach __P((void));
extern	int	ipl_enable __P((void));
extern	int	ipl_disable __P((void));
extern	int	send_icmp_err __P((ip_t *, int, fr_info_t *, int));
extern	int	send_reset __P((ip_t *, fr_info_t *));
# if	SOLARIS
extern	int	fr_check __P((ip_t *, int, void *, int, qif_t *, mb_t **));
extern	int	(*fr_checkp) __P((ip_t *, int, void *,
				  int, qif_t *, mb_t **));
#  if SOLARIS2 >= 7
extern	int	iplioctl __P((dev_t, int, intptr_t, int, cred_t *, int *));
#  else
extern	int	iplioctl __P((dev_t, int, int *, int, cred_t *, int *));
#  endif
extern	int	iplopen __P((dev_t *, int, int, cred_t *));
extern	int	iplclose __P((dev_t, int, int, cred_t *));
extern	int	ipfsync __P((void));
extern	int	ipfr_fastroute __P((ip_t *, mblk_t *, mblk_t **,
				    fr_info_t *, frdest_t *));
extern	void	copyin_mblk __P((mblk_t *, size_t, size_t, char *));
extern	void	copyout_mblk __P((mblk_t *, size_t, size_t, char *));
extern	int	fr_qin __P((queue_t *, mblk_t *));
extern	int	fr_qout __P((queue_t *, mblk_t *));
extern	int	iplread __P((dev_t, struct uio *, cred_t *));
# else /* SOLARIS */
extern	int	fr_check __P((ip_t *, int, void *, int, mb_t **));
extern	int	(*fr_checkp) __P((ip_t *, int, void *, int, mb_t **));
extern	int	ipfr_fastroute __P((mb_t *, mb_t **, fr_info_t *, frdest_t *));
extern	size_t	mbufchainlen __P((mb_t *));
#  ifdef	__sgi
#   include <sys/cred.h>
extern	int	iplioctl __P((dev_t, int, caddr_t, int, cred_t *, int *));
extern	int	iplopen __P((dev_t *, int, int, cred_t *));
extern	int	iplclose __P((dev_t, int, int, cred_t *));
extern	int	iplread __P((dev_t, struct uio *, cred_t *));
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
extern	int	iplioctl __P((struct cdev *, u_long, caddr_t, int, struct thread *));
#    else
extern	int	iplioctl __P((dev_t, int, caddr_t, int, struct thread *));
#    endif
extern	int	iplopen __P((struct cdev *, int, int, struct thread *));
extern	int	iplclose __P((struct cdev *, int, int, struct thread *));
#   else
#    ifndef	linux
extern	int	iplopen __P((dev_t, int));
extern	int	iplclose __P((dev_t, int));
extern	int	iplioctl __P((dev_t, int, caddr_t, int));
#    else
extern	int	iplioctl(struct inode *, struct file *, u_int, u_long);
extern	int	iplopen __P((struct inode *, struct file *));
extern	void	iplclose __P((struct inode *, struct file *));
#    endif /* !linux */
#   endif /* (_BSDI_VERSION >= 199510) */
#   if	BSD >= 199306
extern	int	iplread __P((struct cdev *, struct uio *, int));
#   else
#    ifndef linux
extern	int	iplread __P((dev_t, struct uio *));
#    else
extern	int	iplread(struct inode *, struct file *, char *, int);
#    endif /* !linux */
#   endif /* BSD >= 199306 */
#  endif /* __ sgi */
# endif /* SOLARIS */
#endif /* #ifndef _KERNEL */

extern	char	*memstr __P((char *, char *, int, int));
extern	void	fixskip __P((frentry_t **, frentry_t *, int));
extern	int	countbits __P((u_32_t));
extern	int	ipldetach __P((void));
extern	u_short	ipf_cksum __P((u_short *, int));
extern	int	ircopyptr __P((void *, void *, size_t));
extern	int	iwcopyptr __P((void *, void *, size_t));

extern	void	ipflog_init __P((void));
extern	int	ipflog_clear __P((minor_t));
extern	int	ipflog __P((u_int, ip_t *, fr_info_t *, mb_t *));
extern	int	ipllog __P((int, fr_info_t *, void **, size_t *, int *, int));
extern	int	ipflog_read __P((minor_t, struct uio *));

extern	int	frflush __P((minor_t, int, int));
extern	void	frsync __P((void));
extern	frgroup_t *fr_addgroup __P((u_32_t, frentry_t *, minor_t, int));
extern	void	fr_delgroup __P((u_32_t, u_32_t, minor_t, int));
extern	frgroup_t *fr_findgroup __P((u_32_t, u_32_t, minor_t, int,
				    frgroup_t ***));

extern	int	fr_copytolog __P((int, char *, int));
extern	void	fr_forgetifp __P((void *));
extern	void	fr_getstat __P((struct friostat *));
extern	int	fr_ifpaddr __P((int, void *, struct in_addr *));
extern	int	fr_lock __P((caddr_t, int *));
extern  int	fr_makefrip __P((int, ip_t *, fr_info_t *));
extern	u_short	fr_tcpsum __P((mb_t *, ip_t *, tcphdr_t *));
extern	int	fr_scanlist __P((u_32_t, ip_t *, fr_info_t *, void *));
extern	int	fr_tcpudpchk __P((frtuc_t *, fr_info_t *));
extern	int	fr_verifysrc __P((struct in_addr, void *));

extern	int	ipl_unreach;
extern	int	fr_running;
extern	u_long	ipl_frouteok[2];
extern	int	fr_pass;
extern	int	fr_flags;
extern	int	fr_active;
extern	int	fr_chksrc;
extern	int	fr_minttl;
extern	int	fr_minttllog;
extern	fr_info_t	frcache[2];
extern	char	ipfilter_version[];
extern	iplog_t	**iplh[IPL_LOGMAX+1], *iplt[IPL_LOGMAX+1];
extern	size_t	iplused[IPL_LOGMAX + 1];
extern	struct frentry *ipfilter[2][2], *ipacct[2][2];
#ifdef	USE_INET6
extern	struct frentry *ipfilter6[2][2], *ipacct6[2][2];
extern	int	icmptoicmp6types[ICMP_MAXTYPE+1];
extern	int	icmptoicmp6unreach[ICMP_MAX_UNREACH];
#endif
extern	struct frgroup *ipfgroups[3][2];
extern	struct filterstats frstats[];

#endif	/* __IP_FIL_H__ */
