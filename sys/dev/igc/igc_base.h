/*-
 * Copyright 2021 Intel Corp
 * Copyright 2021 Rubicon Communications, LLC (Netgate)
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $FreeBSD$
 */

#ifndef _IGC_BASE_H_
#define _IGC_BASE_H_

/* forward declaration */
s32 igc_init_hw_base(struct igc_hw *hw);
void igc_power_down_phy_copper_base(struct igc_hw *hw);
extern void igc_rx_fifo_flush_base(struct igc_hw *hw);
s32 igc_acquire_phy_base(struct igc_hw *hw);
void igc_release_phy_base(struct igc_hw *hw);

/* Transmit Descriptor - Advanced */
union igc_adv_tx_desc {
	struct {
		__le64 buffer_addr;    /* Address of descriptor's data buf */
		__le32 cmd_type_len;
		__le32 olinfo_status;
	} read;
	struct {
		__le64 rsvd;       /* Reserved */
		__le32 nxtseq_seed;
		__le32 status;
	} wb;
};

/* Context descriptors */
struct igc_adv_tx_context_desc {
	__le32 vlan_macip_lens;
	union {
		__le32 launch_time;
		__le32 seqnum_seed;
	};
	__le32 type_tucmd_mlhl;
	__le32 mss_l4len_idx;
};

/* Adv Transmit Descriptor Config Masks */
#define IGC_ADVTXD_DTYP_CTXT	0x00200000 /* Advanced Context Descriptor */
#define IGC_ADVTXD_DTYP_DATA	0x00300000 /* Advanced Data Descriptor */
#define IGC_ADVTXD_DCMD_EOP	0x01000000 /* End of Packet */
#define IGC_ADVTXD_DCMD_IFCS	0x02000000 /* Insert FCS (Ethernet CRC) */
#define IGC_ADVTXD_DCMD_RS	0x08000000 /* Report Status */
#define IGC_ADVTXD_DCMD_DDTYP_ISCSI	0x10000000 /* DDP hdr type or iSCSI */
#define IGC_ADVTXD_DCMD_DEXT	0x20000000 /* Descriptor extension (1=Adv) */
#define IGC_ADVTXD_DCMD_VLE	0x40000000 /* VLAN pkt enable */
#define IGC_ADVTXD_DCMD_TSE	0x80000000 /* TCP Seg enable */
#define IGC_ADVTXD_MAC_LINKSEC	0x00040000 /* Apply LinkSec on pkt */
#define IGC_ADVTXD_MAC_TSTAMP		0x00080000 /* IEEE1588 Timestamp pkt */
#define IGC_ADVTXD_STAT_SN_CRC	0x00000002 /* NXTSEQ/SEED prsnt in WB */
#define IGC_ADVTXD_IDX_SHIFT		4  /* Adv desc Index shift */
#define IGC_ADVTXD_POPTS_ISCO_1ST	0x00000000 /* 1st TSO of iSCSI PDU */
#define IGC_ADVTXD_POPTS_ISCO_MDL	0x00000800 /* Middle TSO of iSCSI PDU */
#define IGC_ADVTXD_POPTS_ISCO_LAST	0x00001000 /* Last TSO of iSCSI PDU */
/* 1st & Last TSO-full iSCSI PDU*/
#define IGC_ADVTXD_POPTS_ISCO_FULL	0x00001800
#define IGC_ADVTXD_POPTS_IPSEC	0x00000400 /* IPSec offload request */
#define IGC_ADVTXD_PAYLEN_SHIFT	14 /* Adv desc PAYLEN shift */

/* Advanced Transmit Context Descriptor Config */
#define IGC_ADVTXD_MACLEN_SHIFT	9  /* Adv ctxt desc mac len shift */
#define IGC_ADVTXD_VLAN_SHIFT		16  /* Adv ctxt vlan tag shift */
#define IGC_ADVTXD_TUCMD_IPV4		0x00000400  /* IP Packet Type: 1=IPv4 */
#define IGC_ADVTXD_TUCMD_IPV6		0x00000000  /* IP Packet Type: 0=IPv6 */
#define IGC_ADVTXD_TUCMD_L4T_UDP	0x00000000  /* L4 Packet TYPE of UDP */
#define IGC_ADVTXD_TUCMD_L4T_TCP	0x00000800  /* L4 Packet TYPE of TCP */
#define IGC_ADVTXD_TUCMD_L4T_SCTP	0x00001000  /* L4 Packet TYPE of SCTP */
#define IGC_ADVTXD_TUCMD_IPSEC_TYPE_ESP	0x00002000 /* IPSec Type ESP */
/* IPSec Encrypt Enable for ESP */
#define IGC_ADVTXD_TUCMD_IPSEC_ENCRYPT_EN	0x00004000
/* Req requires Markers and CRC */
#define IGC_ADVTXD_TUCMD_MKRREQ	0x00002000
#define IGC_ADVTXD_L4LEN_SHIFT	8  /* Adv ctxt L4LEN shift */
#define IGC_ADVTXD_MSS_SHIFT		16  /* Adv ctxt MSS shift */
/* Adv ctxt IPSec SA IDX mask */
#define IGC_ADVTXD_IPSEC_SA_INDEX_MASK	0x000000FF
/* Adv ctxt IPSec ESP len mask */
#define IGC_ADVTXD_IPSEC_ESP_LEN_MASK		0x000000FF

#define IGC_RAR_ENTRIES_BASE		16

/* Receive Descriptor - Advanced */
union igc_adv_rx_desc {
	struct {
		__le64 pkt_addr; /* Packet buffer address */
		__le64 hdr_addr; /* Header buffer address */
	} read;
	struct {
		struct {
			union {
				__le32 data;
				struct {
					__le16 pkt_info; /*RSS type, Pkt type*/
					/* Split Header, header buffer len */
					__le16 hdr_info;
				} hs_rss;
			} lo_dword;
			union {
				__le32 rss; /* RSS Hash */
				struct {
					__le16 ip_id; /* IP id */
					__le16 csum; /* Packet Checksum */
				} csum_ip;
			} hi_dword;
		} lower;
		struct {
			__le32 status_error; /* ext status/error */
			__le16 length; /* Packet length */
			__le16 vlan; /* VLAN tag */
		} upper;
	} wb;  /* writeback */
};

/* Additional Transmit Descriptor Control definitions */
#define IGC_TXDCTL_QUEUE_ENABLE	0x02000000 /* Ena specific Tx Queue */

/* Additional Receive Descriptor Control definitions */
#define IGC_RXDCTL_QUEUE_ENABLE	0x02000000 /* Ena specific Rx Queue */

/* SRRCTL bit definitions */
#define IGC_SRRCTL_BSIZEPKT_SHIFT		10 /* Shift _right_ */
#define IGC_SRRCTL_BSIZEHDRSIZE_SHIFT		2  /* Shift _left_ */
#define IGC_SRRCTL_DESCTYPE_ADV_ONEBUF	0x02000000

#endif /* _IGC_BASE_H_ */
