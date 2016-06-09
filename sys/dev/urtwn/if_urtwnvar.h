/*-
 * Copyright (c) 2010 Damien Bergamini <damien.bergamini@free.fr>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 * 
 * $OpenBSD: if_urtwnreg.h,v 1.3 2010/11/16 18:02:59 damien Exp $
 * $FreeBSD$
 */

#define URTWN_RX_LIST_COUNT		64
#define URTWN_TX_LIST_COUNT		8
#define URTWN_HOST_CMD_RING_COUNT	32

#define URTWN_RXBUFSZ	(8 * 1024)
//#define URTWN_TXBUFSZ	(sizeof(struct r92c_tx_desc) + IEEE80211_MAX_LEN)
/* Leave enough space for an A-MSDU frame */
#define URTWN_TXBUFSZ	(16 * 1024)
#define	URTWN_RX_DESC_SIZE	(sizeof(struct r92c_rx_stat))
#define	URTWN_TX_DESC_SIZE	(sizeof(struct r92c_tx_desc))

#define URTWN_TX_TIMEOUT	5000	/* ms */

#define URTWN_LED_LINK	0
#define URTWN_LED_DATA	1

struct urtwn_rx_radiotap_header {
	struct ieee80211_radiotap_header wr_ihdr;
	uint64_t	wr_tsft;
	uint8_t		wr_flags;
	uint8_t		wr_rate;
	uint16_t	wr_chan_freq;
	uint16_t	wr_chan_flags;
	int8_t		wr_dbm_antsignal;
	int8_t		wr_dbm_antnoise;
} __packed __aligned(8);

#define URTWN_RX_RADIOTAP_PRESENT			\
	(1 << IEEE80211_RADIOTAP_TSFT |			\
	 1 << IEEE80211_RADIOTAP_FLAGS |		\
	 1 << IEEE80211_RADIOTAP_RATE |			\
	 1 << IEEE80211_RADIOTAP_CHANNEL |		\
	 1 << IEEE80211_RADIOTAP_DBM_ANTSIGNAL |	\
	 1 << IEEE80211_RADIOTAP_DBM_ANTNOISE)

struct urtwn_tx_radiotap_header {
	struct ieee80211_radiotap_header wt_ihdr;
	uint8_t		wt_flags;
	uint16_t	wt_chan_freq;
	uint16_t	wt_chan_flags;
} __packed __aligned(8);

#define URTWN_TX_RADIOTAP_PRESENT			\
	(1 << IEEE80211_RADIOTAP_FLAGS |		\
	 1 << IEEE80211_RADIOTAP_CHANNEL)

struct urtwn_softc;

struct urtwn_data {
	struct urtwn_softc		*sc;
	uint8_t				*buf;
	uint16_t			buflen;
	struct mbuf			*m;
	struct ieee80211_node		*ni;
	STAILQ_ENTRY(urtwn_data)	next;
};
typedef STAILQ_HEAD(, urtwn_data) urtwn_datahead;

union sec_param {
	struct ieee80211_key		key;
};

#define CMD_FUNC_PROTO			void (*func)(struct urtwn_softc *, \
					    union sec_param *)

struct urtwn_cmdq {
	union sec_param			data;
	CMD_FUNC_PROTO;
};
#define URTWN_CMDQ_SIZE			16

struct urtwn_fw_info {
	const uint8_t		*data;
	size_t			size;
};

struct urtwn_node {
	struct ieee80211_node	ni;	/* must be the first */
	uint8_t			id;
	int			last_rssi;
};
#define URTWN_NODE(ni)	((struct urtwn_node *)(ni))

struct urtwn_vap {
	struct ieee80211vap	vap;

	struct r92c_tx_desc	bcn_desc;
	struct mbuf		*bcn_mbuf;
	struct task		tsf_task_adhoc;

	const struct ieee80211_key	*keys[IEEE80211_WEP_NKID];

	int			(*newstate)(struct ieee80211vap *,
				    enum ieee80211_state, int);
	void			(*recv_mgmt)(struct ieee80211_node *,
				    struct mbuf *, int,
				    const struct ieee80211_rx_stats *,
				    int, int);
};
#define	URTWN_VAP(vap)	((struct urtwn_vap *)(vap))

enum {
	URTWN_BULK_RX,
	URTWN_BULK_TX_BE,	/* = WME_AC_BE */
	URTWN_BULK_TX_BK,	/* = WME_AC_BK */
	URTWN_BULK_TX_VI,	/* = WME_AC_VI */
	URTWN_BULK_TX_VO,	/* = WME_AC_VI */
	URTWN_N_TRANSFER = 5,
};

#define	URTWN_EP_QUEUES	URTWN_BULK_RX

union urtwn_rom {
	struct r92c_rom			r92c_rom;
	struct r88e_rom			r88e_rom;
};

struct urtwn_softc {
	struct ieee80211com		sc_ic;
	struct mbufq			sc_snd;
	device_t			sc_dev;
	struct usb_device		*sc_udev;

	uint32_t			sc_debug;
	uint8_t				sc_iface_index;
	uint8_t				sc_flags;
#define URTWN_FLAG_CCK_HIPWR	0x01
#define URTWN_DETACHED		0x02
#define URTWN_RUNNING		0x04
#define URTWN_FW_LOADED		0x08
#define URTWN_TEMP_MEASURED	0x10

	u_int				chip;
#define	URTWN_CHIP_92C		0x01
#define	URTWN_CHIP_92C_1T2R	0x02
#define	URTWN_CHIP_UMC		0x04
#define	URTWN_CHIP_UMC_A_CUT	0x08
#define	URTWN_CHIP_88E		0x10

#define URTWN_CHIP_HAS_RATECTL(_sc)	(!!((_sc)->chip & URTWN_CHIP_88E))

	void				(*sc_node_free)(struct ieee80211_node *);
	void				(*sc_rf_write)(struct urtwn_softc *,
					    int, uint8_t, uint32_t);
	int				(*sc_power_on)(struct urtwn_softc *);
	void				(*sc_power_off)(struct urtwn_softc *);

	struct ieee80211_node		*node_list[R88E_MACID_MAX + 1];
	struct mtx			nt_mtx;

	uint8_t				board_type;
	uint8_t				regulatory;
	uint8_t				pa_setting;
	int8_t				ofdm_tx_pwr_diff;
	int8_t				bw20_tx_pwr_diff;
	int				avg_pwdb;
	uint8_t				thcal_lctemp;
	int				ntxchains;
	int				nrxchains;
	int				ledlink;
	int				sc_txtimer;

	int				last_rssi;

	int				fwcur;
	struct urtwn_data		sc_rx[URTWN_RX_LIST_COUNT];
	urtwn_datahead			sc_rx_active;
	urtwn_datahead			sc_rx_inactive;
	struct urtwn_data		sc_tx[URTWN_TX_LIST_COUNT];
	urtwn_datahead			sc_tx_active;
	int				sc_tx_n_active;
	urtwn_datahead			sc_tx_inactive;
	urtwn_datahead			sc_tx_pending;

	union urtwn_rom			rom;
	uint16_t			last_rom_addr;

	struct callout			sc_calib_to;
	struct callout			sc_watchdog_ch;
	struct mtx			sc_mtx;
	uint32_t			keys_bmap;

	struct urtwn_cmdq		cmdq[URTWN_CMDQ_SIZE];
	struct mtx			cmdq_mtx;
	struct task			cmdq_task;
	uint8_t				cmdq_first;
	uint8_t				cmdq_last;

	uint32_t			rf_chnlbw[R92C_MAX_CHAINS];
	struct usb_xfer			*sc_xfer[URTWN_N_TRANSFER];

	struct urtwn_rx_radiotap_header	sc_rxtap;
	struct urtwn_tx_radiotap_header	sc_txtap;
};

#define	URTWN_LOCK(sc)			mtx_lock(&(sc)->sc_mtx)
#define	URTWN_UNLOCK(sc)		mtx_unlock(&(sc)->sc_mtx)
#define	URTWN_ASSERT_LOCKED(sc)		mtx_assert(&(sc)->sc_mtx, MA_OWNED)

#define URTWN_CMDQ_LOCK_INIT(sc) \
	mtx_init(&(sc)->cmdq_mtx, "cmdq lock", NULL, MTX_DEF)
#define URTWN_CMDQ_LOCK(sc)		mtx_lock(&(sc)->cmdq_mtx)
#define URTWN_CMDQ_UNLOCK(sc)		mtx_unlock(&(sc)->cmdq_mtx)
#define URTWN_CMDQ_LOCK_DESTROY(sc)	mtx_destroy(&(sc)->cmdq_mtx)

#define URTWN_NT_LOCK_INIT(sc) \
	mtx_init(&(sc)->nt_mtx, "node table lock", NULL, MTX_DEF)
#define URTWN_NT_LOCK(sc)		mtx_lock(&(sc)->nt_mtx)
#define URTWN_NT_UNLOCK(sc)		mtx_unlock(&(sc)->nt_mtx)
#define URTWN_NT_LOCK_DESTROY(sc)	mtx_destroy(&(sc)->nt_mtx)
