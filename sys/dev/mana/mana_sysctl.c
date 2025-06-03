/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Microsoft Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>
#include "mana_sysctl.h"

static int mana_sysctl_cleanup_thread_cpu(SYSCTL_HANDLER_ARGS);

int mana_log_level = MANA_ALERT | MANA_WARNING | MANA_INFO;

unsigned int mana_tx_req_size;
unsigned int mana_rx_req_size;
unsigned int mana_rx_refill_threshold;

SYSCTL_NODE(_hw, OID_AUTO, mana, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "MANA driver parameters");

SYSCTL_UINT(_hw_mana, OID_AUTO, tx_req_size, CTLFLAG_RWTUN,
    &mana_tx_req_size, 0, "requested number of unit of tx queue");
SYSCTL_UINT(_hw_mana, OID_AUTO, rx_req_size, CTLFLAG_RWTUN,
    &mana_rx_req_size, 0, "requested number of unit of rx queue");
SYSCTL_UINT(_hw_mana, OID_AUTO, rx_refill_thresh, CTLFLAG_RWTUN,
    &mana_rx_refill_threshold, 0,
    "number of rx slots before starting the refill");

/*
 * Logging level for changing verbosity of the output
 */
SYSCTL_INT(_hw_mana, OID_AUTO, log_level, CTLFLAG_RWTUN,
    &mana_log_level, 0, "Logging level indicating verbosity of the logs");

SYSCTL_CONST_STRING(_hw_mana, OID_AUTO, driver_version, CTLFLAG_RD,
    DRV_MODULE_VERSION, "MANA driver version");

static int
mana_sysctl_rx_stat_agg_u64(SYSCTL_HANDLER_ARGS)
{
	struct mana_port_context *apc = arg1;
	int offset = arg2, i, err;
	struct mana_rxq *rxq;
	uint64_t stat;

	stat = 0;
	for (i = 0; i < apc->num_queues; i++) {
		rxq = apc->rxqs[i];
		stat += *((uint64_t *)((uint8_t *)rxq + offset));
	}

	err = sysctl_handle_64(oidp, &stat, 0, req);
	if (err || req->newptr == NULL)
		return err;

	for (i = 0; i < apc->num_queues; i++) {
		rxq = apc->rxqs[i];
		*((uint64_t *)((uint8_t *)rxq + offset)) = 0;
	}
	return 0;
}

static int
mana_sysctl_rx_stat_u16(SYSCTL_HANDLER_ARGS)
{
	struct mana_port_context *apc = arg1;
	int offset = arg2, err;
	struct mana_rxq *rxq;
	uint64_t stat;
	uint16_t val;

	rxq = apc->rxqs[0];
	val = *((uint16_t *)((uint8_t *)rxq + offset));
	stat = val;

	err = sysctl_handle_64(oidp, &stat, 0, req);
	if (err || req->newptr == NULL)
		return err;
	else
		return 0;
}

static int
mana_sysctl_rx_stat_u32(SYSCTL_HANDLER_ARGS)
{
	struct mana_port_context *apc = arg1;
	int offset = arg2, err;
	struct mana_rxq *rxq;
	uint64_t stat;
	uint32_t val;

	rxq = apc->rxqs[0];
	val = *((uint32_t *)((uint8_t *)rxq + offset));
	stat = val;

	err = sysctl_handle_64(oidp, &stat, 0, req);
	if (err || req->newptr == NULL)
		return err;
	else
		return 0;
}

static int
mana_sysctl_tx_stat_agg_u64(SYSCTL_HANDLER_ARGS)
{
	struct mana_port_context *apc = arg1;
	int offset = arg2, i, err;
	struct mana_txq *txq;
	uint64_t stat;

	stat = 0;
	for (i = 0; i < apc->num_queues; i++) {
		txq = &apc->tx_qp[i].txq;
		stat += *((uint64_t *)((uint8_t *)txq + offset));
	}

	err = sysctl_handle_64(oidp, &stat, 0, req);
	if (err || req->newptr == NULL)
		return err;

	for (i = 0; i < apc->num_queues; i++) {
		txq = &apc->tx_qp[i].txq;
		*((uint64_t *)((uint8_t *)txq + offset)) = 0;
	}
	return 0;
}

void
mana_sysctl_add_port(struct mana_port_context *apc)
{
	struct gdma_context *gc = apc->ac->gdma_dev->gdma_context;
	device_t dev = gc->dev;
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *tree;
	struct sysctl_oid_list *child;
	struct mana_port_stats *port_stats;
	char node_name[32];

	struct sysctl_oid *port_node, *stats_node;
	struct sysctl_oid_list *stats_list;

	ctx = device_get_sysctl_ctx(dev);
	tree = device_get_sysctl_tree(dev);
	child = SYSCTL_CHILDREN(tree);

	port_stats = &apc->port_stats;

	snprintf(node_name, 32, "port%d", apc->port_idx);

	port_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO,
	    node_name, CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "Port Name");
	apc->port_list = SYSCTL_CHILDREN(port_node);

	SYSCTL_ADD_BOOL(ctx, apc->port_list, OID_AUTO,
	    "enable_altq", CTLFLAG_RW, &apc->enable_tx_altq, 0,
	    "Choose alternative txq under heavy load");

	SYSCTL_ADD_UINT(ctx, apc->port_list, OID_AUTO,
	    "tx_queue_size", CTLFLAG_RD, &apc->tx_queue_size, 0,
	    "number of unit of tx queue");

	SYSCTL_ADD_UINT(ctx, apc->port_list, OID_AUTO,
	    "rx_queue_size", CTLFLAG_RD, &apc->rx_queue_size, 0,
	    "number of unit of rx queue");

	SYSCTL_ADD_PROC(ctx, apc->port_list, OID_AUTO,
	    "bind_cleanup_thread_cpu",
	    CTLTYPE_U8 | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    apc, 0, mana_sysctl_cleanup_thread_cpu, "I",
	    "Bind cleanup thread to a cpu. 0 disables it.");

	stats_node = SYSCTL_ADD_NODE(ctx, apc->port_list, OID_AUTO,
	    "port_stats", CTLFLAG_RD | CTLFLAG_MPSAFE, NULL,
	    "Statistics of port");
	stats_list = SYSCTL_CHILDREN(stats_node);

	SYSCTL_ADD_COUNTER_U64(ctx, stats_list, OID_AUTO, "rx_packets",
	    CTLFLAG_RD, &port_stats->rx_packets, "Packets received");
	SYSCTL_ADD_COUNTER_U64(ctx, stats_list, OID_AUTO, "tx_packets",
	    CTLFLAG_RD, &port_stats->tx_packets, "Packets transmitted");
	SYSCTL_ADD_COUNTER_U64(ctx, stats_list, OID_AUTO, "rx_bytes",
	    CTLFLAG_RD, &port_stats->rx_bytes, "Bytes received");
	SYSCTL_ADD_COUNTER_U64(ctx, stats_list, OID_AUTO, "tx_bytes",
	    CTLFLAG_RD, &port_stats->tx_bytes, "Bytes transmitted");
	SYSCTL_ADD_COUNTER_U64(ctx, stats_list, OID_AUTO, "rx_drops",
	    CTLFLAG_RD, &port_stats->rx_drops, "Receive packet drops");
	SYSCTL_ADD_COUNTER_U64(ctx, stats_list, OID_AUTO, "tx_drops",
	    CTLFLAG_RD, &port_stats->tx_drops, "Transmit packet drops");

	SYSCTL_ADD_PROC(ctx, stats_list, OID_AUTO, "rx_lro_queued",
	    CTLTYPE_U64 | CTLFLAG_RD | CTLFLAG_MPSAFE | CTLFLAG_STATS, apc,
	    __offsetof(struct mana_rxq, lro.lro_queued),
	    mana_sysctl_rx_stat_agg_u64, "LU", "LRO queued");
	SYSCTL_ADD_PROC(ctx, stats_list, OID_AUTO, "rx_lro_flushed",
	    CTLTYPE_U64 | CTLFLAG_RD | CTLFLAG_MPSAFE | CTLFLAG_STATS, apc,
	    __offsetof(struct mana_rxq, lro.lro_flushed),
	    mana_sysctl_rx_stat_agg_u64, "LU", "LRO flushed");
	SYSCTL_ADD_PROC(ctx, stats_list, OID_AUTO, "rx_lro_bad_csum",
	    CTLTYPE_U64 | CTLFLAG_RD | CTLFLAG_MPSAFE | CTLFLAG_STATS, apc,
	    __offsetof(struct mana_rxq, lro.lro_bad_csum),
	    mana_sysctl_rx_stat_agg_u64, "LU", "LRO bad checksum");
	SYSCTL_ADD_PROC(ctx, stats_list, OID_AUTO, "rx_lro_tried",
	    CTLTYPE_U64 | CTLFLAG_RD | CTLFLAG_STATS, apc,
	    __offsetof(struct mana_rxq, lro_tried),
	    mana_sysctl_rx_stat_agg_u64, "LU", "LRO tried");
	SYSCTL_ADD_PROC(ctx, stats_list, OID_AUTO, "rx_lro_failed",
	    CTLTYPE_U64 | CTLFLAG_RD | CTLFLAG_STATS, apc,
	    __offsetof(struct mana_rxq, lro_failed),
	    mana_sysctl_rx_stat_agg_u64, "LU", "LRO failed");

	SYSCTL_ADD_PROC(ctx, stats_list, OID_AUTO, "lro_ackcnt_lim",
	    CTLTYPE_U64 | CTLFLAG_RD | CTLFLAG_STATS, apc,
	    __offsetof(struct mana_rxq, lro.lro_ackcnt_lim),
	    mana_sysctl_rx_stat_u16,
	    "LU", "Max # of ACKs to be aggregated by LRO");
	SYSCTL_ADD_PROC(ctx, stats_list, OID_AUTO, "lro_length_lim",
	    CTLTYPE_U64 | CTLFLAG_RD | CTLFLAG_STATS, apc,
	    __offsetof(struct mana_rxq, lro.lro_length_lim),
	    mana_sysctl_rx_stat_u32,
	    "LU", "Max len of aggregated data in byte by LRO");
	SYSCTL_ADD_PROC(ctx, stats_list, OID_AUTO, "lro_cnt",
	    CTLTYPE_U64 | CTLFLAG_RD | CTLFLAG_STATS, apc,
	    __offsetof(struct mana_rxq, lro.lro_cnt),
	    mana_sysctl_rx_stat_u32,
	    "LU", "Max # or LRO packet count");

	SYSCTL_ADD_PROC(ctx, stats_list, OID_AUTO, "tx_tso_packets",
	    CTLTYPE_U64 | CTLFLAG_RD | CTLFLAG_STATS, apc,
	    __offsetof(struct mana_txq, tso_pkts),
	    mana_sysctl_tx_stat_agg_u64, "LU", "TSO packets");
	SYSCTL_ADD_PROC(ctx, stats_list, OID_AUTO, "tx_tso_bytes",
	    CTLTYPE_U64 | CTLFLAG_RD | CTLFLAG_STATS, apc,
	    __offsetof(struct mana_txq, tso_bytes),
	    mana_sysctl_tx_stat_agg_u64, "LU", "TSO bytes");
}

void
mana_sysctl_add_queues(struct mana_port_context *apc)
{
	struct sysctl_ctx_list *ctx = &apc->que_sysctl_ctx;
	struct sysctl_oid_list *child = apc->port_list;

	struct sysctl_oid *queue_node, *tx_node, *rx_node;
	struct sysctl_oid_list *queue_list, *tx_list, *rx_list;
	struct mana_txq *txq;
	struct mana_rxq *rxq;
	struct mana_stats *tx_stats, *rx_stats;
	char que_name[32];
	int i;

	sysctl_ctx_init(ctx);

	for (i = 0; i < apc->num_queues; i++) {
		rxq = apc->rxqs[i];
		txq = &apc->tx_qp[i].txq;

		snprintf(que_name, 32, "queue%d", i);

		queue_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO,
		    que_name, CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "Queue Name");
		queue_list = SYSCTL_CHILDREN(queue_node);

		/* TX stats */
		tx_node = SYSCTL_ADD_NODE(ctx, queue_list, OID_AUTO,
		    "txq", CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "TX queue");
		tx_list = SYSCTL_CHILDREN(tx_node);

		tx_stats = &txq->stats;

		SYSCTL_ADD_COUNTER_U64(ctx, tx_list, OID_AUTO, "count",
		    CTLFLAG_RD, &tx_stats->packets, "Packets sent");
		SYSCTL_ADD_COUNTER_U64(ctx, tx_list, OID_AUTO, "bytes",
		    CTLFLAG_RD, &tx_stats->bytes, "Bytes sent");
		SYSCTL_ADD_COUNTER_U64(ctx, tx_list, OID_AUTO, "queue_wakeups",
		    CTLFLAG_RD, &tx_stats->wakeup, "Queue wakeups");
		SYSCTL_ADD_COUNTER_U64(ctx, tx_list, OID_AUTO, "queue_stops",
		    CTLFLAG_RD, &tx_stats->stop, "Queue stops");
		SYSCTL_ADD_COUNTER_U64(ctx, tx_list, OID_AUTO, "mbuf_collapse",
		    CTLFLAG_RD, &tx_stats->collapse, "Mbuf collapse count");
		SYSCTL_ADD_COUNTER_U64(ctx, tx_list, OID_AUTO,
		    "mbuf_collapse_err", CTLFLAG_RD,
		    &tx_stats->collapse_err, "Mbuf collapse failures");
		SYSCTL_ADD_COUNTER_U64(ctx, tx_list, OID_AUTO,
		    "dma_mapping_err", CTLFLAG_RD,
		    &tx_stats->dma_mapping_err, "DMA mapping failures");
		SYSCTL_ADD_COUNTER_U64(ctx, tx_list, OID_AUTO,
		    "alt_chg", CTLFLAG_RD,
		    &tx_stats->alt_chg, "Switch to alternative txq");
		SYSCTL_ADD_COUNTER_U64(ctx, tx_list, OID_AUTO,
		    "alt_reset", CTLFLAG_RD,
		    &tx_stats->alt_reset, "Reset to self txq");
		SYSCTL_ADD_COUNTER_U64(ctx, tx_list, OID_AUTO,
		    "cqe_err", CTLFLAG_RD,
		    &tx_stats->cqe_err, "Error CQE count");
		SYSCTL_ADD_COUNTER_U64(ctx, tx_list, OID_AUTO,
		    "cqe_unknown_type", CTLFLAG_RD,
		    &tx_stats->cqe_unknown_type, "Unknown CQE count");

		/* RX stats */
		rx_node = SYSCTL_ADD_NODE(ctx, queue_list, OID_AUTO,
		    "rxq", CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "RX queue");
		rx_list = SYSCTL_CHILDREN(rx_node);

		rx_stats = &rxq->stats;

		SYSCTL_ADD_COUNTER_U64(ctx, rx_list, OID_AUTO, "count",
		    CTLFLAG_RD, &rx_stats->packets, "Packets received");
		SYSCTL_ADD_COUNTER_U64(ctx, rx_list, OID_AUTO, "bytes",
		    CTLFLAG_RD, &rx_stats->bytes, "Bytes received");
		SYSCTL_ADD_COUNTER_U64(ctx, rx_list, OID_AUTO,
		    "mbuf_alloc_fail", CTLFLAG_RD,
		    &rx_stats->mbuf_alloc_fail, "Failed mbuf allocs");
		SYSCTL_ADD_COUNTER_U64(ctx, rx_list, OID_AUTO,
		    "partial_refill", CTLFLAG_RD,
		    &rx_stats->partial_refill, "Partially refilled mbuf");
		SYSCTL_ADD_COUNTER_U64(ctx, rx_list, OID_AUTO,
		    "dma_mapping_err", CTLFLAG_RD,
		    &rx_stats->dma_mapping_err, "DMA mapping errors");
	}
}

/*
 * Free all queues' sysctl trees attached to the port's tree.
 */
void
mana_sysctl_free_queues(struct mana_port_context *apc)
{
	sysctl_ctx_free(&apc->que_sysctl_ctx);
}

static int
mana_sysctl_cleanup_thread_cpu(SYSCTL_HANDLER_ARGS)
{
	struct mana_port_context *apc = arg1;
	bool bind_cpu = false;
	uint8_t val;
	int err;

	val = 0;
	err = sysctl_wire_old_buffer(req, sizeof(val));
	if (err == 0) {
		val = apc->bind_cleanup_thread_cpu;
		err = sysctl_handle_8(oidp, &val, 0, req);
	}

	if (err != 0 || req->newptr == NULL)
		return (err);

	if (val != 0)
		bind_cpu = true;

	if (bind_cpu != apc->bind_cleanup_thread_cpu) {
		apc->bind_cleanup_thread_cpu = bind_cpu;
		err = mana_restart(apc);
	}

	return (err);
}
