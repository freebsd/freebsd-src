/*	$FreeBSD$	*/
/*	$KAME: rtadvd.h,v 1.16 2001/04/10 15:08:31 suz Exp $	*/

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

#define ALLNODES "ff02::1"
#define ALLROUTERS_LINK "ff02::2"
#define ALLROUTERS_SITE "ff05::2"
#define ANY "::"
#define RTSOLLEN 8

/* protocol constants and default values */
#define DEF_MAXRTRADVINTERVAL 600
#define DEF_ADVLINKMTU 0
#define DEF_ADVREACHABLETIME 0
#define DEF_ADVRETRANSTIMER 0
#define DEF_ADVCURHOPLIMIT 64
#define DEF_ADVVALIDLIFETIME 2592000
#define DEF_ADVPREFERREDLIFETIME 604800

/*XXX int-to-double comparison for INTERVAL items */
#ifndef MIP6
#define mobileip6 0
#endif

#define MAXROUTERLIFETIME 9000
#define MIN_MAXINTERVAL (mobileip6 ? 1.5 : 4.0)
#define MAX_MAXINTERVAL 1800
#define MIN_MININTERVAL	(mobileip6 ? 0.5 : 3)
#define MAXREACHABLETIME 3600000

#ifndef MIP6
#undef miobileip6
#endif

#define MAX_INITIAL_RTR_ADVERT_INTERVAL  16
#define MAX_INITIAL_RTR_ADVERTISEMENTS    3
#define MAX_FINAL_RTR_ADVERTISEMENTS      3
#define MIN_DELAY_BETWEEN_RAS             3
#define MAX_RA_DELAY_TIME                 500000 /* usec */

#define PREFIX_FROM_KERNEL 1
#define PREFIX_FROM_CONFIG 2
#define PREFIX_FROM_DYNAMIC 3

struct prefix {
	struct prefix *next;	/* forward link */
	struct prefix *prev;	/* previous link */

	u_int32_t validlifetime; /* AdvValidLifetime */
	long	vltimeexpire;	/* expiration of vltime; decrement case only */
	u_int32_t preflifetime;	/* AdvPreferredLifetime */
	long	pltimeexpire;	/* expiration of pltime; decrement case only */
	u_int onlinkflg;	/* bool: AdvOnLinkFlag */
	u_int autoconfflg;	/* bool: AdvAutonomousFlag */
#ifdef MIP6
	u_int routeraddr;	/* bool: RouterAddress */
#endif
	int prefixlen;
	int origin;		/* from kernel or cofig */
	struct in6_addr prefix;
};

struct rtinfo {
	struct rtinfo *prev;	/* previous link */
	struct rtinfo *next;	/* forward link */

	u_int32_t ltime;	/* route lifetime */
	u_int rtpref;		/* router preference */
	int prefixlen;
	struct in6_addr prefix;
};

struct soliciter {
	struct soliciter *next;
	struct sockaddr_in6 addr;
};

struct	rainfo {
	/* pointer for list */
	struct	rainfo *next;

	/* timer related parameters */
	struct rtadvd_timer *timer;
	int initcounter; /* counter for the first few advertisements */
	struct timeval lastsent; /* timestamp when the latest RA was sent */
	int waiting;		/* number of RS waiting for RA */

	/* interface information */
	int	ifindex;
	int	advlinkopt;	/* bool: whether include link-layer addr opt */
	struct sockaddr_dl *sdl;
	char	ifname[16];
	int	phymtu;		/* mtu of the physical interface */

	/* Router configuration variables */
	u_short lifetime;	/* AdvDefaultLifetime */
	u_int	maxinterval;	/* MaxRtrAdvInterval */
	u_int	mininterval;	/* MinRtrAdvInterval */
	int 	managedflg;	/* AdvManagedFlag */
	int	otherflg;	/* AdvOtherConfigFlag */
#ifdef MIP6
	int	haflg;		/* HAFlag */
#endif
	int	rtpref;		/* router preference */
	u_int32_t linkmtu;	/* AdvLinkMTU */
	u_int32_t reachabletime; /* AdvReachableTime */
	u_int32_t retranstimer;	/* AdvRetransTimer */
	u_int	hoplimit;	/* AdvCurHopLimit */
	struct prefix prefix;	/* AdvPrefixList(link head) */
	int	pfxs;		/* number of prefixes */
	long	clockskew;	/* used for consisitency check of lifetimes */

#ifdef MIP6
	u_short	hapref;		/* Home Agent Preference */
	u_short	hatime;		/* Home Agent Lifetime */
#endif
	struct rtinfo route;	/* route information option (link head) */
	int	routes;		/* number of route information options */

	/* actual RA packet data and its length */
	size_t ra_datalen;
	u_char *ra_data;

	/* statistics */
	u_quad_t raoutput;	/* number of RAs sent */
	u_quad_t rainput;	/* number of RAs received */
	u_quad_t rainconsistent; /* number of RAs inconsistent with ours */
	u_quad_t rsinput;	/* number of RSs received */

	/* info about soliciter */
	struct soliciter *soliciter;	/* recent solication source */
};

void ra_timeout __P((void *));
void ra_timer_update __P((void *, struct timeval *));

int prefix_match __P((struct in6_addr *, int, struct in6_addr *, int));
struct rainfo *if_indextorainfo __P((int));

extern struct in6_addr in6a_site_allrouters;
#ifdef MIP6
extern int mobileip6;
#endif
