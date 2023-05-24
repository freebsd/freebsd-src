/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2023, Intel Corporation
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

#ifndef _ICE_LAN_TX_RX_H_
#define _ICE_LAN_TX_RX_H_
#include "ice_osdep.h"

/* Rx Descriptors */
union ice_16byte_rx_desc {
	struct {
		__le64 pkt_addr; /* Packet buffer address */
		__le64 hdr_addr; /* Header buffer address */
	} read;
	struct {
		struct {
			struct {
				__le16 mirroring_status;
				__le16 l2tag1;
			} lo_dword;
			union {
				__le32 rss; /* RSS Hash */
				__le32 fd_id; /* Flow Director filter ID */
			} hi_dword;
		} qword0;
		struct {
			/* ext status/error/PTYPE/length */
			__le64 status_error_len;
		} qword1;
	} wb;  /* writeback */
};

union ice_32byte_rx_desc {
	struct {
		__le64 pkt_addr; /* Packet buffer address */
		__le64 hdr_addr; /* Header buffer address */
			/* bit 0 of hdr_addr is DD bit */
		__le64 rsvd1;
		__le64 rsvd2;
	} read;
	struct {
		struct {
			struct {
				__le16 mirroring_status;
				__le16 l2tag1;
			} lo_dword;
			union {
				__le32 rss; /* RSS Hash */
				__le32 fd_id; /* Flow Director filter ID */
			} hi_dword;
		} qword0;
		struct {
			/* status/error/PTYPE/length */
			__le64 status_error_len;
		} qword1;
		struct {
			__le16 ext_status; /* extended status */
			__le16 rsvd;
			__le16 l2tag2_1;
			__le16 l2tag2_2;
		} qword2;
		struct {
			__le32 reserved;
			__le32 fd_id;
		} qword3;
	} wb; /* writeback */
};

struct ice_fltr_desc {
	__le64 qidx_compq_space_stat;
	__le64 dtype_cmd_vsi_fdid;
};

#define ICE_FXD_FLTR_QW0_QINDEX_S	0
#define ICE_FXD_FLTR_QW0_QINDEX_M	(0x7FFULL << ICE_FXD_FLTR_QW0_QINDEX_S)
#define ICE_FXD_FLTR_QW0_COMP_Q_S	11
#define ICE_FXD_FLTR_QW0_COMP_Q_M	BIT_ULL(ICE_FXD_FLTR_QW0_COMP_Q_S)
#define ICE_FXD_FLTR_QW0_COMP_Q_ZERO	0x0ULL
#define ICE_FXD_FLTR_QW0_COMP_Q_QINDX	0x1ULL

#define ICE_FXD_FLTR_QW0_COMP_REPORT_S	12
#define ICE_FXD_FLTR_QW0_COMP_REPORT_M	\
				(0x3ULL << ICE_FXD_FLTR_QW0_COMP_REPORT_S)
#define ICE_FXD_FLTR_QW0_COMP_REPORT_NONE	0x0ULL
#define ICE_FXD_FLTR_QW0_COMP_REPORT_SW_FAIL	0x1ULL
#define ICE_FXD_FLTR_QW0_COMP_REPORT_SW		0x2ULL

#define ICE_FXD_FLTR_QW0_FD_SPACE_S	14
#define ICE_FXD_FLTR_QW0_FD_SPACE_M	(0x3ULL << ICE_FXD_FLTR_QW0_FD_SPACE_S)
#define ICE_FXD_FLTR_QW0_FD_SPACE_GUAR			0x0ULL
#define ICE_FXD_FLTR_QW0_FD_SPACE_BEST_EFFORT		0x1ULL
#define ICE_FXD_FLTR_QW0_FD_SPACE_GUAR_BEST		0x2ULL
#define ICE_FXD_FLTR_QW0_FD_SPACE_BEST_GUAR		0x3ULL

#define ICE_FXD_FLTR_QW0_STAT_CNT_S	16
#define ICE_FXD_FLTR_QW0_STAT_CNT_M	\
				(0x1FFFULL << ICE_FXD_FLTR_QW0_STAT_CNT_S)
#define ICE_FXD_FLTR_QW0_STAT_ENA_S	29
#define ICE_FXD_FLTR_QW0_STAT_ENA_M	(0x3ULL << ICE_FXD_FLTR_QW0_STAT_ENA_S)
#define ICE_FXD_FLTR_QW0_STAT_ENA_NONE		0x0ULL
#define ICE_FXD_FLTR_QW0_STAT_ENA_PKTS		0x1ULL
#define ICE_FXD_FLTR_QW0_STAT_ENA_BYTES		0x2ULL
#define ICE_FXD_FLTR_QW0_STAT_ENA_PKTS_BYTES	0x3ULL

#define ICE_FXD_FLTR_QW0_EVICT_ENA_S	31
#define ICE_FXD_FLTR_QW0_EVICT_ENA_M	BIT_ULL(ICE_FXD_FLTR_QW0_EVICT_ENA_S)
#define ICE_FXD_FLTR_QW0_EVICT_ENA_FALSE	0x0ULL
#define ICE_FXD_FLTR_QW0_EVICT_ENA_TRUE		0x1ULL

#define ICE_FXD_FLTR_QW0_TO_Q_S		32
#define ICE_FXD_FLTR_QW0_TO_Q_M		(0x7ULL << ICE_FXD_FLTR_QW0_TO_Q_S)
#define ICE_FXD_FLTR_QW0_TO_Q_EQUALS_QINDEX	0x0ULL

#define ICE_FXD_FLTR_QW0_TO_Q_PRI_S	35
#define ICE_FXD_FLTR_QW0_TO_Q_PRI_M	(0x7ULL << ICE_FXD_FLTR_QW0_TO_Q_PRI_S)
#define ICE_FXD_FLTR_QW0_TO_Q_PRIO1	0x1ULL

#define ICE_FXD_FLTR_QW0_DPU_RECIPE_S	38
#define ICE_FXD_FLTR_QW0_DPU_RECIPE_M	\
			(0x3ULL << ICE_FXD_FLTR_QW0_DPU_RECIPE_S)
#define ICE_FXD_FLTR_QW0_DPU_RECIPE_DFLT	0x0ULL

#define ICE_FXD_FLTR_QW0_DROP_S		40
#define ICE_FXD_FLTR_QW0_DROP_M		BIT_ULL(ICE_FXD_FLTR_QW0_DROP_S)
#define ICE_FXD_FLTR_QW0_DROP_NO	0x0ULL
#define ICE_FXD_FLTR_QW0_DROP_YES	0x1ULL

#define ICE_FXD_FLTR_QW0_FLEX_PRI_S	41
#define ICE_FXD_FLTR_QW0_FLEX_PRI_M	(0x7ULL << ICE_FXD_FLTR_QW0_FLEX_PRI_S)
#define ICE_FXD_FLTR_QW0_FLEX_PRI_NONE	0x0ULL

#define ICE_FXD_FLTR_QW0_FLEX_MDID_S	44
#define ICE_FXD_FLTR_QW0_FLEX_MDID_M	(0xFULL << ICE_FXD_FLTR_QW0_FLEX_MDID_S)
#define ICE_FXD_FLTR_QW0_FLEX_MDID0	0x0ULL

#define ICE_FXD_FLTR_QW0_FLEX_VAL_S	48
#define ICE_FXD_FLTR_QW0_FLEX_VAL_M	\
				(0xFFFFULL << ICE_FXD_FLTR_QW0_FLEX_VAL_S)
#define ICE_FXD_FLTR_QW0_FLEX_VAL0	0x0ULL

#define ICE_FXD_FLTR_QW1_DTYPE_S	0
#define ICE_FXD_FLTR_QW1_DTYPE_M	(0xFULL << ICE_FXD_FLTR_QW1_DTYPE_S)
#define ICE_FXD_FLTR_QW1_PCMD_S		4
#define ICE_FXD_FLTR_QW1_PCMD_M		BIT_ULL(ICE_FXD_FLTR_QW1_PCMD_S)
#define ICE_FXD_FLTR_QW1_PCMD_ADD	0x0ULL
#define ICE_FXD_FLTR_QW1_PCMD_REMOVE	0x1ULL

#define ICE_FXD_FLTR_QW1_PROF_PRI_S	5
#define ICE_FXD_FLTR_QW1_PROF_PRI_M	(0x7ULL << ICE_FXD_FLTR_QW1_PROF_PRI_S)
#define ICE_FXD_FLTR_QW1_PROF_PRIO_ZERO	0x0ULL

#define ICE_FXD_FLTR_QW1_PROF_S		8
#define ICE_FXD_FLTR_QW1_PROF_M		(0x3FULL << ICE_FXD_FLTR_QW1_PROF_S)
#define ICE_FXD_FLTR_QW1_PROF_ZERO	0x0ULL

#define ICE_FXD_FLTR_QW1_FD_VSI_S	14
#define ICE_FXD_FLTR_QW1_FD_VSI_M	(0x3FFULL << ICE_FXD_FLTR_QW1_FD_VSI_S)
#define ICE_FXD_FLTR_QW1_SWAP_S		24
#define ICE_FXD_FLTR_QW1_SWAP_M		BIT_ULL(ICE_FXD_FLTR_QW1_SWAP_S)
#define ICE_FXD_FLTR_QW1_SWAP_NOT_SET	0x0ULL
#define ICE_FXD_FLTR_QW1_SWAP_SET	0x1ULL

#define ICE_FXD_FLTR_QW1_FDID_PRI_S	25
#define ICE_FXD_FLTR_QW1_FDID_PRI_M	(0x7ULL << ICE_FXD_FLTR_QW1_FDID_PRI_S)
#define ICE_FXD_FLTR_QW1_FDID_PRI_ZERO	0x0ULL
#define ICE_FXD_FLTR_QW1_FDID_PRI_ONE	0x1ULL
#define ICE_FXD_FLTR_QW1_FDID_PRI_THREE	0x3ULL

#define ICE_FXD_FLTR_QW1_FDID_MDID_S	28
#define ICE_FXD_FLTR_QW1_FDID_MDID_M	(0xFULL << ICE_FXD_FLTR_QW1_FDID_MDID_S)
#define ICE_FXD_FLTR_QW1_FDID_MDID_FD	0x05ULL

#define ICE_FXD_FLTR_QW1_FDID_S		32
#define ICE_FXD_FLTR_QW1_FDID_M		\
			(0xFFFFFFFFULL << ICE_FXD_FLTR_QW1_FDID_S)
#define ICE_FXD_FLTR_QW1_FDID_ZERO	0x0ULL

enum ice_rx_desc_status_bits {
	/* Note: These are predefined bit offsets */
	ICE_RX_DESC_STATUS_DD_S			= 0,
	ICE_RX_DESC_STATUS_EOF_S		= 1,
	ICE_RX_DESC_STATUS_L2TAG1P_S		= 2,
	ICE_RX_DESC_STATUS_L3L4P_S		= 3,
	ICE_RX_DESC_STATUS_CRCP_S		= 4,
	ICE_RX_DESC_STATUS_TSYNINDX_S		= 5,
	ICE_RX_DESC_STATUS_TSYNVALID_S		= 7,
	ICE_RX_DESC_STATUS_EXT_UDP_0_S		= 8,
	ICE_RX_DESC_STATUS_UMBCAST_S		= 9,
	ICE_RX_DESC_STATUS_FLM_S		= 11,
	ICE_RX_DESC_STATUS_FLTSTAT_S		= 12,
	ICE_RX_DESC_STATUS_LPBK_S		= 14,
	ICE_RX_DESC_STATUS_IPV6EXADD_S		= 15,
	ICE_RX_DESC_STATUS_RESERVED2_S		= 16,
	ICE_RX_DESC_STATUS_INT_UDP_0_S		= 18,
	ICE_RX_DESC_STATUS_LAST /* this entry must be last!!! */
};

#define ICE_RXD_QW1_STATUS_S	0
#define ICE_RXD_QW1_STATUS_M	((BIT(ICE_RX_DESC_STATUS_LAST) - 1) << \
				 ICE_RXD_QW1_STATUS_S)

#define ICE_RXD_QW1_STATUS_TSYNINDX_S ICE_RX_DESC_STATUS_TSYNINDX_S
#define ICE_RXD_QW1_STATUS_TSYNINDX_M (0x3UL << ICE_RXD_QW1_STATUS_TSYNINDX_S)

#define ICE_RXD_QW1_STATUS_TSYNVALID_S ICE_RX_DESC_STATUS_TSYNVALID_S
#define ICE_RXD_QW1_STATUS_TSYNVALID_M BIT_ULL(ICE_RXD_QW1_STATUS_TSYNVALID_S)

enum ice_rx_desc_fltstat_values {
	ICE_RX_DESC_FLTSTAT_NO_DATA	= 0,
	ICE_RX_DESC_FLTSTAT_RSV_FD_ID	= 1, /* 16byte desc? FD_ID : RSV */
	ICE_RX_DESC_FLTSTAT_RSV		= 2,
	ICE_RX_DESC_FLTSTAT_RSS_HASH	= 3,
};

#define ICE_RXD_QW1_ERROR_S	19
#define ICE_RXD_QW1_ERROR_M		(0xFFUL << ICE_RXD_QW1_ERROR_S)

enum ice_rx_desc_error_bits {
	/* Note: These are predefined bit offsets */
	ICE_RX_DESC_ERROR_RXE_S			= 0,
	ICE_RX_DESC_ERROR_RECIPE_S		= 1,
	ICE_RX_DESC_ERROR_HBO_S			= 2,
	ICE_RX_DESC_ERROR_L3L4E_S		= 3, /* 3 BITS */
	ICE_RX_DESC_ERROR_IPE_S			= 3,
	ICE_RX_DESC_ERROR_L4E_S			= 4,
	ICE_RX_DESC_ERROR_EIPE_S		= 5,
	ICE_RX_DESC_ERROR_OVERSIZE_S		= 6,
	ICE_RX_DESC_ERROR_PPRS_S		= 7
};

enum ice_rx_desc_error_l3l4e_masks {
	ICE_RX_DESC_ERROR_L3L4E_NONE		= 0,
	ICE_RX_DESC_ERROR_L3L4E_PROT		= 1,
};

#define ICE_RXD_QW1_PTYPE_S	30
#define ICE_RXD_QW1_PTYPE_M	(0xFFULL << ICE_RXD_QW1_PTYPE_S)

/* Packet type non-ip values */
enum ice_rx_l2_ptype {
	ICE_RX_PTYPE_L2_RESERVED	= 0,
	ICE_RX_PTYPE_L2_MAC_PAY2	= 1,
	ICE_RX_PTYPE_L2_FIP_PAY2	= 3,
	ICE_RX_PTYPE_L2_OUI_PAY2	= 4,
	ICE_RX_PTYPE_L2_MACCNTRL_PAY2	= 5,
	ICE_RX_PTYPE_L2_LLDP_PAY2	= 6,
	ICE_RX_PTYPE_L2_ECP_PAY2	= 7,
	ICE_RX_PTYPE_L2_EVB_PAY2	= 8,
	ICE_RX_PTYPE_L2_QCN_PAY2	= 9,
	ICE_RX_PTYPE_L2_EAPOL_PAY2	= 10,
	ICE_RX_PTYPE_L2_ARP		= 11,
};

struct ice_rx_ptype_decoded {
	u32 known:1;
	u32 outer_ip:1;
	u32 outer_ip_ver:2;
	u32 outer_frag:1;
	u32 tunnel_type:3;
	u32 tunnel_end_prot:2;
	u32 tunnel_end_frag:1;
	u32 inner_prot:4;
	u32 payload_layer:3;
};

enum ice_rx_ptype_outer_ip {
	ICE_RX_PTYPE_OUTER_L2	= 0,
	ICE_RX_PTYPE_OUTER_IP	= 1,
};

enum ice_rx_ptype_outer_ip_ver {
	ICE_RX_PTYPE_OUTER_NONE	= 0,
	ICE_RX_PTYPE_OUTER_IPV4	= 1,
	ICE_RX_PTYPE_OUTER_IPV6	= 2,
};

enum ice_rx_ptype_outer_fragmented {
	ICE_RX_PTYPE_NOT_FRAG	= 0,
	ICE_RX_PTYPE_FRAG	= 1,
};

enum ice_rx_ptype_tunnel_type {
	ICE_RX_PTYPE_TUNNEL_NONE		= 0,
	ICE_RX_PTYPE_TUNNEL_IP_IP		= 1,
	ICE_RX_PTYPE_TUNNEL_IP_GRENAT		= 2,
	ICE_RX_PTYPE_TUNNEL_IP_GRENAT_MAC	= 3,
	ICE_RX_PTYPE_TUNNEL_IP_GRENAT_MAC_VLAN	= 4,
};

enum ice_rx_ptype_tunnel_end_prot {
	ICE_RX_PTYPE_TUNNEL_END_NONE	= 0,
	ICE_RX_PTYPE_TUNNEL_END_IPV4	= 1,
	ICE_RX_PTYPE_TUNNEL_END_IPV6	= 2,
};

enum ice_rx_ptype_inner_prot {
	ICE_RX_PTYPE_INNER_PROT_NONE		= 0,
	ICE_RX_PTYPE_INNER_PROT_UDP		= 1,
	ICE_RX_PTYPE_INNER_PROT_TCP		= 2,
	ICE_RX_PTYPE_INNER_PROT_SCTP		= 3,
	ICE_RX_PTYPE_INNER_PROT_ICMP		= 4,
};

enum ice_rx_ptype_payload_layer {
	ICE_RX_PTYPE_PAYLOAD_LAYER_NONE	= 0,
	ICE_RX_PTYPE_PAYLOAD_LAYER_PAY2	= 1,
	ICE_RX_PTYPE_PAYLOAD_LAYER_PAY3	= 2,
	ICE_RX_PTYPE_PAYLOAD_LAYER_PAY4	= 3,
};

#define ICE_RXD_QW1_LEN_PBUF_S	38
#define ICE_RXD_QW1_LEN_PBUF_M	(0x3FFFULL << ICE_RXD_QW1_LEN_PBUF_S)

#define ICE_RXD_QW1_LEN_HBUF_S	52
#define ICE_RXD_QW1_LEN_HBUF_M	(0x7FFULL << ICE_RXD_QW1_LEN_HBUF_S)

#define ICE_RXD_QW1_LEN_SPH_S	63
#define ICE_RXD_QW1_LEN_SPH_M	BIT_ULL(ICE_RXD_QW1_LEN_SPH_S)

enum ice_rx_desc_ext_status_bits {
	/* Note: These are predefined bit offsets */
	ICE_RX_DESC_EXT_STATUS_L2TAG2P_S	= 0,
	ICE_RX_DESC_EXT_STATUS_L2TAG3P_S	= 1,
	ICE_RX_DESC_EXT_STATUS_FLEXBL_S		= 2,
	ICE_RX_DESC_EXT_STATUS_FLEXBH_S		= 4,
	ICE_RX_DESC_EXT_STATUS_FDLONGB_S	= 9,
	ICE_RX_DESC_EXT_STATUS_PELONGB_S	= 11,
};

enum ice_rx_desc_pe_status_bits {
	/* Note: These are predefined bit offsets */
	ICE_RX_DESC_PE_STATUS_QPID_S		= 0, /* 18 BITS */
	ICE_RX_DESC_PE_STATUS_L4PORT_S		= 0, /* 16 BITS */
	ICE_RX_DESC_PE_STATUS_IPINDEX_S		= 16, /* 8 BITS */
	ICE_RX_DESC_PE_STATUS_QPIDHIT_S		= 24,
	ICE_RX_DESC_PE_STATUS_APBVTHIT_S	= 25,
	ICE_RX_DESC_PE_STATUS_PORTV_S		= 26,
	ICE_RX_DESC_PE_STATUS_URG_S		= 27,
	ICE_RX_DESC_PE_STATUS_IPFRAG_S		= 28,
	ICE_RX_DESC_PE_STATUS_IPOPT_S		= 29
};

#define ICE_RX_PROG_STATUS_DESC_LEN_S	38
#define ICE_RX_PROG_STATUS_DESC_LEN	0x2000000

#define ICE_RX_PROG_STATUS_DESC_QW1_PROGID_S	2
#define ICE_RX_PROG_STATUS_DESC_QW1_PROGID_M	\
			(0x7UL << ICE_RX_PROG_STATUS_DESC_QW1_PROGID_S)

#define ICE_RX_PROG_STATUS_DESC_QW1_ERROR_S	19
#define ICE_RX_PROG_STATUS_DESC_QW1_ERROR_M	\
			(0x3FUL << ICE_RX_PROG_STATUS_DESC_QW1_ERROR_S)

enum ice_rx_prog_status_desc_status_bits {
	/* Note: These are predefined bit offsets */
	ICE_RX_PROG_STATUS_DESC_DD_S		= 0,
	ICE_RX_PROG_STATUS_DESC_PROG_ID_S	= 2 /* 3 BITS */
};

enum ice_rx_prog_status_desc_prog_id_masks {
	ICE_RX_PROG_STATUS_DESC_FD_FLTR_STATUS	= 1,
};

enum ice_rx_prog_status_desc_error_bits {
	/* Note: These are predefined bit offsets */
	ICE_RX_PROG_STATUS_DESC_FD_TBL_FULL_S	= 0,
	ICE_RX_PROG_STATUS_DESC_NO_FD_ENTRY_S	= 1,
};

/* Rx Flex Descriptors
 * These descriptors are used instead of the legacy version descriptors when
 * ice_rlan_ctx.adv_desc is set
 */

union ice_32b_rx_flex_desc {
	struct {
		__le64 pkt_addr; /* Packet buffer address */
		__le64 hdr_addr; /* Header buffer address */
				 /* bit 0 of hdr_addr is DD bit */
		__le64 rsvd1;
		__le64 rsvd2;
	} read;
	struct {
		/* Qword 0 */
		u8 rxdid; /* descriptor builder profile ID */
		u8 mir_id_umb_cast; /* mirror=[5:0], umb=[7:6] */
		__le16 ptype_flex_flags0; /* ptype=[9:0], ff0=[15:10] */
		__le16 pkt_len; /* [15:14] are reserved */
		__le16 hdr_len_sph_flex_flags1; /* header=[10:0] */
						/* sph=[11:11] */
						/* ff1/ext=[15:12] */

		/* Qword 1 */
		__le16 status_error0;
		__le16 l2tag1;
		__le16 flex_meta0;
		__le16 flex_meta1;

		/* Qword 2 */
		__le16 status_error1;
		u8 flex_flags2;
		u8 time_stamp_low;
		__le16 l2tag2_1st;
		__le16 l2tag2_2nd;

		/* Qword 3 */
		__le16 flex_meta2;
		__le16 flex_meta3;
		union {
			struct {
				__le16 flex_meta4;
				__le16 flex_meta5;
			} flex;
			__le32 ts_high;
		} flex_ts;
	} wb; /* writeback */
};

/* Rx Flex Descriptor NIC Profile
 * RxDID Profile ID 2
 * Flex-field 0: RSS hash lower 16-bits
 * Flex-field 1: RSS hash upper 16-bits
 * Flex-field 2: Flow ID lower 16-bits
 * Flex-field 3: Flow ID higher 16-bits
 * Flex-field 4: reserved, VLAN ID taken from L2Tag
 */
struct ice_32b_rx_flex_desc_nic {
	/* Qword 0 */
	u8 rxdid;
	u8 mir_id_umb_cast;
	__le16 ptype_flexi_flags0;
	__le16 pkt_len;
	__le16 hdr_len_sph_flex_flags1;

	/* Qword 1 */
	__le16 status_error0;
	__le16 l2tag1;
	__le32 rss_hash;

	/* Qword 2 */
	__le16 status_error1;
	u8 flexi_flags2;
	u8 ts_low;
	__le16 l2tag2_1st;
	__le16 l2tag2_2nd;

	/* Qword 3 */
	__le32 flow_id;
	union {
		struct {
			__le16 rsvd;
			__le16 flow_id_ipv6;
		} flex;
		__le32 ts_high;
	} flex_ts;
};

/* Rx Flex Descriptor Switch Profile
 * RxDID Profile ID 3
 * Flex-field 0: Source VSI
 */
struct ice_32b_rx_flex_desc_sw {
	/* Qword 0 */
	u8 rxdid;
	u8 mir_id_umb_cast;
	__le16 ptype_flexi_flags0;
	__le16 pkt_len;
	__le16 hdr_len_sph_flex_flags1;

	/* Qword 1 */
	__le16 status_error0;
	__le16 l2tag1;
	__le16 src_vsi; /* [10:15] are reserved */
	__le16 flex_md1_rsvd;

	/* Qword 2 */
	__le16 status_error1;
	u8 flex_flags2;
	u8 ts_low;
	__le16 l2tag2_1st;
	__le16 l2tag2_2nd;

	/* Qword 3 */
	__le32 rsvd; /* flex words 2-3 are reserved */
	__le32 ts_high;
};

/* Rx Flex Descriptor NIC VEB Profile
 * RxDID Profile ID 4
 * Flex-field 0: Destination VSI
 */
struct ice_32b_rx_flex_desc_nic_veb_dbg {
	/* Qword 0 */
	u8 rxdid;
	u8 mir_id_umb_cast;
	__le16 ptype_flexi_flags0;
	__le16 pkt_len;
	__le16 hdr_len_sph_flex_flags1;

	/* Qword 1 */
	__le16 status_error0;
	__le16 l2tag1;
	__le16 dst_vsi; /* [0:12]: destination VSI */
			/* 13: VSI valid bit */
			/* [14:15] are reserved */
	__le16 flex_field_1;

	/* Qword 2 */
	__le16 status_error1;
	u8 flex_flags2;
	u8 ts_low;
	__le16 l2tag2_1st;
	__le16 l2tag2_2nd;

	/* Qword 3 */
	__le32 rsvd; /* flex words 2-3 are reserved */
	__le32 ts_high;
};

/* Rx Flex Descriptor NIC ACL Profile
 * RxDID Profile ID 5
 * Flex-field 0: ACL Counter 0
 * Flex-field 1: ACL Counter 1
 * Flex-field 2: ACL Counter 2
 */
struct ice_32b_rx_flex_desc_nic_acl_dbg {
	/* Qword 0 */
	u8 rxdid;
	u8 mir_id_umb_cast;
	__le16 ptype_flexi_flags0;
	__le16 pkt_len;
	__le16 hdr_len_sph_flex_flags1;

	/* Qword 1 */
	__le16 status_error0;
	__le16 l2tag1;
	__le16 acl_ctr0;
	__le16 acl_ctr1;

	/* Qword 2 */
	__le16 status_error1;
	u8 flex_flags2;
	u8 ts_low;
	__le16 l2tag2_1st;
	__le16 l2tag2_2nd;

	/* Qword 3 */
	__le16 acl_ctr2;
	__le16 rsvd; /* flex words 2-3 are reserved */
	__le32 ts_high;
};

/* Rx Flex Descriptor NIC Profile
 * RxDID Profile ID 6
 * Flex-field 0: RSS hash lower 16-bits
 * Flex-field 1: RSS hash upper 16-bits
 * Flex-field 2: Flow ID lower 16-bits
 * Flex-field 3: Source VSI
 * Flex-field 4: reserved, VLAN ID taken from L2Tag
 */
struct ice_32b_rx_flex_desc_nic_2 {
	/* Qword 0 */
	u8 rxdid;
	u8 mir_id_umb_cast;
	__le16 ptype_flexi_flags0;
	__le16 pkt_len;
	__le16 hdr_len_sph_flex_flags1;

	/* Qword 1 */
	__le16 status_error0;
	__le16 l2tag1;
	__le32 rss_hash;

	/* Qword 2 */
	__le16 status_error1;
	u8 flexi_flags2;
	u8 ts_low;
	__le16 l2tag2_1st;
	__le16 l2tag2_2nd;

	/* Qword 3 */
	__le16 flow_id;
	__le16 src_vsi;
	union {
		struct {
			__le16 rsvd;
			__le16 flow_id_ipv6;
		} flex;
		__le32 ts_high;
	} flex_ts;
};

/* Receive Flex Descriptor profile IDs: There are a total
 * of 64 profiles where profile IDs 0/1 are for legacy; and
 * profiles 2-63 are flex profiles that can be programmed
 * with a specific metadata (profile 7 reserved for HW)
 */
enum ice_rxdid {
	ICE_RXDID_LEGACY_0		= 0,
	ICE_RXDID_LEGACY_1		= 1,
	ICE_RXDID_FLEX_NIC		= 2,
	ICE_RXDID_FLEX_NIC_2		= 6,
	ICE_RXDID_HW			= 7,
	ICE_RXDID_LAST			= 63,
};

/* Recceive Flex descriptor Dword Index */
enum ice_flex_word {
	ICE_RX_FLEX_DWORD_0 = 0,
	ICE_RX_FLEX_DWORD_1,
	ICE_RX_FLEX_DWORD_2,
	ICE_RX_FLEX_DWORD_3,
	ICE_RX_FLEX_DWORD_4,
	ICE_RX_FLEX_DWORD_5
};

/* Receive Flex Descriptor Rx opcode values */
enum ice_flex_opcode {
	ICE_RX_OPC_DEBUG = 0,
	ICE_RX_OPC_MDID,
	ICE_RX_OPC_EXTRACT,
	ICE_RX_OPC_PROTID
};

/* Receive Descriptor MDID values that access packet flags */
enum ice_flex_mdid_pkt_flags {
	ICE_RX_MDID_PKT_FLAGS_15_0	= 20,
	ICE_RX_MDID_PKT_FLAGS_31_16,
	ICE_RX_MDID_PKT_FLAGS_47_32,
	ICE_RX_MDID_PKT_FLAGS_63_48,
};

/* Generic descriptor MDID values */
enum ice_flex_mdid {
	ICE_MDID_GENERIC_WORD_0,
	ICE_MDID_GENERIC_WORD_1,
	ICE_MDID_GENERIC_WORD_2,
	ICE_MDID_GENERIC_WORD_3,
	ICE_MDID_GENERIC_WORD_4,
	ICE_MDID_FLOW_ID_LOWER,
	ICE_MDID_FLOW_ID_HIGH,
	ICE_MDID_RX_DESCR_PROF_IDX,
	ICE_MDID_RX_PKT_DROP,
	ICE_MDID_RX_DST_Q		= 12,
	ICE_MDID_RX_DST_VSI,
	ICE_MDID_SRC_VSI		= 19,
	ICE_MDID_ACL_NOP		= 55,
	/* Entry 56 */
	ICE_MDID_RX_HASH_LOW,
	ICE_MDID_ACL_CNTR_PKT		= ICE_MDID_RX_HASH_LOW,
	/* Entry 57 */
	ICE_MDID_RX_HASH_HIGH,
	ICE_MDID_ACL_CNTR_BYTES		= ICE_MDID_RX_HASH_HIGH,
	ICE_MDID_ACL_CNTR_PKT_BYTES
};

/* for ice_32byte_rx_flex_desc.mir_id_umb_cast member */
#define ICE_RX_FLEX_DESC_MIRROR_M	(0x3F) /* 6-bits */

/* Rx/Tx Flag64 packet flag bits */
enum ice_flg64_bits {
	ICE_FLG_PKT_DSI		= 0,
	/* If there is a 1 in this bit position then that means Rx packet */
	ICE_FLG_PKT_DIR		= 4,
	ICE_FLG_EVLAN_x8100	= 14,
	ICE_FLG_EVLAN_x9100,
	ICE_FLG_VLAN_x8100,
	ICE_FLG_TNL_MAC		= 22,
	ICE_FLG_TNL_VLAN,
	ICE_FLG_PKT_FRG,
	ICE_FLG_FIN		= 32,
	ICE_FLG_SYN,
	ICE_FLG_RST,
	ICE_FLG_TNL0		= 38,
	ICE_FLG_TNL1,
	ICE_FLG_TNL2,
	ICE_FLG_UDP_GRE,
	ICE_FLG_RSVD		= 63
};

enum ice_rx_flex_desc_umb_cast_bits { /* field is 2 bits long */
	ICE_RX_FLEX_DESC_UMB_CAST_S = 6,
	ICE_RX_FLEX_DESC_UMB_CAST_LAST /* this entry must be last!!! */
};

enum ice_umbcast_dest_addr_types {
	ICE_DEST_UNICAST = 0,
	ICE_DEST_MULTICAST,
	ICE_DEST_BROADCAST,
	ICE_DEST_MIRRORED,
};

/* for ice_32byte_rx_flex_desc.ptype_flexi_flags0 member */
#define ICE_RX_FLEX_DESC_PTYPE_M	(0x3FF) /* 10-bits */

enum ice_rx_flex_desc_flexi_flags0_bits { /* field is 6 bits long */
	ICE_RX_FLEX_DESC_FLEXI_FLAGS0_S = 10,
	ICE_RX_FLEX_DESC_FLEXI_FLAGS0_LAST /* this entry must be last!!! */
};

/* for ice_32byte_rx_flex_desc.pkt_length member */
#define ICE_RX_FLX_DESC_PKT_LEN_M	(0x3FFF) /* 14-bits */

/* for ice_32byte_rx_flex_desc.header_length_sph_flexi_flags1 member */
#define ICE_RX_FLEX_DESC_HEADER_LEN_M	(0x7FF) /* 11-bits */

enum ice_rx_flex_desc_sph_bits { /* field is 1 bit long */
	ICE_RX_FLEX_DESC_SPH_S = 11,
	ICE_RX_FLEX_DESC_SPH_LAST /* this entry must be last!!! */
};

enum ice_rx_flex_desc_flexi_flags1_bits { /* field is 4 bits long */
	ICE_RX_FLEX_DESC_FLEXI_FLAGS1_S = 12,
	ICE_RX_FLEX_DESC_FLEXI_FLAGS1_LAST /* this entry must be last!!! */
};

enum ice_rx_flex_desc_ext_status_bits { /* field is 4 bits long */
	ICE_RX_FLEX_DESC_EXT_STATUS_EXT_UDP_S = 12,
	ICE_RX_FLEX_DESC_EXT_STATUS_INT_UDP_S = 13,
	ICE_RX_FLEX_DESC_EXT_STATUS_RECIPE_S = 14,
	ICE_RX_FLEX_DESC_EXT_STATUS_OVERSIZE_S = 15,
	ICE_RX_FLEX_DESC_EXT_STATUS_LAST /* entry must be last!!! */
};

enum ice_rx_flex_desc_status_error_0_bits {
	/* Note: These are predefined bit offsets */
	ICE_RX_FLEX_DESC_STATUS0_DD_S = 0,
	ICE_RX_FLEX_DESC_STATUS0_EOF_S,
	ICE_RX_FLEX_DESC_STATUS0_HBO_S,
	ICE_RX_FLEX_DESC_STATUS0_L3L4P_S,
	ICE_RX_FLEX_DESC_STATUS0_XSUM_IPE_S,
	ICE_RX_FLEX_DESC_STATUS0_XSUM_L4E_S,
	ICE_RX_FLEX_DESC_STATUS0_XSUM_EIPE_S,
	ICE_RX_FLEX_DESC_STATUS0_XSUM_EUDPE_S,
	ICE_RX_FLEX_DESC_STATUS0_LPBK_S,
	ICE_RX_FLEX_DESC_STATUS0_IPV6EXADD_S,
	ICE_RX_FLEX_DESC_STATUS0_RXE_S,
	ICE_RX_FLEX_DESC_STATUS0_CRCP_S,
	ICE_RX_FLEX_DESC_STATUS0_RSS_VALID_S,
	ICE_RX_FLEX_DESC_STATUS0_L2TAG1P_S,
	ICE_RX_FLEX_DESC_STATUS0_XTRMD0_VALID_S,
	ICE_RX_FLEX_DESC_STATUS0_XTRMD1_VALID_S,
	ICE_RX_FLEX_DESC_STATUS0_LAST /* this entry must be last!!! */
};

enum ice_rx_flex_desc_status_error_1_bits {
	/* Note: These are predefined bit offsets */
	ICE_RX_FLEX_DESC_STATUS1_CPM_S = 0, /* 4 bits */
	ICE_RX_FLEX_DESC_STATUS1_NAT_S = 4,
	ICE_RX_FLEX_DESC_STATUS1_CRYPTO_S = 5,
	/* [10:6] reserved */
	ICE_RX_FLEX_DESC_STATUS1_L2TAG2P_S = 11,
	ICE_RX_FLEX_DESC_STATUS1_XTRMD2_VALID_S = 12,
	ICE_RX_FLEX_DESC_STATUS1_XTRMD3_VALID_S = 13,
	ICE_RX_FLEX_DESC_STATUS1_XTRMD4_VALID_S = 14,
	ICE_RX_FLEX_DESC_STATUS1_XTRMD5_VALID_S = 15,
	ICE_RX_FLEX_DESC_STATUS1_LAST /* this entry must be last!!! */
};

enum ice_rx_flex_desc_exstat_bits {
	/* Note: These are predefined bit offsets */
	ICE_RX_FLEX_DESC_EXSTAT_EXTUDP_S = 0,
	ICE_RX_FLEX_DESC_EXSTAT_INTUDP_S = 1,
	ICE_RX_FLEX_DESC_EXSTAT_RECIPE_S = 2,
	ICE_RX_FLEX_DESC_EXSTAT_OVERSIZE_S = 3,
};

/*
 * For ice_32b_rx_flex_desc.ts_low:
 * [0]: Timestamp-low validity bit
 * [1:7]: Timestamp-low value
 */
#define ICE_RX_FLEX_DESC_TS_L_VALID_S	0x01
#define ICE_RX_FLEX_DESC_TS_L_VALID_M	ICE_RX_FLEX_DESC_TS_L_VALID_S
#define ICE_RX_FLEX_DESC_TS_L_M		0xFE

#define ICE_RXQ_CTX_SIZE_DWORDS		8
#define ICE_RXQ_CTX_SZ			(ICE_RXQ_CTX_SIZE_DWORDS * sizeof(u32))
#define ICE_TXQ_CTX_SIZE_DWORDS		10
#define ICE_TXQ_CTX_SZ			(ICE_TXQ_CTX_SIZE_DWORDS * sizeof(u32))
#define ICE_TX_CMPLTNQ_CTX_SIZE_DWORDS	22
#define ICE_TX_DRBELL_Q_CTX_SIZE_DWORDS	5
#define GLTCLAN_CQ_CNTX(i, CQ)		(GLTCLAN_CQ_CNTX0(CQ) + ((i) * 0x0800))

/* RLAN Rx queue context data
 *
 * The sizes of the variables may be larger than needed due to crossing byte
 * boundaries. If we do not have the width of the variable set to the correct
 * size then we could end up shifting bits off the top of the variable when the
 * variable is at the top of a byte and crosses over into the next byte.
 */
struct ice_rlan_ctx {
	u16 head;
	u16 cpuid; /* bigger than needed, see above for reason */
#define ICE_RLAN_BASE_S 7
	u64 base;
	u16 qlen;
#define ICE_RLAN_CTX_DBUF_S 7
	u16 dbuf; /* bigger than needed, see above for reason */
#define ICE_RLAN_CTX_HBUF_S 6
	u16 hbuf; /* bigger than needed, see above for reason */
	u8 dtype;
	u8 dsize;
	u8 crcstrip;
	u8 l2tsel;
	u8 hsplit_0;
	u8 hsplit_1;
	u8 showiv;
	u32 rxmax; /* bigger than needed, see above for reason */
	u8 tphrdesc_ena;
	u8 tphwdesc_ena;
	u8 tphdata_ena;
	u8 tphhead_ena;
	u16 lrxqthresh; /* bigger than needed, see above for reason */
	u8 prefena;	/* NOTE: normally must be set to 1 at init */
};

struct ice_ctx_ele {
	u16 offset;
	u16 size_of;
	u16 width;
	u16 lsb;
};

#define ICE_CTX_STORE(_struct, _ele, _width, _lsb) {	\
	.offset = offsetof(struct _struct, _ele),	\
	.size_of = FIELD_SIZEOF(struct _struct, _ele),	\
	.width = _width,				\
	.lsb = _lsb,					\
}

/* for hsplit_0 field of Rx RLAN context */
enum ice_rlan_ctx_rx_hsplit_0 {
	ICE_RLAN_RX_HSPLIT_0_NO_SPLIT		= 0,
	ICE_RLAN_RX_HSPLIT_0_SPLIT_L2		= 1,
	ICE_RLAN_RX_HSPLIT_0_SPLIT_IP		= 2,
	ICE_RLAN_RX_HSPLIT_0_SPLIT_TCP_UDP	= 4,
	ICE_RLAN_RX_HSPLIT_0_SPLIT_SCTP		= 8,
};

/* for hsplit_1 field of Rx RLAN context */
enum ice_rlan_ctx_rx_hsplit_1 {
	ICE_RLAN_RX_HSPLIT_1_NO_SPLIT		= 0,
	ICE_RLAN_RX_HSPLIT_1_SPLIT_L2		= 1,
	ICE_RLAN_RX_HSPLIT_1_SPLIT_ALWAYS	= 2,
};

/* Tx Descriptor */
struct ice_tx_desc {
	__le64 buf_addr; /* Address of descriptor's data buf */
	__le64 cmd_type_offset_bsz;
};

#define ICE_TXD_QW1_DTYPE_S	0
#define ICE_TXD_QW1_DTYPE_M	(0xFUL << ICE_TXD_QW1_DTYPE_S)

enum ice_tx_desc_dtype_value {
	ICE_TX_DESC_DTYPE_DATA		= 0x0,
	ICE_TX_DESC_DTYPE_CTX		= 0x1,
	ICE_TX_DESC_DTYPE_IPSEC		= 0x3,
	ICE_TX_DESC_DTYPE_FLTR_PROG	= 0x8,
	ICE_TX_DESC_DTYPE_HLP_META	= 0x9,
	/* DESC_DONE - HW has completed write-back of descriptor */
	ICE_TX_DESC_DTYPE_DESC_DONE	= 0xF,
};

#define ICE_TXD_QW1_CMD_S	4
#define ICE_TXD_QW1_CMD_M	(0xFFFUL << ICE_TXD_QW1_CMD_S)

enum ice_tx_desc_cmd_bits {
	ICE_TX_DESC_CMD_EOP			= 0x0001,
	ICE_TX_DESC_CMD_RS			= 0x0002,
	ICE_TX_DESC_CMD_RSVD			= 0x0004,
	ICE_TX_DESC_CMD_IL2TAG1			= 0x0008,
	ICE_TX_DESC_CMD_DUMMY			= 0x0010,
	ICE_TX_DESC_CMD_IIPT_NONIP		= 0x0000,
	ICE_TX_DESC_CMD_IIPT_IPV6		= 0x0020,
	ICE_TX_DESC_CMD_IIPT_IPV4		= 0x0040,
	ICE_TX_DESC_CMD_IIPT_IPV4_CSUM		= 0x0060,
	ICE_TX_DESC_CMD_RSVD2			= 0x0080,
	ICE_TX_DESC_CMD_L4T_EOFT_UNK		= 0x0000,
	ICE_TX_DESC_CMD_L4T_EOFT_TCP		= 0x0100,
	ICE_TX_DESC_CMD_L4T_EOFT_SCTP		= 0x0200,
	ICE_TX_DESC_CMD_L4T_EOFT_UDP		= 0x0300,
	ICE_TX_DESC_CMD_RE			= 0x0400,
	ICE_TX_DESC_CMD_RSVD3			= 0x0800,
};

#define ICE_TXD_QW1_OFFSET_S	16
#define ICE_TXD_QW1_OFFSET_M	(0x3FFFFULL << ICE_TXD_QW1_OFFSET_S)

enum ice_tx_desc_len_fields {
	/* Note: These are predefined bit offsets */
	ICE_TX_DESC_LEN_MACLEN_S	= 0, /* 7 BITS */
	ICE_TX_DESC_LEN_IPLEN_S	= 7, /* 7 BITS */
	ICE_TX_DESC_LEN_L4_LEN_S	= 14 /* 4 BITS */
};

#define ICE_TXD_QW1_MACLEN_M (0x7FUL << ICE_TX_DESC_LEN_MACLEN_S)
#define ICE_TXD_QW1_IPLEN_M  (0x7FUL << ICE_TX_DESC_LEN_IPLEN_S)
#define ICE_TXD_QW1_L4LEN_M  (0xFUL << ICE_TX_DESC_LEN_L4_LEN_S)

/* Tx descriptor field limits in bytes */
#define ICE_TXD_MACLEN_MAX ((ICE_TXD_QW1_MACLEN_M >> \
			     ICE_TX_DESC_LEN_MACLEN_S) * ICE_BYTES_PER_WORD)
#define ICE_TXD_IPLEN_MAX ((ICE_TXD_QW1_IPLEN_M >> \
			    ICE_TX_DESC_LEN_IPLEN_S) * ICE_BYTES_PER_DWORD)
#define ICE_TXD_L4LEN_MAX ((ICE_TXD_QW1_L4LEN_M >> \
			    ICE_TX_DESC_LEN_L4_LEN_S) * ICE_BYTES_PER_DWORD)

#define ICE_TXD_QW1_TX_BUF_SZ_S	34
#define ICE_TXD_QW1_TX_BUF_SZ_M	(0x3FFFULL << ICE_TXD_QW1_TX_BUF_SZ_S)

#define ICE_TXD_QW1_L2TAG1_S	48
#define ICE_TXD_QW1_L2TAG1_M	(0xFFFFULL << ICE_TXD_QW1_L2TAG1_S)

/* Context descriptors */
struct ice_tx_ctx_desc {
	__le32 tunneling_params;
	__le16 l2tag2;
	__le16 rsvd;
	__le64 qw1;
};

#define ICE_TX_GCS_DESC_START	0  /* 7 BITS */
#define ICE_TX_GCS_DESC_OFFSET	7  /* 4 BITS */
#define ICE_TX_GCS_DESC_TYPE	11 /* 2 BITS */
#define ICE_TX_GCS_DESC_ENA	13 /* 1 BIT */

#define ICE_TXD_CTX_QW1_DTYPE_S	0
#define ICE_TXD_CTX_QW1_DTYPE_M	(0xFUL << ICE_TXD_CTX_QW1_DTYPE_S)

#define ICE_TXD_CTX_QW1_CMD_S	4
#define ICE_TXD_CTX_QW1_CMD_M	(0x7FUL << ICE_TXD_CTX_QW1_CMD_S)

#define ICE_TXD_CTX_QW1_IPSEC_S	11
#define ICE_TXD_CTX_QW1_IPSEC_M	(0x7FUL << ICE_TXD_CTX_QW1_IPSEC_S)

#define ICE_TXD_CTX_QW1_TSO_LEN_S	30
#define ICE_TXD_CTX_QW1_TSO_LEN_M	\
			(0x3FFFFULL << ICE_TXD_CTX_QW1_TSO_LEN_S)

#define ICE_TXD_CTX_QW1_TSYN_S	ICE_TXD_CTX_QW1_TSO_LEN_S
#define ICE_TXD_CTX_QW1_TSYN_M	ICE_TXD_CTX_QW1_TSO_LEN_M

#define ICE_TXD_CTX_QW1_MSS_S	50
#define ICE_TXD_CTX_QW1_MSS_M	(0x3FFFULL << ICE_TXD_CTX_QW1_MSS_S)
#define ICE_TXD_CTX_MIN_MSS	64
#define ICE_TXD_CTX_MAX_MSS	9668

#define ICE_TXD_CTX_QW1_VSI_S	50
#define ICE_TXD_CTX_QW1_VSI_M	(0x3FFULL << ICE_TXD_CTX_QW1_VSI_S)

enum ice_tx_ctx_desc_cmd_bits {
	ICE_TX_CTX_DESC_TSO		= 0x01,
	ICE_TX_CTX_DESC_TSYN		= 0x02,
	ICE_TX_CTX_DESC_IL2TAG2		= 0x04,
	ICE_TX_CTX_DESC_IL2TAG2_IL2H	= 0x08,
	ICE_TX_CTX_DESC_SWTCH_NOTAG	= 0x00,
	ICE_TX_CTX_DESC_SWTCH_UPLINK	= 0x10,
	ICE_TX_CTX_DESC_SWTCH_LOCAL	= 0x20,
	ICE_TX_CTX_DESC_SWTCH_VSI	= 0x30,
	ICE_TX_CTX_DESC_RESERVED	= 0x40
};

enum ice_tx_ctx_desc_eipt_offload {
	ICE_TX_CTX_EIPT_NONE		= 0x0,
	ICE_TX_CTX_EIPT_IPV6		= 0x1,
	ICE_TX_CTX_EIPT_IPV4_NO_CSUM	= 0x2,
	ICE_TX_CTX_EIPT_IPV4		= 0x3
};

#define ICE_TXD_CTX_QW0_EIPT_S	0
#define ICE_TXD_CTX_QW0_EIPT_M	(0x3ULL << ICE_TXD_CTX_QW0_EIPT_S)

#define ICE_TXD_CTX_QW0_EIPLEN_S	2
#define ICE_TXD_CTX_QW0_EIPLEN_M	(0x7FUL << ICE_TXD_CTX_QW0_EIPLEN_S)

#define ICE_TXD_CTX_QW0_L4TUNT_S	9
#define ICE_TXD_CTX_QW0_L4TUNT_M	(0x3ULL << ICE_TXD_CTX_QW0_L4TUNT_S)

#define ICE_TXD_CTX_UDP_TUNNELING	BIT_ULL(ICE_TXD_CTX_QW0_L4TUNT_S)
#define ICE_TXD_CTX_GRE_TUNNELING	(0x2ULL << ICE_TXD_CTX_QW0_L4TUNT_S)

#define ICE_TXD_CTX_QW0_EIP_NOINC_S	11
#define ICE_TXD_CTX_QW0_EIP_NOINC_M	BIT_ULL(ICE_TXD_CTX_QW0_EIP_NOINC_S)

#define ICE_TXD_CTX_EIP_NOINC_IPID_CONST	ICE_TXD_CTX_QW0_EIP_NOINC_M

#define ICE_TXD_CTX_QW0_NATLEN_S	12
#define ICE_TXD_CTX_QW0_NATLEN_M	(0X7FULL << ICE_TXD_CTX_QW0_NATLEN_S)

#define ICE_TXD_CTX_QW0_DECTTL_S	19
#define ICE_TXD_CTX_QW0_DECTTL_M	(0xFULL << ICE_TXD_CTX_QW0_DECTTL_S)

#define ICE_TXD_CTX_QW0_L4T_CS_S	23
#define ICE_TXD_CTX_QW0_L4T_CS_M	BIT_ULL(ICE_TXD_CTX_QW0_L4T_CS_S)

#define ICE_LAN_TXQ_MAX_QGRPS	127
#define ICE_LAN_TXQ_MAX_QDIS	1023

/* Tx queue context data
 *
 * The sizes of the variables may be larger than needed due to crossing byte
 * boundaries. If we do not have the width of the variable set to the correct
 * size then we could end up shifting bits off the top of the variable when the
 * variable is at the top of a byte and crosses over into the next byte.
 */
struct ice_tlan_ctx {
#define ICE_TLAN_CTX_BASE_S	7
	u64 base;		/* base is defined in 128-byte units */
	u8 port_num;
	u16 cgd_num;		/* bigger than needed, see above for reason */
	u8 pf_num;
	u16 vmvf_num;
	u8 vmvf_type;
#define ICE_TLAN_CTX_VMVF_TYPE_VF	0
#define ICE_TLAN_CTX_VMVF_TYPE_VMQ	1
#define ICE_TLAN_CTX_VMVF_TYPE_PF	2
	u16 src_vsi;
	u8 tsyn_ena;
	u8 internal_usage_flag;
	u8 alt_vlan;
	u16 cpuid;		/* bigger than needed, see above for reason */
	u8 wb_mode;
	u8 tphrd_desc;
	u8 tphrd;
	u8 tphwr_desc;
	u16 cmpq_id;
	u16 qnum_in_func;
	u8 itr_notification_mode;
	u8 adjust_prof_id;
	u32 qlen;		/* bigger than needed, see above for reason */
	u8 quanta_prof_idx;
	u8 tso_ena;
	u16 tso_qnum;
	u8 legacy_int;
	u8 drop_ena;
	u8 cache_prof_idx;
	u8 pkt_shaper_prof_idx;
	u8 int_q_state;	/* width not needed - internal - DO NOT WRITE!!! */
	u16 tail;
};

/* LAN Tx Completion Queue data */
#pragma pack(1)
struct ice_tx_cmpltnq {
	u16 txq_id;
	u8 generation;
	u16 tx_head;
	u8 cmpl_type;
};
#pragma pack()

/* FIXME: move to a .c file that references this variable */
/* LAN Tx Completion Queue data info */
static const struct ice_ctx_ele ice_tx_cmpltnq_info[] = {
					   /* Field		Width	LSB */
	ICE_CTX_STORE(ice_tx_cmpltnq, txq_id,			14,	0),
	ICE_CTX_STORE(ice_tx_cmpltnq, generation,		1,	15),
	ICE_CTX_STORE(ice_tx_cmpltnq, tx_head,			13,	16),
	ICE_CTX_STORE(ice_tx_cmpltnq, cmpl_type,		3,	29),
	{ 0 }
};

/* LAN Tx Completion Queue Context */
#pragma pack(1)
struct ice_tx_cmpltnq_ctx {
	u64 base;
#define ICE_TX_CMPLTNQ_CTX_BASE_S	7
	u32 q_len;
#define ICE_TX_CMPLTNQ_CTX_Q_LEN_S	4
	u8 generation;
	u32 wrt_ptr;
	u8 pf_num;
	u16 vmvf_num;
	u8 vmvf_type;
#define ICE_TX_CMPLTNQ_CTX_VMVF_TYPE_VF		0
#define ICE_TX_CMPLTNQ_CTX_VMVF_TYPE_VMQ	1
#define ICE_TX_CMPLTNQ_CTX_VMVF_TYPE_PF		2
	u8 tph_desc_wr;
	u8 cpuid;
	u32 cmpltn_cache[16];
};
#pragma pack()

/* LAN Tx Doorbell Descriptor Format */
struct ice_tx_drbell_fmt {
	u16 txq_id;
	u8 dd;
	u8 rs;
	u32 db;
};

/* FIXME: move to a .c file that references this variable */
/* LAN Tx Doorbell Descriptor format info */
static const struct ice_ctx_ele ice_tx_drbell_fmt_info[] = {
					 /* Field		Width	LSB */
	ICE_CTX_STORE(ice_tx_drbell_fmt, txq_id,		14,	0),
	ICE_CTX_STORE(ice_tx_drbell_fmt, dd,			1,	14),
	ICE_CTX_STORE(ice_tx_drbell_fmt, rs,			1,	15),
	ICE_CTX_STORE(ice_tx_drbell_fmt, db,			32,	32),
	{ 0 }
};

/* LAN Tx Doorbell Queue Context */
#pragma pack(1)
struct ice_tx_drbell_q_ctx {
	u64 base;
#define ICE_TX_DRBELL_Q_CTX_BASE_S	7
	u16 ring_len;
#define ICE_TX_DRBELL_Q_CTX_RING_LEN_S	4
	u8 pf_num;
	u16 vf_num;
	u8 vmvf_type;
#define ICE_TX_DRBELL_Q_CTX_VMVF_TYPE_VF	0
#define ICE_TX_DRBELL_Q_CTX_VMVF_TYPE_VMQ	1
#define ICE_TX_DRBELL_Q_CTX_VMVF_TYPE_PF	2
	u8 cpuid;
	u8 tph_desc_rd;
	u8 tph_desc_wr;
	u8 db_q_en;
	u16 rd_head;
	u16 rd_tail;
};
#pragma pack()

/* The ice_ptype_lkup table is used to convert from the 10-bit ptype in the
 * hardware to a bit-field that can be used by SW to more easily determine the
 * packet type.
 *
 * Macros are used to shorten the table lines and make this table human
 * readable.
 *
 * We store the PTYPE in the top byte of the bit field - this is just so that
 * we can check that the table doesn't have a row missing, as the index into
 * the table should be the PTYPE.
 *
 * Typical work flow:
 *
 * IF NOT ice_ptype_lkup[ptype].known
 * THEN
 *      Packet is unknown
 * ELSE IF ice_ptype_lkup[ptype].outer_ip == ICE_RX_PTYPE_OUTER_IP
 *      Use the rest of the fields to look at the tunnels, inner protocols, etc
 * ELSE
 *      Use the enum ice_rx_l2_ptype to decode the packet type
 * ENDIF
 */

/* macro to make the table lines short */
#define ICE_PTT(PTYPE, OUTER_IP, OUTER_IP_VER, OUTER_FRAG, T, TE, TEF, I, PL)\
	{	1, \
		ICE_RX_PTYPE_OUTER_##OUTER_IP, \
		ICE_RX_PTYPE_OUTER_##OUTER_IP_VER, \
		ICE_RX_PTYPE_##OUTER_FRAG, \
		ICE_RX_PTYPE_TUNNEL_##T, \
		ICE_RX_PTYPE_TUNNEL_END_##TE, \
		ICE_RX_PTYPE_##TEF, \
		ICE_RX_PTYPE_INNER_PROT_##I, \
		ICE_RX_PTYPE_PAYLOAD_LAYER_##PL }

#define ICE_PTT_UNUSED_ENTRY(PTYPE) { 0, 0, 0, 0, 0, 0, 0, 0, 0 }

/* shorter macros makes the table fit but are terse */
#define ICE_RX_PTYPE_NOF		ICE_RX_PTYPE_NOT_FRAG
#define ICE_RX_PTYPE_FRG		ICE_RX_PTYPE_FRAG

/* Lookup table mapping the 10-bit HW PTYPE to the bit field for decoding */
static const struct ice_rx_ptype_decoded ice_ptype_lkup[1024] = {
	/* L2 Packet types */
	ICE_PTT_UNUSED_ENTRY(0),
	ICE_PTT(1, L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY2),
	ICE_PTT_UNUSED_ENTRY(2),
	ICE_PTT_UNUSED_ENTRY(3),
	ICE_PTT_UNUSED_ENTRY(4),
	ICE_PTT_UNUSED_ENTRY(5),
	ICE_PTT(6, L2, NONE, NOF, NONE, NONE, NOF, NONE, NONE),
	ICE_PTT(7, L2, NONE, NOF, NONE, NONE, NOF, NONE, NONE),
	ICE_PTT_UNUSED_ENTRY(8),
	ICE_PTT_UNUSED_ENTRY(9),
	ICE_PTT(10, L2, NONE, NOF, NONE, NONE, NOF, NONE, NONE),
	ICE_PTT(11, L2, NONE, NOF, NONE, NONE, NOF, NONE, NONE),
	ICE_PTT_UNUSED_ENTRY(12),
	ICE_PTT_UNUSED_ENTRY(13),
	ICE_PTT_UNUSED_ENTRY(14),
	ICE_PTT_UNUSED_ENTRY(15),
	ICE_PTT_UNUSED_ENTRY(16),
	ICE_PTT_UNUSED_ENTRY(17),
	ICE_PTT_UNUSED_ENTRY(18),
	ICE_PTT_UNUSED_ENTRY(19),
	ICE_PTT_UNUSED_ENTRY(20),
	ICE_PTT_UNUSED_ENTRY(21),

	/* Non Tunneled IPv4 */
	ICE_PTT(22, IP, IPV4, FRG, NONE, NONE, NOF, NONE, PAY3),
	ICE_PTT(23, IP, IPV4, NOF, NONE, NONE, NOF, NONE, PAY3),
	ICE_PTT(24, IP, IPV4, NOF, NONE, NONE, NOF, UDP,  PAY4),
	ICE_PTT_UNUSED_ENTRY(25),
	ICE_PTT(26, IP, IPV4, NOF, NONE, NONE, NOF, TCP,  PAY4),
	ICE_PTT(27, IP, IPV4, NOF, NONE, NONE, NOF, SCTP, PAY4),
	ICE_PTT(28, IP, IPV4, NOF, NONE, NONE, NOF, ICMP, PAY4),

	/* IPv4 --> IPv4 */
	ICE_PTT(29, IP, IPV4, NOF, IP_IP, IPV4, FRG, NONE, PAY3),
	ICE_PTT(30, IP, IPV4, NOF, IP_IP, IPV4, NOF, NONE, PAY3),
	ICE_PTT(31, IP, IPV4, NOF, IP_IP, IPV4, NOF, UDP,  PAY4),
	ICE_PTT_UNUSED_ENTRY(32),
	ICE_PTT(33, IP, IPV4, NOF, IP_IP, IPV4, NOF, TCP,  PAY4),
	ICE_PTT(34, IP, IPV4, NOF, IP_IP, IPV4, NOF, SCTP, PAY4),
	ICE_PTT(35, IP, IPV4, NOF, IP_IP, IPV4, NOF, ICMP, PAY4),

	/* IPv4 --> IPv6 */
	ICE_PTT(36, IP, IPV4, NOF, IP_IP, IPV6, FRG, NONE, PAY3),
	ICE_PTT(37, IP, IPV4, NOF, IP_IP, IPV6, NOF, NONE, PAY3),
	ICE_PTT(38, IP, IPV4, NOF, IP_IP, IPV6, NOF, UDP,  PAY4),
	ICE_PTT_UNUSED_ENTRY(39),
	ICE_PTT(40, IP, IPV4, NOF, IP_IP, IPV6, NOF, TCP,  PAY4),
	ICE_PTT(41, IP, IPV4, NOF, IP_IP, IPV6, NOF, SCTP, PAY4),
	ICE_PTT(42, IP, IPV4, NOF, IP_IP, IPV6, NOF, ICMP, PAY4),

	/* IPv4 --> GRE/NAT */
	ICE_PTT(43, IP, IPV4, NOF, IP_GRENAT, NONE, NOF, NONE, PAY3),

	/* IPv4 --> GRE/NAT --> IPv4 */
	ICE_PTT(44, IP, IPV4, NOF, IP_GRENAT, IPV4, FRG, NONE, PAY3),
	ICE_PTT(45, IP, IPV4, NOF, IP_GRENAT, IPV4, NOF, NONE, PAY3),
	ICE_PTT(46, IP, IPV4, NOF, IP_GRENAT, IPV4, NOF, UDP,  PAY4),
	ICE_PTT_UNUSED_ENTRY(47),
	ICE_PTT(48, IP, IPV4, NOF, IP_GRENAT, IPV4, NOF, TCP,  PAY4),
	ICE_PTT(49, IP, IPV4, NOF, IP_GRENAT, IPV4, NOF, SCTP, PAY4),
	ICE_PTT(50, IP, IPV4, NOF, IP_GRENAT, IPV4, NOF, ICMP, PAY4),

	/* IPv4 --> GRE/NAT --> IPv6 */
	ICE_PTT(51, IP, IPV4, NOF, IP_GRENAT, IPV6, FRG, NONE, PAY3),
	ICE_PTT(52, IP, IPV4, NOF, IP_GRENAT, IPV6, NOF, NONE, PAY3),
	ICE_PTT(53, IP, IPV4, NOF, IP_GRENAT, IPV6, NOF, UDP,  PAY4),
	ICE_PTT_UNUSED_ENTRY(54),
	ICE_PTT(55, IP, IPV4, NOF, IP_GRENAT, IPV6, NOF, TCP,  PAY4),
	ICE_PTT(56, IP, IPV4, NOF, IP_GRENAT, IPV6, NOF, SCTP, PAY4),
	ICE_PTT(57, IP, IPV4, NOF, IP_GRENAT, IPV6, NOF, ICMP, PAY4),

	/* IPv4 --> GRE/NAT --> MAC */
	ICE_PTT(58, IP, IPV4, NOF, IP_GRENAT_MAC, NONE, NOF, NONE, PAY3),

	/* IPv4 --> GRE/NAT --> MAC --> IPv4 */
	ICE_PTT(59, IP, IPV4, NOF, IP_GRENAT_MAC, IPV4, FRG, NONE, PAY3),
	ICE_PTT(60, IP, IPV4, NOF, IP_GRENAT_MAC, IPV4, NOF, NONE, PAY3),
	ICE_PTT(61, IP, IPV4, NOF, IP_GRENAT_MAC, IPV4, NOF, UDP,  PAY4),
	ICE_PTT_UNUSED_ENTRY(62),
	ICE_PTT(63, IP, IPV4, NOF, IP_GRENAT_MAC, IPV4, NOF, TCP,  PAY4),
	ICE_PTT(64, IP, IPV4, NOF, IP_GRENAT_MAC, IPV4, NOF, SCTP, PAY4),
	ICE_PTT(65, IP, IPV4, NOF, IP_GRENAT_MAC, IPV4, NOF, ICMP, PAY4),

	/* IPv4 --> GRE/NAT -> MAC --> IPv6 */
	ICE_PTT(66, IP, IPV4, NOF, IP_GRENAT_MAC, IPV6, FRG, NONE, PAY3),
	ICE_PTT(67, IP, IPV4, NOF, IP_GRENAT_MAC, IPV6, NOF, NONE, PAY3),
	ICE_PTT(68, IP, IPV4, NOF, IP_GRENAT_MAC, IPV6, NOF, UDP,  PAY4),
	ICE_PTT_UNUSED_ENTRY(69),
	ICE_PTT(70, IP, IPV4, NOF, IP_GRENAT_MAC, IPV6, NOF, TCP,  PAY4),
	ICE_PTT(71, IP, IPV4, NOF, IP_GRENAT_MAC, IPV6, NOF, SCTP, PAY4),
	ICE_PTT(72, IP, IPV4, NOF, IP_GRENAT_MAC, IPV6, NOF, ICMP, PAY4),

	/* IPv4 --> GRE/NAT --> MAC/VLAN */
	ICE_PTT(73, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, NONE, NOF, NONE, PAY3),

	/* IPv4 ---> GRE/NAT -> MAC/VLAN --> IPv4 */
	ICE_PTT(74, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV4, FRG, NONE, PAY3),
	ICE_PTT(75, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV4, NOF, NONE, PAY3),
	ICE_PTT(76, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV4, NOF, UDP,  PAY4),
	ICE_PTT_UNUSED_ENTRY(77),
	ICE_PTT(78, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV4, NOF, TCP,  PAY4),
	ICE_PTT(79, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV4, NOF, SCTP, PAY4),
	ICE_PTT(80, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV4, NOF, ICMP, PAY4),

	/* IPv4 -> GRE/NAT -> MAC/VLAN --> IPv6 */
	ICE_PTT(81, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV6, FRG, NONE, PAY3),
	ICE_PTT(82, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV6, NOF, NONE, PAY3),
	ICE_PTT(83, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV6, NOF, UDP,  PAY4),
	ICE_PTT_UNUSED_ENTRY(84),
	ICE_PTT(85, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV6, NOF, TCP,  PAY4),
	ICE_PTT(86, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV6, NOF, SCTP, PAY4),
	ICE_PTT(87, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV6, NOF, ICMP, PAY4),

	/* Non Tunneled IPv6 */
	ICE_PTT(88, IP, IPV6, FRG, NONE, NONE, NOF, NONE, PAY3),
	ICE_PTT(89, IP, IPV6, NOF, NONE, NONE, NOF, NONE, PAY3),
	ICE_PTT(90, IP, IPV6, NOF, NONE, NONE, NOF, UDP,  PAY4),
	ICE_PTT_UNUSED_ENTRY(91),
	ICE_PTT(92, IP, IPV6, NOF, NONE, NONE, NOF, TCP,  PAY4),
	ICE_PTT(93, IP, IPV6, NOF, NONE, NONE, NOF, SCTP, PAY4),
	ICE_PTT(94, IP, IPV6, NOF, NONE, NONE, NOF, ICMP, PAY4),

	/* IPv6 --> IPv4 */
	ICE_PTT(95, IP, IPV6, NOF, IP_IP, IPV4, FRG, NONE, PAY3),
	ICE_PTT(96, IP, IPV6, NOF, IP_IP, IPV4, NOF, NONE, PAY3),
	ICE_PTT(97, IP, IPV6, NOF, IP_IP, IPV4, NOF, UDP,  PAY4),
	ICE_PTT_UNUSED_ENTRY(98),
	ICE_PTT(99, IP, IPV6, NOF, IP_IP, IPV4, NOF, TCP,  PAY4),
	ICE_PTT(100, IP, IPV6, NOF, IP_IP, IPV4, NOF, SCTP, PAY4),
	ICE_PTT(101, IP, IPV6, NOF, IP_IP, IPV4, NOF, ICMP, PAY4),

	/* IPv6 --> IPv6 */
	ICE_PTT(102, IP, IPV6, NOF, IP_IP, IPV6, FRG, NONE, PAY3),
	ICE_PTT(103, IP, IPV6, NOF, IP_IP, IPV6, NOF, NONE, PAY3),
	ICE_PTT(104, IP, IPV6, NOF, IP_IP, IPV6, NOF, UDP,  PAY4),
	ICE_PTT_UNUSED_ENTRY(105),
	ICE_PTT(106, IP, IPV6, NOF, IP_IP, IPV6, NOF, TCP,  PAY4),
	ICE_PTT(107, IP, IPV6, NOF, IP_IP, IPV6, NOF, SCTP, PAY4),
	ICE_PTT(108, IP, IPV6, NOF, IP_IP, IPV6, NOF, ICMP, PAY4),

	/* IPv6 --> GRE/NAT */
	ICE_PTT(109, IP, IPV6, NOF, IP_GRENAT, NONE, NOF, NONE, PAY3),

	/* IPv6 --> GRE/NAT -> IPv4 */
	ICE_PTT(110, IP, IPV6, NOF, IP_GRENAT, IPV4, FRG, NONE, PAY3),
	ICE_PTT(111, IP, IPV6, NOF, IP_GRENAT, IPV4, NOF, NONE, PAY3),
	ICE_PTT(112, IP, IPV6, NOF, IP_GRENAT, IPV4, NOF, UDP,  PAY4),
	ICE_PTT_UNUSED_ENTRY(113),
	ICE_PTT(114, IP, IPV6, NOF, IP_GRENAT, IPV4, NOF, TCP,  PAY4),
	ICE_PTT(115, IP, IPV6, NOF, IP_GRENAT, IPV4, NOF, SCTP, PAY4),
	ICE_PTT(116, IP, IPV6, NOF, IP_GRENAT, IPV4, NOF, ICMP, PAY4),

	/* IPv6 --> GRE/NAT -> IPv6 */
	ICE_PTT(117, IP, IPV6, NOF, IP_GRENAT, IPV6, FRG, NONE, PAY3),
	ICE_PTT(118, IP, IPV6, NOF, IP_GRENAT, IPV6, NOF, NONE, PAY3),
	ICE_PTT(119, IP, IPV6, NOF, IP_GRENAT, IPV6, NOF, UDP,  PAY4),
	ICE_PTT_UNUSED_ENTRY(120),
	ICE_PTT(121, IP, IPV6, NOF, IP_GRENAT, IPV6, NOF, TCP,  PAY4),
	ICE_PTT(122, IP, IPV6, NOF, IP_GRENAT, IPV6, NOF, SCTP, PAY4),
	ICE_PTT(123, IP, IPV6, NOF, IP_GRENAT, IPV6, NOF, ICMP, PAY4),

	/* IPv6 --> GRE/NAT -> MAC */
	ICE_PTT(124, IP, IPV6, NOF, IP_GRENAT_MAC, NONE, NOF, NONE, PAY3),

	/* IPv6 --> GRE/NAT -> MAC -> IPv4 */
	ICE_PTT(125, IP, IPV6, NOF, IP_GRENAT_MAC, IPV4, FRG, NONE, PAY3),
	ICE_PTT(126, IP, IPV6, NOF, IP_GRENAT_MAC, IPV4, NOF, NONE, PAY3),
	ICE_PTT(127, IP, IPV6, NOF, IP_GRENAT_MAC, IPV4, NOF, UDP,  PAY4),
	ICE_PTT_UNUSED_ENTRY(128),
	ICE_PTT(129, IP, IPV6, NOF, IP_GRENAT_MAC, IPV4, NOF, TCP,  PAY4),
	ICE_PTT(130, IP, IPV6, NOF, IP_GRENAT_MAC, IPV4, NOF, SCTP, PAY4),
	ICE_PTT(131, IP, IPV6, NOF, IP_GRENAT_MAC, IPV4, NOF, ICMP, PAY4),

	/* IPv6 --> GRE/NAT -> MAC -> IPv6 */
	ICE_PTT(132, IP, IPV6, NOF, IP_GRENAT_MAC, IPV6, FRG, NONE, PAY3),
	ICE_PTT(133, IP, IPV6, NOF, IP_GRENAT_MAC, IPV6, NOF, NONE, PAY3),
	ICE_PTT(134, IP, IPV6, NOF, IP_GRENAT_MAC, IPV6, NOF, UDP,  PAY4),
	ICE_PTT_UNUSED_ENTRY(135),
	ICE_PTT(136, IP, IPV6, NOF, IP_GRENAT_MAC, IPV6, NOF, TCP,  PAY4),
	ICE_PTT(137, IP, IPV6, NOF, IP_GRENAT_MAC, IPV6, NOF, SCTP, PAY4),
	ICE_PTT(138, IP, IPV6, NOF, IP_GRENAT_MAC, IPV6, NOF, ICMP, PAY4),

	/* IPv6 --> GRE/NAT -> MAC/VLAN */
	ICE_PTT(139, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, NONE, NOF, NONE, PAY3),

	/* IPv6 --> GRE/NAT -> MAC/VLAN --> IPv4 */
	ICE_PTT(140, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV4, FRG, NONE, PAY3),
	ICE_PTT(141, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV4, NOF, NONE, PAY3),
	ICE_PTT(142, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV4, NOF, UDP,  PAY4),
	ICE_PTT_UNUSED_ENTRY(143),
	ICE_PTT(144, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV4, NOF, TCP,  PAY4),
	ICE_PTT(145, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV4, NOF, SCTP, PAY4),
	ICE_PTT(146, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV4, NOF, ICMP, PAY4),

	/* IPv6 --> GRE/NAT -> MAC/VLAN --> IPv6 */
	ICE_PTT(147, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV6, FRG, NONE, PAY3),
	ICE_PTT(148, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV6, NOF, NONE, PAY3),
	ICE_PTT(149, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV6, NOF, UDP,  PAY4),
	ICE_PTT_UNUSED_ENTRY(150),
	ICE_PTT(151, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV6, NOF, TCP,  PAY4),
	ICE_PTT(152, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV6, NOF, SCTP, PAY4),
	ICE_PTT(153, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV6, NOF, ICMP, PAY4),

	/* unused entries */
	ICE_PTT_UNUSED_ENTRY(154),
	ICE_PTT_UNUSED_ENTRY(155),
	ICE_PTT_UNUSED_ENTRY(156),
	ICE_PTT_UNUSED_ENTRY(157),
	ICE_PTT_UNUSED_ENTRY(158),
	ICE_PTT_UNUSED_ENTRY(159),

	ICE_PTT_UNUSED_ENTRY(160),
	ICE_PTT_UNUSED_ENTRY(161),
	ICE_PTT_UNUSED_ENTRY(162),
	ICE_PTT_UNUSED_ENTRY(163),
	ICE_PTT_UNUSED_ENTRY(164),
	ICE_PTT_UNUSED_ENTRY(165),
	ICE_PTT_UNUSED_ENTRY(166),
	ICE_PTT_UNUSED_ENTRY(167),
	ICE_PTT_UNUSED_ENTRY(168),
	ICE_PTT_UNUSED_ENTRY(169),

	ICE_PTT_UNUSED_ENTRY(170),
	ICE_PTT_UNUSED_ENTRY(171),
	ICE_PTT_UNUSED_ENTRY(172),
	ICE_PTT_UNUSED_ENTRY(173),
	ICE_PTT_UNUSED_ENTRY(174),
	ICE_PTT_UNUSED_ENTRY(175),
	ICE_PTT_UNUSED_ENTRY(176),
	ICE_PTT_UNUSED_ENTRY(177),
	ICE_PTT_UNUSED_ENTRY(178),
	ICE_PTT_UNUSED_ENTRY(179),

	ICE_PTT_UNUSED_ENTRY(180),
	ICE_PTT_UNUSED_ENTRY(181),
	ICE_PTT_UNUSED_ENTRY(182),
	ICE_PTT_UNUSED_ENTRY(183),
	ICE_PTT_UNUSED_ENTRY(184),
	ICE_PTT_UNUSED_ENTRY(185),
	ICE_PTT_UNUSED_ENTRY(186),
	ICE_PTT_UNUSED_ENTRY(187),
	ICE_PTT_UNUSED_ENTRY(188),
	ICE_PTT_UNUSED_ENTRY(189),

	ICE_PTT_UNUSED_ENTRY(190),
	ICE_PTT_UNUSED_ENTRY(191),
	ICE_PTT_UNUSED_ENTRY(192),
	ICE_PTT_UNUSED_ENTRY(193),
	ICE_PTT_UNUSED_ENTRY(194),
	ICE_PTT_UNUSED_ENTRY(195),
	ICE_PTT_UNUSED_ENTRY(196),
	ICE_PTT_UNUSED_ENTRY(197),
	ICE_PTT_UNUSED_ENTRY(198),
	ICE_PTT_UNUSED_ENTRY(199),

	ICE_PTT_UNUSED_ENTRY(200),
	ICE_PTT_UNUSED_ENTRY(201),
	ICE_PTT_UNUSED_ENTRY(202),
	ICE_PTT_UNUSED_ENTRY(203),
	ICE_PTT_UNUSED_ENTRY(204),
	ICE_PTT_UNUSED_ENTRY(205),
	ICE_PTT_UNUSED_ENTRY(206),
	ICE_PTT_UNUSED_ENTRY(207),
	ICE_PTT_UNUSED_ENTRY(208),
	ICE_PTT_UNUSED_ENTRY(209),

	ICE_PTT_UNUSED_ENTRY(210),
	ICE_PTT_UNUSED_ENTRY(211),
	ICE_PTT_UNUSED_ENTRY(212),
	ICE_PTT_UNUSED_ENTRY(213),
	ICE_PTT_UNUSED_ENTRY(214),
	ICE_PTT_UNUSED_ENTRY(215),
	ICE_PTT_UNUSED_ENTRY(216),
	ICE_PTT_UNUSED_ENTRY(217),
	ICE_PTT_UNUSED_ENTRY(218),
	ICE_PTT_UNUSED_ENTRY(219),

	ICE_PTT_UNUSED_ENTRY(220),
	ICE_PTT_UNUSED_ENTRY(221),
	ICE_PTT_UNUSED_ENTRY(222),
	ICE_PTT_UNUSED_ENTRY(223),
	ICE_PTT_UNUSED_ENTRY(224),
	ICE_PTT_UNUSED_ENTRY(225),
	ICE_PTT_UNUSED_ENTRY(226),
	ICE_PTT_UNUSED_ENTRY(227),
	ICE_PTT_UNUSED_ENTRY(228),
	ICE_PTT_UNUSED_ENTRY(229),

	ICE_PTT_UNUSED_ENTRY(230),
	ICE_PTT_UNUSED_ENTRY(231),
	ICE_PTT_UNUSED_ENTRY(232),
	ICE_PTT_UNUSED_ENTRY(233),
	ICE_PTT_UNUSED_ENTRY(234),
	ICE_PTT_UNUSED_ENTRY(235),
	ICE_PTT_UNUSED_ENTRY(236),
	ICE_PTT_UNUSED_ENTRY(237),
	ICE_PTT_UNUSED_ENTRY(238),
	ICE_PTT_UNUSED_ENTRY(239),

	ICE_PTT_UNUSED_ENTRY(240),
	ICE_PTT_UNUSED_ENTRY(241),
	ICE_PTT_UNUSED_ENTRY(242),
	ICE_PTT_UNUSED_ENTRY(243),
	ICE_PTT_UNUSED_ENTRY(244),
	ICE_PTT_UNUSED_ENTRY(245),
	ICE_PTT_UNUSED_ENTRY(246),
	ICE_PTT_UNUSED_ENTRY(247),
	ICE_PTT_UNUSED_ENTRY(248),
	ICE_PTT_UNUSED_ENTRY(249),

	ICE_PTT_UNUSED_ENTRY(250),
	ICE_PTT_UNUSED_ENTRY(251),
	ICE_PTT_UNUSED_ENTRY(252),
	ICE_PTT_UNUSED_ENTRY(253),
	ICE_PTT_UNUSED_ENTRY(254),
	ICE_PTT_UNUSED_ENTRY(255),
	ICE_PTT_UNUSED_ENTRY(256),
	ICE_PTT_UNUSED_ENTRY(257),
	ICE_PTT_UNUSED_ENTRY(258),
	ICE_PTT_UNUSED_ENTRY(259),

	ICE_PTT_UNUSED_ENTRY(260),
	ICE_PTT_UNUSED_ENTRY(261),
	ICE_PTT_UNUSED_ENTRY(262),
	ICE_PTT_UNUSED_ENTRY(263),
	ICE_PTT_UNUSED_ENTRY(264),
	ICE_PTT_UNUSED_ENTRY(265),
	ICE_PTT_UNUSED_ENTRY(266),
	ICE_PTT_UNUSED_ENTRY(267),
	ICE_PTT_UNUSED_ENTRY(268),
	ICE_PTT_UNUSED_ENTRY(269),

	ICE_PTT_UNUSED_ENTRY(270),
	ICE_PTT_UNUSED_ENTRY(271),
	ICE_PTT_UNUSED_ENTRY(272),
	ICE_PTT_UNUSED_ENTRY(273),
	ICE_PTT_UNUSED_ENTRY(274),
	ICE_PTT_UNUSED_ENTRY(275),
	ICE_PTT_UNUSED_ENTRY(276),
	ICE_PTT_UNUSED_ENTRY(277),
	ICE_PTT_UNUSED_ENTRY(278),
	ICE_PTT_UNUSED_ENTRY(279),

	ICE_PTT_UNUSED_ENTRY(280),
	ICE_PTT_UNUSED_ENTRY(281),
	ICE_PTT_UNUSED_ENTRY(282),
	ICE_PTT_UNUSED_ENTRY(283),
	ICE_PTT_UNUSED_ENTRY(284),
	ICE_PTT_UNUSED_ENTRY(285),
	ICE_PTT_UNUSED_ENTRY(286),
	ICE_PTT_UNUSED_ENTRY(287),
	ICE_PTT_UNUSED_ENTRY(288),
	ICE_PTT_UNUSED_ENTRY(289),

	ICE_PTT_UNUSED_ENTRY(290),
	ICE_PTT_UNUSED_ENTRY(291),
	ICE_PTT_UNUSED_ENTRY(292),
	ICE_PTT_UNUSED_ENTRY(293),
	ICE_PTT_UNUSED_ENTRY(294),
	ICE_PTT_UNUSED_ENTRY(295),
	ICE_PTT_UNUSED_ENTRY(296),
	ICE_PTT_UNUSED_ENTRY(297),
	ICE_PTT_UNUSED_ENTRY(298),
	ICE_PTT_UNUSED_ENTRY(299),

	ICE_PTT_UNUSED_ENTRY(300),
	ICE_PTT_UNUSED_ENTRY(301),
	ICE_PTT_UNUSED_ENTRY(302),
	ICE_PTT_UNUSED_ENTRY(303),
	ICE_PTT_UNUSED_ENTRY(304),
	ICE_PTT_UNUSED_ENTRY(305),
	ICE_PTT_UNUSED_ENTRY(306),
	ICE_PTT_UNUSED_ENTRY(307),
	ICE_PTT_UNUSED_ENTRY(308),
	ICE_PTT_UNUSED_ENTRY(309),

	ICE_PTT_UNUSED_ENTRY(310),
	ICE_PTT_UNUSED_ENTRY(311),
	ICE_PTT_UNUSED_ENTRY(312),
	ICE_PTT_UNUSED_ENTRY(313),
	ICE_PTT_UNUSED_ENTRY(314),
	ICE_PTT_UNUSED_ENTRY(315),
	ICE_PTT_UNUSED_ENTRY(316),
	ICE_PTT_UNUSED_ENTRY(317),
	ICE_PTT_UNUSED_ENTRY(318),
	ICE_PTT_UNUSED_ENTRY(319),

	ICE_PTT_UNUSED_ENTRY(320),
	ICE_PTT_UNUSED_ENTRY(321),
	ICE_PTT_UNUSED_ENTRY(322),
	ICE_PTT_UNUSED_ENTRY(323),
	ICE_PTT_UNUSED_ENTRY(324),
	ICE_PTT_UNUSED_ENTRY(325),
	ICE_PTT_UNUSED_ENTRY(326),
	ICE_PTT_UNUSED_ENTRY(327),
	ICE_PTT_UNUSED_ENTRY(328),
	ICE_PTT_UNUSED_ENTRY(329),

	ICE_PTT_UNUSED_ENTRY(330),
	ICE_PTT_UNUSED_ENTRY(331),
	ICE_PTT_UNUSED_ENTRY(332),
	ICE_PTT_UNUSED_ENTRY(333),
	ICE_PTT_UNUSED_ENTRY(334),
	ICE_PTT_UNUSED_ENTRY(335),
	ICE_PTT_UNUSED_ENTRY(336),
	ICE_PTT_UNUSED_ENTRY(337),
	ICE_PTT_UNUSED_ENTRY(338),
	ICE_PTT_UNUSED_ENTRY(339),

	ICE_PTT_UNUSED_ENTRY(340),
	ICE_PTT_UNUSED_ENTRY(341),
	ICE_PTT_UNUSED_ENTRY(342),
	ICE_PTT_UNUSED_ENTRY(343),
	ICE_PTT_UNUSED_ENTRY(344),
	ICE_PTT_UNUSED_ENTRY(345),
	ICE_PTT_UNUSED_ENTRY(346),
	ICE_PTT_UNUSED_ENTRY(347),
	ICE_PTT_UNUSED_ENTRY(348),
	ICE_PTT_UNUSED_ENTRY(349),

	ICE_PTT_UNUSED_ENTRY(350),
	ICE_PTT_UNUSED_ENTRY(351),
	ICE_PTT_UNUSED_ENTRY(352),
	ICE_PTT_UNUSED_ENTRY(353),
	ICE_PTT_UNUSED_ENTRY(354),
	ICE_PTT_UNUSED_ENTRY(355),
	ICE_PTT_UNUSED_ENTRY(356),
	ICE_PTT_UNUSED_ENTRY(357),
	ICE_PTT_UNUSED_ENTRY(358),
	ICE_PTT_UNUSED_ENTRY(359),

	ICE_PTT_UNUSED_ENTRY(360),
	ICE_PTT_UNUSED_ENTRY(361),
	ICE_PTT_UNUSED_ENTRY(362),
	ICE_PTT_UNUSED_ENTRY(363),
	ICE_PTT_UNUSED_ENTRY(364),
	ICE_PTT_UNUSED_ENTRY(365),
	ICE_PTT_UNUSED_ENTRY(366),
	ICE_PTT_UNUSED_ENTRY(367),
	ICE_PTT_UNUSED_ENTRY(368),
	ICE_PTT_UNUSED_ENTRY(369),

	ICE_PTT_UNUSED_ENTRY(370),
	ICE_PTT_UNUSED_ENTRY(371),
	ICE_PTT_UNUSED_ENTRY(372),
	ICE_PTT_UNUSED_ENTRY(373),
	ICE_PTT_UNUSED_ENTRY(374),
	ICE_PTT_UNUSED_ENTRY(375),
	ICE_PTT_UNUSED_ENTRY(376),
	ICE_PTT_UNUSED_ENTRY(377),
	ICE_PTT_UNUSED_ENTRY(378),
	ICE_PTT_UNUSED_ENTRY(379),

	ICE_PTT_UNUSED_ENTRY(380),
	ICE_PTT_UNUSED_ENTRY(381),
	ICE_PTT_UNUSED_ENTRY(382),
	ICE_PTT_UNUSED_ENTRY(383),
	ICE_PTT_UNUSED_ENTRY(384),
	ICE_PTT_UNUSED_ENTRY(385),
	ICE_PTT_UNUSED_ENTRY(386),
	ICE_PTT_UNUSED_ENTRY(387),
	ICE_PTT_UNUSED_ENTRY(388),
	ICE_PTT_UNUSED_ENTRY(389),

	ICE_PTT_UNUSED_ENTRY(390),
	ICE_PTT_UNUSED_ENTRY(391),
	ICE_PTT_UNUSED_ENTRY(392),
	ICE_PTT_UNUSED_ENTRY(393),
	ICE_PTT_UNUSED_ENTRY(394),
	ICE_PTT_UNUSED_ENTRY(395),
	ICE_PTT_UNUSED_ENTRY(396),
	ICE_PTT_UNUSED_ENTRY(397),
	ICE_PTT_UNUSED_ENTRY(398),
	ICE_PTT_UNUSED_ENTRY(399),

	ICE_PTT_UNUSED_ENTRY(400),
	ICE_PTT_UNUSED_ENTRY(401),
	ICE_PTT_UNUSED_ENTRY(402),
	ICE_PTT_UNUSED_ENTRY(403),
	ICE_PTT_UNUSED_ENTRY(404),
	ICE_PTT_UNUSED_ENTRY(405),
	ICE_PTT_UNUSED_ENTRY(406),
	ICE_PTT_UNUSED_ENTRY(407),
	ICE_PTT_UNUSED_ENTRY(408),
	ICE_PTT_UNUSED_ENTRY(409),

	ICE_PTT_UNUSED_ENTRY(410),
	ICE_PTT_UNUSED_ENTRY(411),
	ICE_PTT_UNUSED_ENTRY(412),
	ICE_PTT_UNUSED_ENTRY(413),
	ICE_PTT_UNUSED_ENTRY(414),
	ICE_PTT_UNUSED_ENTRY(415),
	ICE_PTT_UNUSED_ENTRY(416),
	ICE_PTT_UNUSED_ENTRY(417),
	ICE_PTT_UNUSED_ENTRY(418),
	ICE_PTT_UNUSED_ENTRY(419),

	ICE_PTT_UNUSED_ENTRY(420),
	ICE_PTT_UNUSED_ENTRY(421),
	ICE_PTT_UNUSED_ENTRY(422),
	ICE_PTT_UNUSED_ENTRY(423),
	ICE_PTT_UNUSED_ENTRY(424),
	ICE_PTT_UNUSED_ENTRY(425),
	ICE_PTT_UNUSED_ENTRY(426),
	ICE_PTT_UNUSED_ENTRY(427),
	ICE_PTT_UNUSED_ENTRY(428),
	ICE_PTT_UNUSED_ENTRY(429),

	ICE_PTT_UNUSED_ENTRY(430),
	ICE_PTT_UNUSED_ENTRY(431),
	ICE_PTT_UNUSED_ENTRY(432),
	ICE_PTT_UNUSED_ENTRY(433),
	ICE_PTT_UNUSED_ENTRY(434),
	ICE_PTT_UNUSED_ENTRY(435),
	ICE_PTT_UNUSED_ENTRY(436),
	ICE_PTT_UNUSED_ENTRY(437),
	ICE_PTT_UNUSED_ENTRY(438),
	ICE_PTT_UNUSED_ENTRY(439),

	ICE_PTT_UNUSED_ENTRY(440),
	ICE_PTT_UNUSED_ENTRY(441),
	ICE_PTT_UNUSED_ENTRY(442),
	ICE_PTT_UNUSED_ENTRY(443),
	ICE_PTT_UNUSED_ENTRY(444),
	ICE_PTT_UNUSED_ENTRY(445),
	ICE_PTT_UNUSED_ENTRY(446),
	ICE_PTT_UNUSED_ENTRY(447),
	ICE_PTT_UNUSED_ENTRY(448),
	ICE_PTT_UNUSED_ENTRY(449),

	ICE_PTT_UNUSED_ENTRY(450),
	ICE_PTT_UNUSED_ENTRY(451),
	ICE_PTT_UNUSED_ENTRY(452),
	ICE_PTT_UNUSED_ENTRY(453),
	ICE_PTT_UNUSED_ENTRY(454),
	ICE_PTT_UNUSED_ENTRY(455),
	ICE_PTT_UNUSED_ENTRY(456),
	ICE_PTT_UNUSED_ENTRY(457),
	ICE_PTT_UNUSED_ENTRY(458),
	ICE_PTT_UNUSED_ENTRY(459),

	ICE_PTT_UNUSED_ENTRY(460),
	ICE_PTT_UNUSED_ENTRY(461),
	ICE_PTT_UNUSED_ENTRY(462),
	ICE_PTT_UNUSED_ENTRY(463),
	ICE_PTT_UNUSED_ENTRY(464),
	ICE_PTT_UNUSED_ENTRY(465),
	ICE_PTT_UNUSED_ENTRY(466),
	ICE_PTT_UNUSED_ENTRY(467),
	ICE_PTT_UNUSED_ENTRY(468),
	ICE_PTT_UNUSED_ENTRY(469),

	ICE_PTT_UNUSED_ENTRY(470),
	ICE_PTT_UNUSED_ENTRY(471),
	ICE_PTT_UNUSED_ENTRY(472),
	ICE_PTT_UNUSED_ENTRY(473),
	ICE_PTT_UNUSED_ENTRY(474),
	ICE_PTT_UNUSED_ENTRY(475),
	ICE_PTT_UNUSED_ENTRY(476),
	ICE_PTT_UNUSED_ENTRY(477),
	ICE_PTT_UNUSED_ENTRY(478),
	ICE_PTT_UNUSED_ENTRY(479),

	ICE_PTT_UNUSED_ENTRY(480),
	ICE_PTT_UNUSED_ENTRY(481),
	ICE_PTT_UNUSED_ENTRY(482),
	ICE_PTT_UNUSED_ENTRY(483),
	ICE_PTT_UNUSED_ENTRY(484),
	ICE_PTT_UNUSED_ENTRY(485),
	ICE_PTT_UNUSED_ENTRY(486),
	ICE_PTT_UNUSED_ENTRY(487),
	ICE_PTT_UNUSED_ENTRY(488),
	ICE_PTT_UNUSED_ENTRY(489),

	ICE_PTT_UNUSED_ENTRY(490),
	ICE_PTT_UNUSED_ENTRY(491),
	ICE_PTT_UNUSED_ENTRY(492),
	ICE_PTT_UNUSED_ENTRY(493),
	ICE_PTT_UNUSED_ENTRY(494),
	ICE_PTT_UNUSED_ENTRY(495),
	ICE_PTT_UNUSED_ENTRY(496),
	ICE_PTT_UNUSED_ENTRY(497),
	ICE_PTT_UNUSED_ENTRY(498),
	ICE_PTT_UNUSED_ENTRY(499),

	ICE_PTT_UNUSED_ENTRY(500),
	ICE_PTT_UNUSED_ENTRY(501),
	ICE_PTT_UNUSED_ENTRY(502),
	ICE_PTT_UNUSED_ENTRY(503),
	ICE_PTT_UNUSED_ENTRY(504),
	ICE_PTT_UNUSED_ENTRY(505),
	ICE_PTT_UNUSED_ENTRY(506),
	ICE_PTT_UNUSED_ENTRY(507),
	ICE_PTT_UNUSED_ENTRY(508),
	ICE_PTT_UNUSED_ENTRY(509),

	ICE_PTT_UNUSED_ENTRY(510),
	ICE_PTT_UNUSED_ENTRY(511),
	ICE_PTT_UNUSED_ENTRY(512),
	ICE_PTT_UNUSED_ENTRY(513),
	ICE_PTT_UNUSED_ENTRY(514),
	ICE_PTT_UNUSED_ENTRY(515),
	ICE_PTT_UNUSED_ENTRY(516),
	ICE_PTT_UNUSED_ENTRY(517),
	ICE_PTT_UNUSED_ENTRY(518),
	ICE_PTT_UNUSED_ENTRY(519),

	ICE_PTT_UNUSED_ENTRY(520),
	ICE_PTT_UNUSED_ENTRY(521),
	ICE_PTT_UNUSED_ENTRY(522),
	ICE_PTT_UNUSED_ENTRY(523),
	ICE_PTT_UNUSED_ENTRY(524),
	ICE_PTT_UNUSED_ENTRY(525),
	ICE_PTT_UNUSED_ENTRY(526),
	ICE_PTT_UNUSED_ENTRY(527),
	ICE_PTT_UNUSED_ENTRY(528),
	ICE_PTT_UNUSED_ENTRY(529),

	ICE_PTT_UNUSED_ENTRY(530),
	ICE_PTT_UNUSED_ENTRY(531),
	ICE_PTT_UNUSED_ENTRY(532),
	ICE_PTT_UNUSED_ENTRY(533),
	ICE_PTT_UNUSED_ENTRY(534),
	ICE_PTT_UNUSED_ENTRY(535),
	ICE_PTT_UNUSED_ENTRY(536),
	ICE_PTT_UNUSED_ENTRY(537),
	ICE_PTT_UNUSED_ENTRY(538),
	ICE_PTT_UNUSED_ENTRY(539),

	ICE_PTT_UNUSED_ENTRY(540),
	ICE_PTT_UNUSED_ENTRY(541),
	ICE_PTT_UNUSED_ENTRY(542),
	ICE_PTT_UNUSED_ENTRY(543),
	ICE_PTT_UNUSED_ENTRY(544),
	ICE_PTT_UNUSED_ENTRY(545),
	ICE_PTT_UNUSED_ENTRY(546),
	ICE_PTT_UNUSED_ENTRY(547),
	ICE_PTT_UNUSED_ENTRY(548),
	ICE_PTT_UNUSED_ENTRY(549),

	ICE_PTT_UNUSED_ENTRY(550),
	ICE_PTT_UNUSED_ENTRY(551),
	ICE_PTT_UNUSED_ENTRY(552),
	ICE_PTT_UNUSED_ENTRY(553),
	ICE_PTT_UNUSED_ENTRY(554),
	ICE_PTT_UNUSED_ENTRY(555),
	ICE_PTT_UNUSED_ENTRY(556),
	ICE_PTT_UNUSED_ENTRY(557),
	ICE_PTT_UNUSED_ENTRY(558),
	ICE_PTT_UNUSED_ENTRY(559),

	ICE_PTT_UNUSED_ENTRY(560),
	ICE_PTT_UNUSED_ENTRY(561),
	ICE_PTT_UNUSED_ENTRY(562),
	ICE_PTT_UNUSED_ENTRY(563),
	ICE_PTT_UNUSED_ENTRY(564),
	ICE_PTT_UNUSED_ENTRY(565),
	ICE_PTT_UNUSED_ENTRY(566),
	ICE_PTT_UNUSED_ENTRY(567),
	ICE_PTT_UNUSED_ENTRY(568),
	ICE_PTT_UNUSED_ENTRY(569),

	ICE_PTT_UNUSED_ENTRY(570),
	ICE_PTT_UNUSED_ENTRY(571),
	ICE_PTT_UNUSED_ENTRY(572),
	ICE_PTT_UNUSED_ENTRY(573),
	ICE_PTT_UNUSED_ENTRY(574),
	ICE_PTT_UNUSED_ENTRY(575),
	ICE_PTT_UNUSED_ENTRY(576),
	ICE_PTT_UNUSED_ENTRY(577),
	ICE_PTT_UNUSED_ENTRY(578),
	ICE_PTT_UNUSED_ENTRY(579),

	ICE_PTT_UNUSED_ENTRY(580),
	ICE_PTT_UNUSED_ENTRY(581),
	ICE_PTT_UNUSED_ENTRY(582),
	ICE_PTT_UNUSED_ENTRY(583),
	ICE_PTT_UNUSED_ENTRY(584),
	ICE_PTT_UNUSED_ENTRY(585),
	ICE_PTT_UNUSED_ENTRY(586),
	ICE_PTT_UNUSED_ENTRY(587),
	ICE_PTT_UNUSED_ENTRY(588),
	ICE_PTT_UNUSED_ENTRY(589),

	ICE_PTT_UNUSED_ENTRY(590),
	ICE_PTT_UNUSED_ENTRY(591),
	ICE_PTT_UNUSED_ENTRY(592),
	ICE_PTT_UNUSED_ENTRY(593),
	ICE_PTT_UNUSED_ENTRY(594),
	ICE_PTT_UNUSED_ENTRY(595),
	ICE_PTT_UNUSED_ENTRY(596),
	ICE_PTT_UNUSED_ENTRY(597),
	ICE_PTT_UNUSED_ENTRY(598),
	ICE_PTT_UNUSED_ENTRY(599),

	ICE_PTT_UNUSED_ENTRY(600),
	ICE_PTT_UNUSED_ENTRY(601),
	ICE_PTT_UNUSED_ENTRY(602),
	ICE_PTT_UNUSED_ENTRY(603),
	ICE_PTT_UNUSED_ENTRY(604),
	ICE_PTT_UNUSED_ENTRY(605),
	ICE_PTT_UNUSED_ENTRY(606),
	ICE_PTT_UNUSED_ENTRY(607),
	ICE_PTT_UNUSED_ENTRY(608),
	ICE_PTT_UNUSED_ENTRY(609),

	ICE_PTT_UNUSED_ENTRY(610),
	ICE_PTT_UNUSED_ENTRY(611),
	ICE_PTT_UNUSED_ENTRY(612),
	ICE_PTT_UNUSED_ENTRY(613),
	ICE_PTT_UNUSED_ENTRY(614),
	ICE_PTT_UNUSED_ENTRY(615),
	ICE_PTT_UNUSED_ENTRY(616),
	ICE_PTT_UNUSED_ENTRY(617),
	ICE_PTT_UNUSED_ENTRY(618),
	ICE_PTT_UNUSED_ENTRY(619),

	ICE_PTT_UNUSED_ENTRY(620),
	ICE_PTT_UNUSED_ENTRY(621),
	ICE_PTT_UNUSED_ENTRY(622),
	ICE_PTT_UNUSED_ENTRY(623),
	ICE_PTT_UNUSED_ENTRY(624),
	ICE_PTT_UNUSED_ENTRY(625),
	ICE_PTT_UNUSED_ENTRY(626),
	ICE_PTT_UNUSED_ENTRY(627),
	ICE_PTT_UNUSED_ENTRY(628),
	ICE_PTT_UNUSED_ENTRY(629),

	ICE_PTT_UNUSED_ENTRY(630),
	ICE_PTT_UNUSED_ENTRY(631),
	ICE_PTT_UNUSED_ENTRY(632),
	ICE_PTT_UNUSED_ENTRY(633),
	ICE_PTT_UNUSED_ENTRY(634),
	ICE_PTT_UNUSED_ENTRY(635),
	ICE_PTT_UNUSED_ENTRY(636),
	ICE_PTT_UNUSED_ENTRY(637),
	ICE_PTT_UNUSED_ENTRY(638),
	ICE_PTT_UNUSED_ENTRY(639),

	ICE_PTT_UNUSED_ENTRY(640),
	ICE_PTT_UNUSED_ENTRY(641),
	ICE_PTT_UNUSED_ENTRY(642),
	ICE_PTT_UNUSED_ENTRY(643),
	ICE_PTT_UNUSED_ENTRY(644),
	ICE_PTT_UNUSED_ENTRY(645),
	ICE_PTT_UNUSED_ENTRY(646),
	ICE_PTT_UNUSED_ENTRY(647),
	ICE_PTT_UNUSED_ENTRY(648),
	ICE_PTT_UNUSED_ENTRY(649),

	ICE_PTT_UNUSED_ENTRY(650),
	ICE_PTT_UNUSED_ENTRY(651),
	ICE_PTT_UNUSED_ENTRY(652),
	ICE_PTT_UNUSED_ENTRY(653),
	ICE_PTT_UNUSED_ENTRY(654),
	ICE_PTT_UNUSED_ENTRY(655),
	ICE_PTT_UNUSED_ENTRY(656),
	ICE_PTT_UNUSED_ENTRY(657),
	ICE_PTT_UNUSED_ENTRY(658),
	ICE_PTT_UNUSED_ENTRY(659),

	ICE_PTT_UNUSED_ENTRY(660),
	ICE_PTT_UNUSED_ENTRY(661),
	ICE_PTT_UNUSED_ENTRY(662),
	ICE_PTT_UNUSED_ENTRY(663),
	ICE_PTT_UNUSED_ENTRY(664),
	ICE_PTT_UNUSED_ENTRY(665),
	ICE_PTT_UNUSED_ENTRY(666),
	ICE_PTT_UNUSED_ENTRY(667),
	ICE_PTT_UNUSED_ENTRY(668),
	ICE_PTT_UNUSED_ENTRY(669),

	ICE_PTT_UNUSED_ENTRY(670),
	ICE_PTT_UNUSED_ENTRY(671),
	ICE_PTT_UNUSED_ENTRY(672),
	ICE_PTT_UNUSED_ENTRY(673),
	ICE_PTT_UNUSED_ENTRY(674),
	ICE_PTT_UNUSED_ENTRY(675),
	ICE_PTT_UNUSED_ENTRY(676),
	ICE_PTT_UNUSED_ENTRY(677),
	ICE_PTT_UNUSED_ENTRY(678),
	ICE_PTT_UNUSED_ENTRY(679),

	ICE_PTT_UNUSED_ENTRY(680),
	ICE_PTT_UNUSED_ENTRY(681),
	ICE_PTT_UNUSED_ENTRY(682),
	ICE_PTT_UNUSED_ENTRY(683),
	ICE_PTT_UNUSED_ENTRY(684),
	ICE_PTT_UNUSED_ENTRY(685),
	ICE_PTT_UNUSED_ENTRY(686),
	ICE_PTT_UNUSED_ENTRY(687),
	ICE_PTT_UNUSED_ENTRY(688),
	ICE_PTT_UNUSED_ENTRY(689),

	ICE_PTT_UNUSED_ENTRY(690),
	ICE_PTT_UNUSED_ENTRY(691),
	ICE_PTT_UNUSED_ENTRY(692),
	ICE_PTT_UNUSED_ENTRY(693),
	ICE_PTT_UNUSED_ENTRY(694),
	ICE_PTT_UNUSED_ENTRY(695),
	ICE_PTT_UNUSED_ENTRY(696),
	ICE_PTT_UNUSED_ENTRY(697),
	ICE_PTT_UNUSED_ENTRY(698),
	ICE_PTT_UNUSED_ENTRY(699),

	ICE_PTT_UNUSED_ENTRY(700),
	ICE_PTT_UNUSED_ENTRY(701),
	ICE_PTT_UNUSED_ENTRY(702),
	ICE_PTT_UNUSED_ENTRY(703),
	ICE_PTT_UNUSED_ENTRY(704),
	ICE_PTT_UNUSED_ENTRY(705),
	ICE_PTT_UNUSED_ENTRY(706),
	ICE_PTT_UNUSED_ENTRY(707),
	ICE_PTT_UNUSED_ENTRY(708),
	ICE_PTT_UNUSED_ENTRY(709),

	ICE_PTT_UNUSED_ENTRY(710),
	ICE_PTT_UNUSED_ENTRY(711),
	ICE_PTT_UNUSED_ENTRY(712),
	ICE_PTT_UNUSED_ENTRY(713),
	ICE_PTT_UNUSED_ENTRY(714),
	ICE_PTT_UNUSED_ENTRY(715),
	ICE_PTT_UNUSED_ENTRY(716),
	ICE_PTT_UNUSED_ENTRY(717),
	ICE_PTT_UNUSED_ENTRY(718),
	ICE_PTT_UNUSED_ENTRY(719),

	ICE_PTT_UNUSED_ENTRY(720),
	ICE_PTT_UNUSED_ENTRY(721),
	ICE_PTT_UNUSED_ENTRY(722),
	ICE_PTT_UNUSED_ENTRY(723),
	ICE_PTT_UNUSED_ENTRY(724),
	ICE_PTT_UNUSED_ENTRY(725),
	ICE_PTT_UNUSED_ENTRY(726),
	ICE_PTT_UNUSED_ENTRY(727),
	ICE_PTT_UNUSED_ENTRY(728),
	ICE_PTT_UNUSED_ENTRY(729),

	ICE_PTT_UNUSED_ENTRY(730),
	ICE_PTT_UNUSED_ENTRY(731),
	ICE_PTT_UNUSED_ENTRY(732),
	ICE_PTT_UNUSED_ENTRY(733),
	ICE_PTT_UNUSED_ENTRY(734),
	ICE_PTT_UNUSED_ENTRY(735),
	ICE_PTT_UNUSED_ENTRY(736),
	ICE_PTT_UNUSED_ENTRY(737),
	ICE_PTT_UNUSED_ENTRY(738),
	ICE_PTT_UNUSED_ENTRY(739),

	ICE_PTT_UNUSED_ENTRY(740),
	ICE_PTT_UNUSED_ENTRY(741),
	ICE_PTT_UNUSED_ENTRY(742),
	ICE_PTT_UNUSED_ENTRY(743),
	ICE_PTT_UNUSED_ENTRY(744),
	ICE_PTT_UNUSED_ENTRY(745),
	ICE_PTT_UNUSED_ENTRY(746),
	ICE_PTT_UNUSED_ENTRY(747),
	ICE_PTT_UNUSED_ENTRY(748),
	ICE_PTT_UNUSED_ENTRY(749),

	ICE_PTT_UNUSED_ENTRY(750),
	ICE_PTT_UNUSED_ENTRY(751),
	ICE_PTT_UNUSED_ENTRY(752),
	ICE_PTT_UNUSED_ENTRY(753),
	ICE_PTT_UNUSED_ENTRY(754),
	ICE_PTT_UNUSED_ENTRY(755),
	ICE_PTT_UNUSED_ENTRY(756),
	ICE_PTT_UNUSED_ENTRY(757),
	ICE_PTT_UNUSED_ENTRY(758),
	ICE_PTT_UNUSED_ENTRY(759),

	ICE_PTT_UNUSED_ENTRY(760),
	ICE_PTT_UNUSED_ENTRY(761),
	ICE_PTT_UNUSED_ENTRY(762),
	ICE_PTT_UNUSED_ENTRY(763),
	ICE_PTT_UNUSED_ENTRY(764),
	ICE_PTT_UNUSED_ENTRY(765),
	ICE_PTT_UNUSED_ENTRY(766),
	ICE_PTT_UNUSED_ENTRY(767),
	ICE_PTT_UNUSED_ENTRY(768),
	ICE_PTT_UNUSED_ENTRY(769),

	ICE_PTT_UNUSED_ENTRY(770),
	ICE_PTT_UNUSED_ENTRY(771),
	ICE_PTT_UNUSED_ENTRY(772),
	ICE_PTT_UNUSED_ENTRY(773),
	ICE_PTT_UNUSED_ENTRY(774),
	ICE_PTT_UNUSED_ENTRY(775),
	ICE_PTT_UNUSED_ENTRY(776),
	ICE_PTT_UNUSED_ENTRY(777),
	ICE_PTT_UNUSED_ENTRY(778),
	ICE_PTT_UNUSED_ENTRY(779),

	ICE_PTT_UNUSED_ENTRY(780),
	ICE_PTT_UNUSED_ENTRY(781),
	ICE_PTT_UNUSED_ENTRY(782),
	ICE_PTT_UNUSED_ENTRY(783),
	ICE_PTT_UNUSED_ENTRY(784),
	ICE_PTT_UNUSED_ENTRY(785),
	ICE_PTT_UNUSED_ENTRY(786),
	ICE_PTT_UNUSED_ENTRY(787),
	ICE_PTT_UNUSED_ENTRY(788),
	ICE_PTT_UNUSED_ENTRY(789),

	ICE_PTT_UNUSED_ENTRY(790),
	ICE_PTT_UNUSED_ENTRY(791),
	ICE_PTT_UNUSED_ENTRY(792),
	ICE_PTT_UNUSED_ENTRY(793),
	ICE_PTT_UNUSED_ENTRY(794),
	ICE_PTT_UNUSED_ENTRY(795),
	ICE_PTT_UNUSED_ENTRY(796),
	ICE_PTT_UNUSED_ENTRY(797),
	ICE_PTT_UNUSED_ENTRY(798),
	ICE_PTT_UNUSED_ENTRY(799),

	ICE_PTT_UNUSED_ENTRY(800),
	ICE_PTT_UNUSED_ENTRY(801),
	ICE_PTT_UNUSED_ENTRY(802),
	ICE_PTT_UNUSED_ENTRY(803),
	ICE_PTT_UNUSED_ENTRY(804),
	ICE_PTT_UNUSED_ENTRY(805),
	ICE_PTT_UNUSED_ENTRY(806),
	ICE_PTT_UNUSED_ENTRY(807),
	ICE_PTT_UNUSED_ENTRY(808),
	ICE_PTT_UNUSED_ENTRY(809),

	ICE_PTT_UNUSED_ENTRY(810),
	ICE_PTT_UNUSED_ENTRY(811),
	ICE_PTT_UNUSED_ENTRY(812),
	ICE_PTT_UNUSED_ENTRY(813),
	ICE_PTT_UNUSED_ENTRY(814),
	ICE_PTT_UNUSED_ENTRY(815),
	ICE_PTT_UNUSED_ENTRY(816),
	ICE_PTT_UNUSED_ENTRY(817),
	ICE_PTT_UNUSED_ENTRY(818),
	ICE_PTT_UNUSED_ENTRY(819),

	ICE_PTT_UNUSED_ENTRY(820),
	ICE_PTT_UNUSED_ENTRY(821),
	ICE_PTT_UNUSED_ENTRY(822),
	ICE_PTT_UNUSED_ENTRY(823),
	ICE_PTT_UNUSED_ENTRY(824),
	ICE_PTT_UNUSED_ENTRY(825),
	ICE_PTT_UNUSED_ENTRY(826),
	ICE_PTT_UNUSED_ENTRY(827),
	ICE_PTT_UNUSED_ENTRY(828),
	ICE_PTT_UNUSED_ENTRY(829),

	ICE_PTT_UNUSED_ENTRY(830),
	ICE_PTT_UNUSED_ENTRY(831),
	ICE_PTT_UNUSED_ENTRY(832),
	ICE_PTT_UNUSED_ENTRY(833),
	ICE_PTT_UNUSED_ENTRY(834),
	ICE_PTT_UNUSED_ENTRY(835),
	ICE_PTT_UNUSED_ENTRY(836),
	ICE_PTT_UNUSED_ENTRY(837),
	ICE_PTT_UNUSED_ENTRY(838),
	ICE_PTT_UNUSED_ENTRY(839),

	ICE_PTT_UNUSED_ENTRY(840),
	ICE_PTT_UNUSED_ENTRY(841),
	ICE_PTT_UNUSED_ENTRY(842),
	ICE_PTT_UNUSED_ENTRY(843),
	ICE_PTT_UNUSED_ENTRY(844),
	ICE_PTT_UNUSED_ENTRY(845),
	ICE_PTT_UNUSED_ENTRY(846),
	ICE_PTT_UNUSED_ENTRY(847),
	ICE_PTT_UNUSED_ENTRY(848),
	ICE_PTT_UNUSED_ENTRY(849),

	ICE_PTT_UNUSED_ENTRY(850),
	ICE_PTT_UNUSED_ENTRY(851),
	ICE_PTT_UNUSED_ENTRY(852),
	ICE_PTT_UNUSED_ENTRY(853),
	ICE_PTT_UNUSED_ENTRY(854),
	ICE_PTT_UNUSED_ENTRY(855),
	ICE_PTT_UNUSED_ENTRY(856),
	ICE_PTT_UNUSED_ENTRY(857),
	ICE_PTT_UNUSED_ENTRY(858),
	ICE_PTT_UNUSED_ENTRY(859),

	ICE_PTT_UNUSED_ENTRY(860),
	ICE_PTT_UNUSED_ENTRY(861),
	ICE_PTT_UNUSED_ENTRY(862),
	ICE_PTT_UNUSED_ENTRY(863),
	ICE_PTT_UNUSED_ENTRY(864),
	ICE_PTT_UNUSED_ENTRY(865),
	ICE_PTT_UNUSED_ENTRY(866),
	ICE_PTT_UNUSED_ENTRY(867),
	ICE_PTT_UNUSED_ENTRY(868),
	ICE_PTT_UNUSED_ENTRY(869),

	ICE_PTT_UNUSED_ENTRY(870),
	ICE_PTT_UNUSED_ENTRY(871),
	ICE_PTT_UNUSED_ENTRY(872),
	ICE_PTT_UNUSED_ENTRY(873),
	ICE_PTT_UNUSED_ENTRY(874),
	ICE_PTT_UNUSED_ENTRY(875),
	ICE_PTT_UNUSED_ENTRY(876),
	ICE_PTT_UNUSED_ENTRY(877),
	ICE_PTT_UNUSED_ENTRY(878),
	ICE_PTT_UNUSED_ENTRY(879),

	ICE_PTT_UNUSED_ENTRY(880),
	ICE_PTT_UNUSED_ENTRY(881),
	ICE_PTT_UNUSED_ENTRY(882),
	ICE_PTT_UNUSED_ENTRY(883),
	ICE_PTT_UNUSED_ENTRY(884),
	ICE_PTT_UNUSED_ENTRY(885),
	ICE_PTT_UNUSED_ENTRY(886),
	ICE_PTT_UNUSED_ENTRY(887),
	ICE_PTT_UNUSED_ENTRY(888),
	ICE_PTT_UNUSED_ENTRY(889),

	ICE_PTT_UNUSED_ENTRY(890),
	ICE_PTT_UNUSED_ENTRY(891),
	ICE_PTT_UNUSED_ENTRY(892),
	ICE_PTT_UNUSED_ENTRY(893),
	ICE_PTT_UNUSED_ENTRY(894),
	ICE_PTT_UNUSED_ENTRY(895),
	ICE_PTT_UNUSED_ENTRY(896),
	ICE_PTT_UNUSED_ENTRY(897),
	ICE_PTT_UNUSED_ENTRY(898),
	ICE_PTT_UNUSED_ENTRY(899),

	ICE_PTT_UNUSED_ENTRY(900),
	ICE_PTT_UNUSED_ENTRY(901),
	ICE_PTT_UNUSED_ENTRY(902),
	ICE_PTT_UNUSED_ENTRY(903),
	ICE_PTT_UNUSED_ENTRY(904),
	ICE_PTT_UNUSED_ENTRY(905),
	ICE_PTT_UNUSED_ENTRY(906),
	ICE_PTT_UNUSED_ENTRY(907),
	ICE_PTT_UNUSED_ENTRY(908),
	ICE_PTT_UNUSED_ENTRY(909),

	ICE_PTT_UNUSED_ENTRY(910),
	ICE_PTT_UNUSED_ENTRY(911),
	ICE_PTT_UNUSED_ENTRY(912),
	ICE_PTT_UNUSED_ENTRY(913),
	ICE_PTT_UNUSED_ENTRY(914),
	ICE_PTT_UNUSED_ENTRY(915),
	ICE_PTT_UNUSED_ENTRY(916),
	ICE_PTT_UNUSED_ENTRY(917),
	ICE_PTT_UNUSED_ENTRY(918),
	ICE_PTT_UNUSED_ENTRY(919),

	ICE_PTT_UNUSED_ENTRY(920),
	ICE_PTT_UNUSED_ENTRY(921),
	ICE_PTT_UNUSED_ENTRY(922),
	ICE_PTT_UNUSED_ENTRY(923),
	ICE_PTT_UNUSED_ENTRY(924),
	ICE_PTT_UNUSED_ENTRY(925),
	ICE_PTT_UNUSED_ENTRY(926),
	ICE_PTT_UNUSED_ENTRY(927),
	ICE_PTT_UNUSED_ENTRY(928),
	ICE_PTT_UNUSED_ENTRY(929),

	ICE_PTT_UNUSED_ENTRY(930),
	ICE_PTT_UNUSED_ENTRY(931),
	ICE_PTT_UNUSED_ENTRY(932),
	ICE_PTT_UNUSED_ENTRY(933),
	ICE_PTT_UNUSED_ENTRY(934),
	ICE_PTT_UNUSED_ENTRY(935),
	ICE_PTT_UNUSED_ENTRY(936),
	ICE_PTT_UNUSED_ENTRY(937),
	ICE_PTT_UNUSED_ENTRY(938),
	ICE_PTT_UNUSED_ENTRY(939),

	ICE_PTT_UNUSED_ENTRY(940),
	ICE_PTT_UNUSED_ENTRY(941),
	ICE_PTT_UNUSED_ENTRY(942),
	ICE_PTT_UNUSED_ENTRY(943),
	ICE_PTT_UNUSED_ENTRY(944),
	ICE_PTT_UNUSED_ENTRY(945),
	ICE_PTT_UNUSED_ENTRY(946),
	ICE_PTT_UNUSED_ENTRY(947),
	ICE_PTT_UNUSED_ENTRY(948),
	ICE_PTT_UNUSED_ENTRY(949),

	ICE_PTT_UNUSED_ENTRY(950),
	ICE_PTT_UNUSED_ENTRY(951),
	ICE_PTT_UNUSED_ENTRY(952),
	ICE_PTT_UNUSED_ENTRY(953),
	ICE_PTT_UNUSED_ENTRY(954),
	ICE_PTT_UNUSED_ENTRY(955),
	ICE_PTT_UNUSED_ENTRY(956),
	ICE_PTT_UNUSED_ENTRY(957),
	ICE_PTT_UNUSED_ENTRY(958),
	ICE_PTT_UNUSED_ENTRY(959),

	ICE_PTT_UNUSED_ENTRY(960),
	ICE_PTT_UNUSED_ENTRY(961),
	ICE_PTT_UNUSED_ENTRY(962),
	ICE_PTT_UNUSED_ENTRY(963),
	ICE_PTT_UNUSED_ENTRY(964),
	ICE_PTT_UNUSED_ENTRY(965),
	ICE_PTT_UNUSED_ENTRY(966),
	ICE_PTT_UNUSED_ENTRY(967),
	ICE_PTT_UNUSED_ENTRY(968),
	ICE_PTT_UNUSED_ENTRY(969),

	ICE_PTT_UNUSED_ENTRY(970),
	ICE_PTT_UNUSED_ENTRY(971),
	ICE_PTT_UNUSED_ENTRY(972),
	ICE_PTT_UNUSED_ENTRY(973),
	ICE_PTT_UNUSED_ENTRY(974),
	ICE_PTT_UNUSED_ENTRY(975),
	ICE_PTT_UNUSED_ENTRY(976),
	ICE_PTT_UNUSED_ENTRY(977),
	ICE_PTT_UNUSED_ENTRY(978),
	ICE_PTT_UNUSED_ENTRY(979),

	ICE_PTT_UNUSED_ENTRY(980),
	ICE_PTT_UNUSED_ENTRY(981),
	ICE_PTT_UNUSED_ENTRY(982),
	ICE_PTT_UNUSED_ENTRY(983),
	ICE_PTT_UNUSED_ENTRY(984),
	ICE_PTT_UNUSED_ENTRY(985),
	ICE_PTT_UNUSED_ENTRY(986),
	ICE_PTT_UNUSED_ENTRY(987),
	ICE_PTT_UNUSED_ENTRY(988),
	ICE_PTT_UNUSED_ENTRY(989),

	ICE_PTT_UNUSED_ENTRY(990),
	ICE_PTT_UNUSED_ENTRY(991),
	ICE_PTT_UNUSED_ENTRY(992),
	ICE_PTT_UNUSED_ENTRY(993),
	ICE_PTT_UNUSED_ENTRY(994),
	ICE_PTT_UNUSED_ENTRY(995),
	ICE_PTT_UNUSED_ENTRY(996),
	ICE_PTT_UNUSED_ENTRY(997),
	ICE_PTT_UNUSED_ENTRY(998),
	ICE_PTT_UNUSED_ENTRY(999),

	ICE_PTT_UNUSED_ENTRY(1000),
	ICE_PTT_UNUSED_ENTRY(1001),
	ICE_PTT_UNUSED_ENTRY(1002),
	ICE_PTT_UNUSED_ENTRY(1003),
	ICE_PTT_UNUSED_ENTRY(1004),
	ICE_PTT_UNUSED_ENTRY(1005),
	ICE_PTT_UNUSED_ENTRY(1006),
	ICE_PTT_UNUSED_ENTRY(1007),
	ICE_PTT_UNUSED_ENTRY(1008),
	ICE_PTT_UNUSED_ENTRY(1009),

	ICE_PTT_UNUSED_ENTRY(1010),
	ICE_PTT_UNUSED_ENTRY(1011),
	ICE_PTT_UNUSED_ENTRY(1012),
	ICE_PTT_UNUSED_ENTRY(1013),
	ICE_PTT_UNUSED_ENTRY(1014),
	ICE_PTT_UNUSED_ENTRY(1015),
	ICE_PTT_UNUSED_ENTRY(1016),
	ICE_PTT_UNUSED_ENTRY(1017),
	ICE_PTT_UNUSED_ENTRY(1018),
	ICE_PTT_UNUSED_ENTRY(1019),

	ICE_PTT_UNUSED_ENTRY(1020),
	ICE_PTT_UNUSED_ENTRY(1021),
	ICE_PTT_UNUSED_ENTRY(1022),
	ICE_PTT_UNUSED_ENTRY(1023)
};

static inline struct ice_rx_ptype_decoded ice_decode_rx_desc_ptype(u16 ptype)
{
	return ice_ptype_lkup[ptype];
}

#define ICE_LINK_SPEED_UNKNOWN		0
#define ICE_LINK_SPEED_10MBPS		10
#define ICE_LINK_SPEED_100MBPS		100
#define ICE_LINK_SPEED_1000MBPS		1000
#define ICE_LINK_SPEED_2500MBPS		2500
#define ICE_LINK_SPEED_5000MBPS		5000
#define ICE_LINK_SPEED_10000MBPS	10000
#define ICE_LINK_SPEED_20000MBPS	20000
#define ICE_LINK_SPEED_25000MBPS	25000
#define ICE_LINK_SPEED_40000MBPS	40000
#define ICE_LINK_SPEED_50000MBPS	50000
#define ICE_LINK_SPEED_100000MBPS	100000
#endif /* _ICE_LAN_TX_RX_H_ */
