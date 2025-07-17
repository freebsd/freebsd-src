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

#ifndef	_IWLWIFI_MVM_RS_H
#define	_IWLWIFI_MVM_RS_H

#include <net/mac80211.h>

#include "iwl-trans.h"
#include "fw-api.h"

#define	RS_NAME		"XXX_unknown"

#define	LINK_QUAL_AGG_FRAME_LIMIT_DEF		(64-1)
#define	LINK_QUAL_AGG_FRAME_LIMIT_GEN2_DEF	(256-1)

struct iwl_mvm;

struct iwl_lq_sta_rs_fw {
	int	last_rate_n_flags;
	struct {
		struct iwl_mvm	*drv;
		uint8_t		sta_id;
		uint8_t		chains;
		uint8_t		chain_signal[IEEE80211_MAX_CHAINS];
		uint8_t		last_rssi;
#ifdef CONFIG_MAC80211_DEBUGFS
		uint32_t	dbg_fixed_rate;
		uint32_t	dbg_agg_frame_count_lim;
#endif
	} pers;
};

struct iwl_lq_sta {
	struct iwl_lq_cmd	lq;
	struct {
		spinlock_t	lock;
		uint16_t	max_agg_bufsize;
		/*
		 * Based on the assumption that these are in "FW" too and
		 * there is a f() to set last_rssi add them here too.
		 */
		uint8_t		chains;
		uint8_t		chain_signal[IEEE80211_MAX_CHAINS];
		uint8_t		last_rssi;
	} pers;
};

#define	RS_DRV_DATA_PACK(_c, _f)	((void *)(uintptr_t)(_c | (uintptr_t)(_f) << sizeof(_c)))	/* XXX TODO | ? */

struct iwl_mvm_sta;
struct iwl_mvm_link_sta;

#ifdef CONFIG_IWLWIFI_DEBUGFS
void iwl_mvm_reset_frame_stats(struct iwl_mvm *);
#endif

void iwl_mvm_rs_add_sta(struct iwl_mvm *, struct iwl_mvm_sta *);
void iwl_mvm_tlc_update_notif(struct iwl_mvm *, struct iwl_rx_cmd_buffer *);
u16 rs_fw_get_max_amsdu_len(struct ieee80211_sta *,
    struct ieee80211_bss_conf *, struct ieee80211_link_sta *);
void rs_fw_rate_init(struct iwl_mvm *, struct ieee80211_sta *,
    enum nl80211_band, bool);
int rs_fw_tx_protection(struct iwl_mvm *, struct iwl_mvm_sta *, bool);
int iwl_mvm_tx_protection(struct iwl_mvm *, struct iwl_mvm_sta *, bool);

int iwl_mvm_rate_control_register(void);
void iwl_mvm_rate_control_unregister(void);
void iwl_mvm_rs_rate_init(struct iwl_mvm *, struct ieee80211_vif *,
    struct ieee80211_sta *, struct ieee80211_bss_conf *,
    struct ieee80211_link_sta *, enum nl80211_band);
void iwl_mvm_rs_fw_rate_init(struct iwl_mvm *, struct ieee80211_vif *,
    struct ieee80211_sta *, struct ieee80211_bss_conf *,
    struct ieee80211_link_sta *, enum nl80211_band);
void iwl_mvm_rs_tx_status(struct iwl_mvm *, struct ieee80211_sta *,
    int, struct ieee80211_tx_info *, bool);
void iwl_mvm_rs_add_sta_link(struct iwl_mvm *, struct iwl_mvm_link_sta *);

#endif	/* _IWLWIFI_MVM_RS_H */
