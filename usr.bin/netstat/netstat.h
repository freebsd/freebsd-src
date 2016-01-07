/*-
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
extern int	hflag;	/* show counters in human readable format */
extern int	iflag;	/* show interfaces */
extern int	Lflag;	/* show size of listen queues */
extern int	mflag;	/* show memory stats */
extern int	noutputs;	/* how much outputs before we exit */
extern int	numeric_addr;	/* show addresses numerically */
extern int	numeric_port;	/* show ports numerically */
extern int	rflag;	/* show routing tables (or routing stats) */
extern int	Rflag;	/* show flowid / RSS information */
extern int	sflag;	/* show protocol statistics */
extern int	Tflag;  /* show TCP control block info */
extern int	Wflag;	/* wide display */
extern int	xflag;	/* extended display, includes all socket buffer info */
extern int	zflag;	/* zero stats */

extern int	interval; /* repeat interval for i/f stats */

extern char	*interface; /* desired i/f for stats, or NULL for all i/fs */
extern int	unit;	/* unit number for above */

extern int	live;	/* true if we are examining a live system */

struct nlist;
int	fetch_stats(const char *sysctlname, u_long addr, void *stats,
	    size_t len, int (*kreadfn)(u_long, void *, size_t));
int	kread(u_long addr, void *buf, size_t size);
uint64_t kread_counter(u_long addr);
int	kread_counters(u_long addr, void *buf, size_t size);
int	kresolve_list(struct nlist *);
const char *plural(uintmax_t);
const char *plurales(uintmax_t);
const char *pluralies(uintmax_t);

struct sockaddr;
struct socket;
struct xsocket;
int	sotoxsocket(struct socket *, struct xsocket *);
void	protopr(u_long, const char *, int, int);
void	tcp_stats(u_long, const char *, int, int);
void	udp_stats(u_long, const char *, int, int);
#ifdef SCTP
void	sctp_protopr(u_long, const char *, int, int);
void	sctp_stats(u_long, const char *, int, int);
#endif
void	arp_stats(u_long, const char *, int, int);
void	ip_stats(u_long, const char *, int, int);
void	icmp_stats(u_long, const char *, int, int);
void	igmp_stats(u_long, const char *, int, int);
void	pim_stats(u_long, const char *, int, int);
void	carp_stats(u_long, const char *, int, int);
void	pfsync_stats(u_long, const char *, int, int);
#ifdef IPSEC
void	ipsec_stats(u_long, const char *, int, int);
void	esp_stats(u_long, const char *, int, int);
void	ah_stats(u_long, const char *, int, int);
void	ipcomp_stats(u_long, const char *, int, int);
#endif

#ifdef INET6
void	ip6_stats(u_long, const char *, int, int);
void	ip6_ifstats(char *);
void	icmp6_stats(u_long, const char *, int, int);
void	icmp6_ifstats(char *);
void	pim6_stats(u_long, const char *, int, int);
void	rip6_stats(u_long, const char *, int, int);
void	mroute6pr(void);
void	mrt6_stats(void);

struct sockaddr_in6;
struct in6_addr;
void in6_fillscopeid(struct sockaddr_in6 *);
char *routename6(struct sockaddr_in6 *);
const char *netname6(struct sockaddr_in6 *, struct in6_addr *);
void	inet6print(struct in6_addr *, int, const char *, int);
#endif /*INET6*/

#ifdef IPSEC
void	pfkey_stats(u_long, const char *, int, int);
#endif

void	mbpr(void *, u_long);

void	netisr_stats(void *);

void	hostpr(u_long, u_long);
void	impstats(u_long, u_long);

void	intpr(int, void (*)(char *), int);

void	pr_rthdr(int);
void	pr_family(int);
void	rt_stats(void);
void	flowtable_stats(void);
char	*ipx_pnet(struct sockaddr *);
char	*ipx_phost(struct sockaddr *);
char	*ns_phost(struct sockaddr *);
void	upHex(char *);

char	*routename(in_addr_t);
char	*netname(in_addr_t, in_addr_t);
char	*atalk_print(struct sockaddr *, int);
char	*atalk_print2(struct sockaddr *, struct sockaddr *, int);
char	*ipx_print(struct sockaddr *);
char	*ns_print(struct sockaddr *);
void	routepr(int, int);

void	ipxprotopr(u_long, const char *, int, int);
void	spx_stats(u_long, const char *, int, int);
void	ipx_stats(u_long, const char *, int, int);
void	ipxerr_stats(u_long, const char *, int, int);

void	nsprotopr(u_long, const char *, int, int);
void	spp_stats(u_long, const char *, int, int);
void	idp_stats(u_long, const char *, int, int);
void	nserr_stats(u_long, const char *, int, int);

void	atalkprotopr(u_long, const char *, int, int);
void	ddp_stats(u_long, const char *, int, int);

#ifdef NETGRAPH
void	netgraphprotopr(u_long, const char *, int, int);
#endif

void	unixpr(u_long, u_long, u_long, u_long, u_long);

void	esis_stats(u_long, const char *, int, int);
void	clnp_stats(u_long, const char *, int, int);
void	cltp_stats(u_long, const char *, int, int);
void	iso_protopr(u_long, const char *, int, int);
void	iso_protopr1(u_long, int);
void	tp_protopr(u_long, const char *, int, int);
void	tp_inproto(u_long);
void	tp_stats(caddr_t, caddr_t);

void	mroutepr(void);
void	mrt_stats(void);
void	bpf_stats(char *);
