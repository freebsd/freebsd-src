/*-
 * Copyright (c) 2017 Dell EMC Isilon
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
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
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

#include <sys/param.h>
#include <net/if.h>
#include <net/if_mib.h>
#include <netinet/in.h>
#include <netinet/ip6.h>

#include <bsnmp/snmpmod.h>
#include <bsnmp/snmp_mibII.h>

#include "ipv6.h"

int
op_ipv6IfTable(struct snmp_context *ctx __unused, struct snmp_value *value,
    u_int sub, u_int iidx __unused, enum snmp_op op)
{
	struct mibif *ifp = NULL;
	asn_subid_t which;
	int ret;

	switch (op) {
#if 0
	case SNMP_OP_GETNEXT:
		if ((ifp = NEXT_OBJECT_INT(&mibif_list, &value->var, sub)) == NULL)
			return (SNMP_ERR_NOSUCHNAME);
		value->var.len = sub + 1;
		value->var.subs[sub] = ifp->index;
		break;
	case SNMP_OP_GET:
		if (value->var.len - sub != 1)
			return (SNMP_ERR_NOSUCHNAME);
		if ((ifp = mib_find_if(value->var.subs[sub])) == NULL)
			return (SNMP_ERR_NOSUCHNAME);
		break;
#endif
	case SNMP_OP_SET:
	case SNMP_OP_COMMIT:
	case SNMP_OP_ROLLBACK:
		return (SNMP_ERR_NOERROR);
	default:
		return (SNMP_ERR_RES_UNAVAIL);
	}

	which = value->var.subs[sub - 1];

#if 0
	/* XXX (ngie): expose mib_fetch_ifmib */
	if (ifp->mibtick < this_tick)
		(void)mib_fetch_ifmib(ifp);
#endif

	switch (which) {
	case LEAF_ipv6IfDescr:
		ret = string_get(value, ifp->descr, -1);
		break;
	case LEAF_ipv6IfLowerLayer:
		/*
		 * XXX (thor): return nullOID until the proper way is figured
		 * out. For now, use `oid_zeroDotZero`.
		 */
		oid_get(value, &oid_zeroDotZero);
		break;
	case LEAF_ipv6IfReasmMaxSize:
		value->v.uint32 = IPV6_MAXPACKET;
		break;
	case LEAF_ipv6IfEffectiveMtu:
		value->v.integer = ifp->mib.ifmd_data.ifi_mtu;
		break;
	case LEAF_ipv6IfIdentifier:
		/* XXX (ngie): implement this */
		string_get(value, "", 0);
		break;
	case LEAF_ipv6IfIdentifierLength:
		/* XXX (ngie): get the length of LEAF_ipv6IfIdentifier */
		value->v.integer = 0;
		break;
	case LEAF_ipv6IfPhysicalAddress:
		ret = string_get(value, ifp->physaddr,
		    ifp->physaddrlen);
		break;
	case LEAF_ipv6IfAdminStatus:
		value->v.integer =
		    (ifp->mib.ifmd_flags & IFF_UP) ? 1 : 2;
		break;
	case LEAF_ipv6IfOperStatus:
		if ((ifp->mib.ifmd_flags & IFF_RUNNING) != 0) {
			if (ifp->mib.ifmd_data.ifi_link_state != LINK_STATE_UP)
				value->v.integer = 5;   /* state dormant */
			else
				value->v.integer = 1;   /* state up */
		} else
			value->v.integer = 2;   /* state down */
		break;
#if 0
	/* XXX (ngie): export from mib-II to somewhere more sensible. */
	case LEAF_ipv6IfLastChange:
		value->v.uint32 =
		    ticks_get_timeval(&ifp->mib.ifmd_data.ifi_lastchange);
		break;
#endif
	default:
		return (SNMP_ERR_RES_UNAVAIL);
	}

	return (SNMP_ERR_NOERROR);
}
