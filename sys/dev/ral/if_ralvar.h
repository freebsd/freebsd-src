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

struct ral_rx_radiotap_header {
	struct ieee80211_radiotap_header wr_ihdr;
	uint64_t	wr_tsf;
	uint8_t		wr_flags;
	uint16_t	wr_chan_freq;
	uint16_t	wr_chan_flags;
	uint8_t		wr_antenna;
	uint8_t		wr_antsignal;
};

#define RAL_RX_RADIOTAP_PRESENT						\
	((1 << IEEE80211_RADIOTAP_TSFT) |				\
	 (1 << IEEE80211_RADIOTAP_FLAGS) |				\
	 (1 << IEEE80211_RADIOTAP_CHANNEL) |				\
	 (1 << IEEE80211_RADIOTAP_ANTENNA) |				\
	 (1 << IEEE80211_RADIOTAP_DB_ANTSIGNAL))

struct ral_tx_radiotap_header {
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

struct ral_tx_data {
	bus_dmamap_t			map;
	struct mbuf			*m;
	struct ieee80211_node		*ni;
	struct ral_rssdesc		id;
};

struct ral_tx_ring {
	bus_dma_tag_t		desc_dmat;
	bus_dma_tag_t		data_dmat;
	bus_dmamap_t		desc_map;
	bus_addr_t		physaddr;
	struct ral_tx_desc	*desc;
	struct ral_tx_data	*data;
	int			count;
	int			queued;
	int			cur;
	int			next;
	int			cur_encrypt;
	int			next_encrypt;
};

struct ral_rx_data {
	bus_dmamap_t	map;
	struct mbuf	*m;
	int		drop;
};

struct ral_rx_ring {
	bus_dma_tag_t		desc_dmat;
	bus_dma_tag_t		data_dmat;
	bus_dmamap_t		desc_map;
	bus_addr_t		physaddr;
	struct ral_rx_desc	*desc;
	struct ral_rx_data	*data;
	int			count;
	int			cur;
	int			next;
	int			cur_decrypt;
};

struct ral_node {
	struct ieee80211_node	ni;
	struct ral_rssadapt	rssadapt;
};

struct ral_softc {
	struct arpcom			sc_arp;
	struct ieee80211com		sc_ic;
	int				(*sc_newstate)(struct ieee80211com *,
					    enum ieee80211_state, int);
	device_t			sc_dev;

	struct mtx			sc_mtx;

	struct callout			scan_ch;
	struct callout			rssadapt_ch;

	int				irq_rid;
	int				mem_rid;
	struct resource			*irq;
	struct resource			*mem;
	bus_space_tag_t			sc_st;
	bus_space_handle_t		sc_sh;
	void				*sc_ih;

	int				sc_tx_timer;

	uint32_t			asic_rev;
	uint32_t			eeprom_rev;
	uint8_t				rf_rev;

	struct ral_tx_ring		txq;
	struct ral_tx_ring		prioq;
	struct ral_tx_ring		atimq;
	struct ral_tx_ring		bcnq;
	struct ral_rx_ring		rxq;

	struct ieee80211_beacon_offsets	sc_bo;

	uint32_t			rf_regs[4];
	uint8_t				txpow[14];

	struct {
		uint8_t		reg;
		uint8_t		val;
	}				bbp_prom[16];

	int				led_mode;
	int				hw_radio;
	int				rx_ant;
	int				tx_ant;
	int				nb_ant;

	int				dwelltime;

	struct bpf_if			*sc_drvbpf;

	union {
		struct ral_rx_radiotap_header th;
		uint8_t	pad[64];
	}				sc_rxtapu;
#define sc_rxtap	sc_rxtapu.th
	int				sc_rxtap_len;

	union {
		struct ral_tx_radiotap_header th;
		uint8_t	pad[64];
	}				sc_txtapu;
#define sc_txtap	sc_txtapu.th
	int				sc_txtap_len;
};

extern devclass_t ral_devclass;

int	ral_attach(device_t);
int	ral_detach(device_t);
void	ral_shutdown(device_t);
int	ral_alloc(device_t, int);
void	ral_free(device_t);
void	ral_stop(void *);

#define RAL_LOCK(sc)	mtx_lock(&(sc)->sc_mtx)
#define RAL_UNLOCK(sc)	mtx_unlock(&(sc)->sc_mtx)
