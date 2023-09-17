/*-
 * Copyright (c) 2020-2021 The FreeBSD Foundation
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

#include <sys/errno.h>

#include "mvm.h"

#ifdef CONFIG_IWLWIFI_DEBUGFS
void
iwl_mvm_update_frame_stats(struct iwl_mvm *mvm, u32 rate, bool agg)
{

}

void
iwl_mvm_reset_frame_stats(struct iwl_mvm *mvm)
{

}
#endif

int
iwl_mvm_rate_control_register(void)
{

	return (0);
}

int
iwl_mvm_tx_protection(struct iwl_mvm *mvm, struct iwl_mvm_sta *sta, bool enable)
{

	return (0);
}

void
iwl_mvm_rate_control_unregister(void)
{
}

void
iwl_mvm_rs_rate_init(struct iwl_mvm *mvm, struct ieee80211_sta *sta,
    enum nl80211_band band, bool t)
{
}

void
iwl_mvm_rs_tx_status(struct iwl_mvm *mvm, struct ieee80211_sta *sta, int tid,
    struct ieee80211_tx_info *ba_info, bool t)
{
}

void
rs_update_last_rssi(struct iwl_mvm *mvm, struct iwl_mvm_sta *mvmsta,
    struct ieee80211_rx_status *rx_status)
{

	/*
	 * Assumption based on mvm/sta.h is that this should update
	 * mvmsta->lq_sta.rs_drv but so far we only saw a iwl_lq_cmd (lq)
	 * access in that struct so nowhere to put rssi information.
	 * So the only thing would be if this is required internally
	 * to functions in this file.
	 */
}

int
rs_pretty_print_rate_v1(char *buf, int bufsz, const u32 rate)
{

	return (0);
}
