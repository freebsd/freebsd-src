/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright 2019 Conrad Meyer <cem@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/ctype.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/rmlock.h>

#include <ddb/ddb.h>
#include <ddb/db_lex.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <net/route/nhop.h>
#include <net/route/route_ctl.h>
#include <net/route/route_var.h>

/*
 * Unfortunately, RTF_ values are expressed as raw masks rather than powers of
 * 2, so we cannot use them as nice C99 initializer indices below.
 */
static const char * const rtf_flag_strings[] = {
	"UP",
	"GATEWAY",
	"HOST",
	"REJECT",
	"DYNAMIC",
	"MODIFIED",
	"DONE",
	"UNUSED_0x80",
	"UNUSED_0x100",
	"XRESOLVE",
	"LLDATA",
	"STATIC",
	"BLACKHOLE",
	"UNUSED_0x2000",
	"PROTO2",
	"PROTO1",
	"UNUSED_0x10000",
	"UNUSED_0x20000",
	"PROTO3",
	"FIXEDMTU",
	"PINNED",
	"LOCAL",
	"BROADCAST",
	"MULTICAST",
	/* Big gap. */
	[28] = "STICKY",
	[30] = "RNH_LOCKED",
	[31] = "GWFLAG_COMPAT",
};

static const char * __pure
rt_flag_name(unsigned idx)
{
	if (idx >= nitems(rtf_flag_strings))
		return ("INVALID_FLAG");
	if (rtf_flag_strings[idx] == NULL)
		return ("UNKNOWN");
	return (rtf_flag_strings[idx]);
}

static void
rt_dumpaddr_ddb(const char *name, const struct sockaddr *sa)
{
	char buf[INET6_ADDRSTRLEN], *res;

	res = NULL;
	if (sa == NULL)
		res = "NULL";
	else if (sa->sa_family == AF_INET) {
		res = inet_ntop(AF_INET,
		    &((const struct sockaddr_in *)sa)->sin_addr,
		    buf, sizeof(buf));
	} else if (sa->sa_family == AF_INET6) {
		res = inet_ntop(AF_INET6,
		    &((const struct sockaddr_in6 *)sa)->sin6_addr,
		    buf, sizeof(buf));
	} else if (sa->sa_family == AF_LINK) {
		res = "on link";
	}

	if (res != NULL) {
		db_printf("%s <%s> ", name, res);
		return;
	}

	db_printf("%s <af:%d> ", name, sa->sa_family);
}

static int
rt_dumpentry_ddb(struct radix_node *rn, void *arg __unused)
{
	struct sockaddr_storage ss;
	struct rtentry *rt;
	struct nhop_object *nh;
	int flags, idx;

	/* If RNTORT is important, put it in a header. */
	rt = (void *)rn;
	nh = (struct nhop_object *)rt->rt_nhop;

	rt_dumpaddr_ddb("dst", rt_key(rt));
	rt_dumpaddr_ddb("gateway", &rt->rt_nhop->gw_sa);
	rt_dumpaddr_ddb("netmask", rtsock_fix_netmask(rt_key(rt), rt_mask(rt),
	    &ss));
	if ((nh->nh_ifp->if_flags & IFF_DYING) == 0) {
		rt_dumpaddr_ddb("ifp", nh->nh_ifp->if_addr->ifa_addr);
		rt_dumpaddr_ddb("ifa", nh->nh_ifa->ifa_addr);
	}

	db_printf("flags ");
	flags = rt->rte_flags | nhop_get_rtflags(nh);
	if (flags == 0)
		db_printf("none");

	while ((idx = ffs(flags)) > 0) {
		idx--;

		db_printf("%s", rt_flag_name(idx));
		flags &= ~(1ul << idx);
		if (flags != 0)
			db_printf(",");
	}

	db_printf("\n");
	return (0);
}

DB_SHOW_COMMAND(routetable, db_show_routetable_cmd)
{
	struct rib_head *rnh;
	int error, i, lim;

	if (have_addr)
		i = lim = addr;
	else {
		i = 1;
		lim = AF_MAX;
	}

	for (; i <= lim; i++) {
		rnh = rt_tables_get_rnh(0, i);
		if (rnh == NULL) {
			if (have_addr) {
				db_printf("%s: AF %d not supported?\n",
				    __func__, i);
				break;
			}
			continue;
		}

		if (!have_addr && i > 1)
			db_printf("\n");

		db_printf("Route table for AF %d%s%s%s:\n", i,
		    (i == AF_INET || i == AF_INET6) ? " (" : "",
		    (i == AF_INET) ? "INET" : (i == AF_INET6) ? "INET6" : "",
		    (i == AF_INET || i == AF_INET6) ? ")" : "");

		error = rnh->rnh_walktree(&rnh->head, rt_dumpentry_ddb, NULL);
		if (error != 0)
			db_printf("%s: walktree(%d): %d\n", __func__, i,
			    error);
	}
}

_DB_FUNC(_show, route, db_show_route_cmd, db_show_table, CS_OWN, NULL)
{
	char abuf[INET6_ADDRSTRLEN], *buf, *end;
	struct rib_head *rh;
	struct radix_node *rn;
	void *dst_addrp;
	struct rtentry *rt;
	union {
		struct sockaddr_in dest_sin;
		struct sockaddr_in6 dest_sin6;
	} u;
	int af;

	buf = db_get_line();

	/* Remove whitespaces from both ends */
	end = buf + strlen(buf) - 1;
	for (; (end >= buf) && (*end=='\n' || isspace(*end)); end--)
		*end = '\0';
	while (isspace(*buf))
		buf++;

	/* Determine AF */
	if (strchr(buf, ':') != NULL) {
		af = AF_INET6;
		u.dest_sin6.sin6_family = af;
		u.dest_sin6.sin6_len = sizeof(struct sockaddr_in6);
		dst_addrp = &u.dest_sin6.sin6_addr;
	} else {
		af = AF_INET;
		u.dest_sin.sin_family = af;
		u.dest_sin.sin_len = sizeof(struct sockaddr_in);
		dst_addrp = &u.dest_sin.sin_addr;
	}

	if (inet_pton(af, buf, dst_addrp) != 1)
		goto usage;

	if (inet_ntop(af, dst_addrp, abuf, sizeof(abuf)) != NULL)
		db_printf("Looking up route to destination '%s'\n", abuf);

	rt = NULL;
	CURVNET_SET(vnet0);

	rh = rt_tables_get_rnh(RT_DEFAULT_FIB, af);

	rn = rh->rnh_matchaddr(&u, &rh->head);
	if (rn && ((rn->rn_flags & RNF_ROOT) == 0))
		rt = (struct rtentry *)rn;

	CURVNET_RESTORE();

	if (rt == NULL) {
		db_printf("Could not get route for that server.\n");
		return;
	}

	rt_dumpentry_ddb((void *)rt, NULL);

	return;
usage:
	db_printf("Usage: 'show route <address>'\n"
	    "  Currently accepts only IPv4 and IPv6 addresses\n");
	db_skip_to_eol();
}
