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
op_ipv6IfTable(struct snmp_context *ctx, struct snmp_value *value,
    u_int sub, u_int iidx, enum snmp_op op)
{
	asn_subid_t which;

	switch (op) {
	case SNMP_OP_GETNEXT:
#if 0
		ip = NEXT_OBJECT_INT(&ipv6_interfaces, &value->var, sub);
		if (ip == NULL)
			return SNMP_ERR_NOSUCHNAME;
		value->var.len = sub + 1;
		value->var.subs[sub] = ip->index;
#endif
		break;
	case SNMP_OP_GET:
#if 0
		ip = FIND_OBJECT_INT(&ipv6_interfaces, &value->var, sub);
		if (ip == NULL)
			return (SNMP_ERR_NOSUCHNAME);
#endif
		break;
	case SNMP_OP_SET:
	case SNMP_OP_COMMIT:
	case SNMP_OP_ROLLBACK:
		return (SNMP_ERR_NOERROR);
	default:
		return (SNMP_ERR_RES_UNAVAIL);
	}

	which = value->var.subs[sub - 1];

	switch (which) {
	case LEAF_ipv6IfDescr:
#if 0
		string_get(value, ip->name, strlen(ip->name));
#endif
		break;
	case LEAF_ipv6IfLowerLayer:
		/*
		 * TODO: return nullOID until the proper way is figured out.
		 * For now, use `oid_zeroDotZero`.
		 */
		oid_get(value, &oid_zeroDotZero);
		break;
	case LEAF_ipv6IfReasmMaxSize:
		value->v.uint32 = IPV6_MAXPACKET;
		break;
	case LEAF_ipv6IfEffectiveMtu:
	{
#if 0
		struct ifreq ifr;
		int s;

		memset(&ifr, 0, sizeof(ifr));
		ifr.ifr_addr.sa_family = AF_INET6;
		strncpy(ifr.ifr_name, ip->name, sizeof(ifr.ifr_name));

		if ((s = socket(ifr.ifr_addr.sa_family, SOCK_DGRAM, 0)) < 0)
			return (SNMP_ERR_RES_UNAVAIL);

		if (ioctl(s, SIOCGIFMTU, (caddr_t) &ifr) != -1)
			value->v.uint32 = ifr.ifr_mtu;

		close(s);
#endif
		break;
	}
	case LEAF_ipv6IfIdentifier:
		string_get(value, "", 0);
		break;
	case LEAF_ipv6IfIdentifierLength:
		/* XXX (ngie): get the length of LEAF_ipv6IfIdentifier */
		value->v.integer = 0;
		break;
	case LEAF_ipv6IfPhysicalAddress:
	{
#if 0
 		get_physaddr();
		if (tmp == NULL)
			string_get(value, "", 0);
		else
			string_get(value, tmp, strlen(tmp));
#endif
		break;
	}
	case LEAF_ipv6IfAdminStatus:
	{
#if 0
		struct ifaddrs *ifap, *ifa;

		if (getifaddrs(&ifap) == -1) {
			value->v.integer = 4; /* Unknown */
			break;
		}

		value->v.integer = 2;
		for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
			if (strcmp(ifa->ifa_name, ip->name) != 0)
				continue;
			if (ifa->ifa_addr->sa_family != AF_INET6)
				continue;
			value->v.integer =
			    (ifa->ifa_flags & IFF_RUNNING) ? 1 : 2;
			break;
		}
		freeifaddrs(ifap);
#endif
		break;
	}
	case LEAF_ipv6IfOperStatus:
	{
#if 0
		struct ifaddrs *ifap, *ifa;
		if (getifaddrs(&ifap) == -1) {
			value->v.integer = 4; /* Unknown */
			break;
		}

		value->v.integer = 2;

		for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
			if (strcmp(ifa->ifa_name, ip->name) != 0)
				continue;
			if (ifa->ifa_addr->sa_family != AF_INET6)
				continue;
			value->v.integer = (ifa->ifa_flags & IFF_UP) ? 1 : 2;
			break;
		}
		freeifaddrs(ifap);
#endif
		break;
	}
	case LEAF_ipv6IfLastChange:
	{
#if 0
		/*
		 * XXX (ngie): not checking for error code from
		 * gettimeofday(2).
		 */
		struct timeval lastchange, now;
		struct ifmibdata ifmd;

		if (if_getifmibdata(ip->index, &ifmd) != 0) {
			value->v.uint32 = 0;
			break;
		}
		lastchange = ifmd.ifmd_data.ifi_lastchange;

		gettimeofday(&now, (struct timezone*)NULL);
		value->v.uint32 =
		    (uint32_t)((now.tv_sec - lastchange.tv_sec) * 100);
		value->v.uint32 +=
		    (uint32_t)((now.tv_usec - lastchange.tv_usec) / 10000);
#endif
		break;
	}
	default:
		return (SNMP_ERR_RES_UNAVAIL);
	}

	return (SNMP_ERR_NOERROR);
}
