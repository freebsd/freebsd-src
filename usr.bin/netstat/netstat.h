/*
 * Copyright (c) 1992, 1993
 *	Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)netstat.h	8.2 (Berkeley) 1/4/94
 * $FreeBSD$
 */

#include <sys/cdefs.h>

extern int	Aflag;	/* show addresses of protocol control block */
extern int	aflag;	/* show all sockets (including servers) */
extern int	bflag;	/* show i/f total bytes in/out */
extern int	dflag;	/* show i/f dropped packets */
extern int	gflag;	/* show group (multicast) routing or stats */
extern int	iflag;	/* show interfaces */
extern int	Lflag;	/* show size of listen queues */
extern int	mflag;	/* show memory stats */
extern int	numeric_addr;	/* show addresses numerically */
extern int	numeric_port;	/* show ports numerically */
extern int	rflag;	/* show routing tables (or routing stats) */
extern int	sflag;	/* show protocol statistics */
extern int	tflag;	/* show i/f watchdog timers */
extern int	Wflag;	/* wide display */
extern int	zflag;	/* zero stats */

extern int	interval; /* repeat interval for i/f stats */

extern char	*interface; /* desired i/f for stats, or NULL for all i/fs */
extern int	unit;	/* unit number for above */

extern int	af;	/* address family */

int	kread (u_long addr, char *buf, int size);
char	*plural (int);
char	*plurales (int);

void	protopr (u_long, char *, int);
void	tcp_stats (u_long, char *, int);
void	udp_stats (u_long, char *, int);
void	ip_stats (u_long, char *, int);
void	icmp_stats (u_long, char *, int);
void	igmp_stats (u_long, char *, int);
void	pim_stats (u_long, char *, int);
#ifdef IPSEC
void	ipsec_stats (u_long, char *, int);
#endif

#ifdef INET6
void	ip6_stats (u_long, char *, int);
void	ip6_ifstats (char *);
void	icmp6_stats (u_long, char *, int);
void	icmp6_ifstats (char *);
void	pim6_stats (u_long, char *, int);
void	rip6_stats (u_long, char *, int);
void	mroute6pr (u_long, u_long);
void	mrt6_stats (u_long);

struct sockaddr_in6;
struct in6_addr;
char *routename6 (struct sockaddr_in6 *);
char *netname6 (struct sockaddr_in6 *, struct in6_addr *);
#endif /*INET6*/

#ifdef IPSEC
void	pfkey_stats (u_long, char *, int);
#endif

void	bdg_stats (u_long, char *, int);

void	mbpr (u_long, u_long, u_long, u_long);

void	hostpr (u_long, u_long);
void	impstats (u_long, u_long);

void	intpr (int, u_long, void (*)(char *));

void	pr_rthdr (int);
void	pr_family (int);
void	rt_stats (u_long, u_long);
char	*ipx_pnet (struct sockaddr *);
char	*ipx_phost (struct sockaddr *);
char	*ns_phost (struct sockaddr *);
void	upHex (char *);

char	*routename (u_long);
char	*netname (u_long, u_long);
char	*atalk_print (struct sockaddr *, int);
char	*atalk_print2 (struct sockaddr *, struct sockaddr *, int);
char	*ipx_print (struct sockaddr *);
char	*ns_print (struct sockaddr *);
void	routepr (u_long);

void	ipxprotopr (u_long, char *, int);
void	spx_stats (u_long, char *, int);
void	ipx_stats (u_long, char *, int);
void	ipxerr_stats (u_long, char *, int);

void	nsprotopr (u_long, char *, int);
void	spp_stats (u_long, char *, int);
void	idp_stats (u_long, char *, int);
void	nserr_stats (u_long, char *, int);

void	atalkprotopr (u_long, char *, int);
void	ddp_stats (u_long, char *, int);

void	netgraphprotopr (u_long, char *, int);

void	unixpr (void);

void	esis_stats (u_long, char *, int);
void	clnp_stats (u_long, char *, int);
void	cltp_stats (u_long, char *, int);
void	iso_protopr (u_long, char *, int);
void	iso_protopr1 (u_long, int);
void	tp_protopr (u_long, char *, int);
void	tp_inproto (u_long);
void	tp_stats (caddr_t, caddr_t);

void	mroutepr (u_long, u_long);
void	mrt_stats (u_long);

