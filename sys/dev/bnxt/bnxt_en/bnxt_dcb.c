/*-
 * Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2024 Broadcom, All Rights Reserved.
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

#include <sys/endian.h>
#include <linux/errno.h>
#include <linux/bitops.h>

#include "bnxt.h"
#include "bnxt_hwrm.h"
#include "bnxt_dcb.h"
#include "hsi_struct_def.h"

static int
bnxt_tx_queue_to_tc(struct bnxt_softc *softc, uint8_t queue_id)
{
	int i, j;

	for (i = 0; i < softc->max_tc; i++) {
		if (softc->tx_q_info[i].queue_id == queue_id) {
			for (j = 0; j < softc->max_tc; j++) {
				if (softc->tc_to_qidx[j] == i)
					return j;
			}
		}
	}
	return -EINVAL;
}

static int
bnxt_hwrm_queue_pri2cos_cfg(struct bnxt_softc *softc,
				       struct bnxt_ieee_ets *ets,
				       uint32_t path_dir)
{
	struct hwrm_queue_pri2cos_cfg_input req = {0};
	struct bnxt_queue_info *q_info;
	uint8_t *pri2cos;
	int i;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_QUEUE_PRI2COS_CFG);

	req.flags = htole32(path_dir | HWRM_QUEUE_PRI2COS_CFG_INPUT_FLAGS_IVLAN);
	if (path_dir == HWRM_QUEUE_PRI2COS_CFG_INPUT_FLAGS_PATH_BIDIR ||
	    path_dir == HWRM_QUEUE_PRI2COS_CFG_INPUT_FLAGS_PATH_TX)
		q_info = softc->tx_q_info;
	else
		q_info = softc->rx_q_info;
	pri2cos = &req.pri0_cos_queue_id;
	for (i = 0; i < BNXT_IEEE_8021QAZ_MAX_TCS; i++) {
		uint8_t qidx;

		req.enables |= htole32(HWRM_QUEUE_PRI2COS_CFG_INPUT_ENABLES_PRI0_COS_QUEUE_ID << i);

		qidx = softc->tc_to_qidx[ets->prio_tc[i]];
		pri2cos[i] = q_info[qidx].queue_id;
	}
	return _hwrm_send_message(softc, &req, sizeof(req));
}

static int
bnxt_hwrm_queue_pri2cos_qcfg(struct bnxt_softc *softc, struct bnxt_ieee_ets *ets)
{
	struct hwrm_queue_pri2cos_qcfg_output *resp =
		(void *)softc->hwrm_cmd_resp.idi_vaddr;
	struct hwrm_queue_pri2cos_qcfg_input req = {0};
	int rc;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_QUEUE_PRI2COS_QCFG);

	req.flags = htole32(HWRM_QUEUE_PRI2COS_QCFG_INPUT_FLAGS_IVLAN);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	if (!rc) {
		uint8_t *pri2cos = &resp->pri0_cos_queue_id;
		int i;

		for (i = 0; i < BNXT_IEEE_8021QAZ_MAX_TCS; i++) {
			uint8_t queue_id = pri2cos[i];
			int tc;

			tc = bnxt_tx_queue_to_tc(softc, queue_id);
			if (tc >= 0)
				ets->prio_tc[i] = tc;
		}
	}
	return rc;
}

static int
bnxt_hwrm_queue_cos2bw_cfg(struct bnxt_softc *softc, struct bnxt_ieee_ets *ets,
				      uint8_t max_tc)
{
	struct hwrm_queue_cos2bw_cfg_input req = {0};
	struct bnxt_cos2bw_cfg cos2bw;
	void *data;
	int i;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_QUEUE_COS2BW_CFG);

	for (i = 0; i < max_tc; i++) {
		uint8_t qidx = softc->tc_to_qidx[i];

		req.enables |=
			htole32(HWRM_QUEUE_COS2BW_CFG_INPUT_ENABLES_COS_QUEUE_ID0_VALID << qidx);

		memset(&cos2bw, 0, sizeof(cos2bw));
		cos2bw.queue_id = softc->tx_q_info[qidx].queue_id;
		if (ets->tc_tsa[i] == BNXT_IEEE_8021QAZ_TSA_STRICT) {
			cos2bw.tsa =
				HWRM_QUEUE_COS2BW_QCFG_OUTPUT_QUEUE_ID0_TSA_ASSIGN_SP;
			cos2bw.pri_lvl = i;
		} else {
			cos2bw.tsa =
				HWRM_QUEUE_COS2BW_QCFG_OUTPUT_QUEUE_ID0_TSA_ASSIGN_ETS;
			cos2bw.bw_weight = ets->tc_tx_bw[i];
			/* older firmware requires min_bw to be set to the
			 * same weight value in percent.
			 */
			if (BNXT_FW_MAJ(softc) < 218) {
				cos2bw.min_bw =
					htole32((ets->tc_tx_bw[i] * 100) |
						    BW_VALUE_UNIT_PERCENT1_100);
			}
		}
		data = &req.unused_0 + qidx * (sizeof(cos2bw) - 4);
		memcpy(data, &cos2bw.queue_id, sizeof(cos2bw) - 4);
		if (qidx == 0) {
			req.queue_id0 = cos2bw.queue_id;
			req.unused_0 = 0;
		}
	}
	return _hwrm_send_message(softc, &req, sizeof(req));
}

static int
bnxt_hwrm_queue_cos2bw_qcfg(struct bnxt_softc *softc, struct bnxt_ieee_ets *ets)
{
	struct hwrm_queue_cos2bw_qcfg_output *resp =
		(void *)softc->hwrm_cmd_resp.idi_vaddr;
	struct hwrm_queue_cos2bw_qcfg_input req = {0};
	struct bnxt_cos2bw_cfg cos2bw;
	uint8_t *data;
	int rc, i;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_QUEUE_COS2BW_QCFG);

	rc = _hwrm_send_message(softc, &req, sizeof(req));
	if (rc) {
		return rc;
	}

	data = &resp->queue_id0 + offsetof(struct bnxt_cos2bw_cfg, queue_id);
	for (i = 0; i < softc->max_tc; i++, data += sizeof(cos2bw.cfg)) {
		int tc;

		memcpy(&cos2bw.cfg, data, sizeof(cos2bw.cfg));
		if (i == 0)
			cos2bw.queue_id = resp->queue_id0;

		tc = bnxt_tx_queue_to_tc(softc, cos2bw.queue_id);
		if (tc < 0)
			continue;

		if (cos2bw.tsa == HWRM_QUEUE_COS2BW_QCFG_OUTPUT_QUEUE_ID0_TSA_ASSIGN_SP) {
			ets->tc_tsa[tc] = BNXT_IEEE_8021QAZ_TSA_STRICT;
		} else {
			ets->tc_tsa[tc] = BNXT_IEEE_8021QAZ_TSA_ETS;
			ets->tc_tx_bw[tc] = cos2bw.bw_weight;
		}
	}
	return 0;
}

static int
bnxt_queue_remap(struct bnxt_softc *softc, unsigned int lltc_mask)
{
	unsigned long qmap = 0;
	int max = softc->max_tc;
	int i, j, rc;

	/* Assign lossless TCs first */
	for (i = 0, j = 0; i < max; ) {
		if (lltc_mask & (1 << i)) {
			if (BNXT_LLQ(softc->rx_q_info[j].queue_profile)) {
				softc->tc_to_qidx[i] = j;
				__set_bit(j, &qmap);
				i++;
			}
			j++;
			continue;
		}
		i++;
	}

	for (i = 0, j = 0; i < max; i++) {
		if (lltc_mask & (1 << i))
			continue;
		j = find_next_zero_bit(&qmap, max, j);
		softc->tc_to_qidx[i] = j;
		__set_bit(j, &qmap);
		j++;
	}

	if (softc->ieee_ets) {
		rc = bnxt_hwrm_queue_cos2bw_cfg(softc, softc->ieee_ets, softc->max_tc);
		if (rc) {
			device_printf(softc->dev, "failed to config BW, rc = %d\n", rc);
			return rc;
		}
		rc = bnxt_hwrm_queue_pri2cos_cfg(softc, softc->ieee_ets,
						 HWRM_QUEUE_PRI2COS_CFG_INPUT_FLAGS_PATH_BIDIR);
		if (rc) {
			device_printf(softc->dev, "failed to config prio, rc = %d\n", rc);
			return rc;
		}
	}
	return 0;
}

static int
bnxt_hwrm_queue_pfc_cfg(struct bnxt_softc *softc, struct bnxt_ieee_pfc *pfc)
{
	struct hwrm_queue_pfcenable_cfg_input req = {0};
	struct bnxt_ieee_ets *my_ets = softc->ieee_ets;
	unsigned int tc_mask = 0, pri_mask = 0;
	uint8_t i, pri, lltc_count = 0;
	bool need_q_remap = false;

	if (!my_ets)
		return -EINVAL;

	for (i = 0; i < softc->max_tc; i++) {
		for (pri = 0; pri < BNXT_IEEE_8021QAZ_MAX_TCS; pri++) {
			if ((pfc->pfc_en & (1 << pri)) &&
			    (my_ets->prio_tc[pri] == i)) {
				pri_mask |= 1 << pri;
				tc_mask |= 1 << i;
			}
		}
		if (tc_mask & (1 << i))
			lltc_count++;
	}

	if (lltc_count > softc->max_lltc) {
		device_printf(softc->dev,
			       "Hardware doesn't support %d lossless queues "
			       "to configure PFC (cap %d)\n", lltc_count, softc->max_lltc);
		return -EINVAL;
	}

	for (i = 0; i < softc->max_tc; i++) {
		if (tc_mask & (1 << i)) {
			uint8_t qidx = softc->tc_to_qidx[i];

			if (!BNXT_LLQ(softc->rx_q_info[qidx].queue_profile)) {
				need_q_remap = true;
				break;
			}
		}
	}

	if (need_q_remap)
		bnxt_queue_remap(softc, tc_mask);

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_QUEUE_PFCENABLE_CFG);

	req.flags = htole32(pri_mask);
	return _hwrm_send_message(softc, &req, sizeof(req));
}

static int
bnxt_hwrm_queue_pfc_qcfg(struct bnxt_softc *softc, struct bnxt_ieee_pfc *pfc)
{
	struct hwrm_queue_pfcenable_qcfg_output *resp =
		(void *)softc->hwrm_cmd_resp.idi_vaddr;
	struct hwrm_queue_pfcenable_qcfg_input req = {0};
	uint8_t pri_mask;
	int rc;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_QUEUE_PFCENABLE_QCFG);

	rc = _hwrm_send_message(softc, &req, sizeof(req));
	if (rc) {
		return rc;
	}

	pri_mask = le32toh(resp->flags);
	pfc->pfc_en = pri_mask;
	return 0;
}

static int
bnxt_hwrm_get_dcbx_app(struct bnxt_softc *softc, struct bnxt_dcb_app *app,
    size_t nitems, int *num_inputs)
{
	struct hwrm_fw_get_structured_data_input get = {0};
	struct hwrm_struct_data_dcbx_app *fw_app;
	struct hwrm_struct_hdr *data;
	struct iflib_dma_info dma_data;
	size_t data_len;
	int rc, n, i;

	if (softc->hwrm_spec_code < 0x10601)
		return 0;

	bnxt_hwrm_cmd_hdr_init(softc, &get, HWRM_FW_GET_STRUCTURED_DATA);

	n = BNXT_IEEE_8021QAZ_MAX_TCS;
	data_len = sizeof(*data) + sizeof(*fw_app) * n;
	rc = iflib_dma_alloc(softc->ctx, data_len, &dma_data,
			     BUS_DMA_NOWAIT);
	if (rc)
		return ENOMEM;
	get.dest_data_addr = htole64(dma_data.idi_paddr);
	get.structure_id = htole16(HWRM_STRUCT_HDR_STRUCT_ID_DCBX_APP);
	get.subtype = htole16(HWRM_STRUCT_DATA_SUBTYPE_HOST_OPERATIONAL);
	get.count = 0;
	rc = _hwrm_send_message(softc, &get, sizeof(get));
	if (rc)
		goto set_app_exit;

	data = (void *)dma_data.idi_vaddr;
	fw_app = (struct hwrm_struct_data_dcbx_app *)(data + 1);

	if (data->struct_id != htole16(HWRM_STRUCT_HDR_STRUCT_ID_DCBX_APP)) {
		rc = -ENODEV;
		goto set_app_exit;
	}

	n = data->count;
	for (i = 0; i < n && *num_inputs < nitems; i++, fw_app++) {
		app[*num_inputs].priority = fw_app->priority;
		app[*num_inputs].protocol = htobe16(fw_app->protocol_id);
		app[*num_inputs].selector = fw_app->protocol_selector;
		(*num_inputs)++;
	}

set_app_exit:
	iflib_dma_free(&dma_data);
	return rc;
}

static int
bnxt_hwrm_set_dcbx_app(struct bnxt_softc *softc, struct bnxt_dcb_app *app,
				  bool add)
{
	struct hwrm_fw_set_structured_data_input set = {0};
	struct hwrm_fw_get_structured_data_input get = {0};
	struct hwrm_struct_data_dcbx_app *fw_app;
	struct hwrm_struct_hdr *data;
	struct iflib_dma_info dma_data;
	size_t data_len;
	int rc, n, i;

	if (softc->hwrm_spec_code < 0x10601)
		return 0;

	bnxt_hwrm_cmd_hdr_init(softc, &get, HWRM_FW_GET_STRUCTURED_DATA);

	n = BNXT_IEEE_8021QAZ_MAX_TCS;
	data_len = sizeof(*data) + sizeof(*fw_app) * n;
	rc = iflib_dma_alloc(softc->ctx, data_len, &dma_data,
			     BUS_DMA_NOWAIT);
	if (rc)
		return ENOMEM;
	get.dest_data_addr = htole64(dma_data.idi_paddr);
	get.structure_id = htole16(HWRM_STRUCT_HDR_STRUCT_ID_DCBX_APP);
	get.subtype = htole16(HWRM_STRUCT_DATA_SUBTYPE_HOST_OPERATIONAL);
	get.count = 0;
	rc = _hwrm_send_message(softc, &get, sizeof(get));
	if (rc)
		goto set_app_exit;

	data = (void *)dma_data.idi_vaddr;
	fw_app = (struct hwrm_struct_data_dcbx_app *)(data + 1);

	if (data->struct_id != htole16(HWRM_STRUCT_HDR_STRUCT_ID_DCBX_APP)) {
		rc = -ENODEV;
		goto set_app_exit;
	}

	n = data->count;
	for (i = 0; i < n; i++, fw_app++) {
		if (fw_app->protocol_id == htobe16(app->protocol) &&
		    fw_app->protocol_selector == app->selector &&
		    fw_app->priority == app->priority) {
			if (add)
				goto set_app_exit;
			else
				break;
		}
	}
	if (add) {
		/* append */
		n++;
		fw_app->protocol_id = htobe16(app->protocol);
		fw_app->protocol_selector = app->selector;
		fw_app->priority = app->priority;
		fw_app->valid = 1;
	} else {
		size_t len = 0;

		/* not found, nothing to delete */
		if (n == i)
			goto set_app_exit;

		len = (n - 1 - i) * sizeof(*fw_app);
		if (len)
			memmove(fw_app, fw_app + 1, len);
		n--;
		memset(fw_app + n, 0, sizeof(*fw_app));
	}
	data->count = n;
	data->len = htole16(sizeof(*fw_app) * n);
	data->subtype = htole16(HWRM_STRUCT_DATA_SUBTYPE_HOST_OPERATIONAL);

	bnxt_hwrm_cmd_hdr_init(softc, &set, HWRM_FW_SET_STRUCTURED_DATA);

	set.src_data_addr = htole64(dma_data.idi_paddr);
	set.data_len = htole16(sizeof(*data) + sizeof(*fw_app) * n);
	set.hdr_cnt = 1;
	rc = _hwrm_send_message(softc, &set, sizeof(set));

set_app_exit:
	iflib_dma_free(&dma_data);
	return rc;
}

static int
bnxt_hwrm_queue_dscp_qcaps(struct bnxt_softc *softc)
{
	struct hwrm_queue_dscp_qcaps_output *resp =
		(void *)softc->hwrm_cmd_resp.idi_vaddr;
	struct hwrm_queue_dscp_qcaps_input req = {0};
	int rc;

	softc->max_dscp_value = 0;
	if (softc->hwrm_spec_code < 0x10800 || BNXT_VF(softc))
		return 0;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_QUEUE_DSCP_QCAPS);

	rc = _hwrm_send_message(softc, &req, sizeof(req));
	if (!rc) {
		softc->max_dscp_value = (1 << resp->num_dscp_bits) - 1;
		if (softc->max_dscp_value < 0x3f)
			softc->max_dscp_value = 0;
	}
	return rc;
}

static int
bnxt_hwrm_queue_dscp2pri_qcfg(struct bnxt_softc *softc, struct bnxt_dcb_app *app,
    size_t nitems, int *num_inputs)
{
	struct hwrm_queue_dscp2pri_qcfg_input req = {0};
	struct hwrm_queue_dscp2pri_qcfg_output *resp =
		(void *)softc->hwrm_cmd_resp.idi_vaddr;
	struct bnxt_dscp2pri_entry *dscp2pri;
	struct iflib_dma_info dma_data;
	int rc, entry_cnt;
	int i;

	if (softc->hwrm_spec_code < 0x10800)
		return 0;

	rc = iflib_dma_alloc(softc->ctx, sizeof(*dscp2pri) * 128, &dma_data,
			     BUS_DMA_NOWAIT);
	if (rc)
		return ENOMEM;

	dscp2pri = (void *)dma_data.idi_vaddr;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_QUEUE_DSCP2PRI_QCFG);

	req.dest_data_addr = htole64(dma_data.idi_paddr);
	req.dest_data_buffer_size = htole16(sizeof(*dscp2pri) * 64);
	req.port_id = htole16(softc->pf.port_id);
	rc = _hwrm_send_message(softc, &req, sizeof(req));

	if (rc)
		goto end;

	entry_cnt =  le16toh(resp->entry_cnt);
	for (i = 0; i < entry_cnt && *num_inputs < nitems; i++) {
		app[*num_inputs].priority = dscp2pri[i].pri;
		app[*num_inputs].protocol = dscp2pri[i].dscp;
		app[*num_inputs].selector = BNXT_IEEE_8021QAZ_APP_SEL_DSCP;
		(*num_inputs)++;
	}

end:
	iflib_dma_free(&dma_data);
	return rc;
}

static int
bnxt_hwrm_queue_dscp2pri_cfg(struct bnxt_softc *softc, struct bnxt_dcb_app *app,
			     bool add)
{
	struct hwrm_queue_dscp2pri_cfg_input req = {0};
	struct bnxt_dscp2pri_entry *dscp2pri;
	struct iflib_dma_info dma_data;
	int rc;

	if (softc->hwrm_spec_code < 0x10800)
		return 0;

	rc = iflib_dma_alloc(softc->ctx, sizeof(*dscp2pri), &dma_data,
			     BUS_DMA_NOWAIT);
	if (rc)
		return ENOMEM;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_QUEUE_DSCP2PRI_CFG);

	req.src_data_addr = htole64(dma_data.idi_paddr);
	dscp2pri = (void *)dma_data.idi_vaddr;
	dscp2pri->dscp = app->protocol;
	if (add)
		dscp2pri->mask = 0x3f;
	else
		dscp2pri->mask = 0;
	dscp2pri->pri = app->priority;
	req.entry_cnt = htole16(1);
	req.port_id = htole16(softc->pf.port_id);
	rc = _hwrm_send_message(softc, &req, sizeof(req));

	iflib_dma_free(&dma_data);
	return rc;
}

static int
bnxt_ets_validate(struct bnxt_softc *softc, struct bnxt_ieee_ets *ets, uint8_t *tc)
{
	int total_ets_bw = 0;
	bool zero = false;
	uint8_t max_tc = 0;
	int i;

	for (i = 0; i < BNXT_IEEE_8021QAZ_MAX_TCS; i++) {
		if (ets->prio_tc[i] > softc->max_tc) {
			device_printf(softc->dev, "priority to TC mapping exceeds TC count %d\n",
				   ets->prio_tc[i]);
			return -EINVAL;
		}
		if (ets->prio_tc[i] > max_tc)
			max_tc = ets->prio_tc[i];

		if ((ets->tc_tx_bw[i] || ets->tc_tsa[i]) && i > softc->max_tc)
			return -EINVAL;

		switch (ets->tc_tsa[i]) {
		case BNXT_IEEE_8021QAZ_TSA_STRICT:
			break;
		case BNXT_IEEE_8021QAZ_TSA_ETS:
			total_ets_bw += ets->tc_tx_bw[i];
			zero = zero || !ets->tc_tx_bw[i];
			break;
		default:
			return -ENOTSUPP;
		}
	}
	if (total_ets_bw > 100) {
		device_printf(softc->dev, "rejecting ETS config exceeding available bandwidth\n");
		return -EINVAL;
	}
	if (zero && total_ets_bw == 100) {
		device_printf(softc->dev, "rejecting ETS config starving a TC\n");
		return -EINVAL;
	}

	if (max_tc >= softc->max_tc)
		*tc = softc->max_tc;
	else
		*tc = max_tc + 1;
	return 0;
}

int
bnxt_dcb_ieee_getets(struct bnxt_softc *softc, struct bnxt_ieee_ets *ets)
{
	struct bnxt_ieee_ets *my_ets = softc->ieee_ets;
	int rc;

	if (!my_ets)
		return 0;

	rc = bnxt_hwrm_queue_cos2bw_qcfg(softc, my_ets);
	if (rc)
		goto error;
	rc = bnxt_hwrm_queue_pri2cos_qcfg(softc, my_ets);
	if (rc)
		goto error;

	if (ets) {
		ets->cbs = my_ets->cbs;
		ets->ets_cap = softc->max_tc;
		memcpy(ets->tc_tx_bw, my_ets->tc_tx_bw, sizeof(ets->tc_tx_bw));
		memcpy(ets->tc_rx_bw, my_ets->tc_rx_bw, sizeof(ets->tc_rx_bw));
		memcpy(ets->tc_tsa, my_ets->tc_tsa, sizeof(ets->tc_tsa));
		memcpy(ets->prio_tc, my_ets->prio_tc, sizeof(ets->prio_tc));
	}
	return 0;
error:
	return rc;
}

int
bnxt_dcb_ieee_setets(struct bnxt_softc *softc, struct bnxt_ieee_ets *ets)
{
	uint8_t max_tc = 0;
	int rc;

	if (!(softc->dcbx_cap & BNXT_DCB_CAP_DCBX_VER_IEEE) ||
	    !(softc->dcbx_cap & BNXT_DCB_CAP_DCBX_HOST))
		return -EINVAL;

	rc = bnxt_ets_validate(softc, ets, &max_tc);
	if (rc)
		return rc;

	rc = bnxt_hwrm_queue_cos2bw_cfg(softc, ets, max_tc);
	if (rc)
		goto error;

	if (!softc->is_asym_q) {
		rc = bnxt_hwrm_queue_pri2cos_cfg(softc, ets,
						 HWRM_QUEUE_PRI2COS_CFG_INPUT_FLAGS_PATH_BIDIR);
		if (rc)
			goto error;
	} else {
		rc = bnxt_hwrm_queue_pri2cos_cfg(softc, ets,
						 HWRM_QUEUE_PRI2COS_QCFG_INPUT_FLAGS_PATH_TX);
		if (rc)
			goto error;

		rc = bnxt_hwrm_queue_pri2cos_cfg(softc, ets,
						 HWRM_QUEUE_PRI2COS_QCFG_INPUT_FLAGS_PATH_RX);
		if (rc)
			goto error;
	}

	memcpy(softc->ieee_ets, ets, sizeof(*ets));
	return 0;
error:
	return rc;
}

int
bnxt_dcb_ieee_getpfc(struct bnxt_softc *softc, struct bnxt_ieee_pfc *pfc)
{
	struct bnxt_ieee_pfc *my_pfc = softc->ieee_pfc;
	int rc;

	if (!my_pfc)
		return -1;

	pfc->pfc_cap = softc->max_lltc;

	rc = bnxt_hwrm_queue_pfc_qcfg(softc, my_pfc);
	if (rc)
		return 0;

	pfc->pfc_en = my_pfc->pfc_en;
	pfc->mbc = my_pfc->mbc;
	pfc->delay = my_pfc->delay;

	return 0;
}

int
bnxt_dcb_ieee_setpfc(struct bnxt_softc *softc, struct bnxt_ieee_pfc *pfc)
{
	struct bnxt_ieee_pfc *my_pfc = softc->ieee_pfc;
	int rc;

	if (!my_pfc)
		return -1;

	if (!(softc->dcbx_cap & BNXT_DCB_CAP_DCBX_VER_IEEE) ||
	    !(softc->dcbx_cap & BNXT_DCB_CAP_DCBX_HOST) ||
	    (softc->phy_flags & BNXT_PHY_FL_NO_PAUSE))
		return -EINVAL;

	rc = bnxt_hwrm_queue_pfc_cfg(softc, pfc);
	if (!rc)
		memcpy(my_pfc, pfc, sizeof(*my_pfc));

	return rc;
}

static int
bnxt_dcb_ieee_dscp_app_prep(struct bnxt_softc *softc, struct bnxt_dcb_app *app)
{
	if (app->selector == BNXT_IEEE_8021QAZ_APP_SEL_DSCP) {
		if (!softc->max_dscp_value)
			return -ENOTSUPP;
		if (app->protocol > softc->max_dscp_value)
			return -EINVAL;
	}
	return 0;
}

int
bnxt_dcb_ieee_setapp(struct bnxt_softc *softc, struct bnxt_dcb_app *app)
{
	int rc;


	if (!(softc->dcbx_cap & BNXT_DCB_CAP_DCBX_VER_IEEE) ||
	    !(softc->dcbx_cap & BNXT_DCB_CAP_DCBX_HOST))
		return -EINVAL;

	rc = bnxt_dcb_ieee_dscp_app_prep(softc, app);
	if (rc)
		return rc;

	if ((app->selector == BNXT_IEEE_8021QAZ_APP_SEL_ETHERTYPE &&
	     app->protocol == ETH_P_ROCE) ||
	    (app->selector == BNXT_IEEE_8021QAZ_APP_SEL_DGRAM &&
	     app->protocol == ROCE_V2_UDP_DPORT))
		rc = bnxt_hwrm_set_dcbx_app(softc, app, true);

	if (app->selector == BNXT_IEEE_8021QAZ_APP_SEL_DSCP)
		rc = bnxt_hwrm_queue_dscp2pri_cfg(softc, app, true);

	return rc;
}

int
bnxt_dcb_ieee_delapp(struct bnxt_softc *softc, struct bnxt_dcb_app *app)
{
	int rc;

	if (!(softc->dcbx_cap & BNXT_DCB_CAP_DCBX_VER_IEEE) ||
	    !(softc->dcbx_cap & BNXT_DCB_CAP_DCBX_HOST))
		return -EINVAL;

	rc = bnxt_dcb_ieee_dscp_app_prep(softc, app);
	if (rc)
		return rc;

	if ((app->selector == BNXT_IEEE_8021QAZ_APP_SEL_ETHERTYPE &&
	     app->protocol == ETH_P_ROCE) ||
	    (app->selector == BNXT_IEEE_8021QAZ_APP_SEL_DGRAM &&
	     app->protocol == ROCE_V2_UDP_DPORT))
		rc = bnxt_hwrm_set_dcbx_app(softc, app, false);

	if (app->selector == BNXT_IEEE_8021QAZ_APP_SEL_DSCP)
		rc = bnxt_hwrm_queue_dscp2pri_cfg(softc, app, false);

	return rc;
}

int
bnxt_dcb_ieee_listapp(struct bnxt_softc *softc, struct bnxt_dcb_app *app,
    size_t nitems, int *num_inputs)
{
	bnxt_hwrm_get_dcbx_app(softc, app, nitems, num_inputs);
	bnxt_hwrm_queue_dscp2pri_qcfg(softc, app, nitems, num_inputs);

	return 0;
}

uint8_t
bnxt_dcb_getdcbx(struct bnxt_softc *softc)
{
	return softc->dcbx_cap;
}

uint8_t
bnxt_dcb_setdcbx(struct bnxt_softc *softc, uint8_t mode)
{
	/* All firmware DCBX settings are set in NVRAM */
	if (softc->dcbx_cap & BNXT_DCB_CAP_DCBX_LLD_MANAGED)
		return 1;

	/*
	 * Do't allow editing CAP_DCBX_LLD_MANAGED since it is driven
	 * based on FUNC_QCFG_OUTPUT_FLAGS_FW_DCBX_AGENT_ENABLED
	 */
	if ((softc->dcbx_cap & BNXT_DCB_CAP_DCBX_LLD_MANAGED) !=
	    (mode & BNXT_DCB_CAP_DCBX_LLD_MANAGED))
		return 1;

	if (mode & BNXT_DCB_CAP_DCBX_HOST) {
		if (BNXT_VF(softc) || (softc->fw_cap & BNXT_FW_CAP_LLDP_AGENT))
			return 1;

		/* only support BNXT_IEEE */
		if ((mode & BNXT_DCB_CAP_DCBX_VER_CEE) ||
		    !(mode & BNXT_DCB_CAP_DCBX_VER_IEEE))
			return 1;
	}

	if (mode == softc->dcbx_cap)
		return 0;

	softc->dcbx_cap = mode;
	return 0;
}

void
bnxt_dcb_init(struct bnxt_softc *softc)
{
	struct bnxt_ieee_ets ets = {0};
	struct bnxt_ieee_pfc pfc = {0};

	softc->dcbx_cap = 0;

	if (softc->hwrm_spec_code < 0x10501)
		return;

	softc->ieee_ets = malloc(sizeof(struct bnxt_ieee_ets), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (!softc->ieee_ets)
		return;

	softc->ieee_pfc = malloc(sizeof(struct bnxt_ieee_pfc), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (!softc->ieee_pfc)
		return;

	bnxt_hwrm_queue_dscp_qcaps(softc);
	softc->dcbx_cap = BNXT_DCB_CAP_DCBX_VER_IEEE;
	if (BNXT_PF(softc) && !(softc->fw_cap & BNXT_FW_CAP_LLDP_AGENT))
		softc->dcbx_cap |= BNXT_DCB_CAP_DCBX_HOST;
	else if (softc->fw_cap & BNXT_FW_CAP_DCBX_AGENT)
		softc->dcbx_cap |= BNXT_DCB_CAP_DCBX_LLD_MANAGED;

	bnxt_dcb_ieee_setets(softc, &ets);
	bnxt_dcb_ieee_setpfc(softc, &pfc);

}

void
bnxt_dcb_free(struct bnxt_softc *softc)
{
	free(softc->ieee_ets, M_DEVBUF);
	softc->ieee_ets = NULL;
	free(softc->ieee_pfc, M_DEVBUF);
	softc->ieee_pfc = NULL;
}
