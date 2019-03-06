/*	$FreeBSD$	*/

/*
 * (C)opyright 1995 by Darren Reed.
 *
 * This code may be freely distributed as long as it retains this notice
 * and is not changed in any way.  The author accepts no responsibility
 * for the use of this software.  I hate legaleese, don't you ?
 *
 * @(#)ip_compat.h	1.2 12/7/95
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


typedef	struct	udphdr	udphdr_t;
typedef	struct	tcphdr	tcphdr_t;
typedef	struct	ip	ip_t;
typedef	struct	ether_header	ether_header_t;


#if defined(__SVR4) || defined(__svr4__)
# define	bcopy(a,b,c)	memmove(b,a,c)
# define	bcmp(a,b,c)	memcmp(a,b,c)
# define	bzero(a,b)	memset(a,0,b)
#endif
