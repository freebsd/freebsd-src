/*	$KAME: config.c,v 1.37 2001/05/25 07:34:00 itojun Exp $	*/

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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/sysctl.h>

#include <net/if.h>
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
#include <net/if_var.h>
#endif /* __FreeBSD__ >= 3 */
#include <net/route.h>
#include <net/if_dl.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet/icmp6.h>
#ifdef MIP6
#include <netinet6/mip6.h>
#endif

#include <arpa/inet.h>

#include <stdio.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <search.h>
#endif
#include <unistd.h>
#include <ifaddrs.h>

#include "rtadvd.h"
#include "advcap.h"
#include "timer.h"
#include "if.h"
#include "config.h"

static void makeentry __P((char *, int, char *, int));
static void get_prefix __P((struct rainfo *));
static int getinet6sysctl __P((int));

extern struct rainfo *ralist;

void
getconfig(intface)
	char *intface;
{
	int stat, pfxs, i;
	char tbuf[BUFSIZ];
	struct rainfo *tmp;
	long val;
	long long val64;
	char buf[BUFSIZ];
	char *bp = buf;
	char *addr;
	static int forwarding = -1;

#define MUSTHAVE(var, cap)	\
    do {								\
	int t;								\
	if ((t = agetnum(cap)) < 0) {					\
		fprintf(stderr, "rtadvd: need %s for interface %s\n",	\
			cap, intface);					\
		exit(1);						\
	}								\
	var = t;							\
     } while (0)
#define MAYHAVE(var, cap, def)	\
     do {								\
	if ((var = agetnum(cap)) < 0)					\
		var = def;						\
     } while (0)

	if ((stat = agetent(tbuf, intface)) <= 0) {
		memset(tbuf, 0, sizeof(tbuf));
		syslog(LOG_INFO,
		       "<%s> %s isn't defined in the configuration file"
		       " or the configuration file doesn't exist."
		       " Treat it as default",
		        __FUNCTION__, intface);
	}

	tmp = (struct rainfo *)malloc(sizeof(*ralist));
	memset(tmp, 0, sizeof(*tmp));
	tmp->prefix.next = tmp->prefix.prev = &tmp->prefix;
	tmp->route.next = tmp->route.prev = &tmp->route;

	/* check if we are allowed to forward packets (if not determined) */
	if (forwarding < 0) {
		if ((forwarding = getinet6sysctl(IPV6CTL_FORWARDING)) < 0)
			exit(1);
	}

	/* get interface information */
	if (agetflag("nolladdr"))
		tmp->advlinkopt = 0;
	else
		tmp->advlinkopt = 1;
	if (tmp->advlinkopt) {
		if ((tmp->sdl = if_nametosdl(intface)) == NULL) {
			syslog(LOG_ERR,
			       "<%s> can't get information of %s",
			       __FUNCTION__, intface);
			exit(1);
		}
		tmp->ifindex = tmp->sdl->sdl_index;
	} else
		tmp->ifindex = if_nametoindex(intface);
	strncpy(tmp->ifname, intface, sizeof(tmp->ifname));
	if ((tmp->phymtu = if_getmtu(intface)) == 0) {
		tmp->phymtu = IPV6_MMTU;
		syslog(LOG_WARNING,
		       "<%s> can't get interface mtu of %s. Treat as %d",
		       __FUNCTION__, intface, IPV6_MMTU);
	}

	/*
	 * set router configuration variables.
	 */
	MAYHAVE(val, "maxinterval", DEF_MAXRTRADVINTERVAL);
	if (val < MIN_MAXINTERVAL || val > MAX_MAXINTERVAL) {
		syslog(LOG_ERR,
		       "<%s> maxinterval must be between %e and %u",
		       __FUNCTION__, MIN_MAXINTERVAL, MAX_MAXINTERVAL);
		exit(1);
	}
	tmp->maxinterval = (u_int)val;
	MAYHAVE(val, "mininterval", tmp->maxinterval/3);
	if (val < MIN_MININTERVAL || val > (tmp->maxinterval * 3) / 4) {
		syslog(LOG_ERR,
		       "<%s> mininterval must be between %e and %d",
		       __FUNCTION__,
		       MIN_MININTERVAL,
		       (tmp->maxinterval * 3) / 4);
		exit(1);
	}
	tmp->mininterval = (u_int)val;

	MAYHAVE(val, "chlim", DEF_ADVCURHOPLIMIT);
	tmp->hoplimit = val & 0xff;

	MAYHAVE(val, "raflags", 0);
	tmp->managedflg = val & ND_RA_FLAG_MANAGED;
	tmp->otherflg = val & ND_RA_FLAG_OTHER;
#ifdef MIP6
	if (mobileip6)
		tmp->haflg = val & ND_RA_FLAG_HA;
#endif
#ifndef ND_RA_FLAG_RTPREF_MASK
#define ND_RA_FLAG_RTPREF_MASK	0x18 /* 00011000 */
#define ND_RA_FLAG_RTPREF_RSV	0x10 /* 00010000 */
#endif
	tmp->rtpref = val & ND_RA_FLAG_RTPREF_MASK;
	if (tmp->rtpref == ND_RA_FLAG_RTPREF_RSV) {
		syslog(LOG_ERR, "<%s> invalid router preference on %s",
		       __FUNCTION__, intface);
		exit(1);
	}

	MAYHAVE(val, "rltime", tmp->maxinterval * 3);
	if (val && (val < tmp->maxinterval || val > MAXROUTERLIFETIME)) {
		syslog(LOG_ERR,
		       "<%s> router lifetime on %s must be 0 or"
		       " between %d and %d",
		       __FUNCTION__, intface,
		       tmp->maxinterval, MAXROUTERLIFETIME);
		exit(1);
	}
	/*
	 * Basically, hosts MUST NOT send Router Advertisement messages at any
	 * time (RFC 2461, Section 6.2.3). However, it would sometimes be
	 * useful to allow hosts to advertise some parameters such as prefix
	 * information and link MTU. Thus, we allow hosts to invoke rtadvd
	 * only when router lifetime (on every advertising interface) is
	 * explicitly set zero. (see also the above section)
	 */
	if (val && forwarding == 0) {
		syslog(LOG_WARNING,
		       "<%s> non zero router lifetime is specified for %s, "
		       "which must not be allowed for hosts.",
		       __FUNCTION__, intface);
		exit(1);
	}
	tmp->lifetime = val & 0xffff;

	MAYHAVE(val, "rtime", DEF_ADVREACHABLETIME);
	if (val > MAXREACHABLETIME) {
		syslog(LOG_ERR,
		       "<%s> reachable time must be no greater than %d",
		       __FUNCTION__, MAXREACHABLETIME);
		exit(1);
	}
	tmp->reachabletime = (u_int32_t)val;

	MAYHAVE(val64, "retrans", DEF_ADVRETRANSTIMER);
	if (val64 < 0 || val64 > 0xffffffff) {
		syslog(LOG_ERR,
		       "<%s> retrans time out of range", __FUNCTION__);
		exit(1);
	}
	tmp->retranstimer = (u_int32_t)val64;

#ifndef MIP6
	if (agetstr("hapref", &bp) || agetstr("hatime", &bp)) {
		syslog(LOG_ERR,
		       "<%s> mobile-ip6 configuration not supported",
		       __FUNCTION__);
		exit(1);
	}
#else
	if (!mobileip6) {
		if (agetstr("hapref", &bp) || agetstr("hatime", &bp)) {
			syslog(LOG_ERR,
			       "<%s> mobile-ip6 configuration without "
			       "proper command line option",
			       __FUNCTION__);
			exit(1);
		}
	} else {
		tmp->hapref = 0;
		if ((val = agetnum("hapref")) >= 0)
			tmp->hapref = (int16_t)val;
		if (tmp->hapref != 0) {
			tmp->hatime = 0;
			MUSTHAVE(val, "hatime");
			tmp->hatime = (u_int16_t)val;
			if (tmp->hatime <= 0) {
				syslog(LOG_ERR,
				       "<%s> home agent lifetime must be greater than 0",
				       __FUNCTION__);
				exit(1);
			}
		}
	}
#endif

	/* prefix information */

	/*
	 * This is an implementation specific parameter to consinder
	 * link propagation delays and poorly synchronized clocks when
	 * checking consistency of advertised lifetimes.
	 */
	MAYHAVE(val, "clockskew", 0);
	tmp->clockskew = val;

	if ((pfxs = agetnum("addrs")) < 0) {
		/* auto configure prefix information */
		if (agetstr("addr", &bp) || agetstr("addr1", &bp)) {
			syslog(LOG_ERR,
			       "<%s> conflicting prefix configuration for %s: "
			       "automatic and manual config at the same time",
			       __FUNCTION__, intface);
			exit(1);
		}
		get_prefix(tmp);
	}
	else {
		tmp->pfxs = pfxs;
		for (i = 0; i < pfxs; i++) {
			struct prefix *pfx;
			char entbuf[256];
			int added = (pfxs > 1) ? 1 : 0;

			/* allocate memory to store prefix information */
			if ((pfx = malloc(sizeof(struct prefix))) == NULL) {
				syslog(LOG_ERR,
				       "<%s> can't allocate enough memory",
				       __FUNCTION__);
				exit(1);
			}
			memset(pfx, 0, sizeof(*pfx));

			/* link into chain */
			insque(pfx, &tmp->prefix);

			pfx->origin = PREFIX_FROM_CONFIG;

			makeentry(entbuf, i, "prefixlen", added);
			MAYHAVE(val, entbuf, 64);
			if (val < 0 || val > 128) {
				syslog(LOG_ERR,
				       "<%s> prefixlen out of range",
				       __FUNCTION__);
				exit(1);
			}
			pfx->prefixlen = (int)val;

			makeentry(entbuf, i, "pinfoflags", added);
#ifdef MIP6
			if (mobileip6)
			{
				MAYHAVE(val, entbuf,
				    (ND_OPT_PI_FLAG_ONLINK|ND_OPT_PI_FLAG_AUTO|
					 ND_OPT_PI_FLAG_ROUTER));
			} else
#endif
			{
				MAYHAVE(val, entbuf,
				    (ND_OPT_PI_FLAG_ONLINK|ND_OPT_PI_FLAG_AUTO));
			}
			pfx->onlinkflg = val & ND_OPT_PI_FLAG_ONLINK;
			pfx->autoconfflg = val & ND_OPT_PI_FLAG_AUTO;
#ifdef MIP6
			pfx->routeraddr = val & ND_OPT_PI_FLAG_ROUTER;
#endif

			makeentry(entbuf, i, "vltime", added);
			MAYHAVE(val64, entbuf, DEF_ADVVALIDLIFETIME);
			if (val64 < 0 || val64 > 0xffffffff) {
				syslog(LOG_ERR,
				       "<%s> vltime out of range",
				       __FUNCTION__);
				exit(1);
			}
			pfx->validlifetime = (u_int32_t)val64;

			makeentry(entbuf, i, "vltimedecr", added);
			if (agetflag(entbuf)) {
				struct timeval now;
				gettimeofday(&now, 0);
				pfx->vltimeexpire =
					now.tv_sec + pfx->validlifetime;
			}

			makeentry(entbuf, i, "pltime", added);
			MAYHAVE(val64, entbuf, DEF_ADVPREFERREDLIFETIME);
			if (val64 < 0 || val64 > 0xffffffff) {
				syslog(LOG_ERR,
				       "<%s> pltime out of range",
				       __FUNCTION__);
				exit(1);
			}
			pfx->preflifetime = (u_int32_t)val64;

			makeentry(entbuf, i, "pltimedecr", added);
			if (agetflag(entbuf)) {
				struct timeval now;
				gettimeofday(&now, 0);
				pfx->pltimeexpire =
					now.tv_sec + pfx->preflifetime;
			}

			makeentry(entbuf, i, "addr", added);
			addr = (char *)agetstr(entbuf, &bp);
			if (addr == NULL) {
				syslog(LOG_ERR,
				       "<%s> need %s as an prefix for "
				       "interface %s",
				       __FUNCTION__, entbuf, intface);
				exit(1);
			}
			if (inet_pton(AF_INET6, addr,
				      &pfx->prefix) != 1) {
				syslog(LOG_ERR,
				       "<%s> inet_pton failed for %s",
				       __FUNCTION__, addr);
				exit(1);
			}
			if (IN6_IS_ADDR_MULTICAST(&pfx->prefix)) {
				syslog(LOG_ERR,
				       "<%s> multicast prefix(%s) must "
				       "not be advertised (IF=%s)",
				       __FUNCTION__, addr, intface);
				exit(1);
			}
			if (IN6_IS_ADDR_LINKLOCAL(&pfx->prefix))
				syslog(LOG_NOTICE,
				       "<%s> link-local prefix(%s) will be"
				       " advertised on %s",
				       __FUNCTION__, addr, intface);
		}
	}

	MAYHAVE(val, "mtu", 0);
	if (val < 0 || val > 0xffffffff) {
		syslog(LOG_ERR,
		       "<%s> mtu out of range", __FUNCTION__);
		exit(1);
	}
	tmp->linkmtu = (u_int32_t)val;
	if (tmp->linkmtu == 0) {
		char *mtustr;

		if ((mtustr = (char *)agetstr("mtu", &bp)) &&
		    strcmp(mtustr, "auto") == 0)
			tmp->linkmtu = tmp->phymtu;
	}
	else if (tmp->linkmtu < IPV6_MMTU || tmp->linkmtu > tmp->phymtu) {
		syslog(LOG_ERR,
		       "<%s> advertised link mtu must be between"
		       " least MTU and physical link MTU",
		       __FUNCTION__);
		exit(1);
	}

	/* route information */

	MAYHAVE(val, "routes", 0);
	if (val < 0 || val > 0xffffffff) {
		syslog(LOG_ERR,
		       "<%s> number of route information improper", __FUNCTION__);
		exit(1);
	}
	tmp->routes = val;
	for (i = 0; i < tmp->routes; i++) {
		struct rtinfo *rti;
		char entbuf[256];
		int added = (tmp->routes > 1) ? 1 : 0;

		/* allocate memory to store prefix information */
		if ((rti = malloc(sizeof(struct rtinfo))) == NULL) {
			syslog(LOG_ERR,
			       "<%s> can't allocate enough memory",
			       __FUNCTION__);
			exit(1);
		}
		memset(rti, 0, sizeof(*rti));

		/* link into chain */
		insque(rti, &tmp->route);

		makeentry(entbuf, i, "rtrplen", added);
		MAYHAVE(val, entbuf, 64);
		if (val < 0 || val > 128) {
			syslog(LOG_ERR,
			       "<%s> prefixlen out of range",
			       __FUNCTION__);
			exit(1);
		}
		rti->prefixlen = (int)val;

		makeentry(entbuf, i, "rtrflags", added);
		MAYHAVE(val, entbuf, 0);
		rti->rtpref = val & ND_RA_FLAG_RTPREF_MASK;
		if (rti->rtpref == ND_RA_FLAG_RTPREF_RSV) {
			syslog(LOG_ERR, "<%s> invalid router preference",
			       __FUNCTION__);
			exit(1);
		}

		makeentry(entbuf, i, "rtrltime", added);
		/*
		 * XXX: since default value of route lifetime is not defined in
		 * draft-draves-route-selection-01.txt, I took the default 
		 * value of valid lifetime of prefix as its default.
		 * It need be much considered.
		 */
		MAYHAVE(val64, entbuf, DEF_ADVVALIDLIFETIME);
		if (val64 < 0 || val64 > 0xffffffff) {
			syslog(LOG_ERR,
			       "<%s> rtrltime out of range",
			       __FUNCTION__);
			exit(1);
		}
		rti->ltime = (u_int32_t)val64;

		makeentry(entbuf, i, "rtrprefix", added);
		addr = (char *)agetstr(entbuf, &bp);
		if (addr == NULL) {
			syslog(LOG_ERR,
			       "<%s> need %s as an route for "
			       "interface %s",
			       __FUNCTION__, entbuf, intface);
			exit(1);
		}
		if (inet_pton(AF_INET6, addr, &rti->prefix) != 1) {
			syslog(LOG_ERR,
			       "<%s> inet_pton failed for %s",
			       __FUNCTION__, addr);
			exit(1);
		}
#if 0
		/*
		 * XXX: currently there's no restriction in route information
		 * prefix according to draft-draves-route-selection-01.txt,
		 * however I think the similar restriction be necessary.
		 */
		MAYHAVE(val64, entbuf, DEF_ADVVALIDLIFETIME);
		if (IN6_IS_ADDR_MULTICAST(&rti->prefix)) {
			syslog(LOG_ERR,
			       "<%s> multicast route (%s) must "
			       "not be advertised (IF=%s)",
			       __FUNCTION__, addr, intface);
			exit(1);
		}
		if (IN6_IS_ADDR_LINKLOCAL(&rti->prefix)) {
			syslog(LOG_NOTICE,
			       "<%s> link-local route (%s) must "
			       "not be advertised on %s",
			       __FUNCTION__, addr, intface);
			exit(1);
		}
#endif
	}

	/* okey */
	tmp->next = ralist;
	ralist = tmp;

	/* construct the sending packet */
	make_packet(tmp);

	/* set timer */
	tmp->timer = rtadvd_add_timer(ra_timeout, ra_timer_update,
				      tmp, tmp);
	ra_timer_update((void *)tmp, &tmp->timer->tm);
	rtadvd_set_timer(&tmp->timer->tm, tmp->timer);
}

static void
get_prefix(struct rainfo *rai)
{
	struct ifaddrs *ifap, *ifa;
	struct prefix *pp;
	struct in6_addr *a;
	u_char *p, *ep, *m, *lim;
	u_char ntopbuf[INET6_ADDRSTRLEN];

	if (getifaddrs(&ifap) < 0) {
		syslog(LOG_ERR,
		       "<%s> can't get interface addresses",
		       __FUNCTION__);
		exit(1);
	}
	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (strcmp(ifa->ifa_name, rai->ifname) != 0)
			continue;
		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;
		a = &((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr;
		if (IN6_IS_ADDR_LINKLOCAL(a))
			continue;

		/* allocate memory to store prefix info. */
		if ((pp = malloc(sizeof(*pp))) == NULL) {
			syslog(LOG_ERR,
			       "<%s> can't get allocate buffer for prefix",
			       __FUNCTION__);
			exit(1);
		}
		memset(pp, 0, sizeof(*pp));

		/* set prefix length */
		m = (u_char *)&((struct sockaddr_in6 *)ifa->ifa_netmask)->sin6_addr;
		lim = (u_char *)(ifa->ifa_netmask) + ifa->ifa_netmask->sa_len;
		pp->prefixlen = prefixlen(m, lim);
		if (pp->prefixlen < 0 || pp->prefixlen > 128) {
			syslog(LOG_ERR,
			       "<%s> failed to get prefixlen "
			       "or prefix is invalid",
			       __FUNCTION__);
			exit(1);
		}

		/* set prefix, sweep bits outside of prefixlen */
		memcpy(&pp->prefix, a, sizeof(*a));
		p = (u_char *)&pp->prefix;
		ep = (u_char *)(&pp->prefix + 1);
		while (m < lim)
			*p++ &= *m++;
		while (p < ep)
			*p++ = 0x00;

	        if (!inet_ntop(AF_INET6, &pp->prefix, ntopbuf,
	            sizeof(ntopbuf))) {
			syslog(LOG_ERR, "<%s> inet_ntop failed", __FUNCTION__);
			exit(1);
		}
		syslog(LOG_DEBUG,
		       "<%s> add %s/%d to prefix list on %s",
		       __FUNCTION__, ntopbuf, pp->prefixlen, rai->ifname);

		/* set other fields with protocol defaults */
		pp->validlifetime = DEF_ADVVALIDLIFETIME;
		pp->preflifetime = DEF_ADVPREFERREDLIFETIME;
		pp->onlinkflg = 1;
		pp->autoconfflg = 1;
		pp->origin = PREFIX_FROM_KERNEL;

		/* link into chain */
		insque(pp, &rai->prefix);

		/* counter increment */
		rai->pfxs++;
	}

	freeifaddrs(ifap);
}

static void
makeentry(buf, id, string, add)
    char *buf, *string;
    int id, add;
{
	strcpy(buf, string);
	if (add) {
		char *cp;

		cp = (char *)index(buf, '\0');
		cp += sprintf(cp, "%d", id);
		*cp = '\0';
	}
}

/*
 * Add a prefix to the list of specified interface and reconstruct
 * the outgoing packet.
 * The prefix must not be in the list.
 * XXX: other parameter of the prefix(e.g. lifetime) shoule be
 * able to be specified.
 */
static void
add_prefix(struct rainfo *rai, struct in6_prefixreq *ipr)
{
	struct prefix *prefix;
	u_char ntopbuf[INET6_ADDRSTRLEN];

	if ((prefix = malloc(sizeof(*prefix))) == NULL) {
		syslog(LOG_ERR, "<%s> memory allocation failed",
		       __FUNCTION__);
		return;		/* XXX: error or exit? */
	}
	memset(prefix, 0, sizeof(*prefix));
	prefix->prefix = ipr->ipr_prefix.sin6_addr;
	prefix->prefixlen = ipr->ipr_plen;
	prefix->validlifetime = ipr->ipr_vltime;
	prefix->preflifetime = ipr->ipr_pltime;
	prefix->onlinkflg = ipr->ipr_raf_onlink;
	prefix->autoconfflg = ipr->ipr_raf_auto;
	prefix->origin = PREFIX_FROM_DYNAMIC;

	insque(prefix, &rai->prefix);

	syslog(LOG_DEBUG, "<%s> new prefix %s/%d was added on %s",
	       __FUNCTION__, inet_ntop(AF_INET6, &ipr->ipr_prefix.sin6_addr,
				       ntopbuf, INET6_ADDRSTRLEN),
	       ipr->ipr_plen, rai->ifname);

	/* free the previous packet */
	free(rai->ra_data);
	rai->ra_data = NULL;

	/* reconstruct the packet */
	rai->pfxs++;
	make_packet(rai);

	/*
	 * reset the timer so that the new prefix will be advertised quickly.
	 */
	rai->initcounter = 0;
	ra_timer_update((void *)rai, &rai->timer->tm);
	rtadvd_set_timer(&rai->timer->tm, rai->timer);
}

/*
 * Delete a prefix to the list of specified interface and reconstruct
 * the outgoing packet.
 * The prefix must be in the list.
 */
void
delete_prefix(struct rainfo *rai, struct prefix *prefix)
{
	u_char ntopbuf[INET6_ADDRSTRLEN];

	remque(prefix);
	syslog(LOG_DEBUG, "<%s> prefix %s/%d was deleted on %s",
	       __FUNCTION__, inet_ntop(AF_INET6, &prefix->prefix,
				       ntopbuf, INET6_ADDRSTRLEN),
	       prefix->prefixlen, rai->ifname);
	free(prefix);
	rai->pfxs--;
	make_packet(rai);
}

/*
 * Try to get an in6_prefixreq contents for a prefix which matches
 * ipr->ipr_prefix and ipr->ipr_plen and belongs to
 * the interface whose name is ipr->ipr_name[].
 */
static int
init_prefix(struct in6_prefixreq *ipr)
{
	int s;

	if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
		syslog(LOG_ERR, "<%s> socket: %s", __FUNCTION__,
		       strerror(errno));
		exit(1);
	}

	if (ioctl(s, SIOCGIFPREFIX_IN6, (caddr_t)ipr) < 0) {
		syslog(LOG_INFO, "<%s> ioctl:SIOCGIFPREFIX %s", __FUNCTION__,
		       strerror(errno));

		ipr->ipr_vltime = DEF_ADVVALIDLIFETIME;
		ipr->ipr_pltime = DEF_ADVPREFERREDLIFETIME;
		ipr->ipr_raf_onlink = 1;
		ipr->ipr_raf_auto = 1;
		/* omit other field initialization */
	}
	else if (ipr->ipr_origin < PR_ORIG_RR) {
		u_char ntopbuf[INET6_ADDRSTRLEN];

		syslog(LOG_WARNING, "<%s> Added prefix(%s)'s origin %d is"
		       "lower than PR_ORIG_RR(router renumbering)."
		       "This should not happen if I am router", __FUNCTION__,
		       inet_ntop(AF_INET6, &ipr->ipr_prefix.sin6_addr, ntopbuf,
				 sizeof(ntopbuf)), ipr->ipr_origin);
		close(s);
		return 1;
	}

	close(s);
	return 0;
}

void
make_prefix(struct rainfo *rai, int ifindex, struct in6_addr *addr, int plen)
{
	struct in6_prefixreq ipr;

	memset(&ipr, 0, sizeof(ipr));
	if (if_indextoname(ifindex, ipr.ipr_name) == NULL) {
		syslog(LOG_ERR, "<%s> Prefix added interface No.%d doesn't"
		       "exist. This should not happen! %s", __FUNCTION__,
		       ifindex, strerror(errno));
		exit(1);
	}
	ipr.ipr_prefix.sin6_len = sizeof(ipr.ipr_prefix);
	ipr.ipr_prefix.sin6_family = AF_INET6;
	ipr.ipr_prefix.sin6_addr = *addr;
	ipr.ipr_plen = plen;

	if (init_prefix(&ipr))
		return; /* init failed by some error */
	add_prefix(rai, &ipr);
}

void
make_packet(struct rainfo *rainfo)
{
	size_t packlen, lladdroptlen = 0;
	char *buf;
	struct nd_router_advert *ra;
	struct nd_opt_prefix_info *ndopt_pi;
	struct nd_opt_mtu *ndopt_mtu;
#ifdef MIP6
	struct nd_opt_advinterval *ndopt_advint;
	struct nd_opt_homeagent_info *ndopt_hai;
#endif
	struct nd_opt_route_info *ndopt_rti;
	struct prefix *pfx;
	struct rtinfo *rti;

	/* calculate total length */
	packlen = sizeof(struct nd_router_advert);
	if (rainfo->advlinkopt) {
		if ((lladdroptlen = lladdropt_length(rainfo->sdl)) == 0) {
			syslog(LOG_INFO,
			       "<%s> link-layer address option has"
			       " null length on %s."
			       " Treat as not included.",
			       __FUNCTION__, rainfo->ifname);
			rainfo->advlinkopt = 0;
		}
		packlen += lladdroptlen;
	}
	if (rainfo->pfxs)
		packlen += sizeof(struct nd_opt_prefix_info) * rainfo->pfxs;
	if (rainfo->linkmtu)
		packlen += sizeof(struct nd_opt_mtu);
#ifdef MIP6
	if (mobileip6 && rainfo->maxinterval)
		packlen += sizeof(struct nd_opt_advinterval);
	if (mobileip6 && rainfo->hatime)
		packlen += sizeof(struct nd_opt_homeagent_info);
#endif
#ifdef ND_OPT_ROUTE_INFO
	for (rti = rainfo->route.next; rti != &rainfo->route; rti = rti->next)
		packlen += sizeof(struct nd_opt_route_info) + 
			   ((rti->prefixlen + 0x3f) >> 6) * 8;
#endif

	/* allocate memory for the packet */
	if ((buf = malloc(packlen)) == NULL) {
		syslog(LOG_ERR,
		       "<%s> can't get enough memory for an RA packet",
		       __FUNCTION__);
		exit(1);
	}
	if (rainfo->ra_data) {
		/* free the previous packet */
		free(rainfo->ra_data);
		rainfo->ra_data = NULL;
	}
	rainfo->ra_data = buf;
	/* XXX: what if packlen > 576? */
	rainfo->ra_datalen = packlen;

	/*
	 * construct the packet
	 */
	ra = (struct nd_router_advert *)buf;
	ra->nd_ra_type = ND_ROUTER_ADVERT;
	ra->nd_ra_code = 0;
	ra->nd_ra_cksum = 0;
	ra->nd_ra_curhoplimit = (u_int8_t)(0xff & rainfo->hoplimit);
	ra->nd_ra_flags_reserved = 0; /* just in case */
	/*
	 * XXX: the router preference field, which is a 2-bit field, should be
	 * initialized before other fields.
	 */
	ra->nd_ra_flags_reserved = 0xff & rainfo->rtpref;
	ra->nd_ra_flags_reserved |=
		rainfo->managedflg ? ND_RA_FLAG_MANAGED : 0;
	ra->nd_ra_flags_reserved |=
		rainfo->otherflg ? ND_RA_FLAG_OTHER : 0;
#ifdef MIP6
	ra->nd_ra_flags_reserved |=
		rainfo->haflg ? ND_RA_FLAG_HA : 0;
#endif
	ra->nd_ra_router_lifetime = htons(rainfo->lifetime);
	ra->nd_ra_reachable = htonl(rainfo->reachabletime);
	ra->nd_ra_retransmit = htonl(rainfo->retranstimer);
	buf += sizeof(*ra);

	if (rainfo->advlinkopt) {
		lladdropt_fill(rainfo->sdl, (struct nd_opt_hdr *)buf);
		buf += lladdroptlen;
	}

	if (rainfo->linkmtu) {
		ndopt_mtu = (struct nd_opt_mtu *)buf;
		ndopt_mtu->nd_opt_mtu_type = ND_OPT_MTU;
		ndopt_mtu->nd_opt_mtu_len = 1;
		ndopt_mtu->nd_opt_mtu_reserved = 0;
		ndopt_mtu->nd_opt_mtu_mtu = htonl(rainfo->linkmtu);
		buf += sizeof(struct nd_opt_mtu);
	}

#ifdef MIP6
	if (mobileip6 && rainfo->maxinterval) {
		ndopt_advint = (struct nd_opt_advinterval *)buf;
		ndopt_advint->nd_opt_adv_type = ND_OPT_ADVINTERVAL;
		ndopt_advint->nd_opt_adv_len = 1;
		ndopt_advint->nd_opt_adv_reserved = 0;
		ndopt_advint->nd_opt_adv_interval = htonl(rainfo->maxinterval *
							  1000);
		buf += sizeof(struct nd_opt_advinterval);
	}
#endif
	
#ifdef MIP6
	if (rainfo->hatime) {
		ndopt_hai = (struct nd_opt_homeagent_info *)buf;
		ndopt_hai->nd_opt_hai_type = ND_OPT_HOMEAGENT_INFO;
		ndopt_hai->nd_opt_hai_len = 1;
		ndopt_hai->nd_opt_hai_reserved = 0;
		ndopt_hai->nd_opt_hai_preference = htons(rainfo->hapref);
		ndopt_hai->nd_opt_hai_lifetime = htons(rainfo->hatime);
		buf += sizeof(struct nd_opt_homeagent_info);
	}
#endif
	
	for (pfx = rainfo->prefix.next;
	     pfx != &rainfo->prefix; pfx = pfx->next) {
		u_int32_t vltime, pltime;
		struct timeval now;

		ndopt_pi = (struct nd_opt_prefix_info *)buf;
		ndopt_pi->nd_opt_pi_type = ND_OPT_PREFIX_INFORMATION;
		ndopt_pi->nd_opt_pi_len = 4;
		ndopt_pi->nd_opt_pi_prefix_len = pfx->prefixlen;
		ndopt_pi->nd_opt_pi_flags_reserved = 0;
		if (pfx->onlinkflg)
			ndopt_pi->nd_opt_pi_flags_reserved |=
				ND_OPT_PI_FLAG_ONLINK;
		if (pfx->autoconfflg)
			ndopt_pi->nd_opt_pi_flags_reserved |=
				ND_OPT_PI_FLAG_AUTO;
#ifdef MIP6
		if (pfx->routeraddr)
			ndopt_pi->nd_opt_pi_flags_reserved |=
				ND_OPT_PI_FLAG_ROUTER;
#endif
		if (pfx->vltimeexpire || pfx->pltimeexpire)
			gettimeofday(&now, NULL);
		if (pfx->vltimeexpire == 0)
			vltime = pfx->validlifetime;
		else
			vltime = (pfx->vltimeexpire > now.tv_sec) ?
				pfx->vltimeexpire - now.tv_sec : 0;
		if (pfx->pltimeexpire == 0)
			pltime = pfx->preflifetime;
		else
			pltime = (pfx->pltimeexpire > now.tv_sec) ? 
				pfx->pltimeexpire - now.tv_sec : 0;
		if (vltime < pltime) {
			/*
			 * this can happen if vltime is decrement but pltime
			 * is not.
			 */
			pltime = vltime;
		}
		ndopt_pi->nd_opt_pi_valid_time = htonl(vltime);
		ndopt_pi->nd_opt_pi_preferred_time = htonl(pltime);
		ndopt_pi->nd_opt_pi_reserved2 = 0;
		ndopt_pi->nd_opt_pi_prefix = pfx->prefix;

		buf += sizeof(struct nd_opt_prefix_info);
	}

#ifdef ND_OPT_ROUTE_INFO
	for (rti = rainfo->route.next; rti != &rainfo->route; rti = rti->next) {
		u_int8_t psize = (rti->prefixlen + 0x3f) >> 6;

		ndopt_rti = (struct nd_opt_route_info *)buf;
		ndopt_rti->nd_opt_rti_type = ND_OPT_ROUTE_INFO;
		ndopt_rti->nd_opt_rti_len = 1 + psize;
		ndopt_rti->nd_opt_rti_prefixlen = rti->prefixlen;
		ndopt_rti->nd_opt_rti_flags = 0xff & rti->rtpref;
		ndopt_rti->nd_opt_rti_lifetime = rti->ltime;
		memcpy(ndopt_rti + 1, &rti->prefix, psize * 8);
		buf += sizeof(struct nd_opt_route_info) + psize * 8;
	}
#endif

	return;
}

static int
getinet6sysctl(int code)
{
	int mib[] = { CTL_NET, PF_INET6, IPPROTO_IPV6, 0 };
	int value;
	size_t size;

	mib[3] = code;
	size = sizeof(value);
	if (sysctl(mib, sizeof(mib)/sizeof(mib[0]), &value, &size, NULL, 0)
	    < 0) {
		syslog(LOG_ERR, "<%s>: failed to get ip6 sysctl(%d): %s",
		       __FUNCTION__, code,
		       strerror(errno));
		return(-1);
	}
	else
		return(value);
}
