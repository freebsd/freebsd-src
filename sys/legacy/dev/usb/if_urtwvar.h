/*	$FreeBSD$	*/

/*-
 * Copyright (c) 2008 Weongyo Jeong <weongyo@FreeBSD.org>
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
 */

/* XXX no definition at net80211?  */
#define URTW_MAX_CHANNELS		15

struct urtw_data {
	struct urtw_softc	*sc;
	usbd_xfer_handle	xfer;
	uint8_t			*buf;
	struct mbuf		*m;
	struct ieee80211_node	*ni;		/* NB: tx only */
};

/* XXX not correct..  */
#define	URTW_MIN_RXBUFSZ						\
	(sizeof(struct ieee80211_frame_min))

#define URTW_RX_DATA_LIST_COUNT		1
#define URTW_TX_DATA_LIST_COUNT		16
#define URTW_RX_MAXSIZE			0x9c4
#define URTW_TX_MAXSIZE			0x9c4

struct urtw_rx_radiotap_header {
	struct ieee80211_radiotap_header wr_ihdr;
	uint8_t		wr_flags;
	uint16_t	wr_chan_freq;
	uint16_t	wr_chan_flags;
	int8_t		wr_dbm_antsignal;
} __packed;

#define URTW_RX_RADIOTAP_PRESENT					\
	((1 << IEEE80211_RADIOTAP_FLAGS) |				\
	 (1 << IEEE80211_RADIOTAP_CHANNEL) |				\
	 (1 << IEEE80211_RADIOTAP_DBM_ANTSIGNAL))

struct urtw_tx_radiotap_header {
	struct ieee80211_radiotap_header wt_ihdr;
	uint8_t		wt_flags;
	uint16_t	wt_chan_freq;
	uint16_t	wt_chan_flags;
} __packed;

#define URTW_TX_RADIOTAP_PRESENT					\
	((1 << IEEE80211_RADIOTAP_FLAGS) |				\
	 (1 << IEEE80211_RADIOTAP_CHANNEL))

struct urtw_vap {
	struct ieee80211vap		vap;
	int				(*newstate)(struct ieee80211vap *,
					    enum ieee80211_state, int);
};
#define	URTW_VAP(vap)	((struct urtw_vap *)(vap))

struct urtw_softc {
	struct ifnet			*sc_ifp;
	device_t			sc_dev;
	usbd_device_handle		sc_udev;
	usbd_interface_handle		sc_iface;
	struct mtx			sc_mtx;

	int				sc_debug;
	int				sc_if_flags;
	int				sc_flags;
#define	URTW_INIT_ONCE			(1 << 1)
	struct usb_task			sc_task;
	struct usb_task			sc_ctxtask;
	int				sc_ctxarg;
#define	URTW_SET_CHANNEL		1
	enum ieee80211_state		sc_state;
	int				sc_arg;
	int				(*sc_newstate)(struct ieee80211com *,
					    enum ieee80211_state, int);

	int				sc_epromtype;
#define URTW_EEPROM_93C46		0
#define URTW_EEPROM_93C56		1
	uint8_t				sc_crcmon;
	uint8_t				sc_bssid[IEEE80211_ADDR_LEN];

	/* for RF  */
	usbd_status			(*sc_rf_init)(struct urtw_softc *);
	usbd_status			(*sc_rf_set_chan)(struct urtw_softc *,
					    int);
	usbd_status			(*sc_rf_set_sens)(struct urtw_softc *,
					    int);
	uint8_t				sc_rfchip;
	uint32_t			sc_max_sens;
	uint32_t			sc_sens;
	/* for LED  */
	struct callout			sc_led_ch;
	struct usb_task			sc_ledtask;
	uint8_t				sc_psr;
	uint8_t				sc_strategy;
#define	URTW_LED_GPIO			1
	uint8_t				sc_gpio_ledon;
	uint8_t				sc_gpio_ledinprogress;
	uint8_t				sc_gpio_ledstate;
	uint8_t				sc_gpio_ledpin;
	uint8_t				sc_gpio_blinktime;
	uint8_t				sc_gpio_blinkstate;
	/* RX/TX  */
	usbd_pipe_handle		sc_rxpipe;
	usbd_pipe_handle		sc_txpipe_low;
	usbd_pipe_handle		sc_txpipe_normal;
#define	URTW_PRIORITY_LOW		0
#define	URTW_PRIORITY_NORMAL		1
#define URTW_DATA_TIMEOUT		10000		/* 10 sec  */
	struct urtw_data		sc_rxdata[URTW_RX_DATA_LIST_COUNT];
	struct urtw_data		sc_txdata[URTW_TX_DATA_LIST_COUNT];
	uint32_t			sc_tx_low_queued;
	uint32_t			sc_tx_normal_queued;
	uint32_t			sc_txidx;
	uint8_t				sc_rts_retry;
	uint8_t				sc_tx_retry;
	uint8_t				sc_preamble_mode;
#define	URTW_PREAMBLE_MODE_SHORT	1
#define	URTW_PREAMBLE_MODE_LONG		2
	struct callout			sc_watchdog_ch;
	int				sc_txtimer;
	int				sc_currate;
	/* TX power  */
	uint8_t				sc_txpwr_cck[URTW_MAX_CHANNELS];
	uint8_t				sc_txpwr_cck_base;
	uint8_t				sc_txpwr_ofdm[URTW_MAX_CHANNELS];
	uint8_t				sc_txpwr_ofdm_base;

	struct	urtw_rx_radiotap_header	sc_rxtap;
	int				sc_rxtap_len;
	struct	urtw_tx_radiotap_header	sc_txtap;
	int				sc_txtap_len;
};

#define URTW_LOCK(sc)			mtx_lock(&(sc)->sc_mtx)
#define URTW_UNLOCK(sc)			mtx_unlock(&(sc)->sc_mtx)
#define URTW_ASSERT_LOCKED(sc)		mtx_assert(&(sc)->sc_mtx, MA_OWNED)
