/*	$FreeBSD$ 	*/

/*-
 * Copyright (c) 2005, 2006 Damien Bergamini <damien.bergamini@free.fr>
 * Copyright (c) 2006 Niall O'Higgins <niallo@openbsd.org>
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

#define	RUM_N_TRANSFER 4

struct rum_node {
	struct ieee80211_node ni;
	struct ieee80211_amrr_node amn;
};

#define	RUM_NODE(ni)   ((struct rum_node *)(ni))

struct rum_vap {
	struct ieee80211vap vap;
	struct ieee80211_beacon_offsets bo;
	struct ieee80211_amrr amrr;

	int     (*newstate) (struct ieee80211vap *,
	    	enum	ieee80211_state, int);
};

#define	RUM_VAP(vap)   ((struct rum_vap *)(vap))

struct rum_config_copy_chan {
	uint32_t chan_to_ieee;
	enum ieee80211_phymode chan_to_mode;
	uint8_t	chan_is_5ghz:1;
	uint8_t	chan_is_2ghz:1;
	uint8_t	chan_is_b:1;
	uint8_t	chan_is_a:1;
	uint8_t	chan_is_g:1;
	uint8_t	unused:3;
};

struct rum_config_copy_bss {
	uint16_t ni_intval;
	uint8_t	ni_bssid[IEEE80211_ADDR_LEN];
	uint8_t	fixed_rate_none;
};

struct rum_config_copy {
	struct rum_config_copy_chan ic_curchan;
	struct rum_config_copy_chan ic_bsschan;
	struct rum_config_copy_bss iv_bss;

	enum ieee80211_opmode ic_opmode;
	uint32_t ic_flags;
	uint32_t if_flags;

	uint16_t ic_txpowlimit;
	uint16_t ic_curmode;

	uint8_t	ic_myaddr[IEEE80211_ADDR_LEN];
	uint8_t	if_broadcastaddr[IEEE80211_ADDR_LEN];
};

struct rum_rx_radiotap_header {
	struct ieee80211_radiotap_header wr_ihdr;
	uint8_t	wr_flags;
	uint8_t	wr_rate;
	uint16_t wr_chan_freq;
	uint16_t wr_chan_flags;
	uint8_t	wr_antenna;
	uint8_t	wr_antsignal;
};

#define	RT2573_RX_RADIOTAP_PRESENT					\
	((1 << IEEE80211_RADIOTAP_FLAGS) |				\
	 (1 << IEEE80211_RADIOTAP_RATE) |				\
	 (1 << IEEE80211_RADIOTAP_CHANNEL) |				\
	 (1 << IEEE80211_RADIOTAP_ANTENNA) |				\
	 (1 << IEEE80211_RADIOTAP_DB_ANTSIGNAL))

struct rum_tx_radiotap_header {
	struct ieee80211_radiotap_header wt_ihdr;
	uint8_t	wt_flags;
	uint8_t	wt_rate;
	uint16_t wt_chan_freq;
	uint16_t wt_chan_flags;
	uint8_t	wt_antenna;
};

#define	RT2573_TX_RADIOTAP_PRESENT					\
	((1 << IEEE80211_RADIOTAP_FLAGS) |				\
	 (1 << IEEE80211_RADIOTAP_RATE) |				\
	 (1 << IEEE80211_RADIOTAP_CHANNEL) |				\
	 (1 << IEEE80211_RADIOTAP_ANTENNA))

struct rum_bbp_prom {
	uint8_t	val;
	uint8_t	reg;
} __packed;

struct rum_ifq {
	struct mbuf *ifq_head;
	struct mbuf *ifq_tail;
	uint16_t ifq_len;
};

struct rum_softc {
	void   *sc_evilhack;		/* XXX this pointer must be first */

	struct rum_ifq sc_tx_queue;
	struct usb2_config_td sc_config_td;
	struct rum_tx_desc sc_tx_desc;
	struct rum_rx_desc sc_rx_desc;
	struct mtx sc_mtx;
	struct usb2_callout sc_watchdog;
	struct rum_bbp_prom sc_bbp_prom[16];
	struct rum_rx_radiotap_header sc_rxtap;
	struct rum_tx_radiotap_header sc_txtap;

	struct usb2_xfer *sc_xfer[RUM_N_TRANSFER];
	struct ifnet *sc_ifp;
	struct usb2_device *sc_udev;
	const struct ieee80211_rate_table *sc_rates;

	int     (*sc_newstate)
	        (struct ieee80211com *, enum ieee80211_state, int);

	enum ieee80211_state sc_ns_state;
	uint32_t sc_sta[6];
	uint32_t sc_unit;
	int	sc_ns_arg;

	uint16_t sc_flags;
#define	RUM_FLAG_READ_STALL		0x0001
#define	RUM_FLAG_WRITE_STALL		0x0002
#define	RUM_FLAG_LL_READY		0x0008
#define	RUM_FLAG_HL_READY		0x0010
#define	RUM_FLAG_WAIT_COMMAND		0x0020
	uint16_t sc_txtap_len;
	uint16_t sc_rxtap_len;
	uint16_t sc_last_chan;

	uint8_t	sc_txpow[44];
	uint8_t	sc_rf_rev;
	uint8_t	sc_rffreq;
	uint8_t	sc_ftype;
	uint8_t	sc_rx_ant;
	uint8_t	sc_tx_ant;
	uint8_t	sc_nb_ant;
	uint8_t	sc_ext_2ghz_lna;
	uint8_t	sc_ext_5ghz_lna;
	uint8_t	sc_sifs;
	uint8_t	sc_bbp17;
	uint8_t	sc_hw_radio;
	uint8_t	sc_amrr_timer;
	uint8_t	sc_beacon_buf[0x800];
	uint8_t	sc_myaddr[IEEE80211_ADDR_LEN];

	int8_t	sc_rssi_2ghz_corr;
	int8_t	sc_rssi_5ghz_corr;

	char	sc_name[32];
};
