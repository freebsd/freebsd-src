/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2022, Intel Corporation
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

#ifndef _ICE_FLOW_H_
#define _ICE_FLOW_H_

#include "ice_flex_type.h"

#define ICE_IPV4_MAKE_PREFIX_MASK(prefix) ((u32)(~0) << (32 - (prefix)))
#define ICE_FLOW_PROF_ID_INVAL		0xfffffffffffffffful
#define ICE_FLOW_PROF_ID_BYPASS		0
#define ICE_FLOW_PROF_ID_DEFAULT	1
#define ICE_FLOW_ENTRY_HANDLE_INVAL	0
#define ICE_FLOW_VSI_INVAL		0xffff
#define ICE_FLOW_FLD_OFF_INVAL		0xffff

/* Generate flow hash field from flow field type(s) */
#define ICE_FLOW_HASH_IPV4	\
	(BIT_ULL(ICE_FLOW_FIELD_IDX_IPV4_SA) | \
	 BIT_ULL(ICE_FLOW_FIELD_IDX_IPV4_DA))
#define ICE_FLOW_HASH_IPV6	\
	(BIT_ULL(ICE_FLOW_FIELD_IDX_IPV6_SA) | \
	 BIT_ULL(ICE_FLOW_FIELD_IDX_IPV6_DA))
#define ICE_FLOW_HASH_TCP_PORT	\
	(BIT_ULL(ICE_FLOW_FIELD_IDX_TCP_SRC_PORT) | \
	 BIT_ULL(ICE_FLOW_FIELD_IDX_TCP_DST_PORT))
#define ICE_FLOW_HASH_UDP_PORT	\
	(BIT_ULL(ICE_FLOW_FIELD_IDX_UDP_SRC_PORT) | \
	 BIT_ULL(ICE_FLOW_FIELD_IDX_UDP_DST_PORT))
#define ICE_FLOW_HASH_SCTP_PORT	\
	(BIT_ULL(ICE_FLOW_FIELD_IDX_SCTP_SRC_PORT) | \
	 BIT_ULL(ICE_FLOW_FIELD_IDX_SCTP_DST_PORT))

#define ICE_HASH_INVALID	0
#define ICE_HASH_TCP_IPV4	(ICE_FLOW_HASH_IPV4 | ICE_FLOW_HASH_TCP_PORT)
#define ICE_HASH_TCP_IPV6	(ICE_FLOW_HASH_IPV6 | ICE_FLOW_HASH_TCP_PORT)
#define ICE_HASH_UDP_IPV4	(ICE_FLOW_HASH_IPV4 | ICE_FLOW_HASH_UDP_PORT)
#define ICE_HASH_UDP_IPV6	(ICE_FLOW_HASH_IPV6 | ICE_FLOW_HASH_UDP_PORT)
#define ICE_HASH_SCTP_IPV4	(ICE_FLOW_HASH_IPV4 | ICE_FLOW_HASH_SCTP_PORT)
#define ICE_HASH_SCTP_IPV6	(ICE_FLOW_HASH_IPV6 | ICE_FLOW_HASH_SCTP_PORT)

/* Protocol header fields within a packet segment. A segment consists of one or
 * more protocol headers that make up a logical group of protocol headers. Each
 * logical group of protocol headers encapsulates or is encapsulated using/by
 * tunneling or encapsulation protocols for network virtualization such as GRE,
 * VxLAN, etc.
 */
enum ice_flow_seg_hdr {
	ICE_FLOW_SEG_HDR_NONE		= 0x00000000,
	ICE_FLOW_SEG_HDR_ETH		= 0x00000001,
	ICE_FLOW_SEG_HDR_VLAN		= 0x00000002,
	ICE_FLOW_SEG_HDR_IPV4		= 0x00000004,
	ICE_FLOW_SEG_HDR_IPV6		= 0x00000008,
	ICE_FLOW_SEG_HDR_ARP		= 0x00000010,
	ICE_FLOW_SEG_HDR_ICMP		= 0x00000020,
	ICE_FLOW_SEG_HDR_TCP		= 0x00000040,
	ICE_FLOW_SEG_HDR_UDP		= 0x00000080,
	ICE_FLOW_SEG_HDR_SCTP		= 0x00000100,
	ICE_FLOW_SEG_HDR_GRE		= 0x00000200,
	/* The following is an additive bit for ICE_FLOW_SEG_HDR_IPV4 and
	 * ICE_FLOW_SEG_HDR_IPV6.
	 */
	ICE_FLOW_SEG_HDR_IPV_FRAG	= 0x40000000,
	ICE_FLOW_SEG_HDR_IPV_OTHER	= 0x80000000,
};

enum ice_flow_field {
	/* L2 */
	ICE_FLOW_FIELD_IDX_ETH_DA,
	ICE_FLOW_FIELD_IDX_ETH_SA,
	ICE_FLOW_FIELD_IDX_S_VLAN,
	ICE_FLOW_FIELD_IDX_C_VLAN,
	ICE_FLOW_FIELD_IDX_ETH_TYPE,
	/* L3 */
	ICE_FLOW_FIELD_IDX_IPV4_DSCP,
	ICE_FLOW_FIELD_IDX_IPV6_DSCP,
	ICE_FLOW_FIELD_IDX_IPV4_TTL,
	ICE_FLOW_FIELD_IDX_IPV4_PROT,
	ICE_FLOW_FIELD_IDX_IPV6_TTL,
	ICE_FLOW_FIELD_IDX_IPV6_PROT,
	ICE_FLOW_FIELD_IDX_IPV4_SA,
	ICE_FLOW_FIELD_IDX_IPV4_DA,
	ICE_FLOW_FIELD_IDX_IPV6_SA,
	ICE_FLOW_FIELD_IDX_IPV6_DA,
	/* L4 */
	ICE_FLOW_FIELD_IDX_TCP_SRC_PORT,
	ICE_FLOW_FIELD_IDX_TCP_DST_PORT,
	ICE_FLOW_FIELD_IDX_UDP_SRC_PORT,
	ICE_FLOW_FIELD_IDX_UDP_DST_PORT,
	ICE_FLOW_FIELD_IDX_SCTP_SRC_PORT,
	ICE_FLOW_FIELD_IDX_SCTP_DST_PORT,
	ICE_FLOW_FIELD_IDX_TCP_FLAGS,
	/* ARP */
	ICE_FLOW_FIELD_IDX_ARP_SIP,
	ICE_FLOW_FIELD_IDX_ARP_DIP,
	ICE_FLOW_FIELD_IDX_ARP_SHA,
	ICE_FLOW_FIELD_IDX_ARP_DHA,
	ICE_FLOW_FIELD_IDX_ARP_OP,
	/* ICMP */
	ICE_FLOW_FIELD_IDX_ICMP_TYPE,
	ICE_FLOW_FIELD_IDX_ICMP_CODE,
	/* GRE */
	ICE_FLOW_FIELD_IDX_GRE_KEYID,
	 /* The total number of enums must not exceed 64 */
	ICE_FLOW_FIELD_IDX_MAX
};

/* Flow headers and fields for AVF support */
enum ice_flow_avf_hdr_field {
	/* Values 0 - 28 are reserved for future use */
	ICE_AVF_FLOW_FIELD_INVALID		= 0,
	ICE_AVF_FLOW_FIELD_UNICAST_IPV4_UDP	= 29,
	ICE_AVF_FLOW_FIELD_MULTICAST_IPV4_UDP,
	ICE_AVF_FLOW_FIELD_IPV4_UDP,
	ICE_AVF_FLOW_FIELD_IPV4_TCP_SYN_NO_ACK,
	ICE_AVF_FLOW_FIELD_IPV4_TCP,
	ICE_AVF_FLOW_FIELD_IPV4_SCTP,
	ICE_AVF_FLOW_FIELD_IPV4_OTHER,
	ICE_AVF_FLOW_FIELD_FRAG_IPV4,
	/* Values 37-38 are reserved */
	ICE_AVF_FLOW_FIELD_UNICAST_IPV6_UDP	= 39,
	ICE_AVF_FLOW_FIELD_MULTICAST_IPV6_UDP,
	ICE_AVF_FLOW_FIELD_IPV6_UDP,
	ICE_AVF_FLOW_FIELD_IPV6_TCP_SYN_NO_ACK,
	ICE_AVF_FLOW_FIELD_IPV6_TCP,
	ICE_AVF_FLOW_FIELD_IPV6_SCTP,
	ICE_AVF_FLOW_FIELD_IPV6_OTHER,
	ICE_AVF_FLOW_FIELD_FRAG_IPV6,
	ICE_AVF_FLOW_FIELD_RSVD47,
	ICE_AVF_FLOW_FIELD_FCOE_OX,
	ICE_AVF_FLOW_FIELD_FCOE_RX,
	ICE_AVF_FLOW_FIELD_FCOE_OTHER,
	/* Values 51-62 are reserved */
	ICE_AVF_FLOW_FIELD_L2_PAYLOAD		= 63,
	ICE_AVF_FLOW_FIELD_MAX
};

/* Supported RSS offloads  This macro is defined to support
 * VIRTCHNL_OP_GET_RSS_HENA_CAPS ops. PF driver sends the RSS hardware
 * capabilities to the caller of this ops.
 */
#define ICE_DEFAULT_RSS_HENA ( \
	BIT_ULL(ICE_AVF_FLOW_FIELD_IPV4_UDP) | \
	BIT_ULL(ICE_AVF_FLOW_FIELD_IPV4_SCTP) | \
	BIT_ULL(ICE_AVF_FLOW_FIELD_IPV4_TCP) | \
	BIT_ULL(ICE_AVF_FLOW_FIELD_IPV4_OTHER) | \
	BIT_ULL(ICE_AVF_FLOW_FIELD_FRAG_IPV4) | \
	BIT_ULL(ICE_AVF_FLOW_FIELD_IPV6_UDP) | \
	BIT_ULL(ICE_AVF_FLOW_FIELD_IPV6_TCP) | \
	BIT_ULL(ICE_AVF_FLOW_FIELD_IPV6_SCTP) | \
	BIT_ULL(ICE_AVF_FLOW_FIELD_IPV6_OTHER) | \
	BIT_ULL(ICE_AVF_FLOW_FIELD_FRAG_IPV6) | \
	BIT_ULL(ICE_AVF_FLOW_FIELD_IPV4_TCP_SYN_NO_ACK) | \
	BIT_ULL(ICE_AVF_FLOW_FIELD_UNICAST_IPV4_UDP) | \
	BIT_ULL(ICE_AVF_FLOW_FIELD_MULTICAST_IPV4_UDP) | \
	BIT_ULL(ICE_AVF_FLOW_FIELD_IPV6_TCP_SYN_NO_ACK) | \
	BIT_ULL(ICE_AVF_FLOW_FIELD_UNICAST_IPV6_UDP) | \
	BIT_ULL(ICE_AVF_FLOW_FIELD_MULTICAST_IPV6_UDP))

enum ice_rss_cfg_hdr_type {
	ICE_RSS_OUTER_HEADERS, /* take outer headers as inputset. */
	ICE_RSS_INNER_HEADERS, /* take inner headers as inputset. */
	/* take inner headers as inputset for packet with outer ipv4. */
	ICE_RSS_INNER_HEADERS_W_OUTER_IPV4,
	/* take inner headers as inputset for packet with outer ipv6. */
	ICE_RSS_INNER_HEADERS_W_OUTER_IPV6,
	/* take outer headers first then inner headers as inputset */
	/* take inner as inputset for GTPoGRE with outer ipv4 + gre. */
	ICE_RSS_INNER_HEADERS_W_OUTER_IPV4_GRE,
	/* take inner as inputset for GTPoGRE with outer ipv6 + gre. */
	ICE_RSS_INNER_HEADERS_W_OUTER_IPV6_GRE,
	ICE_RSS_ANY_HEADERS
};

struct ice_rss_hash_cfg {
	u32 addl_hdrs; /* protocol header fields */
	u64 hash_flds; /* hash bit field (ICE_FLOW_HASH_*) to configure */
	enum ice_rss_cfg_hdr_type hdr_type; /* to specify inner or outer */
	bool symm; /* symmetric or asymmetric hash */
};

enum ice_flow_dir {
	ICE_FLOW_DIR_UNDEFINED	= 0,
	ICE_FLOW_TX		= 0x01,
	ICE_FLOW_RX		= 0x02,
	ICE_FLOW_TX_RX		= ICE_FLOW_RX | ICE_FLOW_TX
};

enum ice_flow_priority {
	ICE_FLOW_PRIO_LOW,
	ICE_FLOW_PRIO_NORMAL,
	ICE_FLOW_PRIO_HIGH
};

#define ICE_FLOW_SEG_SINGLE		1
#define ICE_FLOW_SEG_MAX		2
#define ICE_FLOW_PROFILE_MAX		1024
#define ICE_FLOW_ACL_FIELD_VECTOR_MAX	32
#define ICE_FLOW_FV_EXTRACT_SZ		2

#define ICE_FLOW_SET_HDRS(seg, val)	((seg)->hdrs |= (u32)(val))

struct ice_flow_seg_xtrct {
	u8 prot_id;	/* Protocol ID of extracted header field */
	u16 off;	/* Starting offset of the field in header in bytes */
	u8 idx;		/* Index of FV entry used */
	u8 disp;	/* Displacement of field in bits fr. FV entry's start */
};

enum ice_flow_fld_match_type {
	ICE_FLOW_FLD_TYPE_REG,		/* Value, mask */
	ICE_FLOW_FLD_TYPE_RANGE,	/* Value, mask, last (upper bound) */
	ICE_FLOW_FLD_TYPE_PREFIX,	/* IP address, prefix, size of prefix */
	ICE_FLOW_FLD_TYPE_SIZE,		/* Value, mask, size of match */
};

struct ice_flow_fld_loc {
	/* Describe offsets of field information relative to the beginning of
	 * input buffer provided when adding flow entries.
	 */
	u16 val;	/* Offset where the value is located */
	u16 mask;	/* Offset where the mask/prefix value is located */
	u16 last;	/* Length or offset where the upper value is located */
};

struct ice_flow_fld_info {
	enum ice_flow_fld_match_type type;
	/* Location where to retrieve data from an input buffer */
	struct ice_flow_fld_loc src;
	/* Location where to put the data into the final entry buffer */
	struct ice_flow_fld_loc entry;
	struct ice_flow_seg_xtrct xtrct;
};

struct ice_flow_seg_info {
	u32 hdrs;	/* Bitmask indicating protocol headers present */
	u64 match;	/* Bitmask indicating header fields to be matched */
	u64 range;	/* Bitmask indicating header fields matched as ranges */

	struct ice_flow_fld_info fields[ICE_FLOW_FIELD_IDX_MAX];
};

#define ICE_FLOW_ENTRY_HNDL(e)	((u64)e)

struct ice_flow_prof {
	struct LIST_ENTRY_TYPE l_entry;

	u64 id;
	enum ice_flow_dir dir;
	u8 segs_cnt;

	struct ice_flow_seg_info segs[ICE_FLOW_SEG_MAX];

	/* software VSI handles referenced by this flow profile */
	ice_declare_bitmap(vsis, ICE_MAX_VSI);

	union {
		/* struct sw_recipe */
		bool symm; /* Symmetric Hash for RSS */
	} cfg;
};

struct ice_rss_cfg {
	struct LIST_ENTRY_TYPE l_entry;
	/* bitmap of VSIs added to the RSS entry */
	ice_declare_bitmap(vsis, ICE_MAX_VSI);
	struct ice_rss_hash_cfg hash;
};

enum ice_flow_action_type {
	ICE_FLOW_ACT_NOP,
	ICE_FLOW_ACT_ALLOW,
	ICE_FLOW_ACT_DROP,
	ICE_FLOW_ACT_CNTR_PKT,
	ICE_FLOW_ACT_FWD_VSI,
	ICE_FLOW_ACT_FWD_VSI_LIST,	/* Should be abstracted away */
	ICE_FLOW_ACT_FWD_QUEUE,		/* Can Queues be abstracted away? */
	ICE_FLOW_ACT_FWD_QUEUE_GROUP,	/* Can Queues be abstracted away? */
	ICE_FLOW_ACT_PUSH,
	ICE_FLOW_ACT_POP,
	ICE_FLOW_ACT_MODIFY,
	ICE_FLOW_ACT_CNTR_BYTES,
	ICE_FLOW_ACT_CNTR_PKT_BYTES,
	ICE_FLOW_ACT_GENERIC_0,
	ICE_FLOW_ACT_GENERIC_1,
	ICE_FLOW_ACT_GENERIC_2,
	ICE_FLOW_ACT_GENERIC_3,
	ICE_FLOW_ACT_GENERIC_4,
	ICE_FLOW_ACT_RPT_FLOW_ID,
	ICE_FLOW_ACT_BUILD_PROF_IDX,
};

struct ice_flow_action {
	enum ice_flow_action_type type;
	union {
		u32 dummy;
	} data;
};

u64
ice_flow_find_prof(struct ice_hw *hw, enum ice_block blk, enum ice_flow_dir dir,
		   struct ice_flow_seg_info *segs, u8 segs_cnt);
enum ice_status
ice_flow_assoc_vsig_vsi(struct ice_hw *hw, enum ice_block blk, u16 vsi_handle,
			u16 vsig);
enum ice_status
ice_flow_get_hw_prof(struct ice_hw *hw, enum ice_block blk, u64 prof_id,
		     u8 *hw_prof);
void
ice_flow_set_fld_prefix(struct ice_flow_seg_info *seg, enum ice_flow_field fld,
			u16 val_loc, u16 prefix_loc, u8 prefix_sz);
void ice_rem_vsi_rss_list(struct ice_hw *hw, u16 vsi_handle);
enum ice_status ice_replay_rss_cfg(struct ice_hw *hw, u16 vsi_handle);
enum ice_status
ice_add_avf_rss_cfg(struct ice_hw *hw, u16 vsi_handle, u64 hashed_flds);
enum ice_status ice_rem_vsi_rss_cfg(struct ice_hw *hw, u16 vsi_handle);
enum ice_status
ice_add_rss_cfg(struct ice_hw *hw, u16 vsi_handle,
		const struct ice_rss_hash_cfg *cfg);
enum ice_status
ice_rem_rss_cfg(struct ice_hw *hw, u16 vsi_handle,
		const struct ice_rss_hash_cfg *cfg);
u64 ice_get_rss_cfg(struct ice_hw *hw, u16 vsi_handle, u32 hdrs);
#endif /* _ICE_FLOW_H_ */
