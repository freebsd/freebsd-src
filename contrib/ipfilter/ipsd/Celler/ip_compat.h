/*	$NetBSD$	*/

/*
 * (C)opyright 1995 by Darren Reed.
 *
 * This code may be freely distributed as long as it retains this notice
 * and is not changed in any way.  The author accepts no responsibility
 * for the use of this software.  I hate legaleese, don't you ?
 *
 * @(#)ip_compat.h	1.1 9/14/95
 */

/*
 * These #ifdef's are here mainly for linux, but who knows, they may
 * not be in other places or maybe one day linux will grow up and some
 * of these will turn up there too.
 */
#ifndef	ICMP_UNREACH
# define	ICMP_UNREACH	ICMP_DEST_UNREACH
#endif
#ifndef	ICMP_SOURCEQUENCH
# define	ICMP_SOURCEQUENCH	ICMP_SOURCE_QUENCH
#endif
#ifndef	ICMP_TIMXCEED
# define	ICMP_TIMXCEED	ICMP_TIME_EXCEEDED
#endif
#ifndef	ICMP_PARAMPROB
# define	ICMP_PARAMPROB	ICMP_PARAMETERPROB
#endif
#ifndef	IPVERSION
# define	IPVERSION	4
#endif
#ifndef	IPOPT_MINOFF
# define	IPOPT_MINOFF	4
#endif
#ifndef	IPOPT_COPIED
# define	IPOPT_COPIED(x)	((x)&0x80)
#endif
#ifndef	IPOPT_EOL
# define	IPOPT_EOL	0
#endif
#ifndef	IPOPT_NOP
# define	IPOPT_NOP	1
#endif
#ifndef	IP_MF
# define	IP_MF	((u_short)0x2000)
#endif
#ifndef	ETHERTYPE_IP
# define	ETHERTYPE_IP	((u_short)0x0800)
#endif
#ifndef	TH_FIN
# define	TH_FIN	0x01
#endif
#ifndef	TH_SYN
# define	TH_SYN	0x02
#endif
#ifndef	TH_RST
# define	TH_RST	0x04
#endif
#ifndef	TH_PUSH
# define	TH_PUSH	0x08
#endif
#ifndef	TH_ACK
# define	TH_ACK	0x10
#endif
#ifndef	TH_URG
# define	TH_URG	0x20
#endif
#ifndef	IPOPT_EOL
# define	IPOPT_EOL	0
#endif
#ifndef	IPOPT_NOP
# define	IPOPT_NOP	1
#endif
#ifndef	IPOPT_RR
# define	IPOPT_RR	7
#endif
#ifndef	IPOPT_TS
# define	IPOPT_TS	68
#endif
#ifndef	IPOPT_SECURITY
# define	IPOPT_SECURITY	130
#endif
#ifndef	IPOPT_LSRR
# define	IPOPT_LSRR	131
#endif
#ifndef	IPOPT_SATID
# define	IPOPT_SATID	136
#endif
#ifndef	IPOPT_SSRR
# define	IPOPT_SSRR	137
#endif
#ifndef	IPOPT_SECUR_UNCLASS
# define	IPOPT_SECUR_UNCLASS	((u_short)0x0000)
#endif
#ifndef	IPOPT_SECUR_CONFID
# define	IPOPT_SECUR_CONFID	((u_short)0xf135)
#endif
#ifndef	IPOPT_SECUR_EFTO
# define	IPOPT_SECUR_EFTO	((u_short)0x789a)
#endif
#ifndef	IPOPT_SECUR_MMMM
# define	IPOPT_SECUR_MMMM	((u_short)0xbc4d)
#endif
#ifndef	IPOPT_SECUR_RESTR
# define	IPOPT_SECUR_RESTR	((u_short)0xaf13)
#endif
#ifndef	IPOPT_SECUR_SECRET
# define	IPOPT_SECUR_SECRET	((u_short)0xd788)
#endif
#ifndef IPOPT_SECUR_TOPSECRET
# define	IPOPT_SECUR_TOPSECRET	((u_short)0x6bc5)
#endif

#ifdef linux
# define	icmp	icmphdr
# define	icmp_type	type
# define	icmp_code	code

/*
 * From /usr/include/netinet/ip_var.h
 * !%@#!$@# linux...
 */
struct ipovly {
	caddr_t	ih_next, ih_prev;	/* for protocol sequence q's */
	u_char	ih_x1;			/* (unused) */
	u_char	ih_pr;			/* protocol */
	short	ih_len;			/* protocol length */
	struct	in_addr ih_src;		/* source internet address */
	struct	in_addr ih_dst;		/* destination internet address */
};

typedef	struct	{
	__u16	th_sport;
	__u16	th_dport;
	__u32	th_seq;
	__u32	th_ack;
# if defined(__i386__) || defined(__MIPSEL__) || defined(__alpha__) ||\
    defined(vax)
	__u8	th_res:4;
	__u8	th_off:4;
#else
	__u8	th_off:4;
	__u8	th_res:4;
#endif
	__u8	th_flags;
	__u16	th_win;
	__u16	th_sum;
	__u16	th_urp;
} tcphdr_t;

typedef	struct	{
	__u16	uh_sport;
	__u16	uh_dport;
	__s16	uh_ulen;
	__u16	uh_sum;
} udphdr_t;

typedef	struct	{
# if defined(__i386__) || defined(__MIPSEL__) || defined(__alpha__) ||\
    defined(vax)
	__u8	ip_hl:4;
	__u8	ip_v:4;
# else
	__u8	ip_hl:4;
	__u8	ip_v:4;
# endif
	__u8	ip_tos;
	__u16	ip_len;
	__u16	ip_id;
	__u16	ip_off;
	__u8	ip_ttl;
	__u8	ip_p;
	__u16	ip_sum;
	struct	in_addr	ip_src;
	struct	in_addr	ip_dst;
} ip_t;

typedef	struct	{
	__u8	ether_dhost[6];
	__u8	ether_shost[6];
	__u16	ether_type;
} ether_header_t;

# define	bcopy(a,b,c)	memmove(b,a,c)
# define	bcmp(a,b,c)	memcmp(a,b,c)

# define	ifnet	device

#else

typedef	struct	udphdr	udphdr_t;
typedef	struct	tcphdr	tcphdr_t;
typedef	struct	ip	ip_t;
typedef	struct	ether_header	ether_header_t;

#endif

#ifdef	solaris
# define	bcopy(a,b,c)	memmove(b,a,c)
# define	bcmp(a,b,c)	memcmp(a,b,c)
# define	bzero(a,b)	memset(a,0,b)
#endif
