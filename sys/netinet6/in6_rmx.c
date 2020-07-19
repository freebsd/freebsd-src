/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 *	$KAME: in6_rmx.c,v 1.11 2001/07/26 06:53:16 jinmei Exp $
 */

/*-
 * Copyright 1994, 1995 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_mpath.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/mbuf.h>
#include <sys/rwlock.h>
#include <sys/syslog.h>
#include <sys/callout.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/route.h>
#include <net/route/route_ctl.h>
#include <net/route/route_var.h>
#include <net/route/nhop.h>
#include <net/route/shared.h>

#include <netinet/in.h>
#include <netinet/ip_var.h>
#include <netinet/in_var.h>

#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>

#include <netinet/icmp6.h>
#include <netinet6/nd6.h>

#include <netinet/tcp.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>

extern int	in6_inithead(void **head, int off, u_int fibnum);
#ifdef VIMAGE
extern int	in6_detachhead(void **head, int off);
#endif

static int
rib6_preadd(u_int fibnum, const struct sockaddr *addr, const struct sockaddr *mask,
    struct nhop_object *nh)
{
	uint16_t nh_type;

	/* XXX: RTF_LOCAL */

	/*
	 * Check route MTU:
	 * inherit interface MTU if not set or
	 * check if MTU is too large.
	 */
	if (nh->nh_mtu == 0) {
		nh->nh_mtu = IN6_LINKMTU(nh->nh_ifp);
	} else if (nh->nh_mtu > IN6_LINKMTU(nh->nh_ifp))
		nh->nh_mtu = IN6_LINKMTU(nh->nh_ifp);

	/* Ensure that default route nhop has special flag */
	const struct sockaddr_in6 *mask6 = (const struct sockaddr_in6 *)mask;
	if ((nhop_get_rtflags(nh) & RTF_HOST) == 0 && mask6 != NULL &&
	    IN6_IS_ADDR_UNSPECIFIED(&mask6->sin6_addr))
		nh->nh_flags |= NHF_DEFAULT;

	/* Set nexthop type */
	if (nhop_get_type(nh) == 0) {
		if (nh->nh_flags & NHF_GATEWAY)
			nh_type = NH_TYPE_IPV6_ETHER_NHOP;
		else
			nh_type = NH_TYPE_IPV6_ETHER_RSLV;

		nhop_set_type(nh, nh_type);
	}

	return (0);
}

/*
 * Initialize our routing tree.
 */

int
in6_inithead(void **head, int off, u_int fibnum)
{
	struct epoch_tracker et;
	struct rib_head *rh;

	rh = rt_table_init(offsetof(struct sockaddr_in6, sin6_addr) << 3,
	    AF_INET6, fibnum);
	if (rh == NULL)
		return (0);

	rh->rnh_preadd = rib6_preadd;
#ifdef	RADIX_MPATH
	rt_mpath_init_rnh(rh);
#endif
	*head = (void *)rh;

	NET_EPOCH_ENTER(et);
	if (rib_subscribe(fibnum, AF_INET6, nd6_subscription_cb, NULL,
	    RIB_NOTIFY_IMMEDIATE, M_NOWAIT) == NULL)
		log(LOG_ERR, "in6_inithead(): unable to subscribe to fib %u\n",
		    fibnum);
	NET_EPOCH_EXIT(et);

	return (1);
}

#ifdef VIMAGE
int
in6_detachhead(void **head, int off)
{

	rt_table_destroy((struct rib_head *)(*head));

	return (1);
}
#endif

