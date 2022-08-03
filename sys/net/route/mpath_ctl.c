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
 *
 * $FreeBSD$
 */

#include "opt_inet.h"
#include "opt_route.h"

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/rmlock.h>
#include <sys/rwlock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/kernel.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <net/route/route_ctl.h>
#include <net/route/route_var.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_fib.h>

#include <net/route/nhop_utils.h>
#include <net/route/nhop.h>
#include <net/route/nhop_var.h>

/*
 * This file contains the supporting functions for adding/deleting/updating
 *  multipath routes to the routing table.
 */

SYSCTL_DECL(_net_route);
VNET_DEFINE(u_int, fib_hash_outbound) = 0;
SYSCTL_UINT(_net_route, OID_AUTO, hash_outbound, CTLFLAG_RD | CTLFLAG_VNET,
    &VNET_NAME(fib_hash_outbound), 0,
    "Compute flowid for locally-originated packets");

/* Default entropy to add to the hash calculation for the outbound connections*/
uint8_t mpath_entropy_key[MPATH_ENTROPY_KEY_LEN] = {
	0x6d, 0x5a, 0x56, 0xda, 0x25, 0x5b, 0x0e, 0xc2,
	0x41, 0x67, 0x25, 0x3d, 0x43, 0xa3, 0x8f, 0xb0,
	0xd0, 0xca, 0x2b, 0xcb, 0xae, 0x7b, 0x30, 0xb4,
	0x77, 0xcb, 0x2d, 0xa3, 0x80, 0x30, 0xf2, 0x0c,
	0x6a, 0x42, 0xb7, 0x3b, 0xbe, 0xac, 0x01, 0xfa,
};


/*
 * Tries to add @rnd_add nhop to the existing set of nhops (@nh_orig) for the
 * prefix specified by @rt.
 *
 * Return 0 ans consumes rt / rnd_add nhop references. @rc gets populated
 *   with the operation result.
 * Otherwise errno is returned.
 *
 * caller responsibility is to unlock/free rt and
 *  rt->rt_nhop.
 */
int
add_route_mpath(struct rib_head *rnh, struct rt_addrinfo *info,
    struct rtentry *rt, struct route_nhop_data *rnd_add,
    struct route_nhop_data *rnd_orig, struct rib_cmd_info *rc)
{
	RIB_RLOCK_TRACKER;
	struct route_nhop_data rnd_new;
	int error = 0;

	/*
	 * It is possible that multiple rtsock speakers will try to update
	 * the same route simultaneously. Reduce the chance of failing the
	 * request by retrying the cycle multiple times.
	 */
	for (int i = 0; i < RIB_MAX_RETRIES; i++) {
		error = nhgrp_get_addition_group(rnh, rnd_orig, rnd_add,
		    &rnd_new);
		if (error != 0) {
			if (error != EAGAIN)
				break;

			/*
			 * Group creation failed, most probably because
			 * @rnd_orig data got scheduled for deletion.
			 * Refresh @rnd_orig data and retry.
			 */
			RIB_RLOCK(rnh);
			lookup_prefix_rt(rnh, rt, rnd_orig);
			RIB_RUNLOCK(rnh);
			continue;
		}

		error = change_route_conditional(rnh, rt, rnd_orig, &rnd_new, rc);
		if (error != EAGAIN)
			break;
		RTSTAT_INC(rts_add_retry);
	}

	if (V_fib_hash_outbound == 0 && error == 0 &&
	    NH_IS_NHGRP(rc->rc_nh_new)) {
		/*
		 * First multipath route got installed. Enable local
		 * outbound connections hashing.
		 */
		if (bootverbose)
			printf("FIB: enabled flowid calculation for locally-originated packets\n");
		V_fib_hash_outbound = 1;
	}

	return (error);
}
