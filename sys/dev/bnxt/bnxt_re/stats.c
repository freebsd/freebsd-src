/*
 * Copyright (c) 2015-2024, Broadcom. All rights reserved.  The term
 * Broadcom refers to Broadcom Limited and/or its subsidiaries.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Description: statistics related functions
 */

#include "bnxt_re.h"
#include "bnxt.h"

int bnxt_re_get_flow_stats_from_service_pf(struct bnxt_re_dev *rdev,
					   struct bnxt_re_flow_counters *stats,
					   struct bnxt_qplib_query_stats_info *sinfo)
{
	struct hwrm_cfa_flow_stats_output resp = {};
	struct hwrm_cfa_flow_stats_input req = {};
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	struct bnxt_fw_msg fw_msg = {};
	u16 target_id;
	int rc = 0;

	if (sinfo->function_id == 0xFFFFFFFF)
		target_id = -1;
	else
		target_id = sinfo->function_id + 1;

	/* Issue HWRM cmd to read flow counters for CNP tx and rx */
	bnxt_re_init_hwrm_hdr(rdev, (void *)&req, HWRM_CFA_FLOW_STATS, -1, target_id);
	req.num_flows = cpu_to_le16(6);
	req.flow_handle_0 = cpu_to_le16(HWRM_CFA_FLOW_INFO_INPUT_FLOW_HANDLE_CNP_CNT);
	req.flow_handle_1 = cpu_to_le16(HWRM_CFA_FLOW_INFO_INPUT_FLOW_HANDLE_CNP_CNT |
					HWRM_CFA_FLOW_INFO_INPUT_FLOW_HANDLE_DIR_RX);
	req.flow_handle_2 = cpu_to_le16(HWRM_CFA_FLOW_INFO_INPUT_FLOW_HANDLE_ROCEV1_CNT);
	req.flow_handle_3 = cpu_to_le16(HWRM_CFA_FLOW_INFO_INPUT_FLOW_HANDLE_ROCEV1_CNT |
					HWRM_CFA_FLOW_INFO_INPUT_FLOW_HANDLE_DIR_RX);
	req.flow_handle_4 = cpu_to_le16(HWRM_CFA_FLOW_INFO_INPUT_FLOW_HANDLE_ROCEV2_CNT);
	req.flow_handle_5 = cpu_to_le16(HWRM_CFA_FLOW_INFO_INPUT_FLOW_HANDLE_ROCEV2_CNT |
					HWRM_CFA_FLOW_INFO_INPUT_FLOW_HANDLE_DIR_RX);
	bnxt_re_fill_fw_msg(&fw_msg, &req, sizeof(req), &resp,
			    sizeof(resp), BNXT_RE_HWRM_CMD_TIMEOUT(rdev));
	rc = en_dev->en_ops->bnxt_send_fw_msg(en_dev, BNXT_ROCE_ULP, &fw_msg);
	if (rc) {
		dev_err(rdev_to_dev(rdev),
			"Failed to get CFA Flow stats : rc = 0x%x\n", rc);
		return rc;
	}

	stats->cnp_stats.cnp_tx_pkts = le64_to_cpu(resp.packet_0);
	stats->cnp_stats.cnp_tx_bytes = le64_to_cpu(resp.byte_0);
	stats->cnp_stats.cnp_rx_pkts = le64_to_cpu(resp.packet_1);
	stats->cnp_stats.cnp_rx_bytes = le64_to_cpu(resp.byte_1);

	stats->ro_stats.tx_pkts = le64_to_cpu(resp.packet_2) +
		le64_to_cpu(resp.packet_4);
	stats->ro_stats.tx_bytes = le64_to_cpu(resp.byte_2) +
		le64_to_cpu(resp.byte_4);
	stats->ro_stats.rx_pkts = le64_to_cpu(resp.packet_3) +
		le64_to_cpu(resp.packet_5);
	stats->ro_stats.rx_bytes = le64_to_cpu(resp.byte_3) +
		le64_to_cpu(resp.byte_5);

	return 0;
}

int bnxt_re_get_qos_stats(struct bnxt_re_dev *rdev)
{
	struct bnxt_re_ro_counters roce_only_tmp[2] = {{}, {}};
	struct bnxt_re_cnp_counters tmp_counters[2] = {{}, {}};
	struct hwrm_cfa_flow_stats_output resp = {};
	struct hwrm_cfa_flow_stats_input req = {};
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	struct bnxt_fw_msg fw_msg = {};
	struct bnxt_re_cc_stat *cnps;
	struct bnxt_re_rstat *dstat;
	int rc = 0;
	u64 bytes;
	u64 pkts;

	/* Issue HWRM cmd to read flow counters for CNP tx and rx */
	bnxt_re_init_hwrm_hdr(rdev, (void *)&req, HWRM_CFA_FLOW_STATS, -1, -1);
	req.num_flows = cpu_to_le16(6);
	req.flow_handle_0 = cpu_to_le16(HWRM_CFA_FLOW_INFO_INPUT_FLOW_HANDLE_CNP_CNT);
	req.flow_handle_1 = cpu_to_le16(HWRM_CFA_FLOW_INFO_INPUT_FLOW_HANDLE_CNP_CNT |
					HWRM_CFA_FLOW_INFO_INPUT_FLOW_HANDLE_DIR_RX);
	req.flow_handle_2 = cpu_to_le16(HWRM_CFA_FLOW_INFO_INPUT_FLOW_HANDLE_ROCEV1_CNT);
	req.flow_handle_3 = cpu_to_le16(HWRM_CFA_FLOW_INFO_INPUT_FLOW_HANDLE_ROCEV1_CNT |
					HWRM_CFA_FLOW_INFO_INPUT_FLOW_HANDLE_DIR_RX);
	req.flow_handle_4 = cpu_to_le16(HWRM_CFA_FLOW_INFO_INPUT_FLOW_HANDLE_ROCEV2_CNT);
	req.flow_handle_5 = cpu_to_le16(HWRM_CFA_FLOW_INFO_INPUT_FLOW_HANDLE_ROCEV2_CNT |
				       HWRM_CFA_FLOW_INFO_INPUT_FLOW_HANDLE_DIR_RX);
	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			    sizeof(resp), BNXT_RE_HWRM_CMD_TIMEOUT(rdev));
	rc = en_dev->en_ops->bnxt_send_fw_msg(en_dev, BNXT_ROCE_ULP, &fw_msg);
	if (rc) {
		dev_err(rdev_to_dev(rdev),
			"Failed to get CFA Flow stats : rc = 0x%x\n", rc);
		goto done;
	}

	tmp_counters[0].cnp_tx_pkts = le64_to_cpu(resp.packet_0);
	tmp_counters[0].cnp_tx_bytes = le64_to_cpu(resp.byte_0);
	tmp_counters[0].cnp_rx_pkts = le64_to_cpu(resp.packet_1);
	tmp_counters[0].cnp_rx_bytes = le64_to_cpu(resp.byte_1);

	roce_only_tmp[0].tx_pkts = le64_to_cpu(resp.packet_2) +
				   le64_to_cpu(resp.packet_4);
	roce_only_tmp[0].tx_bytes = le64_to_cpu(resp.byte_2) +
				    le64_to_cpu(resp.byte_4);
	roce_only_tmp[0].rx_pkts = le64_to_cpu(resp.packet_3) +
				   le64_to_cpu(resp.packet_5);
	roce_only_tmp[0].rx_bytes = le64_to_cpu(resp.byte_3) +
				    le64_to_cpu(resp.byte_5);

	cnps = &rdev->stats.cnps;
	dstat = &rdev->stats.dstat;
	if (!cnps->is_first) {
		/* First query done.. */
		cnps->is_first = true;
		cnps->prev[0].cnp_tx_pkts = tmp_counters[0].cnp_tx_pkts;
		cnps->prev[0].cnp_tx_bytes = tmp_counters[0].cnp_tx_bytes;
		cnps->prev[0].cnp_rx_pkts = tmp_counters[0].cnp_rx_pkts;
		cnps->prev[0].cnp_rx_bytes = tmp_counters[0].cnp_rx_bytes;

		cnps->prev[1].cnp_tx_pkts = tmp_counters[1].cnp_tx_pkts;
		cnps->prev[1].cnp_tx_bytes = tmp_counters[1].cnp_tx_bytes;
		cnps->prev[1].cnp_rx_pkts = tmp_counters[1].cnp_rx_pkts;
		cnps->prev[1].cnp_rx_bytes = tmp_counters[1].cnp_rx_bytes;

		dstat->prev[0].tx_pkts = roce_only_tmp[0].tx_pkts;
		dstat->prev[0].tx_bytes = roce_only_tmp[0].tx_bytes;
		dstat->prev[0].rx_pkts = roce_only_tmp[0].rx_pkts;
		dstat->prev[0].rx_bytes = roce_only_tmp[0].rx_bytes;

		dstat->prev[1].tx_pkts = roce_only_tmp[1].tx_pkts;
		dstat->prev[1].tx_bytes = roce_only_tmp[1].tx_bytes;
		dstat->prev[1].rx_pkts = roce_only_tmp[1].rx_pkts;
		dstat->prev[1].rx_bytes = roce_only_tmp[1].rx_bytes;
	} else {
		u64 byte_mask, pkts_mask;
		u64 diff;

		byte_mask = bnxt_re_get_cfa_stat_mask(rdev->chip_ctx,
						      BYTE_MASK);
		pkts_mask = bnxt_re_get_cfa_stat_mask(rdev->chip_ctx,
						      PKTS_MASK);
		/*
		 * Calculate the number of cnp packets and use
		 * the value to calculate the CRC bytes.
		 * Multply pkts with 4 and add it to total bytes
		 */
		pkts = bnxt_re_stat_diff(tmp_counters[0].cnp_tx_pkts,
					 &cnps->prev[0].cnp_tx_pkts,
					 pkts_mask);
		cnps->cur[0].cnp_tx_pkts += pkts;
		diff = bnxt_re_stat_diff(tmp_counters[0].cnp_tx_bytes,
					 &cnps->prev[0].cnp_tx_bytes,
					 byte_mask);
		bytes = diff + pkts * 4;
		cnps->cur[0].cnp_tx_bytes += bytes;
		pkts = bnxt_re_stat_diff(tmp_counters[0].cnp_rx_pkts,
					 &cnps->prev[0].cnp_rx_pkts,
					 pkts_mask);
		cnps->cur[0].cnp_rx_pkts += pkts;
		bytes = bnxt_re_stat_diff(tmp_counters[0].cnp_rx_bytes,
					  &cnps->prev[0].cnp_rx_bytes,
					  byte_mask);
		cnps->cur[0].cnp_rx_bytes += bytes;

		/*
		 * Calculate the number of cnp packets and use
		 * the value to calculate the CRC bytes.
		 * Multply pkts with 4 and add it to total bytes
		 */
		pkts = bnxt_re_stat_diff(tmp_counters[1].cnp_tx_pkts,
					 &cnps->prev[1].cnp_tx_pkts,
					 pkts_mask);
		cnps->cur[1].cnp_tx_pkts += pkts;
		diff = bnxt_re_stat_diff(tmp_counters[1].cnp_tx_bytes,
					 &cnps->prev[1].cnp_tx_bytes,
					 byte_mask);
		cnps->cur[1].cnp_tx_bytes += diff + pkts * 4;
		pkts = bnxt_re_stat_diff(tmp_counters[1].cnp_rx_pkts,
					 &cnps->prev[1].cnp_rx_pkts,
					 pkts_mask);
		cnps->cur[1].cnp_rx_pkts += pkts;
		bytes = bnxt_re_stat_diff(tmp_counters[1].cnp_rx_bytes,
					  &cnps->prev[1].cnp_rx_bytes,
					  byte_mask);
		cnps->cur[1].cnp_rx_bytes += bytes;

		pkts = bnxt_re_stat_diff(roce_only_tmp[0].tx_pkts,
					 &dstat->prev[0].tx_pkts,
					 pkts_mask);
		dstat->cur[0].tx_pkts += pkts;
		diff = bnxt_re_stat_diff(roce_only_tmp[0].tx_bytes,
					 &dstat->prev[0].tx_bytes,
					 byte_mask);
		dstat->cur[0].tx_bytes += diff + pkts * 4;
		pkts = bnxt_re_stat_diff(roce_only_tmp[0].rx_pkts,
					 &dstat->prev[0].rx_pkts,
					 pkts_mask);
		dstat->cur[0].rx_pkts += pkts;

		bytes = bnxt_re_stat_diff(roce_only_tmp[0].rx_bytes,
					  &dstat->prev[0].rx_bytes,
					  byte_mask);
		dstat->cur[0].rx_bytes += bytes;
		pkts = bnxt_re_stat_diff(roce_only_tmp[1].tx_pkts,
					 &dstat->prev[1].tx_pkts,
					 pkts_mask);
		dstat->cur[1].tx_pkts += pkts;
		diff = bnxt_re_stat_diff(roce_only_tmp[1].tx_bytes,
					 &dstat->prev[1].tx_bytes,
					 byte_mask);
		dstat->cur[1].tx_bytes += diff + pkts * 4;
		pkts = bnxt_re_stat_diff(roce_only_tmp[1].rx_pkts,
					 &dstat->prev[1].rx_pkts,
					 pkts_mask);
		dstat->cur[1].rx_pkts += pkts;
		bytes = bnxt_re_stat_diff(roce_only_tmp[1].rx_bytes,
					  &dstat->prev[1].rx_bytes,
					  byte_mask);
		dstat->cur[1].rx_bytes += bytes;
	}
done:
	return rc;
}

static void bnxt_re_copy_ext_stats(struct bnxt_re_dev *rdev,
				   u8 indx, struct bnxt_qplib_ext_stat *s)
{
	struct bnxt_re_ext_roce_stats *e_errs;
	struct bnxt_re_cnp_counters *cnp;
	struct bnxt_re_ext_rstat *ext_d;
	struct bnxt_re_ro_counters *ro;

	cnp = &rdev->stats.cnps.cur[indx];
	ro = &rdev->stats.dstat.cur[indx];
	ext_d = &rdev->stats.dstat.ext_rstat[indx];
	e_errs = &rdev->stats.dstat.e_errs;

	cnp->cnp_tx_pkts = s->tx_cnp;
	cnp->cnp_rx_pkts = s->rx_cnp;
	/* In bonding mode do not duplicate other stats */
	if (indx)
		return;
	cnp->ecn_marked = s->rx_ecn_marked;

	ro->tx_pkts = s->tx_roce_pkts;
	ro->tx_bytes = s->tx_roce_bytes;
	ro->rx_pkts = s->rx_roce_pkts;
	ro->rx_bytes = s->rx_roce_bytes;

	ext_d->tx.atomic_req = s->tx_atomic_req;
	ext_d->tx.read_req = s->tx_read_req;
	ext_d->tx.read_resp = s->tx_read_res;
	ext_d->tx.write_req = s->tx_write_req;
	ext_d->tx.send_req = s->tx_send_req;
	ext_d->rx.atomic_req = s->rx_atomic_req;
	ext_d->rx.read_req = s->rx_read_req;
	ext_d->rx.read_resp = s->rx_read_res;
	ext_d->rx.write_req = s->rx_write_req;
	ext_d->rx.send_req = s->rx_send_req;
	ext_d->grx.rx_pkts = s->rx_roce_good_pkts;
	ext_d->grx.rx_bytes = s->rx_roce_good_bytes;
	ext_d->rx_dcn_payload_cut = s->rx_dcn_payload_cut;
	ext_d->te_bypassed = s->te_bypassed;
	e_errs->oob = s->rx_out_of_buffer;
	e_errs->oos = s->rx_out_of_sequence;
	e_errs->seq_err_naks_rcvd = s->seq_err_naks_rcvd;
	e_errs->rnr_naks_rcvd = s->rnr_naks_rcvd;
	e_errs->missing_resp = s->missing_resp;
	e_errs->to_retransmits = s->to_retransmits;
	e_errs->dup_req = s->dup_req;
}

static int bnxt_re_get_ext_stat(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_ext_stat estat[2] = {{}, {}};
	struct bnxt_qplib_query_stats_info sinfo;
	u32 fid;
	int rc;

	fid = PCI_FUNC(rdev->en_dev->pdev->devfn);
	/* Set default values for sinfo */
	sinfo.function_id = 0xFFFFFFFF;
	sinfo.collection_id = 0xFF;
	sinfo.vf_valid  = false;
	rc = bnxt_qplib_qext_stat(&rdev->rcfw, fid, &estat[0], &sinfo);
	if (rc)
		goto done;
	bnxt_re_copy_ext_stats(rdev, 0, &estat[0]);

done:
	return rc;
}

static void bnxt_re_copy_rstat(struct bnxt_re_rdata_counters *d,
			       struct ctx_hw_stats_ext *s,
			       bool is_thor)
{
	d->tx_ucast_pkts = le64_to_cpu(s->tx_ucast_pkts);
	d->tx_mcast_pkts = le64_to_cpu(s->tx_mcast_pkts);
	d->tx_bcast_pkts = le64_to_cpu(s->tx_bcast_pkts);
	d->tx_discard_pkts = le64_to_cpu(s->tx_discard_pkts);
	d->tx_error_pkts = le64_to_cpu(s->tx_error_pkts);
	d->tx_ucast_bytes = le64_to_cpu(s->tx_ucast_bytes);
	/* Add four bytes of CRC bytes per packet */
	d->tx_ucast_bytes +=  d->tx_ucast_pkts * 4;
	d->tx_mcast_bytes = le64_to_cpu(s->tx_mcast_bytes);
	d->tx_bcast_bytes = le64_to_cpu(s->tx_bcast_bytes);
	d->rx_ucast_pkts = le64_to_cpu(s->rx_ucast_pkts);
	d->rx_mcast_pkts = le64_to_cpu(s->rx_mcast_pkts);
	d->rx_bcast_pkts = le64_to_cpu(s->rx_bcast_pkts);
	d->rx_discard_pkts = le64_to_cpu(s->rx_discard_pkts);
	d->rx_error_pkts = le64_to_cpu(s->rx_error_pkts);
	d->rx_ucast_bytes = le64_to_cpu(s->rx_ucast_bytes);
	d->rx_mcast_bytes = le64_to_cpu(s->rx_mcast_bytes);
	d->rx_bcast_bytes = le64_to_cpu(s->rx_bcast_bytes);
	if (is_thor) {
		d->rx_agg_pkts = le64_to_cpu(s->rx_tpa_pkt);
		d->rx_agg_bytes = le64_to_cpu(s->rx_tpa_bytes);
		d->rx_agg_events = le64_to_cpu(s->rx_tpa_events);
		d->rx_agg_aborts = le64_to_cpu(s->rx_tpa_errors);
	}
}

static void bnxt_re_get_roce_data_stats(struct bnxt_re_dev *rdev)
{
	bool is_thor = _is_chip_gen_p5_p7(rdev->chip_ctx);
	struct bnxt_re_rdata_counters *rstat;

	rstat = &rdev->stats.dstat.rstat[0];
	bnxt_re_copy_rstat(rstat, rdev->qplib_res.hctx->stats.dma, is_thor);

}

int bnxt_re_get_device_stats(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_query_stats_info sinfo;
	int rc = 0;

	/* Stats are in 1s cadence */
	if (test_bit(BNXT_RE_FLAG_ISSUE_CFA_FLOW_STATS, &rdev->flags)) {
		if (bnxt_ext_stats_supported(rdev->chip_ctx, rdev->dev_attr->dev_cap_flags,
					     rdev->is_virtfn))
			rc = bnxt_re_get_ext_stat(rdev);
		else
			rc = bnxt_re_get_qos_stats(rdev);

		if (rc && rc != -ENOMEM)
			clear_bit(BNXT_RE_FLAG_ISSUE_CFA_FLOW_STATS,
				  &rdev->flags);
	}

	if (test_bit(BNXT_RE_FLAG_ISSUE_ROCE_STATS, &rdev->flags)) {
		bnxt_re_get_roce_data_stats(rdev);

		/* Set default values for sinfo */
		sinfo.function_id = 0xFFFFFFFF;
		sinfo.collection_id = 0xFF;
		sinfo.vf_valid  = false;
		rc = bnxt_qplib_get_roce_error_stats(&rdev->rcfw,
						     &rdev->stats.dstat.errs,
						     &sinfo);
		if (rc && rc != -ENOMEM)
			clear_bit(BNXT_RE_FLAG_ISSUE_ROCE_STATS,
				  &rdev->flags);
	}

	return rc;
}

static const char * const bnxt_re_stat_descs[] = {
	"link_state",
	"max_qp",
	"max_srq",
	"max_cq",
	"max_mr",
	"max_mw",
	"max_ah",
	"max_pd",
	"active_qp",
	"active_rc_qp",
	"active_ud_qp",
	"active_srq",
	"active_cq",
	"active_mr",
	"active_mw",
	"active_ah",
	"active_pd",
	"qp_watermark",
	"rc_qp_watermark",
	"ud_qp_watermark",
	"srq_watermark",
	"cq_watermark",
	"mr_watermark",
	"mw_watermark",
	"ah_watermark",
	"pd_watermark",
	"resize_cq_count",
	"hw_retransmission",
	"recoverable_errors",
	"rx_pkts",
	"rx_bytes",
	"tx_pkts",
	"tx_bytes",
	"cnp_tx_pkts",
	"cnp_tx_bytes",
	"cnp_rx_pkts",
	"cnp_rx_bytes",
	"roce_only_rx_pkts",
	"roce_only_rx_bytes",
	"roce_only_tx_pkts",
	"roce_only_tx_bytes",
	"rx_roce_error_pkts",
	"rx_roce_discard_pkts",
	"tx_roce_error_pkts",
	"tx_roce_discards_pkts",
	"res_oob_drop_count",
	"tx_atomic_req",
	"rx_atomic_req",
	"tx_read_req",
	"tx_read_resp",
	"rx_read_req",
	"rx_read_resp",
	"tx_write_req",
	"rx_write_req",
	"tx_send_req",
	"rx_send_req",
	"rx_good_pkts",
	"rx_good_bytes",
	"rx_dcn_payload_cut",
	"te_bypassed",
	"rx_ecn_marked_pkts",
	"max_retry_exceeded",
	"to_retransmits",
	"seq_err_naks_rcvd",
	"rnr_naks_rcvd",
	"missing_resp",
	"dup_reqs",
	"unrecoverable_err",
	"bad_resp_err",
	"local_qp_op_err",
	"local_protection_err",
	"mem_mgmt_op_err",
	"remote_invalid_req_err",
	"remote_access_err",
	"remote_op_err",
	"res_exceed_max",
	"res_length_mismatch",
	"res_exceeds_wqe",
	"res_opcode_err",
	"res_rx_invalid_rkey",
	"res_rx_domain_err",
	"res_rx_no_perm",
	"res_rx_range_err",
	"res_tx_invalid_rkey",
	"res_tx_domain_err",
	"res_tx_no_perm",
	"res_tx_range_err",
	"res_irrq_oflow",
	"res_unsup_opcode",
	"res_unaligned_atomic",
	"res_rem_inv_err",
	"res_mem_error64",
	"res_srq_err",
	"res_cmp_err",
	"res_invalid_dup_rkey",
	"res_wqe_format_err",
	"res_cq_load_err",
	"res_srq_load_err",
	"res_tx_pci_err",
	"res_rx_pci_err",
	"res_oos_drop_count",
	"num_irq_started",
	"num_irq_stopped",
	"poll_in_intr_en",
	"poll_in_intr_dis",
	"cmdq_full_dbg_cnt",
	"fw_service_prof_type_sup",
	"dbq_int_recv",
	"dbq_int_en",
	"dbq_pacing_resched",
	"dbq_pacing_complete",
	"dbq_pacing_alerts",
	"dbq_dbr_fifo_reg"

};

static void bnxt_re_print_ext_stat(struct bnxt_re_dev *rdev,
				   struct rdma_hw_stats *stats)
{
	struct bnxt_re_cnp_counters *cnp;
	struct bnxt_re_ext_rstat *ext_s;

	ext_s = &rdev->stats.dstat.ext_rstat[0];
	cnp = &rdev->stats.cnps.cur[0];

	stats->value[BNXT_RE_TX_ATOMIC_REQ] = ext_s->tx.atomic_req;
	stats->value[BNXT_RE_RX_ATOMIC_REQ] = ext_s->rx.atomic_req;
	stats->value[BNXT_RE_TX_READ_REQ] = ext_s->tx.read_req;
	stats->value[BNXT_RE_TX_READ_RESP] = ext_s->tx.read_resp;
	stats->value[BNXT_RE_RX_READ_REQ] = ext_s->rx.read_req;
	stats->value[BNXT_RE_RX_READ_RESP] = ext_s->rx.read_resp;
	stats->value[BNXT_RE_TX_WRITE_REQ] = ext_s->tx.write_req;
	stats->value[BNXT_RE_RX_WRITE_REQ] = ext_s->rx.write_req;
	stats->value[BNXT_RE_TX_SEND_REQ] = ext_s->tx.send_req;
	stats->value[BNXT_RE_RX_SEND_REQ] = ext_s->rx.send_req;
	stats->value[BNXT_RE_RX_GOOD_PKTS] = ext_s->grx.rx_pkts;
	stats->value[BNXT_RE_RX_GOOD_BYTES] = ext_s->grx.rx_bytes;
	if (_is_chip_p7(rdev->chip_ctx)) {
		stats->value[BNXT_RE_RX_DCN_PAYLOAD_CUT] = ext_s->rx_dcn_payload_cut;
		stats->value[BNXT_RE_TE_BYPASSED] = ext_s->te_bypassed;
	}
	stats->value[BNXT_RE_RX_ECN_MARKED_PKTS] = cnp->ecn_marked;
}

static void bnxt_re_print_roce_only_counters(struct bnxt_re_dev *rdev,
					     struct rdma_hw_stats *stats)
{
	struct bnxt_re_ro_counters *roce_only = &rdev->stats.dstat.cur[0];

	stats->value[BNXT_RE_ROCE_ONLY_RX_PKTS] = roce_only->rx_pkts;
	stats->value[BNXT_RE_ROCE_ONLY_RX_BYTES] = roce_only->rx_bytes;
	stats->value[BNXT_RE_ROCE_ONLY_TX_PKTS] = roce_only->tx_pkts;
	stats->value[BNXT_RE_ROCE_ONLY_TX_BYTES] = roce_only->tx_bytes;
}

static void bnxt_re_print_normal_total_counters(struct bnxt_re_dev *rdev,
						struct rdma_hw_stats *stats)
{
	struct bnxt_re_ro_counters *roce_only;
	struct bnxt_re_cc_stat *cnps;

	cnps = &rdev->stats.cnps;
	roce_only = &rdev->stats.dstat.cur[0];

	stats->value[BNXT_RE_RX_PKTS] = cnps->cur[0].cnp_rx_pkts + roce_only->rx_pkts;
	stats->value[BNXT_RE_RX_BYTES] = cnps->cur[0].cnp_rx_bytes + roce_only->rx_bytes;
	stats->value[BNXT_RE_TX_PKTS] = cnps->cur[0].cnp_tx_pkts + roce_only->tx_pkts;
	stats->value[BNXT_RE_TX_BYTES] = cnps->cur[0].cnp_tx_bytes + roce_only->tx_bytes;
}

static void bnxt_re_print_normal_counters(struct bnxt_re_dev *rdev,
					  struct rdma_hw_stats *rstats)
{
	struct bnxt_re_rdata_counters *stats;
	struct bnxt_re_cc_stat *cnps;
	bool en_disp;

	stats = &rdev->stats.dstat.rstat[0];
	cnps = &rdev->stats.cnps;
	en_disp = !_is_chip_gen_p5_p7(rdev->chip_ctx);

	bnxt_re_print_normal_total_counters(rdev, rstats);
	if (!rdev->is_virtfn) {
		rstats->value[BNXT_RE_CNP_TX_PKTS] = cnps->cur[0].cnp_tx_pkts;
		if (en_disp)
			rstats->value[BNXT_RE_CNP_TX_BYTES] = cnps->cur[0].cnp_tx_bytes;
		rstats->value[BNXT_RE_CNP_RX_PKTS] = cnps->cur[0].cnp_rx_pkts;
		if (en_disp)
			rstats->value[BNXT_RE_CNP_RX_BYTES] = cnps->cur[0].cnp_rx_bytes;
	}
	/* Print RoCE only bytes.. CNP counters include RoCE packets also */
	bnxt_re_print_roce_only_counters(rdev, rstats);

	rstats->value[BNXT_RE_RX_ROCE_ERROR_PKTS] = stats ? stats->rx_error_pkts : 0;
	rstats->value[BNXT_RE_RX_ROCE_DISCARD_PKTS] = stats ? stats->rx_discard_pkts : 0;
	if (!en_disp) {
		rstats->value[BNXT_RE_TX_ROCE_ERROR_PKTS] = stats ? stats->tx_error_pkts : 0;
		rstats->value[BNXT_RE_TX_ROCE_DISCARDS_PKTS] = stats ? stats->tx_discard_pkts : 0;
	}

	if (bnxt_ext_stats_supported(rdev->chip_ctx, rdev->dev_attr->dev_cap_flags,
				     rdev->is_virtfn)) {
		rstats->value[BNXT_RE_RES_OOB_DROP_COUNT] = rdev->stats.dstat.e_errs.oob;
		bnxt_re_print_ext_stat(rdev, rstats);
	}
}

static void bnxt_re_copy_db_pacing_stats(struct bnxt_re_dev *rdev,
					 struct rdma_hw_stats *stats)
{
	struct bnxt_re_dbr_sw_stats *dbr_sw_stats = rdev->dbr_sw_stats;

	stats->value[BNXT_RE_DBQ_PACING_RESCHED] = dbr_sw_stats->dbq_pacing_resched;
	stats->value[BNXT_RE_DBQ_PACING_CMPL] = dbr_sw_stats->dbq_pacing_complete;
	stats->value[BNXT_RE_DBQ_PACING_ALERT] = dbr_sw_stats->dbq_pacing_alerts;
	stats->value[BNXT_RE_DBQ_DBR_FIFO_REG] = readl_fbsd(rdev->en_dev->softc,
						       rdev->dbr_db_fifo_reg_off, 0);
}

int bnxt_re_get_hw_stats(struct ib_device *ibdev,
			    struct rdma_hw_stats *stats,
			    u8 port, int index)
{
	struct bnxt_re_dev *rdev = to_bnxt_re_dev(ibdev, ibdev);
	struct bnxt_re_ext_roce_stats *e_errs;
	struct bnxt_re_rdata_counters *rstat;
	struct bnxt_qplib_roce_stats *errs;
	unsigned long tstamp_diff;
	struct pci_dev *pdev;
	int sched_msec;
	int rc = 0;

	if (!port || !stats)
		return -EINVAL;

	if (!rdev)
		return -ENODEV;

	if (!__bnxt_re_is_rdev_valid(rdev)) {
		return -ENODEV;
	}

	pdev = rdev->en_dev->pdev;
	errs = &rdev->stats.dstat.errs;
	rstat = &rdev->stats.dstat.rstat[0];
	e_errs = &rdev->stats.dstat.e_errs;
#define BNXT_RE_STATS_CTX_UPDATE_TIMER	250
	sched_msec = BNXT_RE_STATS_CTX_UPDATE_TIMER;
	tstamp_diff = jiffies - rdev->stats.read_tstamp;
	if (test_bit(BNXT_RE_FLAG_IBDEV_REGISTERED, &rdev->flags)) {
		if (/* restrict_stats && */ tstamp_diff < msecs_to_jiffies(sched_msec))
			goto skip_query;
		rc = bnxt_re_get_device_stats(rdev);
		if (rc)
			dev_err(rdev_to_dev(rdev),
				"Failed to query device stats\n");
		rdev->stats.read_tstamp = jiffies;
	}

	if (rdev->dbr_pacing)
		bnxt_re_copy_db_pacing_stats(rdev, stats);

skip_query:

	if (rdev->netdev)
		stats->value[BNXT_RE_LINK_STATE] = bnxt_re_link_state(rdev);
	stats->value[BNXT_RE_MAX_QP] = rdev->dev_attr->max_qp;
	stats->value[BNXT_RE_MAX_SRQ] = rdev->dev_attr->max_srq;
	stats->value[BNXT_RE_MAX_CQ] = rdev->dev_attr->max_cq;
	stats->value[BNXT_RE_MAX_MR] = rdev->dev_attr->max_mr;
	stats->value[BNXT_RE_MAX_MW] = rdev->dev_attr->max_mw;
	stats->value[BNXT_RE_MAX_AH] = rdev->dev_attr->max_ah;
	stats->value[BNXT_RE_MAX_PD] = rdev->dev_attr->max_pd;
	stats->value[BNXT_RE_ACTIVE_QP] = atomic_read(&rdev->stats.rsors.qp_count);
	stats->value[BNXT_RE_ACTIVE_RC_QP] = atomic_read(&rdev->stats.rsors.rc_qp_count);
	stats->value[BNXT_RE_ACTIVE_UD_QP] = atomic_read(&rdev->stats.rsors.ud_qp_count);
	stats->value[BNXT_RE_ACTIVE_SRQ] = atomic_read(&rdev->stats.rsors.srq_count);
	stats->value[BNXT_RE_ACTIVE_CQ] = atomic_read(&rdev->stats.rsors.cq_count);
	stats->value[BNXT_RE_ACTIVE_MR] = atomic_read(&rdev->stats.rsors.mr_count);
	stats->value[BNXT_RE_ACTIVE_MW] = atomic_read(&rdev->stats.rsors.mw_count);
	stats->value[BNXT_RE_ACTIVE_AH] = atomic_read(&rdev->stats.rsors.ah_count);
	stats->value[BNXT_RE_ACTIVE_PD] = atomic_read(&rdev->stats.rsors.pd_count);
	stats->value[BNXT_RE_QP_WATERMARK] = atomic_read(&rdev->stats.rsors.max_qp_count);
	stats->value[BNXT_RE_RC_QP_WATERMARK] = atomic_read(&rdev->stats.rsors.max_rc_qp_count);
	stats->value[BNXT_RE_UD_QP_WATERMARK] = atomic_read(&rdev->stats.rsors.max_ud_qp_count);
	stats->value[BNXT_RE_SRQ_WATERMARK] = atomic_read(&rdev->stats.rsors.max_srq_count);
	stats->value[BNXT_RE_CQ_WATERMARK] = atomic_read(&rdev->stats.rsors.max_cq_count);
	stats->value[BNXT_RE_MR_WATERMARK] = atomic_read(&rdev->stats.rsors.max_mr_count);
	stats->value[BNXT_RE_MW_WATERMARK] = atomic_read(&rdev->stats.rsors.max_mw_count);
	stats->value[BNXT_RE_AH_WATERMARK] = atomic_read(&rdev->stats.rsors.max_ah_count);
	stats->value[BNXT_RE_PD_WATERMARK] = atomic_read(&rdev->stats.rsors.max_pd_count);
	stats->value[BNXT_RE_RESIZE_CQ_COUNT] = atomic_read(&rdev->stats.rsors.resize_count);
	stats->value[BNXT_RE_HW_RETRANSMISSION] = BNXT_RE_HW_RETX(rdev->dev_attr->dev_cap_flags) ?  1 : 0;
	stats->value[BNXT_RE_RECOVERABLE_ERRORS] = rstat ? rstat->tx_bcast_pkts : 0;

	bnxt_re_print_normal_counters(rdev, stats);


	stats->value[BNXT_RE_MAX_RETRY_EXCEEDED] = errs->max_retry_exceeded;
	if (bnxt_ext_stats_supported(rdev->chip_ctx, rdev->dev_attr->dev_cap_flags,
				     rdev->is_virtfn) &&
	    _is_hw_retx_supported(rdev->dev_attr->dev_cap_flags)) {
		stats->value[BNXT_RE_TO_RETRANSMITS] = e_errs->to_retransmits;
		stats->value[BNXT_RE_SEQ_ERR_NAKS_RCVD] = e_errs->seq_err_naks_rcvd;
		stats->value[BNXT_RE_RNR_NAKS_RCVD] = e_errs->rnr_naks_rcvd;
		stats->value[BNXT_RE_MISSING_RESP] = e_errs->missing_resp;
		stats->value[BNXT_RE_DUP_REQS] = e_errs->dup_req;
	} else {
		stats->value[BNXT_RE_TO_RETRANSMITS] = errs->to_retransmits;
		stats->value[BNXT_RE_SEQ_ERR_NAKS_RCVD] = errs->seq_err_naks_rcvd;
		stats->value[BNXT_RE_RNR_NAKS_RCVD] = errs->rnr_naks_rcvd;
		stats->value[BNXT_RE_MISSING_RESP] = errs->missing_resp;
		stats->value[BNXT_RE_DUP_REQS] = errs->dup_req;
	}

	stats->value[BNXT_RE_UNRECOVERABLE_ERR] = errs->unrecoverable_err;
	stats->value[BNXT_RE_BAD_RESP_ERR] = errs->bad_resp_err;
	stats->value[BNXT_RE_LOCAL_QP_OP_ERR] = errs->local_qp_op_err;
	stats->value[BNXT_RE_LOCAL_PROTECTION_ERR] = errs->local_protection_err;
	stats->value[BNXT_RE_MEM_MGMT_OP_ERR] = errs->mem_mgmt_op_err;
	stats->value[BNXT_RE_REMOTE_INVALID_REQ_ERR] = errs->remote_invalid_req_err;
	stats->value[BNXT_RE_REMOTE_ACCESS_ERR] = errs->remote_access_err;
	stats->value[BNXT_RE_REMOTE_OP_ERR] = errs->remote_op_err;
	stats->value[BNXT_RE_RES_EXCEED_MAX] = errs->res_exceed_max;
	stats->value[BNXT_RE_RES_LENGTH_MISMATCH] = errs->res_length_mismatch;
	stats->value[BNXT_RE_RES_EXCEEDS_WQE] = errs->res_exceeds_wqe;
	stats->value[BNXT_RE_RES_OPCODE_ERR] = errs->res_opcode_err;
	stats->value[BNXT_RE_RES_RX_INVALID_RKEY] = errs->res_rx_invalid_rkey;
	stats->value[BNXT_RE_RES_RX_DOMAIN_ERR] = errs->res_rx_domain_err;
	stats->value[BNXT_RE_RES_RX_NO_PERM] = errs->res_rx_no_perm;
	stats->value[BNXT_RE_RES_RX_RANGE_ERR] = errs->res_rx_range_err;
	stats->value[BNXT_RE_RES_TX_INVALID_RKEY] = errs->res_tx_invalid_rkey;
	stats->value[BNXT_RE_RES_TX_DOMAIN_ERR] = errs->res_tx_domain_err;
	stats->value[BNXT_RE_RES_TX_NO_PERM] = errs->res_tx_no_perm;
	stats->value[BNXT_RE_RES_TX_RANGE_ERR] = errs->res_tx_range_err;
	stats->value[BNXT_RE_RES_IRRQ_OFLOW] = errs->res_irrq_oflow;
	stats->value[BNXT_RE_RES_UNSUP_OPCODE] = errs->res_unsup_opcode;
	stats->value[BNXT_RE_RES_UNALIGNED_ATOMIC] = errs->res_unaligned_atomic;
	stats->value[BNXT_RE_RES_REM_INV_ERR] = errs->res_rem_inv_err;
	stats->value[BNXT_RE_RES_MEM_ERROR64] = errs->res_mem_error;
	stats->value[BNXT_RE_RES_SRQ_ERR] = errs->res_srq_err;
	stats->value[BNXT_RE_RES_CMP_ERR] = errs->res_cmp_err;
	stats->value[BNXT_RE_RES_INVALID_DUP_RKEY] = errs->res_invalid_dup_rkey;
	stats->value[BNXT_RE_RES_WQE_FORMAT_ERR] = errs->res_wqe_format_err;
	stats->value[BNXT_RE_RES_CQ_LOAD_ERR] = errs->res_cq_load_err;
	stats->value[BNXT_RE_RES_SRQ_LOAD_ERR] = errs->res_srq_load_err;
	stats->value[BNXT_RE_RES_TX_PCI_ERR] = errs->res_tx_pci_err;
	stats->value[BNXT_RE_RES_RX_PCI_ERR] = errs->res_rx_pci_err;


	if (bnxt_ext_stats_supported(rdev->chip_ctx, rdev->dev_attr->dev_cap_flags,
				     rdev->is_virtfn)) {
	stats->value[BNXT_RE_RES_OOS_DROP_COUNT] = e_errs->oos;
	} else {
		/* Display on function 0 as OOS counters are chip-wide */
		if (PCI_FUNC(pdev->devfn) == 0)
			stats->value[BNXT_RE_RES_OOS_DROP_COUNT] = errs->res_oos_drop_count;
	}
	stats->value[BNXT_RE_NUM_IRQ_STARTED] = rdev->rcfw.num_irq_started;
	stats->value[BNXT_RE_NUM_IRQ_STOPPED] = rdev->rcfw.num_irq_stopped;
	stats->value[BNXT_RE_POLL_IN_INTR_EN] = rdev->rcfw.poll_in_intr_en;
	stats->value[BNXT_RE_POLL_IN_INTR_DIS] = rdev->rcfw.poll_in_intr_dis;
	stats->value[BNXT_RE_CMDQ_FULL_DBG_CNT] = rdev->rcfw.cmdq_full_dbg;
	if (!rdev->is_virtfn)
		stats->value[BNXT_RE_FW_SERVICE_PROF_TYPE_SUP] = is_qport_service_type_supported(rdev);

	return ARRAY_SIZE(bnxt_re_stat_descs);
}

struct rdma_hw_stats *bnxt_re_alloc_hw_port_stats(struct ib_device *ibdev,
						     u8 port_num)
{
	return rdma_alloc_hw_stats_struct(bnxt_re_stat_descs,
					  ARRAY_SIZE(bnxt_re_stat_descs),
					  RDMA_HW_STATS_DEFAULT_LIFESPAN);
}
