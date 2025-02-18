/*	$OpenBSD: pfctl.c,v 1.278 2008/08/31 20:18:17 jmc Exp $ */

/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2001 Daniel Hartmeier
 * Copyright (c) 2002,2003 Henning Brauer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
#define PFIOC_USE_LATEST

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/endian.h>

#include <net/if.h>
#include <netinet/in.h>
#include <net/pfvar.h>
#include <arpa/inet.h>
#include <net/altq/altq.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libpfctl.h>
#include <limits.h>
#include <netdb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfctl_parser.h"
#include "pfctl.h"

void	 usage(void);
int	 pfctl_enable(int, int);
int	 pfctl_disable(int, int);
int	 pfctl_clear_stats(struct pfctl_handle *, int);
int	 pfctl_get_skip_ifaces(void);
int	 pfctl_check_skip_ifaces(char *);
int	 pfctl_adjust_skip_ifaces(struct pfctl *);
int	 pfctl_clear_interface_flags(int, int);
int	 pfctl_flush_eth_rules(int, int, char *);
int	 pfctl_flush_rules(int, int, char *);
int	 pfctl_flush_nat(int, int, char *);
int	 pfctl_clear_altq(int, int);
int	 pfctl_clear_src_nodes(int, int);
int	 pfctl_clear_iface_states(int, const char *, int);
void	 pfctl_addrprefix(char *, struct pf_addr *);
int	 pfctl_kill_src_nodes(int, const char *, int);
int	 pfctl_net_kill_states(int, const char *, int);
int	 pfctl_gateway_kill_states(int, const char *, int);
int	 pfctl_label_kill_states(int, const char *, int);
int	 pfctl_id_kill_states(int, const char *, int);
void	 pfctl_init_options(struct pfctl *);
int	 pfctl_load_options(struct pfctl *);
int	 pfctl_load_limit(struct pfctl *, unsigned int, unsigned int);
int	 pfctl_load_timeout(struct pfctl *, unsigned int, unsigned int);
int	 pfctl_load_debug(struct pfctl *, unsigned int);
int	 pfctl_load_logif(struct pfctl *, char *);
int	 pfctl_load_hostid(struct pfctl *, u_int32_t);
int	 pfctl_load_reassembly(struct pfctl *, u_int32_t);
int	 pfctl_load_syncookies(struct pfctl *, u_int8_t);
int	 pfctl_get_pool(int, struct pfctl_pool *, u_int32_t, u_int32_t, int,
	    char *, int);
void	 pfctl_print_eth_rule_counters(struct pfctl_eth_rule *, int);
void	 pfctl_print_rule_counters(struct pfctl_rule *, int);
int	 pfctl_show_eth_rules(int, char *, int, enum pfctl_show, char *, int, int);
int	 pfctl_show_rules(int, char *, int, enum pfctl_show, char *, int, int);
int	 pfctl_show_nat(int, char *, int, char *, int, int);
int	 pfctl_show_src_nodes(int, int);
int	 pfctl_show_states(int, const char *, int);
int	 pfctl_show_status(int, int);
int	 pfctl_show_running(int);
int	 pfctl_show_timeouts(int, int);
int	 pfctl_show_limits(int, int);
void	 pfctl_debug(int, u_int32_t, int);
int	 pfctl_test_altqsupport(int, int);
int	 pfctl_show_anchors(int, int, char *);
int	 pfctl_show_eth_anchors(int, int, char *);
int	 pfctl_ruleset_trans(struct pfctl *, char *, struct pfctl_anchor *, bool);
int	 pfctl_eth_ruleset_trans(struct pfctl *, char *,
	    struct pfctl_eth_anchor *);
int	 pfctl_load_eth_ruleset(struct pfctl *, char *,
	    struct pfctl_eth_ruleset *, int);
int	 pfctl_load_eth_rule(struct pfctl *, char *, struct pfctl_eth_rule *,
	    int);
int	 pfctl_load_ruleset(struct pfctl *, char *,
		struct pfctl_ruleset *, int, int);
int	 pfctl_load_rule(struct pfctl *, char *, struct pfctl_rule *, int);
const char	*pfctl_lookup_option(char *, const char * const *);

static struct pfctl_anchor_global	 pf_anchors;
struct pfctl_anchor	 pf_main_anchor;
struct pfctl_eth_anchor	 pf_eth_main_anchor;
static struct pfr_buffer skip_b;

static const char	*clearopt;
static char		*rulesopt;
static const char	*showopt;
static const char	*debugopt;
static char		*anchoropt;
static const char	*optiopt = NULL;
static const char	*pf_device = PF_DEVICE;
static char		*ifaceopt;
static char		*tableopt;
static const char	*tblcmdopt;
static int		 src_node_killers;
static char		*src_node_kill[2];
static int		 state_killers;
static char		*state_kill[2];
int			 loadopt;
int			 altqsupport;

int			 dev = -1;
struct pfctl_handle	*pfh = NULL;
static int		 first_title = 1;
static int		 labels = 0;

#define INDENT(d, o)	do {						\
				if (o) {				\
					int i;				\
					for (i=0; i < d; i++)		\
						printf("  ");		\
				}					\
			} while (0);					\


static const struct {
	const char	*name;
	int		index;
} pf_limits[] = {
	{ "states",		PF_LIMIT_STATES },
	{ "src-nodes",		PF_LIMIT_SRC_NODES },
	{ "frags",		PF_LIMIT_FRAGS },
	{ "table-entries",	PF_LIMIT_TABLE_ENTRIES },
	{ NULL,			0 }
};

struct pf_hint {
	const char	*name;
	int		timeout;
};
static const struct pf_hint pf_hint_normal[] = {
	{ "tcp.first",		2 * 60 },
	{ "tcp.opening",	30 },
	{ "tcp.established",	24 * 60 * 60 },
	{ "tcp.closing",	15 * 60 },
	{ "tcp.finwait",	45 },
	{ "tcp.closed",		90 },
	{ "tcp.tsdiff",		30 },
	{ NULL,			0 }
};
static const struct pf_hint pf_hint_satellite[] = {
	{ "tcp.first",		3 * 60 },
	{ "tcp.opening",	30 + 5 },
	{ "tcp.established",	24 * 60 * 60 },
	{ "tcp.closing",	15 * 60 + 5 },
	{ "tcp.finwait",	45 + 5 },
	{ "tcp.closed",		90 + 5 },
	{ "tcp.tsdiff",		60 },
	{ NULL,			0 }
};
static const struct pf_hint pf_hint_conservative[] = {
	{ "tcp.first",		60 * 60 },
	{ "tcp.opening",	15 * 60 },
	{ "tcp.established",	5 * 24 * 60 * 60 },
	{ "tcp.closing",	60 * 60 },
	{ "tcp.finwait",	10 * 60 },
	{ "tcp.closed",		3 * 60 },
	{ "tcp.tsdiff",		60 },
	{ NULL,			0 }
};
static const struct pf_hint pf_hint_aggressive[] = {
	{ "tcp.first",		30 },
	{ "tcp.opening",	5 },
	{ "tcp.established",	5 * 60 * 60 },
	{ "tcp.closing",	60 },
	{ "tcp.finwait",	30 },
	{ "tcp.closed",		30 },
	{ "tcp.tsdiff",		10 },
	{ NULL,			0 }
};

static const struct {
	const char *name;
	const struct pf_hint *hint;
} pf_hints[] = {
	{ "normal",		pf_hint_normal },
	{ "satellite",		pf_hint_satellite },
	{ "high-latency",	pf_hint_satellite },
	{ "conservative",	pf_hint_conservative },
	{ "aggressive",		pf_hint_aggressive },
	{ NULL,			NULL }
};

static const char * const clearopt_list[] = {
	"nat", "queue", "rules", "Sources",
	"states", "info", "Tables", "osfp", "all",
	"ethernet", NULL
};

static const char * const showopt_list[] = {
	"ether", "nat", "queue", "rules", "Anchors", "Sources", "states",
	"info", "Interfaces", "labels", "timeouts", "memory", "Tables",
	"osfp", "Running", "all", "creatorids", NULL
};

static const char * const tblcmdopt_list[] = {
	"kill", "flush", "add", "delete", "load", "replace", "show",
	"test", "zero", "expire", "reset", NULL
};

static const char * const debugopt_list[] = {
	"none", "urgent", "misc", "loud", NULL
};

static const char * const optiopt_list[] = {
	"none", "basic", "profile", NULL
};

void
usage(void)
{
	extern char *__progname;

	fprintf(stderr,
"usage: %s [-AdeghMmNnOPqRrvz] [-a anchor] [-D macro=value] [-F modifier]\n"
	"\t[-f file] [-i interface] [-K host | network]\n"
	"\t[-k host | network | gateway | label | id] [-o level] [-p device]\n"
	"\t[-s modifier] [-t table -T command [address ...]] [-x level]\n",
	    __progname);

	exit(1);
}

/*
 * Cache protocol number to name translations.
 *
 * Translation is performed a lot e.g., when dumping states and
 * getprotobynumber is incredibly expensive.
 *
 * Note from the getprotobynumber(3) manpage:
 * <quote>
 * These functions use a thread-specific data space; if the data is needed
 * for future use, it should be copied before any subsequent calls overwrite
 * it.  Only the Internet protocols are currently understood.
 * </quote>
 *
 * Consequently we only cache the name and strdup it for safety.
 *
 * At the time of writing this comment the last entry in /etc/protocols is:
 * divert  258     DIVERT          # Divert pseudo-protocol [non IANA]
 */
const char *
pfctl_proto2name(int proto)
{
	static const char *pfctl_proto_cache[259];
	struct protoent *p;

	if (proto >= nitems(pfctl_proto_cache)) {
		p = getprotobynumber(proto);
		if (p == NULL) {
			return (NULL);
		}
		return (p->p_name);
	}

	if (pfctl_proto_cache[proto] == NULL) {
		p = getprotobynumber(proto);
		if (p == NULL) {
			return (NULL);
		}
		pfctl_proto_cache[proto] = strdup(p->p_name);
	}

	return (pfctl_proto_cache[proto]);
}

int
pfctl_enable(int dev, int opts)
{
	int ret;

	if ((ret = pfctl_startstop(pfh, 1)) != 0) {
		if (ret == EEXIST)
			errx(1, "pf already enabled");
		else if (ret == ESRCH)
			errx(1, "pfil registeration failed");
		else
			errc(1, ret, "DIOCSTART");
	}
	if ((opts & PF_OPT_QUIET) == 0)
		fprintf(stderr, "pf enabled\n");

	if (altqsupport && ioctl(dev, DIOCSTARTALTQ))
		if (errno != EEXIST)
			err(1, "DIOCSTARTALTQ");

	return (0);
}

int
pfctl_disable(int dev, int opts)
{
	int ret;

	if ((ret = pfctl_startstop(pfh, 0)) != 0) {
		if (ret == ENOENT)
			errx(1, "pf not enabled");
		else
			errc(1, ret, "DIOCSTOP");
	}
	if ((opts & PF_OPT_QUIET) == 0)
		fprintf(stderr, "pf disabled\n");

	if (altqsupport && ioctl(dev, DIOCSTOPALTQ))
			if (errno != ENOENT)
				err(1, "DIOCSTOPALTQ");

	return (0);
}

int
pfctl_clear_stats(struct pfctl_handle *h, int opts)
{
	int ret;
	if ((ret = pfctl_clear_status(h)) != 0)
		errc(1, ret, "DIOCCLRSTATUS");
	if ((opts & PF_OPT_QUIET) == 0)
		fprintf(stderr, "pf: statistics cleared\n");
	return (0);
}

int
pfctl_get_skip_ifaces(void)
{
	bzero(&skip_b, sizeof(skip_b));
	skip_b.pfrb_type = PFRB_IFACES;
	for (;;) {
		pfr_buf_grow(&skip_b, skip_b.pfrb_size);
		skip_b.pfrb_size = skip_b.pfrb_msize;
		if (pfi_get_ifaces(NULL, skip_b.pfrb_caddr, &skip_b.pfrb_size))
			err(1, "pfi_get_ifaces");
		if (skip_b.pfrb_size <= skip_b.pfrb_msize)
			break;
	}
	return (0);
}

int
pfctl_check_skip_ifaces(char *ifname)
{
	struct pfi_kif		*p;
	struct node_host	*h = NULL, *n = NULL;

	PFRB_FOREACH(p, &skip_b) {
		if (!strcmp(ifname, p->pfik_name) &&
		    (p->pfik_flags & PFI_IFLAG_SKIP))
			p->pfik_flags &= ~PFI_IFLAG_SKIP;
		if (!strcmp(ifname, p->pfik_name) && p->pfik_group != NULL) {
			if ((h = ifa_grouplookup(p->pfik_name, 0)) == NULL)
				continue;

			for (n = h; n != NULL; n = n->next) {
				if (strncmp(p->pfik_name, ifname, IFNAMSIZ))
					continue;

				p->pfik_flags &= ~PFI_IFLAG_SKIP;
			}
		}
	}
	return (0);
}

int
pfctl_adjust_skip_ifaces(struct pfctl *pf)
{
	struct pfi_kif		*p, *pp;
	struct node_host	*h = NULL, *n = NULL;

	PFRB_FOREACH(p, &skip_b) {
		if (p->pfik_group == NULL || !(p->pfik_flags & PFI_IFLAG_SKIP))
			continue;

		pfctl_set_interface_flags(pf, p->pfik_name, PFI_IFLAG_SKIP, 0);
		if ((h = ifa_grouplookup(p->pfik_name, 0)) == NULL)
			continue;

		for (n = h; n != NULL; n = n->next)
			PFRB_FOREACH(pp, &skip_b) {
				if (strncmp(pp->pfik_name, n->ifname, IFNAMSIZ))
					continue;

				if (!(pp->pfik_flags & PFI_IFLAG_SKIP))
					pfctl_set_interface_flags(pf,
					    pp->pfik_name, PFI_IFLAG_SKIP, 1);
				if (pp->pfik_flags & PFI_IFLAG_SKIP)
					pp->pfik_flags &= ~PFI_IFLAG_SKIP;
			}
	}

	PFRB_FOREACH(p, &skip_b) {
		if (! (p->pfik_flags & PFI_IFLAG_SKIP))
			continue;

		pfctl_set_interface_flags(pf, p->pfik_name, PFI_IFLAG_SKIP, 0);
	}

	return (0);
}

int
pfctl_clear_interface_flags(int dev, int opts)
{
	struct pfioc_iface	pi;

	if ((opts & PF_OPT_NOACTION) == 0) {
		bzero(&pi, sizeof(pi));
		pi.pfiio_flags = PFI_IFLAG_SKIP;

		if (ioctl(dev, DIOCCLRIFFLAG, &pi))
			err(1, "DIOCCLRIFFLAG");
		if ((opts & PF_OPT_QUIET) == 0)
			fprintf(stderr, "pf: interface flags reset\n");
	}
	return (0);
}

int
pfctl_flush_eth_rules(int dev, int opts, char *anchorname)
{
	int ret;

	ret = pfctl_clear_eth_rules(dev, anchorname);
	if (ret != 0)
		err(1, "pfctl_clear_eth_rules");

	if ((opts & PF_OPT_QUIET) == 0)
		fprintf(stderr, "Ethernet rules cleared\n");

	return (ret);
}

int
pfctl_flush_rules(int dev, int opts, char *anchorname)
{
	int ret;

	ret = pfctl_clear_rules(dev, anchorname);
	if (ret != 0)
		err(1, "pfctl_clear_rules");
	if ((opts & PF_OPT_QUIET) == 0)
		fprintf(stderr, "rules cleared\n");
	return (0);
}

int
pfctl_flush_nat(int dev, int opts, char *anchorname)
{
	int ret;

	ret = pfctl_clear_nat(dev, anchorname);
	if (ret != 0)
		err(1, "pfctl_clear_nat");
	if ((opts & PF_OPT_QUIET) == 0)
		fprintf(stderr, "nat cleared\n");
	return (0);
}

int
pfctl_clear_altq(int dev, int opts)
{
	struct pfr_buffer t;

	if (!altqsupport)
		return (-1);
	memset(&t, 0, sizeof(t));
	t.pfrb_type = PFRB_TRANS;
	if (pfctl_add_trans(&t, PF_RULESET_ALTQ, "") ||
	    pfctl_trans(dev, &t, DIOCXBEGIN, 0) ||
	    pfctl_trans(dev, &t, DIOCXCOMMIT, 0))
		err(1, "pfctl_clear_altq");
	if ((opts & PF_OPT_QUIET) == 0)
		fprintf(stderr, "altq cleared\n");
	return (0);
}

int
pfctl_clear_src_nodes(int dev, int opts)
{
	if (ioctl(dev, DIOCCLRSRCNODES))
		err(1, "DIOCCLRSRCNODES");
	if ((opts & PF_OPT_QUIET) == 0)
		fprintf(stderr, "source tracking entries cleared\n");
	return (0);
}

int
pfctl_clear_iface_states(int dev, const char *iface, int opts)
{
	struct pfctl_kill kill;
	unsigned int killed;
	int ret;

	memset(&kill, 0, sizeof(kill));
	if (iface != NULL && strlcpy(kill.ifname, iface,
	    sizeof(kill.ifname)) >= sizeof(kill.ifname))
		errx(1, "invalid interface: %s", iface);

	if (opts & PF_OPT_KILLMATCH)
		kill.kill_match = true;

	if ((ret = pfctl_clear_states_h(pfh, &kill, &killed)) != 0)
		errc(1, ret, "DIOCCLRSTATES");
	if ((opts & PF_OPT_QUIET) == 0)
		fprintf(stderr, "%d states cleared\n", killed);
	return (0);
}

void
pfctl_addrprefix(char *addr, struct pf_addr *mask)
{
	char *p;
	const char *errstr;
	int prefix, ret_ga, q, r;
	struct addrinfo hints, *res;

	if ((p = strchr(addr, '/')) == NULL)
		return;

	*p++ = '\0';
	prefix = strtonum(p, 0, 128, &errstr);
	if (errstr)
		errx(1, "prefix is %s: %s", errstr, p);

	bzero(&hints, sizeof(hints));
	/* prefix only with numeric addresses */
	hints.ai_flags |= AI_NUMERICHOST;

	if ((ret_ga = getaddrinfo(addr, NULL, &hints, &res))) {
		errx(1, "getaddrinfo: %s", gai_strerror(ret_ga));
		/* NOTREACHED */
	}

	if (res->ai_family == AF_INET && prefix > 32)
		errx(1, "prefix too long for AF_INET");
	else if (res->ai_family == AF_INET6 && prefix > 128)
		errx(1, "prefix too long for AF_INET6");

	q = prefix >> 3;
	r = prefix & 7;
	switch (res->ai_family) {
	case AF_INET:
		bzero(&mask->v4, sizeof(mask->v4));
		mask->v4.s_addr = htonl((u_int32_t)
		    (0xffffffffffULL << (32 - prefix)));
		break;
	case AF_INET6:
		bzero(&mask->v6, sizeof(mask->v6));
		if (q > 0)
			memset((void *)&mask->v6, 0xff, q);
		if (r > 0)
			*((u_char *)&mask->v6 + q) =
			    (0xff00 >> r) & 0xff;
		break;
	}
	freeaddrinfo(res);
}

int
pfctl_kill_src_nodes(int dev, const char *iface, int opts)
{
	struct pfioc_src_node_kill psnk;
	struct addrinfo *res[2], *resp[2];
	struct sockaddr last_src, last_dst;
	int killed, sources, dests;
	int ret_ga;

	killed = sources = dests = 0;

	memset(&psnk, 0, sizeof(psnk));
	memset(&psnk.psnk_src.addr.v.a.mask, 0xff,
	    sizeof(psnk.psnk_src.addr.v.a.mask));
	memset(&last_src, 0xff, sizeof(last_src));
	memset(&last_dst, 0xff, sizeof(last_dst));

	pfctl_addrprefix(src_node_kill[0], &psnk.psnk_src.addr.v.a.mask);

	if ((ret_ga = getaddrinfo(src_node_kill[0], NULL, NULL, &res[0]))) {
		errx(1, "getaddrinfo: %s", gai_strerror(ret_ga));
		/* NOTREACHED */
	}
	for (resp[0] = res[0]; resp[0]; resp[0] = resp[0]->ai_next) {
		if (resp[0]->ai_addr == NULL)
			continue;
		/* We get lots of duplicates.  Catch the easy ones */
		if (memcmp(&last_src, resp[0]->ai_addr, sizeof(last_src)) == 0)
			continue;
		last_src = *(struct sockaddr *)resp[0]->ai_addr;

		psnk.psnk_af = resp[0]->ai_family;
		sources++;

		if (psnk.psnk_af == AF_INET)
			psnk.psnk_src.addr.v.a.addr.v4 =
			    ((struct sockaddr_in *)resp[0]->ai_addr)->sin_addr;
		else if (psnk.psnk_af == AF_INET6)
			psnk.psnk_src.addr.v.a.addr.v6 =
			    ((struct sockaddr_in6 *)resp[0]->ai_addr)->
			    sin6_addr;
		else
			errx(1, "Unknown address family %d", psnk.psnk_af);

		if (src_node_killers > 1) {
			dests = 0;
			memset(&psnk.psnk_dst.addr.v.a.mask, 0xff,
			    sizeof(psnk.psnk_dst.addr.v.a.mask));
			memset(&last_dst, 0xff, sizeof(last_dst));
			pfctl_addrprefix(src_node_kill[1],
			    &psnk.psnk_dst.addr.v.a.mask);
			if ((ret_ga = getaddrinfo(src_node_kill[1], NULL, NULL,
			    &res[1]))) {
				errx(1, "getaddrinfo: %s",
				    gai_strerror(ret_ga));
				/* NOTREACHED */
			}
			for (resp[1] = res[1]; resp[1];
			    resp[1] = resp[1]->ai_next) {
				if (resp[1]->ai_addr == NULL)
					continue;
				if (psnk.psnk_af != resp[1]->ai_family)
					continue;

				if (memcmp(&last_dst, resp[1]->ai_addr,
				    sizeof(last_dst)) == 0)
					continue;
				last_dst = *(struct sockaddr *)resp[1]->ai_addr;

				dests++;

				if (psnk.psnk_af == AF_INET)
					psnk.psnk_dst.addr.v.a.addr.v4 =
					    ((struct sockaddr_in *)resp[1]->
					    ai_addr)->sin_addr;
				else if (psnk.psnk_af == AF_INET6)
					psnk.psnk_dst.addr.v.a.addr.v6 =
					    ((struct sockaddr_in6 *)resp[1]->
					    ai_addr)->sin6_addr;
				else
					errx(1, "Unknown address family %d",
					    psnk.psnk_af);

				if (ioctl(dev, DIOCKILLSRCNODES, &psnk))
					err(1, "DIOCKILLSRCNODES");
				killed += psnk.psnk_killed;
			}
			freeaddrinfo(res[1]);
		} else {
			if (ioctl(dev, DIOCKILLSRCNODES, &psnk))
				err(1, "DIOCKILLSRCNODES");
			killed += psnk.psnk_killed;
		}
	}

	freeaddrinfo(res[0]);

	if ((opts & PF_OPT_QUIET) == 0)
		fprintf(stderr, "killed %d src nodes from %d sources and %d "
		    "destinations\n", killed, sources, dests);
	return (0);
}

int
pfctl_net_kill_states(int dev, const char *iface, int opts)
{
	struct pfctl_kill kill;
	struct addrinfo *res[2], *resp[2];
	struct sockaddr last_src, last_dst;
	unsigned int newkilled;
	int killed, sources, dests;
	int ret_ga, ret;

	killed = sources = dests = 0;

	memset(&kill, 0, sizeof(kill));
	memset(&kill.src.addr.v.a.mask, 0xff,
	    sizeof(kill.src.addr.v.a.mask));
	memset(&last_src, 0xff, sizeof(last_src));
	memset(&last_dst, 0xff, sizeof(last_dst));
	if (iface != NULL && strlcpy(kill.ifname, iface,
	    sizeof(kill.ifname)) >= sizeof(kill.ifname))
		errx(1, "invalid interface: %s", iface);

	if (state_killers == 2 && (strcmp(state_kill[0], "nat") == 0)) {
		kill.nat = true;
		state_kill[0] = state_kill[1];
		state_killers = 1;
	}

	pfctl_addrprefix(state_kill[0], &kill.src.addr.v.a.mask);

	if (opts & PF_OPT_KILLMATCH)
		kill.kill_match = true;

	if ((ret_ga = getaddrinfo(state_kill[0], NULL, NULL, &res[0]))) {
		errx(1, "getaddrinfo: %s", gai_strerror(ret_ga));
		/* NOTREACHED */
	}
	for (resp[0] = res[0]; resp[0]; resp[0] = resp[0]->ai_next) {
		if (resp[0]->ai_addr == NULL)
			continue;
		/* We get lots of duplicates.  Catch the easy ones */
		if (memcmp(&last_src, resp[0]->ai_addr, sizeof(last_src)) == 0)
			continue;
		last_src = *(struct sockaddr *)resp[0]->ai_addr;

		kill.af = resp[0]->ai_family;
		sources++;

		if (kill.af == AF_INET)
			kill.src.addr.v.a.addr.v4 =
			    ((struct sockaddr_in *)resp[0]->ai_addr)->sin_addr;
		else if (kill.af == AF_INET6)
			kill.src.addr.v.a.addr.v6 =
			    ((struct sockaddr_in6 *)resp[0]->ai_addr)->
			    sin6_addr;
		else
			errx(1, "Unknown address family %d", kill.af);

		if (state_killers > 1) {
			dests = 0;
			memset(&kill.dst.addr.v.a.mask, 0xff,
			    sizeof(kill.dst.addr.v.a.mask));
			memset(&last_dst, 0xff, sizeof(last_dst));
			pfctl_addrprefix(state_kill[1],
			    &kill.dst.addr.v.a.mask);
			if ((ret_ga = getaddrinfo(state_kill[1], NULL, NULL,
			    &res[1]))) {
				errx(1, "getaddrinfo: %s",
				    gai_strerror(ret_ga));
				/* NOTREACHED */
			}
			for (resp[1] = res[1]; resp[1];
			    resp[1] = resp[1]->ai_next) {
				if (resp[1]->ai_addr == NULL)
					continue;
				if (kill.af != resp[1]->ai_family)
					continue;

				if (memcmp(&last_dst, resp[1]->ai_addr,
				    sizeof(last_dst)) == 0)
					continue;
				last_dst = *(struct sockaddr *)resp[1]->ai_addr;

				dests++;

				if (kill.af == AF_INET)
					kill.dst.addr.v.a.addr.v4 =
					    ((struct sockaddr_in *)resp[1]->
					    ai_addr)->sin_addr;
				else if (kill.af == AF_INET6)
					kill.dst.addr.v.a.addr.v6 =
					    ((struct sockaddr_in6 *)resp[1]->
					    ai_addr)->sin6_addr;
				else
					errx(1, "Unknown address family %d",
					    kill.af);

				if ((ret = pfctl_kill_states_h(pfh, &kill, &newkilled)) != 0)
					errc(1, ret, "DIOCKILLSTATES");
				killed += newkilled;
			}
			freeaddrinfo(res[1]);
		} else {
			if ((ret = pfctl_kill_states_h(pfh, &kill, &newkilled)) != 0)
				errc(1, ret, "DIOCKILLSTATES");
			killed += newkilled;
		}
	}

	freeaddrinfo(res[0]);

	if ((opts & PF_OPT_QUIET) == 0)
		fprintf(stderr, "killed %d states from %d sources and %d "
		    "destinations\n", killed, sources, dests);
	return (0);
}

int
pfctl_gateway_kill_states(int dev, const char *iface, int opts)
{
	struct pfctl_kill kill;
	struct addrinfo *res, *resp;
	struct sockaddr last_src;
	unsigned int newkilled;
	int killed = 0;
	int ret_ga;

	if (state_killers != 2 || (strlen(state_kill[1]) == 0)) {
		warnx("no gateway specified");
		usage();
	}

	memset(&kill, 0, sizeof(kill));
	memset(&kill.rt_addr.addr.v.a.mask, 0xff,
	    sizeof(kill.rt_addr.addr.v.a.mask));
	memset(&last_src, 0xff, sizeof(last_src));
	if (iface != NULL && strlcpy(kill.ifname, iface,
	    sizeof(kill.ifname)) >= sizeof(kill.ifname))
		errx(1, "invalid interface: %s", iface);

	if (opts & PF_OPT_KILLMATCH)
		kill.kill_match = true;

	pfctl_addrprefix(state_kill[1], &kill.rt_addr.addr.v.a.mask);

	if ((ret_ga = getaddrinfo(state_kill[1], NULL, NULL, &res))) {
		errx(1, "getaddrinfo: %s", gai_strerror(ret_ga));
		/* NOTREACHED */
	}
	for (resp = res; resp; resp = resp->ai_next) {
		if (resp->ai_addr == NULL)
			continue;
		/* We get lots of duplicates.  Catch the easy ones */
		if (memcmp(&last_src, resp->ai_addr, sizeof(last_src)) == 0)
			continue;
		last_src = *(struct sockaddr *)resp->ai_addr;

		kill.af = resp->ai_family;

		if (kill.af == AF_INET)
			kill.rt_addr.addr.v.a.addr.v4 =
			    ((struct sockaddr_in *)resp->ai_addr)->sin_addr;
		else if (kill.af == AF_INET6)
			kill.rt_addr.addr.v.a.addr.v6 =
			    ((struct sockaddr_in6 *)resp->ai_addr)->
			    sin6_addr;
		else
			errx(1, "Unknown address family %d", kill.af);

		if (pfctl_kill_states_h(pfh, &kill, &newkilled))
			err(1, "DIOCKILLSTATES");
		killed += newkilled;
	}

	freeaddrinfo(res);

	if ((opts & PF_OPT_QUIET) == 0)
		fprintf(stderr, "killed %d states\n", killed);
	return (0);
}

int
pfctl_label_kill_states(int dev, const char *iface, int opts)
{
	struct pfctl_kill kill;
	unsigned int killed;
	int ret;

	if (state_killers != 2 || (strlen(state_kill[1]) == 0)) {
		warnx("no label specified");
		usage();
	}
	memset(&kill, 0, sizeof(kill));
	if (iface != NULL && strlcpy(kill.ifname, iface,
	    sizeof(kill.ifname)) >= sizeof(kill.ifname))
		errx(1, "invalid interface: %s", iface);

	if (opts & PF_OPT_KILLMATCH)
		kill.kill_match = true;

	if (strlcpy(kill.label, state_kill[1], sizeof(kill.label)) >=
	    sizeof(kill.label))
		errx(1, "label too long: %s", state_kill[1]);

	if ((ret = pfctl_kill_states_h(pfh, &kill, &killed)) != 0)
		errc(1, ret, "DIOCKILLSTATES");

	if ((opts & PF_OPT_QUIET) == 0)
		fprintf(stderr, "killed %d states\n", killed);

	return (0);
}

int
pfctl_id_kill_states(int dev, const char *iface, int opts)
{
	struct pfctl_kill kill;
	unsigned int killed;
	int ret;
	
	if (state_killers != 2 || (strlen(state_kill[1]) == 0)) {
		warnx("no id specified");
		usage();
	}

	memset(&kill, 0, sizeof(kill));

	if (opts & PF_OPT_KILLMATCH)
		kill.kill_match = true;

	if ((sscanf(state_kill[1], "%jx/%x",
	    &kill.cmp.id, &kill.cmp.creatorid)) == 2) {
	}
	else if ((sscanf(state_kill[1], "%jx", &kill.cmp.id)) == 1) {
		kill.cmp.creatorid = 0;
	} else {
		warnx("wrong id format specified");
		usage();
	}
	if (kill.cmp.id == 0) {
		warnx("cannot kill id 0");
		usage();
	}

	if ((ret = pfctl_kill_states_h(pfh, &kill, &killed)) != 0)
		errc(1, ret, "DIOCKILLSTATES");

	if ((opts & PF_OPT_QUIET) == 0)
		fprintf(stderr, "killed %d states\n", killed);

	return (0);
}

int
pfctl_get_pool(int dev, struct pfctl_pool *pool, u_int32_t nr,
    u_int32_t ticket, int r_action, char *anchorname, int which)
{
	struct pfioc_pooladdr pp;
	struct pf_pooladdr *pa;
	u_int32_t pnr, mpnr;
	int ret;

	memset(&pp, 0, sizeof(pp));
	if ((ret = pfctl_get_addrs(pfh, ticket, nr, r_action, anchorname, &mpnr, which)) != 0) {
		warnc(ret, "DIOCGETADDRS");
		return (-1);
	}

	TAILQ_INIT(&pool->list);
	for (pnr = 0; pnr < mpnr; ++pnr) {
		if ((ret = pfctl_get_addr(pfh, ticket, nr, r_action, anchorname, pnr, &pp, which)) != 0) {
			warnc(ret, "DIOCGETADDR");
			return (-1);
		}
		pa = calloc(1, sizeof(struct pf_pooladdr));
		if (pa == NULL)
			err(1, "calloc");
		bcopy(&pp.addr, pa, sizeof(struct pf_pooladdr));
		TAILQ_INSERT_TAIL(&pool->list, pa, entries);
	}

	return (0);
}

void
pfctl_move_pool(struct pfctl_pool *src, struct pfctl_pool *dst)
{
	struct pf_pooladdr *pa;

	while ((pa = TAILQ_FIRST(&src->list)) != NULL) {
		TAILQ_REMOVE(&src->list, pa, entries);
		TAILQ_INSERT_TAIL(&dst->list, pa, entries);
	}
}

void
pfctl_clear_pool(struct pfctl_pool *pool)
{
	struct pf_pooladdr *pa;

	while ((pa = TAILQ_FIRST(&pool->list)) != NULL) {
		TAILQ_REMOVE(&pool->list, pa, entries);
		free(pa);
	}
}

void
pfctl_print_eth_rule_counters(struct pfctl_eth_rule *rule, int opts)
{
	if (opts & PF_OPT_VERBOSE) {
		printf("  [ Evaluations: %-8llu  Packets: %-8llu  "
			    "Bytes: %-10llu]\n",
			    (unsigned long long)rule->evaluations,
			    (unsigned long long)(rule->packets[0] +
			    rule->packets[1]),
			    (unsigned long long)(rule->bytes[0] +
			    rule->bytes[1]));
	}
	if (opts & PF_OPT_VERBOSE2) {
		char timestr[30];

		if (rule->last_active_timestamp != 0) {
			bcopy(ctime(&rule->last_active_timestamp), timestr,
			    sizeof(timestr));
			*strchr(timestr, '\n') = '\0';
		} else {
			snprintf(timestr, sizeof(timestr), "N/A");
		}
		printf("  [ Last Active Time: %s ]\n", timestr);
	}
}

void
pfctl_print_rule_counters(struct pfctl_rule *rule, int opts)
{
	if (opts & PF_OPT_DEBUG) {
		const char *t[PF_SKIP_COUNT] = { "i", "d", "f",
		    "p", "sa", "da", "sp", "dp" };
		int i;

		printf("  [ Skip steps: ");
		for (i = 0; i < PF_SKIP_COUNT; ++i) {
			if (rule->skip[i].nr == rule->nr + 1)
				continue;
			printf("%s=", t[i]);
			if (rule->skip[i].nr == -1)
				printf("end ");
			else
				printf("%u ", rule->skip[i].nr);
		}
		printf("]\n");

		printf("  [ queue: qname=%s qid=%u pqname=%s pqid=%u ]\n",
		    rule->qname, rule->qid, rule->pqname, rule->pqid);
	}
	if (opts & PF_OPT_VERBOSE) {
		printf("  [ Evaluations: %-8llu  Packets: %-8llu  "
			    "Bytes: %-10llu  States: %-6ju]\n",
			    (unsigned long long)rule->evaluations,
			    (unsigned long long)(rule->packets[0] +
			    rule->packets[1]),
			    (unsigned long long)(rule->bytes[0] +
			    rule->bytes[1]), (uintmax_t)rule->states_cur);
		printf("  [ Source Nodes: %-6ju "
			    "Limit: %-6ju "
			    "NAT/RDR: %-6ju "
			    "Route: %-6ju "
			    "]\n",
			    (uintmax_t)rule->src_nodes,
			    (uintmax_t)rule->src_nodes_type[PF_SN_LIMIT],
			    (uintmax_t)rule->src_nodes_type[PF_SN_NAT],
			    (uintmax_t)rule->src_nodes_type[PF_SN_ROUTE]);
		if (!(opts & PF_OPT_DEBUG))
			printf("  [ Inserted: uid %u pid %u "
			    "State Creations: %-6ju]\n",
			    (unsigned)rule->cuid, (unsigned)rule->cpid,
			    (uintmax_t)rule->states_tot);
	}
	if (opts & PF_OPT_VERBOSE2) {
		char timestr[30];
		if (rule->last_active_timestamp != 0) {
			bcopy(ctime(&rule->last_active_timestamp), timestr,
			    sizeof(timestr));
			*strchr(timestr, '\n') = '\0';
		} else {
			snprintf(timestr, sizeof(timestr), "N/A");
		}
		printf("  [ Last Active Time: %s ]\n", timestr);
	}
}

void
pfctl_print_title(char *title)
{
	if (!first_title)
		printf("\n");
	first_title = 0;
	printf("%s\n", title);
}

int
pfctl_show_eth_rules(int dev, char *path, int opts, enum pfctl_show format,
    char *anchorname, int depth, int wildcard)
{
	char anchor_call[MAXPATHLEN];
	struct pfctl_eth_rules_info info;
	struct pfctl_eth_rule rule;
	int brace;
	int dotitle = opts & PF_OPT_SHOWALL;
	int len = strlen(path);
	int ret;
	char *npath, *p;

	/*
	 * Truncate a trailing / and * on an anchorname before searching for
	 * the ruleset, this is syntactic sugar that doesn't actually make it
	 * to the kernel.
	 */
	if ((p = strrchr(anchorname, '/')) != NULL &&
			p[1] == '*' && p[2] == '\0') {
		p[0] = '\0';
	}

	if (anchorname[0] == '/') {
		if ((npath = calloc(1, MAXPATHLEN)) == NULL)
			errx(1, "pfctl_rules: calloc");
		snprintf(npath, MAXPATHLEN, "%s", anchorname);
	} else {
		if (path[0])
			snprintf(&path[len], MAXPATHLEN - len, "/%s", anchorname);
		else
			snprintf(&path[len], MAXPATHLEN - len, "%s", anchorname);
		npath = path;
	}

	/*
	 * If this anchor was called with a wildcard path, go through
	 * the rulesets in the anchor rather than the rules.
	 */
	if (wildcard && (opts & PF_OPT_RECURSE)) {
		struct pfctl_eth_rulesets_info	ri;
		u_int32_t                mnr, nr;

		if ((ret = pfctl_get_eth_rulesets_info(dev, &ri, npath)) != 0) {
			if (ret == EINVAL) {
				fprintf(stderr, "Anchor '%s' "
						"not found.\n", anchorname);
			} else {
				warnc(ret, "DIOCGETETHRULESETS");
				return (-1);
			}
		}
		mnr = ri.nr;

		pfctl_print_eth_rule_counters(&rule, opts);
		for (nr = 0; nr < mnr; ++nr) {
			struct pfctl_eth_ruleset_info	rs;

			if ((ret = pfctl_get_eth_ruleset(dev, npath, nr, &rs)) != 0)
				errc(1, ret, "DIOCGETETHRULESET");
			INDENT(depth, !(opts & PF_OPT_VERBOSE));
			printf("anchor \"%s\" all {\n", rs.name);
			pfctl_show_eth_rules(dev, npath, opts,
					format, rs.name, depth + 1, 0);
			INDENT(depth, !(opts & PF_OPT_VERBOSE));
			printf("}\n");
		}
		path[len] = '\0';
		return (0);
	}

	if ((ret = pfctl_get_eth_rules_info(dev, &info, path)) != 0) {
		warnc(ret, "DIOCGETETHRULES");
		return (-1);
	}
	for (int nr = 0; nr < info.nr; nr++) {
		brace = 0;
		INDENT(depth, !(opts & PF_OPT_VERBOSE));
		if ((ret = pfctl_get_eth_rule(dev, nr, info.ticket, path, &rule,
		    opts & PF_OPT_CLRRULECTRS, anchor_call)) != 0) {
			warnc(ret, "DIOCGETETHRULE");
			return (-1);
		}
		if (anchor_call[0] &&
		   ((((p = strrchr(anchor_call, '_')) != NULL) &&
		   (p == anchor_call ||
		   *(--p) == '/')) || (opts & PF_OPT_RECURSE))) {
			brace++;
			int aclen = strlen(anchor_call);
			if (anchor_call[aclen - 1] == '*')
				anchor_call[aclen - 2] = '\0';
		}
		p = &anchor_call[0];
		if (dotitle) {
			pfctl_print_title("ETH RULES:");
			dotitle = 0;
		}
		print_eth_rule(&rule, anchor_call,
		    opts & (PF_OPT_VERBOSE2 | PF_OPT_DEBUG));
		if (brace)
			printf(" {\n");
		else
			printf("\n");
		pfctl_print_eth_rule_counters(&rule, opts);
		if (brace) {
			pfctl_show_eth_rules(dev, path, opts, format,
			    p, depth + 1, rule.anchor_wildcard);
			INDENT(depth, !(opts & PF_OPT_VERBOSE));
			printf("}\n");
		}
	}

	path[len] = '\0';
	return (0);
}

int
pfctl_show_rules(int dev, char *path, int opts, enum pfctl_show format,
    char *anchorname, int depth, int wildcard)
{
	struct pfctl_rules_info ri;
	struct pfctl_rule rule;
	char anchor_call[MAXPATHLEN];
	u_int32_t nr, header = 0;
	int rule_numbers = opts & (PF_OPT_VERBOSE2 | PF_OPT_DEBUG);
	int numeric = opts & PF_OPT_NUMERIC;
	int len = strlen(path), ret = 0;
	char *npath, *p;

	/*
	 * Truncate a trailing / and * on an anchorname before searching for
	 * the ruleset, this is syntactic sugar that doesn't actually make it
	 * to the kernel.
	 */
	if ((p = strrchr(anchorname, '/')) != NULL &&
	    p[1] == '*' && p[2] == '\0') {
		p[0] = '\0';
	}

	if (anchorname[0] == '/') {
		if ((npath = calloc(1, MAXPATHLEN)) == NULL)
			errx(1, "pfctl_rules: calloc");
		strlcpy(npath, anchorname, MAXPATHLEN);
	} else {
		if (path[0])
			snprintf(&path[len], MAXPATHLEN - len, "/%s", anchorname);
		else
			snprintf(&path[len], MAXPATHLEN - len, "%s", anchorname);
		npath = path;
	}

	/*
	 * If this anchor was called with a wildcard path, go through
	 * the rulesets in the anchor rather than the rules.
	 */
	if (wildcard && (opts & PF_OPT_RECURSE)) {
		struct pfioc_ruleset     prs;
		u_int32_t                mnr, nr;

		memset(&prs, 0, sizeof(prs));
		if ((ret = pfctl_get_rulesets(pfh, npath, &mnr)) != 0) {
			if (ret == EINVAL)
				fprintf(stderr, "Anchor '%s' "
				    "not found.\n", anchorname);
			else
				errc(1, ret, "DIOCGETRULESETS");
		}

		pfctl_print_rule_counters(&rule, opts);
		for (nr = 0; nr < mnr; ++nr) {
			if ((ret = pfctl_get_ruleset(pfh, npath, nr, &prs)) != 0)
				errc(1, ret, "DIOCGETRULESET");
			INDENT(depth, !(opts & PF_OPT_VERBOSE));
			printf("anchor \"%s\" all {\n", prs.name);
			pfctl_show_rules(dev, npath, opts,
			    format, prs.name, depth + 1, 0);
			INDENT(depth, !(opts & PF_OPT_VERBOSE));
			printf("}\n");
		}
		path[len] = '\0';
		return (0);
	}

	if (opts & PF_OPT_SHOWALL) {
		ret = pfctl_get_rules_info_h(pfh, &ri, PF_PASS, path);
		if (ret != 0) {
			warnc(ret, "DIOCGETRULES");
			goto error;
		}
		header++;
	}
	ret = pfctl_get_rules_info_h(pfh, &ri, PF_SCRUB, path);
	if (ret != 0) {
		warnc(ret, "DIOCGETRULES");
		goto error;
	}
	if (opts & PF_OPT_SHOWALL) {
		if (format == PFCTL_SHOW_RULES && (ri.nr > 0 || header))
			pfctl_print_title("FILTER RULES:");
		else if (format == PFCTL_SHOW_LABELS && labels)
			pfctl_print_title("LABEL COUNTERS:");
	}

	for (nr = 0; nr < ri.nr; ++nr) {
		if ((ret = pfctl_get_clear_rule_h(pfh, nr, ri.ticket, path, PF_SCRUB,
		    &rule, anchor_call, opts & PF_OPT_CLRRULECTRS)) != 0) {
			warnc(ret, "DIOCGETRULENV");
			goto error;
		}

		if (pfctl_get_pool(dev, &rule.rdr,
		    nr, ri.ticket, PF_SCRUB, path, PF_RDR) != 0)
			goto error;

		if (pfctl_get_pool(dev, &rule.nat,
		    nr, ri.ticket, PF_SCRUB, path, PF_NAT) != 0)
			goto error;

		if (pfctl_get_pool(dev, &rule.route,
		    nr, ri.ticket, PF_SCRUB, path, PF_RT) != 0)
			goto error;

		switch (format) {
		case PFCTL_SHOW_LABELS:
			break;
		case PFCTL_SHOW_RULES:
			if (rule.label[0][0] && (opts & PF_OPT_SHOWALL))
				labels = 1;
			print_rule(&rule, anchor_call, rule_numbers, numeric);
			printf("\n");
			pfctl_print_rule_counters(&rule, opts);
			break;
		case PFCTL_SHOW_NOTHING:
			break;
		}
		pfctl_clear_pool(&rule.rdr);
		pfctl_clear_pool(&rule.nat);
		pfctl_clear_pool(&rule.route);
	}
	ret = pfctl_get_rules_info_h(pfh, &ri, PF_PASS, path);
	if (ret != 0) {
		warnc(ret, "DIOCGETRULES");
		goto error;
	}
	for (nr = 0; nr < ri.nr; ++nr) {
		if ((ret = pfctl_get_clear_rule_h(pfh, nr, ri.ticket, path, PF_PASS,
		    &rule, anchor_call, opts & PF_OPT_CLRRULECTRS)) != 0) {
			warnc(ret, "DIOCGETRULE");
			goto error;
		}

		if (pfctl_get_pool(dev, &rule.rdr,
		    nr, ri.ticket, PF_PASS, path, PF_RDR) != 0)
			goto error;

		if (pfctl_get_pool(dev, &rule.nat,
		    nr, ri.ticket, PF_PASS, path, PF_NAT) != 0)
			goto error;

		if (pfctl_get_pool(dev, &rule.route,
		    nr, ri.ticket, PF_PASS, path, PF_RT) != 0)
			goto error;

		switch (format) {
		case PFCTL_SHOW_LABELS: {
			bool show = false;
			int i = 0;

			while (rule.label[i][0]) {
				printf("%s ", rule.label[i++]);
				show = true;
			}

			if (show) {
				printf("%llu %llu %llu %llu"
				    " %llu %llu %llu %ju\n",
				    (unsigned long long)rule.evaluations,
				    (unsigned long long)(rule.packets[0] +
				    rule.packets[1]),
				    (unsigned long long)(rule.bytes[0] +
				    rule.bytes[1]),
				    (unsigned long long)rule.packets[0],
				    (unsigned long long)rule.bytes[0],
				    (unsigned long long)rule.packets[1],
				    (unsigned long long)rule.bytes[1],
				    (uintmax_t)rule.states_tot);
			}

			if (anchor_call[0] &&
			    (((p = strrchr(anchor_call, '/')) ?
			      p[1] == '_' : anchor_call[0] == '_') ||
			     opts & PF_OPT_RECURSE)) {
				pfctl_show_rules(dev, npath, opts, format,
				    anchor_call, depth, rule.anchor_wildcard);
			}
			break;
		}
		case PFCTL_SHOW_RULES:
			if (rule.label[0][0] && (opts & PF_OPT_SHOWALL))
				labels = 1;
			INDENT(depth, !(opts & PF_OPT_VERBOSE));
			print_rule(&rule, anchor_call, rule_numbers, numeric);

			/*
			 * If this is a 'unnamed' brace notation
			 * anchor, OR the user has explicitly requested
			 * recursion, print it recursively.
			 */
			if (anchor_call[0] &&
			    (((p = strrchr(anchor_call, '/')) ?
			      p[1] == '_' : anchor_call[0] == '_') ||
			     opts & PF_OPT_RECURSE)) {
				printf(" {\n");
				pfctl_print_rule_counters(&rule, opts);
				pfctl_show_rules(dev, npath, opts, format,
				    anchor_call, depth + 1,
				    rule.anchor_wildcard);
				INDENT(depth, !(opts & PF_OPT_VERBOSE));
				printf("}\n");
			} else {
				printf("\n");
				pfctl_print_rule_counters(&rule, opts);
			}
			break;
		case PFCTL_SHOW_NOTHING:
			break;
		}
		pfctl_clear_pool(&rule.rdr);
		pfctl_clear_pool(&rule.nat);
	}

 error:
	path[len] = '\0';
	return (ret);
}

int
pfctl_show_nat(int dev, char *path, int opts, char *anchorname, int depth,
    int wildcard)
{
	struct pfctl_rules_info ri;
	struct pfctl_rule rule;
	char anchor_call[MAXPATHLEN];
	u_int32_t nr;
	static int nattype[3] = { PF_NAT, PF_RDR, PF_BINAT };
	int i, dotitle = opts & PF_OPT_SHOWALL;
	int ret;
	int len = strlen(path);
	char *npath, *p;

	/*
	 * Truncate a trailing / and * on an anchorname before searching for
	 * the ruleset, this is syntactic sugar that doesn't actually make it
	 * to the kernel.
	 */
	if ((p = strrchr(anchorname, '/')) != NULL &&
	    p[1] == '*' && p[2] == '\0') {
		p[0] = '\0';
	}

	if (anchorname[0] == '/') {
		if ((npath = calloc(1, MAXPATHLEN)) == NULL)
			errx(1, "pfctl_rules: calloc");
		snprintf(npath, MAXPATHLEN, "%s", anchorname);
	} else {
		if (path[0])
			snprintf(&path[len], MAXPATHLEN - len, "/%s", anchorname);
		else
			snprintf(&path[len], MAXPATHLEN - len, "%s", anchorname);
		npath = path;
	}

	/*
	 * If this anchor was called with a wildcard path, go through
	 * the rulesets in the anchor rather than the rules.
	 */
	if (wildcard && (opts & PF_OPT_RECURSE)) {
		struct pfioc_ruleset     prs;
		u_int32_t                mnr, nr;
		memset(&prs, 0, sizeof(prs));
		if ((ret = pfctl_get_rulesets(pfh, npath, &mnr)) != 0) {
			if (ret == EINVAL)
				fprintf(stderr, "NAT anchor '%s' "
				    "not found.\n", anchorname);
			else
				errc(1, ret, "DIOCGETRULESETS");
		}

		pfctl_print_rule_counters(&rule, opts);
		for (nr = 0; nr < mnr; ++nr) {
			if ((ret = pfctl_get_ruleset(pfh, npath, nr, &prs)) != 0)
				errc(1, ret, "DIOCGETRULESET");
			INDENT(depth, !(opts & PF_OPT_VERBOSE));
			printf("nat-anchor \"%s\" all {\n", prs.name);
			pfctl_show_nat(dev, npath, opts,
			    prs.name, depth + 1, 0);
			INDENT(depth, !(opts & PF_OPT_VERBOSE));
			printf("}\n");
		}
		path[len] = '\0';
		return (0);
	}

	for (i = 0; i < 3; i++) {
		ret = pfctl_get_rules_info_h(pfh, &ri, nattype[i], path);
		if (ret != 0) {
			warnc(ret, "DIOCGETRULES");
			return (-1);
		}
		for (nr = 0; nr < ri.nr; ++nr) {
			INDENT(depth, !(opts & PF_OPT_VERBOSE));

			if ((ret = pfctl_get_rule_h(pfh, nr, ri.ticket, path,
			    nattype[i], &rule, anchor_call)) != 0) {
				warnc(ret, "DIOCGETRULE");
				return (-1);
			}
			if (pfctl_get_pool(dev, &rule.rdr, nr,
			    ri.ticket, nattype[i], path, PF_RDR) != 0)
				return (-1);
			if (pfctl_get_pool(dev, &rule.nat, nr,
			    ri.ticket, nattype[i], path, PF_NAT) != 0)
				return (-1);
			if (pfctl_get_pool(dev, &rule.route, nr,
			    ri.ticket, nattype[i], path, PF_RT) != 0)
				return (-1);

			if (dotitle) {
				pfctl_print_title("TRANSLATION RULES:");
				dotitle = 0;
			}
			print_rule(&rule, anchor_call,
			    opts & PF_OPT_VERBOSE2, opts & PF_OPT_NUMERIC);
			if (anchor_call[0] &&
			    (((p = strrchr(anchor_call, '/')) ?
			      p[1] == '_' : anchor_call[0] == '_') ||
			     opts & PF_OPT_RECURSE)) {
				printf(" {\n");
				pfctl_print_rule_counters(&rule, opts);
				pfctl_show_nat(dev, npath, opts, anchor_call,
				    depth + 1, rule.anchor_wildcard);
				INDENT(depth, !(opts & PF_OPT_VERBOSE));
				printf("}\n");
			} else {
				printf("\n");
				pfctl_print_rule_counters(&rule, opts);
			}
		}
	}
	return (0);
}

static int
pfctl_print_src_node(struct pfctl_src_node *sn, void *arg)
{
	int *opts = (int *)arg;

	if (*opts & PF_OPT_SHOWALL) {
		pfctl_print_title("SOURCE TRACKING NODES:");
		*opts &= ~PF_OPT_SHOWALL;
	}

	print_src_node(sn, *opts);

	return (0);
}

int
pfctl_show_src_nodes(int dev, int opts)
{
	int error;

	error = pfctl_get_srcnodes(pfh, pfctl_print_src_node, &opts);

	return (error);
}

struct pfctl_show_state_arg {
	int opts;
	int dotitle;
	const char *iface;
};

static int
pfctl_show_state(struct pfctl_state *s, void *arg)
{
	struct pfctl_show_state_arg *a = (struct pfctl_show_state_arg *)arg;

	if (a->dotitle) {
		pfctl_print_title("STATES:");
		a->dotitle = 0;
	}
	print_state(s, a->opts);

	return (0);
}

int
pfctl_show_states(int dev, const char *iface, int opts)
{
	struct pfctl_show_state_arg arg;
	struct pfctl_state_filter filter = {};

	if (iface != NULL)
		strncpy(filter.ifname, iface, IFNAMSIZ);

	arg.opts = opts;
	arg.dotitle = opts & PF_OPT_SHOWALL;
	arg.iface = iface;

	if (pfctl_get_filtered_states_iter(&filter, pfctl_show_state, &arg))
		return (-1);

	return (0);
}

int
pfctl_show_status(int dev, int opts)
{
	struct pfctl_status	*status;
	struct pfctl_syncookies	cookies;
	int ret;

	if ((status = pfctl_get_status_h(pfh)) == NULL) {
		warn("DIOCGETSTATUS");
		return (-1);
	}
	if ((ret = pfctl_get_syncookies(dev, &cookies)) != 0) {
		pfctl_free_status(status);
		warnc(ret, "DIOCGETSYNCOOKIES");
		return (-1);
	}
	if (opts & PF_OPT_SHOWALL)
		pfctl_print_title("INFO:");
	print_status(status, &cookies, opts);
	pfctl_free_status(status);
	return (0);
}

int
pfctl_show_running(int dev)
{
	struct pfctl_status *status;
	int running;

	if ((status = pfctl_get_status_h(pfh)) == NULL) {
		warn("DIOCGETSTATUS");
		return (-1);
	}

	running = status->running;

	print_running(status);
	pfctl_free_status(status);
	return (!running);
}

int
pfctl_show_timeouts(int dev, int opts)
{
	uint32_t seconds;
	int i;
	int ret;

	if (opts & PF_OPT_SHOWALL)
		pfctl_print_title("TIMEOUTS:");
	for (i = 0; pf_timeouts[i].name; i++) {
		if ((ret = pfctl_get_timeout(pfh, pf_timeouts[i].timeout, &seconds)) != 0)
			errc(1, ret, "DIOCGETTIMEOUT");
		printf("%-20s %10d", pf_timeouts[i].name, seconds);
		if (pf_timeouts[i].timeout >= PFTM_ADAPTIVE_START &&
		    pf_timeouts[i].timeout <= PFTM_ADAPTIVE_END)
			printf(" states");
		else
			printf("s");
		printf("\n");
	}
	return (0);

}

int
pfctl_show_limits(int dev, int opts)
{
	unsigned int limit;
	int i;
	int ret;

	if (opts & PF_OPT_SHOWALL)
		pfctl_print_title("LIMITS:");
	for (i = 0; pf_limits[i].name; i++) {
		if ((ret = pfctl_get_limit(pfh, pf_limits[i].index, &limit)) != 0)
			errc(1, ret, "DIOCGETLIMIT");
		printf("%-13s ", pf_limits[i].name);
		if (limit == UINT_MAX)
			printf("unlimited\n");
		else
			printf("hard limit %8u\n", limit);
	}
	return (0);
}

void
pfctl_show_creators(int opts)
{
	int ret;
	uint32_t creators[16];
	size_t count = nitems(creators);

	ret = pfctl_get_creatorids(pfh, creators, &count);
	if (ret != 0)
		errx(ret, "Failed to retrieve creators");

	printf("Creator IDs:\n");
	for (size_t i = 0; i < count; i++)
		printf("%08x\n", creators[i]);
}

/* callbacks for rule/nat/rdr/addr */
int
pfctl_add_pool(struct pfctl *pf, struct pfctl_pool *p, sa_family_t af, int which)
{
	struct pf_pooladdr *pa;
	int ret;

	pf->paddr.af = af;
	TAILQ_FOREACH(pa, &p->list, entries) {
		memcpy(&pf->paddr.addr, pa, sizeof(struct pf_pooladdr));
		if ((pf->opts & PF_OPT_NOACTION) == 0) {
			if ((ret = pfctl_add_addr(pf->h, &pf->paddr, which)) != 0)
				errc(1, ret, "DIOCADDADDR");
		}
	}
	return (0);
}

int
pfctl_append_rule(struct pfctl *pf, struct pfctl_rule *r,
    const char *anchor_call)
{
	u_int8_t		rs_num;
	struct pfctl_rule	*rule;
	struct pfctl_ruleset	*rs;
	char 			*p;

	rs_num = pf_get_ruleset_number(r->action);
	if (rs_num == PF_RULESET_MAX)
		errx(1, "Invalid rule type %d", r->action);

	rs = &pf->anchor->ruleset;

	if (anchor_call[0] && r->anchor == NULL) {
		/* 
		 * Don't make non-brace anchors part of the main anchor pool.
		 */
		if ((r->anchor = calloc(1, sizeof(*r->anchor))) == NULL)
			err(1, "pfctl_append_rule: calloc");
		
		pf_init_ruleset(&r->anchor->ruleset);
		r->anchor->ruleset.anchor = r->anchor;
		if (strlcpy(r->anchor->path, anchor_call,
		    sizeof(rule->anchor->path)) >= sizeof(rule->anchor->path))
			errx(1, "pfctl_append_rule: strlcpy");
		if ((p = strrchr(anchor_call, '/')) != NULL) {
			if (!strlen(p))
				err(1, "pfctl_append_rule: bad anchor name %s",
				    anchor_call);
		} else
			p = (char *)anchor_call;
		if (strlcpy(r->anchor->name, p,
		    sizeof(rule->anchor->name)) >= sizeof(rule->anchor->name))
			errx(1, "pfctl_append_rule: strlcpy");
	}

	if ((rule = calloc(1, sizeof(*rule))) == NULL)
		err(1, "calloc");
	bcopy(r, rule, sizeof(*rule));
	TAILQ_INIT(&rule->rdr.list);
	pfctl_move_pool(&r->rdr, &rule->rdr);
	TAILQ_INIT(&rule->nat.list);
	pfctl_move_pool(&r->nat, &rule->nat);
	TAILQ_INIT(&rule->route.list);
	pfctl_move_pool(&r->route, &rule->route);

	TAILQ_INSERT_TAIL(rs->rules[rs_num].active.ptr, rule, entries);
	return (0);
}

int
pfctl_append_eth_rule(struct pfctl *pf, struct pfctl_eth_rule *r,
    const char *anchor_call)
{
	struct pfctl_eth_rule		*rule;
	struct pfctl_eth_ruleset	*rs;
	char 				*p;

	rs = &pf->eanchor->ruleset;

	if (anchor_call[0] && r->anchor == NULL) {
		/*
		 * Don't make non-brace anchors part of the main anchor pool.
		 */
		if ((r->anchor = calloc(1, sizeof(*r->anchor))) == NULL)
			err(1, "pfctl_append_rule: calloc");

		pf_init_eth_ruleset(&r->anchor->ruleset);
		r->anchor->ruleset.anchor = r->anchor;
		if (strlcpy(r->anchor->path, anchor_call,
		    sizeof(rule->anchor->path)) >= sizeof(rule->anchor->path))
			errx(1, "pfctl_append_rule: strlcpy");
		if ((p = strrchr(anchor_call, '/')) != NULL) {
			if (!strlen(p))
				err(1, "pfctl_append_eth_rule: bad anchor name %s",
				    anchor_call);
		} else
			p = (char *)anchor_call;
		if (strlcpy(r->anchor->name, p,
		    sizeof(rule->anchor->name)) >= sizeof(rule->anchor->name))
			errx(1, "pfctl_append_eth_rule: strlcpy");
	}

	if ((rule = calloc(1, sizeof(*rule))) == NULL)
		err(1, "calloc");
	bcopy(r, rule, sizeof(*rule));

	TAILQ_INSERT_TAIL(&rs->rules, rule, entries);
	return (0);
}

int
pfctl_eth_ruleset_trans(struct pfctl *pf, char *path,
    struct pfctl_eth_anchor *a)
{
	int osize = pf->trans->pfrb_size;

	if ((pf->loadopt & PFCTL_FLAG_ETH) != 0) {
		if (pfctl_add_trans(pf->trans, PF_RULESET_ETH, path))
			return (1);
	}
	if (pfctl_trans(pf->dev, pf->trans, DIOCXBEGIN, osize))
		return (5);

	return (0);
}

int
pfctl_ruleset_trans(struct pfctl *pf, char *path, struct pfctl_anchor *a, bool do_eth)
{
	int osize = pf->trans->pfrb_size;

	if ((pf->loadopt & PFCTL_FLAG_ETH) != 0 && do_eth) {
		if (pfctl_add_trans(pf->trans, PF_RULESET_ETH, path))
			return (1);
	}
	if ((pf->loadopt & PFCTL_FLAG_NAT) != 0) {
		if (pfctl_add_trans(pf->trans, PF_RULESET_NAT, path) ||
		    pfctl_add_trans(pf->trans, PF_RULESET_BINAT, path) ||
		    pfctl_add_trans(pf->trans, PF_RULESET_RDR, path))
			return (1);
	}
	if (a == pf->astack[0] && ((altqsupport &&
	    (pf->loadopt & PFCTL_FLAG_ALTQ) != 0))) {
		if (pfctl_add_trans(pf->trans, PF_RULESET_ALTQ, path))
			return (2);
	}
	if ((pf->loadopt & PFCTL_FLAG_FILTER) != 0) {
		if (pfctl_add_trans(pf->trans, PF_RULESET_SCRUB, path) ||
		    pfctl_add_trans(pf->trans, PF_RULESET_FILTER, path))
			return (3);
	}
	if (pf->loadopt & PFCTL_FLAG_TABLE)
		if (pfctl_add_trans(pf->trans, PF_RULESET_TABLE, path))
			return (4);
	if (pfctl_trans(pf->dev, pf->trans, DIOCXBEGIN, osize))
		return (5);

	return (0);
}

int
pfctl_load_eth_ruleset(struct pfctl *pf, char *path,
    struct pfctl_eth_ruleset *rs, int depth)
{
	struct pfctl_eth_rule	*r;
	int	error, len = strlen(path);
	int	brace = 0;

	pf->eanchor = rs->anchor;
	if (path[0])
		snprintf(&path[len], MAXPATHLEN - len, "/%s", pf->eanchor->name);
	else
		snprintf(&path[len], MAXPATHLEN - len, "%s", pf->eanchor->name);

	if (depth) {
		if (TAILQ_FIRST(&rs->rules) != NULL) {
			brace++;
			if (pf->opts & PF_OPT_VERBOSE)
				printf(" {\n");
			if ((pf->opts & PF_OPT_NOACTION) == 0 &&
			    (error = pfctl_eth_ruleset_trans(pf,
			    path, rs->anchor))) {
				printf("pfctl_load_eth_rulesets: "
				    "pfctl_eth_ruleset_trans %d\n", error);
				goto error;
			}
		} else if (pf->opts & PF_OPT_VERBOSE)
			printf("\n");
	}

	while ((r = TAILQ_FIRST(&rs->rules)) != NULL) {
		TAILQ_REMOVE(&rs->rules, r, entries);

		error = pfctl_load_eth_rule(pf, path, r, depth);
		if (error)
			return (error);

		if (r->anchor) {
			if ((error = pfctl_load_eth_ruleset(pf, path,
			    &r->anchor->ruleset, depth + 1)))
				return (error);
		} else if (pf->opts & PF_OPT_VERBOSE)
			printf("\n");
		free(r);
	}
	if (brace && pf->opts & PF_OPT_VERBOSE) {
		INDENT(depth - 1, (pf->opts & PF_OPT_VERBOSE));
		printf("}\n");
	}
	path[len] = '\0';

	return (0);
error:
	path[len] = '\0';
	return (error);
}

int
pfctl_load_eth_rule(struct pfctl *pf, char *path, struct pfctl_eth_rule *r,
    int depth)
{
	char			*name;
	char			anchor[PF_ANCHOR_NAME_SIZE];
	int			len = strlen(path);
	int			ret;

	if (strlcpy(anchor, path, sizeof(anchor)) >= sizeof(anchor))
		errx(1, "pfctl_load_eth_rule: strlcpy");

	if (r->anchor) {
		if (r->anchor->match) {
			if (path[0])
				snprintf(&path[len], MAXPATHLEN - len,
				    "/%s", r->anchor->name);
			else
				snprintf(&path[len], MAXPATHLEN - len,
				    "%s", r->anchor->name);
			name = r->anchor->name;
		} else
			name = r->anchor->path;
	} else
		name = "";

	if ((pf->opts & PF_OPT_NOACTION) == 0)
		if ((ret = pfctl_add_eth_rule(pf->dev, r, anchor, name,
		    pf->eth_ticket)) != 0)
			errc(1, ret, "DIOCADDETHRULENV");

	if (pf->opts & PF_OPT_VERBOSE) {
		INDENT(depth, !(pf->opts & PF_OPT_VERBOSE2));
		print_eth_rule(r, r->anchor ? r->anchor->name : "",
		    pf->opts & (PF_OPT_VERBOSE2 | PF_OPT_DEBUG));
	}

	path[len] = '\0';

	return (0);
}

int
pfctl_load_ruleset(struct pfctl *pf, char *path, struct pfctl_ruleset *rs,
    int rs_num, int depth)
{
	struct pfctl_rule *r;
	int		error, len = strlen(path);
	int		brace = 0;

	pf->anchor = rs->anchor;

	if (path[0])
		snprintf(&path[len], MAXPATHLEN - len, "/%s", pf->anchor->name);
	else
		snprintf(&path[len], MAXPATHLEN - len, "%s", pf->anchor->name);

	if (depth) {
		if (TAILQ_FIRST(rs->rules[rs_num].active.ptr) != NULL) {
			brace++;
			if (pf->opts & PF_OPT_VERBOSE)
				printf(" {\n");
			if ((pf->opts & PF_OPT_NOACTION) == 0 &&
			    (error = pfctl_ruleset_trans(pf,
			    path, rs->anchor, false))) {
				printf("pfctl_load_rulesets: "
				    "pfctl_ruleset_trans %d\n", error);
				goto error;
			}
		} else if (pf->opts & PF_OPT_VERBOSE)
			printf("\n");

	}

	if (pf->optimize && rs_num == PF_RULESET_FILTER)
		pfctl_optimize_ruleset(pf, rs);

	while ((r = TAILQ_FIRST(rs->rules[rs_num].active.ptr)) != NULL) {
		TAILQ_REMOVE(rs->rules[rs_num].active.ptr, r, entries);

		for (int i = 0; i < PF_RULE_MAX_LABEL_COUNT; i++)
			expand_label(r->label[i], PF_RULE_LABEL_SIZE, r);
		expand_label(r->tagname, PF_TAG_NAME_SIZE, r);
		expand_label(r->match_tagname, PF_TAG_NAME_SIZE, r);

		if ((error = pfctl_load_rule(pf, path, r, depth)))
			goto error;
		if (r->anchor) {
			if ((error = pfctl_load_ruleset(pf, path,
			    &r->anchor->ruleset, rs_num, depth + 1)))
				goto error;
		} else if (pf->opts & PF_OPT_VERBOSE)
			printf("\n");
		free(r);
	}
	if (brace && pf->opts & PF_OPT_VERBOSE) {
		INDENT(depth - 1, (pf->opts & PF_OPT_VERBOSE));
		printf("}\n");
	}
	path[len] = '\0';
	return (0);

 error:
	path[len] = '\0';
	return (error);

}

int
pfctl_load_rule(struct pfctl *pf, char *path, struct pfctl_rule *r, int depth)
{
	u_int8_t		rs_num = pf_get_ruleset_number(r->action);
	char			*name;
	u_int32_t		ticket;
	char			anchor[PF_ANCHOR_NAME_SIZE];
	int			len = strlen(path);
	int			error;
	bool			was_present;

	/* set up anchor before adding to path for anchor_call */
	if ((pf->opts & PF_OPT_NOACTION) == 0)
		ticket = pfctl_get_ticket(pf->trans, rs_num, path);
	if (strlcpy(anchor, path, sizeof(anchor)) >= sizeof(anchor))
		errx(1, "pfctl_load_rule: strlcpy");

	if (r->anchor) {
		if (r->anchor->match) {
			if (path[0])
				snprintf(&path[len], MAXPATHLEN - len,
				    "/%s", r->anchor->name);
			else
				snprintf(&path[len], MAXPATHLEN - len,
				    "%s", r->anchor->name);
			name = r->anchor->name;
		} else
			name = r->anchor->path;
	} else
		name = "";

	was_present = false;
	if ((pf->opts & PF_OPT_NOACTION) == 0) {
		if ((pf->opts & PF_OPT_NOACTION) == 0) {
			if ((error = pfctl_begin_addrs(pf->h,
			    &pf->paddr.ticket)) != 0)
				errc(1, error, "DIOCBEGINADDRS");
		}

		if (pfctl_add_pool(pf, &r->rdr, r->af, PF_RDR))
			return (1);
		if (pfctl_add_pool(pf, &r->nat, r->naf ? r->naf : r->af, PF_NAT))
			return (1);
		if (pfctl_add_pool(pf, &r->route, r->af, PF_RT))
			return (1);
		error = pfctl_add_rule_h(pf->h, r, anchor, name, ticket,
		    pf->paddr.ticket);
		switch (error) {
		case 0:
			/* things worked, do nothing */
			break;
		case EEXIST:
			/* an identical rule is already present */
			was_present = true;
			break;
		default:
			errc(1, error, "DIOCADDRULE");
		}
	}

	if (pf->opts & PF_OPT_VERBOSE) {
		INDENT(depth, !(pf->opts & PF_OPT_VERBOSE2));
		print_rule(r, name,
		    pf->opts & PF_OPT_VERBOSE2,
		    pf->opts & PF_OPT_NUMERIC);
		if (was_present)
			printf(" -- rule was already present");
	}
	path[len] = '\0';
	pfctl_clear_pool(&r->rdr);
	pfctl_clear_pool(&r->nat);
	return (0);
}

int
pfctl_add_altq(struct pfctl *pf, struct pf_altq *a)
{
	if (altqsupport &&
	    (loadopt & PFCTL_FLAG_ALTQ) != 0) {
		memcpy(&pf->paltq->altq, a, sizeof(struct pf_altq));
		if ((pf->opts & PF_OPT_NOACTION) == 0) {
			if (ioctl(pf->dev, DIOCADDALTQ, pf->paltq)) {
				if (errno == ENXIO)
					errx(1, "qtype not configured");
				else if (errno == ENODEV)
					errx(1, "%s: driver does not support "
					    "altq", a->ifname);
				else
					err(1, "DIOCADDALTQ");
			}
		}
		pfaltq_store(&pf->paltq->altq);
	}
	return (0);
}

int
pfctl_rules(int dev, char *filename, int opts, int optimize,
    char *anchorname, struct pfr_buffer *trans)
{
#define ERR(x) do { warn(x); goto _error; } while(0)
#define ERRX(x) do { warnx(x); goto _error; } while(0)

	struct pfr_buffer	*t, buf;
	struct pfioc_altq	 pa;
	struct pfctl		 pf;
	struct pfctl_ruleset	*rs;
	struct pfctl_eth_ruleset	*ethrs;
	struct pfr_table	 trs;
	char			*path;
	int			 osize;

	RB_INIT(&pf_anchors);
	memset(&pf_main_anchor, 0, sizeof(pf_main_anchor));
	pf_init_ruleset(&pf_main_anchor.ruleset);
	pf_main_anchor.ruleset.anchor = &pf_main_anchor;

	memset(&pf_eth_main_anchor, 0, sizeof(pf_eth_main_anchor));
	pf_init_eth_ruleset(&pf_eth_main_anchor.ruleset);
	pf_eth_main_anchor.ruleset.anchor = &pf_eth_main_anchor;

	if (trans == NULL) {
		bzero(&buf, sizeof(buf));
		buf.pfrb_type = PFRB_TRANS;
		t = &buf;
		osize = 0;
	} else {
		t = trans;
		osize = t->pfrb_size;
	}

	memset(&pa, 0, sizeof(pa));
	pa.version = PFIOC_ALTQ_VERSION;
	memset(&pf, 0, sizeof(pf));
	memset(&trs, 0, sizeof(trs));
	if ((path = calloc(1, MAXPATHLEN)) == NULL)
		ERRX("pfctl_rules: calloc");
	if (strlcpy(trs.pfrt_anchor, anchorname,
	    sizeof(trs.pfrt_anchor)) >= sizeof(trs.pfrt_anchor))
		ERRX("pfctl_rules: strlcpy");
	pf.dev = dev;
	pf.h = pfh;
	pf.opts = opts;
	pf.optimize = optimize;
	pf.loadopt = loadopt;

	/* non-brace anchor, create without resolving the path */
	if ((pf.anchor = calloc(1, sizeof(*pf.anchor))) == NULL)
		ERRX("pfctl_rules: calloc");
	rs = &pf.anchor->ruleset;
	pf_init_ruleset(rs);
	rs->anchor = pf.anchor;
	if (strlcpy(pf.anchor->path, anchorname,
	    sizeof(pf.anchor->path)) >= sizeof(pf.anchor->path))
		errx(1, "pfctl_rules: strlcpy");
	if (strlcpy(pf.anchor->name, anchorname,
	    sizeof(pf.anchor->name)) >= sizeof(pf.anchor->name))
		errx(1, "pfctl_rules: strlcpy");


	pf.astack[0] = pf.anchor;
	pf.asd = 0;
	if (anchorname[0])
		pf.loadopt &= ~PFCTL_FLAG_ALTQ;
	pf.paltq = &pa;
	pf.trans = t;
	pfctl_init_options(&pf);

	/* Set up ethernet anchor */
	if ((pf.eanchor = calloc(1, sizeof(*pf.eanchor))) == NULL)
		ERRX("pfctl_rules: calloc");

	if (strlcpy(pf.eanchor->path, anchorname,
	    sizeof(pf.eanchor->path)) >= sizeof(pf.eanchor->path))
		errx(1, "pfctl_rules: strlcpy");
	if (strlcpy(pf.eanchor->name, anchorname,
	    sizeof(pf.eanchor->name)) >= sizeof(pf.eanchor->name))
		errx(1, "pfctl_rules: strlcpy");

	ethrs = &pf.eanchor->ruleset;
	pf_init_eth_ruleset(ethrs);
	ethrs->anchor = pf.eanchor;
	pf.eastack[0] = pf.eanchor;

	if ((opts & PF_OPT_NOACTION) == 0) {
		/*
		 * XXX For the time being we need to open transactions for
		 * the main ruleset before parsing, because tables are still
		 * loaded at parse time.
		 */
		if (pfctl_ruleset_trans(&pf, anchorname, pf.anchor, true))
			ERRX("pfctl_rules");
		if (pf.loadopt & PFCTL_FLAG_ETH)
			pf.eth_ticket = pfctl_get_ticket(t, PF_RULESET_ETH, anchorname);
		if (altqsupport && (pf.loadopt & PFCTL_FLAG_ALTQ))
			pa.ticket =
			    pfctl_get_ticket(t, PF_RULESET_ALTQ, anchorname);
		if (pf.loadopt & PFCTL_FLAG_TABLE)
			pf.astack[0]->ruleset.tticket =
			    pfctl_get_ticket(t, PF_RULESET_TABLE, anchorname);
	}

	if (parse_config(filename, &pf) < 0) {
		if ((opts & PF_OPT_NOACTION) == 0)
			ERRX("Syntax error in config file: "
			    "pf rules not loaded");
		else
			goto _error;
	}
	if (loadopt & PFCTL_FLAG_OPTION)
		pfctl_adjust_skip_ifaces(&pf);

	if ((pf.loadopt & PFCTL_FLAG_FILTER &&
	    (pfctl_load_ruleset(&pf, path, rs, PF_RULESET_SCRUB, 0))) ||
	    (pf.loadopt & PFCTL_FLAG_ETH &&
	    (pfctl_load_eth_ruleset(&pf, path, ethrs, 0))) ||
	    (pf.loadopt & PFCTL_FLAG_NAT &&
	    (pfctl_load_ruleset(&pf, path, rs, PF_RULESET_NAT, 0) ||
	    pfctl_load_ruleset(&pf, path, rs, PF_RULESET_RDR, 0) ||
	    pfctl_load_ruleset(&pf, path, rs, PF_RULESET_BINAT, 0))) ||
	    (pf.loadopt & PFCTL_FLAG_FILTER &&
	    pfctl_load_ruleset(&pf, path, rs, PF_RULESET_FILTER, 0))) {
		if ((opts & PF_OPT_NOACTION) == 0)
			ERRX("Unable to load rules into kernel");
		else
			goto _error;
	}

	if ((altqsupport && (pf.loadopt & PFCTL_FLAG_ALTQ) != 0))
		if (check_commit_altq(dev, opts) != 0)
			ERRX("errors in altq config");

	/* process "load anchor" directives */
	if (!anchorname[0])
		if (pfctl_load_anchors(dev, &pf, t) == -1)
			ERRX("load anchors");

	if (trans == NULL && (opts & PF_OPT_NOACTION) == 0) {
		if (!anchorname[0])
			if (pfctl_load_options(&pf))
				goto _error;
		if (pfctl_trans(dev, t, DIOCXCOMMIT, osize))
			ERR("DIOCXCOMMIT");
	}
	free(path);
	return (0);

_error:
	if (trans == NULL) {	/* main ruleset */
		if ((opts & PF_OPT_NOACTION) == 0)
			if (pfctl_trans(dev, t, DIOCXROLLBACK, osize))
				err(1, "DIOCXROLLBACK");
		exit(1);
	} else {		/* sub ruleset */
		free(path);
		return (-1);
	}

#undef ERR
#undef ERRX
}

FILE *
pfctl_fopen(const char *name, const char *mode)
{
	struct stat	 st;
	FILE		*fp;

	fp = fopen(name, mode);
	if (fp == NULL)
		return (NULL);
	if (fstat(fileno(fp), &st)) {
		fclose(fp);
		return (NULL);
	}
	if (S_ISDIR(st.st_mode)) {
		fclose(fp);
		errno = EISDIR;
		return (NULL);
	}
	return (fp);
}

void
pfctl_init_options(struct pfctl *pf)
{

	pf->timeout[PFTM_TCP_FIRST_PACKET] = PFTM_TCP_FIRST_PACKET_VAL;
	pf->timeout[PFTM_TCP_OPENING] = PFTM_TCP_OPENING_VAL;
	pf->timeout[PFTM_TCP_ESTABLISHED] = PFTM_TCP_ESTABLISHED_VAL;
	pf->timeout[PFTM_TCP_CLOSING] = PFTM_TCP_CLOSING_VAL;
	pf->timeout[PFTM_TCP_FIN_WAIT] = PFTM_TCP_FIN_WAIT_VAL;
	pf->timeout[PFTM_TCP_CLOSED] = PFTM_TCP_CLOSED_VAL;
	pf->timeout[PFTM_SCTP_FIRST_PACKET] = PFTM_TCP_FIRST_PACKET_VAL;
	pf->timeout[PFTM_SCTP_OPENING] = PFTM_TCP_OPENING_VAL;
	pf->timeout[PFTM_SCTP_ESTABLISHED] = PFTM_TCP_ESTABLISHED_VAL;
	pf->timeout[PFTM_SCTP_CLOSING] = PFTM_TCP_CLOSING_VAL;
	pf->timeout[PFTM_SCTP_CLOSED] = PFTM_TCP_CLOSED_VAL;
	pf->timeout[PFTM_UDP_FIRST_PACKET] = PFTM_UDP_FIRST_PACKET_VAL;
	pf->timeout[PFTM_UDP_SINGLE] = PFTM_UDP_SINGLE_VAL;
	pf->timeout[PFTM_UDP_MULTIPLE] = PFTM_UDP_MULTIPLE_VAL;
	pf->timeout[PFTM_ICMP_FIRST_PACKET] = PFTM_ICMP_FIRST_PACKET_VAL;
	pf->timeout[PFTM_ICMP_ERROR_REPLY] = PFTM_ICMP_ERROR_REPLY_VAL;
	pf->timeout[PFTM_OTHER_FIRST_PACKET] = PFTM_OTHER_FIRST_PACKET_VAL;
	pf->timeout[PFTM_OTHER_SINGLE] = PFTM_OTHER_SINGLE_VAL;
	pf->timeout[PFTM_OTHER_MULTIPLE] = PFTM_OTHER_MULTIPLE_VAL;
	pf->timeout[PFTM_FRAG] = PFTM_FRAG_VAL;
	pf->timeout[PFTM_INTERVAL] = PFTM_INTERVAL_VAL;
	pf->timeout[PFTM_SRC_NODE] = PFTM_SRC_NODE_VAL;
	pf->timeout[PFTM_TS_DIFF] = PFTM_TS_DIFF_VAL;
	pf->timeout[PFTM_ADAPTIVE_START] = PFSTATE_ADAPT_START;
	pf->timeout[PFTM_ADAPTIVE_END] = PFSTATE_ADAPT_END;

	pf->limit[PF_LIMIT_STATES] = PFSTATE_HIWAT;
	pf->limit[PF_LIMIT_FRAGS] = PFFRAG_FRENT_HIWAT;
	pf->limit[PF_LIMIT_SRC_NODES] = PFSNODE_HIWAT;
	pf->limit[PF_LIMIT_TABLE_ENTRIES] = PFR_KENTRY_HIWAT;

	pf->debug = PF_DEBUG_URGENT;
	pf->reassemble = 0;

	pf->syncookies = false;
	pf->syncookieswat[0] = PF_SYNCOOKIES_LOWATPCT;
	pf->syncookieswat[1] = PF_SYNCOOKIES_HIWATPCT;
}

int
pfctl_load_options(struct pfctl *pf)
{
	int i, error = 0;

	if ((loadopt & PFCTL_FLAG_OPTION) == 0)
		return (0);

	/* load limits */
	for (i = 0; i < PF_LIMIT_MAX; i++) {
		if ((pf->opts & PF_OPT_MERGE) && !pf->limit_set[i])
			continue;
		if (pfctl_load_limit(pf, i, pf->limit[i]))
			error = 1;
	}

	/*
	 * If we've set the limit, but haven't explicitly set adaptive
	 * timeouts, do it now with a start of 60% and end of 120%.
	 */
	if (pf->limit_set[PF_LIMIT_STATES] &&
	    !pf->timeout_set[PFTM_ADAPTIVE_START] &&
	    !pf->timeout_set[PFTM_ADAPTIVE_END]) {
		pf->timeout[PFTM_ADAPTIVE_START] =
			(pf->limit[PF_LIMIT_STATES] / 10) * 6;
		pf->timeout_set[PFTM_ADAPTIVE_START] = 1;
		pf->timeout[PFTM_ADAPTIVE_END] =
			(pf->limit[PF_LIMIT_STATES] / 10) * 12;
		pf->timeout_set[PFTM_ADAPTIVE_END] = 1;
	}

	/* load timeouts */
	for (i = 0; i < PFTM_MAX; i++) {
		if ((pf->opts & PF_OPT_MERGE) && !pf->timeout_set[i])
			continue;
		if (pfctl_load_timeout(pf, i, pf->timeout[i]))
			error = 1;
	}

	/* load debug */
	if (!(pf->opts & PF_OPT_MERGE) || pf->debug_set)
		if (pfctl_load_debug(pf, pf->debug))
			error = 1;

	/* load logif */
	if (!(pf->opts & PF_OPT_MERGE) || pf->ifname_set)
		if (pfctl_load_logif(pf, pf->ifname))
			error = 1;

	/* load hostid */
	if (!(pf->opts & PF_OPT_MERGE) || pf->hostid_set)
		if (pfctl_load_hostid(pf, pf->hostid))
			error = 1;

	/* load reassembly settings */
	if (!(pf->opts & PF_OPT_MERGE) || pf->reass_set)
		if (pfctl_load_reassembly(pf, pf->reassemble))
			error = 1;

	/* load keepcounters */
	if (pfctl_set_keepcounters(pf->dev, pf->keep_counters))
		error = 1;

	/* load syncookies settings */
	if (pfctl_load_syncookies(pf, pf->syncookies))
		error = 1;

	return (error);
}

int
pfctl_apply_limit(struct pfctl *pf, const char *opt, unsigned int limit)
{
	int i;


	for (i = 0; pf_limits[i].name; i++) {
		if (strcasecmp(opt, pf_limits[i].name) == 0) {
			pf->limit[pf_limits[i].index] = limit;
			pf->limit_set[pf_limits[i].index] = 1;
			break;
		}
	}
	if (pf_limits[i].name == NULL) {
		warnx("Bad pool name.");
		return (1);
	}

	if (pf->opts & PF_OPT_VERBOSE)
		printf("set limit %s %d\n", opt, limit);

	return (0);
}

int
pfctl_load_limit(struct pfctl *pf, unsigned int index, unsigned int limit)
{
	if (pfctl_set_limit(pf->h, index, limit)) {
		if (errno == EBUSY)
			warnx("Current pool size exceeds requested hard limit");
		else
			warnx("cannot set '%s' limit", pf_limits[index].name);
		return (1);
	}
	return (0);
}

int
pfctl_apply_timeout(struct pfctl *pf, const char *opt, int seconds, int quiet)
{
	int i;

	if ((loadopt & PFCTL_FLAG_OPTION) == 0)
		return (0);

	for (i = 0; pf_timeouts[i].name; i++) {
		if (strcasecmp(opt, pf_timeouts[i].name) == 0) {
			pf->timeout[pf_timeouts[i].timeout] = seconds;
			pf->timeout_set[pf_timeouts[i].timeout] = 1;
			break;
		}
	}

	if (pf_timeouts[i].name == NULL) {
		warnx("Bad timeout name.");
		return (1);
	}


	if (pf->opts & PF_OPT_VERBOSE && ! quiet)
		printf("set timeout %s %d\n", opt, seconds);

	return (0);
}

int
pfctl_load_timeout(struct pfctl *pf, unsigned int timeout, unsigned int seconds)
{
	if (pfctl_set_timeout(pf->h, timeout, seconds)) {
		warnx("DIOCSETTIMEOUT");
		return (1);
	}
	return (0);
}

int
pfctl_set_reassembly(struct pfctl *pf, int on, int nodf)
{
	if ((loadopt & PFCTL_FLAG_OPTION) == 0)
		return (0);

	pf->reass_set = 1;
	if (on) {
		pf->reassemble = PF_REASS_ENABLED;
		if (nodf)
			pf->reassemble |= PF_REASS_NODF;
	} else {
		pf->reassemble = 0;
	}

	if (pf->opts & PF_OPT_VERBOSE)
		printf("set reassemble %s %s\n", on ? "yes" : "no",
		    nodf ? "no-df" : "");

	return (0);
}

int
pfctl_set_optimization(struct pfctl *pf, const char *opt)
{
	const struct pf_hint *hint;
	int i, r;

	if ((loadopt & PFCTL_FLAG_OPTION) == 0)
		return (0);

	for (i = 0; pf_hints[i].name; i++)
		if (strcasecmp(opt, pf_hints[i].name) == 0)
			break;

	hint = pf_hints[i].hint;
	if (hint == NULL) {
		warnx("invalid state timeouts optimization");
		return (1);
	}

	for (i = 0; hint[i].name; i++)
		if ((r = pfctl_apply_timeout(pf, hint[i].name,
		    hint[i].timeout, 1)))
			return (r);

	if (pf->opts & PF_OPT_VERBOSE)
		printf("set optimization %s\n", opt);

	return (0);
}

int
pfctl_set_logif(struct pfctl *pf, char *ifname)
{

	if ((loadopt & PFCTL_FLAG_OPTION) == 0)
		return (0);

	if (!strcmp(ifname, "none")) {
		free(pf->ifname);
		pf->ifname = NULL;
	} else {
		pf->ifname = strdup(ifname);
		if (!pf->ifname)
			errx(1, "pfctl_set_logif: strdup");
	}
	pf->ifname_set = 1;

	if (pf->opts & PF_OPT_VERBOSE)
		printf("set loginterface %s\n", ifname);

	return (0);
}

int
pfctl_load_logif(struct pfctl *pf, char *ifname)
{
	if (ifname != NULL && strlen(ifname) >= IFNAMSIZ) {
		warnx("pfctl_load_logif: strlcpy");
		return (1);
	}
	return (pfctl_set_statusif(pfh, ifname ? ifname : ""));
}

void
pfctl_set_hostid(struct pfctl *pf, u_int32_t hostid)
{
	if ((loadopt & PFCTL_FLAG_OPTION) == 0)
		return;

	HTONL(hostid);

	pf->hostid = hostid;
	pf->hostid_set = 1;

	if (pf->opts & PF_OPT_VERBOSE)
		printf("set hostid 0x%08x\n", ntohl(hostid));
}

int
pfctl_load_hostid(struct pfctl *pf, u_int32_t hostid)
{
	if (ioctl(dev, DIOCSETHOSTID, &hostid)) {
		warnx("DIOCSETHOSTID");
		return (1);
	}
	return (0);
}

int
pfctl_load_reassembly(struct pfctl *pf, u_int32_t reassembly)
{
	if (ioctl(dev, DIOCSETREASS, &reassembly)) {
		warnx("DIOCSETREASS");
		return (1);
	}
	return (0);
}

int
pfctl_load_syncookies(struct pfctl *pf, u_int8_t val)
{
	struct pfctl_syncookies	cookies;

	bzero(&cookies, sizeof(cookies));

	cookies.mode = val;
	cookies.lowwater = pf->syncookieswat[0];
	cookies.highwater = pf->syncookieswat[1];

	if (pfctl_set_syncookies(dev, &cookies)) {
		warnx("DIOCSETSYNCOOKIES");
		return (1);
	}
	return (0);
}

int
pfctl_cfg_syncookies(struct pfctl *pf, uint8_t val, struct pfctl_watermarks *w)
{
	if (val != PF_SYNCOOKIES_ADAPTIVE && w != NULL) {
		warnx("syncookies start/end only apply to adaptive");
		return (1);
	}
	if (val == PF_SYNCOOKIES_ADAPTIVE && w != NULL) {
		if (!w->hi)
			w->hi = PF_SYNCOOKIES_HIWATPCT;
		if (!w->lo)
			w->lo = w->hi / 2;
		if (w->lo >= w->hi) {
			warnx("start must be higher than end");
			return (1);
		}
		pf->syncookieswat[0] = w->lo;
		pf->syncookieswat[1] = w->hi;
		pf->syncookieswat_set = 1;
	}

	if (pf->opts & PF_OPT_VERBOSE) {
		if (val == PF_SYNCOOKIES_NEVER)
			printf("set syncookies never\n");
		else if (val == PF_SYNCOOKIES_ALWAYS)
			printf("set syncookies always\n");
		else if (val == PF_SYNCOOKIES_ADAPTIVE) {
			if (pf->syncookieswat_set)
				printf("set syncookies adaptive (start %u%%, "
				    "end %u%%)\n", pf->syncookieswat[1],
				    pf->syncookieswat[0]);
			else
				printf("set syncookies adaptive\n");
		} else {        /* cannot happen */
			warnx("king bula ate all syncookies");
			return (1);
		}
	}

	pf->syncookies = val;
	return (0);
}

int
pfctl_do_set_debug(struct pfctl *pf, char *d)
{
	u_int32_t	level;
	int		ret;

	if ((loadopt & PFCTL_FLAG_OPTION) == 0)
		return (0);

	if (!strcmp(d, "none"))
		pf->debug = PF_DEBUG_NONE;
	else if (!strcmp(d, "urgent"))
		pf->debug = PF_DEBUG_URGENT;
	else if (!strcmp(d, "misc"))
		pf->debug = PF_DEBUG_MISC;
	else if (!strcmp(d, "loud"))
		pf->debug = PF_DEBUG_NOISY;
	else {
		warnx("unknown debug level \"%s\"", d);
		return (-1);
	}

	pf->debug_set = 1;
	level = pf->debug;

	if ((pf->opts & PF_OPT_NOACTION) == 0)
		if ((ret = pfctl_set_debug(pfh, level)) != 0)
			errc(1, ret, "DIOCSETDEBUG");

	if (pf->opts & PF_OPT_VERBOSE)
		printf("set debug %s\n", d);

	return (0);
}

int
pfctl_load_debug(struct pfctl *pf, unsigned int level)
{
	if (pfctl_set_debug(pf->h, level)) {
		warnx("DIOCSETDEBUG");
		return (1);
	}
	return (0);
}

int
pfctl_set_interface_flags(struct pfctl *pf, char *ifname, int flags, int how)
{
	struct pfioc_iface	pi;
	struct node_host	*h = NULL, *n = NULL;

	if ((loadopt & PFCTL_FLAG_OPTION) == 0)
		return (0);

	bzero(&pi, sizeof(pi));

	pi.pfiio_flags = flags;

	/* Make sure our cache matches the kernel. If we set or clear the flag
	 * for a group this applies to all members. */
	h = ifa_grouplookup(ifname, 0);
	for (n = h; n != NULL; n = n->next)
		pfctl_set_interface_flags(pf, n->ifname, flags, how);

	if (strlcpy(pi.pfiio_name, ifname, sizeof(pi.pfiio_name)) >=
	    sizeof(pi.pfiio_name))
		errx(1, "pfctl_set_interface_flags: strlcpy");

	if ((pf->opts & PF_OPT_NOACTION) == 0) {
		if (how == 0) {
			if (ioctl(pf->dev, DIOCCLRIFFLAG, &pi))
				err(1, "DIOCCLRIFFLAG");
		} else {
			if (ioctl(pf->dev, DIOCSETIFFLAG, &pi))
				err(1, "DIOCSETIFFLAG");
			pfctl_check_skip_ifaces(ifname);
		}
	}
	return (0);
}

void
pfctl_debug(int dev, u_int32_t level, int opts)
{
	int ret;

	if ((ret = pfctl_set_debug(pfh, level)) != 0)
		errc(1, ret, "DIOCSETDEBUG");
	if ((opts & PF_OPT_QUIET) == 0) {
		fprintf(stderr, "debug level set to '");
		switch (level) {
		case PF_DEBUG_NONE:
			fprintf(stderr, "none");
			break;
		case PF_DEBUG_URGENT:
			fprintf(stderr, "urgent");
			break;
		case PF_DEBUG_MISC:
			fprintf(stderr, "misc");
			break;
		case PF_DEBUG_NOISY:
			fprintf(stderr, "loud");
			break;
		default:
			fprintf(stderr, "<invalid>");
			break;
		}
		fprintf(stderr, "'\n");
	}
}

int
pfctl_test_altqsupport(int dev, int opts)
{
	struct pfioc_altq pa;

	pa.version = PFIOC_ALTQ_VERSION;
	if (ioctl(dev, DIOCGETALTQS, &pa)) {
		if (errno == ENODEV) {
			if (opts & PF_OPT_VERBOSE)
				fprintf(stderr, "No ALTQ support in kernel\n"
				    "ALTQ related functions disabled\n");
			return (0);
		} else
			err(1, "DIOCGETALTQS");
	}
	return (1);
}

int
pfctl_show_anchors(int dev, int opts, char *anchorname)
{
	struct pfioc_ruleset	 pr;
	u_int32_t		 mnr, nr;
	int			 ret;

	memset(&pr, 0, sizeof(pr));
	if ((ret = pfctl_get_rulesets(pfh, anchorname, &mnr)) != 0) {
		if (ret == EINVAL)
			fprintf(stderr, "Anchor '%s' not found.\n",
			    anchorname);
		else
			errc(1, ret, "DIOCGETRULESETS");
		return (-1);
	}
	for (nr = 0; nr < mnr; ++nr) {
		char sub[MAXPATHLEN];

		if ((ret = pfctl_get_ruleset(pfh, anchorname, nr, &pr)) != 0)
			errc(1, ret, "DIOCGETRULESET");
		if (!strcmp(pr.name, PF_RESERVED_ANCHOR))
			continue;
		sub[0] = 0;
		if (pr.path[0]) {
			strlcat(sub, pr.path, sizeof(sub));
			strlcat(sub, "/", sizeof(sub));
		}
		strlcat(sub, pr.name, sizeof(sub));
		if (sub[0] != '_' || (opts & PF_OPT_VERBOSE))
			printf("  %s\n", sub);
		if ((opts & PF_OPT_VERBOSE) && pfctl_show_anchors(dev, opts, sub))
			return (-1);
	}
	return (0);
}

int
pfctl_show_eth_anchors(int dev, int opts, char *anchorname)
{
	struct pfctl_eth_rulesets_info ri;
	struct pfctl_eth_ruleset_info rs;
	int ret;

	if ((ret = pfctl_get_eth_rulesets_info(dev, &ri, anchorname)) != 0) {
		if (ret == ENOENT)
			fprintf(stderr, "Anchor '%s' not found.\n",
			    anchorname);
		else
			errc(1, ret, "DIOCGETETHRULESETS");
		return (-1);
	}

	for (int nr = 0; nr < ri.nr; nr++) {
		char sub[MAXPATHLEN];

		if ((ret = pfctl_get_eth_ruleset(dev, anchorname, nr, &rs)) != 0)
			errc(1, ret, "DIOCGETETHRULESET");

		if (!strcmp(rs.name, PF_RESERVED_ANCHOR))
			continue;
		sub[0] = 0;
		if (rs.path[0]) {
			strlcat(sub, rs.path, sizeof(sub));
			strlcat(sub, "/", sizeof(sub));
		}
		strlcat(sub, rs.name, sizeof(sub));
		if (sub[0] != '_' || (opts & PF_OPT_VERBOSE))
			printf("  %s\n", sub);
		if ((opts & PF_OPT_VERBOSE) && pfctl_show_eth_anchors(dev, opts, sub))
			return (-1);
	}
	return (0);
}

const char *
pfctl_lookup_option(char *cmd, const char * const *list)
{
	if (cmd != NULL && *cmd)
		for (; *list; list++)
			if (!strncmp(cmd, *list, strlen(cmd)))
				return (*list);
	return (NULL);
}

int
main(int argc, char *argv[])
{
	int	 error = 0;
	int	 ch;
	int	 mode = O_RDONLY;
	int	 opts = 0;
	int	 optimize = PF_OPTIMIZE_BASIC;
	char	 anchorname[MAXPATHLEN];
	char	*path;

	if (argc < 2)
		usage();

	while ((ch = getopt(argc, argv,
	    "a:AdD:eqf:F:ghi:k:K:mMnNOo:Pp:rRs:t:T:vx:z")) != -1) {
		switch (ch) {
		case 'a':
			anchoropt = optarg;
			break;
		case 'd':
			opts |= PF_OPT_DISABLE;
			mode = O_RDWR;
			break;
		case 'D':
			if (pfctl_cmdline_symset(optarg) < 0)
				warnx("could not parse macro definition %s",
				    optarg);
			break;
		case 'e':
			opts |= PF_OPT_ENABLE;
			mode = O_RDWR;
			break;
		case 'q':
			opts |= PF_OPT_QUIET;
			break;
		case 'F':
			clearopt = pfctl_lookup_option(optarg, clearopt_list);
			if (clearopt == NULL) {
				warnx("Unknown flush modifier '%s'", optarg);
				usage();
			}
			mode = O_RDWR;
			break;
		case 'i':
			ifaceopt = optarg;
			break;
		case 'k':
			if (state_killers >= 2) {
				warnx("can only specify -k twice");
				usage();
				/* NOTREACHED */
			}
			state_kill[state_killers++] = optarg;
			mode = O_RDWR;
			break;
		case 'K':
			if (src_node_killers >= 2) {
				warnx("can only specify -K twice");
				usage();
				/* NOTREACHED */
			}
			src_node_kill[src_node_killers++] = optarg;
			mode = O_RDWR;
			break;
		case 'm':
			opts |= PF_OPT_MERGE;
			break;
		case 'M':
			opts |= PF_OPT_KILLMATCH;
			break;
		case 'n':
			opts |= PF_OPT_NOACTION;
			break;
		case 'N':
			loadopt |= PFCTL_FLAG_NAT;
			break;
		case 'r':
			opts |= PF_OPT_USEDNS;
			break;
		case 'f':
			rulesopt = optarg;
			mode = O_RDWR;
			break;
		case 'g':
			opts |= PF_OPT_DEBUG;
			break;
		case 'A':
			loadopt |= PFCTL_FLAG_ALTQ;
			break;
		case 'R':
			loadopt |= PFCTL_FLAG_FILTER;
			break;
		case 'o':
			optiopt = pfctl_lookup_option(optarg, optiopt_list);
			if (optiopt == NULL) {
				warnx("Unknown optimization '%s'", optarg);
				usage();
			}
			opts |= PF_OPT_OPTIMIZE;
			break;
		case 'O':
			loadopt |= PFCTL_FLAG_OPTION;
			break;
		case 'p':
			pf_device = optarg;
			break;
		case 'P':
			opts |= PF_OPT_NUMERIC;
			break;
		case 's':
			showopt = pfctl_lookup_option(optarg, showopt_list);
			if (showopt == NULL) {
				warnx("Unknown show modifier '%s'", optarg);
				usage();
			}
			break;
		case 't':
			tableopt = optarg;
			break;
		case 'T':
			tblcmdopt = pfctl_lookup_option(optarg, tblcmdopt_list);
			if (tblcmdopt == NULL) {
				warnx("Unknown table command '%s'", optarg);
				usage();
			}
			break;
		case 'v':
			if (opts & PF_OPT_VERBOSE)
				opts |= PF_OPT_VERBOSE2;
			opts |= PF_OPT_VERBOSE;
			break;
		case 'x':
			debugopt = pfctl_lookup_option(optarg, debugopt_list);
			if (debugopt == NULL) {
				warnx("Unknown debug level '%s'", optarg);
				usage();
			}
			mode = O_RDWR;
			break;
		case 'z':
			opts |= PF_OPT_CLRRULECTRS;
			mode = O_RDWR;
			break;
		case 'h':
			/* FALLTHROUGH */
		default:
			usage();
			/* NOTREACHED */
		}
	}

	if (tblcmdopt != NULL) {
		argc -= optind;
		argv += optind;
		ch = *tblcmdopt;
		if (ch == 'l') {
			loadopt |= PFCTL_FLAG_TABLE;
			tblcmdopt = NULL;
		} else
			mode = strchr("acdefkrz", ch) ? O_RDWR : O_RDONLY;
	} else if (argc != optind) {
		warnx("unknown command line argument: %s ...", argv[optind]);
		usage();
		/* NOTREACHED */
	}
	if (loadopt == 0)
		loadopt = ~0;

	if ((path = calloc(1, MAXPATHLEN)) == NULL)
		errx(1, "pfctl: calloc");
	memset(anchorname, 0, sizeof(anchorname));
	if (anchoropt != NULL) {
		int len = strlen(anchoropt);

		if (len >= 1 && anchoropt[len - 1] == '*') {
			if (len >= 2 && anchoropt[len - 2] == '/')
				anchoropt[len - 2] = '\0';
			else
				anchoropt[len - 1] = '\0';
			opts |= PF_OPT_RECURSE;
		}
		if (strlcpy(anchorname, anchoropt,
		    sizeof(anchorname)) >= sizeof(anchorname))
			errx(1, "anchor name '%s' too long",
			    anchoropt);
		loadopt &= PFCTL_FLAG_FILTER|PFCTL_FLAG_NAT|PFCTL_FLAG_TABLE|PFCTL_FLAG_ETH;
	}

	if ((opts & PF_OPT_NOACTION) == 0) {
		dev = open(pf_device, mode);
		if (dev == -1)
			err(1, "%s", pf_device);
		altqsupport = pfctl_test_altqsupport(dev, opts);
	} else {
		dev = open(pf_device, O_RDONLY);
		if (dev >= 0)
			opts |= PF_OPT_DUMMYACTION;
		/* turn off options */
		opts &= ~ (PF_OPT_DISABLE | PF_OPT_ENABLE);
		clearopt = showopt = debugopt = NULL;
#if !defined(ENABLE_ALTQ)
		altqsupport = 0;
#else
		altqsupport = 1;
#endif
	}
	pfh = pfctl_open(pf_device);
	if (pfh == NULL)
		err(1, "Failed to open netlink");

	if (opts & PF_OPT_DISABLE)
		if (pfctl_disable(dev, opts))
			error = 1;

	if (showopt != NULL) {
		switch (*showopt) {
		case 'A':
			pfctl_show_anchors(dev, opts, anchorname);
			if (opts & PF_OPT_VERBOSE2)
				printf("Ethernet:\n");
			pfctl_show_eth_anchors(dev, opts, anchorname);
			break;
		case 'r':
			pfctl_load_fingerprints(dev, opts);
			pfctl_show_rules(dev, path, opts, PFCTL_SHOW_RULES,
			    anchorname, 0, 0);
			break;
		case 'l':
			pfctl_load_fingerprints(dev, opts);
			pfctl_show_rules(dev, path, opts, PFCTL_SHOW_LABELS,
			    anchorname, 0, 0);
			break;
		case 'n':
			pfctl_load_fingerprints(dev, opts);
			pfctl_show_nat(dev, path, opts, anchorname, 0, 0);
			break;
		case 'q':
			pfctl_show_altq(dev, ifaceopt, opts,
			    opts & PF_OPT_VERBOSE2);
			break;
		case 's':
			pfctl_show_states(dev, ifaceopt, opts);
			break;
		case 'S':
			pfctl_show_src_nodes(dev, opts);
			break;
		case 'i':
			pfctl_show_status(dev, opts);
			break;
		case 'R':
			error = pfctl_show_running(dev);
			break;
		case 't':
			pfctl_show_timeouts(dev, opts);
			break;
		case 'm':
			pfctl_show_limits(dev, opts);
			break;
		case 'e':
			pfctl_show_eth_rules(dev, path, opts, 0, anchorname, 0,
			    0);
			break;
		case 'a':
			opts |= PF_OPT_SHOWALL;
			pfctl_load_fingerprints(dev, opts);

			pfctl_show_eth_rules(dev, path, opts, 0, anchorname, 0,
			    0);

			pfctl_show_nat(dev, path, opts, anchorname, 0, 0);
			pfctl_show_rules(dev, path, opts, 0, anchorname, 0, 0);
			pfctl_show_altq(dev, ifaceopt, opts, 0);
			pfctl_show_states(dev, ifaceopt, opts);
			pfctl_show_src_nodes(dev, opts);
			pfctl_show_status(dev, opts);
			pfctl_show_rules(dev, path, opts, 1, anchorname, 0, 0);
			pfctl_show_timeouts(dev, opts);
			pfctl_show_limits(dev, opts);
			pfctl_show_tables(anchorname, opts);
			pfctl_show_fingerprints(opts);
			break;
		case 'T':
			pfctl_show_tables(anchorname, opts);
			break;
		case 'o':
			pfctl_load_fingerprints(dev, opts);
			pfctl_show_fingerprints(opts);
			break;
		case 'I':
			pfctl_show_ifaces(ifaceopt, opts);
			break;
		case 'c':
			pfctl_show_creators(opts);
			break;
		}
	}

	if ((opts & PF_OPT_CLRRULECTRS) && showopt == NULL) {
		pfctl_show_eth_rules(dev, path, opts, PFCTL_SHOW_NOTHING,
		    anchorname, 0, 0);
		pfctl_show_rules(dev, path, opts, PFCTL_SHOW_NOTHING,
		    anchorname, 0, 0);
	}

	if (clearopt != NULL) {
		if (anchorname[0] == '_' || strstr(anchorname, "/_") != NULL)
			errx(1, "anchor names beginning with '_' cannot "
			    "be modified from the command line");

		switch (*clearopt) {
		case 'e':
			pfctl_flush_eth_rules(dev, opts, anchorname);
			break;
		case 'r':
			pfctl_flush_rules(dev, opts, anchorname);
			break;
		case 'n':
			pfctl_flush_nat(dev, opts, anchorname);
			break;
		case 'q':
			pfctl_clear_altq(dev, opts);
			break;
		case 's':
			pfctl_clear_iface_states(dev, ifaceopt, opts);
			break;
		case 'S':
			pfctl_clear_src_nodes(dev, opts);
			break;
		case 'i':
			pfctl_clear_stats(pfh, opts);
			break;
		case 'a':
			pfctl_flush_eth_rules(dev, opts, anchorname);
			pfctl_flush_rules(dev, opts, anchorname);
			pfctl_flush_nat(dev, opts, anchorname);
			pfctl_do_clear_tables(anchorname, opts);
			if (!*anchorname) {
				pfctl_clear_altq(dev, opts);
				pfctl_clear_iface_states(dev, ifaceopt, opts);
				pfctl_clear_src_nodes(dev, opts);
				pfctl_clear_stats(pfh, opts);
				pfctl_clear_fingerprints(dev, opts);
				pfctl_clear_interface_flags(dev, opts);
			}
			break;
		case 'o':
			pfctl_clear_fingerprints(dev, opts);
			break;
		case 'T':
			pfctl_do_clear_tables(anchorname, opts);
			break;
		}
	}
	if (state_killers) {
		if (!strcmp(state_kill[0], "label"))
			pfctl_label_kill_states(dev, ifaceopt, opts);
		else if (!strcmp(state_kill[0], "id"))
			pfctl_id_kill_states(dev, ifaceopt, opts);
		else if (!strcmp(state_kill[0], "gateway"))
			pfctl_gateway_kill_states(dev, ifaceopt, opts);
		else
			pfctl_net_kill_states(dev, ifaceopt, opts);
	}

	if (src_node_killers)
		pfctl_kill_src_nodes(dev, ifaceopt, opts);

	if (tblcmdopt != NULL) {
		error = pfctl_command_tables(argc, argv, tableopt,
		    tblcmdopt, rulesopt, anchorname, opts);
		rulesopt = NULL;
	}
	if (optiopt != NULL) {
		switch (*optiopt) {
		case 'n':
			optimize = 0;
			break;
		case 'b':
			optimize |= PF_OPTIMIZE_BASIC;
			break;
		case 'o':
		case 'p':
			optimize |= PF_OPTIMIZE_PROFILE;
			break;
		}
	}

	if ((rulesopt != NULL) && (loadopt & PFCTL_FLAG_OPTION) &&
	    !anchorname[0] && !(opts & PF_OPT_NOACTION))
		if (pfctl_get_skip_ifaces())
			error = 1;

	if (rulesopt != NULL && !(opts & PF_OPT_MERGE) &&
	    !anchorname[0] && (loadopt & PFCTL_FLAG_OPTION))
		if (pfctl_file_fingerprints(dev, opts, PF_OSFP_FILE))
			error = 1;

	if (rulesopt != NULL) {
		if (anchorname[0] == '_' || strstr(anchorname, "/_") != NULL)
			errx(1, "anchor names beginning with '_' cannot "
			    "be modified from the command line");
		if (pfctl_rules(dev, rulesopt, opts, optimize,
		    anchorname, NULL))
			error = 1;
		else if (!(opts & PF_OPT_NOACTION) &&
		    (loadopt & PFCTL_FLAG_TABLE))
			warn_namespace_collision(NULL);
	}

	if (opts & PF_OPT_ENABLE)
		if (pfctl_enable(dev, opts))
			error = 1;

	if (debugopt != NULL) {
		switch (*debugopt) {
		case 'n':
			pfctl_debug(dev, PF_DEBUG_NONE, opts);
			break;
		case 'u':
			pfctl_debug(dev, PF_DEBUG_URGENT, opts);
			break;
		case 'm':
			pfctl_debug(dev, PF_DEBUG_MISC, opts);
			break;
		case 'l':
			pfctl_debug(dev, PF_DEBUG_NOISY, opts);
			break;
		}
	}

	exit(error);
}
