/*
 * ipsend.h (C) 1997 Darren Reed
 *
 * This was written to test what size TCP fragments would get through
 * various TCP/IP packet filters, as used in IP firewalls.  In certain
 * conditions, enough of the TCP header is missing for unpredictable
 * results unless the filter is aware that this can happen.
 *
 * The author provides this program as-is, with no gaurantee for its
 * suitability for any specific purpose.  The author takes no responsibility
 * for the misuse/abuse of this program and provides it for the sole purpose
 * of testing packet filter policies.  This file maybe distributed freely
 * providing it is not modified and that this notice remains in tact.
 *
 */
#ifndef	__P
# ifdef	__STDC__
#  define	__P(x)	x
# else
#  define	__P(x)	()
# endif
#endif

#include "ip_compat.h"
#ifdef	linux
#include <linux/sockios.h>
#endif
#include "tcpip.h"
#include "ipt.h"
#include "ipf.h"

extern	int	resolve __P((char *, char *));
extern	int	arp __P((char *, char *));
extern	u_short	chksum __P((u_short *, int));
extern	int	send_ether __P((int, char *, int, struct in_addr));
extern	int	send_ip __P((int, int, ip_t *, struct in_addr, int));
extern	int	send_tcp __P((int, int, ip_t *, struct in_addr));
extern	int	send_udp __P((int, int, ip_t *, struct in_addr));
extern	int	send_icmp __P((int, int, ip_t *, struct in_addr));
extern	int	send_packet __P((int, int, ip_t *, struct in_addr));
extern	int	send_packets __P((char *, int, ip_t *, struct in_addr));
extern	u_short	seclevel __P((char *));
extern	u_32_t	buildopts __P((char *, char *, int));
extern	int	addipopt __P((char *, struct ipopt_names *, int, char *));
extern	int	initdevice __P((char *, int, int));
extern	int	sendip __P((int, char *, int));
#ifdef	linux
extern	struct	sock	*find_tcp __P((int, struct tcpiphdr *));
#else
extern	struct	tcpcb	*find_tcp __P((int, struct tcpiphdr *));
#endif
extern	int	ip_resend __P((char *, int, struct ipread *, struct in_addr, char *));

extern	void	ip_test1 __P((char *, int, ip_t *, struct in_addr, int));
extern	void	ip_test2 __P((char *, int, ip_t *, struct in_addr, int));
extern	void	ip_test3 __P((char *, int, ip_t *, struct in_addr, int));
extern	void	ip_test4 __P((char *, int, ip_t *, struct in_addr, int));
extern	void	ip_test5 __P((char *, int, ip_t *, struct in_addr, int));
extern	void	ip_test6 __P((char *, int, ip_t *, struct in_addr, int));
extern	void	ip_test7 __P((char *, int, ip_t *, struct in_addr, int));
extern	int	do_socket __P((char *, int, struct tcpiphdr *, struct in_addr));
extern	int	openkmem __P((void));
extern	int	kmemcpy __P((char *, void *, int));

#define	KMCPY(a,b,c)	kmemcpy((char *)(a), (void *)(b), (int)(c))

#define	OPT_RAW	0x80000
