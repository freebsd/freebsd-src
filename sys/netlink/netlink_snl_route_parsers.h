/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Alexander V. Chernikov <melifaro@FreeBSD.org>
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
#ifndef	_NETLINK_NETLINK_SNL_ROUTE_PARSERS_H_
#define	_NETLINK_NETLINK_SNL_ROUTE_PARSERS_H_

#include <netlink/netlink_snl.h>
#include <netlink/netlink_snl_route.h>
#include <netlink/route/nexthop.h>

/* TODO: this file should be generated automatically */

static inline void
finalize_sockaddr(struct sockaddr *sa, uint32_t ifindex)
{
	if (sa != NULL && sa->sa_family == AF_INET6) {
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)(void *)sa;

		if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr))
			sin6->sin6_scope_id = ifindex;
	}
}

/* RTM_<NEW|DEL|GET>ROUTE message parser */

struct rta_mpath_nh {
	struct sockaddr	*gw;
	uint32_t	ifindex;
	uint8_t		rtnh_flags;
	uint8_t		rtnh_weight;
	uint32_t	rtax_mtu;
	uint32_t	rta_metric;
	uint32_t	rta_rtflags;
	uint32_t	rta_expire;
};

#define	_IN(_field)	offsetof(struct rtnexthop, _field)
#define	_OUT(_field)	offsetof(struct rta_mpath_nh, _field)
static const struct snl_attr_parser _nla_p_mp_nh_metrics[] = {
	{ .type = NL_RTAX_MTU, .off = _OUT(rtax_mtu), .cb = snl_attr_get_uint32 },
};
SNL_DECLARE_ATTR_PARSER(_metrics_mp_nh_parser, _nla_p_mp_nh_metrics);

static const struct snl_attr_parser _nla_p_mp_nh[] = {
	{ .type = NL_RTA_GATEWAY, .off = _OUT(gw), .cb = snl_attr_get_ip },
	{ .type = NL_RTA_PRIORITY, .off = _OUT(rta_metric), .cb = snl_attr_get_uint32 },
	{ .type = NL_RTA_METRICS, .arg = &_metrics_mp_nh_parser, .cb = snl_attr_get_nested },
	{ .type = NL_RTA_RTFLAGS, .off = _OUT(rta_rtflags), .cb = snl_attr_get_uint32 },
	{ .type = NL_RTA_VIA, .off = _OUT(gw), .cb = snl_attr_get_ipvia },
	{ .type = NL_RTA_EXPIRES, .off = _OUT(rta_expire), .cb = snl_attr_get_uint32 },
};

static const struct snl_field_parser _fp_p_mp_nh[] = {
	{ .off_in = _IN(rtnh_flags), .off_out = _OUT(rtnh_flags), .cb = snl_field_get_uint8 },
	{ .off_in = _IN(rtnh_hops), .off_out = _OUT(rtnh_weight), .cb = snl_field_get_uint8 },
	{ .off_in = _IN(rtnh_ifindex), .off_out = _OUT(ifindex), .cb = snl_field_get_uint32 },
};

static inline bool
_cb_p_mp_nh(struct snl_state *ss __unused, void *_target)
{
	struct rta_mpath_nh *target = (struct rta_mpath_nh *)_target;

	finalize_sockaddr(target->gw, target->ifindex);
	return (true);
}
#undef _IN
#undef _OUT
SNL_DECLARE_PARSER_EXT(_mpath_nh_parser, sizeof(struct rtnexthop),
		sizeof(struct rta_mpath_nh), _fp_p_mp_nh, _nla_p_mp_nh,
		_cb_p_mp_nh);

struct rta_mpath {
	uint32_t num_nhops;
	struct rta_mpath_nh **nhops;
};

static bool
nlattr_get_multipath(struct snl_state *ss, struct nlattr *nla,
    const void *arg __unused, void *target)
{
	uint32_t start_size = 4;

	while (start_size < NLA_DATA_LEN(nla) / sizeof(struct rtnexthop))
		start_size *= 2;

	return (snl_attr_get_parray_sz(ss, nla, start_size, &_mpath_nh_parser, target));
}

struct snl_parsed_route {
	struct sockaddr		*rta_dst;
	struct sockaddr		*rta_gw;
	struct sockaddr		*rta_pref_src;
	struct nlattr		*rta_metrics;
	struct rta_mpath	rta_multipath;
	uint32_t		rta_oif;
	uint32_t		rta_expire;
	uint32_t		rta_table;
	uint32_t		rta_knh_id;
	uint32_t		rta_nh_id;
	uint32_t		rta_rtflags;
	uint32_t		rtax_mtu;
	uint32_t		rtax_weight;
	uint32_t		rta_metric;
	uint8_t			rtm_family;
	uint8_t			rtm_type;
	uint8_t			rtm_protocol;
	uint8_t			rtm_dst_len;
};

#define	_IN(_field)	offsetof(struct rtmsg, _field)
#define	_OUT(_field)	offsetof(struct snl_parsed_route, _field)
static const struct snl_attr_parser _nla_p_rtmetrics[] = {
	{ .type = NL_RTAX_MTU, .off = _OUT(rtax_mtu), .cb = snl_attr_get_uint32 },
};
SNL_DECLARE_ATTR_PARSER(_metrics_parser, _nla_p_rtmetrics);

static const struct snl_attr_parser _nla_p_route[] = {
	{ .type = NL_RTA_DST, .off = _OUT(rta_dst), .cb = snl_attr_get_ip },
	{ .type = NL_RTA_OIF, .off = _OUT(rta_oif), .cb = snl_attr_get_uint32 },
	{ .type = NL_RTA_GATEWAY, .off = _OUT(rta_gw), .cb = snl_attr_get_ip },
	{ .type = NL_RTA_PRIORITY, .off = _OUT(rta_metric), .cb = snl_attr_get_uint32 },
	{ .type = NL_RTA_PREFSRC, .off = _OUT(rta_pref_src), .cb = snl_attr_get_ip },
	{ .type = NL_RTA_METRICS, .arg = &_metrics_parser, .cb = snl_attr_get_nested },
	{ .type = NL_RTA_MULTIPATH, .off = _OUT(rta_multipath), .cb = nlattr_get_multipath },
	{ .type = NL_RTA_KNH_ID, .off = _OUT(rta_knh_id), .cb = snl_attr_get_uint32 },
	{ .type = NL_RTA_WEIGHT, .off = _OUT(rtax_weight), .cb = snl_attr_get_uint32 },
	{ .type = NL_RTA_RTFLAGS, .off = _OUT(rta_rtflags), .cb = snl_attr_get_uint32 },
	{ .type = NL_RTA_TABLE, .off = _OUT(rta_table), .cb = snl_attr_get_uint32 },
	{ .type = NL_RTA_VIA, .off = _OUT(rta_gw), .cb = snl_attr_get_ipvia },
	{ .type = NL_RTA_EXPIRES, .off = _OUT(rta_expire), .cb = snl_attr_get_uint32 },
	{ .type = NL_RTA_NH_ID, .off = _OUT(rta_nh_id), .cb = snl_attr_get_uint32 },
};

static const struct snl_field_parser _fp_p_route[] = {
	{.off_in = _IN(rtm_family), .off_out = _OUT(rtm_family), .cb = snl_field_get_uint8 },
	{.off_in = _IN(rtm_type), .off_out = _OUT(rtm_type), .cb = snl_field_get_uint8 },
	{.off_in = _IN(rtm_protocol), .off_out = _OUT(rtm_protocol), .cb = snl_field_get_uint8 },
	{.off_in = _IN(rtm_dst_len), .off_out = _OUT(rtm_dst_len), .cb = snl_field_get_uint8 },
};

static inline bool
_cb_p_route(struct snl_state *ss __unused, void *_target)
{
	struct snl_parsed_route *target = (struct snl_parsed_route *)_target;

	finalize_sockaddr(target->rta_dst, target->rta_oif);
	finalize_sockaddr(target->rta_gw, target->rta_oif);
	return (true);
}
#undef _IN
#undef _OUT
SNL_DECLARE_PARSER_EXT(snl_rtm_route_parser, sizeof(struct rtmsg),
		sizeof(struct snl_parsed_route), _fp_p_route, _nla_p_route,
		_cb_p_route);

/* RTM_<NEW|DEL|GET>LINK message parser */
struct snl_parsed_vf_extension {
	char			*name;
	struct nlattr		*data;
};

#define	_OUT(_field)	offsetof(struct snl_parsed_vf_extension, _field)
static const struct snl_attr_parser _nla_p_vf_extension[] = {
	{ .type = IFLAF_VF_EXT_NAME, .off = _OUT(name),
	    .cb = snl_attr_dup_string },
	{ .type = IFLAF_VF_EXT_DATA, .off = _OUT(data),
	    .cb = snl_attr_dup_nla },
};
#undef _OUT
SNL_DECLARE_ATTR_PARSER_EXT(_vf_extension_parser,
	    sizeof(struct snl_parsed_vf_extension), _nla_p_vf_extension, NULL);

struct snl_parsed_vf {
	uint32_t		index;
	uint32_t		vlan_count;
	uint32_t		vlan_limit;
	uint32_t		num_queues;
	uint16_t		vlan;
	uint8_t			configured;
	uint8_t			initialized;
	uint8_t			vlan_mode;
	uint8_t			allow_set_mac;
	uint8_t			allow_set_vlan;
	uint8_t			mac_anti_spoof;
	uint8_t			allow_promisc;
	uint8_t			traffic_enabled;
	uint8_t			mdd_blocked;
	uint8_t			quarantined;
	uint8_t			link_state_policy;
	char			*api_version;
	struct nlattr		*mac;
	struct snl_parray	extensions;
};

#define	_OUT(_field)	offsetof(struct snl_parsed_vf, _field)
static const struct snl_attr_parser _nla_p_vf[] = {
	{ .type = IFLAF_VF_INDEX, .off = _OUT(index),
	    .cb = snl_attr_get_uint32 },
	{ .type = IFLAF_VF_CONFIGURED, .off = _OUT(configured),
	    .cb = snl_attr_get_uint8 },
	{ .type = IFLAF_VF_INITIALIZED, .off = _OUT(initialized),
	    .cb = snl_attr_get_uint8 },
	{ .type = IFLAF_VF_MAC, .off = _OUT(mac), .cb = snl_attr_dup_nla },
	{ .type = IFLAF_VF_VLAN_MODE, .off = _OUT(vlan_mode),
	    .cb = snl_attr_get_uint8 },
	{ .type = IFLAF_VF_VLAN, .off = _OUT(vlan),
	    .cb = snl_attr_get_uint16 },
	{ .type = IFLAF_VF_VLAN_COUNT, .off = _OUT(vlan_count),
	    .cb = snl_attr_get_uint32 },
	{ .type = IFLAF_VF_VLAN_LIMIT, .off = _OUT(vlan_limit),
	    .cb = snl_attr_get_uint32 },
	{ .type = IFLAF_VF_NUM_QUEUES, .off = _OUT(num_queues),
	    .cb = snl_attr_get_uint32 },
	{ .type = IFLAF_VF_ALLOW_SET_MAC, .off = _OUT(allow_set_mac),
	    .cb = snl_attr_get_uint8 },
	{ .type = IFLAF_VF_ALLOW_SET_VLAN, .off = _OUT(allow_set_vlan),
	    .cb = snl_attr_get_uint8 },
	{ .type = IFLAF_VF_MAC_ANTI_SPOOF, .off = _OUT(mac_anti_spoof),
	    .cb = snl_attr_get_uint8 },
	{ .type = IFLAF_VF_ALLOW_PROMISC, .off = _OUT(allow_promisc),
	    .cb = snl_attr_get_uint8 },
	{ .type = IFLAF_VF_TRAFFIC_ENABLED, .off = _OUT(traffic_enabled),
	    .cb = snl_attr_get_uint8 },
	{ .type = IFLAF_VF_MDD_BLOCKED, .off = _OUT(mdd_blocked),
	    .cb = snl_attr_get_uint8 },
	{ .type = IFLAF_VF_QUARANTINED, .off = _OUT(quarantined),
	    .cb = snl_attr_get_uint8 },
	{ .type = IFLAF_VF_API_VERSION, .off = _OUT(api_version),
	    .cb = snl_attr_dup_string },
	{ .type = IFLAF_VF_LINK_STATE_POLICY,
	    .off = _OUT(link_state_policy), .cb = snl_attr_get_uint8 },
	{ .type = IFLAF_VF_EXTENSIONS, .off = _OUT(extensions),
	    .arg = &_vf_extension_parser, .cb = snl_attr_get_parray },
};
#undef _OUT
SNL_DECLARE_ATTR_PARSER_EXT(_vf_parser, sizeof(struct snl_parsed_vf),
	    _nla_p_vf, NULL);

struct snl_parsed_vf_status {
	uint64_t		pf_link_speed;
	uint32_t		version;
	uint32_t		error;
	uint8_t			pf_link_state;
	struct snl_parray	vfs;
};

#define	_OUT(_field)	offsetof(struct snl_parsed_vf_status, _field)
static const struct snl_attr_parser _nla_p_vf_status[] = {
	{ .type = IFLAF_VFS_VERSION, .off = _OUT(version),
	    .cb = snl_attr_get_uint32 },
	{ .type = IFLAF_VFS_ERROR, .off = _OUT(error),
	    .cb = snl_attr_get_uint32 },
	{ .type = IFLAF_VFS_PF_LINK_STATE, .off = _OUT(pf_link_state),
	    .cb = snl_attr_get_uint8 },
	{ .type = IFLAF_VFS_PF_LINK_SPEED, .off = _OUT(pf_link_speed),
	    .cb = snl_attr_get_uint64 },
	{ .type = IFLAF_VFS_LIST, .off = _OUT(vfs),
	    .arg = &_vf_parser, .cb = snl_attr_get_parray },
};
#undef _OUT
SNL_DECLARE_ATTR_PARSER_EXT(_vf_status_parser,
	    sizeof(struct snl_parsed_vf_status), _nla_p_vf_status, NULL);

struct snl_parsed_link {
	uint32_t			ifi_index;
	uint32_t			ifi_flags;
	uint32_t			ifi_change;
	uint16_t			ifi_type;
	uint8_t				ifla_operstate;
	uint8_t				ifla_carrier;
	uint32_t			ifla_mtu;
	char				*ifla_ifname;
	struct nlattr			*ifla_address;
	struct nlattr			*ifla_broadcast;
	char				*ifla_ifalias;
	uint32_t			ifla_promiscuity;
	uint32_t			ifla_num_vf;
	struct rtnl_link_stats64	*ifla_stats64;
	struct nlattr			*iflaf_orig_hwaddr;
	struct snl_attr_bitset		iflaf_caps;
	struct snl_parsed_vf_status	iflaf_vf_status;
};

#define	_IN(_field)	offsetof(struct ifinfomsg, _field)
#define	_OUT(_field)	offsetof(struct snl_parsed_link, _field)
static const struct snl_attr_parser _nla_p_link_fbsd[] = {
	{ .type = IFLAF_ORIG_HWADDR, .off = _OUT(iflaf_orig_hwaddr), .cb = snl_attr_dup_nla },
	{ .type = IFLAF_CAPS, .off = _OUT(iflaf_caps), .cb = snl_attr_get_bitset_c },
	{ .type = IFLAF_VF_STATUS, .off = _OUT(iflaf_vf_status),
	    .arg = &_vf_status_parser, .cb = snl_attr_get_nested },
};
SNL_DECLARE_ATTR_PARSER(_link_fbsd_parser, _nla_p_link_fbsd);

static const struct snl_attr_parser _nla_p_link[] = {
	{ .type = IFLA_ADDRESS, .off = _OUT(ifla_address), .cb = snl_attr_dup_nla },
	{ .type = IFLA_BROADCAST, .off = _OUT(ifla_broadcast), .cb = snl_attr_dup_nla },
	{ .type = IFLA_IFNAME, .off = _OUT(ifla_ifname), .cb = snl_attr_dup_string },
	{ .type = IFLA_MTU, .off = _OUT(ifla_mtu), .cb = snl_attr_get_uint32 },
	{ .type = IFLA_OPERSTATE, .off = _OUT(ifla_operstate), .cb = snl_attr_get_uint8 },
	{ .type = IFLA_IFALIAS, .off = _OUT(ifla_ifalias), .cb = snl_attr_dup_string },
	{ .type = IFLA_NUM_VF, .off = _OUT(ifla_num_vf), .cb = snl_attr_get_uint32 },
	{ .type = IFLA_STATS64, .off = _OUT(ifla_stats64), .cb = snl_attr_dup_struct },
	{ .type = IFLA_PROMISCUITY, .off = _OUT(ifla_promiscuity), .cb = snl_attr_get_uint32 },
	{ .type = IFLA_CARRIER, .off = _OUT(ifla_carrier), .cb = snl_attr_get_uint8 },
	{ .type = IFLA_FREEBSD, .arg = &_link_fbsd_parser, .cb = snl_attr_get_nested },
};
static const struct snl_field_parser _fp_p_link[] = {
	{.off_in = _IN(ifi_index), .off_out = _OUT(ifi_index), .cb = snl_field_get_uint32 },
	{.off_in = _IN(ifi_flags), .off_out = _OUT(ifi_flags), .cb = snl_field_get_uint32 },
	{.off_in = _IN(ifi_change), .off_out = _OUT(ifi_change), .cb = snl_field_get_uint32 },
	{.off_in = _IN(ifi_type), .off_out = _OUT(ifi_type), .cb = snl_field_get_uint16 },
};
#undef _IN
#undef _OUT
SNL_DECLARE_PARSER(snl_rtm_link_parser, struct ifinfomsg, _fp_p_link, _nla_p_link);

struct snl_parsed_link_simple {
	uint32_t		ifi_index;
	uint32_t		ifla_mtu;
	uint16_t		ifi_type;
	uint32_t		ifi_flags;
	char			*ifla_ifname;
};

#define	_IN(_field)	offsetof(struct ifinfomsg, _field)
#define	_OUT(_field)	offsetof(struct snl_parsed_link_simple, _field)
static struct snl_attr_parser _nla_p_link_s[] = {
	{ .type = IFLA_IFNAME, .off = _OUT(ifla_ifname), .cb = snl_attr_dup_string },
	{ .type = IFLA_MTU, .off = _OUT(ifla_mtu), .cb = snl_attr_get_uint32 },
};
static struct snl_field_parser _fp_p_link_s[] = {
	{.off_in = _IN(ifi_index), .off_out = _OUT(ifi_index), .cb = snl_field_get_uint32 },
	{.off_in = _IN(ifi_type), .off_out = _OUT(ifi_type), .cb = snl_field_get_uint16 },
	{.off_in = _IN(ifi_flags), .off_out = _OUT(ifi_flags), .cb = snl_field_get_uint32 },
};
#undef _IN
#undef _OUT
SNL_DECLARE_PARSER(snl_rtm_link_parser_simple, struct ifinfomsg, _fp_p_link_s, _nla_p_link_s);

struct snl_parsed_neigh {
	uint8_t		ndm_family;
	uint8_t		ndm_flags;
	uint16_t	ndm_state;
	uint32_t	nda_ifindex;
	uint32_t	nda_probes;
	uint32_t	ndaf_next_ts;
	struct sockaddr	*nda_dst;
	struct nlattr	*nda_lladdr;
};

#define	_IN(_field)	offsetof(struct ndmsg, _field)
#define	_OUT(_field)	offsetof(struct snl_parsed_neigh, _field)
static const struct snl_attr_parser _nla_p_neigh_fbsd[] = {
	{ .type = NDAF_NEXT_STATE_TS, .off = _OUT(ndaf_next_ts), .cb = snl_attr_get_uint32 },
};
SNL_DECLARE_ATTR_PARSER(_neigh_fbsd_parser, _nla_p_neigh_fbsd);

static struct snl_attr_parser _nla_p_neigh_s[] = {
	{ .type = NDA_DST, .off = _OUT(nda_dst), .cb = snl_attr_get_ip },
	{ .type = NDA_LLADDR , .off = _OUT(nda_lladdr), .cb = snl_attr_dup_nla },
	{ .type = NDA_PROBES, .off = _OUT(nda_probes), .cb = snl_attr_get_uint32 },
	{ .type = NDA_IFINDEX, .off = _OUT(nda_ifindex), .cb = snl_attr_get_uint32 },
	{ .type = NDA_FREEBSD, .arg = &_neigh_fbsd_parser, .cb = snl_attr_get_nested },
};
static struct snl_field_parser _fp_p_neigh_s[] = {
	{.off_in = _IN(ndm_family), .off_out = _OUT(ndm_family), .cb = snl_field_get_uint8 },
	{.off_in = _IN(ndm_flags), .off_out = _OUT(ndm_flags), .cb = snl_field_get_uint8 },
	{.off_in = _IN(ndm_state), .off_out = _OUT(ndm_state), .cb = snl_field_get_uint16 },
	{.off_in = _IN(ndm_ifindex), .off_out = _OUT(nda_ifindex), .cb = snl_field_get_uint32 },
};

static inline bool
_cb_p_neigh(struct snl_state *ss __unused, void *_target)
{
	struct snl_parsed_neigh *target = (struct snl_parsed_neigh *)_target;

	finalize_sockaddr(target->nda_dst, target->nda_ifindex);
	return (true);
}
#undef _IN
#undef _OUT
SNL_DECLARE_PARSER_EXT(snl_rtm_neigh_parser, sizeof(struct ndmsg),
		sizeof(struct snl_parsed_neigh), _fp_p_neigh_s, _nla_p_neigh_s,
		_cb_p_neigh);

struct snl_parsed_addr {
	uint8_t		ifa_family;
	uint8_t		ifa_prefixlen;
	uint32_t	ifa_index;
	struct sockaddr	*ifa_local;
	struct sockaddr	*ifa_address;
	struct sockaddr	*ifa_broadcast;
	char		*ifa_label;
	struct ifa_cacheinfo	*ifa_cacheinfo;
	uint32_t	ifaf_vhid;
	uint32_t	ifaf_flags;
};

#define	_IN(_field)	offsetof(struct ifaddrmsg, _field)
#define	_OUT(_field)	offsetof(struct snl_parsed_addr, _field)
static const struct snl_attr_parser _nla_p_addr_fbsd[] = {
	{ .type = IFAF_VHID, .off = _OUT(ifaf_vhid), .cb = snl_attr_get_uint32 },
	{ .type = IFAF_FLAGS, .off = _OUT(ifaf_flags), .cb = snl_attr_get_uint32 },
};
SNL_DECLARE_ATTR_PARSER(_addr_fbsd_parser, _nla_p_addr_fbsd);

static const struct snl_attr_parser _nla_p_addr_s[] = {
	{ .type = IFA_ADDRESS, .off = _OUT(ifa_address), .cb = snl_attr_get_ip },
	{ .type = IFA_LOCAL, .off = _OUT(ifa_local), .cb = snl_attr_get_ip },
	{ .type = IFA_LABEL, .off = _OUT(ifa_label), .cb = snl_attr_dup_string },
	{ .type = IFA_BROADCAST, .off = _OUT(ifa_broadcast), .cb = snl_attr_get_ip },
	{ .type = IFA_CACHEINFO, .off = _OUT(ifa_cacheinfo), .cb = snl_attr_dup_struct },
	{ .type = IFA_FREEBSD, .arg = &_addr_fbsd_parser, .cb = snl_attr_get_nested },
};
static const struct snl_field_parser _fp_p_addr_s[] = {
	{.off_in = _IN(ifa_family), .off_out = _OUT(ifa_family), .cb = snl_field_get_uint8 },
	{.off_in = _IN(ifa_prefixlen), .off_out = _OUT(ifa_prefixlen), .cb = snl_field_get_uint8 },
	{.off_in = _IN(ifa_index), .off_out = _OUT(ifa_index), .cb = snl_field_get_uint32 },
};

static inline bool
_cb_p_addr(struct snl_state *ss __unused, void *_target)
{
	struct snl_parsed_addr *target = (struct snl_parsed_addr *)_target;

	finalize_sockaddr(target->ifa_address, target->ifa_index);
	finalize_sockaddr(target->ifa_local, target->ifa_index);
	return (true);
}
#undef _IN
#undef _OUT
SNL_DECLARE_PARSER_EXT(snl_rtm_addr_parser, sizeof(struct ifaddrmsg),
		sizeof(struct snl_parsed_addr), _fp_p_addr_s, _nla_p_addr_s,
		_cb_p_addr);

struct snl_parsed_nhop {
	uint32_t	nha_id;
	uint8_t		nha_blackhole;
	uint8_t		nha_groups;
	uint8_t		nhaf_knhops;
	uint8_t		nhaf_family;
	uint32_t	nha_oif;
	struct sockaddr	*nha_gw;
	uint8_t		nh_family;
	uint8_t		nh_protocol;
	uint32_t	nhaf_table;
	uint32_t	nhaf_kid;
	uint32_t	nhaf_aif;
};

#define	_IN(_field)	offsetof(struct nhmsg, _field)
#define	_OUT(_field)	offsetof(struct snl_parsed_nhop, _field)
static struct snl_attr_parser _nla_p_nh_fbsd[] = {
	{ .type = NHAF_KNHOPS, .off = _OUT(nhaf_knhops), .cb = snl_attr_get_flag },
	{ .type = NHAF_TABLE, .off = _OUT(nhaf_table), .cb = snl_attr_get_uint32 },
	{ .type = NHAF_KID, .off = _OUT(nhaf_kid), .cb = snl_attr_get_uint32 },
	{ .type = NHAF_AIF, .off = _OUT(nhaf_aif), .cb = snl_attr_get_uint32 },
};
SNL_DECLARE_ATTR_PARSER(_nh_fbsd_parser, _nla_p_nh_fbsd);

static const struct snl_field_parser _fp_p_nh[] = {
	{ .off_in = _IN(nh_family), .off_out = _OUT(nh_family), .cb = snl_field_get_uint8 },
	{ .off_in = _IN(nh_protocol), .off_out = _OUT(nh_protocol), .cb = snl_field_get_uint8 },
};

static const struct snl_attr_parser _nla_p_nh[] = {
	{ .type = NHA_ID, .off = _OUT(nha_id), .cb = snl_attr_get_uint32 },
	{ .type = NHA_BLACKHOLE, .off = _OUT(nha_blackhole), .cb = snl_attr_get_flag },
	{ .type = NHA_OIF, .off = _OUT(nha_oif), .cb = snl_attr_get_uint32 },
	{ .type = NHA_GATEWAY, .off = _OUT(nha_gw), .cb = snl_attr_get_ip },
	{ .type = NHA_FREEBSD, .arg = &_nh_fbsd_parser, .cb = snl_attr_get_nested },
};

static inline bool
_cb_p_nh(struct snl_state *ss __unused, void *_target)
{
	struct snl_parsed_nhop *target = (struct snl_parsed_nhop *)_target;

	finalize_sockaddr(target->nha_gw, target->nha_oif);
	return (true);
}
#undef _IN
#undef _OUT
SNL_DECLARE_PARSER_EXT(snl_nhmsg_parser, sizeof(struct nhmsg),
		sizeof(struct snl_parsed_nhop), _fp_p_nh, _nla_p_nh, _cb_p_nh);

#endif
