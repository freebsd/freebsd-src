/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2023-2024 Google LLC
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "gve.h"

static SYSCTL_NODE(_hw, OID_AUTO, gve, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "GVE driver parameters");

bool gve_disable_hw_lro = false;
SYSCTL_BOOL(_hw_gve, OID_AUTO, disable_hw_lro, CTLFLAG_RDTUN,
    &gve_disable_hw_lro, 0, "Controls if hardware LRO is used");

bool gve_allow_4k_rx_buffers = false;
SYSCTL_BOOL(_hw_gve, OID_AUTO, allow_4k_rx_buffers, CTLFLAG_RDTUN,
    &gve_allow_4k_rx_buffers, 0, "Controls if 4K RX Buffers are allowed");

char gve_queue_format[8];
SYSCTL_STRING(_hw_gve, OID_AUTO, queue_format, CTLFLAG_RD,
    &gve_queue_format, 0, "Queue format being used by the iface");

char gve_version[8];
SYSCTL_STRING(_hw_gve, OID_AUTO, driver_version, CTLFLAG_RD,
    &gve_version, 0, "Driver version");

static void
gve_setup_rxq_sysctl(struct sysctl_ctx_list *ctx,
    struct sysctl_oid_list *child, struct gve_rx_ring *rxq)
{
	struct sysctl_oid *node;
	struct sysctl_oid_list *list;
	struct gve_rxq_stats *stats;
	char namebuf[16];

	snprintf(namebuf, sizeof(namebuf), "rxq%d", rxq->com.id);
	node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, namebuf,
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "Receive Queue");
	list = SYSCTL_CHILDREN(node);

	stats = &rxq->stats;

	SYSCTL_ADD_COUNTER_U64(ctx, list, OID_AUTO,
	    "rx_bytes", CTLFLAG_RD,
	    &stats->rbytes, "Bytes received");
	SYSCTL_ADD_COUNTER_U64(ctx, list, OID_AUTO,
	    "rx_packets", CTLFLAG_RD,
	    &stats->rpackets, "Packets received");
	SYSCTL_ADD_COUNTER_U64(ctx, list, OID_AUTO, "rx_copybreak_cnt",
	    CTLFLAG_RD, &stats->rx_copybreak_cnt,
	    "Total frags with mbufs allocated for copybreak");
	SYSCTL_ADD_COUNTER_U64(ctx, list, OID_AUTO, "rx_frag_flip_cnt",
	    CTLFLAG_RD, &stats->rx_frag_flip_cnt,
	    "Total frags that allocated mbuf with page flip");
	SYSCTL_ADD_COUNTER_U64(ctx, list, OID_AUTO, "rx_frag_copy_cnt",
	    CTLFLAG_RD, &stats->rx_frag_copy_cnt,
	    "Total frags with mbuf that copied payload into mbuf");
	SYSCTL_ADD_COUNTER_U64(ctx, list, OID_AUTO, "rx_dropped_pkt",
	    CTLFLAG_RD, &stats->rx_dropped_pkt,
	    "Total rx packets dropped");
	SYSCTL_ADD_COUNTER_U64(ctx, list, OID_AUTO,
	    "rx_dropped_pkt_desc_err", CTLFLAG_RD,
	    &stats->rx_dropped_pkt_desc_err,
	    "Packets dropped due to descriptor error");
	SYSCTL_ADD_COUNTER_U64(ctx, list, OID_AUTO,
	    "rx_dropped_pkt_buf_post_fail", CTLFLAG_RD,
	    &stats->rx_dropped_pkt_buf_post_fail,
	    "Packets dropped due to failure to post enough buffers");
	SYSCTL_ADD_COUNTER_U64(ctx, list, OID_AUTO,
	    "rx_dropped_pkt_mbuf_alloc_fail", CTLFLAG_RD,
	    &stats->rx_dropped_pkt_mbuf_alloc_fail,
	    "Packets dropped due to failed mbuf allocation");
	SYSCTL_ADD_COUNTER_U64(ctx, list, OID_AUTO,
	    "rx_mbuf_dmamap_err", CTLFLAG_RD,
	    &stats->rx_mbuf_dmamap_err,
	    "Number of rx mbufs which could not be dma mapped");
	SYSCTL_ADD_COUNTER_U64(ctx, list, OID_AUTO,
	    "rx_mbuf_mclget_null", CTLFLAG_RD,
	    &stats->rx_mbuf_mclget_null,
	    "Number of times when there were no cluster mbufs");
	SYSCTL_ADD_U32(ctx, list, OID_AUTO,
	    "rx_completed_desc", CTLFLAG_RD,
	    &rxq->cnt, 0, "Number of descriptors completed");
	SYSCTL_ADD_U32(ctx, list, OID_AUTO,
	    "num_desc_posted", CTLFLAG_RD,
	    &rxq->fill_cnt, rxq->fill_cnt,
	    "Toal number of descriptors posted");
}

static void
gve_setup_txq_sysctl(struct sysctl_ctx_list *ctx,
    struct sysctl_oid_list *child, struct gve_tx_ring *txq)
{
	struct sysctl_oid *node;
	struct sysctl_oid_list *tx_list;
	struct gve_txq_stats *stats;
	char namebuf[16];

	snprintf(namebuf, sizeof(namebuf), "txq%d", txq->com.id);
	node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, namebuf,
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "Transmit Queue");
	tx_list = SYSCTL_CHILDREN(node);

	stats = &txq->stats;

	SYSCTL_ADD_U32(ctx, tx_list, OID_AUTO,
	    "tx_posted_desc", CTLFLAG_RD,
	    &txq->req, 0, "Number of descriptors posted by NIC");
	SYSCTL_ADD_U32(ctx, tx_list, OID_AUTO,
	    "tx_completed_desc", CTLFLAG_RD,
	    &txq->done, 0, "Number of descriptors completed");
	SYSCTL_ADD_COUNTER_U64(ctx, tx_list, OID_AUTO,
	    "tx_packets", CTLFLAG_RD,
	    &stats->tpackets, "Packets transmitted");
	SYSCTL_ADD_COUNTER_U64(ctx, tx_list, OID_AUTO,
	    "tx_tso_packets", CTLFLAG_RD,
	    &stats->tso_packet_cnt, "TSO Packets transmitted");
	SYSCTL_ADD_COUNTER_U64(ctx, tx_list, OID_AUTO,
	    "tx_bytes", CTLFLAG_RD,
	    &stats->tbytes, "Bytes transmitted");
	SYSCTL_ADD_COUNTER_U64(ctx, tx_list, OID_AUTO,
	    "tx_delayed_pkt_nospace_device", CTLFLAG_RD,
	    &stats->tx_delayed_pkt_nospace_device,
	    "Packets delayed due to no space in device");
	SYSCTL_ADD_COUNTER_U64(ctx, tx_list, OID_AUTO,
	    "tx_dropped_pkt_nospace_bufring", CTLFLAG_RD,
	    &stats->tx_dropped_pkt_nospace_bufring,
	    "Packets dropped due to no space in br ring");
	SYSCTL_ADD_COUNTER_U64(ctx, tx_list, OID_AUTO,
	    "tx_dropped_pkt_vlan", CTLFLAG_RD,
	    &stats->tx_dropped_pkt_vlan,
	    "Dropped VLAN packets");
	SYSCTL_ADD_COUNTER_U64(ctx, tx_list, OID_AUTO,
	    "tx_delayed_pkt_nospace_descring", CTLFLAG_RD,
	    &stats->tx_delayed_pkt_nospace_descring,
	    "Packets delayed due to no space in desc ring");
	SYSCTL_ADD_COUNTER_U64(ctx, tx_list, OID_AUTO,
	    "tx_delayed_pkt_nospace_compring", CTLFLAG_RD,
	    &stats->tx_delayed_pkt_nospace_compring,
	    "Packets delayed due to no space in comp ring");
	SYSCTL_ADD_COUNTER_U64(ctx, tx_list, OID_AUTO,
	    "tx_delayed_pkt_nospace_qpl_bufs", CTLFLAG_RD,
	    &stats->tx_delayed_pkt_nospace_qpl_bufs,
	    "Packets delayed due to not enough qpl bufs");
	SYSCTL_ADD_COUNTER_U64(ctx, tx_list, OID_AUTO,
	    "tx_delayed_pkt_tsoerr", CTLFLAG_RD,
	    &stats->tx_delayed_pkt_tsoerr,
	    "TSO packets delayed due to err in prep errors");
	SYSCTL_ADD_COUNTER_U64(ctx, tx_list, OID_AUTO,
	    "tx_mbuf_collapse", CTLFLAG_RD,
	    &stats->tx_mbuf_collapse,
	    "tx mbufs that had to be collapsed");
	SYSCTL_ADD_COUNTER_U64(ctx, tx_list, OID_AUTO,
	    "tx_mbuf_defrag", CTLFLAG_RD,
	    &stats->tx_mbuf_defrag,
	    "tx mbufs that had to be defragged");
	SYSCTL_ADD_COUNTER_U64(ctx, tx_list, OID_AUTO,
	    "tx_mbuf_defrag_err", CTLFLAG_RD,
	    &stats->tx_mbuf_defrag_err,
	    "tx mbufs that failed defrag");
	SYSCTL_ADD_COUNTER_U64(ctx, tx_list, OID_AUTO,
	    "tx_mbuf_dmamap_enomem_err", CTLFLAG_RD,
	    &stats->tx_mbuf_dmamap_enomem_err,
	    "tx mbufs that could not be dma-mapped due to low mem");
	SYSCTL_ADD_COUNTER_U64(ctx, tx_list, OID_AUTO,
	    "tx_mbuf_dmamap_err", CTLFLAG_RD,
	    &stats->tx_mbuf_dmamap_err,
	    "tx mbufs that could not be dma-mapped");
	SYSCTL_ADD_COUNTER_U64(ctx, tx_list, OID_AUTO,
	    "tx_timeout", CTLFLAG_RD,
	    &stats->tx_timeout,
	    "detections of timed out packets on tx queues");
}

static void
gve_setup_queue_stat_sysctl(struct sysctl_ctx_list *ctx, struct sysctl_oid_list *child,
    struct gve_priv *priv)
{
	int i;

	for (i = 0; i < priv->rx_cfg.num_queues; i++) {
		gve_setup_rxq_sysctl(ctx, child, &priv->rx[i]);
	}
	for (i = 0; i < priv->tx_cfg.num_queues; i++) {
		gve_setup_txq_sysctl(ctx, child, &priv->tx[i]);
	}
}

static void
gve_setup_adminq_stat_sysctl(struct sysctl_ctx_list *ctx,
    struct sysctl_oid_list *child, struct gve_priv *priv)
{
	struct sysctl_oid *admin_node;
	struct sysctl_oid_list *admin_list;

	/* Admin queue stats */
	admin_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, "adminq_stats",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "Admin Queue statistics");
	admin_list = SYSCTL_CHILDREN(admin_node);

	SYSCTL_ADD_U32(ctx, admin_list, OID_AUTO, "adminq_prod_cnt", CTLFLAG_RD,
	    &priv->adminq_prod_cnt, 0, "Adminq Commands issued");
	SYSCTL_ADD_U32(ctx, admin_list, OID_AUTO, "adminq_cmd_fail", CTLFLAG_RD,
	    &priv->adminq_cmd_fail, 0, "Aqminq Failed commands");
	SYSCTL_ADD_U32(ctx, admin_list, OID_AUTO, "adminq_timeouts", CTLFLAG_RD,
	    &priv->adminq_timeouts, 0, "Adminq Timedout commands");
	SYSCTL_ADD_U32(ctx, admin_list, OID_AUTO, "adminq_describe_device_cnt",
	    CTLFLAG_RD, &priv->adminq_describe_device_cnt, 0,
	    "adminq_describe_device_cnt");
	SYSCTL_ADD_U32(ctx, admin_list, OID_AUTO,
	    "adminq_cfg_device_resources_cnt", CTLFLAG_RD,
	    &priv->adminq_cfg_device_resources_cnt, 0,
	    "adminq_cfg_device_resources_cnt");
	SYSCTL_ADD_U32(ctx, admin_list, OID_AUTO,
	    "adminq_register_page_list_cnt", CTLFLAG_RD,
	    &priv->adminq_register_page_list_cnt, 0,
	    "adminq_register_page_list_cnt");
	SYSCTL_ADD_U32(ctx, admin_list, OID_AUTO,
	    "adminq_unregister_page_list_cnt", CTLFLAG_RD,
	    &priv->adminq_unregister_page_list_cnt, 0,
	    "adminq_unregister_page_list_cnt");
	SYSCTL_ADD_U32(ctx, admin_list, OID_AUTO, "adminq_create_tx_queue_cnt",
	    CTLFLAG_RD, &priv->adminq_create_tx_queue_cnt, 0,
	    "adminq_create_tx_queue_cnt");
	SYSCTL_ADD_U32(ctx, admin_list, OID_AUTO, "adminq_create_rx_queue_cnt",
	    CTLFLAG_RD, &priv->adminq_create_rx_queue_cnt, 0,
	    "adminq_create_rx_queue_cnt");
	SYSCTL_ADD_U32(ctx, admin_list, OID_AUTO, "adminq_destroy_tx_queue_cnt",
	    CTLFLAG_RD, &priv->adminq_destroy_tx_queue_cnt, 0,
	    "adminq_destroy_tx_queue_cnt");
	SYSCTL_ADD_U32(ctx, admin_list, OID_AUTO, "adminq_destroy_rx_queue_cnt",
	    CTLFLAG_RD, &priv->adminq_destroy_rx_queue_cnt, 0,
	    "adminq_destroy_rx_queue_cnt");
	SYSCTL_ADD_U32(ctx, admin_list, OID_AUTO, "adminq_get_ptype_map_cnt",
	    CTLFLAG_RD, &priv->adminq_get_ptype_map_cnt, 0,
	    "adminq_get_ptype_map_cnt");
	SYSCTL_ADD_U32(ctx, admin_list, OID_AUTO,
	    "adminq_dcfg_device_resources_cnt", CTLFLAG_RD,
	    &priv->adminq_dcfg_device_resources_cnt, 0,
	    "adminq_dcfg_device_resources_cnt");
	SYSCTL_ADD_U32(ctx, admin_list, OID_AUTO,
	    "adminq_set_driver_parameter_cnt", CTLFLAG_RD,
	    &priv->adminq_set_driver_parameter_cnt, 0,
	    "adminq_set_driver_parameter_cnt");
	SYSCTL_ADD_U32(ctx, admin_list, OID_AUTO,
	    "adminq_verify_driver_compatibility_cnt", CTLFLAG_RD,
	    &priv->adminq_verify_driver_compatibility_cnt, 0,
	    "adminq_verify_driver_compatibility_cnt");
}

static void
gve_setup_main_stat_sysctl(struct sysctl_ctx_list *ctx,
    struct sysctl_oid_list *child, struct gve_priv *priv)
{
	struct sysctl_oid *main_node;
	struct sysctl_oid_list *main_list;

	/* Main stats */
	main_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, "main_stats",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "Main statistics");
	main_list = SYSCTL_CHILDREN(main_node);

	SYSCTL_ADD_U32(ctx, main_list, OID_AUTO, "interface_up_cnt", CTLFLAG_RD,
	    &priv->interface_up_cnt, 0, "Times interface was set to up");
	SYSCTL_ADD_U32(ctx, main_list, OID_AUTO, "interface_down_cnt", CTLFLAG_RD,
	    &priv->interface_down_cnt, 0, "Times interface was set to down");
	SYSCTL_ADD_U32(ctx, main_list, OID_AUTO, "reset_cnt", CTLFLAG_RD,
	    &priv->reset_cnt, 0, "Times reset");
}

static int
gve_check_num_queues(struct gve_priv *priv, int val, bool is_rx)
{
	if (val < 1) {
		device_printf(priv->dev,
		    "Requested num queues (%u) must be a positive integer\n", val);
		return (EINVAL);
	}

	if (val > (is_rx ? priv->rx_cfg.max_queues : priv->tx_cfg.max_queues)) {
		device_printf(priv->dev,
		    "Requested num queues (%u) is too large\n", val);
		return (EINVAL);
	}

	return (0);
}

static int
gve_sysctl_num_tx_queues(SYSCTL_HANDLER_ARGS)
{
	struct gve_priv *priv = arg1;
	int val;
	int err;

	val = priv->tx_cfg.num_queues;
	err = sysctl_handle_int(oidp, &val, 0, req);
	if (err != 0 || req->newptr == NULL)
		return (err);

	err = gve_check_num_queues(priv, val, /*is_rx=*/false);
	if (err != 0)
		return (err);

	if (val != priv->tx_cfg.num_queues) {
		GVE_IFACE_LOCK_LOCK(priv->gve_iface_lock);
		err = gve_adjust_tx_queues(priv, val);
		GVE_IFACE_LOCK_UNLOCK(priv->gve_iface_lock);
	}

	return (err);
}

static int
gve_sysctl_num_rx_queues(SYSCTL_HANDLER_ARGS)
{
	struct gve_priv *priv = arg1;
	int val;
	int err;

	val = priv->rx_cfg.num_queues;
	err = sysctl_handle_int(oidp, &val, 0, req);
	if (err != 0 || req->newptr == NULL)
		return (err);

	err = gve_check_num_queues(priv, val, /*is_rx=*/true);

	if (err != 0)
		return (err);

	if (val != priv->rx_cfg.num_queues) {
		GVE_IFACE_LOCK_LOCK(priv->gve_iface_lock);
		err = gve_adjust_rx_queues(priv, val);
		GVE_IFACE_LOCK_UNLOCK(priv->gve_iface_lock);
	}

	return (err);
}

static int
gve_check_ring_size(struct gve_priv *priv, int val, bool is_rx)
{
	if (!powerof2(val) || val == 0) {
		device_printf(priv->dev,
		    "Requested ring size (%u) must be a power of 2\n", val);
		return (EINVAL);
	}

	if (val < (is_rx ? priv->min_rx_desc_cnt : priv->min_tx_desc_cnt)) {
		device_printf(priv->dev,
		    "Requested ring size (%u) cannot be less than %d\n", val,
		    (is_rx ? priv->min_rx_desc_cnt : priv->min_tx_desc_cnt));
		return (EINVAL);
	}


	if (val > (is_rx ? priv->max_rx_desc_cnt : priv->max_tx_desc_cnt)) {
		device_printf(priv->dev,
		    "Requested ring size (%u) cannot be greater than %d\n", val,
		    (is_rx ? priv->max_rx_desc_cnt : priv->max_tx_desc_cnt));
		return (EINVAL);
	}

	return (0);
}

static int
gve_sysctl_tx_ring_size(SYSCTL_HANDLER_ARGS)
{
	struct gve_priv *priv = arg1;
	int val;
	int err;

	val = priv->tx_desc_cnt;
	err = sysctl_handle_int(oidp, &val, 0, req);
	if (err != 0 || req->newptr == NULL)
		return (err);

	err = gve_check_ring_size(priv, val, /*is_rx=*/false);
	if (err != 0)
		return (err);

	if (val != priv->tx_desc_cnt) {
		GVE_IFACE_LOCK_LOCK(priv->gve_iface_lock);
		err = gve_adjust_ring_sizes(priv, val, /*is_rx=*/false);
		GVE_IFACE_LOCK_UNLOCK(priv->gve_iface_lock);
	}

	return (err);
}

static int
gve_sysctl_rx_ring_size(SYSCTL_HANDLER_ARGS)
{
	struct gve_priv *priv = arg1;
	int val;
	int err;

	val = priv->rx_desc_cnt;
	err = sysctl_handle_int(oidp, &val, 0, req);
	if (err != 0 || req->newptr == NULL)
		return (err);

	err = gve_check_ring_size(priv, val, /*is_rx=*/true);
	if (err != 0)
		return (err);

	if (val != priv->rx_desc_cnt) {
		GVE_IFACE_LOCK_LOCK(priv->gve_iface_lock);
		err = gve_adjust_ring_sizes(priv, val, /*is_rx=*/true);
		GVE_IFACE_LOCK_UNLOCK(priv->gve_iface_lock);
	}

	return (err);
}

static void
gve_setup_sysctl_writables(struct sysctl_ctx_list *ctx,
    struct sysctl_oid_list *child, struct gve_priv *priv)
{
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "num_tx_queues",
	    CTLTYPE_UINT | CTLFLAG_RW | CTLFLAG_MPSAFE, priv, 0,
	    gve_sysctl_num_tx_queues, "I", "Number of TX queues");

	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "num_rx_queues",
	    CTLTYPE_UINT | CTLFLAG_RW | CTLFLAG_MPSAFE, priv, 0,
	    gve_sysctl_num_rx_queues, "I", "Number of RX queues");

	if (priv->modify_ringsize_enabled) {
		SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "tx_ring_size",
		    CTLTYPE_UINT | CTLFLAG_RW | CTLFLAG_MPSAFE, priv, 0,
		    gve_sysctl_tx_ring_size, "I", "TX ring size");

		SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "rx_ring_size",
		    CTLTYPE_UINT | CTLFLAG_RW | CTLFLAG_MPSAFE, priv, 0,
		    gve_sysctl_rx_ring_size, "I", "RX ring size");
	}
}

void gve_setup_sysctl(struct gve_priv *priv)
{
	device_t dev;
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *tree;
	struct sysctl_oid_list *child;

	dev = priv->dev;
	ctx = device_get_sysctl_ctx(dev);
	tree = device_get_sysctl_tree(dev);
	child = SYSCTL_CHILDREN(tree);

	gve_setup_queue_stat_sysctl(ctx, child, priv);
	gve_setup_adminq_stat_sysctl(ctx, child, priv);
	gve_setup_main_stat_sysctl(ctx, child, priv);
	gve_setup_sysctl_writables(ctx, child, priv);
}

void
gve_accum_stats(struct gve_priv *priv, uint64_t *rpackets,
    uint64_t *rbytes, uint64_t *rx_dropped_pkt, uint64_t *tpackets,
    uint64_t *tbytes, uint64_t *tx_dropped_pkt)
{
	struct gve_rxq_stats *rxqstats;
	struct gve_txq_stats *txqstats;
	int i;

	for (i = 0; i < priv->rx_cfg.num_queues; i++) {
		rxqstats = &priv->rx[i].stats;
		*rpackets += counter_u64_fetch(rxqstats->rpackets);
		*rbytes += counter_u64_fetch(rxqstats->rbytes);
		*rx_dropped_pkt += counter_u64_fetch(rxqstats->rx_dropped_pkt);
	}

	for (i = 0; i < priv->tx_cfg.num_queues; i++) {
		txqstats = &priv->tx[i].stats;
		*tpackets += counter_u64_fetch(txqstats->tpackets);
		*tbytes += counter_u64_fetch(txqstats->tbytes);
		*tx_dropped_pkt += counter_u64_fetch(txqstats->tx_dropped_pkt);
	}
}
