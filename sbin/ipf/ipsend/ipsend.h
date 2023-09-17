
/*
 * ipsend.h (C) 1997-1998 Darren Reed
 *
 * This was written to test what size TCP fragments would get through
 * various TCP/IP packet filters, as used in IP firewalls.  In certain
 * conditions, enough of the TCP header is missing for unpredictable
 * results unless the filter is aware that this can happen.
 *
 * The author provides this program as-is, with no guarantee for its
 * suitability for any specific purpose.  The author takes no responsibility
 * for the misuse/abuse of this program and provides it for the sole purpose
 * of testing packet filter policies.  This file maybe distributed freely
 * providing it is not modified and that this notice remains in tact.
 *
 */
#ifndef	__P
#  define	__P(x)	x
#endif

#include <net/if.h>

#include "ipf.h"
/* XXX:	The following is needed by tcpip.h */
#include <netinet/ip_var.h>
#include "netinet/tcpip.h"
#include "ipt.h"

extern	int	resolve(char *, char *);
extern	int	arp(char *, char *);
extern	u_short	chksum(u_short *, int);
extern	int	send_ether(int, char *, int, struct in_addr);
extern	int	send_ip(int, int, ip_t *, struct in_addr, int);
extern	int	send_tcp(int, int, ip_t *, struct in_addr);
extern	int	send_udp(int, int, ip_t *, struct in_addr);
extern	int	send_icmp(int, int, ip_t *, struct in_addr);
extern	int	send_packet(int, int, ip_t *, struct in_addr);
extern	int	send_packets(char *, int, ip_t *, struct in_addr);
extern	u_short	ipseclevel(char *);
extern	u_32_t	buildopts(char *, char *, int);
extern	int	addipopt(char *, struct ipopt_names *, int, char *);
extern	int	initdevice(char *, int);
extern	int	sendip(int, char *, int);
extern	struct	tcpcb	*find_tcp(int, struct tcpiphdr *);
extern	int	ip_resend(char *, int, struct ipread *, struct in_addr, char *);

extern	void	ip_test1(char *, int, ip_t *, struct in_addr, int);
extern	void	ip_test2(char *, int, ip_t *, struct in_addr, int);
extern	void	ip_test3(char *, int, ip_t *, struct in_addr, int);
extern	void	ip_test4(char *, int, ip_t *, struct in_addr, int);
extern	void	ip_test5(char *, int, ip_t *, struct in_addr, int);
extern	void	ip_test6(char *, int, ip_t *, struct in_addr, int);
extern	void	ip_test7(char *, int, ip_t *, struct in_addr, int);
extern	int	do_socket(char *, int, struct tcpiphdr *, struct in_addr);
extern	int	kmemcpy(char *, void *, int);

#define	KMCPY(a,b,c)	kmemcpy((char *)(a), (void *)(b), (int)(c))

#ifndef	OPT_RAW
#define	OPT_RAW	0x80000
#endif
