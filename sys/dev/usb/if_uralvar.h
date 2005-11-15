/*	$FreeBSD$	*/

/*-
 * Copyright (c) 2005
 *	Damien Bergamini <damien.bergamini@free.fr>
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

#define RAL_RX_LIST_COUNT	1
#define RAL_TX_LIST_COUNT	1

struct ural_rx_radiotap_header {
	struct ieee80211_radiotap_header wr_ihdr;
	uint8_t		wr_flags;
	uint16_t	wr_chan_freq;
	uint16_t	wr_chan_flags;
	uint8_t		wr_antenna;
	uint8_t		wr_antsignal;
};

#define RAL_RX_RADIOTAP_PRESENT						\
	((1 << IEEE80211_RADIOTAP_FLAGS) |				\
	 (1 << IEEE80211_RADIOTAP_CHANNEL) |				\
	 (1 << IEEE80211_RADIOTAP_ANTENNA) |				\
	 (1 << IEEE80211_RADIOTAP_DB_ANTSIGNAL))

struct ural_tx_radiotap_header {
	struct ieee80211_radiotap_header wt_ihdr;
	uint8_t		wt_flags;
	uint8_t		wt_rate;
	uint16_t	wt_chan_freq;
	uint16_t	wt_chan_flags;
	uint8_t		wt_antenna;
};

#define RAL_TX_RADIOTAP_PRESENT						\
	((1 << IEEE80211_RADIOTAP_FLAGS) |				\
	 (1 << IEEE80211_RADIOTAP_RATE) |				\
	 (1 << IEEE80211_RADIOTAP_CHANNEL) |				\
	 (1 << IEEE80211_RADIOTAP_ANTENNA))

struct ural_softc;

struct ural_tx_data {
	struct ural_softc	*sc;
	usbd_xfer_handle	xfer;
	uint8_t			*buf;
	struct mbuf		*m;
	struct ieee80211_node	*ni;
};

struct ural_rx_data {
	struct ural_softc	*sc;
	usbd_xfer_handle	xfer;
	uint8_t			*buf;
	struct mbuf		*m;
};

struct ural_softc {
	struct ifnet			*sc_ifp;
	struct ieee80211com		sc_ic;
	int				(*sc_newstate)(struct ieee80211com *,
					    enum ieee80211_state, int);
	USBBASEDEVICE			sc_dev;
	usbd_device_handle		sc_udev;
	usbd_interface_handle		sc_iface;

	int				sc_rx_no;
	int				sc_tx_no;

	uint32_t			asic_rev;
	uint8_t				rf_rev;

	usbd_pipe_handle		sc_rx_pipeh;
	usbd_pipe_handle		sc_tx_pipeh;

	enum ieee80211_state		sc_state;
	struct usb_task			sc_task;

	struct ural_rx_data		rx_data[RAL_RX_LIST_COUNT];
	struct ural_tx_data		tx_data[RAL_TX_LIST_COUNT];
	int				tx_queued;

	struct ieee80211_beacon_offsets	sc_bo;

	struct mtx			sc_mtx;

	struct callout			scan_ch;

	int				sc_tx_timer;

	uint32_t			rf_regs[4];
	uint8_t				txpow[14];

	struct {
		uint8_t			val;
		uint8_t			reg;
	} __packed			bbp_prom[16];

	int				led_mode;
	int				hw_radio;
	int				rx_ant;
	int				tx_ant;
	int				nb_ant;

	struct bpf_if			*sc_drvbpf;

	union {
		struct ural_rx_radiotap_header th;
		uint8_t	pad[64];
	}				sc_rxtapu;
#define sc_rxtap	sc_rxtapu.th
	int				sc_rxtap_len;

	union {
		struct ural_tx_radiotap_header th;
		uint8_t	pad[64];
	}				sc_txtapu;
#define sc_txtap	sc_txtapu.th
	int				sc_txtap_len;
};

#if 0
#define RAL_LOCK(sc)	mtx_lock(&(sc)->sc_mtx)
#define RAL_UNLOCK(sc)	mtx_unlock(&(sc)->sc_mtx)
#else
#define RAL_LOCK(sc)	do { ((sc) = (sc)); mtx_lock(&Giant); } while (0)
#define RAL_UNLOCK(sc)	mtx_unlock(&Giant)
#endif
