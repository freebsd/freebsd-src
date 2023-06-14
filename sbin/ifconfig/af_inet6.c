/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <ifaddrs.h>

#include <arpa/inet.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <netinet6/nd6.h>	/* Define ND6_INFINITE_LIFETIME */

#include "ifconfig.h"
#include "ifconfig_netlink.h"

#ifndef WITHOUT_NETLINK
struct in6_px {
	struct in6_addr		addr;
	int			plen;
	bool			set;
};
struct in6_pdata {
	struct in6_px		addr;
	struct in6_px		dst_addr;
	struct in6_addrlifetime	lifetime;
	uint32_t		flags;
	uint32_t		vhid;
};

static struct in6_pdata in6_del;
static struct in6_pdata in6_add = {
	.lifetime = { 0, 0, ND6_INFINITE_LIFETIME, ND6_INFINITE_LIFETIME },
};
#else
static	struct in6_ifreq in6_ridreq;
static	struct in6_aliasreq in6_addreq =
  { .ifra_flags = 0,
    .ifra_lifetime = { 0, 0, ND6_INFINITE_LIFETIME, ND6_INFINITE_LIFETIME } };
#endif
static	int ip6lifetime;

#ifdef WITHOUT_NETLINK
static	int prefix(void *, int);
#endif
static	char *sec2str(time_t);
static	int explicit_prefix = 0;
extern	char *f_inet6, *f_addr;

extern void setnd6flags(if_ctx *, const char *, int);
extern void setnd6defif(if_ctx *,const char *, int);
extern void nd6_status(if_ctx *);

static	char addr_buf[NI_MAXHOST];	/*for getnameinfo()*/

static void
setifprefixlen(if_ctx *ctx __netlink_unused, const char *addr, int dummy __unused)
{
#ifdef WITHOUT_NETLINK
	const struct afswtch *afp = ctx->afp;

        if (afp->af_getprefix != NULL)
                afp->af_getprefix(addr, MASK);
#else
	int plen = strtol(addr, NULL, 10);

	if ((plen < 0) || (plen > 128))
		errx(1, "%s: bad value", addr);
	in6_add.addr.plen = plen;
#endif
	explicit_prefix = 1;
}

static void
setip6flags(if_ctx *ctx, const char *dummyaddr __unused, int flag)
{
	const struct afswtch *afp = ctx->afp;

	if (afp->af_af != AF_INET6)
		err(1, "address flags can be set only for inet6 addresses");

#ifdef WITHOUT_NETLINK
	if (flag < 0)
		in6_addreq.ifra_flags &= ~(-flag);
	else
		in6_addreq.ifra_flags |= flag;
#else
	if (flag < 0)
		in6_add.flags &= ~(-flag);
	else
		in6_add.flags |= flag;
#endif
}

static void
setip6lifetime(if_ctx *ctx, const char *cmd, const char *val)
{
	const struct afswtch *afp = ctx->afp;
	struct timespec now;
	time_t newval;
	char *ep;
#ifdef WITHOUT_NETLINK
	struct in6_addrlifetime *lifetime = &in6_addreq.ifra_lifetime;
#else
	struct in6_addrlifetime *lifetime = &in6_add.lifetime;
#endif

	clock_gettime(CLOCK_MONOTONIC_FAST, &now);
	newval = (time_t)strtoul(val, &ep, 0);
	if (val == ep)
		errx(1, "invalid %s", cmd);
	if (afp->af_af != AF_INET6)
		errx(1, "%s not allowed for the AF", cmd);
	if (strcmp(cmd, "vltime") == 0) {
		lifetime->ia6t_expire = now.tv_sec + newval;
		lifetime->ia6t_vltime = newval;
	} else if (strcmp(cmd, "pltime") == 0) {
		lifetime->ia6t_preferred = now.tv_sec + newval;
		lifetime->ia6t_pltime = newval;
	}
}

static void
setip6pltime(if_ctx *ctx, const char *seconds, int dummy __unused)
{
	setip6lifetime(ctx, "pltime", seconds);
}

static void
setip6vltime(if_ctx *ctx, const char *seconds, int dummy __unused)
{
	setip6lifetime(ctx, "vltime", seconds);
}

static void
setip6eui64(if_ctx *ctx, const char *cmd, int dummy __unused)
{
	const struct afswtch *afp = ctx->afp;
	struct ifaddrs *ifap, *ifa;
	const struct sockaddr_in6 *sin6 = NULL;
	const struct in6_addr *lladdr = NULL;
	struct in6_addr *in6;

	if (afp->af_af != AF_INET6)
		errx(EXIT_FAILURE, "%s not allowed for the AF", cmd);
#ifdef WITHOUT_NETLINK
 	in6 = (struct in6_addr *)&in6_addreq.ifra_addr.sin6_addr;
#else
	in6 = &in6_add.addr.addr;
#endif
	if (memcmp(&in6addr_any.s6_addr[8], &in6->s6_addr[8], 8) != 0)
		errx(EXIT_FAILURE, "interface index is already filled");
	if (getifaddrs(&ifap) != 0)
		err(EXIT_FAILURE, "getifaddrs");
	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr->sa_family == AF_INET6 &&
		    strcmp(ifa->ifa_name, ctx->ifname) == 0) {
			sin6 = (const struct sockaddr_in6 *)satosin6(ifa->ifa_addr);
			if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr)) {
				lladdr = &sin6->sin6_addr;
				break;
			}
		}
	}
	if (!lladdr)
		errx(EXIT_FAILURE, "could not determine link local address"); 

 	memcpy(&in6->s6_addr[8], &lladdr->s6_addr[8], 8);

	freeifaddrs(ifap);
}

static void
print_addr(struct sockaddr_in6 *sin)
{
	int error, n_flags;

	if (f_addr != NULL && strcmp(f_addr, "fqdn") == 0)
		n_flags = 0;
	else if (f_addr != NULL && strcmp(f_addr, "host") == 0)
		n_flags = NI_NOFQDN;
	else
		n_flags = NI_NUMERICHOST;
	error = getnameinfo((struct sockaddr *)sin, sin->sin6_len,
			    addr_buf, sizeof(addr_buf), NULL, 0,
			    n_flags);
	if (error != 0)
		inet_ntop(AF_INET6, &sin->sin6_addr, addr_buf,
			  sizeof(addr_buf));
	printf("\tinet6 %s", addr_buf);
}

static void
print_p2p(struct sockaddr_in6 *sin)
{
	int error;

	error = getnameinfo((struct sockaddr *)sin, sin->sin6_len, addr_buf,
	    sizeof(addr_buf), NULL, 0, NI_NUMERICHOST);

	if (error != 0)
		inet_ntop(AF_INET6, &sin->sin6_addr, addr_buf, sizeof(addr_buf));
	printf(" --> %s", addr_buf);
}

static void
print_mask(int plen)
{
	if (f_inet6 != NULL && strcmp(f_inet6, "cidr") == 0)
		printf("/%d", plen);
	else
		printf(" prefixlen %d", plen);
}

static void
print_flags(int flags6)
{
	if ((flags6 & IN6_IFF_ANYCAST) != 0)
		printf(" anycast");
	if ((flags6 & IN6_IFF_TENTATIVE) != 0)
		printf(" tentative");
	if ((flags6 & IN6_IFF_DUPLICATED) != 0)
		printf(" duplicated");
	if ((flags6 & IN6_IFF_DETACHED) != 0)
		printf(" detached");
	if ((flags6 & IN6_IFF_DEPRECATED) != 0)
		printf(" deprecated");
	if ((flags6 & IN6_IFF_AUTOCONF) != 0)
		printf(" autoconf");
	if ((flags6 & IN6_IFF_TEMPORARY) != 0)
		printf(" temporary");
	if ((flags6 & IN6_IFF_PREFER_SOURCE) != 0)
		printf(" prefer_source");

}

static void
print_lifetime(const char *prepend, time_t px_time, struct timespec *now)
{
	printf(" %s", prepend);
	if (px_time == 0)
		printf(" infty");

	printf(" %s", px_time < now->tv_sec ? "0" : sec2str(px_time - now->tv_sec));
}

#ifdef WITHOUT_NETLINK
static void
in6_status(if_ctx *ctx, const struct ifaddrs *ifa)
{
	struct sockaddr_in6 *sin, null_sin = {};
	struct in6_ifreq ifr6;
	int s6;
	u_int32_t flags6;
	struct in6_addrlifetime lifetime;

	sin = satosin6(ifa->ifa_addr);
	if (sin == NULL)
		return;

	strlcpy(ifr6.ifr_name, ctx->ifname, sizeof(ifr6.ifr_name));
	if ((s6 = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
		warn("socket(AF_INET6,SOCK_DGRAM)");
		return;
	}
	ifr6.ifr_addr = *sin;
	if (ioctl(s6, SIOCGIFAFLAG_IN6, &ifr6) < 0) {
		warn("ioctl(SIOCGIFAFLAG_IN6)");
		close(s6);
		return;
	}
	flags6 = ifr6.ifr_ifru.ifru_flags6;
	memset(&lifetime, 0, sizeof(lifetime));
	ifr6.ifr_addr = *sin;
	if (ioctl(s6, SIOCGIFALIFETIME_IN6, &ifr6) < 0) {
		warn("ioctl(SIOCGIFALIFETIME_IN6)");
		close(s6);
		return;
	}
	lifetime = ifr6.ifr_ifru.ifru_lifetime;
	close(s6);

	print_addr(sin);

	if (ifa->ifa_flags & IFF_POINTOPOINT) {
		sin = satosin6(ifa->ifa_dstaddr);
		/*
		 * some of the interfaces do not have valid destination
		 * address.
		 */
		if (sin != NULL && sin->sin6_family == AF_INET6)
			print_p2p(sin);
	}

	sin = satosin6(ifa->ifa_netmask);
	if (sin == NULL)
		sin = &null_sin;
	print_mask(prefix(&sin->sin6_addr, sizeof(struct in6_addr)));

	print_flags(flags6);

	if ((satosin6(ifa->ifa_addr))->sin6_scope_id)
		printf(" scopeid 0x%x",
		    (satosin6(ifa->ifa_addr))->sin6_scope_id);

	if (ip6lifetime && (lifetime.ia6t_preferred || lifetime.ia6t_expire)) {
		struct timespec now;

		clock_gettime(CLOCK_MONOTONIC_FAST, &now);
		print_lifetime("pltime", lifetime.ia6t_preferred, &now);
		print_lifetime("vltime", lifetime.ia6t_expire, &now);
	}

	print_vhid(ifa);

	putchar('\n');
}

#else
static void
show_lifetime(struct ifa_cacheinfo *ci)
{
	struct timespec now;
	uint32_t pl, vl;

	if (ci == NULL)
		return;

	int count = ci->ifa_prefered != ND6_INFINITE_LIFETIME;
	count += ci->ifa_valid != ND6_INFINITE_LIFETIME;
	if (count == 0)
		return;

	pl = (ci->ifa_prefered == ND6_INFINITE_LIFETIME) ? 0 : ci->ifa_prefered;
	vl = (ci->ifa_valid == ND6_INFINITE_LIFETIME) ? 0 : ci->ifa_valid;

	clock_gettime(CLOCK_MONOTONIC_FAST, &now);
	print_lifetime("pltime", pl + now.tv_sec, &now);
	print_lifetime("vltime", vl + now.tv_sec, &now);
}

static void
in6_status_nl(if_ctx *ctx __unused, if_link_t *link __unused, if_addr_t *ifa)
{
	int plen = ifa->ifa_prefixlen;
	uint32_t scopeid;

	if (ifa->ifa_local == NULL) {
		/* Non-P2P address */
		scopeid = satosin6(ifa->ifa_address)->sin6_scope_id;
		print_addr(satosin6(ifa->ifa_address));
	} else {
		scopeid = satosin6(ifa->ifa_local)->sin6_scope_id;
		print_addr(satosin6(ifa->ifa_local));
		print_p2p(satosin6(ifa->ifa_address));
	}

	print_mask(plen);
	print_flags(ifa->ifaf_flags);

	if (scopeid != 0)
		printf(" scopeid 0x%x", scopeid);

	show_lifetime(ifa->ifa_cacheinfo);

	if (ifa->ifaf_vhid != 0)
		printf(" vhid %d", ifa->ifaf_vhid);

	putchar('\n');
}

static struct in6_px *sin6tab_nl[] = {
        &in6_del.addr,          /* RIDADDR */
        &in6_add.addr,          /* ADDR */
        NULL,                   /* MASK */
        &in6_add.dst_addr,      /* DSTADDR*/
};

static void
in6_copyaddr(if_ctx *ctx __unused, int to, int from)
{
	sin6tab_nl[to]->addr = sin6tab_nl[from]->addr;
	sin6tab_nl[to]->set = sin6tab_nl[from]->set;
}

static void
in6_getaddr(const char *addr_str, int which)
{
        struct in6_px *px = sin6tab_nl[which];

        px->set = true;
        px->plen = 128;
        if (which == ADDR) {
                char *p = NULL;
                if((p = strrchr(addr_str, '/')) != NULL) {
                        *p = '\0';
                        int plen = strtol(p + 1, NULL, 10);
			if (plen < 0 || plen > 128)
                                errx(1, "%s: bad value", p + 1);
                        px->plen = plen;
                        explicit_prefix = 1;
                }
        }

        struct addrinfo hints = { .ai_family = AF_INET6 };
        struct addrinfo *res;

        int error = getaddrinfo(addr_str, NULL, &hints, &res);
        if (error != 0) {
                if (inet_pton(AF_INET6, addr_str, &px->addr) != 1)
                        errx(1, "%s: bad value", addr_str);
        } else {
                struct sockaddr_in6 *sin6;

                sin6 = (struct sockaddr_in6 *)(void *)res->ai_addr;
                px->addr = sin6->sin6_addr;
                freeaddrinfo(res);
        }
}

static int
in6_exec_nl(if_ctx *ctx, unsigned long action, void *data)
{
	struct in6_pdata *pdata = (struct in6_pdata *)data;
	struct snl_writer nw = {};

	snl_init_writer(ctx->io_ss, &nw);
	struct nlmsghdr *hdr = snl_create_msg_request(&nw, action);
	struct ifaddrmsg *ifahdr = snl_reserve_msg_object(&nw, struct ifaddrmsg);

	ifahdr->ifa_family = AF_INET6;
	ifahdr->ifa_prefixlen = pdata->addr.plen;
	ifahdr->ifa_index = if_nametoindex_nl(ctx->io_ss, ctx->ifname);

	snl_add_msg_attr_ip6(&nw, IFA_LOCAL, &pdata->addr.addr);
	if (action == NL_RTM_NEWADDR && pdata->dst_addr.set)
		snl_add_msg_attr_ip6(&nw, IFA_ADDRESS, &pdata->dst_addr.addr);

	struct ifa_cacheinfo ci = {
		.ifa_prefered = pdata->lifetime.ia6t_pltime,
		.ifa_valid =  pdata->lifetime.ia6t_vltime,
	};
	snl_add_msg_attr(&nw, IFA_CACHEINFO, sizeof(ci), &ci);

	int off = snl_add_msg_attr_nested(&nw, IFA_FREEBSD);
	snl_add_msg_attr_u32(&nw, IFAF_FLAGS, pdata->flags);
	if (pdata->vhid != 0)
		snl_add_msg_attr_u32(&nw, IFAF_VHID, pdata->vhid);
	snl_end_attr_nested(&nw, off);

	if (!snl_finalize_msg(&nw) || !snl_send_message(ctx->io_ss, hdr))
		return (0);

	struct snl_errmsg_data e = {};
	snl_read_reply_code(ctx->io_ss, hdr->nlmsg_seq, &e);

	return (e.error);
}
#endif

#ifdef WITHOUT_NETLINK
static struct	sockaddr_in6 *sin6tab[] = {
	&in6_ridreq.ifr_addr, &in6_addreq.ifra_addr,
	&in6_addreq.ifra_prefixmask, &in6_addreq.ifra_dstaddr
};

static void
in6_copyaddr(if_ctx *ctx __unused, int to, int from)
{
	memcpy(sin6tab[to], sin6tab[from], sizeof(struct sockaddr_in6));
}

static void
in6_getprefix(const char *plen, int which)
{
	struct sockaddr_in6 *sin = sin6tab[which];
	u_char *cp;
	int len = atoi(plen);

	if ((len < 0) || (len > 128))
		errx(1, "%s: bad value", plen);
	sin->sin6_len = sizeof(*sin);
	if (which != MASK)
		sin->sin6_family = AF_INET6;
	if ((len == 0) || (len == 128)) {
		memset(&sin->sin6_addr, 0xff, sizeof(struct in6_addr));
		return;
	}
	memset((void *)&sin->sin6_addr, 0x00, sizeof(sin->sin6_addr));
	for (cp = (u_char *)&sin->sin6_addr; len > 7; len -= 8)
		*cp++ = 0xff;
	*cp = 0xff << (8 - len);
}

static void
in6_getaddr(const char *s, int which)
{
	struct sockaddr_in6 *sin = sin6tab[which];
	struct addrinfo hints, *res;
	int error = -1;

	sin->sin6_len = sizeof(*sin);
	if (which != MASK)
		sin->sin6_family = AF_INET6;

	if (which == ADDR) {
		char *p = NULL;
		if((p = strrchr(s, '/')) != NULL) {
			*p = '\0';
			in6_getprefix(p + 1, MASK);
			explicit_prefix = 1;
		}
	}

	if (sin->sin6_family == AF_INET6) {
		bzero(&hints, sizeof(struct addrinfo));
		hints.ai_family = AF_INET6;
		error = getaddrinfo(s, NULL, &hints, &res);
		if (error != 0) {
			if (inet_pton(AF_INET6, s, &sin->sin6_addr) != 1)
				errx(1, "%s: bad value", s);
		} else {
			bcopy(res->ai_addr, sin, res->ai_addrlen);
			freeaddrinfo(res);
		}
	}
}

static int
prefix(void *val, int size)
{
	u_char *name = (u_char *)val;
	int byte, bit, plen = 0;

	for (byte = 0; byte < size; byte++, plen += 8)
		if (name[byte] != 0xff)
			break;
	if (byte == size)
		return (plen);
	for (bit = 7; bit != 0; bit--, plen++)
		if (!(name[byte] & (1 << bit)))
			break;
	for (; bit != 0; bit--)
		if (name[byte] & (1 << bit))
			return(0);
	byte++;
	for (; byte < size; byte++)
		if (name[byte])
			return(0);
	return (plen);
}
#endif

static char *
sec2str(time_t total)
{
	static char result[256];
	int days, hours, mins, secs;
	int first = 1;
	char *p = result;

	if (0) {
		days = total / 3600 / 24;
		hours = (total / 3600) % 24;
		mins = (total / 60) % 60;
		secs = total % 60;

		if (days) {
			first = 0;
			p += sprintf(p, "%dd", days);
		}
		if (!first || hours) {
			first = 0;
			p += sprintf(p, "%dh", hours);
		}
		if (!first || mins) {
			first = 0;
			p += sprintf(p, "%dm", mins);
		}
		sprintf(p, "%ds", secs);
	} else
		sprintf(result, "%lu", (unsigned long)total);

	return(result);
}

static void
in6_postproc(if_ctx *ctx, int newaddr __unused,
    int ifflags __unused)
{
	if (explicit_prefix == 0) {
		/* Aggregatable address architecture defines all prefixes
		   are 64. So, it is convenient to set prefixlen to 64 if
		   it is not specified. */
		setifprefixlen(ctx, "64", 0);
		/* in6_getprefix("64", MASK) if MASK is available here... */
	}
}

static void
in6_status_tunnel(if_ctx *ctx)
{
	char src[NI_MAXHOST];
	char dst[NI_MAXHOST];
	struct in6_ifreq in6_ifr;
	const struct sockaddr *sa = (const struct sockaddr *) &in6_ifr.ifr_addr;

	memset(&in6_ifr, 0, sizeof(in6_ifr));
	strlcpy(in6_ifr.ifr_name, ctx->ifname, sizeof(in6_ifr.ifr_name));

	if (ioctl_ctx(ctx, SIOCGIFPSRCADDR_IN6, (caddr_t)&in6_ifr) < 0)
		return;
	if (sa->sa_family != AF_INET6)
		return;
	if (getnameinfo(sa, sa->sa_len, src, sizeof(src), 0, 0,
	    NI_NUMERICHOST) != 0)
		src[0] = '\0';

	if (ioctl_ctx(ctx, SIOCGIFPDSTADDR_IN6, (caddr_t)&in6_ifr) < 0)
		return;
	if (sa->sa_family != AF_INET6)
		return;
	if (getnameinfo(sa, sa->sa_len, dst, sizeof(dst), 0, 0,
	    NI_NUMERICHOST) != 0)
		dst[0] = '\0';

	printf("\ttunnel inet6 %s --> %s\n", src, dst);
}

static void
in6_set_tunnel(if_ctx *ctx, struct addrinfo *srcres, struct addrinfo *dstres)
{
	struct in6_aliasreq in6_req = {}; 

	strlcpy(in6_req.ifra_name, ctx->ifname, sizeof(in6_req.ifra_name));
	memcpy(&in6_req.ifra_addr, srcres->ai_addr, srcres->ai_addr->sa_len);
	memcpy(&in6_req.ifra_dstaddr, dstres->ai_addr,
	    dstres->ai_addr->sa_len);

	if (ioctl_ctx(ctx, SIOCSIFPHYADDR_IN6, &in6_req) < 0)
		warn("SIOCSIFPHYADDR_IN6");
}

static void
in6_set_vhid(int vhid)
{
#ifdef WITHOUT_NETLINK
	in6_addreq.ifra_vhid = vhid;
#else
	in6_add.vhid = (uint32_t)vhid;
#endif
}

static struct cmd inet6_cmds[] = {
	DEF_CMD_ARG("prefixlen",			setifprefixlen),
	DEF_CMD("anycast",	IN6_IFF_ANYCAST,	setip6flags),
	DEF_CMD("tentative",	IN6_IFF_TENTATIVE,	setip6flags),
	DEF_CMD("-tentative",	-IN6_IFF_TENTATIVE,	setip6flags),
	DEF_CMD("deprecated",	IN6_IFF_DEPRECATED,	setip6flags),
	DEF_CMD("-deprecated", -IN6_IFF_DEPRECATED,	setip6flags),
	DEF_CMD("autoconf",	IN6_IFF_AUTOCONF,	setip6flags),
	DEF_CMD("-autoconf",	-IN6_IFF_AUTOCONF,	setip6flags),
	DEF_CMD("prefer_source",IN6_IFF_PREFER_SOURCE,	setip6flags),
	DEF_CMD("-prefer_source",-IN6_IFF_PREFER_SOURCE,setip6flags),
	DEF_CMD("accept_rtadv",	ND6_IFF_ACCEPT_RTADV,	setnd6flags),
	DEF_CMD("-accept_rtadv",-ND6_IFF_ACCEPT_RTADV,	setnd6flags),
	DEF_CMD("no_radr",	ND6_IFF_NO_RADR,	setnd6flags),
	DEF_CMD("-no_radr",	-ND6_IFF_NO_RADR,	setnd6flags),
	DEF_CMD("defaultif",	1,			setnd6defif),
	DEF_CMD("-defaultif",	-1,			setnd6defif),
	DEF_CMD("ifdisabled",	ND6_IFF_IFDISABLED,	setnd6flags),
	DEF_CMD("-ifdisabled",	-ND6_IFF_IFDISABLED,	setnd6flags),
	DEF_CMD("nud",		ND6_IFF_PERFORMNUD,	setnd6flags),
	DEF_CMD("-nud",		-ND6_IFF_PERFORMNUD,	setnd6flags),
	DEF_CMD("auto_linklocal",ND6_IFF_AUTO_LINKLOCAL,setnd6flags),
	DEF_CMD("-auto_linklocal",-ND6_IFF_AUTO_LINKLOCAL,setnd6flags),
	DEF_CMD("no_prefer_iface",ND6_IFF_NO_PREFER_IFACE,setnd6flags),
	DEF_CMD("-no_prefer_iface",-ND6_IFF_NO_PREFER_IFACE,setnd6flags),
	DEF_CMD("no_dad",	ND6_IFF_NO_DAD,		setnd6flags),
	DEF_CMD("-no_dad",	-ND6_IFF_NO_DAD,	setnd6flags),
	DEF_CMD_ARG("pltime",        			setip6pltime),
	DEF_CMD_ARG("vltime",        			setip6vltime),
	DEF_CMD("eui64",	0,			setip6eui64),
#ifdef EXPERIMENTAL
	DEF_CMD("ipv6_only",	ND6_IFF_IPV6_ONLY_MANUAL,setnd6flags),
	DEF_CMD("-ipv6_only",	-ND6_IFF_IPV6_ONLY_MANUAL,setnd6flags),
#endif
};

static struct afswtch af_inet6 = {
	.af_name	= "inet6",
	.af_af		= AF_INET6,
#ifdef WITHOUT_NETLINK
	.af_status	= in6_status,
#else
	.af_status	= in6_status_nl,
#endif
	.af_getaddr	= in6_getaddr,
	.af_copyaddr	= in6_copyaddr,
#ifdef WITHOUT_NETLINK
	.af_getprefix	= in6_getprefix,
#endif
	.af_other_status = nd6_status,
	.af_postproc	= in6_postproc,
	.af_status_tunnel = in6_status_tunnel,
	.af_settunnel	= in6_set_tunnel,
	.af_setvhid	= in6_set_vhid,
#ifdef WITHOUT_NETLINK
	.af_difaddr	= SIOCDIFADDR_IN6,
	.af_aifaddr	= SIOCAIFADDR_IN6,
	.af_ridreq	= &in6_addreq,
	.af_addreq	= &in6_addreq,
	.af_exec	= af_exec_ioctl,
#else
	.af_difaddr	= NL_RTM_DELADDR,
	.af_aifaddr	= NL_RTM_NEWADDR,
	.af_ridreq	= &in6_add,
	.af_addreq	= &in6_add,
	.af_exec	= in6_exec_nl,
#endif
};

static void
in6_Lopt_cb(const char *arg __unused)
{
	ip6lifetime++;	/* print IPv6 address lifetime */
}
static struct option in6_Lopt = {
	.opt = "L",
	.opt_usage = "[-L]",
	.cb = in6_Lopt_cb
};

static __constructor void
inet6_ctor(void)
{
	size_t i;

#ifndef RESCUE
	if (!feature_present("inet6"))
		return;
#endif

	for (i = 0; i < nitems(inet6_cmds);  i++)
		cmd_register(&inet6_cmds[i]);
	af_register(&af_inet6);
	opt_register(&in6_Lopt);
}
