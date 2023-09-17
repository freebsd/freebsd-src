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

#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ifaddrs.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "ifconfig.h"
#include "ifconfig_netlink.h"

#ifdef WITHOUT_NETLINK
static struct in_aliasreq in_addreq;
static struct ifreq in_ridreq;
#else
struct in_px {
	struct in_addr		addr;
	int			plen;
	bool			addrset;
	bool			maskset;
};
struct in_pdata {
	struct in_px		addr;
	struct in_px		dst_addr;
	struct in_px		brd_addr;
	uint32_t		flags;
	uint32_t		vhid;
};
static struct in_pdata in_add, in_del;
#endif

static char addr_buf[NI_MAXHOST];	/*for getnameinfo()*/
extern char *f_inet, *f_addr;

static void
print_addr(struct sockaddr_in *sin)
{
	int error, n_flags;

	if (f_addr != NULL && strcmp(f_addr, "fqdn") == 0)
		n_flags = 0;
	else if (f_addr != NULL && strcmp(f_addr, "host") == 0)
		n_flags = NI_NOFQDN;
	else
		n_flags = NI_NUMERICHOST;

	error = getnameinfo((struct sockaddr *)sin, sin->sin_len, addr_buf,
			    sizeof(addr_buf), NULL, 0, n_flags);

	if (error)
		inet_ntop(AF_INET, &sin->sin_addr, addr_buf, sizeof(addr_buf));
	
	printf("\tinet %s", addr_buf);
}

#ifdef WITHOUT_NETLINK
static void
in_status(if_ctx *ctx __unused, const struct ifaddrs *ifa)
{
	struct sockaddr_in *sin, null_sin = {};

	sin = satosin(ifa->ifa_addr);
	if (sin == NULL)
		return;

	print_addr(sin);

	if (ifa->ifa_flags & IFF_POINTOPOINT) {
		sin = satosin(ifa->ifa_dstaddr);
		if (sin == NULL)
			sin = &null_sin;
		printf(" --> %s", inet_ntoa(sin->sin_addr));
	}

	sin = satosin(ifa->ifa_netmask);
	if (sin == NULL)
		sin = &null_sin;
	if (f_inet != NULL && strcmp(f_inet, "cidr") == 0) {
		int cidr = 32;
		unsigned long smask;

		smask = ntohl(sin->sin_addr.s_addr);
		while ((smask & 1) == 0) {
			smask = smask >> 1;
			cidr--;
			if (cidr == 0)
				break;
		}
		printf("/%d", cidr);
	} else if (f_inet != NULL && strcmp(f_inet, "dotted") == 0)
		printf(" netmask %s", inet_ntoa(sin->sin_addr));
	else
		printf(" netmask 0x%lx", (unsigned long)ntohl(sin->sin_addr.s_addr));

	if (ifa->ifa_flags & IFF_BROADCAST) {
		sin = satosin(ifa->ifa_broadaddr);
		if (sin != NULL && sin->sin_addr.s_addr != 0)
			printf(" broadcast %s", inet_ntoa(sin->sin_addr));
	}

	print_vhid(ifa);

	putchar('\n');
}

#else
static struct in_addr
get_mask(int plen)
{
	struct in_addr a;

	a.s_addr = htonl(plen ? ~((1 << (32 - plen)) - 1) : 0);

	return (a);
}

static void
in_status_nl(if_ctx *ctx __unused, if_link_t *link, if_addr_t *ifa)
{
	struct sockaddr_in *sin = satosin(ifa->ifa_local);
	int plen = ifa->ifa_prefixlen;

	print_addr(sin);

	if (link->ifi_flags & IFF_POINTOPOINT) {
		struct sockaddr_in *dst = satosin(ifa->ifa_address);

		printf(" --> %s", inet_ntoa(dst->sin_addr));
	}
	if (f_inet != NULL && strcmp(f_inet, "cidr") == 0) {
		printf("/%d", plen);
	} else if (f_inet != NULL && strcmp(f_inet, "dotted") == 0)
		printf(" netmask %s", inet_ntoa(get_mask(plen)));
	else
		printf(" netmask 0x%lx", (unsigned long)ntohl(get_mask(plen).s_addr));

	if ((link->ifi_flags & IFF_BROADCAST) && plen != 0)  {
		struct sockaddr_in *brd = satosin(ifa->ifa_broadcast);
		if (brd != NULL)
			printf(" broadcast %s", inet_ntoa(brd->sin_addr));
	}

	if (ifa->ifaf_vhid != 0)
		printf(" vhid %d", ifa->ifaf_vhid);

	putchar('\n');
}
#endif


#ifdef WITHOUT_NETLINK
#define SIN(x) ((struct sockaddr_in *) &(x))
static struct sockaddr_in *sintab[] = {
	SIN(in_ridreq.ifr_addr), SIN(in_addreq.ifra_addr),
	SIN(in_addreq.ifra_mask), SIN(in_addreq.ifra_broadaddr)
};

static void
in_copyaddr(if_ctx *ctx __unused, int to, int from)
{
	memcpy(sintab[to], sintab[from], sizeof(struct sockaddr_in));
}

static void
in_getaddr(const char *s, int which)
{
	struct sockaddr_in *sin = sintab[which];
	struct hostent *hp;
	struct netent *np;

	sin->sin_len = sizeof(*sin);
	sin->sin_family = AF_INET;

	if (which == ADDR) {
		char *p = NULL;

		if((p = strrchr(s, '/')) != NULL) {
			const char *errstr;
			/* address is `name/masklen' */
			int masklen = 0;
			struct sockaddr_in *min = sintab[MASK];
			*p = '\0';
			if (!isdigit(*(p + 1)))
				errstr = "invalid";
			else
				masklen = (int)strtonum(p + 1, 0, 32, &errstr);
			if (errstr != NULL) {
				*p = '/';
				errx(1, "%s: bad value (width %s)", s, errstr);
			}
			min->sin_family = AF_INET;
			min->sin_len = sizeof(*min);
			min->sin_addr.s_addr = htonl(~((1LL << (32 - masklen)) - 1) & 
				              0xffffffff);
		}
	}

	if (inet_aton(s, &sin->sin_addr))
		return;
	if ((hp = gethostbyname(s)) != NULL)
		bcopy(hp->h_addr, (char *)&sin->sin_addr, 
		    MIN((size_t)hp->h_length, sizeof(sin->sin_addr)));
	else if ((np = getnetbyname(s)) != NULL)
		sin->sin_addr = inet_makeaddr(np->n_net, INADDR_ANY);
	else
		errx(1, "%s: bad value", s);
}

#else

static struct in_px *sintab_nl[] = {
	&in_del.addr,		/* RIDADDR */
	&in_add.addr,		/* ADDR */
	NULL,			/* MASK */
	&in_add.dst_addr,	/* DSTADDR*/
	&in_add.brd_addr,	/* BRDADDR*/
};

static void
in_copyaddr(if_ctx *ctx __unused, int to, int from)
{
	sintab_nl[to]->addr = sintab_nl[from]->addr;
	sintab_nl[to]->addrset = sintab_nl[from]->addrset;
}

static void
in_getip(const char *addr_str, struct in_addr *ip)
{
	struct hostent *hp;
	struct netent *np;

	if (inet_aton(addr_str, ip))
		return;
	if ((hp = gethostbyname(addr_str)) != NULL)
		bcopy(hp->h_addr, (char *)ip,
		    MIN((size_t)hp->h_length, sizeof(ip)));
	else if ((np = getnetbyname(addr_str)) != NULL)
		*ip = inet_makeaddr(np->n_net, INADDR_ANY);
	else
		errx(1, "%s: bad value", addr_str);
}

static void
in_getaddr(const char *s, int which)
{
        struct in_px *px = sintab_nl[which];

	if (which == MASK) {
		struct in_px *px_addr = sintab_nl[ADDR];
		struct in_addr mask = {};

		in_getip(s, &mask);
		px_addr->plen = __bitcount32(mask.s_addr);
		px_addr->maskset = true;
		return;
	}

	if (which == ADDR) {
		char *p = NULL;

		if((p = strrchr(s, '/')) != NULL) {
			const char *errstr;
			/* address is `name/masklen' */
			int masklen;
			*p = '\0';
			if (!isdigit(*(p + 1)))
				errstr = "invalid";
			else
				masklen = (int)strtonum(p + 1, 0, 32, &errstr);
			if (errstr != NULL) {
				*p = '/';
				errx(1, "%s: bad value (width %s)", s, errstr);
			}
			px->plen = masklen;
			px->maskset = true;
		}
	}

	in_getip(s, &px->addr);
	px->addrset = true;
}

/*
 * Deletes the first found IPv4 interface address for the interface.
 *
 * This function provides SIOCDIFADDR semantics missing in Netlink.
 * When no valid IPv4 address is specified (sin_family or sin_len is wrong) to
 * the SIOCDIFADDR call, it deletes the first found IPv4 address on the interface.
 * 'ifconfig IFNAME inet addr/prefix' relies on that behavior, as it
 *  executes empty SIOCDIFADDR before adding a new address.
 */
static int
in_delete_first_nl(if_ctx *ctx)
{
	struct nlmsghdr *hdr;
	struct ifaddrmsg *ifahdr;
	uint32_t nlmsg_seq;
	struct in_addr addr;
	struct snl_writer nw = {};
	struct snl_errmsg_data e = {};
	struct snl_state *ss = ctx->io_ss;
	bool found = false;

	uint32_t ifindex = if_nametoindex_nl(ss, ctx->ifname);
	if (ifindex == 0) {
		/* No interface with the desired name, nothing to delete */
		return (EADDRNOTAVAIL);
	}

	snl_init_writer(ss, &nw);
	hdr = snl_create_msg_request(&nw, NL_RTM_GETADDR);
	hdr->nlmsg_flags |= NLM_F_DUMP;
	ifahdr = snl_reserve_msg_object(&nw, struct ifaddrmsg);
	ifahdr->ifa_family = AF_INET;
	ifahdr->ifa_index = ifindex;

	if (!snl_finalize_msg(&nw) || !snl_send_message(ss, hdr))
		return (EINVAL);

	nlmsg_seq = hdr->nlmsg_seq;
	while ((hdr = snl_read_reply_multi(ss, nlmsg_seq, &e)) != NULL) {
		struct snl_parsed_addr attrs = {};
		if (snl_parse_nlmsg(ss, hdr, &snl_rtm_addr_parser, &attrs)) {
			addr = satosin(attrs.ifa_local)->sin_addr;
			ifindex = attrs.ifa_index;
			found = true;
			break;
		} else
			return (EINVAL);
	}
	if (e.error != 0) {
		if (e.error_str != NULL)
			warnx("%s(): %s", __func__, e.error_str);
		return (e.error);
	}

	if (!found)
		return (0);

	/* Try to delete the found address */
	snl_init_writer(ss, &nw);
	hdr = snl_create_msg_request(&nw, NL_RTM_DELADDR);
	ifahdr = snl_reserve_msg_object(&nw, struct ifaddrmsg);
	ifahdr->ifa_family = AF_INET;
	ifahdr->ifa_index = ifindex;
	snl_add_msg_attr_ip4(&nw, IFA_LOCAL, &addr);

	if (!snl_finalize_msg(&nw) || !snl_send_message(ss, hdr))
		return (EINVAL);
	memset(&e, 0, sizeof(e));
	snl_read_reply_code(ss, hdr->nlmsg_seq, &e);
	if (e.error_str != NULL)
		warnx("%s(): %s", __func__, e.error_str);

	return (e.error);
}


static int
in_exec_nl(if_ctx *ctx, unsigned long action, void *data)
{
	struct in_pdata *pdata = (struct in_pdata *)data;
	struct snl_writer nw = {};

	if (action == NL_RTM_DELADDR && !pdata->addr.addrset)
		return (in_delete_first_nl(ctx));

	snl_init_writer(ctx->io_ss, &nw);
	struct nlmsghdr *hdr = snl_create_msg_request(&nw, action);
	struct ifaddrmsg *ifahdr = snl_reserve_msg_object(&nw, struct ifaddrmsg);

	ifahdr->ifa_family = AF_INET;
	ifahdr->ifa_prefixlen = pdata->addr.plen;
	ifahdr->ifa_index = if_nametoindex_nl(ctx->io_ss, ctx->ifname);

	snl_add_msg_attr_ip4(&nw, IFA_LOCAL, &pdata->addr.addr);
	if (action == NL_RTM_NEWADDR && pdata->dst_addr.addrset)
		snl_add_msg_attr_ip4(&nw, IFA_ADDRESS, &pdata->dst_addr.addr);
	if (action == NL_RTM_NEWADDR && pdata->brd_addr.addrset)
		snl_add_msg_attr_ip4(&nw, IFA_BROADCAST, &pdata->brd_addr.addr);

	int off = snl_add_msg_attr_nested(&nw, IFA_FREEBSD);
	snl_add_msg_attr_u32(&nw, IFAF_FLAGS, pdata->flags);
	if (pdata->vhid != 0)
		snl_add_msg_attr_u32(&nw, IFAF_VHID, pdata->vhid);
	snl_end_attr_nested(&nw, off);

	if (!snl_finalize_msg(&nw) || !snl_send_message(ctx->io_ss, hdr))
		return (0);

	struct snl_errmsg_data e = {};
	snl_read_reply_code(ctx->io_ss, hdr->nlmsg_seq, &e);
	if (e.error_str != NULL)
		warnx("%s(): %s", __func__, e.error_str);

	return (e.error);
}

static void
in_setdefaultmask_nl(void)
{
        struct in_px *px = sintab_nl[ADDR];

	in_addr_t i = ntohl(px->addr.s_addr);

	/*
	 * If netmask isn't supplied, use historical default.
	 * This is deprecated for interfaces other than loopback
	 * or point-to-point; warn in other cases.  In the future
	 * we should return an error rather than warning.
	 */
	if (IN_CLASSA(i))
		px->plen = IN_CLASSA_NSHIFT;
	else if (IN_CLASSB(i))
		px->plen = IN_CLASSB_NSHIFT;
	else
		px->plen = IN_CLASSC_NSHIFT;
	px->maskset = true;
}
#endif

static void
warn_nomask(int ifflags)
{
    if ((ifflags & (IFF_POINTOPOINT | IFF_LOOPBACK)) == 0) {
	warnx("WARNING: setting interface address without mask "
	    "is deprecated,\ndefault mask may not be correct.");
    }
}

static void
in_postproc(if_ctx *ctx __unused, int newaddr, int ifflags)
{
#ifdef WITHOUT_NETLINK
	if (sintab[ADDR]->sin_len != 0 && sintab[MASK]->sin_len == 0 && newaddr) {
		warn_nomask(ifflags);
	}
#else
	if (sintab_nl[ADDR]->addrset && !sintab_nl[ADDR]->maskset && newaddr) {
		warn_nomask(ifflags);
	    in_setdefaultmask_nl();
	}
#endif
}

static void
in_status_tunnel(if_ctx *ctx)
{
	char src[NI_MAXHOST];
	char dst[NI_MAXHOST];
	struct ifreq ifr;
	const struct sockaddr *sa = (const struct sockaddr *) &ifr.ifr_addr;

	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, ctx->ifname, IFNAMSIZ);

	if (ioctl_ctx(ctx, SIOCGIFPSRCADDR, (caddr_t)&ifr) < 0)
		return;
	if (sa->sa_family != AF_INET)
		return;
	if (getnameinfo(sa, sa->sa_len, src, sizeof(src), 0, 0, NI_NUMERICHOST) != 0)
		src[0] = '\0';

	if (ioctl_ctx(ctx, SIOCGIFPDSTADDR, (caddr_t)&ifr) < 0)
		return;
	if (sa->sa_family != AF_INET)
		return;
	if (getnameinfo(sa, sa->sa_len, dst, sizeof(dst), 0, 0, NI_NUMERICHOST) != 0)
		dst[0] = '\0';

	printf("\ttunnel inet %s --> %s\n", src, dst);
}

static void
in_set_tunnel(if_ctx *ctx, struct addrinfo *srcres, struct addrinfo *dstres)
{
	struct in_aliasreq addreq;

	memset(&addreq, 0, sizeof(addreq));
	strlcpy(addreq.ifra_name, ctx->ifname, IFNAMSIZ);
	memcpy(&addreq.ifra_addr, srcres->ai_addr, srcres->ai_addr->sa_len);
	memcpy(&addreq.ifra_dstaddr, dstres->ai_addr, dstres->ai_addr->sa_len);

	if (ioctl_ctx(ctx, SIOCSIFPHYADDR, &addreq) < 0)
		warn("SIOCSIFPHYADDR");
}

static void
in_set_vhid(int vhid)
{
#ifdef WITHOUT_NETLINK
	in_addreq.ifra_vhid = vhid;
#else
	in_add.vhid = (uint32_t)vhid;
#endif
}

static struct afswtch af_inet = {
	.af_name	= "inet",
	.af_af		= AF_INET,
#ifdef WITHOUT_NETLINK
	.af_status	= in_status,
#else
	.af_status	= in_status_nl,
#endif
	.af_getaddr	= in_getaddr,
	.af_copyaddr	= in_copyaddr,
	.af_postproc	= in_postproc,
	.af_status_tunnel = in_status_tunnel,
	.af_settunnel	= in_set_tunnel,
	.af_setvhid	= in_set_vhid,
#ifdef WITHOUT_NETLINK
	.af_difaddr	= SIOCDIFADDR,
	.af_aifaddr	= SIOCAIFADDR,
	.af_ridreq	= &in_ridreq,
	.af_addreq	= &in_addreq,
	.af_exec	= af_exec_ioctl,
#else
	.af_difaddr	= NL_RTM_DELADDR,
	.af_aifaddr	= NL_RTM_NEWADDR,
	.af_ridreq	= &in_del,
	.af_addreq	= &in_add,
	.af_exec	= in_exec_nl,
#endif
};

static __constructor void
inet_ctor(void)
{

#ifndef RESCUE
	if (!feature_present("inet"))
		return;
#endif
	af_register(&af_inet);
}
