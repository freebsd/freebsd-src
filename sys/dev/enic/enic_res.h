/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2017 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 */

#ifndef _ENIC_RES_H_
#define _ENIC_RES_H_

#include "wq_enet_desc.h"
#include "rq_enet_desc.h"
#include "vnic_wq.h"
#include "vnic_rq.h"

#define ENIC_MIN_WQ_DESCS		64
#define ENIC_MAX_WQ_DESCS		4096
#define ENIC_MIN_RQ_DESCS		64
#define ENIC_MAX_RQ_DESCS		4096

#define ENIC_MAX_MULTICAST_ADDRESSES	32

/* A descriptor ring has a multiple of 32 descriptors */
#define ENIC_ALIGN_DESCS		32
#define ENIC_ALIGN_DESCS_MASK		~(ENIC_ALIGN_DESCS - 1)

/* Request a completion index every 32 buffers (roughly packets) */
#define ENIC_WQ_CQ_THRESH		32

#define ENIC_MIN_MTU			68

/* Does not include (possible) inserted VLAN tag and FCS */
#define ENIC_DEFAULT_RX_MAX_PKT_SIZE	9022

/* Does not include (possible) inserted VLAN tag and FCS */
#define ENIC_TX_MAX_PKT_SIZE		9208

#define ENIC_MULTICAST_PERFECT_FILTERS	32
#define ENIC_UNICAST_PERFECT_FILTERS	32

#define ENIC_NON_TSO_MAX_DESC		16
#define ENIC_DEFAULT_RX_FREE_THRESH	32
#define ENIC_TX_XMIT_MAX		64
#define ENIC_RX_BURST_MAX		64

/* Defaults for dev_info.default_{rx,tx}portconf */
#define ENIC_DEFAULT_RX_BURST		32
#define ENIC_DEFAULT_RX_RINGS		1
#define ENIC_DEFAULT_RX_RING_SIZE	512
#define ENIC_DEFAULT_TX_BURST		32
#define ENIC_DEFAULT_TX_RINGS		1
#define ENIC_DEFAULT_TX_RING_SIZE	512

#define ENIC_RSS_DEFAULT_CPU	0
#define ENIC_RSS_BASE_CPU	0
#define ENIC_RSS_HASH_BITS	7
#define ENIC_RSS_RETA_SIZE	(1 << ENIC_RSS_HASH_BITS)
#define ENIC_RSS_HASH_KEY_SIZE	40

#define ENIC_SETTING(enic, f) ((enic->config.flags & VENETF_##f) ? 1 : 0)

struct enic;

int enic_get_vnic_config(struct enic *);
int enic_add_vlan(struct enic *enic, u16 vlanid);
int enic_del_vlan(struct enic *enic, u16 vlanid);
int enic_set_nic_cfg(struct enic *enic, u8 rss_default_cpu, u8 rss_hash_type,
	u8 rss_hash_bits, u8 rss_base_cpu, u8 rss_enable, u8 tso_ipid_split_en,
	u8 ig_vlan_strip_en);
void enic_get_res_counts(struct enic *enic);
void enic_init_vnic_resources(struct enic *enic);

#endif /* _ENIC_RES_H_ */
