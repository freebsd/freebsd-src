/*
 * Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2026 Broadcom, All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
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

#include <sys/param.h>
#include <sys/time.h>
#include <sys/clock.h>
#include <sys/seqc.h>
#include <machine/atomic.h>
#include "bnxt_ptp.h"
#include "bnxt_hwrm.h"

static int
bnxt_map_ptp_regs(struct bnxt_softc *bp)
{
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;
	uint32_t *reg_arr;
	int i;

	reg_arr = ptp->refclk_regs;
	for (i = 0; i < 2; i++) {
		if (reg_arr[i] & BNXT_GRC_BASE_MASK)
			return (EINVAL);
		ptp->refclk_mapped_regs[i] = ptp->refclk_regs[i];
	}

	return (0);
}

static int
bnxt_ptp_cfg_settime(struct bnxt_softc *bp, uint64_t time)
{
	struct hwrm_func_ptp_cfg_input req = {0};

	bnxt_hwrm_cmd_hdr_init(bp, &req, HWRM_FUNC_PTP_CFG);

	req.enables = htole16(HWRM_FUNC_PTP_CFG_INPUT_ENABLES_PTP_SET_TIME);
	req.ptp_set_time = htole64(time);
	return (hwrm_send_message(bp, &req, sizeof(req)));
}

static int
bnxt_ptp_init_rtc(struct bnxt_softc *bp, bool phc_cfg)
{
	struct timespec ts;
	uint64_t ns;
	int rc;

	if (!bp->ptp_cfg || !BNXT_PTP_USE_RTC(bp))
		return (ENODEV);

	if (!phc_cfg) {
		nanotime(&ts);
		ns = (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
		rc = bnxt_ptp_cfg_settime(bp, ns);
		if (rc)
			return (rc);
	}

	return (0);
}

static int
__bnxt_refclk_read(struct bnxt_softc *bp, uint64_t *ns)
{
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;
	uint32_t high_before, high_now, low;

	if (bnxt_drv_state_test(bp, BNXT_STATE_IN_FW_RESET))
		return (EIO);

	/* Refclk registers live in the HWRM BAR, not the doorbell BAR. */
	high_before = readl_fbsd(bp, ptp->refclk_mapped_regs[1], BNXT_HWRM_BAR_IDX);
	low = readl_fbsd(bp, ptp->refclk_mapped_regs[0], BNXT_HWRM_BAR_IDX);
	high_now = readl_fbsd(bp, ptp->refclk_mapped_regs[1], BNXT_HWRM_BAR_IDX);
	if (high_now != high_before)
		low = readl_fbsd(bp, ptp->refclk_mapped_regs[0], BNXT_HWRM_BAR_IDX);
	*ns = (((uint64_t)high_now) << 32) | ((uint64_t)low);

	return (0);
}

static int
bnxt_refclk_read(struct bnxt_softc *bp, uint64_t *ns)
{
	int rc;

	/* Serializes reg access vs FW reset. Must stay a spin mutex: the
	 * calibration callout runs C_DIRECT_EXEC, sometimes from interrupt context. */
	mtx_lock_spin(&bp->ptp_cfg->ptp_lock);
	rc = __bnxt_refclk_read(bp, ns);
	mtx_unlock_spin(&bp->ptp_cfg->ptp_lock);
	return (rc);
}

uint64_t
bnxt_ptp_hwtstamp_to_ns(struct bnxt_ptp_cfg *priv, uint64_t hw_tstmp)
{
	struct bnxt_clbr_point *cp, dcp;
	sbintime_t sbt_cur_to_prev, sbt;
	uint64_t hw_clocks;
	uint64_t hw_clk_div;
	seqc_t gen;

	for (;;) {
		cp = &priv->clbr_points[priv->clbr_curr];
		/* Not seqc_read(): a permanently-invalidated slot stays odd
		 * forever, which would spin here indefinitely. Bail out instead. */
		gen = seqc_read_any(&cp->gen_ts);
		if (seqc_in_modify(gen))
			return (0);
		dcp = *cp;
		if (seqc_consistent(&cp->gen_ts, gen))
			break;
	}

	/* Interpolate: (hw_tstmp - hw_prev) * (cur_time - prev_time) /
	 * (hw_cur - hw_prev) + prev_time, using integer math to avoid float/overflow. */
	if (hw_tstmp < dcp.hw_prev_ts)
		return (0);

	hw_clocks = hw_tstmp - dcp.hw_prev_ts;
	sbt_cur_to_prev = (dcp.sbt_cur_ts - dcp.sbt_prev_ts);
	hw_clk_div = dcp.hw_curr_ts - dcp.hw_prev_ts;

	if (!hw_clk_div)
		return (0);

	/* Reject stale calibration intervals; also bounds the math below. */
	if (hw_clk_div > BNXT_PTP_MAX_CLBR_INTERVAL_NS)
		return (0);

	/* Allow extrapolating up to one interval past hw_curr_ts; most RX
	 * timestamps land there, not inside [hw_prev_ts, hw_curr_ts]. */
	if (hw_clocks > 2 * hw_clk_div)
		return (0);

	/* Split the divide so hw_clocks * remainder can't overflow 64 bits. */
	sbt = hw_clocks * (sbt_cur_to_prev / hw_clk_div) +
	    hw_clocks * (sbt_cur_to_prev % hw_clk_div) / hw_clk_div +
	    dcp.sbt_prev_ts;
	return (sbttons(sbt));
}

static void
bnxt_ptp_get_current_time(struct bnxt_softc *bp)
{
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;

	if (!ptp)
		return;

	atomic_store_64((volatile uint64_t *)&ptp->old_time,
			ptp->current_time);
	bnxt_refclk_read(bp, &ptp->current_time);
}

static void
bnxt_calibration_callout(void *arg)
{
	struct bnxt_ptp_cfg *priv;
	struct bnxt_clbr_point *next, *curr;
	int clbr_curr_next;
	sbintime_t sbt;
	uint64_t nxt_clbr_hw_curr;

	priv = arg;
	curr = &priv->clbr_points[priv->clbr_curr];
	clbr_curr_next = priv->clbr_curr + 1;
	if (clbr_curr_next >= nitems(priv->clbr_points))
		clbr_curr_next = 0;
	next = &priv->clbr_points[clbr_curr_next];

	bnxt_ptp_get_current_time(priv->bp);
	sbt = sbinuptime();
	nxt_clbr_hw_curr = priv->current_time;

	if (priv->clbr_ticks &&
	    ((nxt_clbr_hw_curr - curr->hw_curr_ts) >> BNXT_TSTMP_PREC) == 0) {
		/* A stalled tick isn't necessarily a dead counter (HW may briefly
		 * not count right after a reset); only disable after MAX_CLBR_STALLS. */
		if (++priv->clbr_stall_cnt < BNXT_PTP_MAX_CLBR_STALLS) {
			bnxt_reset_calibration_callout(priv);
			return;
		}
		device_printf(priv->bp->dev,
			"HW failed tstmp frozen, current %#jx prev %#jx, disabling\n",
			(uintmax_t)nxt_clbr_hw_curr, (uintmax_t)curr->hw_curr_ts);
		priv->clbr_ticks = 0;
		priv->clbr_stall_cnt = 0;
		/* Permanently invalidate by leaving gen_ts odd (seqcount's in-modify
		 * state); not via seqc_write_begin() since we never pair it with _end(). */
		atomic_store_rel_int(&curr->gen_ts, curr->gen_ts + SEQC_MOD);
		return;
	}

	priv->clbr_stall_cnt = 0;
	seqc_write_begin(&next->gen_ts);
	next->sbt_prev_ts = curr->sbt_cur_ts;
	next->hw_prev_ts = curr->hw_curr_ts;
	next->hw_curr_ts = (priv->current_time) & BNXT_TIMER_MASK;
	next->sbt_cur_ts = sbt;
	priv->clbr_curr = clbr_curr_next;
	seqc_write_end(&next->gen_ts);

	priv->clbr_ticks++;
	bnxt_reset_calibration_callout(priv);
}

void
bnxt_reset_calibration_callout(struct bnxt_ptp_cfg *priv)
{
	if (priv->clbr_ticks == 0)
		bnxt_calibration_callout(priv);
	else
		callout_reset_sbt_curcpu(&priv->tstmp_clbr, SBT_1S, 0,
			bnxt_calibration_callout, priv, C_DIRECT_EXEC);
}

void
bnxt_ptp_stop_calibration(struct bnxt_ptp_cfg *priv)
{
	/* Reset clbr_ticks so the next bnxt_reset_calibration_callout() calibrates
	 * immediately; clbr_points is stale after a down/up cycle. */
	priv->clbr_ticks = 0;
	priv->clbr_stall_cnt = 0;
	memset(priv->clbr_points, 0, sizeof(priv->clbr_points));
}

void
bnxt_ptp_free(struct bnxt_softc *bp)
{
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;

	if (!ptp)
		return;

	/* ptp_lock and tstmp_clbr are set up together, so drain the callout
	 * before destroying the lock it takes, in case one is still in flight. */
	if (mtx_initialized(&ptp->ptp_lock)) {
		callout_drain(&ptp->tstmp_clbr);
		mtx_destroy(&ptp->ptp_lock);
	}
	free(ptp, M_DEVBUF);
	bp->ptp_cfg = NULL;
}

static int
bnxt_ptp_init(struct bnxt_softc *bp)
{
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;
	int rc;

	if (!ptp)
		return (0);

	rc = bnxt_map_ptp_regs(bp);
	if (rc)
		return (rc);

	if (BNXT_PTP_USE_RTC(bp)) {
		rc = bnxt_ptp_init_rtc(bp, ptp->rtc_configured);
		if (rc)
			goto out;
	}

	mtx_init(&ptp->ptp_lock, "BNXT PTP LOCK", NULL,
	    MTX_SPIN | MTX_NOWITNESS);
	/* Init unconditionally so bnxt_ptp_free() can always safely drain it
	 * whenever ptp_lock is initialized. */
	callout_init(&ptp->tstmp_clbr, 1);

	bnxt_refclk_read(bp, &ptp->current_time);
	atomic_store_64((volatile uint64_t *)&ptp->old_time, ptp->current_time);

	return (0);
out:
	/* Nothing to unmap: bnxt_map_ptp_regs() rejects GRC-window regs before mapping. */
	return (rc);
}

int
bnxt_hwrm_ptp_qcfg(struct bnxt_softc *bp)
{
	struct hwrm_port_mac_ptp_qcfg_output *resp =
	    (void *)bp->hwrm_cmd_resp.idi_vaddr;
	struct hwrm_port_mac_ptp_qcfg_input req = {0};
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;
	uint8_t flags;
	int rc;

	if (!BNXT_CHIP_P7(bp)) {
		rc = ENODEV;
		goto no_ptp;
	}

	bnxt_hwrm_cmd_hdr_init(bp, &req, HWRM_PORT_MAC_PTP_QCFG);

	req.port_id = htole16(bp->pf.port_id);
	rc = hwrm_send_message(bp, &req, sizeof(req));
	if (rc)
		goto exit;

	flags = resp->flags;
	if (!ptp)
		ptp = malloc(sizeof(*ptp), M_DEVBUF, M_NOWAIT | M_ZERO);

	if (!ptp) {
		rc = ENOMEM;
		goto exit;
	}

	if (flags &
	  (HWRM_PORT_MAC_PTP_QCFG_OUTPUT_FLAGS_PARTIAL_DIRECT_ACCESS_REF_CLOCK |
	  HWRM_PORT_MAC_PTP_QCFG_OUTPUT_FLAGS_64B_PHC_TIME)) {
		ptp->refclk_regs[0] = le32toh(resp->ts_ref_clock_reg_lower);
		ptp->refclk_regs[1] = le32toh(resp->ts_ref_clock_reg_upper);
	}

	ptp->bp = bp;
	bp->ptp_cfg = ptp;

	ptp->rtc_configured =
	  (flags & HWRM_PORT_MAC_PTP_QCFG_OUTPUT_FLAGS_RTC_CONFIGURED) != 0;
	rc = bnxt_ptp_init(bp);
	if (rc)
		device_printf(bp->dev, "PTP initialization failed.\n");
exit:
	if (!rc)
		return (0);

no_ptp:
	/* Use bnxt_ptp_free(), not free(): ptp may already have a mutex/callout
	 * set up from a prior call, which a plain free() would leak. */
	bnxt_ptp_free(bp);
	return (rc);
}

int
bnxt_ptp_cfg_tstamp_filters(struct bnxt_softc *bp)
{
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;
	struct hwrm_port_mac_cfg_input req = {0};
	int rc;

	if (!ptp)
		return (EOPNOTSUPP);

	bnxt_hwrm_cmd_hdr_init(bp, &req, HWRM_PORT_MAC_CFG);

	if (!(bp->fw_cap & BNXT_FW_CAP_RX_ALL_PKT_TS)) {
		device_printf(bp->dev,
		    "Unsupported FW for all RX pkts timestamp filter\n");
		return (EOPNOTSUPP);
	}

	req.flags =
	    htole32(HWRM_PORT_MAC_CFG_INPUT_FLAGS_ALL_RX_TS_CAPTURE_ENABLE);
	req.enables =
	    htole32(HWRM_PORT_MAC_CFG_INPUT_ENABLES_RX_TS_CAPTURE_PTP_MSG_TYPE);

	rc = hwrm_send_message(bp, &req, sizeof(req));
	if (rc)
		device_printf(bp->dev,
		    "Failed to configure HW packet timestamp filters\n");

	return (rc);
}
