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

#ifndef _AQ_RING_H_
#define _AQ_RING_H_

#include "aq_hw.h"

#define REFILL_THRESHOLD 128


typedef volatile struct {
    u32 rss_type:4;
    u32 pkt_type:8;
    u32 rdm_err:1;
    u32 rsvd:6;
    u32 rx_cntl:2;
    u32 sph:1;
    u32 hdr_len:10;
    u32 rss_hash;
    u16 dd:1;
    u16 eop:1;
    u16 rx_stat:4;
    u16 rx_estat:6;
    u16 rsc_cnt:4;
    u16 pkt_len;
    u16 next_desp;
    u16 vlan;
} __attribute__((__packed__)) aq_rx_wb_t;

typedef volatile struct {
    union {
        /* HW RX descriptor */
        struct __packed {
            u64 buf_addr;
            u64 hdr_addr;
        } read;

        /* HW RX descriptor writeback */
        aq_rx_wb_t wb;
    };
} __attribute__((__packed__)) aq_rx_desc_t;

/* Hardware tx descriptor */
typedef volatile struct {
    u64 buf_addr;

    union {
        struct {
            u32 type:3;
            u32 :1;
            u32 len:16;
            u32 dd:1;
            u32 eop:1;
            u32 cmd:8;
            u32 :14;
            u32 ct_idx:1;
            u32 ct_en:1;
            u32 pay_len:18;
        } __attribute__((__packed__));
        u64 flags;
    };
} __attribute__((__packed__)) aq_tx_desc_t;

enum aq_tx_desc_type {
    tx_desc_type_desc = 1,
    tx_desc_type_ctx = 2,
};

enum aq_tx_desc_cmd {
    tx_desc_cmd_vlan = 1,
    tx_desc_cmd_fcs = 2,
    tx_desc_cmd_ipv4 = 4,
    tx_desc_cmd_l4cs = 8,
    tx_desc_cmd_lso = 0x10,
    tx_desc_cmd_wb = 0x20,
};

/* Hardware tx context descriptor */
typedef volatile union {
    struct __packed {
        u64 flags1;
        u64 flags2;
    };

    struct __packed {
        u64 :40;
        u32 tun_len:8;
        u32 out_len:16;
        u32 type:3;
        u32 idx:1;
        u32 vlan_tag:16;
        u32 cmd:4;
        u32 l2_len:7;
        u32 l3_len:9;
        u32 l4_len:8;
        u32 mss_len:16;
    };
} __attribute__((__packed__)) aq_txc_desc_t;

struct aq_ring_stats {
	u64 rx_pkts;
	u64 rx_bytes;
	u64 jumbo_pkts;
	u64 rx_err;
	u64 irq;

	u64 tx_pkts;
	u64 tx_bytes;
	u64 tx_drops;
	u64 tx_queue_full;
};

struct aq_dev;

struct aq_ring {
    struct aq_dev *dev;
    int index;

    struct if_irq irq;
    int msix;
/* RX */
    qidx_t rx_size;
    int rx_max_frame_size;
    void *rx_desc_area_ptr;
    aq_rx_desc_t *rx_descs;
    uint64_t rx_descs_phys;

/* TX */
    int tx_head, tx_tail;
    qidx_t tx_size;
    void *tx_desc_area_ptr;
    aq_tx_desc_t *tx_descs;
    uint64_t tx_descs_phys;

    struct aq_ring_stats stats;
};

int aq_ring_rx_init(struct aq_hw *hw, struct aq_ring *ring);
int aq_ring_tx_init(struct aq_hw *hw, struct aq_ring *ring);

int aq_ring_tx_start(struct aq_hw *hw, struct aq_ring *ring);
int aq_ring_tx_stop(struct aq_hw *hw, struct aq_ring *ring);
int aq_ring_rx_start(struct aq_hw *hw, struct aq_ring *ring);
int aq_ring_rx_stop(struct aq_hw *hw, struct aq_ring *ring);

int aq_ring_tx_tail_update(struct aq_hw *hw, struct aq_ring *ring, u32 tail);


extern struct if_txrx aq_txrx;
int		aq_intr(void *arg);

#endif /* _AQ_RING_H_ */
