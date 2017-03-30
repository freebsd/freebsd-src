/*
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
#include <sys/sysctl.h>
#include <bsnmp/snmpmod.h>

#include "ipv6.h"
#include "ipv6_sys.h"
#include "ipv6mib_oid.h"

static struct lmodule *module;

static const struct asn_oid oid_ipv6mib = OIDX_ipv6MIB;

uint32_t mib_ipv6_ipv6Interfaces;

uint64_t mib_ipv6_ipv6IfTableLastChange;

static u_int ipv6_reg;

int
op_ipv6MIBObjects(struct snmp_context *ctx __unused, struct snmp_value *value,
    u_int sub, u_int iidx __unused, enum snmp_op op)
{
	const char *namestr = NULL;
	int name[] = { CTL_NET, PF_INET6, IPPROTO_IPV6, 0 };
	asn_subid_t which;
	size_t resultsz;
	int result;

	result = 0;
	resultsz = sizeof(result);

	switch (op) {
	case SNMP_OP_GETNEXT:
	case SNMP_OP_GET:
		break;
	case SNMP_OP_SET:
		return (SNMP_ERR_NOT_WRITEABLE);
	case SNMP_OP_ROLLBACK:
	case SNMP_OP_COMMIT:
		return (SNMP_ERR_NOERROR);
	}

	which = value->var.subs[sub - 1];

	switch (which) {
	case LEAF_ipv6Forwarding:
		name[3] = IPV6CTL_FORWARDING;
		namestr = "IPV6CTL_FORWARDING";
		if (sysctl(name, nitems(name), &result, &resultsz, NULL,
		    0) < 0)
			return (SNMP_ERR_GENERR);
		if (result == 0)
			value->v.integer = ipv6Forwarding_notForwarding;
		else
			value->v.integer = ipv6Forwarding_forwarding;
		break;
	case LEAF_ipv6DefaultHopLimit:
		name[3] = IPV6CTL_DEFHLIM;
		namestr = "IPV6CTL_DEFHLIM";
		if (sysctl(name, nitems(name), &result, &resultsz, NULL,
		    0) < 0)
			return (SNMP_ERR_GENERR);
		value->v.integer = result;
		break;
	case LEAF_ipv6IfTableLastChange:
		mib_ipv6_refresh_interfaces();
		if (mib_ipv6_ipv6IfTableLastChange > start_tick)
			value->v.uint32 =
			    mib_ipv6_ipv6IfTableLastChange - start_tick;
		else
			value->v.uint32 = 0;
		break;
	case LEAF_ipv6Interfaces:
		mib_ipv6_refresh_interfaces();
		value->v.integer = mib_ipv6_ipv6Interfaces;
		break;
	default:
		return (SNMP_ERR_NOSUCHNAME);
	}

	return (SNMP_ERR_NOERROR);
}

static void
ipv6mib_start(void)
{

	ipv6_reg = or_register(&oid_ipv6mib,
	    "The (incomplete) MIB module for RFC 2465.", module);
}

static int
ipv6mib_init(struct lmodule *mod, int argc __unused, char *argv[] __unused)
{
	module = mod;

	return (0);
}

static int
ipv6mib_fini(void)
{

	or_unregister(ipv6_reg);

	return (0);
}

const struct snmp_module config = {
	"This module implements RFC 2465.",
	ipv6mib_init,
	ipv6mib_fini,
	NULL,
	NULL,
	NULL,
	ipv6mib_start,
	NULL,
	ipv6MIB_ctree,
	ipv6MIB_CTREE_SIZE,
	NULL
};
