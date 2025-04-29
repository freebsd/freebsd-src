/*-
 * Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2016 Broadcom, All Rights Reserved.
 * The term Broadcom refers to Broadcom Limited and/or its subsidiaries
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/ctype.h>
#include <linux/delay.h>

#include "bnxt.h"
#include "bnxt_hwrm.h"
#include "bnxt_sysctl.h"

DEFINE_MUTEX(tmp_mutex); /* mutex lock for driver */
extern void bnxt_fw_reset(struct bnxt_softc *bp);
extern void bnxt_queue_sp_work(struct bnxt_softc *bp);
extern void
process_nq(struct bnxt_softc *softc, uint16_t nqid);
/*
 * We want to create:
 * dev.bnxt.0.hwstats.txq0
 * dev.bnxt.0.hwstats.txq0.txmbufs
 * dev.bnxt.0.hwstats.rxq0
 * dev.bnxt.0.hwstats.txq0.rxmbufs
 * so the hwstats ctx list needs to be created in attach_post and populated
 * during init.
 *
 * Then, it needs to be cleaned up in stop.
 */

int
bnxt_init_sysctl_ctx(struct bnxt_softc *softc)
{
	struct sysctl_ctx_list *ctx;

	sysctl_ctx_init(&softc->hw_stats);
	ctx = device_get_sysctl_ctx(softc->dev);
	softc->hw_stats_oid = SYSCTL_ADD_NODE(ctx,
	    SYSCTL_CHILDREN(device_get_sysctl_tree(softc->dev)), OID_AUTO,
	    "hwstats", CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "hardware statistics");
	if (!softc->hw_stats_oid) {
		sysctl_ctx_free(&softc->hw_stats);
		return ENOMEM;
	}

	sysctl_ctx_init(&softc->ver_info->ver_ctx);
	ctx = device_get_sysctl_ctx(softc->dev);
	softc->ver_info->ver_oid = SYSCTL_ADD_NODE(ctx,
	    SYSCTL_CHILDREN(device_get_sysctl_tree(softc->dev)), OID_AUTO,
	    "ver", CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
	    "hardware/firmware version information");
	if (!softc->ver_info->ver_oid) {
		sysctl_ctx_free(&softc->ver_info->ver_ctx);
		return ENOMEM;
	}

	if (BNXT_PF(softc)) {
		sysctl_ctx_init(&softc->nvm_info->nvm_ctx);
		ctx = device_get_sysctl_ctx(softc->dev);
		softc->nvm_info->nvm_oid = SYSCTL_ADD_NODE(ctx,
		    SYSCTL_CHILDREN(device_get_sysctl_tree(softc->dev)), OID_AUTO,
		    "nvram", CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
		    "nvram information");
		if (!softc->nvm_info->nvm_oid) {
			sysctl_ctx_free(&softc->nvm_info->nvm_ctx);
			return ENOMEM;
		}
	}

	sysctl_ctx_init(&softc->hw_lro_ctx);
	ctx = device_get_sysctl_ctx(softc->dev);
	softc->hw_lro_oid = SYSCTL_ADD_NODE(ctx,
	    SYSCTL_CHILDREN(device_get_sysctl_tree(softc->dev)), OID_AUTO,
	    "hw_lro", CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "hardware lro");
	if (!softc->hw_lro_oid) {
		sysctl_ctx_free(&softc->hw_lro_ctx);
		return ENOMEM;
	}

	sysctl_ctx_init(&softc->flow_ctrl_ctx);
	ctx = device_get_sysctl_ctx(softc->dev);
	softc->flow_ctrl_oid = SYSCTL_ADD_NODE(ctx,
	    SYSCTL_CHILDREN(device_get_sysctl_tree(softc->dev)), OID_AUTO,
	    "fc", CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "flow ctrl");
	if (!softc->flow_ctrl_oid) {
		sysctl_ctx_free(&softc->flow_ctrl_ctx);
		return ENOMEM;
	}

	sysctl_ctx_init(&softc->dcb_ctx);
	ctx = device_get_sysctl_ctx(softc->dev);
	softc->dcb_oid = SYSCTL_ADD_NODE(ctx,
	    SYSCTL_CHILDREN(device_get_sysctl_tree(softc->dev)), OID_AUTO,
	    "dcb", CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "Data Center Bridging");
	if (!softc->dcb_oid) {
		sysctl_ctx_free(&softc->dcb_ctx);
		return ENOMEM;
	}

	return 0;
}

int
bnxt_free_sysctl_ctx(struct bnxt_softc *softc)
{
	int orc;
	int rc = 0;

	if (softc->hw_stats_oid != NULL) {
		orc = sysctl_ctx_free(&softc->hw_stats);
		if (orc)
			rc = orc;
		else
			softc->hw_stats_oid = NULL;
	}
	if (softc->ver_info->ver_oid != NULL) {
		orc = sysctl_ctx_free(&softc->ver_info->ver_ctx);
		if (orc)
			rc = orc;
		else
			softc->ver_info->ver_oid = NULL;
	}
	if (BNXT_PF(softc) && softc->nvm_info->nvm_oid != NULL) {
		orc = sysctl_ctx_free(&softc->nvm_info->nvm_ctx);
		if (orc)
			rc = orc;
		else
			softc->nvm_info->nvm_oid = NULL;
	}
	if (softc->hw_lro_oid != NULL) {
		orc = sysctl_ctx_free(&softc->hw_lro_ctx);
		if (orc)
			rc = orc;
		else
			softc->hw_lro_oid = NULL;
	}

	if (softc->flow_ctrl_oid != NULL) {
		orc = sysctl_ctx_free(&softc->flow_ctrl_ctx);
		if (orc)
			rc = orc;
		else
			softc->flow_ctrl_oid = NULL;
	}

	if (softc->dcb_oid != NULL) {
		orc = sysctl_ctx_free(&softc->dcb_ctx);
		if (orc)
			rc = orc;
		else
			softc->dcb_oid = NULL;
	}

	return rc;
}

int
bnxt_create_tx_sysctls(struct bnxt_softc *softc, int txr)
{
	struct sysctl_oid *oid;
	struct ctx_hw_stats *tx_stats = (void *)softc->tx_stats[txr].idi_vaddr;
	char	name[32];
	char	desc[64];

	sprintf(name, "txq%d", txr);
	sprintf(desc, "transmit queue %d", txr);
	oid = SYSCTL_ADD_NODE(&softc->hw_stats,
	    SYSCTL_CHILDREN(softc->hw_stats_oid), OID_AUTO, name,
	    CTLFLAG_RD | CTLFLAG_MPSAFE, 0, desc);
	if (!oid)
		return ENOMEM;

	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "ucast_pkts", CTLFLAG_RD, &tx_stats->tx_ucast_pkts,
	    "unicast packets sent");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "mcast_pkts", CTLFLAG_RD, &tx_stats->tx_mcast_pkts,
	    "multicast packets sent");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "bcast_pkts", CTLFLAG_RD, &tx_stats->tx_bcast_pkts,
	    "broadcast packets sent");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "discard_pkts", CTLFLAG_RD,
	    &tx_stats->tx_discard_pkts, "discarded transmit packets");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "error_pkts", CTLFLAG_RD, &tx_stats->tx_error_pkts,
	    "Error transmit packets");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "ucast_bytes", CTLFLAG_RD, &tx_stats->tx_ucast_bytes,
	    "unicast bytes sent");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "mcast_bytes", CTLFLAG_RD, &tx_stats->tx_mcast_bytes,
	    "multicast bytes sent");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "bcast_bytes", CTLFLAG_RD, &tx_stats->tx_bcast_bytes,
	    "broadcast bytes sent");

	return 0;
}

int
bnxt_create_port_stats_sysctls(struct bnxt_softc *softc)
{
	struct sysctl_oid *oid;
	char	name[32];
	char	desc[64];

	sprintf(name, "port_stats");
	sprintf(desc, "Port Stats");
	oid = SYSCTL_ADD_NODE(&softc->hw_stats,
	    SYSCTL_CHILDREN(softc->hw_stats_oid), OID_AUTO, name,
	        CTLFLAG_RD | CTLFLAG_MPSAFE, 0, desc);
	if (!oid)
		return ENOMEM;

	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "tx_64b_frames", CTLFLAG_RD,
	    &softc->tx_port_stats->tx_64b_frames, "Transmitted 64b frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "tx_65b_127b_frames", CTLFLAG_RD,
	    &softc->tx_port_stats->tx_65b_127b_frames,
	    "Transmitted 65b 127b frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "tx_128b_255b_frames", CTLFLAG_RD,
	    &softc->tx_port_stats->tx_128b_255b_frames,
	    "Transmitted 128b 255b frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "tx_256b_511b_frames", CTLFLAG_RD,
	    &softc->tx_port_stats->tx_256b_511b_frames,
	    "Transmitted 256b 511b frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "tx_512b_1023b_frames", CTLFLAG_RD,
	    &softc->tx_port_stats->tx_512b_1023b_frames,
	    "Transmitted 512b 1023b frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "tx_1024b_1518b_frames", CTLFLAG_RD,
	    &softc->tx_port_stats->tx_1024b_1518b_frames,
	    "Transmitted 1024b 1518b frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "tx_good_vlan_frames", CTLFLAG_RD,
	    &softc->tx_port_stats->tx_good_vlan_frames,
	    "Transmitted good vlan frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "tx_1519b_2047b_frames", CTLFLAG_RD,
	    &softc->tx_port_stats->tx_1519b_2047b_frames,
	    "Transmitted 1519b 2047b frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "tx_2048b_4095b_frames", CTLFLAG_RD,
	    &softc->tx_port_stats->tx_2048b_4095b_frames,
	    "Transmitted 2048b 4095b frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "tx_4096b_9216b_frames", CTLFLAG_RD,
	    &softc->tx_port_stats->tx_4096b_9216b_frames,
	    "Transmitted 4096b 9216b frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "tx_9217b_16383b_frames", CTLFLAG_RD,
	    &softc->tx_port_stats->tx_9217b_16383b_frames,
	    "Transmitted 9217b 16383b frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "tx_good_frames", CTLFLAG_RD,
	    &softc->tx_port_stats->tx_good_frames, "Transmitted good frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "tx_total_frames", CTLFLAG_RD,
	    &softc->tx_port_stats->tx_total_frames, "Transmitted total frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "tx_ucast_frames", CTLFLAG_RD,
	    &softc->tx_port_stats->tx_ucast_frames, "Transmitted ucast frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "tx_mcast_frames", CTLFLAG_RD,
	    &softc->tx_port_stats->tx_mcast_frames, "Transmitted mcast frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "tx_bcast_frames", CTLFLAG_RD,
	    &softc->tx_port_stats->tx_bcast_frames, "Transmitted bcast frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "tx_pause_frames", CTLFLAG_RD,
	    &softc->tx_port_stats->tx_pause_frames, "Transmitted pause frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "tx_pfc_frames", CTLFLAG_RD,
	    &softc->tx_port_stats->tx_pfc_frames, "Transmitted pfc frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "tx_jabber_frames", CTLFLAG_RD,
	    &softc->tx_port_stats->tx_jabber_frames, "Transmitted jabber frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "tx_fcs_err_frames", CTLFLAG_RD,
	    &softc->tx_port_stats->tx_fcs_err_frames,
	    "Transmitted fcs err frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "tx_err", CTLFLAG_RD,
	    &softc->tx_port_stats->tx_err, "Transmitted err");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "tx_fifo_underruns", CTLFLAG_RD,
	    &softc->tx_port_stats->tx_fifo_underruns,
	    "Transmitted fifo underruns");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "tx_pfc_ena_frames_pri0", CTLFLAG_RD,
	    &softc->tx_port_stats->tx_pfc_ena_frames_pri0,
	    "Transmitted pfc ena frames pri0");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "tx_pfc_ena_frames_pri1", CTLFLAG_RD,
	    &softc->tx_port_stats->tx_pfc_ena_frames_pri1,
	    "Transmitted pfc ena frames pri1");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "tx_pfc_ena_frames_pri2", CTLFLAG_RD,
	    &softc->tx_port_stats->tx_pfc_ena_frames_pri2,
	    "Transmitted pfc ena frames pri2");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "tx_pfc_ena_frames_pri3", CTLFLAG_RD,
	    &softc->tx_port_stats->tx_pfc_ena_frames_pri3,
	    "Transmitted pfc ena frames pri3");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "tx_pfc_ena_frames_pri4", CTLFLAG_RD,
	    &softc->tx_port_stats->tx_pfc_ena_frames_pri4,
	    "Transmitted pfc ena frames pri4");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "tx_pfc_ena_frames_pri5", CTLFLAG_RD,
	    &softc->tx_port_stats->tx_pfc_ena_frames_pri5,
	    "Transmitted pfc ena frames pri5");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "tx_pfc_ena_frames_pri6", CTLFLAG_RD,
	    &softc->tx_port_stats->tx_pfc_ena_frames_pri6,
	    "Transmitted pfc ena frames pri6");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "tx_pfc_ena_frames_pri7", CTLFLAG_RD,
	    &softc->tx_port_stats->tx_pfc_ena_frames_pri7,
	    "Transmitted pfc ena frames pri7");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "tx_eee_lpi_events", CTLFLAG_RD,
	    &softc->tx_port_stats->tx_eee_lpi_events,
	    "Transmitted eee lpi events");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "tx_eee_lpi_duration", CTLFLAG_RD,
	    &softc->tx_port_stats->tx_eee_lpi_duration,
	    "Transmitted eee lpi duration");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "tx_llfc_logical_msgs", CTLFLAG_RD,
	    &softc->tx_port_stats->tx_llfc_logical_msgs,
	    "Transmitted llfc logical msgs");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "tx_hcfc_msgs", CTLFLAG_RD,
	    &softc->tx_port_stats->tx_hcfc_msgs, "Transmitted hcfc msgs");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "tx_total_collisions", CTLFLAG_RD,
	    &softc->tx_port_stats->tx_total_collisions,
	    "Transmitted total collisions");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "tx_bytes", CTLFLAG_RD,
	    &softc->tx_port_stats->tx_bytes, "Transmitted bytes");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "tx_xthol_frames", CTLFLAG_RD,
	    &softc->tx_port_stats->tx_xthol_frames, "Transmitted xthol frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "tx_stat_discard", CTLFLAG_RD,
	    &softc->tx_port_stats->tx_stat_discard, "Transmitted stat discard");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "tx_stat_error", CTLFLAG_RD,
	    &softc->tx_port_stats->tx_stat_error, "Transmitted stat error");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_64b_frames", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_64b_frames, "Received 64b frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_65b_127b_frames", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_65b_127b_frames, "Received 65b 127b frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_128b_255b_frames", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_128b_255b_frames,
	    "Received 128b 255b frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_256b_511b_frames", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_256b_511b_frames,
	    "Received 256b 511b frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_512b_1023b_frames", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_512b_1023b_frames,
	    "Received 512b 1023b frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_1024b_1518b_frames", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_1024b_1518b_frames,
	    "Received 1024b 1518 frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_good_vlan_frames", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_good_vlan_frames,
	    "Received good vlan frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_1519b_2047b_frames", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_1519b_2047b_frames,
	    "Received 1519b 2047b frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_2048b_4095b_frames", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_2048b_4095b_frames,
	    "Received 2048b 4095b frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_4096b_9216b_frames", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_4096b_9216b_frames,
	    "Received 4096b 9216b frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_9217b_16383b_frames", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_9217b_16383b_frames,
	    "Received 9217b 16383b frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_total_frames", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_total_frames, "Received total frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_ucast_frames", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_ucast_frames, "Received ucast frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_mcast_frames", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_mcast_frames, "Received mcast frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_bcast_frames", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_bcast_frames, "Received bcast frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_fcs_err_frames", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_fcs_err_frames, "Received fcs err frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_ctrl_frames", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_ctrl_frames, "Received ctrl frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_pause_frames", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_pause_frames, "Received pause frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_pfc_frames", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_pfc_frames, "Received pfc frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_align_err_frames", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_align_err_frames,
	    "Received align err frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_ovrsz_frames", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_ovrsz_frames,
	    "Received ovrsz frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_jbr_frames", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_jbr_frames,
	    "Received jbr frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_mtu_err_frames", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_mtu_err_frames,
	    "Received mtu err frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_tagged_frames", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_tagged_frames,
	    "Received tagged frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_double_tagged_frames", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_double_tagged_frames,
	    "Received double tagged frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_good_frames", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_good_frames,
	    "Received good frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_pfc_ena_frames_pri0", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_pfc_ena_frames_pri0,
	    "Received pfc ena frames pri0");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_pfc_ena_frames_pri1", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_pfc_ena_frames_pri1,
	    "Received pfc ena frames pri1");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_pfc_ena_frames_pri2", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_pfc_ena_frames_pri2,
	    "Received pfc ena frames pri2");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_pfc_ena_frames_pri3", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_pfc_ena_frames_pri3,
	    "Received pfc ena frames pri3");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_pfc_ena_frames_pri4", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_pfc_ena_frames_pri4,
	    "Received pfc ena frames pri4");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_pfc_ena_frames_pri5", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_pfc_ena_frames_pri5,
	    "Received pfc ena frames pri5");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_pfc_ena_frames_pri6", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_pfc_ena_frames_pri6,
	    "Received pfc ena frames pri6");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_pfc_ena_frames_pri7", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_pfc_ena_frames_pri7,
	    "Received pfc ena frames pri7");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_sch_crc_err_frames", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_sch_crc_err_frames,
	    "Received sch crc err frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_undrsz_frames", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_undrsz_frames, "Received undrsz frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_eee_lpi_events", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_eee_lpi_events, "Received eee lpi events");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_eee_lpi_duration", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_eee_lpi_duration,
	    "Received eee lpi duration");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_llfc_physical_msgs", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_llfc_physical_msgs,
	    "Received llfc physical msgs");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_llfc_logical_msgs", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_llfc_logical_msgs,
	    "Received llfc logical msgs");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_llfc_msgs_with_crc_err", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_llfc_msgs_with_crc_err,
	    "Received llfc msgs with crc err");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_hcfc_msgs", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_hcfc_msgs, "Received hcfc msgs");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_hcfc_msgs_with_crc_err", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_hcfc_msgs_with_crc_err,
	    "Received hcfc msgs with crc err");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_bytes", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_bytes, "Received bytes");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_runt_bytes", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_runt_bytes, "Received runt bytes");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_runt_frames", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_runt_frames, "Received runt frames");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_stat_discard", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_stat_discard, "Received stat discard");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx_stat_err", CTLFLAG_RD,
	    &softc->rx_port_stats->rx_stat_err, "Received stat err");

	if (BNXT_CHIP_P5_PLUS(softc) &&
	    (softc->flags & BNXT_FLAG_FW_CAP_EXT_STATS)) {
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "tx_bytes_cos0", CTLFLAG_RD,
		    &softc->tx_port_stats_ext->tx_bytes_cos0, "Transmitted bytes count cos0");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "tx_packets_cos0", CTLFLAG_RD,
		    &softc->tx_port_stats_ext->tx_packets_cos0, "Transmitted packets count cos0");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "tx_bytes_cos1", CTLFLAG_RD,
		    &softc->tx_port_stats_ext->tx_bytes_cos1, "Transmitted bytes count cos1");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "tx_packets_cos1", CTLFLAG_RD,
		    &softc->tx_port_stats_ext->tx_packets_cos1, "Transmitted packets count cos1");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "tx_bytes_cos2", CTLFLAG_RD,
		    &softc->tx_port_stats_ext->tx_bytes_cos2, "Transmitted bytes count cos2");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "tx_packets_cos2", CTLFLAG_RD,
		    &softc->tx_port_stats_ext->tx_packets_cos2, "Transmitted packets count cos2");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "tx_bytes_cos3", CTLFLAG_RD,
		    &softc->tx_port_stats_ext->tx_bytes_cos3, "Transmitted bytes count cos3");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "tx_packets_cos3", CTLFLAG_RD,
		    &softc->tx_port_stats_ext->tx_packets_cos3, "Transmitted packets count cos3");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "tx_bytes_cos4", CTLFLAG_RD,
		    &softc->tx_port_stats_ext->tx_bytes_cos4, "Transmitted bytes count cos4");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "tx_packets_cos4", CTLFLAG_RD,
		    &softc->tx_port_stats_ext->tx_packets_cos4, "Transmitted packets count cos4");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "tx_bytes_cos5", CTLFLAG_RD,
		    &softc->tx_port_stats_ext->tx_bytes_cos5, "Transmitted bytes count cos5");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "tx_packets_cos5", CTLFLAG_RD,
		    &softc->tx_port_stats_ext->tx_packets_cos5, "Transmitted packets count cos5");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "tx_bytes_cos6", CTLFLAG_RD,
		    &softc->tx_port_stats_ext->tx_bytes_cos6, "Transmitted bytes count cos6");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "tx_packets_cos6", CTLFLAG_RD,
		    &softc->tx_port_stats_ext->tx_packets_cos6, "Transmitted packets count cos6");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "tx_bytes_cos7", CTLFLAG_RD,
		    &softc->tx_port_stats_ext->tx_bytes_cos7, "Transmitted bytes count cos7");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "tx_packets_cos7", CTLFLAG_RD,
		    &softc->tx_port_stats_ext->tx_packets_cos7, "Transmitted packets count cos7");

		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "tx_bytes_pri0", CTLFLAG_RD,
		    &softc->tx_bytes_pri[0], "Transmitted bytes count pri0");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "tx_packets_pri0", CTLFLAG_RD,
		    &softc->tx_packets_pri[0], "Transmitted packets count pri0");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "tx_bytes_pri1", CTLFLAG_RD,
		    &softc->tx_bytes_pri[1], "Transmitted bytes count pri1");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "tx_packets_pri1", CTLFLAG_RD,
		    &softc->tx_packets_pri[1], "Transmitted packets count pri1");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "tx_bytes_pri2", CTLFLAG_RD,
		    &softc->tx_bytes_pri[2], "Transmitted bytes count pri2");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "tx_packets_pri2", CTLFLAG_RD,
		    &softc->tx_packets_pri[2], "Transmitted packets count pri2");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "tx_bytes_pri3", CTLFLAG_RD,
		    &softc->tx_bytes_pri[3], "Transmitted bytes count pri3");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "tx_packets_pri3", CTLFLAG_RD,
		    &softc->tx_packets_pri[3], "Transmitted packets count pri3");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "tx_bytes_pri4", CTLFLAG_RD,
		    &softc->tx_bytes_pri[4], "Transmitted bytes count pri4");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "tx_packets_pri4", CTLFLAG_RD,
		    &softc->tx_packets_pri[4], "Transmitted packets count pri4");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "tx_bytes_pri5", CTLFLAG_RD,
		    &softc->tx_bytes_pri[5], "Transmitted bytes count pri5");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "tx_packets_pri5", CTLFLAG_RD,
		    &softc->tx_packets_pri[5], "Transmitted packets count pri5");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "tx_bytes_pri6", CTLFLAG_RD,
		    &softc->tx_bytes_pri[6], "Transmitted bytes count pri6");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "tx_packets_pri6", CTLFLAG_RD,
		    &softc->tx_packets_pri[6], "Transmitted packets count pri6");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "tx_bytes_pri7", CTLFLAG_RD,
		    &softc->tx_bytes_pri[7], "Transmitted bytes count pri7");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "tx_packets_pri7", CTLFLAG_RD,
		    &softc->tx_packets_pri[7], "Transmitted packets count pri7");

		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "pfc_pri0_tx_duration_us", CTLFLAG_RD,
		    &softc->tx_port_stats_ext->pfc_pri0_tx_duration_us, "Time duration between"
		    "XON to XOFF and XOFF to XON for pri0");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "pfc_pri0_tx_transitions", CTLFLAG_RD,
		    &softc->tx_port_stats_ext->pfc_pri0_tx_transitions, "Num times transition"
		    "between XON to XOFF and XOFF to XON for pri0");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "pfc_pri1_tx_duration_us", CTLFLAG_RD,
		    &softc->tx_port_stats_ext->pfc_pri1_tx_duration_us, "Time duration between"
		    "XON to XOFF and XOFF to XON for pri1");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "pfc_pri1_tx_transitions", CTLFLAG_RD,
		    &softc->tx_port_stats_ext->pfc_pri1_tx_transitions, "Num times transition"
		    "between XON to XOFF and XOFF to XON for pri1");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "pfc_pri2_tx_duration_us", CTLFLAG_RD,
		    &softc->tx_port_stats_ext->pfc_pri2_tx_duration_us, "Time duration between"
		    "XON to XOFF and XOFF to XON for pri2");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "pfc_pri2_tx_transitions", CTLFLAG_RD,
		    &softc->tx_port_stats_ext->pfc_pri2_tx_transitions, "Num times transition"
		    "between XON to XOFF and XOFF to XON for pri2");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "pfc_pri3_tx_duration_us", CTLFLAG_RD,
		    &softc->tx_port_stats_ext->pfc_pri3_tx_duration_us, "Time duration between"
		    "XON to XOFF and XOFF to XON for pri3");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "pfc_pri3_tx_transitions", CTLFLAG_RD,
		    &softc->tx_port_stats_ext->pfc_pri3_tx_transitions, "Num times transition"
		    "between XON to XOFF and XOFF to XON for pri3");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "pfc_pri4_tx_duration_us", CTLFLAG_RD,
		    &softc->tx_port_stats_ext->pfc_pri4_tx_duration_us, "Time duration between"
		    "XON to XOFF and XOFF to XON for pri4");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "pfc_pri4_tx_transitions", CTLFLAG_RD,
		    &softc->tx_port_stats_ext->pfc_pri4_tx_transitions, "Num times transition"
		    "between XON to XOFF and XOFF to XON for pri4");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "pfc_pri5_tx_duration_us", CTLFLAG_RD,
		    &softc->tx_port_stats_ext->pfc_pri5_tx_duration_us, "Time duration between"
		    "XON to XOFF and XOFF to XON for pri5");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "pfc_pri5_tx_transitions", CTLFLAG_RD,
		    &softc->tx_port_stats_ext->pfc_pri5_tx_transitions, "Num times transition"
		    "between XON to XOFF and XOFF to XON for pri5");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "pfc_pri6_tx_duration_us", CTLFLAG_RD,
		    &softc->tx_port_stats_ext->pfc_pri6_tx_duration_us, "Time duration between"
		    "XON to XOFF and XOFF to XON for pri6");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "pfc_pri6_tx_transitions", CTLFLAG_RD,
		    &softc->tx_port_stats_ext->pfc_pri6_tx_transitions, "Num times transition"
		    "between XON to XOFF and XOFF to XON for pri6");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "pfc_pri7_tx_duration_us", CTLFLAG_RD,
		    &softc->tx_port_stats_ext->pfc_pri7_tx_duration_us, "Time duration between"
		    "XON to XOFF and XOFF to XON for pri7");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "pfc_pri7_tx_transitions", CTLFLAG_RD,
		    &softc->tx_port_stats_ext->pfc_pri7_tx_transitions, "Num times transition"
		    "between XON to XOFF and XOFF to XON for pri7");

		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "link_down_events", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->link_down_events, "Num times link states down");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "continuous_pause_events", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->continuous_pause_events, "Num times pause events");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "resume_pause_events", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->resume_pause_events, "Num times pause events"
		    "resumes");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "continuous_roce_pause_events", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->continuous_roce_pause_events, "Num times roce"
		    "pause events");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "resume_roce_pause_events", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->resume_roce_pause_events, "Num times roce pause"
		    "events resumes");

		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_bytes_cos0", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->rx_bytes_cos0, "Received bytes count cos0");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_packets_cos0", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->rx_packets_cos0, "Received packets count cos0");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_bytes_cos1", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->rx_bytes_cos1, "Received bytes count cos1");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_packets_cos1", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->rx_packets_cos1, "Received packets count cos1");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_bytes_cos2", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->rx_bytes_cos2, "Received bytes count cos2");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_packets_cos2", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->rx_packets_cos2, "Received packets count cos2");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_bytes_cos3", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->rx_bytes_cos3, "Received bytes count cos3");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_packets_cos3", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->rx_packets_cos3, "Received packets count cos3");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_bytes_cos4", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->rx_bytes_cos4, "Received bytes count cos4");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_packets_cos4", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->rx_packets_cos4, "Received packets count cos4");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_bytes_cos5", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->rx_bytes_cos5, "Received bytes count cos5");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_packets_cos5", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->rx_packets_cos5, "Received packets count cos5");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_bytes_cos6", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->rx_bytes_cos6, "Received bytes count cos6");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_packets_cos6", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->rx_packets_cos6, "Received packets count cos6");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_bytes_cos7", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->rx_bytes_cos7, "Received bytes count cos7");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_packets_cos7", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->rx_packets_cos7, "Received packets count cos7");

		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_bytes_pri0", CTLFLAG_RD,
		    &softc->rx_bytes_pri[0], "Received bytes count pri0");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_packets_pri0", CTLFLAG_RD,
		    &softc->rx_packets_pri[0], "Received packets count pri0");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_bytes_pri1", CTLFLAG_RD,
		    &softc->rx_bytes_pri[1], "Received bytes count pri1");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_packets_pri1", CTLFLAG_RD,
		    &softc->rx_packets_pri[1], "Received packets count pri1");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_bytes_pri2", CTLFLAG_RD,
		    &softc->rx_bytes_pri[2], "Received bytes count pri2");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_packets_pri2", CTLFLAG_RD,
		    &softc->rx_packets_pri[2], "Received packets count pri2");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_bytes_pri3", CTLFLAG_RD,
		    &softc->rx_bytes_pri[3], "Received bytes count pri3");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_packets_pri3", CTLFLAG_RD,
		    &softc->rx_packets_pri[3], "Received packets count pri3");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_bytes_pri4", CTLFLAG_RD,
		    &softc->rx_bytes_pri[4], "Received bytes count pri4");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_packets_pri4", CTLFLAG_RD,
		    &softc->rx_packets_pri[4], "Received packets count pri4");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_bytes_pri5", CTLFLAG_RD,
		    &softc->rx_bytes_pri[5], "Received bytes count pri5");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_packets_pri5", CTLFLAG_RD,
		    &softc->rx_packets_pri[5], "Received packets count pri5");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_bytes_pri6", CTLFLAG_RD,
		    &softc->rx_bytes_pri[6], "Received bytes count pri6");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_packets_pri6", CTLFLAG_RD,
		    &softc->rx_packets_pri[6], "Received packets count pri6");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_bytes_pri7", CTLFLAG_RD,
		    &softc->rx_bytes_pri[7], "Received bytes count pri7");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_packets_pri7", CTLFLAG_RD,
		    &softc->rx_packets_pri[7], "Received packets count pri7");

		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "pfc_pri0_rx_duration_us", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->pfc_pri0_rx_duration_us, "Time duration in receiving"
		    "between XON to XOFF and XOFF to XON for pri0");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "pfc_pri0_rx_transitions", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->pfc_pri0_rx_transitions, "Num times rx transition"
		    "between XON to XOFF and XOFF to XON for pri0");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "pfc_pri1_rx_duration_us", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->pfc_pri1_rx_duration_us, "Time duration in receiving"
		    "between XON to XOFF and XOFF to XON for pri1");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "pfc_pri1_rx_transitions", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->pfc_pri1_rx_transitions, "Num times rx transition"
		    "between XON to XOFF and XOFF to XON for pri1");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "pfc_pri2_rx_duration_us", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->pfc_pri2_rx_duration_us, "Time duration in receiving"
		    "between XON to XOFF and XOFF to XON for pri2");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "pfc_pri2_rx_transitions", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->pfc_pri2_rx_transitions, "Num times rx transition"
		    "between XON to XOFF and XOFF to XON for pri2");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "pfc_pri3_rx_duration_us", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->pfc_pri3_rx_duration_us, "Time duration in receiving"
		    "between XON to XOFF and XOFF to XON for pri3");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "pfc_pri3_rx_transitions", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->pfc_pri3_rx_transitions, "Num times rx transition"
		    "between XON to XOFF and XOFF to XON for pri3");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "pfc_pri4_rx_duration_us", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->pfc_pri4_rx_duration_us, "Time duration in receiving"
		    "between XON to XOFF and XOFF to XON for pri4");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "pfc_pri4_rx_transitions", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->pfc_pri4_rx_transitions, "Num times rx transition"
		    "between XON to XOFF and XOFF to XON for pri4");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "pfc_pri5_rx_duration_us", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->pfc_pri5_rx_duration_us, "Time duration in receiving"
		    "between XON to XOFF and XOFF to XON for pri5");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "pfc_pri5_rx_transitions", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->pfc_pri5_rx_transitions, "Num times rx transition"
		    "between XON to XOFF and XOFF to XON for pri5");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "pfc_pri6_rx_duration_us", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->pfc_pri6_rx_duration_us, "Time duration in receiving"
		    "between XON to XOFF and XOFF to XON for pri6");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "pfc_pri6_rx_transitions", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->pfc_pri6_rx_transitions, "Num times rx transition"
		    "between XON to XOFF and XOFF to XON for pri6");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "pfc_pri7_rx_duration_us", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->pfc_pri7_rx_duration_us, "Time duration in receiving"
		    "between XON to XOFF and XOFF to XON for pri7");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "pfc_pri7_rx_transitions", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->pfc_pri7_rx_transitions, "Num times rx transition"
		    "between XON to XOFF and XOFF to XON for pri7");

		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_bits", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->rx_bits, "total number of received bits");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_buffer_passed_threshold", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->rx_buffer_passed_threshold, "num of events port"
		    "buffer"
		    "was over 85%");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_pcs_symbol_err", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->rx_pcs_symbol_err, "num of symbol errors wasn't"
		    "corrected by FEC");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_corrected_bits", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->rx_corrected_bits, "num of bits corrected by FEC");

		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_discard_bytes_cos0", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->rx_discard_bytes_cos0, "num of rx discard bytes"
		    "count on cos0");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_discard_packets_cos0", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->rx_discard_packets_cos0, "num of rx discard packets"
		    "count on cos0");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_discard_bytes_cos1", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->rx_discard_bytes_cos1, "num of rx discard bytes"
		    "count on cos1");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_discard_packets_cos1", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->rx_discard_packets_cos1, "num of rx discard packets"
		    "count on cos1");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_discard_bytes_cos2", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->rx_discard_bytes_cos2, "num of rx discard bytes"
		    "count on cos2");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_discard_packets_cos2", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->rx_discard_packets_cos2, "num of rx discard packets"
		    "count on cos2");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_discard_bytes_cos3", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->rx_discard_bytes_cos3, "num of rx discard bytes"
		    "count on cos3");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_discard_packets_cos3", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->rx_discard_packets_cos3, "num of rx discard packets"
		    "count on cos3");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_discard_bytes_cos4", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->rx_discard_bytes_cos4, "num of rx discard bytes"
		    "count on cos4");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_discard_packets_cos4", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->rx_discard_packets_cos4, "num of rx discard packets"
		    "count on cos4");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_discard_bytes_cos5", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->rx_discard_bytes_cos5, "num of rx discard bytes"
		    "count on cos5");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_discard_packets_cos5", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->rx_discard_packets_cos5, "num of rx discard packets"
		    "count on cos5");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_discard_bytes_cos6", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->rx_discard_bytes_cos6, "num of rx discard bytes"
		    "count on cos6");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_discard_packets_cos6", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->rx_discard_packets_cos6, "num of rx discard packets"
		    "count on cos6");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_discard_bytes_cos7", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->rx_discard_bytes_cos7, "num of rx discard bytes"
		    "count on cos7");
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rx_discard_packets_cos7", CTLFLAG_RD,
		    &softc->rx_port_stats_ext->rx_discard_packets_cos7, "num of rx discard packets"
		    "count on cos7");
	}


	return 0;
}

int
bnxt_create_rx_sysctls(struct bnxt_softc *softc, int rxr)
{
	struct sysctl_oid *oid;
	struct ctx_hw_stats *rx_stats = (void *)softc->rx_stats[rxr].idi_vaddr;
	char	name[32];
	char	desc[64];

	sprintf(name, "rxq%d", rxr);
	sprintf(desc, "receive queue %d", rxr);
	oid = SYSCTL_ADD_NODE(&softc->hw_stats,
	    SYSCTL_CHILDREN(softc->hw_stats_oid), OID_AUTO, name,
	    CTLFLAG_RD | CTLFLAG_MPSAFE, 0, desc);
	if (!oid)
		return ENOMEM;

	if (BNXT_CHIP_P5_PLUS(softc))
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "nq_num_ints", CTLFLAG_RD, &softc->nq_rings[rxr].int_count,
		    "Num Interrupts");
	else
		SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rq_num_ints", CTLFLAG_RD, &softc->rx_cp_rings[rxr].int_count,
		    "Num Interrupts");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "ucast_pkts", CTLFLAG_RD, &rx_stats->rx_ucast_pkts,
	    "unicast packets received");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "mcast_pkts", CTLFLAG_RD, &rx_stats->rx_mcast_pkts,
	    "multicast packets received");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "bcast_pkts", CTLFLAG_RD, &rx_stats->rx_bcast_pkts,
	    "broadcast packets received");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "discard_pkts", CTLFLAG_RD,
	    &rx_stats->rx_discard_pkts, "discarded receive packets");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "error_pkts", CTLFLAG_RD, &rx_stats->rx_error_pkts,
	    "Error receive packets");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "ucast_bytes", CTLFLAG_RD, &rx_stats->rx_ucast_bytes,
	    "unicast bytes received");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "mcast_bytes", CTLFLAG_RD, &rx_stats->rx_mcast_bytes,
	    "multicast bytes received");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "bcast_bytes", CTLFLAG_RD, &rx_stats->rx_bcast_bytes,
	    "broadcast bytes received");

	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "tpa_pkts", CTLFLAG_RD, &rx_stats->tpa_pkts,
	    "TPA packets");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "tpa_bytes", CTLFLAG_RD, &rx_stats->tpa_bytes,
	    "TPA bytes");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "tpa_events", CTLFLAG_RD, &rx_stats->tpa_events,
	    "TPA events");
	SYSCTL_ADD_QUAD(&softc->hw_stats, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "tpa_aborts", CTLFLAG_RD, &rx_stats->tpa_aborts,
	    "TPA aborts");

	return 0;
}

static char *bnxt_chip_type[] = {
	"ASIC",
	"FPGA",
	"Palladium",
	"Unknown"
};
#define MAX_CHIP_TYPE 3

static char *bnxt_parse_pkglog(int desired_field, uint8_t *data, size_t datalen)
{
	char    *retval = NULL;
	char    *p;
	char    *value;
	int     field = 0;

	if (datalen < 1)
		return NULL;
	/* null-terminate the log data (removing last '\n'): */
	data[datalen - 1] = 0;
	for (p = data; *p != 0; p++) {
		field = 0;
		retval = NULL;
		while (*p != 0 && *p != '\n') {
			value = p;
			while (*p != 0 && *p != '\t' && *p != '\n')
				p++;
			if (field == desired_field)
				retval = value;
			if (*p != '\t')
				break;
			*p = 0;
			field++;
			p++;
		}
		if (*p == 0)
			break;
		*p = 0;
	}
	return retval;
}

static int
bnxt_package_ver_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct bnxt_softc *softc = arg1;
	struct iflib_dma_info dma_data;
	char *pkglog = NULL;
	char *p;
	char unk[] = "<unknown>";
	char *buf = unk;
	int rc;
	uint16_t ordinal = BNX_DIR_ORDINAL_FIRST;
	uint16_t index;
	uint32_t data_len;

	rc = bnxt_hwrm_nvm_find_dir_entry(softc, BNX_DIR_TYPE_PKG_LOG,
	    &ordinal, BNX_DIR_EXT_NONE, &index, false,
	    HWRM_NVM_FIND_DIR_ENTRY_INPUT_OPT_ORDINAL_EQ,
	    &data_len, NULL, NULL);
	dma_data.idi_vaddr = NULL;
	if (rc == 0 && data_len) {
		rc = iflib_dma_alloc(softc->ctx, data_len, &dma_data,
		    BUS_DMA_NOWAIT);
		if (rc == 0) {
			rc = bnxt_hwrm_nvm_read(softc, index, 0, data_len,
			    &dma_data);
			if (rc == 0) {
				pkglog = dma_data.idi_vaddr;
				p = bnxt_parse_pkglog(BNX_PKG_LOG_FIELD_IDX_PKG_VERSION, pkglog, data_len);
				if (p && *p != 0 && isdigit(*p))
					buf = p;
			}
		} else
			dma_data.idi_vaddr = NULL;
	}

	rc = sysctl_handle_string(oidp, buf, 0, req);
	if (dma_data.idi_vaddr)
		iflib_dma_free(&dma_data);
	return rc;
}

static int
bnxt_hwrm_min_ver_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct bnxt_softc *softc = arg1;
	char buf[16];
	uint8_t	newver[3];
	int rc;

	sprintf(buf, "%hhu.%hhu.%hhu", softc->ver_info->hwrm_min_major,
	    softc->ver_info->hwrm_min_minor, softc->ver_info->hwrm_min_update);

	rc = sysctl_handle_string(oidp, buf, sizeof(buf), req);
	if (rc || req->newptr == NULL)
		return rc;
	if (sscanf(buf, "%hhu.%hhu.%hhu%*c", &newver[0], &newver[1],
	    &newver[2]) != 3)
		return EINVAL;
	softc->ver_info->hwrm_min_major = newver[0];
	softc->ver_info->hwrm_min_minor = newver[1];
	softc->ver_info->hwrm_min_update = newver[2];
	bnxt_check_hwrm_version(softc);

	return rc;
}

int
bnxt_create_ver_sysctls(struct bnxt_softc *softc)
{
	struct bnxt_ver_info *vi = softc->ver_info;
	struct sysctl_oid *oid = vi->ver_oid;

	if (!oid)
		return ENOMEM;

	SYSCTL_ADD_STRING(&vi->ver_ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "hwrm_if", CTLFLAG_RD, vi->hwrm_if_ver, 0,
	    "HWRM interface version");
	SYSCTL_ADD_STRING(&vi->ver_ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "driver_hwrm_if", CTLFLAG_RD, vi->driver_hwrm_if_ver, 0,
	    "HWRM firmware version");
	SYSCTL_ADD_STRING(&vi->ver_ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "mgmt_fw", CTLFLAG_RD, vi->mgmt_fw_ver, 0,
	    "management firmware version");
	SYSCTL_ADD_STRING(&vi->ver_ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "netctrl_fw", CTLFLAG_RD, vi->netctrl_fw_ver, 0,
	    "network control firmware version");
	SYSCTL_ADD_STRING(&vi->ver_ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "roce_fw", CTLFLAG_RD, vi->roce_fw_ver, 0,
	    "RoCE firmware version");
	SYSCTL_ADD_STRING(&vi->ver_ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "fw_ver", CTLFLAG_RD, vi->fw_ver_str, 0,
	    "Firmware version");
	SYSCTL_ADD_STRING(&vi->ver_ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "phy", CTLFLAG_RD, vi->phy_ver, 0,
	    "PHY version");
	SYSCTL_ADD_STRING(&vi->ver_ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "hwrm_fw_name", CTLFLAG_RD, vi->hwrm_fw_name, 0,
	    "HWRM firmware name");
	SYSCTL_ADD_STRING(&vi->ver_ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "mgmt_fw_name", CTLFLAG_RD, vi->mgmt_fw_name, 0,
	    "management firmware name");
	SYSCTL_ADD_STRING(&vi->ver_ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "netctrl_fw_name", CTLFLAG_RD, vi->netctrl_fw_name, 0,
	    "network control firmware name");
	SYSCTL_ADD_STRING(&vi->ver_ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "roce_fw_name", CTLFLAG_RD, vi->roce_fw_name, 0,
	    "RoCE firmware name");
	SYSCTL_ADD_STRING(&vi->ver_ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "phy_vendor", CTLFLAG_RD, vi->phy_vendor, 0,
	    "PHY vendor name");
	SYSCTL_ADD_STRING(&vi->ver_ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "phy_partnumber", CTLFLAG_RD, vi->phy_partnumber, 0,
	    "PHY vendor part number");
	SYSCTL_ADD_U16(&vi->ver_ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "chip_num", CTLFLAG_RD, &vi->chip_num, 0, "chip number");
	SYSCTL_ADD_U8(&vi->ver_ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "chip_rev", CTLFLAG_RD, &vi->chip_rev, 0, "chip revision");
	SYSCTL_ADD_U8(&vi->ver_ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "chip_metal", CTLFLAG_RD, &vi->chip_metal, 0, "chip metal number");
	SYSCTL_ADD_U8(&vi->ver_ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "chip_bond_id", CTLFLAG_RD, &vi->chip_bond_id, 0,
	    "chip bond id");
	SYSCTL_ADD_STRING(&vi->ver_ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "chip_type", CTLFLAG_RD, vi->chip_type > MAX_CHIP_TYPE ?
	    bnxt_chip_type[MAX_CHIP_TYPE] : bnxt_chip_type[vi->chip_type], 0,
	    "RoCE firmware name");
	SYSCTL_ADD_PROC(&vi->ver_ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "package_ver", CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    softc, 0, bnxt_package_ver_sysctl, "A",
	    "currently installed package version");
	SYSCTL_ADD_PROC(&vi->ver_ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "hwrm_min_ver", CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
	    softc, 0, bnxt_hwrm_min_ver_sysctl, "A",
	    "minimum hwrm API vesion to support");

	return 0;
}

int
bnxt_create_nvram_sysctls(struct bnxt_nvram_info *ni)
{
	struct sysctl_oid *oid = ni->nvm_oid;

	if (!oid)
		return ENOMEM;

	SYSCTL_ADD_U16(&ni->nvm_ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "mfg_id", CTLFLAG_RD, &ni->mfg_id, 0, "manufacturer id");
	SYSCTL_ADD_U16(&ni->nvm_ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "device_id", CTLFLAG_RD, &ni->device_id, 0, "device id");
	SYSCTL_ADD_U32(&ni->nvm_ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "sector_size", CTLFLAG_RD, &ni->sector_size, 0, "sector size");
	SYSCTL_ADD_U32(&ni->nvm_ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "size", CTLFLAG_RD, &ni->size, 0, "nvram total size");
	SYSCTL_ADD_U32(&ni->nvm_ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "reserved_size", CTLFLAG_RD, &ni->reserved_size, 0,
	    "total reserved space");
	SYSCTL_ADD_U32(&ni->nvm_ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "available_size", CTLFLAG_RD, &ni->available_size, 0,
	    "total available space");

	return 0;
}

static int
bnxt_rss_key_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct bnxt_softc *softc = arg1;
	char buf[HW_HASH_KEY_SIZE*2+1] = {0};
	char *p;
	int i;
	int rc;

	for (p = buf, i=0; i<HW_HASH_KEY_SIZE; i++)
		p += sprintf(p, "%02x", softc->vnic_info.rss_hash_key[i]);

	rc = sysctl_handle_string(oidp, buf, sizeof(buf), req);
	if (rc || req->newptr == NULL)
		return rc;

	if (strspn(buf, "0123456789abcdefABCDEF") != (HW_HASH_KEY_SIZE * 2))
		return EINVAL;

	for (p = buf, i=0; i<HW_HASH_KEY_SIZE; i++) {
		if (sscanf(p, "%02hhx", &softc->vnic_info.rss_hash_key[i]) != 1)
			return EINVAL;
		p += 2;
	}

	if (if_getdrvflags(iflib_get_ifp(softc->ctx)) & IFF_DRV_RUNNING)
		bnxt_hwrm_rss_cfg(softc, &softc->vnic_info,
		    softc->vnic_info.rss_hash_type);

	return rc;
}

static const char *bnxt_hash_types[] = {"ipv4", "tcp_ipv4", "udp_ipv4", "ipv6",
    "tcp_ipv6", "udp_ipv6", NULL};

static int bnxt_get_rss_type_str_bit(char *str)
{
	int i;

	for (i=0; bnxt_hash_types[i]; i++)
		if (strcmp(bnxt_hash_types[i], str) == 0)
			return i;

	return -1;
}

static int
bnxt_rss_type_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct bnxt_softc *softc = arg1;
	char buf[256] = {0};
	char *p;
	char *next;
	int rc;
	int type;
	int bit;

	for (type = softc->vnic_info.rss_hash_type; type;
	    type &= ~(1<<bit)) {
		bit = ffs(type) - 1;
		if (bit >= sizeof(bnxt_hash_types) / sizeof(const char *))
			continue;
		if (type != softc->vnic_info.rss_hash_type)
			strcat(buf, ",");
		strcat(buf, bnxt_hash_types[bit]);
	}

	rc = sysctl_handle_string(oidp, buf, sizeof(buf), req);
	if (rc || req->newptr == NULL)
		return rc;

	for (type = 0, next = buf, p = strsep(&next, " ,"); p;
	    p = strsep(&next, " ,")) {
		bit = bnxt_get_rss_type_str_bit(p);
		if (bit == -1)
			return EINVAL;
		type |= 1<<bit;
	}
	if (type != softc->vnic_info.rss_hash_type) {
		softc->vnic_info.rss_hash_type = type;
		if (if_getdrvflags(iflib_get_ifp(softc->ctx)) & IFF_DRV_RUNNING)
			bnxt_hwrm_rss_cfg(softc, &softc->vnic_info,
			    softc->vnic_info.rss_hash_type);
	}

	return rc;
}

static int
bnxt_rx_stall_sysctl(SYSCTL_HANDLER_ARGS) {
	struct bnxt_softc *softc = arg1;
	int rc;
	int val;

	if (softc == NULL)
		return EBUSY;

	val = (bool)(softc->vnic_info.flags & BNXT_VNIC_FLAG_BD_STALL);
	rc = sysctl_handle_int(oidp, &val, 0, req);
	if (rc || !req->newptr)
		return rc;

	if (val)
		softc->vnic_info.flags |= BNXT_VNIC_FLAG_BD_STALL;
	else
		softc->vnic_info.flags &= ~BNXT_VNIC_FLAG_BD_STALL;

	if (if_getdrvflags(iflib_get_ifp(softc->ctx)) & IFF_DRV_RUNNING)
		rc = bnxt_hwrm_vnic_cfg(softc, &softc->vnic_info);

	return rc;
}

static int
bnxt_vlan_strip_sysctl(SYSCTL_HANDLER_ARGS) {
	struct bnxt_softc *softc = arg1;
	int rc;
	int val;

	if (softc == NULL)
		return EBUSY;

	val = (bool)(softc->vnic_info.flags & BNXT_VNIC_FLAG_VLAN_STRIP);
	rc = sysctl_handle_int(oidp, &val, 0, req);
	if (rc || !req->newptr)
		return rc;

	if (val)
		softc->vnic_info.flags |= BNXT_VNIC_FLAG_VLAN_STRIP;
	else
		softc->vnic_info.flags &= ~BNXT_VNIC_FLAG_VLAN_STRIP;

	if (if_getdrvflags(iflib_get_ifp(softc->ctx)) & IFF_DRV_RUNNING)
		rc = bnxt_hwrm_vnic_cfg(softc, &softc->vnic_info);

	return rc;
}

static int
bnxt_set_coal_rx_usecs(SYSCTL_HANDLER_ARGS) {
	struct bnxt_softc *softc = arg1;
	int rc;
	int val;

	if (softc == NULL)
		return EBUSY;

	val = softc->rx_coal_usecs;
	rc = sysctl_handle_int(oidp, &val, 0, req);
	if (rc || !req->newptr)
		return rc;

	softc->rx_coal_usecs = val;
	rc = bnxt_hwrm_set_coal(softc);

	return rc;
}

static int
bnxt_set_coal_rx_frames(SYSCTL_HANDLER_ARGS) {
	struct bnxt_softc *softc = arg1;
	int rc;
	int val;

	if (softc == NULL)
		return EBUSY;

	val = softc->rx_coal_frames;
	rc = sysctl_handle_int(oidp, &val, 0, req);
	if (rc || !req->newptr)
		return rc;

	softc->rx_coal_frames = val;
	rc = bnxt_hwrm_set_coal(softc);

	return rc;
}

static int
bnxt_set_coal_rx_usecs_irq(SYSCTL_HANDLER_ARGS) {
	struct bnxt_softc *softc = arg1;
	int rc;
	int val;

	if (softc == NULL)
		return EBUSY;

	val = softc->rx_coal_usecs_irq;
	rc = sysctl_handle_int(oidp, &val, 0, req);
	if (rc || !req->newptr)
		return rc;

	softc->rx_coal_usecs_irq = val;
	rc = bnxt_hwrm_set_coal(softc);

	return rc;
}

static int
bnxt_set_coal_rx_frames_irq(SYSCTL_HANDLER_ARGS) {
	struct bnxt_softc *softc = arg1;
	int rc;
	int val;

	if (softc == NULL)
		return EBUSY;

	val = softc->rx_coal_frames_irq;
	rc = sysctl_handle_int(oidp, &val, 0, req);
	if (rc || !req->newptr)
		return rc;

	softc->rx_coal_frames_irq = val;
	rc = bnxt_hwrm_set_coal(softc);

	return rc;
}

static int
bnxt_set_coal_tx_usecs(SYSCTL_HANDLER_ARGS) {
	struct bnxt_softc *softc = arg1;
	int rc;
	int val;

	if (softc == NULL)
		return EBUSY;

	val = softc->tx_coal_usecs;
	rc = sysctl_handle_int(oidp, &val, 0, req);
	if (rc || !req->newptr)
		return rc;

	softc->tx_coal_usecs = val;
	rc = bnxt_hwrm_set_coal(softc);

	return rc;
}

static int
bnxt_set_coal_tx_frames(SYSCTL_HANDLER_ARGS) {
	struct bnxt_softc *softc = arg1;
	int rc;
	int val;

	if (softc == NULL)
		return EBUSY;

	val = softc->tx_coal_frames;
	rc = sysctl_handle_int(oidp, &val, 0, req);
	if (rc || !req->newptr)
		return rc;

	softc->tx_coal_frames = val;
	rc = bnxt_hwrm_set_coal(softc);

	return rc;
}

static int
bnxt_set_coal_tx_usecs_irq(SYSCTL_HANDLER_ARGS) {
	struct bnxt_softc *softc = arg1;
	int rc;
	int val;

	if (softc == NULL)
		return EBUSY;

	val = softc->tx_coal_usecs_irq;
	rc = sysctl_handle_int(oidp, &val, 0, req);
	if (rc || !req->newptr)
		return rc;

	softc->tx_coal_usecs_irq = val;
	rc = bnxt_hwrm_set_coal(softc);

	return rc;
}

static int
bnxt_set_coal_tx_frames_irq(SYSCTL_HANDLER_ARGS) {
	struct bnxt_softc *softc = arg1;
	int rc;
	int val;

	if (softc == NULL)
		return EBUSY;

	val = softc->tx_coal_frames_irq;
	rc = sysctl_handle_int(oidp, &val, 0, req);
	if (rc || !req->newptr)
		return rc;

	softc->tx_coal_frames_irq = val;
	rc = bnxt_hwrm_set_coal(softc);

	return rc;
}

static
void simulate_reset(struct bnxt_softc *bp, char *fwcli_string)
{
	struct hwrm_dbg_fw_cli_input req = {0};
	int rc = 0;

	bnxt_hwrm_cmd_hdr_init(bp, &req, HWRM_DBG_FW_CLI);
	req.cmpl_ring = -1;
	req.target_id = -1;
	req.cli_cmd_len = strlen(fwcli_string);
	req.host_buf_len = 64 * 1024;
	strcpy((char *)req.cli_cmd, fwcli_string);

	BNXT_HWRM_LOCK(bp);
	rc = _hwrm_send_message(bp, &req, sizeof(req));
	if (rc) {
		device_printf(bp->dev, " Manual FW fault failed, rc:%x\n", rc);
	}
	BNXT_HWRM_UNLOCK(bp);
}

static int
bnxt_reset_ctrl(SYSCTL_HANDLER_ARGS) {
	struct bnxt_softc *softc = arg1;
	int rc = 0;
	char buf[50] = {0};

	if (softc == NULL)
		return EBUSY;

	rc = sysctl_handle_string(oidp, buf, sizeof(buf), req);
	if (rc || req->newptr == NULL)
		return rc;

	if (BNXT_CHIP_P5_PLUS(softc))
		simulate_reset(softc, buf);

	return rc;
}

int
bnxt_create_config_sysctls_pre(struct bnxt_softc *softc)
{
	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(softc->dev);
	struct sysctl_oid_list *children;

	children = SYSCTL_CHILDREN(device_get_sysctl_tree(softc->dev));

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "rss_key",
	    CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE, softc, 0,
	    bnxt_rss_key_sysctl, "A", "RSS key");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "rss_type",
	    CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE, softc, 0,
	    bnxt_rss_type_sysctl, "A", "RSS type bits");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "rx_stall",
	    CTLTYPE_INT | CTLFLAG_RWTUN | CTLFLAG_MPSAFE, softc, 0,
	    bnxt_rx_stall_sysctl, "I",
	    "buffer rx packets in hardware until the host posts new buffers");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "vlan_strip",
	    CTLTYPE_INT | CTLFLAG_RWTUN | CTLFLAG_MPSAFE, softc, 0,
	    bnxt_vlan_strip_sysctl, "I", "strip VLAN tag in the RX path");
	SYSCTL_ADD_CONST_STRING(ctx, children, OID_AUTO, "if_name", CTLFLAG_RD,
		if_name(iflib_get_ifp(softc->ctx)), "interface name");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "intr_coal_rx_usecs",
	    CTLTYPE_INT | CTLFLAG_RWTUN | CTLFLAG_MPSAFE, softc, 0,
	    bnxt_set_coal_rx_usecs, "I", "interrupt coalescing Rx Usecs");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "intr_coal_rx_frames",
	    CTLTYPE_INT | CTLFLAG_RWTUN | CTLFLAG_MPSAFE, softc, 0,
	    bnxt_set_coal_rx_frames, "I", "interrupt coalescing Rx Frames");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "intr_coal_rx_usecs_irq",
	    CTLTYPE_INT | CTLFLAG_RWTUN | CTLFLAG_MPSAFE, softc, 0,
	    bnxt_set_coal_rx_usecs_irq, "I",
	    "interrupt coalescing Rx Usecs IRQ");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "intr_coal_rx_frames_irq",
	    CTLTYPE_INT | CTLFLAG_RWTUN | CTLFLAG_MPSAFE, softc, 0,
	    bnxt_set_coal_rx_frames_irq, "I",
	    "interrupt coalescing Rx Frames IRQ");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "intr_coal_tx_usecs",
	    CTLTYPE_INT | CTLFLAG_RWTUN | CTLFLAG_MPSAFE, softc, 0,
	    bnxt_set_coal_tx_usecs, "I", "interrupt coalescing Tx Usces");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "intr_coal_tx_frames",
	    CTLTYPE_INT | CTLFLAG_RWTUN | CTLFLAG_MPSAFE, softc, 0,
	    bnxt_set_coal_tx_frames, "I", "interrupt coalescing Tx Frames");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "intr_coal_tx_usecs_irq",
	    CTLTYPE_INT | CTLFLAG_RWTUN | CTLFLAG_MPSAFE, softc, 0,
	    bnxt_set_coal_tx_usecs_irq, "I",
	    "interrupt coalescing Tx Usecs IRQ");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "intr_coal_tx_frames_irq",
	    CTLTYPE_INT | CTLFLAG_RWTUN | CTLFLAG_MPSAFE, softc, 0,
	    bnxt_set_coal_tx_frames_irq, "I",
	    "interrupt coalescing Tx Frames IRQ");
	SYSCTL_ADD_U32(ctx, children, OID_AUTO, "flags", CTLFLAG_RD,
		&softc->flags, 0, "flags");
	SYSCTL_ADD_U64(ctx, children, OID_AUTO, "fw_cap", CTLFLAG_RD,
		&softc->fw_cap, 0, "FW caps");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO,
	    "reset_ctrl", CTLTYPE_STRING | CTLFLAG_RWTUN, softc,
	    0, bnxt_reset_ctrl, "A",
	    "Issue controller reset: 0 / 1");
	return 0;
}

#define BNXT_HW_LRO_FN(fn_name, arg)			                   \
static int						                   \
fn_name(SYSCTL_HANDLER_ARGS) {				                   \
	struct bnxt_softc *softc = arg1;		                   \
	int rc;						                   \
	int val;					                   \
							                   \
	if (softc == NULL)				                   \
		return EBUSY;				                   \
							                   \
	val = softc->hw_lro.arg;			                   \
	rc = sysctl_handle_int(oidp, &val, 0, req);	                   \
	if (rc || !req->newptr)				                   \
		return rc;				                   \
							                   \
	if ((if_getdrvflags(iflib_get_ifp(softc->ctx)) & IFF_DRV_RUNNING)) \
		return EBUSY;				                   \
							                   \
	if (!(softc->flags & BNXT_FLAG_TPA))				   \
		return EINVAL;						   \
							                   \
	softc->hw_lro.arg = val;			                   \
	bnxt_validate_hw_lro_settings(softc);		                   \
	rc = bnxt_hwrm_vnic_tpa_cfg(softc);		                   \
							                   \
	return rc;					                   \
}

BNXT_HW_LRO_FN(bnxt_hw_lro_enable_disable, enable)
BNXT_HW_LRO_FN(bnxt_hw_lro_set_mode, is_mode_gro)
BNXT_HW_LRO_FN(bnxt_hw_lro_set_max_agg_segs, max_agg_segs)
BNXT_HW_LRO_FN(bnxt_hw_lro_set_max_aggs, max_aggs)
BNXT_HW_LRO_FN(bnxt_hw_lro_set_min_agg_len, min_agg_len)

#define BNXT_FLOW_CTRL_FN(fn_name, arg)			                   \
static int						                   \
fn_name(SYSCTL_HANDLER_ARGS) {				                   \
	struct bnxt_softc *softc = arg1;		                   \
	int rc;						                   \
	int val;					                   \
							                   \
	if (softc == NULL)				                   \
		return EBUSY;				                   \
							                   \
	val = softc->link_info.flow_ctrl.arg;			           \
	rc = sysctl_handle_int(oidp, &val, 0, req);	                   \
	if (rc || !req->newptr)				                   \
		return rc;				                   \
							                   \
	if (val)					                   \
	   	val = 1; 				                   \
	        					                   \
	if (softc->link_info.flow_ctrl.arg != val) {		           \
		softc->link_info.flow_ctrl.arg = val;		           \
		rc = bnxt_hwrm_set_link_setting(softc, true, false, false);\
		rc = bnxt_hwrm_port_phy_qcfg(softc);			   \
	}						                   \
							                   \
	return rc;					                   \
}

BNXT_FLOW_CTRL_FN(bnxt_flow_ctrl_tx, tx)
BNXT_FLOW_CTRL_FN(bnxt_flow_ctrl_rx, rx)
BNXT_FLOW_CTRL_FN(bnxt_flow_ctrl_autoneg, autoneg)
int
bnxt_create_pause_fc_sysctls(struct bnxt_softc *softc)
{
	struct sysctl_oid *oid = softc->flow_ctrl_oid;

	if (!oid)
		return ENOMEM;

	SYSCTL_ADD_PROC(&softc->flow_ctrl_ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "tx", CTLTYPE_INT | CTLFLAG_RWTUN | CTLFLAG_MPSAFE, softc, 0,
	    bnxt_flow_ctrl_tx, "A", "Enable or Disable Tx Flow Ctrl: 0 / 1");

	SYSCTL_ADD_PROC(&softc->flow_ctrl_ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "rx", CTLTYPE_INT | CTLFLAG_RWTUN | CTLFLAG_MPSAFE, softc, 0,
	    bnxt_flow_ctrl_rx, "A", "Enable or Disable Tx Flow Ctrl: 0 / 1");

	SYSCTL_ADD_PROC(&softc->flow_ctrl_ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "autoneg", CTLTYPE_INT | CTLFLAG_RWTUN | CTLFLAG_MPSAFE, softc,
	    0, bnxt_flow_ctrl_autoneg, "A",
	    "Enable or Disable Autoneg Flow Ctrl: 0 / 1");

	return 0;
}

int
bnxt_create_hw_lro_sysctls(struct bnxt_softc *softc)
{
	struct sysctl_oid *oid = softc->hw_lro_oid;

	if (!oid)
		return ENOMEM;

	SYSCTL_ADD_PROC(&softc->hw_lro_ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "enable", CTLTYPE_INT | CTLFLAG_RWTUN | CTLFLAG_MPSAFE, softc,
	    0, bnxt_hw_lro_enable_disable, "A",
	    "Enable or Disable HW LRO: 0 / 1");

	SYSCTL_ADD_PROC(&softc->hw_lro_ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "gro_mode", CTLTYPE_INT | CTLFLAG_RWTUN | CTLFLAG_MPSAFE, softc,
	    0, bnxt_hw_lro_set_mode, "A",
	    "Set mode: 1 = GRO mode, 0 = RSC mode");

	SYSCTL_ADD_PROC(&softc->hw_lro_ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "max_agg_segs", CTLTYPE_INT | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
	    softc, 0, bnxt_hw_lro_set_max_agg_segs, "A",
	    "Set Max Agg Seg Value (unit is Log2): "
	    "0 (= 1 seg) / 1 (= 2 segs) /  ... / 31 (= 2^31 segs)");

        SYSCTL_ADD_PROC(&softc->hw_lro_ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "max_aggs", CTLTYPE_INT | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
	    softc, 0, bnxt_hw_lro_set_max_aggs, "A",
	    "Set Max Aggs Value (unit is Log2): "
	    "0 (= 1 agg) / 1 (= 2 aggs) /  ... / 7 (= 2^7 segs)");

	SYSCTL_ADD_PROC(&softc->hw_lro_ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "min_agg_len", CTLTYPE_INT | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
	    softc, 0, bnxt_hw_lro_set_min_agg_len, "A",
	    "Min Agg Len: 1 to 9000");

	return 0;
}

static int
bnxt_dcb_dcbx_cap(SYSCTL_HANDLER_ARGS)
{
	struct bnxt_softc *softc = arg1;
	int val;
	int rc;

	val = bnxt_dcb_getdcbx(softc);
	rc = sysctl_handle_int(oidp, &val, 0, req);
	if (rc || !req->newptr)
		return rc;

	bnxt_dcb_setdcbx(softc, val);

	return rc;
}

static char
bnxt_ets_tsa_to_str(struct bnxt_softc *softc, uint32_t tc)
{
	switch (softc->ieee_ets->tc_tsa[tc]) {
	case BNXT_IEEE_8021QAZ_TSA_STRICT:
		return 's';
	case BNXT_IEEE_8021QAZ_TSA_ETS:
		return 'e';
	default:
		return 'X';

	}
}

static uint32_t
bnxt_ets_str_to_tsa(char tsa_str)
{
	switch (tsa_str) {
	case 's':
		return BNXT_IEEE_8021QAZ_TSA_STRICT;
	case 'e':
		return BNXT_IEEE_8021QAZ_TSA_ETS;
	default:
		return -1;
	}
}

static int
bnxt_ets_get_val(struct bnxt_softc *softc, uint32_t type, uint32_t tc)
{
	switch (type) {
	case BNXT_TYPE_ETS_TSA:
		if (softc->ieee_ets)
			return softc->ieee_ets->tc_tsa[tc];
		break;
	case BNXT_TYPE_ETS_PRI2TC:
		if (softc->ieee_ets)
			return softc->ieee_ets->prio_tc[tc];
		break;
	case BNXT_TYPE_ETS_TCBW:
		if (softc->ieee_ets)
			return softc->ieee_ets->tc_tx_bw[tc];
		break;
	default:
		break;
	}

	return -1;
}

static void
bnxt_pfc_get_string(struct bnxt_softc *softc, char *buf, struct bnxt_ieee_pfc *pfc)
{
	uint32_t i;
	bool found = false;

	for (i = 0; i < BNXT_IEEE_8021QAZ_MAX_TCS; i++) {
		if (pfc->pfc_en & (1 << i)) {
			if (found)
				buf += sprintf(buf, ", ");
			buf += sprintf(buf, "%d", i);
			found = true;
		}
	}

	if (!found)
		buf += sprintf(buf, "none");
}

static const char *bnxt_get_tlv_selector_str(uint8_t selector)
{
	switch (selector) {
	case BNXT_IEEE_8021QAZ_APP_SEL_ETHERTYPE:
		return "Ethertype";
	case BNXT_IEEE_8021QAZ_APP_SEL_DGRAM:
		return "UDP or DCCP";
	case BNXT_IEEE_8021QAZ_APP_SEL_DSCP:
		return "DSCP";
	default:
		return "Unknown";
	}
}

static void
bnxt_app_tlv_get_string(struct sbuf *sb, struct bnxt_dcb_app *app, int num)
{
	int i;

	if (num == 0) {
		sbuf_printf(sb, " None");
		return;
	}

	sbuf_putc(sb, '\n');
	for (i = 0; i < num; i++) {
		sbuf_printf(sb, "\tAPP#%0d:\tpri: %d,\tSel: %d,\t%s: %d\n",
		    i,
		    app[i].priority,
		    app[i].selector,
		    bnxt_get_tlv_selector_str(app[i].selector),
		    app[i].protocol);
	}
}

static void
bnxt_ets_get_string(struct bnxt_softc *softc, char *buf)
{
	uint32_t type, i;

	type = BNXT_TYPE_ETS_TSA;
	for (type = 0; type < BNXT_TYPE_ETS_MAX; type++) {
		for (i = 0; i < BNXT_IEEE_8021QAZ_MAX_TCS; i++) {
			if (i == 0)
				buf += sprintf(buf, "%s:", BNXT_ETS_TYPE_STR[type]);

			if (!softc->ieee_ets)
				buf += sprintf(buf, "x");
			else if (type == BNXT_TYPE_ETS_TSA)
				buf += sprintf(buf, "%c", bnxt_ets_tsa_to_str(softc, i));
			else
				buf += sprintf(buf, "%d", bnxt_ets_get_val(softc, type, i));

			if (i != BNXT_IEEE_8021QAZ_MAX_TCS - 1)
				buf += sprintf(buf, ",");
		}
		if (type != BNXT_TYPE_ETS_MAX - 1)
			buf += sprintf(buf, "#");
	}
}

static int
bnxt_dcb_list_app(SYSCTL_HANDLER_ARGS)
{
	struct sbuf sb;
	struct bnxt_dcb_app app[128] = {0};
	struct bnxt_softc *softc = arg1;
	int rc, num_inputs = 0;

	sbuf_new_for_sysctl(&sb, NULL, 128, req);
	bnxt_dcb_ieee_listapp(softc, app, nitems(app), &num_inputs);
	bnxt_app_tlv_get_string(&sb, app, num_inputs);
	rc = sbuf_finish(&sb);
	sbuf_delete(&sb);
	return rc;
}

static int
bnxt_dcb_del_app(SYSCTL_HANDLER_ARGS)
{
	struct bnxt_softc *softc = arg1;
	struct bnxt_dcb_app app = {0};
	char buf[256] = {0};
	int rc, num_inputs;

	rc = sysctl_handle_string(oidp, buf, sizeof(buf), req);
	if (rc || req->newptr == NULL)
		return rc;

	num_inputs = sscanf(buf, "%hhu,%hhu,%hd", &app.priority, &app.selector, &app.protocol);

	if (num_inputs != 3) {
		device_printf(softc->dev,
			      "Invalid app tlv syntax, inputs = %d\n", num_inputs);
		return EINVAL;
	}

	bnxt_dcb_ieee_delapp(softc, &app);

	return rc;
}
static int
bnxt_dcb_set_app(SYSCTL_HANDLER_ARGS)
{
	struct bnxt_softc *softc = arg1;
	struct bnxt_dcb_app app = {0};
	char buf[256] = {0};
	int rc, num_inputs;

	rc = sysctl_handle_string(oidp, buf, sizeof(buf), req);
	if (rc || req->newptr == NULL)
		return rc;

	num_inputs = sscanf(buf, "%hhu,%hhu,%hd", &app.priority, &app.selector, &app.protocol);

	if (num_inputs != 3) {
		device_printf(softc->dev,
			      "Invalid app tlv syntax, inputs = %d\n", num_inputs);
		return EINVAL;
	}

	bnxt_dcb_ieee_setapp(softc, &app);

	return rc;
}

static int
bnxt_dcb_pfc(SYSCTL_HANDLER_ARGS)
{
	struct bnxt_softc *softc = arg1;
	struct bnxt_ieee_pfc pfc = {0};
	int rc, i, num_inputs;
	char buf[256] = {0};
	int pri_mask = 0;
	char pri[8];

	rc = bnxt_dcb_ieee_getpfc(softc, &pfc);
	if (!rc)
		bnxt_pfc_get_string(softc, buf, &pfc);
	else
		sprintf(buf, "## getpfc failed with error %d ##", rc);

	rc = sysctl_handle_string(oidp, buf, sizeof(buf), req);
	if (rc || req->newptr == NULL)
		return rc;

	/* Check for 'none' string first */
	if (sscanf(buf,  "%s", buf) == 1) {
		if (strncmp(buf, "none", 8) == 0) {
			goto configure;
		}
	}
	num_inputs = sscanf(buf, "%hhu,%hhu,%hhu,%hhu,%hhu,%hhu,%hhu,%hhu",
			    &pri[0], &pri[1], &pri[2], &pri[3], &pri[4],
			    &pri[5], &pri[6], &pri[7]);

	if (num_inputs < 1 || num_inputs > 8) {
		device_printf(softc->dev,
			      "Invalid pfc syntax, inputs = %d\n", num_inputs);
		return EINVAL;
	}

	for (i = 0; i < num_inputs; i++) {
		if (pri[i] > 7 || pri[i] < 0) {
			device_printf(softc->dev,
				      "Invalid priority %d. Valid priorties are "
				      "from 0 to 7 and string \"none\".\n", pri[i]);
			return EINVAL;
		}

		pri_mask |= (1 << pri[i]) & 0xFF;
	}

configure:
	pfc.pfc_en = pri_mask;
	rc = bnxt_dcb_ieee_setpfc(softc, &pfc);
	if (rc)
		device_printf(softc->dev,
			      "setpfc failed with status %d\n", rc);
	return rc;
}

static int
bnxt_dcb_ets(SYSCTL_HANDLER_ARGS)
{
	struct bnxt_softc *softc = arg1;
	struct bnxt_ieee_ets ets = {0};
	int rc = 0, i, num_inputs;
	char buf[256] = {0};
	char tsa[8];

	rc = bnxt_dcb_ieee_getets(softc, &ets);
	if (!rc)
		bnxt_ets_get_string(softc, buf);
	else
		sprintf(buf, "## getets failed with error %d ##", rc);

	rc = sysctl_handle_string(oidp, buf, sizeof(buf), req);
	if (rc || req->newptr == NULL)
		return rc;

	num_inputs = sscanf(buf,  "tsa:%c,%c,%c,%c,%c,%c,%c,%c#"
			    "pri2tc:%hhu,%hhu,%hhu,%hhu,%hhu,%hhu,%hhu,%hhu#"
			    "tcbw:%hhu,%hhu,%hhu,%hhu,%hhu,%hhu,%hhu,%hhu",
			    &tsa[0], &tsa[1], &tsa[2], &tsa[3], &tsa[4], &tsa[5], &tsa[6], &tsa[7],
			    &ets.prio_tc[0], &ets.prio_tc[1], &ets.prio_tc[2], &ets.prio_tc[3],
			    &ets.prio_tc[4], &ets.prio_tc[5], &ets.prio_tc[6], &ets.prio_tc[7],
			    &ets.tc_tx_bw[0], &ets.tc_tx_bw[1], &ets.tc_tx_bw[2], &ets.tc_tx_bw[3],
			    &ets.tc_tx_bw[4], &ets.tc_tx_bw[5], &ets.tc_tx_bw[6], &ets.tc_tx_bw[7]);

	if (num_inputs != 24)
		return EINVAL;

	for ( i= 0; i < 8; i++)
		ets.tc_tsa[i] = bnxt_ets_str_to_tsa(tsa[i]);

	rc = bnxt_dcb_ieee_setets(softc, &ets);

	return rc;
}

int
bnxt_create_dcb_sysctls(struct bnxt_softc *softc)
{
	struct sysctl_oid *oid = softc->dcb_oid;

	if (!oid)
		return ENOMEM;

	SYSCTL_ADD_PROC(&softc->dcb_ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "dcbx_cap", CTLTYPE_INT | CTLFLAG_RWTUN, softc,
	    0, bnxt_dcb_dcbx_cap, "A",
	    "Enable DCB Capability Exchange Protocol (DCBX) capabilities");

	SYSCTL_ADD_PROC(&softc->dcb_ctx, SYSCTL_CHILDREN(oid), OID_AUTO, "ets",
	    CTLTYPE_STRING | CTLFLAG_RWTUN, softc, 0,
	    bnxt_dcb_ets, "A", "Enhanced Transmission Selection (ETS)");

	SYSCTL_ADD_PROC(&softc->dcb_ctx, SYSCTL_CHILDREN(oid), OID_AUTO, "pfc",
	    CTLTYPE_STRING | CTLFLAG_RWTUN, softc, 0,
	    bnxt_dcb_pfc, "A", "Enhanced Transmission Selection (ETS)");

	SYSCTL_ADD_PROC(&softc->dcb_ctx, SYSCTL_CHILDREN(oid), OID_AUTO, "set_apptlv",
	    CTLTYPE_STRING | CTLFLAG_WR, softc, 0,
	    bnxt_dcb_set_app, "A", "Set App TLV");

	SYSCTL_ADD_PROC(&softc->dcb_ctx, SYSCTL_CHILDREN(oid), OID_AUTO, "del_apptlv",
	    CTLTYPE_STRING | CTLFLAG_WR, softc, 0,
	    bnxt_dcb_del_app, "A", "Delete App TLV");

	SYSCTL_ADD_PROC(&softc->dcb_ctx, SYSCTL_CHILDREN(oid), OID_AUTO, "list_apptlv",
	    CTLTYPE_STRING | CTLFLAG_RD, softc, 0,
	    bnxt_dcb_list_app, "A", "List all App TLVs");

	return 0;
}

int
bnxt_create_config_sysctls_post(struct bnxt_softc *softc)
{
	/* Nothing for now, meant for future expansion */
	return 0;
}
