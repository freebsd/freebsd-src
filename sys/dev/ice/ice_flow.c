/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2021, Intel Corporation
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *   3. Neither the name of the Intel Corporation nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */
/*$FreeBSD$*/

#include "ice_common.h"
#include "ice_flow.h"

/* Size of known protocol header fields */
#define ICE_FLOW_FLD_SZ_ETH_TYPE	2
#define ICE_FLOW_FLD_SZ_VLAN		2
#define ICE_FLOW_FLD_SZ_IPV4_ADDR	4
#define ICE_FLOW_FLD_SZ_IPV6_ADDR	16
#define ICE_FLOW_FLD_SZ_IP_DSCP		1
#define ICE_FLOW_FLD_SZ_IP_TTL		1
#define ICE_FLOW_FLD_SZ_IP_PROT		1
#define ICE_FLOW_FLD_SZ_PORT		2
#define ICE_FLOW_FLD_SZ_TCP_FLAGS	1
#define ICE_FLOW_FLD_SZ_ICMP_TYPE	1
#define ICE_FLOW_FLD_SZ_ICMP_CODE	1
#define ICE_FLOW_FLD_SZ_ARP_OPER	2
#define ICE_FLOW_FLD_SZ_GRE_KEYID	4

/* Describe properties of a protocol header field */
struct ice_flow_field_info {
	enum ice_flow_seg_hdr hdr;
	s16 off;	/* Offset from start of a protocol header, in bits */
	u16 size;	/* Size of fields in bits */
};

#define ICE_FLOW_FLD_INFO(_hdr, _offset_bytes, _size_bytes) { \
	.hdr = _hdr, \
	.off = (_offset_bytes) * BITS_PER_BYTE, \
	.size = (_size_bytes) * BITS_PER_BYTE, \
}

/* Table containing properties of supported protocol header fields */
static const
struct ice_flow_field_info ice_flds_info[ICE_FLOW_FIELD_IDX_MAX] = {
	/* Ether */
	/* ICE_FLOW_FIELD_IDX_ETH_DA */
	ICE_FLOW_FLD_INFO(ICE_FLOW_SEG_HDR_ETH, 0, ETH_ALEN),
	/* ICE_FLOW_FIELD_IDX_ETH_SA */
	ICE_FLOW_FLD_INFO(ICE_FLOW_SEG_HDR_ETH, ETH_ALEN, ETH_ALEN),
	/* ICE_FLOW_FIELD_IDX_S_VLAN */
	ICE_FLOW_FLD_INFO(ICE_FLOW_SEG_HDR_VLAN, 12, ICE_FLOW_FLD_SZ_VLAN),
	/* ICE_FLOW_FIELD_IDX_C_VLAN */
	ICE_FLOW_FLD_INFO(ICE_FLOW_SEG_HDR_VLAN, 14, ICE_FLOW_FLD_SZ_VLAN),
	/* ICE_FLOW_FIELD_IDX_ETH_TYPE */
	ICE_FLOW_FLD_INFO(ICE_FLOW_SEG_HDR_ETH, 0, ICE_FLOW_FLD_SZ_ETH_TYPE),
	/* IPv4 / IPv6 */
	/* ICE_FLOW_FIELD_IDX_IPV4_DSCP */
	ICE_FLOW_FLD_INFO(ICE_FLOW_SEG_HDR_IPV4, 1, ICE_FLOW_FLD_SZ_IP_DSCP),
	/* ICE_FLOW_FIELD_IDX_IPV6_DSCP */
	ICE_FLOW_FLD_INFO(ICE_FLOW_SEG_HDR_IPV6, 0, ICE_FLOW_FLD_SZ_IP_DSCP),
	/* ICE_FLOW_FIELD_IDX_IPV4_TTL */
	ICE_FLOW_FLD_INFO(ICE_FLOW_SEG_HDR_NONE, 8, ICE_FLOW_FLD_SZ_IP_TTL),
	/* ICE_FLOW_FIELD_IDX_IPV4_PROT */
	ICE_FLOW_FLD_INFO(ICE_FLOW_SEG_HDR_NONE, 9, ICE_FLOW_FLD_SZ_IP_PROT),
	/* ICE_FLOW_FIELD_IDX_IPV6_TTL */
	ICE_FLOW_FLD_INFO(ICE_FLOW_SEG_HDR_NONE, 7, ICE_FLOW_FLD_SZ_IP_TTL),
	/* ICE_FLOW_FIELD_IDX_IPV4_PROT */
	ICE_FLOW_FLD_INFO(ICE_FLOW_SEG_HDR_NONE, 6, ICE_FLOW_FLD_SZ_IP_PROT),
	/* ICE_FLOW_FIELD_IDX_IPV4_SA */
	ICE_FLOW_FLD_INFO(ICE_FLOW_SEG_HDR_IPV4, 12, ICE_FLOW_FLD_SZ_IPV4_ADDR),
	/* ICE_FLOW_FIELD_IDX_IPV4_DA */
	ICE_FLOW_FLD_INFO(ICE_FLOW_SEG_HDR_IPV4, 16, ICE_FLOW_FLD_SZ_IPV4_ADDR),
	/* ICE_FLOW_FIELD_IDX_IPV6_SA */
	ICE_FLOW_FLD_INFO(ICE_FLOW_SEG_HDR_IPV6, 8, ICE_FLOW_FLD_SZ_IPV6_ADDR),
	/* ICE_FLOW_FIELD_IDX_IPV6_DA */
	ICE_FLOW_FLD_INFO(ICE_FLOW_SEG_HDR_IPV6, 24, ICE_FLOW_FLD_SZ_IPV6_ADDR),
	/* Transport */
	/* ICE_FLOW_FIELD_IDX_TCP_SRC_PORT */
	ICE_FLOW_FLD_INFO(ICE_FLOW_SEG_HDR_TCP, 0, ICE_FLOW_FLD_SZ_PORT),
	/* ICE_FLOW_FIELD_IDX_TCP_DST_PORT */
	ICE_FLOW_FLD_INFO(ICE_FLOW_SEG_HDR_TCP, 2, ICE_FLOW_FLD_SZ_PORT),
	/* ICE_FLOW_FIELD_IDX_UDP_SRC_PORT */
	ICE_FLOW_FLD_INFO(ICE_FLOW_SEG_HDR_UDP, 0, ICE_FLOW_FLD_SZ_PORT),
	/* ICE_FLOW_FIELD_IDX_UDP_DST_PORT */
	ICE_FLOW_FLD_INFO(ICE_FLOW_SEG_HDR_UDP, 2, ICE_FLOW_FLD_SZ_PORT),
	/* ICE_FLOW_FIELD_IDX_SCTP_SRC_PORT */
	ICE_FLOW_FLD_INFO(ICE_FLOW_SEG_HDR_SCTP, 0, ICE_FLOW_FLD_SZ_PORT),
	/* ICE_FLOW_FIELD_IDX_SCTP_DST_PORT */
	ICE_FLOW_FLD_INFO(ICE_FLOW_SEG_HDR_SCTP, 2, ICE_FLOW_FLD_SZ_PORT),
	/* ICE_FLOW_FIELD_IDX_TCP_FLAGS */
	ICE_FLOW_FLD_INFO(ICE_FLOW_SEG_HDR_TCP, 13, ICE_FLOW_FLD_SZ_TCP_FLAGS),
	/* ARP */
	/* ICE_FLOW_FIELD_IDX_ARP_SIP */
	ICE_FLOW_FLD_INFO(ICE_FLOW_SEG_HDR_ARP, 14, ICE_FLOW_FLD_SZ_IPV4_ADDR),
	/* ICE_FLOW_FIELD_IDX_ARP_DIP */
	ICE_FLOW_FLD_INFO(ICE_FLOW_SEG_HDR_ARP, 24, ICE_FLOW_FLD_SZ_IPV4_ADDR),
	/* ICE_FLOW_FIELD_IDX_ARP_SHA */
	ICE_FLOW_FLD_INFO(ICE_FLOW_SEG_HDR_ARP, 8, ETH_ALEN),
	/* ICE_FLOW_FIELD_IDX_ARP_DHA */
	ICE_FLOW_FLD_INFO(ICE_FLOW_SEG_HDR_ARP, 18, ETH_ALEN),
	/* ICE_FLOW_FIELD_IDX_ARP_OP */
	ICE_FLOW_FLD_INFO(ICE_FLOW_SEG_HDR_ARP, 6, ICE_FLOW_FLD_SZ_ARP_OPER),
	/* ICMP */
	/* ICE_FLOW_FIELD_IDX_ICMP_TYPE */
	ICE_FLOW_FLD_INFO(ICE_FLOW_SEG_HDR_ICMP, 0, ICE_FLOW_FLD_SZ_ICMP_TYPE),
	/* ICE_FLOW_FIELD_IDX_ICMP_CODE */
	ICE_FLOW_FLD_INFO(ICE_FLOW_SEG_HDR_ICMP, 1, ICE_FLOW_FLD_SZ_ICMP_CODE),
	/* GRE */
	/* ICE_FLOW_FIELD_IDX_GRE_KEYID */
	ICE_FLOW_FLD_INFO(ICE_FLOW_SEG_HDR_GRE, 12, ICE_FLOW_FLD_SZ_GRE_KEYID),
};

/* Bitmaps indicating relevant packet types for a particular protocol header
 *
 * Packet types for packets with an Outer/First/Single MAC header
 */
static const u32 ice_ptypes_mac_ofos[] = {
	0xFDC00846, 0xBFBF7F7E, 0xF70001DF, 0xFEFDFDFB,
	0x0000077E, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

/* Packet types for packets with an Innermost/Last MAC VLAN header */
static const u32 ice_ptypes_macvlan_il[] = {
	0x00000000, 0xBC000000, 0x000001DF, 0xF0000000,
	0x0000077E, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

/* Packet types for packets with an Outer/First/Single IPv4 header, does NOT
 * include IPV4 other PTYPEs
 */
static const u32 ice_ptypes_ipv4_ofos[] = {
	0x1DC00000, 0x04000800, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

/* Packet types for packets with an Outer/First/Single IPv4 header, includes
 * IPV4 other PTYPEs
 */
static const u32 ice_ptypes_ipv4_ofos_all[] = {
	0x1DC00000, 0x04000800, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

/* Packet types for packets with an Innermost/Last IPv4 header */
static const u32 ice_ptypes_ipv4_il[] = {
	0xE0000000, 0xB807700E, 0x80000003, 0xE01DC03B,
	0x0000000E, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

/* Packet types for packets with an Outer/First/Single IPv6 header, does NOT
 * include IVP6 other PTYPEs
 */
static const u32 ice_ptypes_ipv6_ofos[] = {
	0x00000000, 0x00000000, 0x77000000, 0x10002000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

/* Packet types for packets with an Outer/First/Single IPv6 header, includes
 * IPV6 other PTYPEs
 */
static const u32 ice_ptypes_ipv6_ofos_all[] = {
	0x00000000, 0x00000000, 0x77000000, 0x10002000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

/* Packet types for packets with an Innermost/Last IPv6 header */
static const u32 ice_ptypes_ipv6_il[] = {
	0x00000000, 0x03B80770, 0x000001DC, 0x0EE00000,
	0x00000770, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

/* Packet types for packets with an Outer/First/Single IPv4 header - no L4 */
static const u32 ice_ptypes_ipv4_ofos_no_l4[] = {
	0x10C00000, 0x04000800, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

/* Packet types for packets with an Innermost/Last IPv4 header - no L4 */
static const u32 ice_ptypes_ipv4_il_no_l4[] = {
	0x60000000, 0x18043008, 0x80000002, 0x6010c021,
	0x00000008, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

/* Packet types for packets with an Outer/First/Single IPv6 header - no L4 */
static const u32 ice_ptypes_ipv6_ofos_no_l4[] = {
	0x00000000, 0x00000000, 0x43000000, 0x10002000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

/* Packet types for packets with an Innermost/Last IPv6 header - no L4 */
static const u32 ice_ptypes_ipv6_il_no_l4[] = {
	0x00000000, 0x02180430, 0x0000010c, 0x086010c0,
	0x00000430, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

/* Packet types for packets with an Outermost/First ARP header */
static const u32 ice_ptypes_arp_of[] = {
	0x00000800, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

/* UDP Packet types for non-tunneled packets or tunneled
 * packets with inner UDP.
 */
static const u32 ice_ptypes_udp_il[] = {
	0x81000000, 0x20204040, 0x04000010, 0x80810102,
	0x00000040, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

/* Packet types for packets with an Innermost/Last TCP header */
static const u32 ice_ptypes_tcp_il[] = {
	0x04000000, 0x80810102, 0x10000040, 0x02040408,
	0x00000102, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

/* Packet types for packets with an Innermost/Last SCTP header */
static const u32 ice_ptypes_sctp_il[] = {
	0x08000000, 0x01020204, 0x20000081, 0x04080810,
	0x00000204, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

/* Packet types for packets with an Outermost/First ICMP header */
static const u32 ice_ptypes_icmp_of[] = {
	0x10000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

/* Packet types for packets with an Innermost/Last ICMP header */
static const u32 ice_ptypes_icmp_il[] = {
	0x00000000, 0x02040408, 0x40000102, 0x08101020,
	0x00000408, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

/* Packet types for packets with an Outermost/First GRE header */
static const u32 ice_ptypes_gre_of[] = {
	0x00000000, 0xBFBF7800, 0x000001DF, 0xFEFDE000,
	0x0000017E, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

/* Packet types for packets with an Innermost/Last MAC header */
static const u32 ice_ptypes_mac_il[] = {
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

/* Manage parameters and info. used during the creation of a flow profile */
struct ice_flow_prof_params {
	enum ice_block blk;
	u16 entry_length; /* # of bytes formatted entry will require */
	u8 es_cnt;
	struct ice_flow_prof *prof;

	/* For ACL, the es[0] will have the data of ICE_RX_MDID_PKT_FLAGS_15_0
	 * This will give us the direction flags.
	 */
	struct ice_fv_word es[ICE_MAX_FV_WORDS];
	ice_declare_bitmap(ptypes, ICE_FLOW_PTYPE_MAX);
};

#define ICE_FLOW_SEG_HDRS_L3_MASK	\
	(ICE_FLOW_SEG_HDR_IPV4 | ICE_FLOW_SEG_HDR_IPV6 | \
	 ICE_FLOW_SEG_HDR_ARP)
#define ICE_FLOW_SEG_HDRS_L4_MASK	\
	(ICE_FLOW_SEG_HDR_ICMP | ICE_FLOW_SEG_HDR_TCP | ICE_FLOW_SEG_HDR_UDP | \
	 ICE_FLOW_SEG_HDR_SCTP)
/* mask for L4 protocols that are NOT part of IPV4/6 OTHER PTYPE groups */
#define ICE_FLOW_SEG_HDRS_L4_MASK_NO_OTHER	\
	(ICE_FLOW_SEG_HDR_TCP | ICE_FLOW_SEG_HDR_UDP | ICE_FLOW_SEG_HDR_SCTP)

/**
 * ice_flow_val_hdrs - validates packet segments for valid protocol headers
 * @segs: array of one or more packet segments that describe the flow
 * @segs_cnt: number of packet segments provided
 */
static enum ice_status
ice_flow_val_hdrs(struct ice_flow_seg_info *segs, u8 segs_cnt)
{
	u8 i;

	for (i = 0; i < segs_cnt; i++) {
		/* Multiple L3 headers */
		if (segs[i].hdrs & ICE_FLOW_SEG_HDRS_L3_MASK &&
		    !ice_is_pow2(segs[i].hdrs & ICE_FLOW_SEG_HDRS_L3_MASK))
			return ICE_ERR_PARAM;

		/* Multiple L4 headers */
		if (segs[i].hdrs & ICE_FLOW_SEG_HDRS_L4_MASK &&
		    !ice_is_pow2(segs[i].hdrs & ICE_FLOW_SEG_HDRS_L4_MASK))
			return ICE_ERR_PARAM;
	}

	return ICE_SUCCESS;
}

/**
 * ice_flow_proc_seg_hdrs - process protocol headers present in pkt segments
 * @params: information about the flow to be processed
 *
 * This function identifies the packet types associated with the protocol
 * headers being present in packet segments of the specified flow profile.
 */
static enum ice_status
ice_flow_proc_seg_hdrs(struct ice_flow_prof_params *params)
{
	struct ice_flow_prof *prof;
	u8 i;

	ice_memset(params->ptypes, 0xff, sizeof(params->ptypes),
		   ICE_NONDMA_MEM);

	prof = params->prof;

	for (i = 0; i < params->prof->segs_cnt; i++) {
		const ice_bitmap_t *src;
		u32 hdrs;

		hdrs = prof->segs[i].hdrs;

		if (hdrs & ICE_FLOW_SEG_HDR_ETH) {
			src = !i ? (const ice_bitmap_t *)ice_ptypes_mac_ofos :
				(const ice_bitmap_t *)ice_ptypes_mac_il;
			ice_and_bitmap(params->ptypes, params->ptypes, src,
				       ICE_FLOW_PTYPE_MAX);
		}

		if (i && hdrs & ICE_FLOW_SEG_HDR_VLAN) {
			src = (const ice_bitmap_t *)ice_ptypes_macvlan_il;
			ice_and_bitmap(params->ptypes, params->ptypes, src,
				       ICE_FLOW_PTYPE_MAX);
		}

		if (!i && hdrs & ICE_FLOW_SEG_HDR_ARP) {
			ice_and_bitmap(params->ptypes, params->ptypes,
				       (const ice_bitmap_t *)ice_ptypes_arp_of,
				       ICE_FLOW_PTYPE_MAX);
		}

		if ((hdrs & ICE_FLOW_SEG_HDR_IPV4) &&
		    (hdrs & ICE_FLOW_SEG_HDR_IPV_OTHER)) {
			src = i ?
				(const ice_bitmap_t *)ice_ptypes_ipv4_il :
				(const ice_bitmap_t *)ice_ptypes_ipv4_ofos_all;
			ice_and_bitmap(params->ptypes, params->ptypes, src,
				       ICE_FLOW_PTYPE_MAX);
		} else if ((hdrs & ICE_FLOW_SEG_HDR_IPV6) &&
			   (hdrs & ICE_FLOW_SEG_HDR_IPV_OTHER)) {
			src = i ?
				(const ice_bitmap_t *)ice_ptypes_ipv6_il :
				(const ice_bitmap_t *)ice_ptypes_ipv6_ofos_all;
			ice_and_bitmap(params->ptypes, params->ptypes, src,
				       ICE_FLOW_PTYPE_MAX);
		} else if ((hdrs & ICE_FLOW_SEG_HDR_IPV4) &&
			   !(hdrs & ICE_FLOW_SEG_HDRS_L4_MASK_NO_OTHER)) {
			src = !i ? (const ice_bitmap_t *)ice_ptypes_ipv4_ofos_no_l4 :
				(const ice_bitmap_t *)ice_ptypes_ipv4_il_no_l4;
			ice_and_bitmap(params->ptypes, params->ptypes, src,
				       ICE_FLOW_PTYPE_MAX);
		} else if (hdrs & ICE_FLOW_SEG_HDR_IPV4) {
			src = !i ? (const ice_bitmap_t *)ice_ptypes_ipv4_ofos :
				(const ice_bitmap_t *)ice_ptypes_ipv4_il;
			ice_and_bitmap(params->ptypes, params->ptypes, src,
				       ICE_FLOW_PTYPE_MAX);
		} else if ((hdrs & ICE_FLOW_SEG_HDR_IPV6) &&
			   !(hdrs & ICE_FLOW_SEG_HDRS_L4_MASK_NO_OTHER)) {
			src = !i ? (const ice_bitmap_t *)ice_ptypes_ipv6_ofos_no_l4 :
				(const ice_bitmap_t *)ice_ptypes_ipv6_il_no_l4;
			ice_and_bitmap(params->ptypes, params->ptypes, src,
				       ICE_FLOW_PTYPE_MAX);
		} else if (hdrs & ICE_FLOW_SEG_HDR_IPV6) {
			src = !i ? (const ice_bitmap_t *)ice_ptypes_ipv6_ofos :
				(const ice_bitmap_t *)ice_ptypes_ipv6_il;
			ice_and_bitmap(params->ptypes, params->ptypes, src,
				       ICE_FLOW_PTYPE_MAX);
		}

		if (hdrs & ICE_FLOW_SEG_HDR_UDP) {
			src = (const ice_bitmap_t *)ice_ptypes_udp_il;
			ice_and_bitmap(params->ptypes, params->ptypes, src,
				       ICE_FLOW_PTYPE_MAX);
		} else if (hdrs & ICE_FLOW_SEG_HDR_TCP) {
			ice_and_bitmap(params->ptypes, params->ptypes,
				       (const ice_bitmap_t *)ice_ptypes_tcp_il,
				       ICE_FLOW_PTYPE_MAX);
		} else if (hdrs & ICE_FLOW_SEG_HDR_SCTP) {
			src = (const ice_bitmap_t *)ice_ptypes_sctp_il;
			ice_and_bitmap(params->ptypes, params->ptypes, src,
				       ICE_FLOW_PTYPE_MAX);
		}

		if (hdrs & ICE_FLOW_SEG_HDR_ICMP) {
			src = !i ? (const ice_bitmap_t *)ice_ptypes_icmp_of :
				(const ice_bitmap_t *)ice_ptypes_icmp_il;
			ice_and_bitmap(params->ptypes, params->ptypes, src,
				       ICE_FLOW_PTYPE_MAX);
		} else if (hdrs & ICE_FLOW_SEG_HDR_GRE) {
			if (!i) {
				src = (const ice_bitmap_t *)ice_ptypes_gre_of;
				ice_and_bitmap(params->ptypes, params->ptypes,
					       src, ICE_FLOW_PTYPE_MAX);
			}
		}
	}

	return ICE_SUCCESS;
}

/**
 * ice_flow_xtract_fld - Create an extraction sequence entry for the given field
 * @hw: pointer to the HW struct
 * @params: information about the flow to be processed
 * @seg: packet segment index of the field to be extracted
 * @fld: ID of field to be extracted
 *
 * This function determines the protocol ID, offset, and size of the given
 * field. It then allocates one or more extraction sequence entries for the
 * given field, and fill the entries with protocol ID and offset information.
 */
static enum ice_status
ice_flow_xtract_fld(struct ice_hw *hw, struct ice_flow_prof_params *params,
		    u8 seg, enum ice_flow_field fld)
{
	enum ice_flow_field sib = ICE_FLOW_FIELD_IDX_MAX;
	enum ice_prot_id prot_id = ICE_PROT_ID_INVAL;
	u8 fv_words = hw->blk[params->blk].es.fvw;
	struct ice_flow_fld_info *flds;
	u16 cnt, ese_bits, i;
	u16 off;

	flds = params->prof->segs[seg].fields;

	switch (fld) {
	case ICE_FLOW_FIELD_IDX_ETH_DA:
	case ICE_FLOW_FIELD_IDX_ETH_SA:
	case ICE_FLOW_FIELD_IDX_S_VLAN:
	case ICE_FLOW_FIELD_IDX_C_VLAN:
		prot_id = seg == 0 ? ICE_PROT_MAC_OF_OR_S : ICE_PROT_MAC_IL;
		break;
	case ICE_FLOW_FIELD_IDX_ETH_TYPE:
		prot_id = seg == 0 ? ICE_PROT_ETYPE_OL : ICE_PROT_ETYPE_IL;
		break;
	case ICE_FLOW_FIELD_IDX_IPV4_DSCP:
		prot_id = seg == 0 ? ICE_PROT_IPV4_OF_OR_S : ICE_PROT_IPV4_IL;
		break;
	case ICE_FLOW_FIELD_IDX_IPV6_DSCP:
		prot_id = seg == 0 ? ICE_PROT_IPV6_OF_OR_S : ICE_PROT_IPV6_IL;
		break;
	case ICE_FLOW_FIELD_IDX_IPV4_TTL:
	case ICE_FLOW_FIELD_IDX_IPV4_PROT:
		prot_id = seg == 0 ? ICE_PROT_IPV4_OF_OR_S : ICE_PROT_IPV4_IL;

		/* TTL and PROT share the same extraction seq. entry.
		 * Each is considered a sibling to the other in terms of sharing
		 * the same extraction sequence entry.
		 */
		if (fld == ICE_FLOW_FIELD_IDX_IPV4_TTL)
			sib = ICE_FLOW_FIELD_IDX_IPV4_PROT;
		else
			sib = ICE_FLOW_FIELD_IDX_IPV4_TTL;
		break;
	case ICE_FLOW_FIELD_IDX_IPV6_TTL:
	case ICE_FLOW_FIELD_IDX_IPV6_PROT:
		prot_id = seg == 0 ? ICE_PROT_IPV6_OF_OR_S : ICE_PROT_IPV6_IL;

		/* TTL and PROT share the same extraction seq. entry.
		 * Each is considered a sibling to the other in terms of sharing
		 * the same extraction sequence entry.
		 */
		if (fld == ICE_FLOW_FIELD_IDX_IPV6_TTL)
			sib = ICE_FLOW_FIELD_IDX_IPV6_PROT;
		else
			sib = ICE_FLOW_FIELD_IDX_IPV6_TTL;
		break;
	case ICE_FLOW_FIELD_IDX_IPV4_SA:
	case ICE_FLOW_FIELD_IDX_IPV4_DA:
		prot_id = seg == 0 ? ICE_PROT_IPV4_OF_OR_S : ICE_PROT_IPV4_IL;
		break;
	case ICE_FLOW_FIELD_IDX_IPV6_SA:
	case ICE_FLOW_FIELD_IDX_IPV6_DA:
		prot_id = seg == 0 ? ICE_PROT_IPV6_OF_OR_S : ICE_PROT_IPV6_IL;
		break;
	case ICE_FLOW_FIELD_IDX_TCP_SRC_PORT:
	case ICE_FLOW_FIELD_IDX_TCP_DST_PORT:
	case ICE_FLOW_FIELD_IDX_TCP_FLAGS:
		prot_id = ICE_PROT_TCP_IL;
		break;
	case ICE_FLOW_FIELD_IDX_UDP_SRC_PORT:
	case ICE_FLOW_FIELD_IDX_UDP_DST_PORT:
		prot_id = ICE_PROT_UDP_IL_OR_S;
		break;
	case ICE_FLOW_FIELD_IDX_SCTP_SRC_PORT:
	case ICE_FLOW_FIELD_IDX_SCTP_DST_PORT:
		prot_id = ICE_PROT_SCTP_IL;
		break;
	case ICE_FLOW_FIELD_IDX_ARP_SIP:
	case ICE_FLOW_FIELD_IDX_ARP_DIP:
	case ICE_FLOW_FIELD_IDX_ARP_SHA:
	case ICE_FLOW_FIELD_IDX_ARP_DHA:
	case ICE_FLOW_FIELD_IDX_ARP_OP:
		prot_id = ICE_PROT_ARP_OF;
		break;
	case ICE_FLOW_FIELD_IDX_ICMP_TYPE:
	case ICE_FLOW_FIELD_IDX_ICMP_CODE:
		/* ICMP type and code share the same extraction seq. entry */
		prot_id = (params->prof->segs[seg].hdrs &
			   ICE_FLOW_SEG_HDR_IPV4) ?
			ICE_PROT_ICMP_IL : ICE_PROT_ICMPV6_IL;
		sib = fld == ICE_FLOW_FIELD_IDX_ICMP_TYPE ?
			ICE_FLOW_FIELD_IDX_ICMP_CODE :
			ICE_FLOW_FIELD_IDX_ICMP_TYPE;
		break;
	case ICE_FLOW_FIELD_IDX_GRE_KEYID:
		prot_id = ICE_PROT_GRE_OF;
		break;
	default:
		return ICE_ERR_NOT_IMPL;
	}

	/* Each extraction sequence entry is a word in size, and extracts a
	 * word-aligned offset from a protocol header.
	 */
	ese_bits = ICE_FLOW_FV_EXTRACT_SZ * BITS_PER_BYTE;

	flds[fld].xtrct.prot_id = prot_id;
	flds[fld].xtrct.off = (ice_flds_info[fld].off / ese_bits) *
		ICE_FLOW_FV_EXTRACT_SZ;
	flds[fld].xtrct.disp = (u8)(ice_flds_info[fld].off % ese_bits);
	flds[fld].xtrct.idx = params->es_cnt;

	/* Adjust the next field-entry index after accommodating the number of
	 * entries this field consumes
	 */
	cnt = DIVIDE_AND_ROUND_UP(flds[fld].xtrct.disp +
				  ice_flds_info[fld].size, ese_bits);

	/* Fill in the extraction sequence entries needed for this field */
	off = flds[fld].xtrct.off;
	for (i = 0; i < cnt; i++) {
		/* Only consume an extraction sequence entry if there is no
		 * sibling field associated with this field or the sibling entry
		 * already extracts the word shared with this field.
		 */
		if (sib == ICE_FLOW_FIELD_IDX_MAX ||
		    flds[sib].xtrct.prot_id == ICE_PROT_ID_INVAL ||
		    flds[sib].xtrct.off != off) {
			u8 idx;

			/* Make sure the number of extraction sequence required
			 * does not exceed the block's capability
			 */
			if (params->es_cnt >= fv_words)
				return ICE_ERR_MAX_LIMIT;

			/* some blocks require a reversed field vector layout */
			if (hw->blk[params->blk].es.reverse)
				idx = fv_words - params->es_cnt - 1;
			else
				idx = params->es_cnt;

			params->es[idx].prot_id = prot_id;
			params->es[idx].off = off;
			params->es_cnt++;
		}

		off += ICE_FLOW_FV_EXTRACT_SZ;
	}

	return ICE_SUCCESS;
}

/**
 * ice_flow_create_xtrct_seq - Create an extraction sequence for given segments
 * @hw: pointer to the HW struct
 * @params: information about the flow to be processed
 *
 * This function iterates through all matched fields in the given segments, and
 * creates an extraction sequence for the fields.
 */
static enum ice_status
ice_flow_create_xtrct_seq(struct ice_hw *hw,
			  struct ice_flow_prof_params *params)
{
	enum ice_status status = ICE_SUCCESS;
	u8 i;

	for (i = 0; i < params->prof->segs_cnt; i++) {
		u64 match = params->prof->segs[i].match;
		enum ice_flow_field j;

		ice_for_each_set_bit(j, (ice_bitmap_t *)&match,
				     ICE_FLOW_FIELD_IDX_MAX) {
			status = ice_flow_xtract_fld(hw, params, i, j);
			if (status)
				return status;
			ice_clear_bit(j, (ice_bitmap_t *)&match);
		}
	}

	return status;
}

/**
 * ice_flow_proc_segs - process all packet segments associated with a profile
 * @hw: pointer to the HW struct
 * @params: information about the flow to be processed
 */
static enum ice_status
ice_flow_proc_segs(struct ice_hw *hw, struct ice_flow_prof_params *params)
{
	enum ice_status status;

	status = ice_flow_proc_seg_hdrs(params);
	if (status)
		return status;

	status = ice_flow_create_xtrct_seq(hw, params);
	if (status)
		return status;

	switch (params->blk) {
	case ICE_BLK_RSS:
		status = ICE_SUCCESS;
		break;
	default:
		return ICE_ERR_NOT_IMPL;
	}

	return status;
}

#define ICE_FLOW_FIND_PROF_CHK_FLDS	0x00000001
#define ICE_FLOW_FIND_PROF_CHK_VSI	0x00000002
#define ICE_FLOW_FIND_PROF_NOT_CHK_DIR	0x00000004

/**
 * ice_flow_find_prof_conds - Find a profile matching headers and conditions
 * @hw: pointer to the HW struct
 * @blk: classification stage
 * @dir: flow direction
 * @segs: array of one or more packet segments that describe the flow
 * @segs_cnt: number of packet segments provided
 * @vsi_handle: software VSI handle to check VSI (ICE_FLOW_FIND_PROF_CHK_VSI)
 * @conds: additional conditions to be checked (ICE_FLOW_FIND_PROF_CHK_*)
 */
static struct ice_flow_prof *
ice_flow_find_prof_conds(struct ice_hw *hw, enum ice_block blk,
			 enum ice_flow_dir dir, struct ice_flow_seg_info *segs,
			 u8 segs_cnt, u16 vsi_handle, u32 conds)
{
	struct ice_flow_prof *p, *prof = NULL;

	ice_acquire_lock(&hw->fl_profs_locks[blk]);
	LIST_FOR_EACH_ENTRY(p, &hw->fl_profs[blk], ice_flow_prof, l_entry)
		if ((p->dir == dir || conds & ICE_FLOW_FIND_PROF_NOT_CHK_DIR) &&
		    segs_cnt && segs_cnt == p->segs_cnt) {
			u8 i;

			/* Check for profile-VSI association if specified */
			if ((conds & ICE_FLOW_FIND_PROF_CHK_VSI) &&
			    ice_is_vsi_valid(hw, vsi_handle) &&
			    !ice_is_bit_set(p->vsis, vsi_handle))
				continue;

			/* Protocol headers must be checked. Matched fields are
			 * checked if specified.
			 */
			for (i = 0; i < segs_cnt; i++)
				if (segs[i].hdrs != p->segs[i].hdrs ||
				    ((conds & ICE_FLOW_FIND_PROF_CHK_FLDS) &&
				     segs[i].match != p->segs[i].match))
					break;

			/* A match is found if all segments are matched */
			if (i == segs_cnt) {
				prof = p;
				break;
			}
		}
	ice_release_lock(&hw->fl_profs_locks[blk]);

	return prof;
}

/**
 * ice_flow_find_prof - Look up a profile matching headers and matched fields
 * @hw: pointer to the HW struct
 * @blk: classification stage
 * @dir: flow direction
 * @segs: array of one or more packet segments that describe the flow
 * @segs_cnt: number of packet segments provided
 */
u64
ice_flow_find_prof(struct ice_hw *hw, enum ice_block blk, enum ice_flow_dir dir,
		   struct ice_flow_seg_info *segs, u8 segs_cnt)
{
	struct ice_flow_prof *p;

	p = ice_flow_find_prof_conds(hw, blk, dir, segs, segs_cnt,
				     ICE_MAX_VSI, ICE_FLOW_FIND_PROF_CHK_FLDS);

	return p ? p->id : ICE_FLOW_PROF_ID_INVAL;
}

/**
 * ice_flow_find_prof_id - Look up a profile with given profile ID
 * @hw: pointer to the HW struct
 * @blk: classification stage
 * @prof_id: unique ID to identify this flow profile
 */
static struct ice_flow_prof *
ice_flow_find_prof_id(struct ice_hw *hw, enum ice_block blk, u64 prof_id)
{
	struct ice_flow_prof *p;

	LIST_FOR_EACH_ENTRY(p, &hw->fl_profs[blk], ice_flow_prof, l_entry)
		if (p->id == prof_id)
			return p;

	return NULL;
}

/**
 * ice_flow_get_hw_prof - return the HW profile for a specific profile ID handle
 * @hw: pointer to the HW struct
 * @blk: classification stage
 * @prof_id: the profile ID handle
 * @hw_prof_id: pointer to variable to receive the HW profile ID
 */
enum ice_status
ice_flow_get_hw_prof(struct ice_hw *hw, enum ice_block blk, u64 prof_id,
		     u8 *hw_prof_id)
{
	enum ice_status status = ICE_ERR_DOES_NOT_EXIST;
	struct ice_prof_map *map;

	ice_acquire_lock(&hw->blk[blk].es.prof_map_lock);
	map = ice_search_prof_id(hw, blk, prof_id);
	if (map) {
		*hw_prof_id = map->prof_id;
		status = ICE_SUCCESS;
	}
	ice_release_lock(&hw->blk[blk].es.prof_map_lock);
	return status;
}

/**
 * ice_flow_add_prof_sync - Add a flow profile for packet segments and fields
 * @hw: pointer to the HW struct
 * @blk: classification stage
 * @dir: flow direction
 * @prof_id: unique ID to identify this flow profile
 * @segs: array of one or more packet segments that describe the flow
 * @segs_cnt: number of packet segments provided
 * @acts: array of default actions
 * @acts_cnt: number of default actions
 * @prof: stores the returned flow profile added
 *
 * Assumption: the caller has acquired the lock to the profile list
 */
static enum ice_status
ice_flow_add_prof_sync(struct ice_hw *hw, enum ice_block blk,
		       enum ice_flow_dir dir, u64 prof_id,
		       struct ice_flow_seg_info *segs, u8 segs_cnt,
		       struct ice_flow_action *acts, u8 acts_cnt,
		       struct ice_flow_prof **prof)
{
	struct ice_flow_prof_params *params;
	enum ice_status status;
	u8 i;

	if (!prof || (acts_cnt && !acts))
		return ICE_ERR_BAD_PTR;

	params = (struct ice_flow_prof_params *)ice_malloc(hw, sizeof(*params));
	if (!params)
		return ICE_ERR_NO_MEMORY;

	params->prof = (struct ice_flow_prof *)
		ice_malloc(hw, sizeof(*params->prof));
	if (!params->prof) {
		status = ICE_ERR_NO_MEMORY;
		goto free_params;
	}

	/* initialize extraction sequence to all invalid (0xff) */
	for (i = 0; i < ICE_MAX_FV_WORDS; i++) {
		params->es[i].prot_id = ICE_PROT_INVALID;
		params->es[i].off = ICE_FV_OFFSET_INVAL;
	}

	params->blk = blk;
	params->prof->id = prof_id;
	params->prof->dir = dir;
	params->prof->segs_cnt = segs_cnt;

	/* Make a copy of the segments that need to be persistent in the flow
	 * profile instance
	 */
	for (i = 0; i < segs_cnt; i++)
		ice_memcpy(&params->prof->segs[i], &segs[i], sizeof(*segs),
			   ICE_NONDMA_TO_NONDMA);

	status = ice_flow_proc_segs(hw, params);
	if (status) {
		ice_debug(hw, ICE_DBG_FLOW, "Error processing a flow's packet segments\n");
		goto out;
	}

	/* Add a HW profile for this flow profile */
	status = ice_add_prof(hw, blk, prof_id, (u8 *)params->ptypes,
			      params->es);
	if (status) {
		ice_debug(hw, ICE_DBG_FLOW, "Error adding a HW flow profile\n");
		goto out;
	}

	*prof = params->prof;

out:
	if (status) {
		ice_free(hw, params->prof);
	}
free_params:
	ice_free(hw, params);

	return status;
}

/**
 * ice_flow_rem_prof_sync - remove a flow profile
 * @hw: pointer to the hardware structure
 * @blk: classification stage
 * @prof: pointer to flow profile to remove
 *
 * Assumption: the caller has acquired the lock to the profile list
 */
static enum ice_status
ice_flow_rem_prof_sync(struct ice_hw *hw, enum ice_block blk,
		       struct ice_flow_prof *prof)
{
	enum ice_status status;

	/* Remove all hardware profiles associated with this flow profile */
	status = ice_rem_prof(hw, blk, prof->id);
	if (!status) {
		LIST_DEL(&prof->l_entry);
		ice_free(hw, prof);
	}

	return status;
}

/**
 * ice_flow_assoc_vsig_vsi - associate a VSI with VSIG
 * @hw: pointer to the hardware structure
 * @blk: classification stage
 * @vsi_handle: software VSI handle
 * @vsig: target VSI group
 *
 * Assumption: the caller has already verified that the VSI to
 * be added has the same characteristics as the VSIG and will
 * thereby have access to all resources added to that VSIG.
 */
enum ice_status
ice_flow_assoc_vsig_vsi(struct ice_hw *hw, enum ice_block blk, u16 vsi_handle,
			u16 vsig)
{
	enum ice_status status;

	if (!ice_is_vsi_valid(hw, vsi_handle) || blk >= ICE_BLK_COUNT)
		return ICE_ERR_PARAM;

	ice_acquire_lock(&hw->fl_profs_locks[blk]);
	status = ice_add_vsi_flow(hw, blk, ice_get_hw_vsi_num(hw, vsi_handle),
				  vsig);
	ice_release_lock(&hw->fl_profs_locks[blk]);

	return status;
}

/**
 * ice_flow_assoc_prof - associate a VSI with a flow profile
 * @hw: pointer to the hardware structure
 * @blk: classification stage
 * @prof: pointer to flow profile
 * @vsi_handle: software VSI handle
 *
 * Assumption: the caller has acquired the lock to the profile list
 * and the software VSI handle has been validated
 */
static enum ice_status
ice_flow_assoc_prof(struct ice_hw *hw, enum ice_block blk,
		    struct ice_flow_prof *prof, u16 vsi_handle)
{
	enum ice_status status = ICE_SUCCESS;

	if (!ice_is_bit_set(prof->vsis, vsi_handle)) {
		status = ice_add_prof_id_flow(hw, blk,
					      ice_get_hw_vsi_num(hw,
								 vsi_handle),
					      prof->id);
		if (!status)
			ice_set_bit(vsi_handle, prof->vsis);
		else
			ice_debug(hw, ICE_DBG_FLOW, "HW profile add failed, %d\n",
				  status);
	}

	return status;
}

/**
 * ice_flow_disassoc_prof - disassociate a VSI from a flow profile
 * @hw: pointer to the hardware structure
 * @blk: classification stage
 * @prof: pointer to flow profile
 * @vsi_handle: software VSI handle
 *
 * Assumption: the caller has acquired the lock to the profile list
 * and the software VSI handle has been validated
 */
static enum ice_status
ice_flow_disassoc_prof(struct ice_hw *hw, enum ice_block blk,
		       struct ice_flow_prof *prof, u16 vsi_handle)
{
	enum ice_status status = ICE_SUCCESS;

	if (ice_is_bit_set(prof->vsis, vsi_handle)) {
		status = ice_rem_prof_id_flow(hw, blk,
					      ice_get_hw_vsi_num(hw,
								 vsi_handle),
					      prof->id);
		if (!status)
			ice_clear_bit(vsi_handle, prof->vsis);
		else
			ice_debug(hw, ICE_DBG_FLOW, "HW profile remove failed, %d\n",
				  status);
	}

	return status;
}

/**
 * ice_flow_add_prof - Add a flow profile for packet segments and matched fields
 * @hw: pointer to the HW struct
 * @blk: classification stage
 * @dir: flow direction
 * @prof_id: unique ID to identify this flow profile
 * @segs: array of one or more packet segments that describe the flow
 * @segs_cnt: number of packet segments provided
 * @acts: array of default actions
 * @acts_cnt: number of default actions
 * @prof: stores the returned flow profile added
 */
static enum ice_status
ice_flow_add_prof(struct ice_hw *hw, enum ice_block blk, enum ice_flow_dir dir,
		  u64 prof_id, struct ice_flow_seg_info *segs, u8 segs_cnt,
		  struct ice_flow_action *acts, u8 acts_cnt,
		  struct ice_flow_prof **prof)
{
	enum ice_status status;

	if (segs_cnt > ICE_FLOW_SEG_MAX)
		return ICE_ERR_MAX_LIMIT;

	if (!segs_cnt)
		return ICE_ERR_PARAM;

	if (!segs)
		return ICE_ERR_BAD_PTR;

	status = ice_flow_val_hdrs(segs, segs_cnt);
	if (status)
		return status;

	ice_acquire_lock(&hw->fl_profs_locks[blk]);

	status = ice_flow_add_prof_sync(hw, blk, dir, prof_id, segs, segs_cnt,
					acts, acts_cnt, prof);
	if (!status)
		LIST_ADD(&(*prof)->l_entry, &hw->fl_profs[blk]);

	ice_release_lock(&hw->fl_profs_locks[blk]);

	return status;
}

/**
 * ice_flow_rem_prof - Remove a flow profile and all entries associated with it
 * @hw: pointer to the HW struct
 * @blk: the block for which the flow profile is to be removed
 * @prof_id: unique ID of the flow profile to be removed
 */
static enum ice_status
ice_flow_rem_prof(struct ice_hw *hw, enum ice_block blk, u64 prof_id)
{
	struct ice_flow_prof *prof;
	enum ice_status status;

	ice_acquire_lock(&hw->fl_profs_locks[blk]);

	prof = ice_flow_find_prof_id(hw, blk, prof_id);
	if (!prof) {
		status = ICE_ERR_DOES_NOT_EXIST;
		goto out;
	}

	/* prof becomes invalid after the call */
	status = ice_flow_rem_prof_sync(hw, blk, prof);

out:
	ice_release_lock(&hw->fl_profs_locks[blk]);

	return status;
}

/**
 * ice_flow_set_fld_ext - specifies locations of field from entry's input buffer
 * @seg: packet segment the field being set belongs to
 * @fld: field to be set
 * @field_type: type of the field
 * @val_loc: if not ICE_FLOW_FLD_OFF_INVAL, location of the value to match from
 *           entry's input buffer
 * @mask_loc: if not ICE_FLOW_FLD_OFF_INVAL, location of mask value from entry's
 *            input buffer
 * @last_loc: if not ICE_FLOW_FLD_OFF_INVAL, location of last/upper value from
 *            entry's input buffer
 *
 * This helper function stores information of a field being matched, including
 * the type of the field and the locations of the value to match, the mask, and
 * the upper-bound value in the start of the input buffer for a flow entry.
 * This function should only be used for fixed-size data structures.
 *
 * This function also opportunistically determines the protocol headers to be
 * present based on the fields being set. Some fields cannot be used alone to
 * determine the protocol headers present. Sometimes, fields for particular
 * protocol headers are not matched. In those cases, the protocol headers
 * must be explicitly set.
 */
static void
ice_flow_set_fld_ext(struct ice_flow_seg_info *seg, enum ice_flow_field fld,
		     enum ice_flow_fld_match_type field_type, u16 val_loc,
		     u16 mask_loc, u16 last_loc)
{
	u64 bit = BIT_ULL(fld);

	seg->match |= bit;
	if (field_type == ICE_FLOW_FLD_TYPE_RANGE)
		seg->range |= bit;

	seg->fields[fld].type = field_type;
	seg->fields[fld].src.val = val_loc;
	seg->fields[fld].src.mask = mask_loc;
	seg->fields[fld].src.last = last_loc;

	ICE_FLOW_SET_HDRS(seg, ice_flds_info[fld].hdr);
}

/**
 * ice_flow_set_fld - specifies locations of field from entry's input buffer
 * @seg: packet segment the field being set belongs to
 * @fld: field to be set
 * @val_loc: if not ICE_FLOW_FLD_OFF_INVAL, location of the value to match from
 *           entry's input buffer
 * @mask_loc: if not ICE_FLOW_FLD_OFF_INVAL, location of mask value from entry's
 *            input buffer
 * @last_loc: if not ICE_FLOW_FLD_OFF_INVAL, location of last/upper value from
 *            entry's input buffer
 * @range: indicate if field being matched is to be in a range
 *
 * This function specifies the locations, in the form of byte offsets from the
 * start of the input buffer for a flow entry, from where the value to match,
 * the mask value, and upper value can be extracted. These locations are then
 * stored in the flow profile. When adding a flow entry associated with the
 * flow profile, these locations will be used to quickly extract the values and
 * create the content of a match entry. This function should only be used for
 * fixed-size data structures.
 */
static void
ice_flow_set_fld(struct ice_flow_seg_info *seg, enum ice_flow_field fld,
		 u16 val_loc, u16 mask_loc, u16 last_loc, bool range)
{
	enum ice_flow_fld_match_type t = range ?
		ICE_FLOW_FLD_TYPE_RANGE : ICE_FLOW_FLD_TYPE_REG;

	ice_flow_set_fld_ext(seg, fld, t, val_loc, mask_loc, last_loc);
}

/**
 * ice_flow_set_fld_prefix - sets locations of prefix field from entry's buf
 * @seg: packet segment the field being set belongs to
 * @fld: field to be set
 * @val_loc: if not ICE_FLOW_FLD_OFF_INVAL, location of the value to match from
 *           entry's input buffer
 * @pref_loc: location of prefix value from entry's input buffer
 * @pref_sz: size of the location holding the prefix value
 *
 * This function specifies the locations, in the form of byte offsets from the
 * start of the input buffer for a flow entry, from where the value to match
 * and the IPv4 prefix value can be extracted. These locations are then stored
 * in the flow profile. When adding flow entries to the associated flow profile,
 * these locations can be used to quickly extract the values to create the
 * content of a match entry. This function should only be used for fixed-size
 * data structures.
 */
void
ice_flow_set_fld_prefix(struct ice_flow_seg_info *seg, enum ice_flow_field fld,
			u16 val_loc, u16 pref_loc, u8 pref_sz)
{
	/* For this type of field, the "mask" location is for the prefix value's
	 * location and the "last" location is for the size of the location of
	 * the prefix value.
	 */
	ice_flow_set_fld_ext(seg, fld, ICE_FLOW_FLD_TYPE_PREFIX, val_loc,
			     pref_loc, (u16)pref_sz);
}

#define ICE_FLOW_RSS_SEG_HDR_L3_MASKS \
	(ICE_FLOW_SEG_HDR_IPV4 | ICE_FLOW_SEG_HDR_IPV6)

#define ICE_FLOW_RSS_SEG_HDR_L4_MASKS \
	(ICE_FLOW_SEG_HDR_TCP | ICE_FLOW_SEG_HDR_UDP | ICE_FLOW_SEG_HDR_SCTP)

#define ICE_FLOW_RSS_SEG_HDR_VAL_MASKS \
	(ICE_FLOW_RSS_SEG_HDR_L3_MASKS | \
	 ICE_FLOW_RSS_SEG_HDR_L4_MASKS)

/**
 * ice_flow_set_rss_seg_info - setup packet segments for RSS
 * @segs: pointer to the flow field segment(s)
 * @seg_cnt: segment count
 * @cfg: configure parameters
 *
 * Helper function to extract fields from hash bitmap and use flow
 * header value to set flow field segment for further use in flow
 * profile entry or removal.
 */
static enum ice_status
ice_flow_set_rss_seg_info(struct ice_flow_seg_info *segs, u8 seg_cnt,
			  const struct ice_rss_hash_cfg *cfg)
{
	struct ice_flow_seg_info *seg;
	u64 val;
	u8 i;

	/* set inner most segment */
	seg = &segs[seg_cnt - 1];

	ice_for_each_set_bit(i, (const ice_bitmap_t *)&cfg->hash_flds,
			     ICE_FLOW_FIELD_IDX_MAX)
		ice_flow_set_fld(seg, (enum ice_flow_field)i,
				 ICE_FLOW_FLD_OFF_INVAL, ICE_FLOW_FLD_OFF_INVAL,
				 ICE_FLOW_FLD_OFF_INVAL, false);

	ICE_FLOW_SET_HDRS(seg, cfg->addl_hdrs);

	/* set outer most header */
	if (cfg->hdr_type == ICE_RSS_INNER_HEADERS_W_OUTER_IPV4)
		segs[ICE_RSS_OUTER_HEADERS].hdrs |= ICE_FLOW_SEG_HDR_IPV4 |
						   ICE_FLOW_SEG_HDR_IPV_OTHER;
	else if (cfg->hdr_type == ICE_RSS_INNER_HEADERS_W_OUTER_IPV6)
		segs[ICE_RSS_OUTER_HEADERS].hdrs |= ICE_FLOW_SEG_HDR_IPV6 |
						   ICE_FLOW_SEG_HDR_IPV_OTHER;

	if (seg->hdrs & ~ICE_FLOW_RSS_SEG_HDR_VAL_MASKS)
		return ICE_ERR_PARAM;

	val = (u64)(seg->hdrs & ICE_FLOW_RSS_SEG_HDR_L3_MASKS);
	if (val && !ice_is_pow2(val))
		return ICE_ERR_CFG;

	val = (u64)(seg->hdrs & ICE_FLOW_RSS_SEG_HDR_L4_MASKS);
	if (val && !ice_is_pow2(val))
		return ICE_ERR_CFG;

	return ICE_SUCCESS;
}

/**
 * ice_rem_vsi_rss_list - remove VSI from RSS list
 * @hw: pointer to the hardware structure
 * @vsi_handle: software VSI handle
 *
 * Remove the VSI from all RSS configurations in the list.
 */
void ice_rem_vsi_rss_list(struct ice_hw *hw, u16 vsi_handle)
{
	struct ice_rss_cfg *r, *tmp;

	if (LIST_EMPTY(&hw->rss_list_head))
		return;

	ice_acquire_lock(&hw->rss_locks);
	LIST_FOR_EACH_ENTRY_SAFE(r, tmp, &hw->rss_list_head,
				 ice_rss_cfg, l_entry)
		if (ice_test_and_clear_bit(vsi_handle, r->vsis))
			if (!ice_is_any_bit_set(r->vsis, ICE_MAX_VSI)) {
				LIST_DEL(&r->l_entry);
				ice_free(hw, r);
			}
	ice_release_lock(&hw->rss_locks);
}

/**
 * ice_rem_vsi_rss_cfg - remove RSS configurations associated with VSI
 * @hw: pointer to the hardware structure
 * @vsi_handle: software VSI handle
 *
 * This function will iterate through all flow profiles and disassociate
 * the VSI from that profile. If the flow profile has no VSIs it will
 * be removed.
 */
enum ice_status ice_rem_vsi_rss_cfg(struct ice_hw *hw, u16 vsi_handle)
{
	const enum ice_block blk = ICE_BLK_RSS;
	struct ice_flow_prof *p, *t;
	enum ice_status status = ICE_SUCCESS;

	if (!ice_is_vsi_valid(hw, vsi_handle))
		return ICE_ERR_PARAM;

	if (LIST_EMPTY(&hw->fl_profs[blk]))
		return ICE_SUCCESS;

	ice_acquire_lock(&hw->rss_locks);
	LIST_FOR_EACH_ENTRY_SAFE(p, t, &hw->fl_profs[blk], ice_flow_prof,
				 l_entry)
		if (ice_is_bit_set(p->vsis, vsi_handle)) {
			status = ice_flow_disassoc_prof(hw, blk, p, vsi_handle);
			if (status)
				break;

			if (!ice_is_any_bit_set(p->vsis, ICE_MAX_VSI)) {
				status = ice_flow_rem_prof(hw, blk, p->id);
				if (status)
					break;
			}
		}
	ice_release_lock(&hw->rss_locks);

	return status;
}

/**
 * ice_get_rss_hdr_type - get a RSS profile's header type
 * @prof: RSS flow profile
 */
static enum ice_rss_cfg_hdr_type
ice_get_rss_hdr_type(struct ice_flow_prof *prof)
{
	enum ice_rss_cfg_hdr_type hdr_type = ICE_RSS_ANY_HEADERS;

	if (prof->segs_cnt == ICE_FLOW_SEG_SINGLE) {
		hdr_type = ICE_RSS_OUTER_HEADERS;
	} else if (prof->segs_cnt == ICE_FLOW_SEG_MAX) {
		if (prof->segs[ICE_RSS_OUTER_HEADERS].hdrs == ICE_FLOW_SEG_HDR_NONE)
			hdr_type = ICE_RSS_INNER_HEADERS;
		if (prof->segs[ICE_RSS_OUTER_HEADERS].hdrs & ICE_FLOW_SEG_HDR_IPV4)
			hdr_type = ICE_RSS_INNER_HEADERS_W_OUTER_IPV4;
		if (prof->segs[ICE_RSS_OUTER_HEADERS].hdrs & ICE_FLOW_SEG_HDR_IPV6)
			hdr_type = ICE_RSS_INNER_HEADERS_W_OUTER_IPV6;
	}

	return hdr_type;
}

/**
 * ice_rem_rss_list - remove RSS configuration from list
 * @hw: pointer to the hardware structure
 * @vsi_handle: software VSI handle
 * @prof: pointer to flow profile
 *
 * Assumption: lock has already been acquired for RSS list
 */
static void
ice_rem_rss_list(struct ice_hw *hw, u16 vsi_handle, struct ice_flow_prof *prof)
{
	enum ice_rss_cfg_hdr_type hdr_type;
	struct ice_rss_cfg *r, *tmp;

	/* Search for RSS hash fields associated to the VSI that match the
	 * hash configurations associated to the flow profile. If found
	 * remove from the RSS entry list of the VSI context and delete entry.
	 */
	hdr_type = ice_get_rss_hdr_type(prof);
	LIST_FOR_EACH_ENTRY_SAFE(r, tmp, &hw->rss_list_head,
				 ice_rss_cfg, l_entry)
		if (r->hash.hash_flds == prof->segs[prof->segs_cnt - 1].match &&
		    r->hash.addl_hdrs == prof->segs[prof->segs_cnt - 1].hdrs &&
		    r->hash.hdr_type == hdr_type) {
			ice_clear_bit(vsi_handle, r->vsis);
			if (!ice_is_any_bit_set(r->vsis, ICE_MAX_VSI)) {
				LIST_DEL(&r->l_entry);
				ice_free(hw, r);
			}
			return;
		}
}

/**
 * ice_add_rss_list - add RSS configuration to list
 * @hw: pointer to the hardware structure
 * @vsi_handle: software VSI handle
 * @prof: pointer to flow profile
 *
 * Assumption: lock has already been acquired for RSS list
 */
static enum ice_status
ice_add_rss_list(struct ice_hw *hw, u16 vsi_handle, struct ice_flow_prof *prof)
{
	enum ice_rss_cfg_hdr_type hdr_type;
	struct ice_rss_cfg *r, *rss_cfg;

	hdr_type = ice_get_rss_hdr_type(prof);
	LIST_FOR_EACH_ENTRY(r, &hw->rss_list_head,
			    ice_rss_cfg, l_entry)
		if (r->hash.hash_flds == prof->segs[prof->segs_cnt - 1].match &&
		    r->hash.addl_hdrs == prof->segs[prof->segs_cnt - 1].hdrs &&
		    r->hash.hdr_type == hdr_type) {
			ice_set_bit(vsi_handle, r->vsis);
			return ICE_SUCCESS;
		}

	rss_cfg = (struct ice_rss_cfg *)ice_malloc(hw, sizeof(*rss_cfg));
	if (!rss_cfg)
		return ICE_ERR_NO_MEMORY;

	rss_cfg->hash.hash_flds = prof->segs[prof->segs_cnt - 1].match;
	rss_cfg->hash.addl_hdrs = prof->segs[prof->segs_cnt - 1].hdrs;
	rss_cfg->hash.hdr_type = hdr_type;
	rss_cfg->hash.symm = prof->cfg.symm;
	ice_set_bit(vsi_handle, rss_cfg->vsis);

	LIST_ADD_TAIL(&rss_cfg->l_entry, &hw->rss_list_head);

	return ICE_SUCCESS;
}

#define ICE_FLOW_PROF_HASH_S	0
#define ICE_FLOW_PROF_HASH_M	(0xFFFFFFFFULL << ICE_FLOW_PROF_HASH_S)
#define ICE_FLOW_PROF_HDR_S	32
#define ICE_FLOW_PROF_HDR_M	(0x3FFFFFFFULL << ICE_FLOW_PROF_HDR_S)
#define ICE_FLOW_PROF_ENCAP_S	62
#define ICE_FLOW_PROF_ENCAP_M	(0x3ULL << ICE_FLOW_PROF_ENCAP_S)

/* Flow profile ID format:
 * [0:31] - Packet match fields
 * [32:61] - Protocol header
 * [62:63] - Encapsulation flag:
 *	     0 if non-tunneled
 *	     1 if tunneled
 *	     2 for tunneled with outer ipv4
 *	     3 for tunneled with outer ipv6
 */
#define ICE_FLOW_GEN_PROFID(hash, hdr, encap) \
	((u64)(((u64)(hash) & ICE_FLOW_PROF_HASH_M) | \
	       (((u64)(hdr) << ICE_FLOW_PROF_HDR_S) & ICE_FLOW_PROF_HDR_M) | \
	       (((u64)(encap) << ICE_FLOW_PROF_ENCAP_S) & ICE_FLOW_PROF_ENCAP_M)))

/**
 * ice_add_rss_cfg_sync - add an RSS configuration
 * @hw: pointer to the hardware structure
 * @vsi_handle: software VSI handle
 * @cfg: configure parameters
 *
 * Assumption: lock has already been acquired for RSS list
 */
static enum ice_status
ice_add_rss_cfg_sync(struct ice_hw *hw, u16 vsi_handle,
		     const struct ice_rss_hash_cfg *cfg)
{
	const enum ice_block blk = ICE_BLK_RSS;
	struct ice_flow_prof *prof = NULL;
	struct ice_flow_seg_info *segs;
	enum ice_status status;
	u8 segs_cnt;

	if (cfg->symm)
		return ICE_ERR_PARAM;

	segs_cnt = (cfg->hdr_type == ICE_RSS_OUTER_HEADERS) ?
			ICE_FLOW_SEG_SINGLE : ICE_FLOW_SEG_MAX;

	segs = (struct ice_flow_seg_info *)ice_calloc(hw, segs_cnt,
						      sizeof(*segs));
	if (!segs)
		return ICE_ERR_NO_MEMORY;

	/* Construct the packet segment info from the hashed fields */
	status = ice_flow_set_rss_seg_info(segs, segs_cnt, cfg);
	if (status)
		goto exit;

	/* Search for a flow profile that has matching headers, hash fields
	 * and has the input VSI associated to it. If found, no further
	 * operations required and exit.
	 */
	prof = ice_flow_find_prof_conds(hw, blk, ICE_FLOW_RX, segs, segs_cnt,
					vsi_handle,
					ICE_FLOW_FIND_PROF_CHK_FLDS |
					ICE_FLOW_FIND_PROF_CHK_VSI);
	if (prof)
		goto exit;

	/* Check if a flow profile exists with the same protocol headers and
	 * associated with the input VSI. If so disassociate the VSI from
	 * this profile. The VSI will be added to a new profile created with
	 * the protocol header and new hash field configuration.
	 */
	prof = ice_flow_find_prof_conds(hw, blk, ICE_FLOW_RX, segs, segs_cnt,
					vsi_handle, ICE_FLOW_FIND_PROF_CHK_VSI);
	if (prof) {
		status = ice_flow_disassoc_prof(hw, blk, prof, vsi_handle);
		if (!status)
			ice_rem_rss_list(hw, vsi_handle, prof);
		else
			goto exit;

		/* Remove profile if it has no VSIs associated */
		if (!ice_is_any_bit_set(prof->vsis, ICE_MAX_VSI)) {
			status = ice_flow_rem_prof(hw, blk, prof->id);
			if (status)
				goto exit;
		}
	}

	/* Search for a profile that has same match fields only. If this
	 * exists then associate the VSI to this profile.
	 */
	prof = ice_flow_find_prof_conds(hw, blk, ICE_FLOW_RX, segs, segs_cnt,
					vsi_handle,
					ICE_FLOW_FIND_PROF_CHK_FLDS);
	if (prof) {
		status = ice_flow_assoc_prof(hw, blk, prof, vsi_handle);
		if (!status)
			status = ice_add_rss_list(hw, vsi_handle, prof);
		goto exit;
	}

	/* Create a new flow profile with generated profile and packet
	 * segment information.
	 */
	status = ice_flow_add_prof(hw, blk, ICE_FLOW_RX,
				   ICE_FLOW_GEN_PROFID(cfg->hash_flds,
						       segs[segs_cnt - 1].hdrs,
						       cfg->hdr_type),
				   segs, segs_cnt, NULL, 0, &prof);
	if (status)
		goto exit;

	status = ice_flow_assoc_prof(hw, blk, prof, vsi_handle);
	/* If association to a new flow profile failed then this profile can
	 * be removed.
	 */
	if (status) {
		ice_flow_rem_prof(hw, blk, prof->id);
		goto exit;
	}

	status = ice_add_rss_list(hw, vsi_handle, prof);

	prof->cfg.symm = cfg->symm;

exit:
	ice_free(hw, segs);
	return status;
}

/**
 * ice_add_rss_cfg - add an RSS configuration with specified hashed fields
 * @hw: pointer to the hardware structure
 * @vsi_handle: software VSI handle
 * @cfg: configure parameters
 *
 * This function will generate a flow profile based on fields associated with
 * the input fields to hash on, the flow type and use the VSI number to add
 * a flow entry to the profile.
 */
enum ice_status
ice_add_rss_cfg(struct ice_hw *hw, u16 vsi_handle,
		const struct ice_rss_hash_cfg *cfg)
{
	struct ice_rss_hash_cfg local_cfg;
	enum ice_status status;

	if (!ice_is_vsi_valid(hw, vsi_handle) ||
	    !cfg || cfg->hdr_type > ICE_RSS_ANY_HEADERS ||
	    cfg->hash_flds == ICE_HASH_INVALID)
		return ICE_ERR_PARAM;

	local_cfg = *cfg;
	if (cfg->hdr_type < ICE_RSS_ANY_HEADERS) {
		ice_acquire_lock(&hw->rss_locks);
		status = ice_add_rss_cfg_sync(hw, vsi_handle, &local_cfg);
		ice_release_lock(&hw->rss_locks);
	} else {
		ice_acquire_lock(&hw->rss_locks);
		local_cfg.hdr_type = ICE_RSS_OUTER_HEADERS;
		status = ice_add_rss_cfg_sync(hw, vsi_handle, &local_cfg);
		if (!status) {
			local_cfg.hdr_type = ICE_RSS_INNER_HEADERS;
			status = ice_add_rss_cfg_sync(hw, vsi_handle,
						      &local_cfg);
		}
		ice_release_lock(&hw->rss_locks);
	}

	return status;
}

/**
 * ice_rem_rss_cfg_sync - remove an existing RSS configuration
 * @hw: pointer to the hardware structure
 * @vsi_handle: software VSI handle
 * @cfg: configure parameters
 *
 * Assumption: lock has already been acquired for RSS list
 */
static enum ice_status
ice_rem_rss_cfg_sync(struct ice_hw *hw, u16 vsi_handle,
		     const struct ice_rss_hash_cfg *cfg)
{
	const enum ice_block blk = ICE_BLK_RSS;
	struct ice_flow_seg_info *segs;
	struct ice_flow_prof *prof;
	enum ice_status status;
	u8 segs_cnt;

	segs_cnt = (cfg->hdr_type == ICE_RSS_OUTER_HEADERS) ?
			ICE_FLOW_SEG_SINGLE : ICE_FLOW_SEG_MAX;
	segs = (struct ice_flow_seg_info *)ice_calloc(hw, segs_cnt,
						      sizeof(*segs));
	if (!segs)
		return ICE_ERR_NO_MEMORY;

	/* Construct the packet segment info from the hashed fields */
	status = ice_flow_set_rss_seg_info(segs, segs_cnt, cfg);
	if (status)
		goto out;

	prof = ice_flow_find_prof_conds(hw, blk, ICE_FLOW_RX, segs, segs_cnt,
					vsi_handle,
					ICE_FLOW_FIND_PROF_CHK_FLDS);
	if (!prof) {
		status = ICE_ERR_DOES_NOT_EXIST;
		goto out;
	}

	status = ice_flow_disassoc_prof(hw, blk, prof, vsi_handle);
	if (status)
		goto out;

	/* Remove RSS configuration from VSI context before deleting
	 * the flow profile.
	 */
	ice_rem_rss_list(hw, vsi_handle, prof);

	if (!ice_is_any_bit_set(prof->vsis, ICE_MAX_VSI))
		status = ice_flow_rem_prof(hw, blk, prof->id);

out:
	ice_free(hw, segs);
	return status;
}

/**
 * ice_rem_rss_cfg - remove an existing RSS config with matching hashed fields
 * @hw: pointer to the hardware structure
 * @vsi_handle: software VSI handle
 * @cfg: configure parameters
 *
 * This function will lookup the flow profile based on the input
 * hash field bitmap, iterate through the profile entry list of
 * that profile and find entry associated with input VSI to be
 * removed. Calls are made to underlying flow apis which will in
 * turn build or update buffers for RSS XLT1 section.
 */
enum ice_status
ice_rem_rss_cfg(struct ice_hw *hw, u16 vsi_handle,
		const struct ice_rss_hash_cfg *cfg)
{
	struct ice_rss_hash_cfg local_cfg;
	enum ice_status status;

	if (!ice_is_vsi_valid(hw, vsi_handle) ||
	    !cfg || cfg->hdr_type > ICE_RSS_ANY_HEADERS ||
	    cfg->hash_flds == ICE_HASH_INVALID)
		return ICE_ERR_PARAM;

	ice_acquire_lock(&hw->rss_locks);
	local_cfg = *cfg;
	if (cfg->hdr_type < ICE_RSS_ANY_HEADERS) {
		status = ice_rem_rss_cfg_sync(hw, vsi_handle, &local_cfg);
	} else {
		local_cfg.hdr_type = ICE_RSS_OUTER_HEADERS;
		status = ice_rem_rss_cfg_sync(hw, vsi_handle, &local_cfg);

		if (!status) {
			local_cfg.hdr_type = ICE_RSS_INNER_HEADERS;
			status = ice_rem_rss_cfg_sync(hw, vsi_handle,
						      &local_cfg);
		}
	}
	ice_release_lock(&hw->rss_locks);

	return status;
}

/* Mapping of AVF hash bit fields to an L3-L4 hash combination.
 * As the ice_flow_avf_hdr_field represent individual bit shifts in a hash,
 * convert its values to their appropriate flow L3, L4 values.
 */
#define ICE_FLOW_AVF_RSS_IPV4_MASKS \
	(BIT_ULL(ICE_AVF_FLOW_FIELD_IPV4_OTHER) | \
	 BIT_ULL(ICE_AVF_FLOW_FIELD_FRAG_IPV4))
#define ICE_FLOW_AVF_RSS_TCP_IPV4_MASKS \
	(BIT_ULL(ICE_AVF_FLOW_FIELD_IPV4_TCP_SYN_NO_ACK) | \
	 BIT_ULL(ICE_AVF_FLOW_FIELD_IPV4_TCP))
#define ICE_FLOW_AVF_RSS_UDP_IPV4_MASKS \
	(BIT_ULL(ICE_AVF_FLOW_FIELD_UNICAST_IPV4_UDP) | \
	 BIT_ULL(ICE_AVF_FLOW_FIELD_MULTICAST_IPV4_UDP) | \
	 BIT_ULL(ICE_AVF_FLOW_FIELD_IPV4_UDP))
#define ICE_FLOW_AVF_RSS_ALL_IPV4_MASKS \
	(ICE_FLOW_AVF_RSS_TCP_IPV4_MASKS | ICE_FLOW_AVF_RSS_UDP_IPV4_MASKS | \
	 ICE_FLOW_AVF_RSS_IPV4_MASKS | BIT_ULL(ICE_AVF_FLOW_FIELD_IPV4_SCTP))

#define ICE_FLOW_AVF_RSS_IPV6_MASKS \
	(BIT_ULL(ICE_AVF_FLOW_FIELD_IPV6_OTHER) | \
	 BIT_ULL(ICE_AVF_FLOW_FIELD_FRAG_IPV6))
#define ICE_FLOW_AVF_RSS_UDP_IPV6_MASKS \
	(BIT_ULL(ICE_AVF_FLOW_FIELD_UNICAST_IPV6_UDP) | \
	 BIT_ULL(ICE_AVF_FLOW_FIELD_MULTICAST_IPV6_UDP) | \
	 BIT_ULL(ICE_AVF_FLOW_FIELD_IPV6_UDP))
#define ICE_FLOW_AVF_RSS_TCP_IPV6_MASKS \
	(BIT_ULL(ICE_AVF_FLOW_FIELD_IPV6_TCP_SYN_NO_ACK) | \
	 BIT_ULL(ICE_AVF_FLOW_FIELD_IPV6_TCP))
#define ICE_FLOW_AVF_RSS_ALL_IPV6_MASKS \
	(ICE_FLOW_AVF_RSS_TCP_IPV6_MASKS | ICE_FLOW_AVF_RSS_UDP_IPV6_MASKS | \
	 ICE_FLOW_AVF_RSS_IPV6_MASKS | BIT_ULL(ICE_AVF_FLOW_FIELD_IPV6_SCTP))

/**
 * ice_add_avf_rss_cfg - add an RSS configuration for AVF driver
 * @hw: pointer to the hardware structure
 * @vsi_handle: software VSI handle
 * @avf_hash: hash bit fields (ICE_AVF_FLOW_FIELD_*) to configure
 *
 * This function will take the hash bitmap provided by the AVF driver via a
 * message, convert it to ICE-compatible values, and configure RSS flow
 * profiles.
 */
enum ice_status
ice_add_avf_rss_cfg(struct ice_hw *hw, u16 vsi_handle, u64 avf_hash)
{
	enum ice_status status = ICE_SUCCESS;
	struct ice_rss_hash_cfg hcfg;
	u64 hash_flds;

	if (avf_hash == ICE_AVF_FLOW_FIELD_INVALID ||
	    !ice_is_vsi_valid(hw, vsi_handle))
		return ICE_ERR_PARAM;

	/* Make sure no unsupported bits are specified */
	if (avf_hash & ~(ICE_FLOW_AVF_RSS_ALL_IPV4_MASKS |
			 ICE_FLOW_AVF_RSS_ALL_IPV6_MASKS))
		return ICE_ERR_CFG;

	hash_flds = avf_hash;

	/* Always create an L3 RSS configuration for any L4 RSS configuration */
	if (hash_flds & ICE_FLOW_AVF_RSS_ALL_IPV4_MASKS)
		hash_flds |= ICE_FLOW_AVF_RSS_IPV4_MASKS;

	if (hash_flds & ICE_FLOW_AVF_RSS_ALL_IPV6_MASKS)
		hash_flds |= ICE_FLOW_AVF_RSS_IPV6_MASKS;

	/* Create the corresponding RSS configuration for each valid hash bit */
	while (hash_flds) {
		u64 rss_hash = ICE_HASH_INVALID;

		if (hash_flds & ICE_FLOW_AVF_RSS_ALL_IPV4_MASKS) {
			if (hash_flds & ICE_FLOW_AVF_RSS_IPV4_MASKS) {
				rss_hash = ICE_FLOW_HASH_IPV4;
				hash_flds &= ~ICE_FLOW_AVF_RSS_IPV4_MASKS;
			} else if (hash_flds &
				   ICE_FLOW_AVF_RSS_TCP_IPV4_MASKS) {
				rss_hash = ICE_FLOW_HASH_IPV4 |
					ICE_FLOW_HASH_TCP_PORT;
				hash_flds &= ~ICE_FLOW_AVF_RSS_TCP_IPV4_MASKS;
			} else if (hash_flds &
				   ICE_FLOW_AVF_RSS_UDP_IPV4_MASKS) {
				rss_hash = ICE_FLOW_HASH_IPV4 |
					ICE_FLOW_HASH_UDP_PORT;
				hash_flds &= ~ICE_FLOW_AVF_RSS_UDP_IPV4_MASKS;
			} else if (hash_flds &
				   BIT_ULL(ICE_AVF_FLOW_FIELD_IPV4_SCTP)) {
				rss_hash = ICE_FLOW_HASH_IPV4 |
					ICE_FLOW_HASH_SCTP_PORT;
				hash_flds &=
					~BIT_ULL(ICE_AVF_FLOW_FIELD_IPV4_SCTP);
			}
		} else if (hash_flds & ICE_FLOW_AVF_RSS_ALL_IPV6_MASKS) {
			if (hash_flds & ICE_FLOW_AVF_RSS_IPV6_MASKS) {
				rss_hash = ICE_FLOW_HASH_IPV6;
				hash_flds &= ~ICE_FLOW_AVF_RSS_IPV6_MASKS;
			} else if (hash_flds &
				   ICE_FLOW_AVF_RSS_TCP_IPV6_MASKS) {
				rss_hash = ICE_FLOW_HASH_IPV6 |
					ICE_FLOW_HASH_TCP_PORT;
				hash_flds &= ~ICE_FLOW_AVF_RSS_TCP_IPV6_MASKS;
			} else if (hash_flds &
				   ICE_FLOW_AVF_RSS_UDP_IPV6_MASKS) {
				rss_hash = ICE_FLOW_HASH_IPV6 |
					ICE_FLOW_HASH_UDP_PORT;
				hash_flds &= ~ICE_FLOW_AVF_RSS_UDP_IPV6_MASKS;
			} else if (hash_flds &
				   BIT_ULL(ICE_AVF_FLOW_FIELD_IPV6_SCTP)) {
				rss_hash = ICE_FLOW_HASH_IPV6 |
					ICE_FLOW_HASH_SCTP_PORT;
				hash_flds &=
					~BIT_ULL(ICE_AVF_FLOW_FIELD_IPV6_SCTP);
			}
		}

		if (rss_hash == ICE_HASH_INVALID)
			return ICE_ERR_OUT_OF_RANGE;

		hcfg.addl_hdrs = ICE_FLOW_SEG_HDR_NONE;
		hcfg.hash_flds = rss_hash;
		hcfg.symm = false;
		hcfg.hdr_type = ICE_RSS_ANY_HEADERS;
		status = ice_add_rss_cfg(hw, vsi_handle, &hcfg);
		if (status)
			break;
	}

	return status;
}

/**
 * ice_replay_rss_cfg - replay RSS configurations associated with VSI
 * @hw: pointer to the hardware structure
 * @vsi_handle: software VSI handle
 */
enum ice_status ice_replay_rss_cfg(struct ice_hw *hw, u16 vsi_handle)
{
	enum ice_status status = ICE_SUCCESS;
	struct ice_rss_cfg *r;

	if (!ice_is_vsi_valid(hw, vsi_handle))
		return ICE_ERR_PARAM;

	ice_acquire_lock(&hw->rss_locks);
	LIST_FOR_EACH_ENTRY(r, &hw->rss_list_head,
			    ice_rss_cfg, l_entry) {
		if (ice_is_bit_set(r->vsis, vsi_handle)) {
			status = ice_add_rss_cfg_sync(hw, vsi_handle, &r->hash);
			if (status)
				break;
		}
	}
	ice_release_lock(&hw->rss_locks);

	return status;
}

/**
 * ice_get_rss_cfg - returns hashed fields for the given header types
 * @hw: pointer to the hardware structure
 * @vsi_handle: software VSI handle
 * @hdrs: protocol header type
 *
 * This function will return the match fields of the first instance of flow
 * profile having the given header types and containing input VSI
 */
u64 ice_get_rss_cfg(struct ice_hw *hw, u16 vsi_handle, u32 hdrs)
{
	u64 rss_hash = ICE_HASH_INVALID;
	struct ice_rss_cfg *r;

	/* verify if the protocol header is non zero and VSI is valid */
	if (hdrs == ICE_FLOW_SEG_HDR_NONE || !ice_is_vsi_valid(hw, vsi_handle))
		return ICE_HASH_INVALID;

	ice_acquire_lock(&hw->rss_locks);
	LIST_FOR_EACH_ENTRY(r, &hw->rss_list_head,
			    ice_rss_cfg, l_entry)
		if (ice_is_bit_set(r->vsis, vsi_handle) &&
		    r->hash.addl_hdrs == hdrs) {
			rss_hash = r->hash.hash_flds;
			break;
		}
	ice_release_lock(&hw->rss_locks);

	return rss_hash;
}
