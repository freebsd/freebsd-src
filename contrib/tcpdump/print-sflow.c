/*
 * Copyright (c) 1998-2007 The TCPDUMP project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code
 * distributions retain the above copyright notice and this paragraph
 * in its entirety, and (2) distributions including binary code include
 * the above copyright notice and this paragraph in its entirety in
 * the documentation or other materials provided with the distribution.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND
 * WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, WITHOUT
 * LIMITATION, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE.
 *
 * Original code by Carles Kishimoto <carles.kishimoto@gmail.com>
 *
 * Expansion and refactoring by Rick Jones <rick.jones2@hp.com>
 */

/* \summary: sFlow protocol printer */

/* specification: https://sflow.org/developers/specifications.php */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#define ND_LONGJMP_FROM_TCHECK
#include "netdissect.h"
#include "extract.h"
#include "addrtoname.h"

/*
 * sFlow datagram
 *
 * 0                   1                   2                   3
 * 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                     Sflow version (2,4,5)                     |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |               IP version (1 for IPv4 | 2 for IPv6)            |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                     IP Address AGENT (4 or 16 bytes)          |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                          Sub agent ID                         |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                      Datagram sequence number                 |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                      Switch uptime in ms                      |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                    num samples in datagram                    |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 */

struct sflow_datagram_t {
    nd_uint32_t version;
    nd_uint32_t ip_version;
    nd_ipv4	agent;
    nd_uint32_t	agent_id;
    nd_uint32_t	seqnum;
    nd_uint32_t	uptime;
    nd_uint32_t	samples;
};

struct sflow_v6_datagram_t {
    nd_uint32_t version;
    nd_uint32_t ip_version;
    nd_ipv6     agent;
    nd_uint32_t	agent_id;
    nd_uint32_t	seqnum;
    nd_uint32_t	uptime;
    nd_uint32_t	samples;
};

struct sflow_sample_header {
    nd_uint32_t	format;
    nd_uint32_t	len;
};

#define		SFLOW_FLOW_SAMPLE		1
#define		SFLOW_COUNTER_SAMPLE		2
#define		SFLOW_EXPANDED_FLOW_SAMPLE	3
#define		SFLOW_EXPANDED_COUNTER_SAMPLE	4

static const struct tok sflow_format_values[] = {
    { SFLOW_FLOW_SAMPLE, "flow sample" },
    { SFLOW_COUNTER_SAMPLE, "counter sample" },
    { SFLOW_EXPANDED_FLOW_SAMPLE, "expanded flow sample" },
    { SFLOW_EXPANDED_COUNTER_SAMPLE, "expanded counter sample" },
    { 0, NULL}
};

struct sflow_flow_sample_t {
    nd_uint32_t seqnum;
    nd_uint8_t  type;
    nd_uint24_t index;
    nd_uint32_t rate;
    nd_uint32_t pool;
    nd_uint32_t drops;
    nd_uint32_t in_interface;
    nd_uint32_t out_interface;
    nd_uint32_t records;

};

struct sflow_expanded_flow_sample_t {
    nd_uint32_t seqnum;
    nd_uint32_t type;
    nd_uint32_t index;
    nd_uint32_t rate;
    nd_uint32_t pool;
    nd_uint32_t drops;
    nd_uint32_t in_interface_format;
    nd_uint32_t in_interface_value;
    nd_uint32_t out_interface_format;
    nd_uint32_t out_interface_value;
    nd_uint32_t records;
};

#define	SFLOW_FLOW_RAW_PACKET			1
#define	SFLOW_FLOW_ETHERNET_FRAME		2
#define	SFLOW_FLOW_IPV4_DATA			3
#define	SFLOW_FLOW_IPV6_DATA			4
#define	SFLOW_FLOW_EXTENDED_SWITCH_DATA		1001
#define	SFLOW_FLOW_EXTENDED_ROUTER_DATA		1002
#define	SFLOW_FLOW_EXTENDED_GATEWAY_DATA	1003
#define	SFLOW_FLOW_EXTENDED_USER_DATA		1004
#define	SFLOW_FLOW_EXTENDED_URL_DATA		1005
#define	SFLOW_FLOW_EXTENDED_MPLS_DATA		1006
#define	SFLOW_FLOW_EXTENDED_NAT_DATA		1007
#define	SFLOW_FLOW_EXTENDED_MPLS_TUNNEL		1008
#define	SFLOW_FLOW_EXTENDED_MPLS_VC		1009
#define	SFLOW_FLOW_EXTENDED_MPLS_FEC		1010
#define	SFLOW_FLOW_EXTENDED_MPLS_LVP_FEC	1011
#define	SFLOW_FLOW_EXTENDED_VLAN_TUNNEL		1012

static const struct tok sflow_flow_type_values[] = {
    { SFLOW_FLOW_RAW_PACKET, "Raw packet"},
    { SFLOW_FLOW_ETHERNET_FRAME, "Ethernet frame"},
    { SFLOW_FLOW_IPV4_DATA, "IPv4 Data"},
    { SFLOW_FLOW_IPV6_DATA, "IPv6 Data"},
    { SFLOW_FLOW_EXTENDED_SWITCH_DATA, "Extended Switch data"},
    { SFLOW_FLOW_EXTENDED_ROUTER_DATA, "Extended Router data"},
    { SFLOW_FLOW_EXTENDED_GATEWAY_DATA, "Extended Gateway data"},
    { SFLOW_FLOW_EXTENDED_USER_DATA, "Extended User data"},
    { SFLOW_FLOW_EXTENDED_URL_DATA, "Extended URL data"},
    { SFLOW_FLOW_EXTENDED_MPLS_DATA, "Extended MPLS data"},
    { SFLOW_FLOW_EXTENDED_NAT_DATA, "Extended NAT data"},
    { SFLOW_FLOW_EXTENDED_MPLS_TUNNEL, "Extended MPLS tunnel"},
    { SFLOW_FLOW_EXTENDED_MPLS_VC, "Extended MPLS VC"},
    { SFLOW_FLOW_EXTENDED_MPLS_FEC, "Extended MPLS FEC"},
    { SFLOW_FLOW_EXTENDED_MPLS_LVP_FEC, "Extended MPLS LVP FEC"},
    { SFLOW_FLOW_EXTENDED_VLAN_TUNNEL, "Extended VLAN Tunnel"},
    { 0, NULL}
};

#define		SFLOW_HEADER_PROTOCOL_ETHERNET	1
#define		SFLOW_HEADER_PROTOCOL_IPV4	11
#define		SFLOW_HEADER_PROTOCOL_IPV6	12

static const struct tok sflow_flow_raw_protocol_values[] = {
    { SFLOW_HEADER_PROTOCOL_ETHERNET, "Ethernet"},
    { SFLOW_HEADER_PROTOCOL_IPV4, "IPv4"},
    { SFLOW_HEADER_PROTOCOL_IPV6, "IPv6"},
    { 0, NULL}
};

struct sflow_expanded_flow_raw_t {
    nd_uint32_t protocol;
    nd_uint32_t length;
    nd_uint32_t stripped_bytes;
    nd_uint32_t header_size;
};

struct sflow_ethernet_frame_t {
    nd_uint32_t length;
    nd_byte     src_mac[8];
    nd_byte     dst_mac[8];
    nd_uint32_t type;
};

struct sflow_extended_switch_data_t {
    nd_uint32_t src_vlan;
    nd_uint32_t src_pri;
    nd_uint32_t dst_vlan;
    nd_uint32_t dst_pri;
};

struct sflow_counter_record_t {
    nd_uint32_t    format;
    nd_uint32_t    length;
};

struct sflow_flow_record_t {
    nd_uint32_t    format;
    nd_uint32_t    length;
};

struct sflow_counter_sample_t {
    nd_uint32_t    seqnum;
    nd_uint8_t     type;
    nd_uint24_t    index;
    nd_uint32_t    records;
};

struct sflow_expanded_counter_sample_t {
    nd_uint32_t    seqnum;
    nd_uint32_t    type;
    nd_uint32_t    index;
    nd_uint32_t    records;
};

#define         SFLOW_COUNTER_GENERIC           1
#define         SFLOW_COUNTER_ETHERNET          2
#define         SFLOW_COUNTER_TOKEN_RING        3
#define         SFLOW_COUNTER_BASEVG            4
#define         SFLOW_COUNTER_VLAN              5
#define         SFLOW_COUNTER_PROCESSOR         1001

static const struct tok sflow_counter_type_values[] = {
    { SFLOW_COUNTER_GENERIC, "Generic counter"},
    { SFLOW_COUNTER_ETHERNET, "Ethernet counter"},
    { SFLOW_COUNTER_TOKEN_RING, "Token ring counter"},
    { SFLOW_COUNTER_BASEVG, "100 BaseVG counter"},
    { SFLOW_COUNTER_VLAN, "Vlan counter"},
    { SFLOW_COUNTER_PROCESSOR, "Processor counter"},
    { 0, NULL}
};

#define		SFLOW_IFACE_DIRECTION_UNKNOWN		0
#define		SFLOW_IFACE_DIRECTION_FULLDUPLEX	1
#define		SFLOW_IFACE_DIRECTION_HALFDUPLEX	2
#define		SFLOW_IFACE_DIRECTION_IN		3
#define		SFLOW_IFACE_DIRECTION_OUT		4

static const struct tok sflow_iface_direction_values[] = {
    { SFLOW_IFACE_DIRECTION_UNKNOWN, "unknown"},
    { SFLOW_IFACE_DIRECTION_FULLDUPLEX, "full-duplex"},
    { SFLOW_IFACE_DIRECTION_HALFDUPLEX, "half-duplex"},
    { SFLOW_IFACE_DIRECTION_IN, "in"},
    { SFLOW_IFACE_DIRECTION_OUT, "out"},
    { 0, NULL}
};

struct sflow_generic_counter_t {
    nd_uint32_t    ifindex;
    nd_uint32_t    iftype;
    nd_uint64_t    ifspeed;
    nd_uint32_t    ifdirection;
    nd_uint32_t    ifstatus;
    nd_uint64_t    ifinoctets;
    nd_uint32_t    ifinunicastpkts;
    nd_uint32_t    ifinmulticastpkts;
    nd_uint32_t    ifinbroadcastpkts;
    nd_uint32_t    ifindiscards;
    nd_uint32_t    ifinerrors;
    nd_uint32_t    ifinunkownprotos;
    nd_uint64_t    ifoutoctets;
    nd_uint32_t    ifoutunicastpkts;
    nd_uint32_t    ifoutmulticastpkts;
    nd_uint32_t    ifoutbroadcastpkts;
    nd_uint32_t    ifoutdiscards;
    nd_uint32_t    ifouterrors;
    nd_uint32_t    ifpromiscmode;
};

struct sflow_ethernet_counter_t {
    nd_uint32_t    alignerrors;
    nd_uint32_t    fcserrors;
    nd_uint32_t    single_collision_frames;
    nd_uint32_t    multiple_collision_frames;
    nd_uint32_t    test_errors;
    nd_uint32_t    deferred_transmissions;
    nd_uint32_t    late_collisions;
    nd_uint32_t    excessive_collisions;
    nd_uint32_t    mac_transmit_errors;
    nd_uint32_t    carrier_sense_errors;
    nd_uint32_t    frame_too_longs;
    nd_uint32_t    mac_receive_errors;
    nd_uint32_t    symbol_errors;
};

struct sflow_100basevg_counter_t {
    nd_uint32_t    in_highpriority_frames;
    nd_uint64_t    in_highpriority_octets;
    nd_uint32_t    in_normpriority_frames;
    nd_uint64_t    in_normpriority_octets;
    nd_uint32_t    in_ipmerrors;
    nd_uint32_t    in_oversized;
    nd_uint32_t    in_data_errors;
    nd_uint32_t    in_null_addressed_frames;
    nd_uint32_t    out_highpriority_frames;
    nd_uint64_t    out_highpriority_octets;
    nd_uint32_t    transitioninto_frames;
    nd_uint64_t    hc_in_highpriority_octets;
    nd_uint64_t    hc_in_normpriority_octets;
    nd_uint64_t    hc_out_highpriority_octets;
};

struct sflow_vlan_counter_t {
    nd_uint32_t    vlan_id;
    nd_uint64_t    octets;
    nd_uint32_t    unicast_pkt;
    nd_uint32_t    multicast_pkt;
    nd_uint32_t    broadcast_pkt;
    nd_uint32_t    discards;
};

static int
print_sflow_counter_generic(netdissect_options *ndo,
                            const u_char *pointer, u_int len)
{
    const struct sflow_generic_counter_t *sflow_gen_counter;

    if (len < sizeof(struct sflow_generic_counter_t))
	return 1;

    sflow_gen_counter = (const struct sflow_generic_counter_t *)pointer;
    ND_PRINT("\n\t      ifindex %u, iftype %u, ifspeed %" PRIu64 ", ifdirection %u (%s)",
	   GET_BE_U_4(sflow_gen_counter->ifindex),
	   GET_BE_U_4(sflow_gen_counter->iftype),
	   GET_BE_U_8(sflow_gen_counter->ifspeed),
	   GET_BE_U_4(sflow_gen_counter->ifdirection),
	   tok2str(sflow_iface_direction_values, "Unknown",
	   GET_BE_U_4(sflow_gen_counter->ifdirection)));
    ND_PRINT("\n\t      ifstatus %u, adminstatus: %s, operstatus: %s",
	   GET_BE_U_4(sflow_gen_counter->ifstatus),
	   GET_BE_U_4(sflow_gen_counter->ifstatus)&1 ? "up" : "down",
	   (GET_BE_U_4(sflow_gen_counter->ifstatus)>>1)&1 ? "up" : "down");
    ND_PRINT("\n\t      In octets %" PRIu64
	   ", unicast pkts %u, multicast pkts %u, broadcast pkts %u, discards %u",
	   GET_BE_U_8(sflow_gen_counter->ifinoctets),
	   GET_BE_U_4(sflow_gen_counter->ifinunicastpkts),
	   GET_BE_U_4(sflow_gen_counter->ifinmulticastpkts),
	   GET_BE_U_4(sflow_gen_counter->ifinbroadcastpkts),
	   GET_BE_U_4(sflow_gen_counter->ifindiscards));
    ND_PRINT("\n\t      In errors %u, unknown protos %u",
	   GET_BE_U_4(sflow_gen_counter->ifinerrors),
	   GET_BE_U_4(sflow_gen_counter->ifinunkownprotos));
    ND_PRINT("\n\t      Out octets %" PRIu64
	   ", unicast pkts %u, multicast pkts %u, broadcast pkts %u, discards %u",
	   GET_BE_U_8(sflow_gen_counter->ifoutoctets),
	   GET_BE_U_4(sflow_gen_counter->ifoutunicastpkts),
	   GET_BE_U_4(sflow_gen_counter->ifoutmulticastpkts),
	   GET_BE_U_4(sflow_gen_counter->ifoutbroadcastpkts),
	   GET_BE_U_4(sflow_gen_counter->ifoutdiscards));
    ND_PRINT("\n\t      Out errors %u, promisc mode %u",
	   GET_BE_U_4(sflow_gen_counter->ifouterrors),
	   GET_BE_U_4(sflow_gen_counter->ifpromiscmode));

    return 0;
}

static int
print_sflow_counter_ethernet(netdissect_options *ndo,
                             const u_char *pointer, u_int len)
{
    const struct sflow_ethernet_counter_t *sflow_eth_counter;

    if (len < sizeof(struct sflow_ethernet_counter_t))
	return 1;

    sflow_eth_counter = (const struct sflow_ethernet_counter_t *)pointer;
    ND_PRINT("\n\t      align errors %u, fcs errors %u, single collision %u, multiple collision %u, test error %u",
	   GET_BE_U_4(sflow_eth_counter->alignerrors),
	   GET_BE_U_4(sflow_eth_counter->fcserrors),
	   GET_BE_U_4(sflow_eth_counter->single_collision_frames),
	   GET_BE_U_4(sflow_eth_counter->multiple_collision_frames),
	   GET_BE_U_4(sflow_eth_counter->test_errors));
    ND_PRINT("\n\t      deferred %u, late collision %u, excessive collision %u, mac trans error %u",
	   GET_BE_U_4(sflow_eth_counter->deferred_transmissions),
	   GET_BE_U_4(sflow_eth_counter->late_collisions),
	   GET_BE_U_4(sflow_eth_counter->excessive_collisions),
	   GET_BE_U_4(sflow_eth_counter->mac_transmit_errors));
    ND_PRINT("\n\t      carrier error %u, frames too long %u, mac receive errors %u, symbol errors %u",
	   GET_BE_U_4(sflow_eth_counter->carrier_sense_errors),
	   GET_BE_U_4(sflow_eth_counter->frame_too_longs),
	   GET_BE_U_4(sflow_eth_counter->mac_receive_errors),
	   GET_BE_U_4(sflow_eth_counter->symbol_errors));

    return 0;
}

static int
print_sflow_counter_token_ring(netdissect_options *ndo _U_,
                               const u_char *pointer _U_, u_int len _U_)
{
    return 0;
}

static int
print_sflow_counter_basevg(netdissect_options *ndo,
                           const u_char *pointer, u_int len)
{
    const struct sflow_100basevg_counter_t *sflow_100basevg_counter;

    if (len < sizeof(struct sflow_100basevg_counter_t))
	return 1;

    sflow_100basevg_counter = (const struct sflow_100basevg_counter_t *)pointer;
    ND_PRINT("\n\t      in high prio frames %u, in high prio octets %" PRIu64,
	   GET_BE_U_4(sflow_100basevg_counter->in_highpriority_frames),
	   GET_BE_U_8(sflow_100basevg_counter->in_highpriority_octets));
    ND_PRINT("\n\t      in norm prio frames %u, in norm prio octets %" PRIu64,
	   GET_BE_U_4(sflow_100basevg_counter->in_normpriority_frames),
	   GET_BE_U_8(sflow_100basevg_counter->in_normpriority_octets));
    ND_PRINT("\n\t      in ipm errors %u, oversized %u, in data errors %u, null addressed frames %u",
	   GET_BE_U_4(sflow_100basevg_counter->in_ipmerrors),
	   GET_BE_U_4(sflow_100basevg_counter->in_oversized),
	   GET_BE_U_4(sflow_100basevg_counter->in_data_errors),
	   GET_BE_U_4(sflow_100basevg_counter->in_null_addressed_frames));
    ND_PRINT("\n\t      out high prio frames %u, out high prio octets %" PRIu64
	   ", trans into frames %u",
	   GET_BE_U_4(sflow_100basevg_counter->out_highpriority_frames),
	   GET_BE_U_8(sflow_100basevg_counter->out_highpriority_octets),
	   GET_BE_U_4(sflow_100basevg_counter->transitioninto_frames));
    ND_PRINT("\n\t      in hc high prio octets %" PRIu64
	   ", in hc norm prio octets %" PRIu64
	   ", out hc high prio octets %" PRIu64,
	   GET_BE_U_8(sflow_100basevg_counter->hc_in_highpriority_octets),
	   GET_BE_U_8(sflow_100basevg_counter->hc_in_normpriority_octets),
	   GET_BE_U_8(sflow_100basevg_counter->hc_out_highpriority_octets));

    return 0;
}

static int
print_sflow_counter_vlan(netdissect_options *ndo,
                         const u_char *pointer, u_int len)
{
    const struct sflow_vlan_counter_t *sflow_vlan_counter;

    if (len < sizeof(struct sflow_vlan_counter_t))
	return 1;

    sflow_vlan_counter = (const struct sflow_vlan_counter_t *)pointer;
    ND_PRINT("\n\t      vlan_id %u, octets %" PRIu64
	   ", unicast_pkt %u, multicast_pkt %u, broadcast_pkt %u, discards %u",
	   GET_BE_U_4(sflow_vlan_counter->vlan_id),
	   GET_BE_U_8(sflow_vlan_counter->octets),
	   GET_BE_U_4(sflow_vlan_counter->unicast_pkt),
	   GET_BE_U_4(sflow_vlan_counter->multicast_pkt),
	   GET_BE_U_4(sflow_vlan_counter->broadcast_pkt),
	   GET_BE_U_4(sflow_vlan_counter->discards));

    return 0;
}

struct sflow_processor_counter_t {
    nd_uint32_t five_sec_util;
    nd_uint32_t one_min_util;
    nd_uint32_t five_min_util;
    nd_uint64_t total_memory;
    nd_uint64_t free_memory;
};

static int
print_sflow_counter_processor(netdissect_options *ndo,
                              const u_char *pointer, u_int len)
{
    const struct sflow_processor_counter_t *sflow_processor_counter;

    if (len < sizeof(struct sflow_processor_counter_t))
	return 1;

    sflow_processor_counter = (const struct sflow_processor_counter_t *)pointer;
    ND_PRINT("\n\t      5sec %u, 1min %u, 5min %u, total_mem %" PRIu64
	   ", total_mem %" PRIu64,
	   GET_BE_U_4(sflow_processor_counter->five_sec_util),
	   GET_BE_U_4(sflow_processor_counter->one_min_util),
	   GET_BE_U_4(sflow_processor_counter->five_min_util),
	   GET_BE_U_8(sflow_processor_counter->total_memory),
	   GET_BE_U_8(sflow_processor_counter->free_memory));

    return 0;
}

static int
sflow_print_counter_records(netdissect_options *ndo,
                            const u_char *pointer, u_int len, u_int records)
{
    u_int nrecords;
    const u_char *tptr;
    u_int tlen;
    u_int counter_type;
    u_int counter_len;
    u_int enterprise;
    const struct sflow_counter_record_t *sflow_counter_record;

    nrecords = records;
    tptr = pointer;
    tlen = len;

    while (nrecords > 0) {
	/* do we have the "header?" */
	if (tlen < sizeof(struct sflow_counter_record_t))
	    return 1;
	sflow_counter_record = (const struct sflow_counter_record_t *)tptr;

	enterprise = GET_BE_U_4(sflow_counter_record->format);
	counter_type = enterprise & 0x0FFF;
	enterprise = enterprise >> 20;
	counter_len  = GET_BE_U_4(sflow_counter_record->length);
	ND_PRINT("\n\t    enterprise %u, %s (%u) length %u",
	       enterprise,
	       (enterprise == 0) ? tok2str(sflow_counter_type_values,"Unknown",counter_type) : "Unknown",
	       counter_type,
	       counter_len);

	tptr += sizeof(struct sflow_counter_record_t);
	tlen -= sizeof(struct sflow_counter_record_t);

	if (tlen < counter_len)
	    return 1;
	if (enterprise == 0) {
	    switch (counter_type) {
	    case SFLOW_COUNTER_GENERIC:
		if (print_sflow_counter_generic(ndo, tptr, tlen))
		    return 1;
		break;
	    case SFLOW_COUNTER_ETHERNET:
		if (print_sflow_counter_ethernet(ndo, tptr, tlen))
		    return 1;
		break;
	    case SFLOW_COUNTER_TOKEN_RING:
		if (print_sflow_counter_token_ring(ndo, tptr,tlen))
		    return 1;
		break;
	    case SFLOW_COUNTER_BASEVG:
		if (print_sflow_counter_basevg(ndo, tptr, tlen))
		    return 1;
		break;
	    case SFLOW_COUNTER_VLAN:
		if (print_sflow_counter_vlan(ndo, tptr, tlen))
		    return 1;
		break;
	    case SFLOW_COUNTER_PROCESSOR:
		if (print_sflow_counter_processor(ndo, tptr, tlen))
		    return 1;
		break;
	    default:
		if (ndo->ndo_vflag <= 1)
		    print_unknown_data(ndo, tptr, "\n\t\t", counter_len);
		break;
	    }
	}
	tptr += counter_len;
	tlen -= counter_len;
	nrecords--;

    }

    return 0;
}

static int
sflow_print_counter_sample(netdissect_options *ndo,
                           const u_char *pointer, u_int len)
{
    const struct sflow_counter_sample_t *sflow_counter_sample;
    u_int           nrecords;

    if (len < sizeof(struct sflow_counter_sample_t))
	return 1;

    sflow_counter_sample = (const struct sflow_counter_sample_t *)pointer;

    nrecords   = GET_BE_U_4(sflow_counter_sample->records);

    ND_PRINT(" seqnum %u, type %u, idx %u, records %u",
	   GET_BE_U_4(sflow_counter_sample->seqnum),
	   GET_U_1(sflow_counter_sample->type),
	   GET_BE_U_3(sflow_counter_sample->index),
	   nrecords);

    return sflow_print_counter_records(ndo, pointer + sizeof(struct sflow_counter_sample_t),
				       len - sizeof(struct sflow_counter_sample_t),
				       nrecords);
}

static int
sflow_print_expanded_counter_sample(netdissect_options *ndo,
                                    const u_char *pointer, u_int len)
{
    const struct sflow_expanded_counter_sample_t *sflow_expanded_counter_sample;
    u_int           nrecords;


    if (len < sizeof(struct sflow_expanded_counter_sample_t))
	return 1;

    sflow_expanded_counter_sample = (const struct sflow_expanded_counter_sample_t *)pointer;

    nrecords = GET_BE_U_4(sflow_expanded_counter_sample->records);

    ND_PRINT(" seqnum %u, type %u, idx %u, records %u",
	   GET_BE_U_4(sflow_expanded_counter_sample->seqnum),
	   GET_BE_U_4(sflow_expanded_counter_sample->type),
	   GET_BE_U_4(sflow_expanded_counter_sample->index),
	   nrecords);

    return sflow_print_counter_records(ndo, pointer + sizeof(struct sflow_expanded_counter_sample_t),
				       len - sizeof(struct sflow_expanded_counter_sample_t),
				       nrecords);
}

static int
print_sflow_raw_packet(netdissect_options *ndo,
                       const u_char *pointer, u_int len)
{
    const struct sflow_expanded_flow_raw_t *sflow_flow_raw;

    if (len < sizeof(struct sflow_expanded_flow_raw_t))
	return 1;

    sflow_flow_raw = (const struct sflow_expanded_flow_raw_t *)pointer;
    ND_PRINT("\n\t      protocol %s (%u), length %u, stripped bytes %u, header_size %u",
	   tok2str(sflow_flow_raw_protocol_values,"Unknown",GET_BE_U_4(sflow_flow_raw->protocol)),
	   GET_BE_U_4(sflow_flow_raw->protocol),
	   GET_BE_U_4(sflow_flow_raw->length),
	   GET_BE_U_4(sflow_flow_raw->stripped_bytes),
	   GET_BE_U_4(sflow_flow_raw->header_size));

    /* QUESTION - should we attempt to print the raw header itself?
       assuming of course there is enough data present to do so... */

    return 0;
}

static int
print_sflow_ethernet_frame(netdissect_options *ndo,
                           const u_char *pointer, u_int len)
{
    const struct sflow_ethernet_frame_t *sflow_ethernet_frame;

    if (len < sizeof(struct sflow_ethernet_frame_t))
	return 1;

    sflow_ethernet_frame = (const struct sflow_ethernet_frame_t *)pointer;

    ND_PRINT("\n\t      frame len %u, type %u",
	   GET_BE_U_4(sflow_ethernet_frame->length),
	   GET_BE_U_4(sflow_ethernet_frame->type));

    return 0;
}

static int
print_sflow_extended_switch_data(netdissect_options *ndo,
                                 const u_char *pointer, u_int len)
{
    const struct sflow_extended_switch_data_t *sflow_extended_sw_data;

    if (len < sizeof(struct sflow_extended_switch_data_t))
	return 1;

    sflow_extended_sw_data = (const struct sflow_extended_switch_data_t *)pointer;
    ND_PRINT("\n\t      src vlan %u, src pri %u, dst vlan %u, dst pri %u",
	   GET_BE_U_4(sflow_extended_sw_data->src_vlan),
	   GET_BE_U_4(sflow_extended_sw_data->src_pri),
	   GET_BE_U_4(sflow_extended_sw_data->dst_vlan),
	   GET_BE_U_4(sflow_extended_sw_data->dst_pri));

    return 0;
}

static int
sflow_print_flow_records(netdissect_options *ndo,
                         const u_char *pointer, u_int len, u_int records)
{
    u_int nrecords;
    const u_char *tptr;
    u_int tlen;
    u_int flow_type;
    u_int enterprise;
    u_int flow_len;
    const struct sflow_flow_record_t *sflow_flow_record;

    nrecords = records;
    tptr = pointer;
    tlen = len;

    while (nrecords > 0) {
	/* do we have the "header?" */
	if (tlen < sizeof(struct sflow_flow_record_t))
	    return 1;

	sflow_flow_record = (const struct sflow_flow_record_t *)tptr;

	/* so, the funky encoding means we cannot blythly mask-off
	   bits, we must also check the enterprise. */

	enterprise = GET_BE_U_4(sflow_flow_record->format);
	flow_type = enterprise & 0x0FFF;
	enterprise = enterprise >> 12;
	flow_len  = GET_BE_U_4(sflow_flow_record->length);
	ND_PRINT("\n\t    enterprise %u %s (%u) length %u",
	       enterprise,
	       (enterprise == 0) ? tok2str(sflow_flow_type_values,"Unknown",flow_type) : "Unknown",
	       flow_type,
	       flow_len);

	tptr += sizeof(struct sflow_flow_record_t);
	tlen -= sizeof(struct sflow_flow_record_t);

	if (tlen < flow_len)
	    return 1;

	if (enterprise == 0) {
	    switch (flow_type) {
	    case SFLOW_FLOW_RAW_PACKET:
		if (print_sflow_raw_packet(ndo, tptr, tlen))
		    return 1;
		break;
	    case SFLOW_FLOW_EXTENDED_SWITCH_DATA:
		if (print_sflow_extended_switch_data(ndo, tptr, tlen))
		    return 1;
		break;
	    case SFLOW_FLOW_ETHERNET_FRAME:
		if (print_sflow_ethernet_frame(ndo, tptr, tlen))
		    return 1;
		break;
		/* FIXME these need a decoder */
	    case SFLOW_FLOW_IPV4_DATA:
	    case SFLOW_FLOW_IPV6_DATA:
	    case SFLOW_FLOW_EXTENDED_ROUTER_DATA:
	    case SFLOW_FLOW_EXTENDED_GATEWAY_DATA:
	    case SFLOW_FLOW_EXTENDED_USER_DATA:
	    case SFLOW_FLOW_EXTENDED_URL_DATA:
	    case SFLOW_FLOW_EXTENDED_MPLS_DATA:
	    case SFLOW_FLOW_EXTENDED_NAT_DATA:
	    case SFLOW_FLOW_EXTENDED_MPLS_TUNNEL:
	    case SFLOW_FLOW_EXTENDED_MPLS_VC:
	    case SFLOW_FLOW_EXTENDED_MPLS_FEC:
	    case SFLOW_FLOW_EXTENDED_MPLS_LVP_FEC:
	    case SFLOW_FLOW_EXTENDED_VLAN_TUNNEL:
		break;
	    default:
		if (ndo->ndo_vflag <= 1)
		    print_unknown_data(ndo, tptr, "\n\t\t", flow_len);
		break;
	    }
	}
	tptr += flow_len;
	tlen -= flow_len;
	nrecords--;

    }

    return 0;
}

static int
sflow_print_flow_sample(netdissect_options *ndo,
                        const u_char *pointer, u_int len)
{
    const struct sflow_flow_sample_t *sflow_flow_sample;
    u_int          nrecords;

    if (len < sizeof(struct sflow_flow_sample_t))
	return 1;

    sflow_flow_sample = (const struct sflow_flow_sample_t *)pointer;

    nrecords = GET_BE_U_4(sflow_flow_sample->records);

    ND_PRINT(" seqnum %u, type %u, idx %u, rate %u, pool %u, drops %u, input %u output %u records %u",
	   GET_BE_U_4(sflow_flow_sample->seqnum),
	   GET_U_1(sflow_flow_sample->type),
	   GET_BE_U_3(sflow_flow_sample->index),
	   GET_BE_U_4(sflow_flow_sample->rate),
	   GET_BE_U_4(sflow_flow_sample->pool),
	   GET_BE_U_4(sflow_flow_sample->drops),
	   GET_BE_U_4(sflow_flow_sample->in_interface),
	   GET_BE_U_4(sflow_flow_sample->out_interface),
	   nrecords);

    return sflow_print_flow_records(ndo, pointer + sizeof(struct sflow_flow_sample_t),
				    len - sizeof(struct sflow_flow_sample_t),
				    nrecords);
}

static int
sflow_print_expanded_flow_sample(netdissect_options *ndo,
                                 const u_char *pointer, u_int len)
{
    const struct sflow_expanded_flow_sample_t *sflow_expanded_flow_sample;
    u_int nrecords;

    if (len < sizeof(struct sflow_expanded_flow_sample_t))
	return 1;

    sflow_expanded_flow_sample = (const struct sflow_expanded_flow_sample_t *)pointer;

    nrecords = GET_BE_U_4(sflow_expanded_flow_sample->records);

    ND_PRINT(" seqnum %u, type %u, idx %u, rate %u, pool %u, drops %u, records %u",
	   GET_BE_U_4(sflow_expanded_flow_sample->seqnum),
	   GET_BE_U_4(sflow_expanded_flow_sample->type),
	   GET_BE_U_4(sflow_expanded_flow_sample->index),
	   GET_BE_U_4(sflow_expanded_flow_sample->rate),
	   GET_BE_U_4(sflow_expanded_flow_sample->pool),
	   GET_BE_U_4(sflow_expanded_flow_sample->drops),
	   nrecords);

    return sflow_print_flow_records(ndo, pointer + sizeof(struct sflow_expanded_flow_sample_t),
				    len - sizeof(struct sflow_expanded_flow_sample_t),
				    nrecords);
}

void
sflow_print(netdissect_options *ndo,
            const u_char *pptr, u_int len)
{
    const struct sflow_datagram_t *sflow_datagram;
    const struct sflow_v6_datagram_t *sflow_v6_datagram;
    const struct sflow_sample_header *sflow_sample;

    const u_char *tptr;
    u_int tlen;
    uint32_t sflow_sample_type, sflow_sample_len;
    uint32_t nsamples;
    uint32_t ip_version;

    ndo->ndo_protocol = "sflow";
    tptr = pptr;
    tlen = len;
    sflow_datagram = (const struct sflow_datagram_t *)pptr;
    sflow_v6_datagram = (const struct sflow_v6_datagram_t *)pptr;
    ip_version = GET_BE_U_4(sflow_datagram->ip_version);

    if ((len < sizeof(struct sflow_datagram_t) && (ip_version == 1)) ||
        (len < sizeof(struct sflow_v6_datagram_t) && (ip_version == 2))) {
        ND_PRINT("sFlowv%u", GET_BE_U_4(sflow_datagram->version));
        ND_PRINT(" [length %u < %zu]", len, sizeof(struct sflow_datagram_t));
        nd_print_invalid(ndo);
        return;
    }
    ND_TCHECK_SIZE(sflow_datagram);

    /*
     * Sanity checking of the header.
     */
    if (GET_BE_U_4(sflow_datagram->version) != 5) {
        ND_PRINT("sFlow version %u packet not supported",
               GET_BE_U_4(sflow_datagram->version));
        return;
    }

    if (ndo->ndo_vflag < 1) {
        ND_PRINT("sFlowv%u, %s agent %s, agent-id %u, length %u",
               GET_BE_U_4(sflow_datagram->version),
               ip_version == 1 ? "IPv4" : "IPv6",
               ip_version == 1 ? GET_IPADDR_STRING(sflow_datagram->agent) :
                                 GET_IP6ADDR_STRING( sflow_v6_datagram->agent),
               ip_version == 1 ? GET_BE_U_4(sflow_datagram->agent_id) :
                                 GET_BE_U_4(sflow_v6_datagram->agent_id),
               len);
        return;
    }

    /* ok they seem to want to know everything - lets fully decode it */
    if (ip_version == 1) {
        nsamples=GET_BE_U_4(sflow_datagram->samples);
        ND_PRINT("sFlowv%u, %s agent %s, agent-id %u, seqnum %u, uptime %u, samples %u, length %u",
               GET_BE_U_4(sflow_datagram->version),
               "IPv4",
               GET_IPADDR_STRING(sflow_datagram->agent),
               GET_BE_U_4(sflow_datagram->agent_id),
               GET_BE_U_4(sflow_datagram->seqnum),
               GET_BE_U_4(sflow_datagram->uptime),
               nsamples,
               len);

        /* skip Common header */
        ND_LCHECK_ZU(tlen, sizeof(struct sflow_datagram_t));
        tptr += sizeof(struct sflow_datagram_t);
        tlen -= sizeof(struct sflow_datagram_t);
    } else {
        nsamples=GET_BE_U_4(sflow_v6_datagram->samples);
        ND_PRINT("sFlowv%u, %s agent %s, agent-id %u, seqnum %u, uptime %u, samples %u, length %u",
               GET_BE_U_4(sflow_v6_datagram->version),
               "IPv6",
               GET_IP6ADDR_STRING(sflow_v6_datagram->agent),
               GET_BE_U_4(sflow_v6_datagram->agent_id),
               GET_BE_U_4(sflow_v6_datagram->seqnum),
               GET_BE_U_4(sflow_v6_datagram->uptime),
               nsamples,
               len);

        /* skip Common header */
        ND_LCHECK_ZU(tlen, sizeof(struct sflow_v6_datagram_t));
        tptr += sizeof(struct sflow_v6_datagram_t);
        tlen -= sizeof(struct sflow_v6_datagram_t);
    }
    while (nsamples > 0 && tlen > 0) {
        sflow_sample = (const struct sflow_sample_header *)tptr;

        sflow_sample_type = (GET_BE_U_4(sflow_sample->format)&0x0FFF);
        sflow_sample_len = GET_BE_U_4(sflow_sample->len);

	if (tlen < sizeof(struct sflow_sample_header))
	    goto invalid;

        tptr += sizeof(struct sflow_sample_header);
        tlen -= sizeof(struct sflow_sample_header);

        ND_PRINT("\n\t%s (%u), length %u,",
               tok2str(sflow_format_values, "Unknown", sflow_sample_type),
               sflow_sample_type,
               sflow_sample_len);

        /* basic sanity check */
        if (sflow_sample_type == 0 || sflow_sample_len ==0) {
            return;
        }

	if (tlen < sflow_sample_len)
	    goto invalid;

        /* did we capture enough for fully decoding the sample ? */
        ND_TCHECK_LEN(tptr, sflow_sample_len);

	switch(sflow_sample_type) {
        case SFLOW_FLOW_SAMPLE:
	    if (sflow_print_flow_sample(ndo, tptr, tlen))
		goto invalid;
            break;

        case SFLOW_COUNTER_SAMPLE:
	    if (sflow_print_counter_sample(ndo, tptr,tlen))
		goto invalid;
            break;

        case SFLOW_EXPANDED_FLOW_SAMPLE:
	    if (sflow_print_expanded_flow_sample(ndo, tptr, tlen))
		goto invalid;
	    break;

        case SFLOW_EXPANDED_COUNTER_SAMPLE:
	    if (sflow_print_expanded_counter_sample(ndo, tptr,tlen))
		goto invalid;
	    break;

        default:
            if (ndo->ndo_vflag <= 1)
                print_unknown_data(ndo, tptr, "\n\t    ", sflow_sample_len);
            break;
        }
        tptr += sflow_sample_len;
        tlen -= sflow_sample_len;
        nsamples--;
    }
    return;

invalid:
    nd_print_invalid(ndo);
    ND_TCHECK_LEN(tptr, tlen);
}
