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

#ifndef BNXT_PTP_H
#define BNXT_PTP_H

#include "bnxt.h"

/*
 * BNXT_GRCPF_REG_CHIMP_COMM, BNXT_GRCPF_REG_CHIMP_COMM_TRIGGER,
 * BNXT_GRCPF_REG_WINDOW_BASE_OUT, BNXT_GRC_BASE_MASK, and
 * BNXT_GRC_OFFSET_MASK are already defined in bnxt.h, included above.
 */

#define BNXT_TIMER_MASK	0x0000ffffffffffffUL

#define BNXT_LO_TIMER_MASK	0x0000ffffffffUL
#define BNXT_HI_TIMER_MASK	0xffff00000000UL

#define BNXT_PTP_USE_RTC(bp)	((bp)->fw_cap & BNXT_FW_CAP_PTP_RTC)

#define BNXT_TSTMP_PREC 10

/* 1s cadence plus callout jitter; wider gaps are too stale to use. */
#define BNXT_PTP_MAX_CLBR_INTERVAL_NS	2000000000ULL

/* Consecutive stalled 1s ticks required before declaring the counter frozen;
 * a single stall right after a reset isn't necessarily dead. */
#define BNXT_PTP_MAX_CLBR_STALLS	3

struct bnxt_clbr_point {
	uint64_t hw_curr_ts;
	uint64_t hw_prev_ts;
	sbintime_t sbt_cur_ts;
	sbintime_t sbt_prev_ts;
	seqc_t gen_ts;
};

struct bnxt_ptp_cfg {
	uint64_t		current_time;
	uint64_t		old_time;
	struct bnxt_softc	*bp;
	uint8_t			rtc_configured:1;

	uint32_t		refclk_regs[2];
	uint32_t		refclk_mapped_regs[2];

	struct callout		tstmp_clbr;
	int			clbr_ticks;
	int			clbr_curr;
	int			clbr_stall_cnt;
	struct bnxt_clbr_point	clbr_points[2];

	struct mtx		ptp_lock;
};

void bnxt_reset_calibration_callout(struct bnxt_ptp_cfg *priv);
void bnxt_ptp_stop_calibration(struct bnxt_ptp_cfg *priv);
int bnxt_ptp_cfg_tstamp_filters(struct bnxt_softc *bp);
uint64_t bnxt_ptp_hwtstamp_to_ns(struct bnxt_ptp_cfg *priv, uint64_t hw_tstmp);
void bnxt_ptp_free(struct bnxt_softc *bp);

#endif /* BNXT_PTP_H */
