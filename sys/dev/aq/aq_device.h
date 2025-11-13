/*
 * aQuantia Corporation Network Driver
 * Copyright (C) 2014-2017 aQuantia Corporation. All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   (1) Redistributions of source code must retain the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer.
 *
 *   (2) Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials provided
 *   with the distribution.
 *
 *   (3)The name of the author may not be used to endorse or promote
 *   products derived from this software without specific prior
 *   written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _AQ_DEVICE_H_
#define _AQ_DEVICE_H_

#include "aq_hw.h"

enum aq_media_type {
	AQ_MEDIA_TYPE_UNKNOWN = 0,
	AQ_MEDIA_TYPE_FIBRE,
	AQ_MEDIA_TYPE_TP,
};

#define	AQ_LINK_UNKNOWN	0x00000000
#define	AQ_LINK_100M	0x00000001
#define	AQ_LINK_1G		0x00000002
#define	AQ_LINK_2G5		0x00000004
#define	AQ_LINK_5G		0x00000008
#define	AQ_LINK_10G		0x00000010

#define	AQ_LINK_ALL	(AQ_LINK_100M | AQ_LINK_1G | AQ_LINK_2G5 | AQ_LINK_5G | \
					 AQ_LINK_10G )

struct aq_stats_s {
    u64 prc;
    u64 uprc;
    u64 mprc;
    u64 bprc;
    u64 cprc;
    u64 erpr;
    u64 dpc;
    u64 brc;
    u64 ubrc;
    u64 mbrc;
    u64 bbrc;

    u64 ptc;
    u64 uptc;
    u64 mptc;
    u64 bptc;
    u64 erpt;
    u64 btc;
    u64 ubtc;
    u64 mbtc;
    u64 bbtc;
};

enum aq_dev_state_e {
    AQ_DEV_STATE_UNLOAD,
    AQ_DEV_STATE_PCI_STOP,
    AQ_DEV_STATE_DOWN,
    AQ_DEV_STATE_UP,
};

struct aq_rx_filters {
    unsigned int rule_cnt;
    struct aq_rx_filter_vlan vlan_filters[AQ_HW_VLAN_MAX_FILTERS];
    struct aq_rx_filter_l2 etype_filters[AQ_HW_ETYPE_MAX_FILTERS];
};

struct aq_vlan_tag {
	SLIST_ENTRY(aq_vlan_tag) next;
	uint16_t	tag;
};

struct aq_dev {
	device_t		dev;
	if_ctx_t		ctx;
	if_softc_ctx_t		scctx;
	if_shared_ctx_t		sctx;
	struct ifmedia *	media;

    struct aq_hw          hw;

	enum aq_media_type	media_type;
	uint32_t		link_speeds;
	uint32_t		chip_features;
	uint32_t		mbox_addr;
	uint8_t			mac_addr[ETHER_ADDR_LEN];
	uint64_t		admin_ticks;
	struct if_irq	irq;
	int				msix;

	int			mmio_rid;
	struct resource *	mmio_res;
	bus_space_tag_t		mmio_tag;
	bus_space_handle_t	mmio_handle;
	bus_size_t		mmio_size;

	struct aq_ring    *tx_rings[HW_ATL_B0_RINGS_MAX];
	struct aq_ring    *rx_rings[HW_ATL_B0_RINGS_MAX];
	uint32_t          tx_rings_count;
	uint32_t          rx_rings_count;
	bool              linkup;
	int               media_active;

	struct aq_hw_stats_s  last_stats;
	struct aq_stats_s     curr_stats;

	bitstr_t               *vlan_tags;
	int                     mcnt;

	uint8_t			rss_key[HW_ATL_RSS_HASHKEY_SIZE];
	uint8_t			rss_table[HW_ATL_RSS_INDIRECTION_TABLE_MAX];
};

typedef struct aq_dev aq_dev_t;

int aq_update_hw_stats(aq_dev_t *aq_dev);
void aq_initmedia(aq_dev_t *aq_dev);
int aq_linkstat_isr(void *arg);
int aq_isr_rx(void *arg);
void aq_mediastatus_update(aq_dev_t *aq_dev, u32 link_speed, const struct aq_hw_fc_info *fc_neg);
void aq_mediastatus(struct ifnet *ifp, struct ifmediareq *ifmr);
int aq_mediachange(struct ifnet *ifp);
void aq_if_update_admin_status(if_ctx_t ctx);

#endif
