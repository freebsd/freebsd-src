/*
 * Copyright (c) 2001-2003
 *	Fraunhofer Institute for Open Communication Systems (FhG Fokus).
 *	All rights reserved.
 *
 * Author: Harti Brandt <harti@freebsd.org>
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
 *
 * $Begemot: bsnmp/snmp_mibII/mibII_route.c,v 1.5 2004/08/06 08:47:04 brandt Exp $
 *
 * Routing table
 */
#include "mibII.h"
#include "mibII_oid.h"

struct sroute {
	TAILQ_ENTRY(sroute) link;
	struct asn_oid	index;
	u_int		ifindex;
	u_int		type;
	u_int		proto;
};
static TAILQ_HEAD(, sroute) sroute_list = TAILQ_HEAD_INITIALIZER(sroute_list);

static uint32_t route_tick;
static u_int route_total;

static int
fetch_route(void)
{
	u_char *rtab, *next;
	size_t len;
	struct sroute *r;
	struct rt_msghdr *rtm;
	struct sockaddr *addrs[RTAX_MAX];
	struct sockaddr_in *sa, *gw;
	struct in_addr mask, nhop;
	in_addr_t ha;
	struct mibif *ifp;

	while ((r = TAILQ_FIRST(&sroute_list)) != NULL) {
		TAILQ_REMOVE(&sroute_list, r, link);
		free(r);
	}
	route_total = 0;

	if ((rtab = mib_fetch_rtab(AF_INET, NET_RT_DUMP, 0, &len)) == NULL)
		return (-1);

	next = rtab;
	for (next = rtab; next < rtab + len; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)(void *)next;
		if (rtm->rtm_type != RTM_GET ||
		    !(rtm->rtm_flags & RTF_UP))
			continue;
		mib_extract_addrs(rtm->rtm_addrs, (u_char *)(rtm + 1), addrs);

		if (addrs[RTAX_DST] == NULL || addrs[RTAX_GATEWAY] == NULL ||
		    addrs[RTAX_DST]->sa_family != AF_INET)
			continue;

		sa = (struct sockaddr_in *)(void *)addrs[RTAX_DST];

		if (rtm->rtm_flags & RTF_HOST) {
			mask.s_addr = 0xffffffff;
		} else {
			if (addrs[RTAX_NETMASK] == NULL ||
			    addrs[RTAX_NETMASK]->sa_len == 0)
				mask.s_addr = 0;
			else
				mask = ((struct sockaddr_in *)(void *)
				    addrs[RTAX_NETMASK])->sin_addr;
		}
		if (addrs[RTAX_GATEWAY] == NULL) {
			nhop.s_addr = 0;
		} else if (rtm->rtm_flags & RTF_LLINFO) {
			nhop = sa->sin_addr;
		} else {
			gw = (struct sockaddr_in *)(void *)addrs[RTAX_GATEWAY];
			if (gw->sin_family != AF_INET)
				continue;
			nhop = gw->sin_addr;
		}
		if ((ifp = mib_find_if_sys(rtm->rtm_index)) == NULL) {
			mib_iflist_bad = 1;
			continue;
		}

		if ((r = malloc(sizeof(*r))) == NULL) {
			syslog(LOG_ERR, "%m");
			continue;
		}

		route_total++;

		r->index.len = 13;
		ha = ntohl(sa->sin_addr.s_addr);
		r->index.subs[0] = (ha >> 24) & 0xff;
		r->index.subs[1] = (ha >> 16) & 0xff;
		r->index.subs[2] = (ha >>  8) & 0xff;
		r->index.subs[3] = (ha >>  0) & 0xff;
		ha = ntohl(mask.s_addr);
		r->index.subs[4] = (ha >> 24) & 0xff;
		r->index.subs[5] = (ha >> 16) & 0xff;
		r->index.subs[6] = (ha >>  8) & 0xff;
		r->index.subs[7] = (ha >>  0) & 0xff;

		r->index.subs[8] = 0;

		ha = ntohl(nhop.s_addr);
		r->index.subs[9] = (ha >> 24) & 0xff;
		r->index.subs[10] = (ha >> 16) & 0xff;
		r->index.subs[11] = (ha >>  8) & 0xff;
		r->index.subs[12] = (ha >>  0) & 0xff;

		r->ifindex = ifp->index;

		r->type = (rtm->rtm_flags & RTF_LLINFO) ? 3 :
		    (rtm->rtm_flags & RTF_REJECT) ? 2 : 4;

		/* cannot really know, what protocol it runs */
		r->proto = (rtm->rtm_flags & RTF_LOCAL) ? 2 :
		    (rtm->rtm_flags & RTF_STATIC) ? 3 :
		    (rtm->rtm_flags & RTF_DYNAMIC) ? 4 : 10;

		INSERT_OBJECT_OID(r, &sroute_list);
	}

	free(rtab);
	route_tick = get_ticks();

	return (0);
}

/*
 * Table
 */
int
op_route_table(struct snmp_context *ctx __unused, struct snmp_value *value,
    u_int sub, u_int iidx __unused, enum snmp_op op)
{
	static struct sroute *r;

	if (route_tick < this_tick)
		if (fetch_route() == -1)
			return (SNMP_ERR_GENERR);

	switch (op) {

	  case SNMP_OP_GETNEXT:
		if ((r = NEXT_OBJECT_OID(&sroute_list, &value->var, sub)) == NULL)
			return (SNMP_ERR_NOSUCHNAME);
		index_append(&value->var, sub, &r->index);
		break;

	  case SNMP_OP_GET:
		if ((r = FIND_OBJECT_OID(&sroute_list, &value->var, sub)) == NULL)
			return (SNMP_ERR_NOSUCHNAME);
		break;

	  case SNMP_OP_SET:
		if ((r = FIND_OBJECT_OID(&sroute_list, &value->var, sub)) == NULL)
			return (SNMP_ERR_NO_CREATION);
		return (SNMP_ERR_NOT_WRITEABLE);

	  case SNMP_OP_ROLLBACK:
	  case SNMP_OP_COMMIT:
		abort();

	  default:
		abort();
	}

	switch (value->var.subs[sub - 1]) {

	  case LEAF_ipCidrRouteDest:
		value->v.ipaddress[0] = r->index.subs[0];
		value->v.ipaddress[1] = r->index.subs[1];
		value->v.ipaddress[2] = r->index.subs[2];
		value->v.ipaddress[3] = r->index.subs[3];
		break;

	  case LEAF_ipCidrRouteMask:
		value->v.ipaddress[0] = r->index.subs[4];
		value->v.ipaddress[1] = r->index.subs[5];
		value->v.ipaddress[2] = r->index.subs[6];
		value->v.ipaddress[3] = r->index.subs[7];
		break;

	  case LEAF_ipCidrRouteTos:
		value->v.integer = r->index.subs[8];
		break;

	  case LEAF_ipCidrRouteNextHop:
		value->v.ipaddress[0] = r->index.subs[9];
		value->v.ipaddress[1] = r->index.subs[10];
		value->v.ipaddress[2] = r->index.subs[11];
		value->v.ipaddress[3] = r->index.subs[12];
		break;

	  case LEAF_ipCidrRouteIfIndex:
		value->v.integer = r->ifindex;
		break;

	  case LEAF_ipCidrRouteType:
		value->v.integer = r->type;
		break;

	  case LEAF_ipCidrRouteProto:
		value->v.integer = r->proto;
		break;

	  case LEAF_ipCidrRouteAge:
		value->v.integer = 0;
		break;

	  case LEAF_ipCidrRouteInfo:
		value->v.oid = oid_zeroDotZero;
		break;

	  case LEAF_ipCidrRouteNextHopAS:
		value->v.integer = 0;
		break;

	  case LEAF_ipCidrRouteMetric1:
	  case LEAF_ipCidrRouteMetric2:
	  case LEAF_ipCidrRouteMetric3:
	  case LEAF_ipCidrRouteMetric4:
	  case LEAF_ipCidrRouteMetric5:
		value->v.integer = -1;
		break;

	  case LEAF_ipCidrRouteStatus:
		value->v.integer = 1;
		break;
	}
	return (SNMP_ERR_NOERROR);
}

/*
 * scalars
 */
int
op_route(struct snmp_context *ctx __unused, struct snmp_value *value,
    u_int sub, u_int iidx __unused, enum snmp_op op)
{
	switch (op) {

	  case SNMP_OP_GETNEXT:
		abort();

	  case SNMP_OP_GET:
		break;

	  case SNMP_OP_SET:
		return (SNMP_ERR_NOT_WRITEABLE);

	  case SNMP_OP_ROLLBACK:
	  case SNMP_OP_COMMIT:
		abort();
	}

	if (route_tick < this_tick)
		if (fetch_route() == -1)
			return (SNMP_ERR_GENERR);

	switch (value->var.subs[sub - 1]) {

	  case LEAF_ipCidrRouteNumber:
		value->v.uint32 = route_total;
		break;

	}
	return (SNMP_ERR_NOERROR);
}
