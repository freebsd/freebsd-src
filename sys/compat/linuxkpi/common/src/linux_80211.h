/*-
 * Copyright (c) 2020-2023 The FreeBSD Foundation
 * Copyright (c) 2020-2021 Bjoern A. Zeeb
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

/*
 * Public functions are called linuxkpi_*().
 * Internal (static) functions are called lkpi_*().
 *
 * The internal structures holding metadata over public structures are also
 * called lkpi_xxx (usually with a member at the end called xxx).
 * Note: we do not replicate the structure names but the general variable names
 * for these (e.g., struct hw -> struct lkpi_hw, struct sta -> struct lkpi_sta).
 * There are macros to access one from the other.
 * We call the internal versions lxxx (e.g., hw -> lhw, sta -> lsta).
 */

#ifndef _LKPI_SRC_LINUX_80211_H
#define _LKPI_SRC_LINUX_80211_H

/* #define	LINUXKPI_DEBUG_80211 */

#ifndef	D80211_TODO
#define	D80211_TODO		0x00000001
#endif
#ifndef D80211_IMPROVE
#define	D80211_IMPROVE		0x00000002
#endif
#define	D80211_IMPROVE_TXQ	0x00000004
#define	D80211_TRACE		0x00000010
#define	D80211_TRACEOK		0x00000020
#define	D80211_TRACE_TX		0x00000100
#define	D80211_TRACE_TX_DUMP	0x00000200
#define	D80211_TRACE_RX		0x00001000
#define	D80211_TRACE_RX_DUMP	0x00002000
#define	D80211_TRACE_RX_BEACONS	0x00004000
#define	D80211_TRACEX		(D80211_TRACE_TX|D80211_TRACE_RX)
#define	D80211_TRACEX_DUMP	(D80211_TRACE_TX_DUMP|D80211_TRACE_RX_DUMP)
#define	D80211_TRACE_STA	0x00010000
#define	D80211_TRACE_MO		0x00100000
#define	D80211_TRACE_MODE	0x0f000000
#define	D80211_TRACE_MODE_HT	0x01000000
#define	D80211_TRACE_MODE_VHT	0x02000000
#define	D80211_TRACE_MODE_HE	0x04000000
#define	D80211_TRACE_MODE_EHT	0x08000000

#define	IMPROVE_TXQ(...)						\
    if (linuxkpi_debug_80211 & D80211_IMPROVE_TXQ)			\
	printf("%s:%d: XXX LKPI80211 IMPROVE_TXQ\n", __func__, __LINE__)

#define	IMPROVE_HT(...)							\
    if (linuxkpi_debug_80211 & D80211_TRACE_MODE_HT)			\
	printf("%s:%d: XXX LKPI80211 IMPROVE_HT\n", __func__, __LINE__)

struct lkpi_radiotap_tx_hdr {
	struct ieee80211_radiotap_header wt_ihdr;
	uint8_t		wt_flags;
	uint8_t		wt_rate;
	uint16_t	wt_chan_freq;
	uint16_t	wt_chan_flags;
} __packed;
#define	LKPI_RTAP_TX_FLAGS_PRESENT					\
	((1 << IEEE80211_RADIOTAP_FLAGS) |				\
	 (1 << IEEE80211_RADIOTAP_RATE) |				\
	 (1 << IEEE80211_RADIOTAP_CHANNEL))

struct lkpi_radiotap_rx_hdr {
	struct ieee80211_radiotap_header wr_ihdr;
	uint64_t	wr_tsft;
	uint8_t		wr_flags;
	uint8_t		wr_rate;
	uint16_t	wr_chan_freq;
	uint16_t	wr_chan_flags;
	int8_t		wr_dbm_antsignal;
	int8_t		wr_dbm_antnoise;
} __packed __aligned(8);
#define	LKPI_RTAP_RX_FLAGS_PRESENT					\
	((1 << IEEE80211_RADIOTAP_TSFT) |				\
	 (1 << IEEE80211_RADIOTAP_FLAGS) |				\
	 (1 << IEEE80211_RADIOTAP_RATE) |				\
	 (1 << IEEE80211_RADIOTAP_CHANNEL) |				\
	 (1 << IEEE80211_RADIOTAP_DBM_ANTSIGNAL) |			\
	 (1 << IEEE80211_RADIOTAP_DBM_ANTNOISE))

struct lkpi_txq {
	TAILQ_ENTRY(lkpi_txq)	txq_entry;

	struct mtx		ltxq_mtx;
	bool			seen_dequeue;
	bool			stopped;
	uint32_t		txq_generation;
	struct sk_buff_head	skbq;

	/* Must be last! */
	struct ieee80211_txq	txq __aligned(CACHE_LINE_SIZE);
};
#define	TXQ_TO_LTXQ(_txq)	container_of(_txq, struct lkpi_txq, txq)


struct lkpi_sta {
        TAILQ_ENTRY(lkpi_sta)	lsta_entry;
	struct ieee80211_node	*ni;

	/* Deferred TX path. */
	/* Eventually we might want to migrate this into net80211 entirely. */
	/* XXX-BZ can we use sta->txq[] instead directly? */
	struct task		txq_task;
	struct mbufq		txq;
	struct mtx		txq_mtx;

	struct ieee80211_key_conf *kc;
	enum ieee80211_sta_state state;
	bool			added_to_drv;			/* Driver knows; i.e. we called ...(). */
	bool			in_mgd;				/* XXX-BZ should this be per-vif? */

	/* Must be last! */
	struct ieee80211_sta	sta __aligned(CACHE_LINE_SIZE);
};
#define	STA_TO_LSTA(_sta)	container_of(_sta, struct lkpi_sta, sta)
#define	LSTA_TO_STA(_lsta)	(&(_lsta)->sta)

struct lkpi_vif {
        TAILQ_ENTRY(lkpi_vif)	lvif_entry;
	struct ieee80211vap	iv_vap;

	struct mtx		mtx;
	struct wireless_dev	wdev;

	/* Other local stuff. */
	int			(*iv_newstate)(struct ieee80211vap *,
				    enum ieee80211_state, int);
	struct ieee80211_node *	(*iv_update_bss)(struct ieee80211vap *,
				    struct ieee80211_node *);
	TAILQ_HEAD(, lkpi_sta)	lsta_head;
	struct lkpi_sta		*lvif_bss;
	bool			lvif_bss_synched;
	bool			added_to_drv;			/* Driver knows; i.e. we called add_interface(). */

	bool			hw_queue_stopped[IEEE80211_NUM_ACS];

	/* Must be last! */
	struct ieee80211_vif	vif __aligned(CACHE_LINE_SIZE);
};
#define	VAP_TO_LVIF(_vap)	container_of(_vap, struct lkpi_vif, iv_vap)
#define	LVIF_TO_VAP(_lvif)	(&(_lvif)->iv_vap)
#define	VIF_TO_LVIF(_vif)	container_of(_vif, struct lkpi_vif, vif)
#define	LVIF_TO_VIF(_lvif)	(&(_lvif)->vif)


struct lkpi_hw {	/* name it mac80211_sc? */
	const struct ieee80211_ops	*ops;
	struct ieee80211_scan_request	*hw_req;
	struct workqueue_struct		*workq;

	/* FreeBSD specific compat. */
	/* Linux device is in hw.wiphy->dev after SET_IEEE80211_DEV(). */
	struct ieee80211com		*ic;
	struct lkpi_radiotap_tx_hdr	rtap_tx;
	struct lkpi_radiotap_rx_hdr	rtap_rx;

	TAILQ_HEAD(, lkpi_vif)		lvif_head;
	struct sx			lvif_sx;

	struct sx			sx;

	struct mtx			txq_mtx;
	uint32_t			txq_generation[IEEE80211_NUM_ACS];
	TAILQ_HEAD(, lkpi_txq)		scheduled_txqs[IEEE80211_NUM_ACS];

	/* Scan functions we overload to handle depending on scan mode. */
	void                    (*ic_scan_curchan)(struct ieee80211_scan_state *,
				    unsigned long);
	void                    (*ic_scan_mindwell)(struct ieee80211_scan_state *);

	/* Node functions we overload to sync state. */
	struct ieee80211_node *	(*ic_node_alloc)(struct ieee80211vap *,
				    const uint8_t [IEEE80211_ADDR_LEN]);
	int			(*ic_node_init)(struct ieee80211_node *);
	void			(*ic_node_cleanup)(struct ieee80211_node *);
	void			(*ic_node_free)(struct ieee80211_node *);

	/* HT and later functions. */
	int			(*ic_recv_action)(struct ieee80211_node *,
				    const struct ieee80211_frame *,
				    const uint8_t *, const uint8_t *);
	int			(*ic_send_action)(struct ieee80211_node *,
				    int, int, void *);
	int			(*ic_ampdu_enable)(struct ieee80211_node *,
				    struct ieee80211_tx_ampdu *);
	int			(*ic_addba_request)(struct ieee80211_node *,
				    struct ieee80211_tx_ampdu *, int, int, int);
	int			(*ic_addba_response)(struct ieee80211_node *,
				    struct ieee80211_tx_ampdu *, int, int, int);
	void			(*ic_addba_stop)(struct ieee80211_node *,
				    struct ieee80211_tx_ampdu *);
	void			(*ic_addba_response_timeout)(struct ieee80211_node *,
				    struct ieee80211_tx_ampdu *);
	void			(*ic_bar_response)(struct ieee80211_node *,
				    struct ieee80211_tx_ampdu *, int);
	int			(*ic_ampdu_rx_start)(struct ieee80211_node *,
				    struct ieee80211_rx_ampdu *, int, int, int);
	void			(*ic_ampdu_rx_stop)(struct ieee80211_node *,
				    struct ieee80211_rx_ampdu *);

#define	LKPI_MAC80211_DRV_STARTED	0x00000001
	uint32_t			sc_flags;
#define	LKPI_LHW_SCAN_RUNNING		0x00000001
#define	LKPI_LHW_SCAN_HW		0x00000002
	uint32_t			scan_flags;
	struct mtx			scan_mtx;

	int				supbands;	/* Number of supported bands. */
	int				max_rates;	/* Maximum number of bitrates supported in any channel. */
	int				scan_ie_len;	/* Length of common per-band scan IEs. */

	bool				update_mc;
	bool				update_wme;

	/* Must be last! */
	struct ieee80211_hw		hw __aligned(CACHE_LINE_SIZE);
};
#define	LHW_TO_HW(_lhw)		(&(_lhw)->hw)
#define	HW_TO_LHW(_hw)		container_of(_hw, struct lkpi_hw, hw)

struct lkpi_chanctx {
	bool				added_to_drv;	/* Managed by MO */
	struct ieee80211_chanctx_conf	conf __aligned(CACHE_LINE_SIZE);
};
#define	LCHANCTX_TO_CHANCTX_CONF(_lchanctx)		\
    (&(_lchanctx)->conf)
#define	CHANCTX_CONF_TO_LCHANCTX(_conf)			\
    container_of(_conf, struct lkpi_chanctx, conf)

struct lkpi_wiphy {
	const struct cfg80211_ops	*ops;

	/* Must be last! */
	struct wiphy			wiphy __aligned(CACHE_LINE_SIZE);
};
#define	WIPHY_TO_LWIPHY(_wiphy)	container_of(_wiphy, struct lkpi_wiphy, wiphy)
#define	LWIPHY_TO_WIPHY(_lwiphy)	(&(_lwiphy)->wiphy)

#define	LKPI_80211_LHW_LOCK_INIT(_lhw)			\
    sx_init_flags(&(_lhw)->sx, "lhw", SX_RECURSE);
#define	LKPI_80211_LHW_LOCK_DESTROY(_lhw)		\
    sx_destroy(&(_lhw)->sx);
#define	LKPI_80211_LHW_LOCK(_lhw)			\
    sx_xlock(&(_lhw)->sx)
#define	LKPI_80211_LHW_UNLOCK(_lhw)			\
    sx_xunlock(&(_lhw)->sx)
#define	LKPI_80211_LHW_LOCK_ASSERT(_lhw)		\
    sx_assert(&(_lhw)->sx, SA_LOCKED)
#define	LKPI_80211_LHW_UNLOCK_ASSERT(_lhw)		\
    sx_assert(&(_lhw)->sx, SA_UNLOCKED)

#define	LKPI_80211_LHW_SCAN_LOCK_INIT(_lhw)		\
    mtx_init(&(_lhw)->scan_mtx, "lhw-scan", NULL, MTX_DEF | MTX_RECURSE);
#define	LKPI_80211_LHW_SCAN_LOCK_DESTROY(_lhw)		\
    mtx_destroy(&(_lhw)->scan_mtx);
#define	LKPI_80211_LHW_SCAN_LOCK(_lhw)			\
    mtx_lock(&(_lhw)->scan_mtx)
#define	LKPI_80211_LHW_SCAN_UNLOCK(_lhw)		\
    mtx_unlock(&(_lhw)->scan_mtx)
#define	LKPI_80211_LHW_SCAN_LOCK_ASSERT(_lhw)		\
    mtx_assert(&(_lhw)->scan_mtx, MA_OWNED)
#define	LKPI_80211_LHW_SCAN_UNLOCK_ASSERT(_lhw)		\
    mtx_assert(&(_lhw)->scan_mtx, MA_NOTOWNED)

#define	LKPI_80211_LHW_TXQ_LOCK_INIT(_lhw)		\
    mtx_init(&(_lhw)->txq_mtx, "lhw-txq", NULL, MTX_DEF | MTX_RECURSE);
#define	LKPI_80211_LHW_TXQ_LOCK_DESTROY(_lhw)		\
    mtx_destroy(&(_lhw)->txq_mtx);
#define	LKPI_80211_LHW_TXQ_LOCK(_lhw)			\
    mtx_lock(&(_lhw)->txq_mtx)
#define	LKPI_80211_LHW_TXQ_UNLOCK(_lhw)			\
    mtx_unlock(&(_lhw)->txq_mtx)
#define	LKPI_80211_LHW_TXQ_LOCK_ASSERT(_lhw)		\
    mtx_assert(&(_lhw)->txq_mtx, MA_OWNED)
#define	LKPI_80211_LHW_TXQ_UNLOCK_ASSERT(_lhw)		\
    mtx_assert(&(_lhw)->txq_mtx, MA_NOTOWNED)

#define	LKPI_80211_LHW_LVIF_LOCK(_lhw)	sx_xlock(&(_lhw)->lvif_sx)
#define	LKPI_80211_LHW_LVIF_UNLOCK(_lhw) sx_xunlock(&(_lhw)->lvif_sx)

#define	LKPI_80211_LVIF_LOCK(_lvif)	mtx_lock(&(_lvif)->mtx)
#define	LKPI_80211_LVIF_UNLOCK(_lvif)	mtx_unlock(&(_lvif)->mtx)

#define	LKPI_80211_LSTA_LOCK(_lsta)	mtx_lock(&(_lsta)->txq_mtx)
#define	LKPI_80211_LSTA_UNLOCK(_lsta)	mtx_unlock(&(_lsta)->txq_mtx)

#define	LKPI_80211_LTXQ_LOCK_INIT(_ltxq)		\
    mtx_init(&(_ltxq)->ltxq_mtx, "ltxq", NULL, MTX_DEF);
#define	LKPI_80211_LTXQ_LOCK_DESTROY(_ltxq)		\
    mtx_destroy(&(_ltxq)->ltxq_mtx);
#define	LKPI_80211_LTXQ_LOCK(_ltxq)			\
    mtx_lock(&(_ltxq)->ltxq_mtx)
#define	LKPI_80211_LTXQ_UNLOCK(_ltxq)			\
    mtx_unlock(&(_ltxq)->ltxq_mtx)
#define	LKPI_80211_LTXQ_LOCK_ASSERT(_ltxq)		\
    mtx_assert(&(_ltxq)->ltxq_mtx, MA_OWNED)
#define	LKPI_80211_LTXQ_UNLOCK_ASSERT(_ltxq)		\
    mtx_assert(&(_ltxq)->ltxq_mtx, MA_NOTOWNED)

int lkpi_80211_mo_start(struct ieee80211_hw *);
void lkpi_80211_mo_stop(struct ieee80211_hw *);
int lkpi_80211_mo_get_antenna(struct ieee80211_hw *, u32 *, u32 *);
int lkpi_80211_mo_set_frag_threshold(struct ieee80211_hw *, uint32_t);
int lkpi_80211_mo_set_rts_threshold(struct ieee80211_hw *, uint32_t);
int lkpi_80211_mo_add_interface(struct ieee80211_hw *, struct ieee80211_vif *);
void lkpi_80211_mo_remove_interface(struct ieee80211_hw *, struct ieee80211_vif *);
int lkpi_80211_mo_hw_scan(struct ieee80211_hw *, struct ieee80211_vif *,
    struct ieee80211_scan_request *);
void lkpi_80211_mo_cancel_hw_scan(struct ieee80211_hw *, struct ieee80211_vif *);
void lkpi_80211_mo_sw_scan_complete(struct ieee80211_hw *, struct ieee80211_vif *);
void lkpi_80211_mo_sw_scan_start(struct ieee80211_hw *, struct ieee80211_vif *,
    const u8 *);
u64 lkpi_80211_mo_prepare_multicast(struct ieee80211_hw *,
    struct netdev_hw_addr_list *);
void lkpi_80211_mo_configure_filter(struct ieee80211_hw *, unsigned int,
    unsigned int *, u64);
int lkpi_80211_mo_sta_state(struct ieee80211_hw *, struct ieee80211_vif *,
    struct lkpi_sta *, enum ieee80211_sta_state);
int lkpi_80211_mo_config(struct ieee80211_hw *, uint32_t);
int lkpi_80211_mo_assign_vif_chanctx(struct ieee80211_hw *, struct ieee80211_vif *,
    struct ieee80211_bss_conf *, struct ieee80211_chanctx_conf *);
void lkpi_80211_mo_unassign_vif_chanctx(struct ieee80211_hw *, struct ieee80211_vif *,
    struct ieee80211_bss_conf *, struct ieee80211_chanctx_conf **);
int lkpi_80211_mo_add_chanctx(struct ieee80211_hw *, struct ieee80211_chanctx_conf *);
void lkpi_80211_mo_change_chanctx(struct ieee80211_hw *,
    struct ieee80211_chanctx_conf *, uint32_t);
void lkpi_80211_mo_remove_chanctx(struct ieee80211_hw *,
    struct ieee80211_chanctx_conf *);
void lkpi_80211_mo_bss_info_changed(struct ieee80211_hw *, struct ieee80211_vif *,
    struct ieee80211_bss_conf *, uint64_t);
int lkpi_80211_mo_conf_tx(struct ieee80211_hw *, struct ieee80211_vif *,
    uint32_t, uint16_t, const struct ieee80211_tx_queue_params *);
void lkpi_80211_mo_flush(struct ieee80211_hw *, struct ieee80211_vif *,
    uint32_t, bool);
void lkpi_80211_mo_mgd_prepare_tx(struct ieee80211_hw *, struct ieee80211_vif *,
    struct ieee80211_prep_tx_info *);
void lkpi_80211_mo_mgd_complete_tx(struct ieee80211_hw *, struct ieee80211_vif *,
    struct ieee80211_prep_tx_info *);
void lkpi_80211_mo_tx(struct ieee80211_hw *, struct ieee80211_tx_control *,
    struct sk_buff *);
void lkpi_80211_mo_wake_tx_queue(struct ieee80211_hw *, struct ieee80211_txq *);
void lkpi_80211_mo_sync_rx_queues(struct ieee80211_hw *);
void lkpi_80211_mo_sta_pre_rcu_remove(struct ieee80211_hw *,
    struct ieee80211_vif *, struct ieee80211_sta *);
int lkpi_80211_mo_set_key(struct ieee80211_hw *, enum set_key_cmd,
    struct ieee80211_vif *, struct ieee80211_sta *,
    struct ieee80211_key_conf *);
int lkpi_80211_mo_ampdu_action(struct ieee80211_hw *, struct ieee80211_vif *,
    struct ieee80211_ampdu_params *);


#endif	/* _LKPI_SRC_LINUX_80211_H */
