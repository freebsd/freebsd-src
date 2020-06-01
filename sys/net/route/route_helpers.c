/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2020 Alexander V. Chernikov
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
#include "opt_route.h"

#include <sys/param.h>
#include <sys/jail.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/sysproto.h>
#include <sys/proc.h>
#include <sys/domain.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/rmlock.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <net/route/route_ctl.h>
#include <net/route/route_var.h>
#include <net/route/nhop_utils.h>
#include <net/route/nhop.h>
#include <net/route/nhop_var.h>
#include <net/route/shared.h>
#ifdef INET
#include <netinet/in_fib.h>
#endif
#ifdef INET6
#include <netinet6/in6_fib.h>
#endif
#include <net/vnet.h>

/*
 * RIB helper functions.
 */

/*
 * Calls @wa_f with @arg for each entry in the table specified by
 * @af and @fibnum.
 *
 * Table is traversed under read lock.
 */
void
rib_walk(int af, u_int fibnum, rt_walktree_f_t *wa_f, void *arg)
{
	RIB_RLOCK_TRACKER;
	struct rib_head *rnh;

	if ((rnh = rt_tables_get_rnh(fibnum, af)) == NULL)
		return;

	RIB_RLOCK(rnh);
	rnh->rnh_walktree(&rnh->head, (walktree_f_t *)wa_f, arg);
	RIB_RUNLOCK(rnh);
}

/*
 * Wrapper for the control plane functions for performing af-agnostic
 *  lookups.
 * @fibnum: fib to perform the lookup.
 * @dst: sockaddr with family and addr filled in. IPv6 addresses needs to be in
 *  deembedded from.
 * @flags: fib(9) flags.
 * @flowid: flow id for path selection in multipath use case.
 *
 * Returns nhop_object or NULL.
 *
 * Requires NET_EPOCH.
 *
 */
struct nhop_object *
rib_lookup(uint32_t fibnum, const struct sockaddr *dst, uint32_t flags,
    uint32_t flowid)
{
	struct nhop_object *nh;

	nh = NULL;

	switch (dst->sa_family) {
#ifdef INET
	case AF_INET:
	{
		const struct sockaddr_in *a = (const struct sockaddr_in *)dst;
		nh = fib4_lookup(fibnum, a->sin_addr, 0, flags, flowid);
		break;
	}
#endif
#ifdef INET6
	case AF_INET6:
	{
		const struct sockaddr_in6 *a = (const struct sockaddr_in6*)dst;
		nh = fib6_lookup(fibnum, &a->sin6_addr, a->sin6_scope_id,
		    flags, flowid);
		break;
	}
#endif
	}

	return (nh);
}

