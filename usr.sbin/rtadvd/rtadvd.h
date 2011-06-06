/*	$FreeBSD$	*/
/*	$KAME: rtadvd.h,v 1.26 2003/08/05 12:34:23 itojun Exp $	*/

/*
 * Copyright (C) 1998 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#define IN6ADDR_LINKLOCAL_ALLNODES_INIT				\
	{{{ 0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	\
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 }}}

#define IN6ADDR_LINKLOCAL_ALLROUTERS_INIT			\
	{{{ 0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	\
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02 }}}

#define IN6ADDR_SITELOCAL_ALLROUTERS_INIT			\
	{{{ 0xff, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	\
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02 }}}

extern struct sockaddr_in6 sin6_linklocal_allnodes;
extern struct sockaddr_in6 sin6_linklocal_allrouters;
extern struct sockaddr_in6 sin6_sitelocal_allrouters;

/*
 * RFC 3542 API deprecates IPV6_PKTINFO in favor of
 * IPV6_RECVPKTINFO
 */
#ifndef IPV6_RECVPKTINFO
#ifdef IPV6_PKTINFO
#define IPV6_RECVPKTINFO	IPV6_PKTINFO
#endif
#endif

/*
 * RFC 3542 API deprecates IPV6_HOPLIMIT in favor of
 * IPV6_RECVHOPLIMIT
 */
#ifndef IPV6_RECVHOPLIMIT
#ifdef IPV6_HOPLIMIT
#define IPV6_RECVHOPLIMIT	IPV6_HOPLIMIT
#endif
#endif

/* protocol constants and default values */
#define DEF_MAXRTRADVINTERVAL 600
#define DEF_ADVLINKMTU 0
#define DEF_ADVREACHABLETIME 0
#define DEF_ADVRETRANSTIMER 0
#define DEF_ADVCURHOPLIMIT 64
#define DEF_ADVVALIDLIFETIME 2592000
#define DEF_ADVPREFERREDLIFETIME 604800

#define MAXROUTERLIFETIME 9000
#define MIN_MAXINTERVAL 4
#define MAX_MAXINTERVAL 1800
#define MIN_MININTERVAL 3
#define MAXREACHABLETIME 3600000

#define MAX_INITIAL_RTR_ADVERT_INTERVAL  16
#define MAX_INITIAL_RTR_ADVERTISEMENTS    3
#define MAX_FINAL_RTR_ADVERTISEMENTS      3
#define MIN_DELAY_BETWEEN_RAS             3
#define MAX_RA_DELAY_TIME                 500000 /* usec */

#define PREFIX_FROM_KERNEL 1
#define PREFIX_FROM_CONFIG 2
#define PREFIX_FROM_DYNAMIC 3

struct prefix {
	TAILQ_ENTRY(prefix)	pfx_next;

	struct rainfo *pfx_rainfo;	/* back pointer to the interface */
	/*
	 * Expiration timer.  This is used when a prefix derived from
	 * the kernel is deleted.
	 */
	struct rtadvd_timer *pfx_timer;

	u_int32_t	pfx_validlifetime;	/* AdvValidLifetime */
	long		pfx_vltimeexpire;	/* Expiration of vltime */
	u_int32_t	pfx_preflifetime;	/* AdvPreferredLifetime */
	long		pfx_pltimeexpire;	/* Expiration of pltime */
	u_int		pfx_onlinkflg;		/* bool: AdvOnLinkFlag */
	u_int		pfx_autoconfflg;	/* bool: AdvAutonomousFlag */
	int		pfx_prefixlen;
	int		pfx_origin;		/* From kernel or config */

	struct in6_addr	pfx_prefix;
};

#ifdef ROUTEINFO
struct rtinfo {
	TAILQ_ENTRY(rtinfo)	rti_next;

	u_int32_t	rti_ltime;	/* route lifetime */
	u_int		rti_rtpref;	/* route preference */
	int		rti_prefixlen;
	struct in6_addr	rti_prefix;
};
#endif

struct rdnss_addr {
	TAILQ_ENTRY(rdnss_addr)	ra_next;

	struct in6_addr ra_dns;	/* DNS server entry */
};

struct rdnss {
	TAILQ_ENTRY(rdnss) rd_next;

	TAILQ_HEAD(, rdnss_addr) rd_list;	/* list of DNS servers */
	int rd_cnt;		/* number of DNS servers */
	u_int32_t rd_ltime;	/* number of seconds valid */
};

/*
 * The maximum length of a domain name in a DNS search list is calculated
 * by a domain name + length fields per 63 octets + a zero octet at
 * the tail and adding 8 octet boundary padding.
 */
#define _DNAME_LABELENC_MAXLEN \
	(NI_MAXHOST + (NI_MAXHOST / 64 + 1) + 1)

#define DNAME_LABELENC_MAXLEN \
	(_DNAME_LABELENC_MAXLEN + 8 - _DNAME_LABELENC_MAXLEN % 8)

struct dnssl_addr {
	TAILQ_ENTRY(dnssl_addr)	da_next;

	int da_len;				/* length of entry */
	char da_dom[DNAME_LABELENC_MAXLEN];	/* search domain name entry */
};

struct dnssl {
	TAILQ_ENTRY(dnssl)	dn_next;

	TAILQ_HEAD(, dnssl_addr) dn_list;	/* list of search domains */
	u_int32_t dn_ltime;			/* number of seconds valid */
};

struct soliciter {
	TAILQ_ENTRY(soliciter)	sol_next;

	struct sockaddr_in6	sol_addr;
};

struct	rainfo {
	/* pointer for list */
	TAILQ_ENTRY(rainfo)	rai_next;

	/* timer related parameters */
	struct rtadvd_timer	*rai_timer;
	/* counter for the first few advertisements */
	int			rai_initcounter;
	/* timestamp when the latest RA was sent */
	struct timeval		rai_lastsent;
	/* number of RS waiting for RA */
	int			rai_waiting;

	/* interface information */
	int	rai_ifindex;
	int	rai_advlinkopt;	/* bool: whether include link-layer addr opt */
	int	rai_advifprefix;	/* bool: gather IF prefixes? */
	struct sockaddr_dl *rai_sdl;
	char	rai_ifname[IFNAMSIZ];
	u_int32_t	rai_phymtu;	/* mtu of the physical interface */

	/* Router configuration variables */
	u_short	rai_lifetime;		/* AdvDefaultLifetime */
	u_int	rai_maxinterval;	/* MaxRtrAdvInterval */
	u_int	rai_mininterval;	/* MinRtrAdvInterval */
	int 	rai_managedflg;		/* AdvManagedFlag */
	int	rai_otherflg;		/* AdvOtherConfigFlag */

	int	rai_rtpref;		/* router preference */
	u_int32_t rai_linkmtu;		/* AdvLinkMTU */
	u_int32_t rai_reachabletime;	/* AdvReachableTime */
	u_int32_t rai_retranstimer;	/* AdvRetransTimer */
	u_int	rai_hoplimit;		/* AdvCurHopLimit */

	TAILQ_HEAD(, prefix) rai_prefix;/* AdvPrefixList(link head) */
	int	rai_pfxs;		/* number of prefixes */

	long	rai_clockskew;	/* used for consisitency check of lifetimes */

	TAILQ_HEAD(, rdnss) rai_rdnss;	/* DNS server list */
	TAILQ_HEAD(, dnssl) rai_dnssl;	/* search domain list */
#ifdef ROUTEINFO
	TAILQ_HEAD(, rtinfo) rai_route;	/* route information option (link head) */
	int	rai_routes;		/* number of route information options */
#endif
	/* actual RA packet data and its length */
	size_t	rai_ra_datalen;
	u_char	*rai_ra_data;

	/* statistics */
	u_quad_t rai_raoutput;		/* # of RAs sent */
	u_quad_t rai_rainput;		/* # of RAs received */
	u_quad_t rai_rainconsistent;	/* # of RAs inconsistent with ours */
	u_quad_t rai_rsinput;		/* # of RSs received */

	/* info about soliciter */
	TAILQ_HEAD(, soliciter) rai_soliciter;	/* recent solication source */
};

/* Interface list including RA information */
extern TAILQ_HEAD(railist_head_t, rainfo) railist;

struct rtadvd_timer	*ra_timeout(void *);
void			ra_timer_update(void *, struct timeval *);

int			prefix_match(struct in6_addr *, int,
			    struct in6_addr *, int);
struct rainfo		*if_indextorainfo(int);
struct prefix		*find_prefix(struct rainfo *,
			    struct in6_addr *, int);
