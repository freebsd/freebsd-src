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
#include <net/vnet.h>
#include <net/route.h>
#include <net/route/route_var.h>
#include <net/route/nhop.h>
#include <netinet/in.h>

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
	flags = rt->rt_flags;
	if (flags == 0)
		db_printf("none");

	while ((idx = ffs(flags)) > 0) {
		idx--;

		if (flags != rt->rt_flags)
			db_printf(",");
		db_printf("%s", rt_flag_name(idx));

		flags &= ~(1ul << idx);
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
	char buf[INET6_ADDRSTRLEN], *bp;
	const void *dst_addrp;
	struct sockaddr *dstp;
	struct rtentry *rt;
	union {
		struct sockaddr_in dest_sin;
		struct sockaddr_in6 dest_sin6;
	} u;
	uint16_t hextets[8];
	unsigned i, tets;
	int t, af, exp, tokflags;

	/*
	 * Undecoded address family.  No double-colon expansion seen yet.
	 */
	af = -1;
	exp = -1;
	/* Assume INET6 to start; we can work back if guess was wrong. */
	tokflags = DRT_WSPACE | DRT_HEX | DRT_HEXADECIMAL;

	/*
	 * db_command has lexed 'show route' for us.
	 */
	t = db_read_token_flags(tokflags);
	if (t == tWSPACE)
		t = db_read_token_flags(tokflags);

	/*
	 * tEOL: Just 'show route' isn't a valid mode.
	 * tMINUS: It's either '-h' or some invalid option.  Regardless, usage.
	 */
	if (t == tEOL || t == tMINUS)
		goto usage;

	db_unread_token(t);

	tets = nitems(hextets);

	/*
	 * Each loop iteration, we expect to read one octet (v4) or hextet
	 * (v6), followed by an appropriate field separator ('.' or ':' or
	 * '::').
	 *
	 * At the start of each loop, we're looking for a number (octet or
	 * hextet).
	 *
	 * INET6 addresses have a special case where they may begin with '::'.
	 */
	for (i = 0; i < tets; i++) {
		t = db_read_token_flags(tokflags);

		if (t == tCOLONCOLON) {
			/* INET6 with leading '::' or invalid. */
			if (i != 0) {
				db_printf("Parse error: unexpected extra "
				    "colons.\n");
				goto exit;
			}

			af = AF_INET6;
			exp = i;
			hextets[i] = 0;
			continue;
		} else if (t == tNUMBER) {
			/*
			 * Lexer separates out '-' as tMINUS, but make the
			 * assumption explicit here.
			 */
			MPASS(db_tok_number >= 0);

			if (af == AF_INET && db_tok_number > UINT8_MAX) {
				db_printf("Not a valid v4 octet: %ld\n",
				    (long)db_tok_number);
				goto exit;
			}
			hextets[i] = db_tok_number;
		} else if (t == tEOL) {
			/*
			 * We can only detect the end of an IPv6 address in
			 * compact representation with EOL.
			 */
			if (af != AF_INET6 || exp < 0) {
				db_printf("Parse failed.  Got unexpected EOF "
				    "when the address is not a compact-"
				    "representation IPv6 address.\n");
				goto exit;
			}
			break;
		} else {
			db_printf("Parse failed.  Unexpected token %d.\n", t);
			goto exit;
		}

		/* Next, look for a separator, if appropriate. */
		if (i == tets - 1)
			continue;

		t = db_read_token_flags(tokflags);
		if (af < 0) {
			if (t == tCOLON) {
				af = AF_INET6;
				continue;
			}
			if (t == tCOLONCOLON) {
				af = AF_INET6;
				i++;
				hextets[i] = 0;
				exp = i;
				continue;
			}
			if (t == tDOT) {
				unsigned hn, dn;

				af = AF_INET;
				/* Need to fixup the first parsed number. */
				if (hextets[0] > 0x255 ||
				    (hextets[0] & 0xf0) > 0x90 ||
				    (hextets[0] & 0xf) > 9) {
					db_printf("Not a valid v4 octet: %x\n",
					    hextets[0]);
					goto exit;
				}

				hn = hextets[0];
				dn = (hn >> 8) * 100 +
				    ((hn >> 4) & 0xf) * 10 +
				    (hn & 0xf);

				hextets[0] = dn;

				/* Switch to decimal for remaining octets. */
				tokflags &= ~DRT_RADIX_MASK;
				tokflags |= DRT_DECIMAL;

				tets = 4;
				continue;
			}

			db_printf("Parse error.  Unexpected token %d.\n", t);
			goto exit;
		} else if (af == AF_INET) {
			if (t == tDOT)
				continue;
			db_printf("Expected '.' (%d) between octets but got "
			    "(%d).\n", tDOT, t);
			goto exit;

		} else if (af == AF_INET6) {
			if (t == tCOLON)
				continue;
			if (t == tCOLONCOLON) {
				if (exp < 0) {
					i++;
					hextets[i] = 0;
					exp = i;
					continue;
				}
				db_printf("Got bogus second '::' in v6 "
				    "address.\n");
				goto exit;
			}
			if (t == tEOL) {
				/*
				 * Handle in the earlier part of the loop
				 * because we need to handle trailing :: too.
				 */
				db_unread_token(t);
				continue;
			}

			db_printf("Expected ':' (%d) or '::' (%d) between "
			    "hextets but got (%d).\n", tCOLON, tCOLONCOLON, t);
			goto exit;
		}
	}

	/* Check for trailing garbage. */
	if (i == tets) {
		t = db_read_token_flags(tokflags);
		if (t != tEOL) {
			db_printf("Got unexpected garbage after address "
			    "(%d).\n", t);
			goto exit;
		}
	}

	/*
	 * Need to expand compact INET6 addresses.
	 *
	 * Technically '::' for a single ':0:' is MUST NOT but just in case,
	 * don't bother expanding that form (exp >= 0 && i == tets case).
	 */
	if (af == AF_INET6 && exp >= 0 && i < tets) {
		if (exp + 1 < i) {
			memmove(&hextets[exp + 1 + (nitems(hextets) - i)],
			    &hextets[exp + 1],
			    (i - (exp + 1)) * sizeof(hextets[0]));
		}
		memset(&hextets[exp + 1], 0, (nitems(hextets) - i) *
		    sizeof(hextets[0]));
	}

	memset(&u, 0, sizeof(u));
	if (af == AF_INET) {
		u.dest_sin.sin_family = AF_INET;
		u.dest_sin.sin_len = sizeof(u.dest_sin);
		u.dest_sin.sin_addr.s_addr = htonl(
		    ((uint32_t)hextets[0] << 24) |
		    ((uint32_t)hextets[1] << 16) |
		    ((uint32_t)hextets[2] << 8) |
		    (uint32_t)hextets[3]);
		dstp = (void *)&u.dest_sin;
		dst_addrp = &u.dest_sin.sin_addr;
	} else if (af == AF_INET6) {
		u.dest_sin6.sin6_family = AF_INET6;
		u.dest_sin6.sin6_len = sizeof(u.dest_sin6);
		for (i = 0; i < nitems(hextets); i++)
			u.dest_sin6.sin6_addr.s6_addr16[i] = htons(hextets[i]);
		dstp = (void *)&u.dest_sin6;
		dst_addrp = &u.dest_sin6.sin6_addr;
	} else {
		MPASS(false);
		/* UNREACHABLE */
		/* Appease Clang false positive: */
		dstp = NULL;
	}

	bp = inet_ntop(af, dst_addrp, buf, sizeof(buf));
	if (bp != NULL)
		db_printf("Looking up route to destination '%s'\n", bp);

	CURVNET_SET(vnet0);
	rt = rtalloc1(dstp, 0, RTF_RNH_LOCKED);
	CURVNET_RESTORE();

	if (rt == NULL) {
		db_printf("Could not get route for that server.\n");
		return;
	}

	rt_dumpentry_ddb((void *)rt, NULL);
	RTFREE_LOCKED(rt);

	return;
usage:
	db_printf("Usage: 'show route <address>'\n"
	    "  Currently accepts only dotted-decimal INET or colon-separated\n"
	    "  hextet INET6 addresses.\n");
exit:
	db_skip_to_eol();
}

