/*-
 * Copyright (c) 2020-2025 The FreeBSD Foundation
 *
 * This software was developed by Björn Zeeb under sponsorship from
 * the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * XXX-BZ:
 * This file is left as a wrapper to make mvm compile and we will only
 * deal with it on a need basis.  Most newer chipsets do this in firmware.
 */
#include <sys/param.h>
#include <net/cfg80211.h>		/* LinuxKPI 802.11 TODO() calls. */

#include "rs.h"
#include "mvm.h"

#ifdef CONFIG_IWLWIFI_DEBUGFS
/*
 * Fill struct iwl_mvm_frame_stats.
 * Deal with various RATE_MCS_*_MSK. See rx.c, fw/api/rs.h, et al.
 * XXX-BZ consider calling iwl_new_rate_from_v1() in rx.c so we can also
 * use this in rxmq.c.
 */
void
iwl_mvm_update_frame_stats(struct iwl_mvm *mvm, u32 rate, bool agg)
{
	uint8_t nss;

	spin_lock_bh(&mvm->drv_stats_lock);
	mvm->drv_rx_stats.success_frames++;

	if (rate & RATE_MCS_HT_MSK_V1) {
		mvm->drv_rx_stats.ht_frames++;
		nss = 1 + ((rate & RATE_HT_MCS_NSS_MSK_V1) >> RATE_HT_MCS_NSS_POS_V1);
	} else if (rate & RATE_MCS_VHT_MSK_V1) {
		mvm->drv_rx_stats.vht_frames++;
		nss = 1 + FIELD_GET(RATE_MCS_NSS_MSK, rate);
	} else {
		mvm->drv_rx_stats.legacy_frames++;
		nss = 0;
	}

	switch (rate & RATE_MCS_CHAN_WIDTH_MSK_V1) {
	case RATE_MCS_CHAN_WIDTH_20:
		mvm->drv_rx_stats.bw_20_frames++;
		break;
	case RATE_MCS_CHAN_WIDTH_40:
		mvm->drv_rx_stats.bw_40_frames++;
		break;
	case RATE_MCS_CHAN_WIDTH_80:
		mvm->drv_rx_stats.bw_80_frames++;
		break;
	case RATE_MCS_CHAN_WIDTH_160:
		mvm->drv_rx_stats.bw_160_frames++;
		break;
	}

	if ((rate & RATE_MCS_CCK_MSK_V1) == 0 &&
	    (rate & RATE_MCS_SGI_MSK_V1) != 0)
		mvm->drv_rx_stats.sgi_frames++;
	else
		mvm->drv_rx_stats.ngi_frames++;

	switch (nss) {
	case 1:
		mvm->drv_rx_stats.siso_frames++;
		break;
	case 2:
		mvm->drv_rx_stats.mimo2_frames++;
		break;
	}

	if (agg)
		mvm->drv_rx_stats.agg_frames++;

	/* ampdu_count? */
	/* fail_frames? */

	mvm->drv_rx_stats.last_rates[mvm->drv_rx_stats.last_frame_idx] = rate;
	mvm->drv_rx_stats.last_frame_idx++;
	mvm->drv_rx_stats.last_frame_idx %=
	    ARRAY_SIZE(mvm->drv_rx_stats.last_rates);

	spin_unlock_bh(&mvm->drv_stats_lock);
}

void
iwl_mvm_reset_frame_stats(struct iwl_mvm *mvm)
{
	/* Apply same locking rx.c does; debugfs seems to read unloked? */
	spin_lock_bh(&mvm->drv_stats_lock);
	memset(&mvm->drv_rx_stats, 0, sizeof(mvm->drv_rx_stats));
	spin_unlock_bh(&mvm->drv_stats_lock);
}
#endif

int
iwl_mvm_rate_control_register(void)
{
	TODO("This likely has to call into net80211 unless we gain compat code in LinuxKPI");
	return (0);
}

void
iwl_mvm_rate_control_unregister(void)
{
	TODO("This likely has to call into net80211 unless we gain compat code in LinuxKPI");
}

int
iwl_mvm_tx_protection(struct iwl_mvm *mvm, struct iwl_mvm_sta *mvmsta, bool enable)
{
	if (iwl_mvm_has_tlc_offload(mvm))
		return (rs_fw_tx_protection(mvm, mvmsta, enable));
        else {
		TODO();
		return (0);
	}
}

static void
iwl_mvm_rs_sw_rate_init(struct iwl_mvm *mvm, struct ieee80211_vif *vif,
    struct ieee80211_sta *sta,
    struct ieee80211_bss_conf *link_conf, struct ieee80211_link_sta *link_sta,
    enum nl80211_band band)
{
	TODO();
}

void
iwl_mvm_rs_rate_init(struct iwl_mvm *mvm, struct ieee80211_vif *vif,
    struct ieee80211_sta *sta,
    struct ieee80211_bss_conf *link_conf, struct ieee80211_link_sta *link_sta,
    enum nl80211_band band)
{
	if (iwl_mvm_has_tlc_offload(mvm))
		iwl_mvm_rs_fw_rate_init(mvm, vif, sta, link_conf, link_sta, band);
	else
		iwl_mvm_rs_sw_rate_init(mvm, vif, sta, link_conf, link_sta, band);
}

void
iwl_mvm_rs_tx_status(struct iwl_mvm *mvm, struct ieee80211_sta *sta, int tid,
    struct ieee80211_tx_info *ba_info, bool t)
{
	TODO();
}

void
rs_update_last_rssi(struct iwl_mvm *mvm __unused, struct iwl_mvm_sta *mvmsta,
    struct ieee80211_rx_status *rx_status)
{
	struct iwl_lq_sta *lq_sta;
	int i;

	if (mvmsta == NULL || rx_status == NULL)
		return;

	/*
	 * Assumption based on mvm/sta.h is that this should update
	 * mvmsta->lq_sta.rs_drv but so far we only saw a iwl_lq_cmd (lq)
	 * access in that struct so nowhere to put rssi information.
	 * So the only thing would be if this is required internally
	 * to functions in this file.
	 * The "FW" version accesses more fields.  We assume they
	 * are the same for now.
	 */

	lq_sta = &mvmsta->deflink.lq_sta.rs_drv;

	lq_sta->pers.last_rssi = S8_MIN;
	lq_sta->pers.chains = rx_status->chains;

	for (i = 0; i < nitems(lq_sta->pers.chain_signal); i++) {
		if ((rx_status->chains & BIT(i)) == 0)
			continue;

		lq_sta->pers.chain_signal[i] = rx_status->chain_signal[i];
		if (rx_status->chain_signal[i] > lq_sta->pers.last_rssi)
			lq_sta->pers.last_rssi = rx_status->chain_signal[i];
	}
}

int
rs_pretty_print_rate_v1(char *buf, int bufsz, const u32 rate)
{
	TODO();
	return (0);
}
