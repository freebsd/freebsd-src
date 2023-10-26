/*-
 * Copyright (c) 2021-2022 The FreeBSD Foundation
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
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/errno.h>

#define	LINUXKPI_NET80211
#include <net/mac80211.h>

#include "linux_80211.h"

/* Could be a different tracing framework later. */
#ifdef LINUXKPI_DEBUG_80211
#define	LKPI_80211_TRACE_MO(fmt, ...)					\
    if (linuxkpi_debug_80211 & D80211_TRACE_MO)				\
	printf("LKPI_80211_TRACE_MO %s:%d: %d %d %u_" fmt "\n",		\
	    __func__, __LINE__, curcpu, curthread->td_tid,		\
	    (unsigned int)ticks, __VA_ARGS__)
#else
#define	LKPI_80211_TRACE_MO(...)	do { } while(0)
#endif

int
lkpi_80211_mo_start(struct ieee80211_hw *hw)
{
	struct lkpi_hw *lhw;
	int error;

	lhw = HW_TO_LHW(hw);
	if (lhw->ops->start == NULL) {
		error = EOPNOTSUPP;
		goto out;
	}

	if ((lhw->sc_flags & LKPI_MAC80211_DRV_STARTED)) {
		/* Trying to start twice is an error. */
		error = EEXIST;
		goto out;
	}
	LKPI_80211_TRACE_MO("hw %p", hw);
	error = lhw->ops->start(hw);
	if (error == 0)
		lhw->sc_flags |= LKPI_MAC80211_DRV_STARTED;

out:
	return (error);
}

void
lkpi_80211_mo_stop(struct ieee80211_hw *hw)
{
	struct lkpi_hw *lhw;

	lhw = HW_TO_LHW(hw);
	if (lhw->ops->stop == NULL)
		return;

	LKPI_80211_TRACE_MO("hw %p", hw);
	lhw->ops->stop(hw);
	lhw->sc_flags &= ~LKPI_MAC80211_DRV_STARTED;
}

int
lkpi_80211_mo_get_antenna(struct ieee80211_hw *hw, u32 *txs, u32 *rxs)
{
	struct lkpi_hw *lhw;
	int error;

	lhw = HW_TO_LHW(hw);
	if (lhw->ops->get_antenna == NULL) {
		error = EOPNOTSUPP;
		goto out;
	}

	LKPI_80211_TRACE_MO("hw %p", hw);
	error = lhw->ops->get_antenna(hw, txs, rxs);

out:
	return (error);
}

int
lkpi_80211_mo_set_frag_threshold(struct ieee80211_hw *hw, uint32_t frag_th)
{
	struct lkpi_hw *lhw;
	int error;

	lhw = HW_TO_LHW(hw);
	if (lhw->ops->set_frag_threshold == NULL) {
		error = EOPNOTSUPP;
		goto out;
	}

	LKPI_80211_TRACE_MO("hw %p frag_th %u", hw, frag_th);
	error = lhw->ops->set_frag_threshold(hw, frag_th);

out:
	return (error);
}

int
lkpi_80211_mo_set_rts_threshold(struct ieee80211_hw *hw, uint32_t rts_th)
{
	struct lkpi_hw *lhw;
	int error;

	lhw = HW_TO_LHW(hw);
	if (lhw->ops->set_rts_threshold == NULL) {
		error = EOPNOTSUPP;
		goto out;
	}

	LKPI_80211_TRACE_MO("hw %p rts_th %u", hw, rts_th);
	error = lhw->ops->set_rts_threshold(hw, rts_th);

out:
	return (error);
}


int
lkpi_80211_mo_add_interface(struct ieee80211_hw *hw, struct ieee80211_vif *vif)
{
	struct lkpi_hw *lhw;
	struct lkpi_vif *lvif;
	int error;

	lhw = HW_TO_LHW(hw);
	if (lhw->ops->add_interface == NULL) {
		error = EOPNOTSUPP;
		goto out;
	}

	lvif = VIF_TO_LVIF(vif);
	LKPI_80211_LVIF_LOCK(lvif);
	if (lvif->added_to_drv) {
		LKPI_80211_LVIF_UNLOCK(lvif);
		/* Trying to add twice is an error. */
		error = EEXIST;
		goto out;
	}
	LKPI_80211_LVIF_UNLOCK(lvif);

	LKPI_80211_TRACE_MO("hw %p vif %p", hw, vif);
	error = lhw->ops->add_interface(hw, vif);
	if (error == 0) {
		LKPI_80211_LVIF_LOCK(lvif);
		lvif->added_to_drv = true;
		LKPI_80211_LVIF_UNLOCK(lvif);
	}

out:
	return (error);
}

void
lkpi_80211_mo_remove_interface(struct ieee80211_hw *hw, struct ieee80211_vif *vif)
{
	struct lkpi_hw *lhw;
	struct lkpi_vif *lvif;

	lhw = HW_TO_LHW(hw);
	if (lhw->ops->remove_interface == NULL)
		return;

	lvif = VIF_TO_LVIF(vif);
	LKPI_80211_LVIF_LOCK(lvif);
	if (!lvif->added_to_drv) {
		LKPI_80211_LVIF_UNLOCK(lvif);
		return;
	}
	LKPI_80211_LVIF_UNLOCK(lvif);

	LKPI_80211_TRACE_MO("hw %p vif %p", hw, vif);
	lhw->ops->remove_interface(hw, vif);
	LKPI_80211_LVIF_LOCK(lvif);
	lvif->added_to_drv = false;
	LKPI_80211_LVIF_UNLOCK(lvif);
}


int
lkpi_80211_mo_hw_scan(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
    struct ieee80211_scan_request *sr)
{
	struct lkpi_hw *lhw;
	int error;

	/*
	 * MUST NOT return EPERM as that is a "magic number 1" based on rtw88
	 * driver indicating hw_scan is not supported despite the ops call
	 * being available.
	 */

	lhw = HW_TO_LHW(hw);
	if (lhw->ops->hw_scan == NULL) {
		/* Return magic number to use sw scan. */
		error = 1;
		goto out;
	}

	LKPI_80211_TRACE_MO("CALLING hw %p vif %p sr %p", hw, vif, sr);
	error = lhw->ops->hw_scan(hw, vif, sr);
	LKPI_80211_TRACE_MO("RETURNING hw %p vif %p sr %p error %d", hw, vif, sr, error);

out:
	return (error);
}

void
lkpi_80211_mo_cancel_hw_scan(struct ieee80211_hw *hw, struct ieee80211_vif *vif)
{
	struct lkpi_hw *lhw;

	lhw = HW_TO_LHW(hw);
	if (lhw->ops->cancel_hw_scan == NULL)
		return;

	LKPI_80211_TRACE_MO("hw %p vif %p", hw, vif);
	lhw->ops->cancel_hw_scan(hw, vif);
}

void
lkpi_80211_mo_sw_scan_complete(struct ieee80211_hw *hw, struct ieee80211_vif *vif)
{
	struct lkpi_hw *lhw;

	lhw = HW_TO_LHW(hw);
	if (lhw->ops->sw_scan_complete == NULL)
		return;

	LKPI_80211_TRACE_MO("hw %p vif %p", hw, vif);
	lhw->ops->sw_scan_complete(hw, vif);
	lhw->scan_flags &= ~LKPI_LHW_SCAN_RUNNING;
}

void
lkpi_80211_mo_sw_scan_start(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
    const u8 *addr)
{
	struct lkpi_hw *lhw;

	lhw = HW_TO_LHW(hw);
	if (lhw->ops->sw_scan_start == NULL)
		return;

	LKPI_80211_TRACE_MO("hw %p vif %p", hw, vif);
	lhw->ops->sw_scan_start(hw, vif, addr);
}


/*
 * We keep the Linux type here;  it really is an uintptr_t.
 */
u64
lkpi_80211_mo_prepare_multicast(struct ieee80211_hw *hw,
    struct netdev_hw_addr_list *mc_list)
{
	struct lkpi_hw *lhw;
	u64 ptr;

	lhw = HW_TO_LHW(hw);
	if (lhw->ops->prepare_multicast == NULL)
		return (0);

	LKPI_80211_TRACE_MO("hw %p mc_list %p", hw, mc_list);
	ptr = lhw->ops->prepare_multicast(hw, mc_list);
	return (ptr);
}

void
lkpi_80211_mo_configure_filter(struct ieee80211_hw *hw, unsigned int changed_flags,
    unsigned int *total_flags, u64 mc_ptr)
{
	struct lkpi_hw *lhw;

	lhw = HW_TO_LHW(hw);
	if (lhw->ops->configure_filter == NULL)
		return;

	if (mc_ptr == 0)
		return;

	LKPI_80211_TRACE_MO("hw %p changed_flags %#x total_flags %p mc_ptr %ju", hw, changed_flags, total_flags, (uintmax_t)mc_ptr);
	lhw->ops->configure_filter(hw, changed_flags, total_flags, mc_ptr);
}


/*
 * So far we only called sta_{add,remove} as an alternative to sta_state.
 * Let's keep the implementation simpler and hide sta_{add,remove} under the
 * hood here calling them if state_state is not available from mo_sta_state.
 */
static int
lkpi_80211_mo_sta_add(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
    struct ieee80211_sta *sta)
{
	struct lkpi_hw *lhw;
	struct lkpi_sta *lsta;
	int error;

	lhw = HW_TO_LHW(hw);
	if (lhw->ops->sta_add == NULL) {
		error = EOPNOTSUPP;
		goto out;
	}

	lsta = STA_TO_LSTA(sta);
	if (lsta->added_to_drv) {
		error = EEXIST;
		goto out;
	}

	LKPI_80211_TRACE_MO("hw %p vif %p sta %p", hw, vif, sta);
	error = lhw->ops->sta_add(hw, vif, sta);
	if (error == 0)
		lsta->added_to_drv = true;

out:
	return error;
}

static int
lkpi_80211_mo_sta_remove(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
    struct ieee80211_sta *sta)
{
	struct lkpi_hw *lhw;
	struct lkpi_sta *lsta;
	int error;

	lhw = HW_TO_LHW(hw);
	if (lhw->ops->sta_remove == NULL) {
		error = EOPNOTSUPP;
		goto out;
	}

	lsta = STA_TO_LSTA(sta);
	if (!lsta->added_to_drv) {
		/* If we never added the sta, do not complain on cleanup. */
		error = 0;
		goto out;
	}

	LKPI_80211_TRACE_MO("hw %p vif %p sta %p", hw, vif, sta);
	error = lhw->ops->sta_remove(hw, vif, sta);
	if (error == 0)
		lsta->added_to_drv = false;

out:
	return error;
}

int
lkpi_80211_mo_sta_state(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
    struct lkpi_sta *lsta, enum ieee80211_sta_state nstate)
{
	struct lkpi_hw *lhw;
	struct ieee80211_sta *sta;
	int error;

	lhw = HW_TO_LHW(hw);
	sta = LSTA_TO_STA(lsta);
	if (lhw->ops->sta_state != NULL) {
		LKPI_80211_TRACE_MO("hw %p vif %p sta %p nstate %d", hw, vif, sta, nstate);
		error = lhw->ops->sta_state(hw, vif, sta, lsta->state, nstate);
		if (error == 0) {
			if (nstate == IEEE80211_STA_NOTEXIST)
				lsta->added_to_drv = false;
			else
				lsta->added_to_drv = true;
			lsta->state = nstate;
		}
		goto out;
	}

	/* XXX-BZ is the change state AUTH or ASSOC here? */
	if (lsta->state < IEEE80211_STA_ASSOC && nstate == IEEE80211_STA_ASSOC) {
		error = lkpi_80211_mo_sta_add(hw, vif, sta);
		if (error == 0)
			lsta->added_to_drv = true;
	} else if (lsta->state >= IEEE80211_STA_ASSOC &&
	    nstate < IEEE80211_STA_ASSOC) {
		error = lkpi_80211_mo_sta_remove(hw, vif, sta);
		if (error == 0)
			lsta->added_to_drv = false;
	} else
		/* Nothing to do. */
		error = 0;
	if (error == 0)
		lsta->state = nstate;

out:
	/* XXX-BZ should we manage state in here? */
	return (error);
}

int
lkpi_80211_mo_config(struct ieee80211_hw *hw, uint32_t changed)
{
	struct lkpi_hw *lhw;
	int error;

	lhw = HW_TO_LHW(hw);
	if (lhw->ops->config == NULL) {
		error = EOPNOTSUPP;
		goto out;
	}

	LKPI_80211_TRACE_MO("hw %p changed %u", hw, changed);
	error = lhw->ops->config(hw, changed);

out:
	return (error);
}


int
lkpi_80211_mo_assign_vif_chanctx(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
    struct ieee80211_bss_conf *conf, struct ieee80211_chanctx_conf *chanctx_conf)
{
	struct lkpi_hw *lhw;
	int error;

	lhw = HW_TO_LHW(hw);
	if (lhw->ops->assign_vif_chanctx == NULL) {
		error = EOPNOTSUPP;
		goto out;
	}

	LKPI_80211_TRACE_MO("hw %p vif %p bss_conf %p chanctx_conf %p",
	    hw, vif, conf, chanctx_conf);
	error = lhw->ops->assign_vif_chanctx(hw, vif, conf, chanctx_conf);
	if (error == 0)
		vif->chanctx_conf = chanctx_conf;

out:
	return (error);
}

void
lkpi_80211_mo_unassign_vif_chanctx(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
    struct ieee80211_bss_conf *conf, struct ieee80211_chanctx_conf **chanctx_conf)
{
	struct lkpi_hw *lhw;

	lhw = HW_TO_LHW(hw);
	if (lhw->ops->unassign_vif_chanctx == NULL)
		return;

	if (*chanctx_conf == NULL)
		return;

	LKPI_80211_TRACE_MO("hw %p vif %p bss_conf %p chanctx_conf %p",
	    hw, vif, conf, *chanctx_conf);
	lhw->ops->unassign_vif_chanctx(hw, vif, conf, *chanctx_conf);
	*chanctx_conf = NULL;
}


int
lkpi_80211_mo_add_chanctx(struct ieee80211_hw *hw,
    struct ieee80211_chanctx_conf *chanctx_conf)
{
	struct lkpi_hw *lhw;
	struct lkpi_chanctx *lchanctx;
	int error;

	lhw = HW_TO_LHW(hw);
	if (lhw->ops->add_chanctx == NULL) {
		error = EOPNOTSUPP;
		goto out;
	}

	LKPI_80211_TRACE_MO("hw %p chanctx_conf %p", hw, chanctx_conf);
	error = lhw->ops->add_chanctx(hw, chanctx_conf);
	if (error == 0) {
		lchanctx = CHANCTX_CONF_TO_LCHANCTX(chanctx_conf);
		lchanctx->added_to_drv = true;
	}

out:
	return (error);
}

void
lkpi_80211_mo_change_chanctx(struct ieee80211_hw *hw,
    struct ieee80211_chanctx_conf *chanctx_conf, uint32_t changed)
{
	struct lkpi_hw *lhw;

	lhw = HW_TO_LHW(hw);
	if (lhw->ops->change_chanctx == NULL)
		return;

	LKPI_80211_TRACE_MO("hw %p chanctx_conf %p changed %u", hw, chanctx_conf, changed);
	lhw->ops->change_chanctx(hw, chanctx_conf, changed);
}

void
lkpi_80211_mo_remove_chanctx(struct ieee80211_hw *hw,
    struct ieee80211_chanctx_conf *chanctx_conf)
{
	struct lkpi_hw *lhw;
	struct lkpi_chanctx *lchanctx;

	lhw = HW_TO_LHW(hw);
	if (lhw->ops->remove_chanctx == NULL)
		return;

	LKPI_80211_TRACE_MO("hw %p chanctx_conf %p", hw, chanctx_conf);
	lhw->ops->remove_chanctx(hw, chanctx_conf);
	lchanctx = CHANCTX_CONF_TO_LCHANCTX(chanctx_conf);
	lchanctx->added_to_drv = false;
}

void
lkpi_80211_mo_bss_info_changed(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
    struct ieee80211_bss_conf *conf, uint64_t changed)
{
	struct lkpi_hw *lhw;

	lhw = HW_TO_LHW(hw);
	if (lhw->ops->link_info_changed == NULL &&
	    lhw->ops->bss_info_changed == NULL)
		return;

	LKPI_80211_TRACE_MO("hw %p vif %p conf %p changed %#jx", hw, vif, conf, (uintmax_t)changed);
	if (lhw->ops->link_info_changed != NULL)
		lhw->ops->link_info_changed(hw, vif, conf, changed);
	else
		lhw->ops->bss_info_changed(hw, vif, conf, changed);
}

int
lkpi_80211_mo_conf_tx(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
    uint32_t link_id, uint16_t ac, const struct ieee80211_tx_queue_params *txqp)
{
	struct lkpi_hw *lhw;
	int error;

	lhw = HW_TO_LHW(hw);
	if (lhw->ops->conf_tx == NULL) {
		error = EOPNOTSUPP;
		goto out;
	}

	LKPI_80211_TRACE_MO("hw %p vif %p link_id %u ac %u txpq %p",
	    hw, vif, link_id, ac, txqp);
	error = lhw->ops->conf_tx(hw, vif, link_id, ac, txqp);

out:
	return (error);
}

void
lkpi_80211_mo_flush(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
    uint32_t nqueues, bool drop)
{
	struct lkpi_hw *lhw;

	lhw = HW_TO_LHW(hw);
	if (lhw->ops->flush == NULL)
		return;

	LKPI_80211_TRACE_MO("hw %p vif %p nqueues %u drop %d", hw, vif, nqueues, drop);
	lhw->ops->flush(hw, vif, nqueues, drop);
}

void
lkpi_80211_mo_mgd_prepare_tx(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
    struct ieee80211_prep_tx_info *txinfo)
{
	struct lkpi_hw *lhw;

	lhw = HW_TO_LHW(hw);
	if (lhw->ops->mgd_prepare_tx == NULL)
		return;

	LKPI_80211_TRACE_MO("hw %p vif %p txinfo %p", hw, vif, txinfo);
	lhw->ops->mgd_prepare_tx(hw, vif, txinfo);
}

void
lkpi_80211_mo_mgd_complete_tx(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
    struct ieee80211_prep_tx_info *txinfo)
{
	struct lkpi_hw *lhw;

	lhw = HW_TO_LHW(hw);
	if (lhw->ops->mgd_complete_tx == NULL)
		return;

	LKPI_80211_TRACE_MO("hw %p vif %p txinfo %p", hw, vif, txinfo);
	lhw->ops->mgd_complete_tx(hw, vif, txinfo);
}

void
lkpi_80211_mo_tx(struct ieee80211_hw *hw, struct ieee80211_tx_control *txctrl,
    struct sk_buff *skb)
{
	struct lkpi_hw *lhw;

	lhw = HW_TO_LHW(hw);
	if (lhw->ops->tx == NULL)
		return;

	LKPI_80211_TRACE_MO("hw %p txctrl %p skb %p", hw, txctrl, skb);
	lhw->ops->tx(hw, txctrl, skb);
}

void
lkpi_80211_mo_wake_tx_queue(struct ieee80211_hw *hw, struct ieee80211_txq *txq)
{
	struct lkpi_hw *lhw;

	lhw = HW_TO_LHW(hw);
	if (lhw->ops->wake_tx_queue == NULL)
		return;

	LKPI_80211_TRACE_MO("hw %p txq %p", hw, txq);
	lhw->ops->wake_tx_queue(hw, txq);
}

void
lkpi_80211_mo_sync_rx_queues(struct ieee80211_hw *hw)
{
	struct lkpi_hw *lhw;

	lhw = HW_TO_LHW(hw);
	if (lhw->ops->sync_rx_queues == NULL)
		return;

	LKPI_80211_TRACE_MO("hw %p", hw);
	lhw->ops->sync_rx_queues(hw);
}

void
lkpi_80211_mo_sta_pre_rcu_remove(struct ieee80211_hw *hw,
    struct ieee80211_vif *vif, struct ieee80211_sta *sta)
{
	struct lkpi_hw *lhw;

	lhw = HW_TO_LHW(hw);
	if (lhw->ops->sta_pre_rcu_remove == NULL)
		return;

	LKPI_80211_TRACE_MO("hw %p vif %p sta %p", hw, vif, sta);
	lhw->ops->sta_pre_rcu_remove(hw, vif, sta);
}

int
lkpi_80211_mo_set_key(struct ieee80211_hw *hw, enum set_key_cmd cmd,
    struct ieee80211_vif *vif, struct ieee80211_sta *sta,
    struct ieee80211_key_conf *kc)
{
	struct lkpi_hw *lhw;
	int error;

	lhw = HW_TO_LHW(hw);
	if (lhw->ops->set_key == NULL) {
		error = EOPNOTSUPP;
		goto out;
	}

	LKPI_80211_TRACE_MO("hw %p cmd %d vif %p sta %p kc %p", hw, cmd, vif, sta, kc);
	error = lhw->ops->set_key(hw, cmd, vif, sta, kc);

out:
	return (error);
}

int
lkpi_80211_mo_ampdu_action(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
    struct ieee80211_ampdu_params *params)
{
	struct lkpi_hw *lhw;
	int error;

	lhw = HW_TO_LHW(hw);
	if (lhw->ops->ampdu_action == NULL) {
		error = EOPNOTSUPP;
		goto out;
	}

	LKPI_80211_TRACE_MO("hw %p vif %p params %p { %p, %d, %u, %u, %u, %u, %d }",
	    hw, vif, params, params->sta, params->action, params->buf_size,
	    params->timeout, params->ssn, params->tid, params->amsdu);
	error = lhw->ops->ampdu_action(hw, vif, params);

out:
	return (error);
}
