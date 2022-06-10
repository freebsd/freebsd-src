/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2015-2021 Amazon.com, Inc. or its affiliates.
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
#include <sys/param.h>
__FBSDID("$FreeBSD$");

#include "opt_rss.h"

#include "ena_rss.h"
#include "ena_sysctl.h"

static void ena_sysctl_add_wd(struct ena_adapter *);
static void ena_sysctl_add_stats(struct ena_adapter *);
static void ena_sysctl_add_eni_metrics(struct ena_adapter *);
static void ena_sysctl_add_tuneables(struct ena_adapter *);
/* Kernel option RSS prevents manipulation of key hash and indirection table. */
#ifndef RSS
static void ena_sysctl_add_rss(struct ena_adapter *);
#endif
static int ena_sysctl_buf_ring_size(SYSCTL_HANDLER_ARGS);
static int ena_sysctl_rx_queue_size(SYSCTL_HANDLER_ARGS);
static int ena_sysctl_io_queues_nb(SYSCTL_HANDLER_ARGS);
static int ena_sysctl_eni_metrics_interval(SYSCTL_HANDLER_ARGS);
#ifndef RSS
static int ena_sysctl_rss_key(SYSCTL_HANDLER_ARGS);
static int ena_sysctl_rss_indir_table(SYSCTL_HANDLER_ARGS);
#endif

/* Limit max ENI sample rate to be an hour. */
#define ENI_METRICS_MAX_SAMPLE_INTERVAL 3600
#define ENA_HASH_KEY_MSG_SIZE (ENA_HASH_KEY_SIZE * 2 + 1)

static SYSCTL_NODE(_hw, OID_AUTO, ena, CTLFLAG_RD, 0, "ENA driver parameters");

/*
 * Logging level for changing verbosity of the output
 */
int ena_log_level = ENA_INFO;
SYSCTL_INT(_hw_ena, OID_AUTO, log_level, CTLFLAG_RWTUN, &ena_log_level, 0,
    "Logging level indicating verbosity of the logs");

SYSCTL_CONST_STRING(_hw_ena, OID_AUTO, driver_version, CTLFLAG_RD,
    DRV_MODULE_VERSION, "ENA driver version");

/*
 * Use 9k mbufs for the Rx buffers. Default to 0 (use page size mbufs instead).
 * Using 9k mbufs in low memory conditions might cause allocation to take a lot
 * of time and lead to the OS instability as it needs to look for the contiguous
 * pages.
 * However, page size mbufs has a bit smaller throughput than 9k mbufs, so if
 * the network performance is the priority, the 9k mbufs can be used.
 */
int ena_enable_9k_mbufs = 0;
SYSCTL_INT(_hw_ena, OID_AUTO, enable_9k_mbufs, CTLFLAG_RDTUN,
    &ena_enable_9k_mbufs, 0, "Use 9 kB mbufs for Rx descriptors");

/*
 * Force the driver to use large LLQ (Low Latency Queue) header. Defaults to
 * false. This option may be important for platforms, which often handle packet
 * headers on Tx with total header size greater than 96B, as it may
 * reduce the latency.
 * It also reduces the maximum Tx queue size by half, so it may cause more Tx
 * packet drops.
 */
bool ena_force_large_llq_header = false;
SYSCTL_BOOL(_hw_ena, OID_AUTO, force_large_llq_header, CTLFLAG_RDTUN,
    &ena_force_large_llq_header, 0,
    "Increases maximum supported header size in LLQ mode to 224 bytes, while reducing the maximum Tx queue size by half.\n");

int ena_rss_table_size = ENA_RX_RSS_TABLE_SIZE;

void
ena_sysctl_add_nodes(struct ena_adapter *adapter)
{
	ena_sysctl_add_wd(adapter);
	ena_sysctl_add_stats(adapter);
	ena_sysctl_add_eni_metrics(adapter);
	ena_sysctl_add_tuneables(adapter);
#ifndef RSS
	ena_sysctl_add_rss(adapter);
#endif
}

static void
ena_sysctl_add_wd(struct ena_adapter *adapter)
{
	device_t dev;

	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *tree;
	struct sysctl_oid_list *child;

	dev = adapter->pdev;

	ctx = device_get_sysctl_ctx(dev);
	tree = device_get_sysctl_tree(dev);
	child = SYSCTL_CHILDREN(tree);

	/* Sysctl calls for Watchdog service */
	SYSCTL_ADD_INT(ctx, child, OID_AUTO, "wd_active", CTLFLAG_RWTUN,
	    &adapter->wd_active, 0, "Watchdog is active");

	SYSCTL_ADD_QUAD(ctx, child, OID_AUTO, "keep_alive_timeout",
	    CTLFLAG_RWTUN, &adapter->keep_alive_timeout,
	    "Timeout for Keep Alive messages");

	SYSCTL_ADD_QUAD(ctx, child, OID_AUTO, "missing_tx_timeout",
	    CTLFLAG_RWTUN, &adapter->missing_tx_timeout,
	    "Timeout for TX completion");

	SYSCTL_ADD_U32(ctx, child, OID_AUTO, "missing_tx_max_queues",
	    CTLFLAG_RWTUN, &adapter->missing_tx_max_queues, 0,
	    "Number of TX queues to check per run");

	SYSCTL_ADD_U32(ctx, child, OID_AUTO, "missing_tx_threshold",
	    CTLFLAG_RWTUN, &adapter->missing_tx_threshold, 0,
	    "Max number of timeouted packets");
}

static void
ena_sysctl_add_stats(struct ena_adapter *adapter)
{
	device_t dev;

	struct ena_ring *tx_ring;
	struct ena_ring *rx_ring;

	struct ena_hw_stats *hw_stats;
	struct ena_stats_dev *dev_stats;
	struct ena_stats_tx *tx_stats;
	struct ena_stats_rx *rx_stats;
	struct ena_com_stats_admin *admin_stats;

	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *tree;
	struct sysctl_oid_list *child;

	struct sysctl_oid *queue_node, *tx_node, *rx_node, *hw_node;
	struct sysctl_oid *admin_node;
	struct sysctl_oid_list *queue_list, *tx_list, *rx_list, *hw_list;
	struct sysctl_oid_list *admin_list;

#define QUEUE_NAME_LEN 32
	char namebuf[QUEUE_NAME_LEN];
	int i;

	dev = adapter->pdev;

	ctx = device_get_sysctl_ctx(dev);
	tree = device_get_sysctl_tree(dev);
	child = SYSCTL_CHILDREN(tree);

	tx_ring = adapter->tx_ring;
	rx_ring = adapter->rx_ring;

	hw_stats = &adapter->hw_stats;
	dev_stats = &adapter->dev_stats;
	admin_stats = &adapter->ena_dev->admin_queue.stats;

	SYSCTL_ADD_COUNTER_U64(ctx, child, OID_AUTO, "wd_expired", CTLFLAG_RD,
	    &dev_stats->wd_expired, "Watchdog expiry count");
	SYSCTL_ADD_COUNTER_U64(ctx, child, OID_AUTO, "interface_up", CTLFLAG_RD,
	    &dev_stats->interface_up, "Network interface up count");
	SYSCTL_ADD_COUNTER_U64(ctx, child, OID_AUTO, "interface_down",
	    CTLFLAG_RD, &dev_stats->interface_down,
	    "Network interface down count");
	SYSCTL_ADD_COUNTER_U64(ctx, child, OID_AUTO, "admin_q_pause",
	    CTLFLAG_RD, &dev_stats->admin_q_pause, "Admin queue pauses");

	for (i = 0; i < adapter->num_io_queues; ++i, ++tx_ring, ++rx_ring) {
		snprintf(namebuf, QUEUE_NAME_LEN, "queue%d", i);

		queue_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, namebuf,
		    CTLFLAG_RD, NULL, "Queue Name");
		queue_list = SYSCTL_CHILDREN(queue_node);

		adapter->que[i].oid = queue_node;

#ifdef RSS
		/* Common stats */
		SYSCTL_ADD_INT(ctx, queue_list, OID_AUTO, "cpu", CTLFLAG_RD,
		    &adapter->que[i].cpu, 0, "CPU affinity");
		SYSCTL_ADD_INT(ctx, queue_list, OID_AUTO, "domain", CTLFLAG_RD,
		    &adapter->que[i].domain, 0, "NUMA domain");
#endif

		/* TX specific stats */
		tx_node = SYSCTL_ADD_NODE(ctx, queue_list, OID_AUTO, "tx_ring",
		    CTLFLAG_RD, NULL, "TX ring");
		tx_list = SYSCTL_CHILDREN(tx_node);

		tx_stats = &tx_ring->tx_stats;

		SYSCTL_ADD_COUNTER_U64(ctx, tx_list, OID_AUTO, "count",
		    CTLFLAG_RD, &tx_stats->cnt, "Packets sent");
		SYSCTL_ADD_COUNTER_U64(ctx, tx_list, OID_AUTO, "bytes",
		    CTLFLAG_RD, &tx_stats->bytes, "Bytes sent");
		SYSCTL_ADD_COUNTER_U64(ctx, tx_list, OID_AUTO,
		    "prepare_ctx_err", CTLFLAG_RD, &tx_stats->prepare_ctx_err,
		    "TX buffer preparation failures");
		SYSCTL_ADD_COUNTER_U64(ctx, tx_list, OID_AUTO,
		    "dma_mapping_err", CTLFLAG_RD, &tx_stats->dma_mapping_err,
		    "DMA mapping failures");
		SYSCTL_ADD_COUNTER_U64(ctx, tx_list, OID_AUTO, "doorbells",
		    CTLFLAG_RD, &tx_stats->doorbells, "Queue doorbells");
		SYSCTL_ADD_COUNTER_U64(ctx, tx_list, OID_AUTO,
		    "missing_tx_comp", CTLFLAG_RD, &tx_stats->missing_tx_comp,
		    "TX completions missed");
		SYSCTL_ADD_COUNTER_U64(ctx, tx_list, OID_AUTO, "bad_req_id",
		    CTLFLAG_RD, &tx_stats->bad_req_id, "Bad request id count");
		SYSCTL_ADD_COUNTER_U64(ctx, tx_list, OID_AUTO, "mbuf_collapses",
		    CTLFLAG_RD, &tx_stats->collapse, "Mbuf collapse count");
		SYSCTL_ADD_COUNTER_U64(ctx, tx_list, OID_AUTO,
		    "mbuf_collapse_err", CTLFLAG_RD, &tx_stats->collapse_err,
		    "Mbuf collapse failures");
		SYSCTL_ADD_COUNTER_U64(ctx, tx_list, OID_AUTO, "queue_wakeups",
		    CTLFLAG_RD, &tx_stats->queue_wakeup, "Queue wakeups");
		SYSCTL_ADD_COUNTER_U64(ctx, tx_list, OID_AUTO, "queue_stops",
		    CTLFLAG_RD, &tx_stats->queue_stop, "Queue stops");
		SYSCTL_ADD_COUNTER_U64(ctx, tx_list, OID_AUTO,
		    "llq_buffer_copy", CTLFLAG_RD, &tx_stats->llq_buffer_copy,
		    "Header copies for llq transaction");
		SYSCTL_ADD_COUNTER_U64(ctx, tx_list, OID_AUTO,
		    "unmask_interrupt_num", CTLFLAG_RD,
		    &tx_stats->unmask_interrupt_num,
		    "Unmasked interrupt count");

		/* RX specific stats */
		rx_node = SYSCTL_ADD_NODE(ctx, queue_list, OID_AUTO, "rx_ring",
		    CTLFLAG_RD, NULL, "RX ring");
		rx_list = SYSCTL_CHILDREN(rx_node);

		rx_stats = &rx_ring->rx_stats;

		SYSCTL_ADD_COUNTER_U64(ctx, rx_list, OID_AUTO, "count",
		    CTLFLAG_RD, &rx_stats->cnt, "Packets received");
		SYSCTL_ADD_COUNTER_U64(ctx, rx_list, OID_AUTO, "bytes",
		    CTLFLAG_RD, &rx_stats->bytes, "Bytes received");
		SYSCTL_ADD_COUNTER_U64(ctx, rx_list, OID_AUTO, "refil_partial",
		    CTLFLAG_RD, &rx_stats->refil_partial,
		    "Partial refilled mbufs");
		SYSCTL_ADD_COUNTER_U64(ctx, rx_list, OID_AUTO, "csum_bad",
		    CTLFLAG_RD, &rx_stats->csum_bad, "Bad RX checksum");
		SYSCTL_ADD_COUNTER_U64(ctx, rx_list, OID_AUTO,
		    "mbuf_alloc_fail", CTLFLAG_RD, &rx_stats->mbuf_alloc_fail,
		    "Failed mbuf allocs");
		SYSCTL_ADD_COUNTER_U64(ctx, rx_list, OID_AUTO,
		    "mjum_alloc_fail", CTLFLAG_RD, &rx_stats->mjum_alloc_fail,
		    "Failed jumbo mbuf allocs");
		SYSCTL_ADD_COUNTER_U64(ctx, rx_list, OID_AUTO,
		    "dma_mapping_err", CTLFLAG_RD, &rx_stats->dma_mapping_err,
		    "DMA mapping errors");
		SYSCTL_ADD_COUNTER_U64(ctx, rx_list, OID_AUTO, "bad_desc_num",
		    CTLFLAG_RD, &rx_stats->bad_desc_num,
		    "Bad descriptor count");
		SYSCTL_ADD_COUNTER_U64(ctx, rx_list, OID_AUTO, "bad_req_id",
		    CTLFLAG_RD, &rx_stats->bad_req_id, "Bad request id count");
		SYSCTL_ADD_COUNTER_U64(ctx, rx_list, OID_AUTO, "empty_rx_ring",
		    CTLFLAG_RD, &rx_stats->empty_rx_ring,
		    "RX descriptors depletion count");
		SYSCTL_ADD_COUNTER_U64(ctx, rx_list, OID_AUTO, "csum_good",
		    CTLFLAG_RD, &rx_stats->csum_good,
		    "Valid RX checksum calculations");
	}

	/* Stats read from device */
	hw_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, "hw_stats",
	    CTLFLAG_RD, NULL, "Statistics from hardware");
	hw_list = SYSCTL_CHILDREN(hw_node);

	SYSCTL_ADD_COUNTER_U64(ctx, hw_list, OID_AUTO, "rx_packets", CTLFLAG_RD,
	    &hw_stats->rx_packets, "Packets received");
	SYSCTL_ADD_COUNTER_U64(ctx, hw_list, OID_AUTO, "tx_packets", CTLFLAG_RD,
	    &hw_stats->tx_packets, "Packets transmitted");
	SYSCTL_ADD_COUNTER_U64(ctx, hw_list, OID_AUTO, "rx_bytes", CTLFLAG_RD,
	    &hw_stats->rx_bytes, "Bytes received");
	SYSCTL_ADD_COUNTER_U64(ctx, hw_list, OID_AUTO, "tx_bytes", CTLFLAG_RD,
	    &hw_stats->tx_bytes, "Bytes transmitted");
	SYSCTL_ADD_COUNTER_U64(ctx, hw_list, OID_AUTO, "rx_drops", CTLFLAG_RD,
	    &hw_stats->rx_drops, "Receive packet drops");
	SYSCTL_ADD_COUNTER_U64(ctx, hw_list, OID_AUTO, "tx_drops", CTLFLAG_RD,
	    &hw_stats->tx_drops, "Transmit packet drops");

	/* ENA Admin queue stats */
	admin_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, "admin_stats",
	    CTLFLAG_RD, NULL, "ENA Admin Queue statistics");
	admin_list = SYSCTL_CHILDREN(admin_node);

	SYSCTL_ADD_U64(ctx, admin_list, OID_AUTO, "aborted_cmd", CTLFLAG_RD,
	    &admin_stats->aborted_cmd, 0, "Aborted commands");
	SYSCTL_ADD_U64(ctx, admin_list, OID_AUTO, "sumbitted_cmd", CTLFLAG_RD,
	    &admin_stats->submitted_cmd, 0, "Submitted commands");
	SYSCTL_ADD_U64(ctx, admin_list, OID_AUTO, "completed_cmd", CTLFLAG_RD,
	    &admin_stats->completed_cmd, 0, "Completed commands");
	SYSCTL_ADD_U64(ctx, admin_list, OID_AUTO, "out_of_space", CTLFLAG_RD,
	    &admin_stats->out_of_space, 0, "Queue out of space");
	SYSCTL_ADD_U64(ctx, admin_list, OID_AUTO, "no_completion", CTLFLAG_RD,
	    &admin_stats->no_completion, 0, "Commands not completed");
}

static void
ena_sysctl_add_eni_metrics(struct ena_adapter *adapter)
{
	device_t dev;
	struct ena_admin_eni_stats *eni_metrics;

	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *tree;
	struct sysctl_oid_list *child;

	struct sysctl_oid *eni_node;
	struct sysctl_oid_list *eni_list;

	dev = adapter->pdev;

	ctx = device_get_sysctl_ctx(dev);
	tree = device_get_sysctl_tree(dev);
	child = SYSCTL_CHILDREN(tree);

	eni_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, "eni_metrics",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "ENA's ENI metrics");
	eni_list = SYSCTL_CHILDREN(eni_node);

	eni_metrics = &adapter->eni_metrics;

	SYSCTL_ADD_U64(ctx, eni_list, OID_AUTO, "bw_in_allowance_exceeded",
	    CTLFLAG_RD, &eni_metrics->bw_in_allowance_exceeded, 0,
	    "Inbound BW allowance exceeded");
	SYSCTL_ADD_U64(ctx, eni_list, OID_AUTO, "bw_out_allowance_exceeded",
	    CTLFLAG_RD, &eni_metrics->bw_out_allowance_exceeded, 0,
	    "Outbound BW allowance exceeded");
	SYSCTL_ADD_U64(ctx, eni_list, OID_AUTO, "pps_allowance_exceeded",
	    CTLFLAG_RD, &eni_metrics->pps_allowance_exceeded, 0,
	    "PPS allowance exceeded");
	SYSCTL_ADD_U64(ctx, eni_list, OID_AUTO, "conntrack_allowance_exceeded",
	    CTLFLAG_RD, &eni_metrics->conntrack_allowance_exceeded, 0,
	    "Connection tracking allowance exceeded");
	SYSCTL_ADD_U64(ctx, eni_list, OID_AUTO, "linklocal_allowance_exceeded",
	    CTLFLAG_RD, &eni_metrics->linklocal_allowance_exceeded, 0,
	    "Linklocal packet rate allowance exceeded");

	/*
	 * Tuneable, which determines how often ENI metrics will be read.
	 * 0 means it's turned off. Maximum allowed value is limited by:
	 * ENI_METRICS_MAX_SAMPLE_INTERVAL.
	 */
	SYSCTL_ADD_PROC(ctx, eni_list, OID_AUTO, "sample_interval",
	    CTLTYPE_U16 | CTLFLAG_RW | CTLFLAG_MPSAFE, adapter, 0,
	    ena_sysctl_eni_metrics_interval, "SU",
	    "Interval in seconds for updating ENI emetrics. 0 turns off the update.");
}

static void
ena_sysctl_add_tuneables(struct ena_adapter *adapter)
{
	device_t dev;

	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *tree;
	struct sysctl_oid_list *child;

	dev = adapter->pdev;

	ctx = device_get_sysctl_ctx(dev);
	tree = device_get_sysctl_tree(dev);
	child = SYSCTL_CHILDREN(tree);

	/* Tuneable number of buffers in the buf-ring (drbr) */
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "buf_ring_size",
	    CTLTYPE_U32 | CTLFLAG_RW | CTLFLAG_MPSAFE, adapter, 0,
	    ena_sysctl_buf_ring_size, "I",
	    "Size of the Tx buffer ring (drbr).");

	/* Tuneable number of the Rx ring size */
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "rx_queue_size",
	    CTLTYPE_U32 | CTLFLAG_RW | CTLFLAG_MPSAFE, adapter, 0,
	    ena_sysctl_rx_queue_size, "I",
	    "Size of the Rx ring. The size should be a power of 2.");

	/* Tuneable number of IO queues */
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "io_queues_nb",
	    CTLTYPE_U32 | CTLFLAG_RW | CTLFLAG_MPSAFE, adapter, 0,
	    ena_sysctl_io_queues_nb, "I", "Number of IO queues.");
}

/* Kernel option RSS prevents manipulation of key hash and indirection table. */
#ifndef RSS
static void
ena_sysctl_add_rss(struct ena_adapter *adapter)
{
	device_t dev;

	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *tree;
	struct sysctl_oid_list *child;

	dev = adapter->pdev;

	ctx = device_get_sysctl_ctx(dev);
	tree = device_get_sysctl_tree(dev);
	child = SYSCTL_CHILDREN(tree);

	/* RSS options */
	tree = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, "rss",
	    CTLFLAG_RW | CTLFLAG_MPSAFE, NULL, "Receive Side Scaling options.");
	child = SYSCTL_CHILDREN(tree);

	/* RSS hash key */
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "key",
	    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_MPSAFE, adapter, 0,
	    ena_sysctl_rss_key, "A", "RSS key.");

	/* Tuneable RSS indirection table */
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "indir_table",
	    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_MPSAFE, adapter, 0,
	    ena_sysctl_rss_indir_table, "A", "RSS indirection table.");

	/* RSS indirection table size */
	SYSCTL_ADD_INT(ctx, child, OID_AUTO, "indir_table_size",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, &ena_rss_table_size, 0,
	    "RSS indirection table size.");
}
#endif /* RSS */


/*
 * ena_sysctl_update_queue_node_nb - Register/unregister sysctl queue nodes.
 *
 * Whether the nodes are registered or unregistered depends on a delta between
 * the `old` and `new` parameters, representing the number of queues.
 *
 * This function is used to hide sysctl attributes for queue nodes which aren't
 * currently used by the HW (e.g. after a call to `ena_sysctl_io_queues_nb`).
 *
 * NOTE:
 * All unregistered nodes must be registered again at detach, i.e. by a call to
 * this function.
 */
void
ena_sysctl_update_queue_node_nb(struct ena_adapter *adapter, int old, int new)
{
	device_t dev;
	struct sysctl_oid *oid;
	int min, max, i;

	dev = adapter->pdev;
	min = MIN(old, new);
	max = MIN(MAX(old, new), adapter->max_num_io_queues);

	for (i = min; i < max; ++i) {
		oid = adapter->que[i].oid;

		sysctl_wlock();
		if (old > new)
			sysctl_unregister_oid(oid);
		else
			sysctl_register_oid(oid);
		sysctl_wunlock();
	}
}

static int
ena_sysctl_buf_ring_size(SYSCTL_HANDLER_ARGS)
{
	struct ena_adapter *adapter = arg1;
	uint32_t val;
	int error;

	ENA_LOCK_LOCK();
	if (unlikely(!ENA_FLAG_ISSET(ENA_FLAG_DEVICE_RUNNING, adapter))) {
		error = EINVAL;
		goto unlock;
	}

	val = 0;
	error = sysctl_wire_old_buffer(req, sizeof(val));
	if (error == 0) {
		val = adapter->buf_ring_size;
		error = sysctl_handle_32(oidp, &val, 0, req);
	}
	if (error != 0 || req->newptr == NULL)
		goto unlock;

	if (!powerof2(val) || val == 0) {
		ena_log(adapter->pdev, ERR,
		    "Requested new Tx buffer ring size (%u) is not a power of 2\n",
		    val);
		error = EINVAL;
		goto unlock;
	}

	if (val != adapter->buf_ring_size) {
		ena_log(adapter->pdev, INFO,
		    "Requested new Tx buffer ring size: %d. Old size: %d\n",
		    val, adapter->buf_ring_size);

		error = ena_update_buf_ring_size(adapter, val);
	} else {
		ena_log(adapter->pdev, ERR,
		    "New Tx buffer ring size is the same as already used: %u\n",
		    adapter->buf_ring_size);
	}

unlock:
	ENA_LOCK_UNLOCK();

	return (error);
}

static int
ena_sysctl_rx_queue_size(SYSCTL_HANDLER_ARGS)
{
	struct ena_adapter *adapter = arg1;
	uint32_t val;
	int error;

	ENA_LOCK_LOCK();
	if (unlikely(!ENA_FLAG_ISSET(ENA_FLAG_DEVICE_RUNNING, adapter))) {
		error = EINVAL;
		goto unlock;
	}

	val = 0;
	error = sysctl_wire_old_buffer(req, sizeof(val));
	if (error == 0) {
		val = adapter->requested_rx_ring_size;
		error = sysctl_handle_32(oidp, &val, 0, req);
	}
	if (error != 0 || req->newptr == NULL)
		goto unlock;

	if (val < ENA_MIN_RING_SIZE || val > adapter->max_rx_ring_size) {
		ena_log(adapter->pdev, ERR,
		    "Requested new Rx queue size (%u) is out of range: [%u, %u]\n",
		    val, ENA_MIN_RING_SIZE, adapter->max_rx_ring_size);
		error = EINVAL;
		goto unlock;
	}

	/* Check if the parameter is power of 2 */
	if (!powerof2(val)) {
		ena_log(adapter->pdev, ERR,
		    "Requested new Rx queue size (%u) is not a power of 2\n",
		    val);
		error = EINVAL;
		goto unlock;
	}

	if (val != adapter->requested_rx_ring_size) {
		ena_log(adapter->pdev, INFO,
		    "Requested new Rx queue size: %u. Old size: %u\n", val,
		    adapter->requested_rx_ring_size);

		error = ena_update_queue_size(adapter,
		    adapter->requested_tx_ring_size, val);
	} else {
		ena_log(adapter->pdev, ERR,
		    "New Rx queue size is the same as already used: %u\n",
		    adapter->requested_rx_ring_size);
	}

unlock:
	ENA_LOCK_UNLOCK();

	return (error);
}

/*
 * Change number of effectively used IO queues adapter->num_io_queues
 */
static int
ena_sysctl_io_queues_nb(SYSCTL_HANDLER_ARGS)
{
	struct ena_adapter *adapter = arg1;
	uint32_t old_num_queues, tmp = 0;
	int error;

	ENA_LOCK_LOCK();
	if (unlikely(!ENA_FLAG_ISSET(ENA_FLAG_DEVICE_RUNNING, adapter))) {
		error = EINVAL;
		goto unlock;
	}

	error = sysctl_wire_old_buffer(req, sizeof(tmp));
	if (error == 0) {
		tmp = adapter->num_io_queues;
		error = sysctl_handle_int(oidp, &tmp, 0, req);
	}
	if (error != 0 || req->newptr == NULL)
		goto unlock;

	if (tmp == 0) {
		ena_log(adapter->pdev, ERR,
		    "Requested number of IO queues is zero\n");
		error = EINVAL;
		goto unlock;
	}

	/*
	 * The adapter::max_num_io_queues is the HW capability. The system
	 * resources availability may potentially be a tighter limit. Therefore
	 * the relation `adapter::max_num_io_queues >= adapter::msix_vecs`
	 * always holds true, while the `adapter::msix_vecs` is variable across
	 * device reset (`ena_destroy_device()` + `ena_restore_device()`).
	 */
	if (tmp > (adapter->msix_vecs - ENA_ADMIN_MSIX_VEC)) {
		ena_log(adapter->pdev, ERR,
		    "Requested number of IO queues is higher than maximum allowed (%u)\n",
		    adapter->msix_vecs - ENA_ADMIN_MSIX_VEC);
		error = EINVAL;
		goto unlock;
	}
	if (tmp == adapter->num_io_queues) {
		ena_log(adapter->pdev, ERR,
		    "Requested number of IO queues is equal to current value "
		    "(%u)\n",
		    adapter->num_io_queues);
	} else {
		ena_log(adapter->pdev, INFO,
		    "Requested new number of IO queues: %u, current value: "
		    "%u\n",
		    tmp, adapter->num_io_queues);

		old_num_queues = adapter->num_io_queues;
		error = ena_update_io_queue_nb(adapter, tmp);
		if (error != 0)
			return (error);

		ena_sysctl_update_queue_node_nb(adapter, old_num_queues, tmp);
	}

unlock:
	ENA_LOCK_UNLOCK();

	return (error);
}

static int
ena_sysctl_eni_metrics_interval(SYSCTL_HANDLER_ARGS)
{
	struct ena_adapter *adapter = arg1;
	uint16_t interval;
	int error;

	ENA_LOCK_LOCK();
	if (unlikely(!ENA_FLAG_ISSET(ENA_FLAG_DEVICE_RUNNING, adapter))) {
		error = EINVAL;
		goto unlock;
	}

	error = sysctl_wire_old_buffer(req, sizeof(interval));
	if (error == 0) {
		interval = adapter->eni_metrics_sample_interval;
		error = sysctl_handle_16(oidp, &interval, 0, req);
	}
	if (error != 0 || req->newptr == NULL)
		goto unlock;

	if (interval > ENI_METRICS_MAX_SAMPLE_INTERVAL) {
		ena_log(adapter->pdev, ERR,
		    "ENI metrics update interval is out of range - maximum allowed value: %d seconds\n",
		    ENI_METRICS_MAX_SAMPLE_INTERVAL);
		error = EINVAL;
		goto unlock;
	}

	if (interval == 0) {
		ena_log(adapter->pdev, INFO,
		    "ENI metrics update is now turned off\n");
		bzero(&adapter->eni_metrics, sizeof(adapter->eni_metrics));
	} else {
		ena_log(adapter->pdev, INFO,
		    "ENI metrics update interval is set to: %" PRIu16
		    " seconds\n",
		    interval);
	}

	adapter->eni_metrics_sample_interval = interval;

unlock:
	ENA_LOCK_UNLOCK();

	return (0);
}

#ifndef RSS
/*
 * Change the Receive Side Scaling hash key.
 */
static int
ena_sysctl_rss_key(SYSCTL_HANDLER_ARGS)
{
	struct ena_adapter *adapter = arg1;
	struct ena_com_dev *ena_dev = adapter->ena_dev;
	enum ena_admin_hash_functions ena_func;
	char msg[ENA_HASH_KEY_MSG_SIZE];
	char elem[3] = { 0 };
	char *endp;
	u8 rss_key[ENA_HASH_KEY_SIZE];
	int error, i;

	ENA_LOCK_LOCK();
	if (unlikely(!ENA_FLAG_ISSET(ENA_FLAG_DEVICE_RUNNING, adapter))) {
		error = EINVAL;
		goto unlock;
	}

	if (unlikely(!ENA_FLAG_ISSET(ENA_FLAG_RSS_ACTIVE, adapter))) {
		error = ENOTSUP;
		goto unlock;
	}

	error = sysctl_wire_old_buffer(req, sizeof(msg));
	if (error != 0)
		goto unlock;

	error = ena_com_get_hash_function(adapter->ena_dev, &ena_func);
	if (error != 0) {
		device_printf(adapter->pdev, "Cannot get hash function\n");
		goto unlock;
	}

	if (ena_func != ENA_ADMIN_TOEPLITZ) {
		error = EINVAL;
		device_printf(adapter->pdev, "Unsupported hash algorithm\n");
		goto unlock;
	}

	error = ena_rss_get_hash_key(ena_dev, rss_key);
	if (error != 0) {
		device_printf(adapter->pdev, "Cannot get hash key\n");
		goto unlock;
	}

	for (i = 0; i < ENA_HASH_KEY_SIZE; ++i)
		snprintf(&msg[i * 2], 3, "%02x", rss_key[i]);

	error = sysctl_handle_string(oidp, msg, sizeof(msg), req);
	if (error != 0 || req->newptr == NULL)
		goto unlock;

	if (strlen(msg) != sizeof(msg) - 1) {
		error = EINVAL;
		device_printf(adapter->pdev, "Invalid key size\n");
		goto unlock;
	}

	for (i = 0; i < ENA_HASH_KEY_SIZE; ++i) {
		strncpy(elem, &msg[i * 2], 2);
		rss_key[i] = strtol(elem, &endp, 16);

		/* Both hex nibbles in the string must be valid to continue. */
		if (endp == elem || *endp != '\0' || rss_key[i] < 0) {
			error = EINVAL;
			device_printf(adapter->pdev,
			    "Invalid key hex value: '%c'\n", *endp);
			goto unlock;
		}
	}

	error = ena_rss_set_hash(ena_dev, rss_key);
	if (error != 0)
		device_printf(adapter->pdev, "Cannot fill hash key\n");

unlock:
	ENA_LOCK_UNLOCK();

	return (error);
}

/*
 * Change the Receive Side Scaling indirection table.
 *
 * The sysctl entry string consists of one or more `x:y` keypairs, where
 * x stands for the table index and y for its new value.
 * Table indices that don't need to be updated can be omitted from the string
 * and will retain their existing values. If an index is entered more than once,
 * the last value is used.
 *
 * Example:
 * To update two selected indices in the RSS indirection table, e.g. setting
 * index 0 to queue 5 and then index 5 to queue 0, the below command should be
 * used:
 *   sysctl dev.ena.0.rss.indir_table="0:5 5:0"
 */
static int
ena_sysctl_rss_indir_table(SYSCTL_HANDLER_ARGS)
{
	int num_queues, error;
	struct ena_adapter *adapter = arg1;
	struct ena_com_dev *ena_dev;
	struct ena_indir *indir;
	char *msg, *buf, *endp;
	uint32_t idx, value;

	ENA_LOCK_LOCK();
	if (unlikely(!ENA_FLAG_ISSET(ENA_FLAG_DEVICE_RUNNING, adapter))) {
		error = EINVAL;
		goto unlock;
	}

	if (unlikely(!ENA_FLAG_ISSET(ENA_FLAG_RSS_ACTIVE, adapter))) {
		error = ENOTSUP;
		goto unlock;
	}

	ena_dev = adapter->ena_dev;
	indir = adapter->rss_indir;
	msg = indir->sysctl_buf;

	if (unlikely(indir == NULL)) {
		error = ENOTSUP;
		goto unlock;
	}

	error = sysctl_handle_string(oidp, msg, sizeof(indir->sysctl_buf), req);
	if (error != 0 || req->newptr == NULL)
		goto unlock;

	num_queues = adapter->num_io_queues;

	/*
	 * This sysctl expects msg to be a list of `x:y` record pairs,
	 * where x is the indirection table index and y is its value.
	 */
	for (buf = msg; *buf != '\0'; buf = endp) {
		idx = strtol(buf, &endp, 10);

		if (endp == buf || idx < 0) {
			device_printf(adapter->pdev, "Invalid index: %s\n",
			    buf);
			error = EINVAL;
			break;
		}

		if (idx >= ENA_RX_RSS_TABLE_SIZE) {
			device_printf(adapter->pdev, "Index %d out of range\n",
			    idx);
			error = ERANGE;
			break;
		}

		buf = endp;

		if (*buf++ != ':') {
			device_printf(adapter->pdev, "Missing ':' separator\n");
			error = EINVAL;
			break;
		}

		value = strtol(buf, &endp, 10);

		if (endp == buf || value < 0) {
			device_printf(adapter->pdev, "Invalid value: %s\n",
			    buf);
			error = EINVAL;
			break;
		}

		if (value >= num_queues) {
			device_printf(adapter->pdev, "Value %d out of range\n",
			    value);
			error = ERANGE;
			break;
		}

		indir->table[idx] = value;
	}

	if (error != 0) /* Reload indirection table with last good data. */
		ena_rss_indir_get(adapter, indir->table);

	/* At this point msg has been clobbered by sysctl_handle_string. */
	ena_rss_copy_indir_buf(msg, indir->table);

	if (error == 0)
		error = ena_rss_indir_set(adapter, indir->table);

unlock:
	ENA_LOCK_UNLOCK();

	return (error);
}
#endif /* RSS */
