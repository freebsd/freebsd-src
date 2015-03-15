/*	$FreeBSD$	*/

/*-
 * Copyright (c) 2006,2007
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
struct wpi_rx_radiotap_header {
	struct ieee80211_radiotap_header wr_ihdr;
	uint64_t	wr_tsft;
	uint8_t		wr_flags;
	uint8_t		wr_rate;
	uint16_t	wr_chan_freq;
	uint16_t	wr_chan_flags;
	int8_t		wr_dbm_antsignal;
	int8_t		wr_dbm_antnoise;
	uint8_t		wr_antenna;
} __packed;

#define WPI_RX_RADIOTAP_PRESENT						\
	((1 << IEEE80211_RADIOTAP_TSFT) |				\
	 (1 << IEEE80211_RADIOTAP_FLAGS) |				\
	 (1 << IEEE80211_RADIOTAP_RATE) |				\
	 (1 << IEEE80211_RADIOTAP_CHANNEL) |				\
	 (1 << IEEE80211_RADIOTAP_DBM_ANTSIGNAL) |			\
	 (1 << IEEE80211_RADIOTAP_DBM_ANTNOISE) |			\
	 (1 << IEEE80211_RADIOTAP_ANTENNA))

struct wpi_tx_radiotap_header {
	struct ieee80211_radiotap_header wt_ihdr;
	uint8_t		wt_flags;
	uint8_t		wt_rate;
	uint16_t	wt_chan_freq;
	uint16_t	wt_chan_flags;
} __packed;

#define WPI_TX_RADIOTAP_PRESENT						\
	((1 << IEEE80211_RADIOTAP_FLAGS) |				\
	 (1 << IEEE80211_RADIOTAP_RATE) |				\
	 (1 << IEEE80211_RADIOTAP_CHANNEL))

struct wpi_dma_info {
	bus_dma_tag_t		tag;
	bus_dmamap_t		map;
	bus_addr_t		paddr;
	caddr_t			vaddr;
	bus_size_t		size;
};

struct wpi_tx_data {
	bus_dmamap_t		map;
	bus_addr_t		cmd_paddr;
	struct mbuf		*m;
	struct ieee80211_node	*ni;
};

struct wpi_tx_ring {
	struct wpi_dma_info	desc_dma;
	struct wpi_dma_info	cmd_dma;
	struct wpi_tx_desc	*desc;
	struct wpi_tx_cmd	*cmd;
	struct wpi_tx_data	data[WPI_TX_RING_COUNT];
	bus_dma_tag_t		data_dmat;
	int			qid;
	int			queued;
	int			cur;
	int			update;
};

struct wpi_rx_data {
	struct mbuf	*m;
	bus_dmamap_t	map;
};

struct wpi_rx_ring {
	struct wpi_dma_info	desc_dma;
	uint32_t		*desc;
	struct wpi_rx_data	data[WPI_RX_RING_COUNT];
	bus_dma_tag_t		data_dmat;
	int			cur;
	int			update;
};

struct wpi_node {
	struct ieee80211_node	ni;	/* must be the first */
	uint8_t			id;
};
#define WPI_NODE(ni)	((struct wpi_node *)(ni))

struct wpi_power_sample {
	uint8_t	index;
	int8_t	power;
};

struct wpi_power_group {
#define WPI_SAMPLES_COUNT	5
	struct wpi_power_sample samples[WPI_SAMPLES_COUNT];
	uint8_t	chan;
	int8_t	maxpwr;
	int16_t	temp;
};

struct wpi_buf {
	void			*data;
	struct ieee80211_node	*ni;
	struct mbuf		*m;
	size_t			size;
	int			code;
	int			ac;
};

struct wpi_vap {
	struct ieee80211vap	vap;
	struct wpi_buf		wv_bcbuf;

	int			(*newstate)(struct ieee80211vap *,
				    enum ieee80211_state, int);
};
#define	WPI_VAP(vap)	((struct wpi_vap *)(vap))

struct wpi_fw_part {
	const uint8_t	*text;
	uint32_t	textsz;
	const uint8_t	*data;
	uint32_t	datasz;
};

struct wpi_fw_info {
	const uint8_t		*data;
	size_t			size;
	struct wpi_fw_part	init;
	struct wpi_fw_part	main;
	struct wpi_fw_part	boot;
};

struct wpi_softc {
	device_t		sc_dev;

	struct ifnet		*sc_ifp;
	int			sc_debug;

	struct mtx		sc_mtx;

	/* Flags indicating the current state the driver
	 * expects the hardware to be in
	 */
	uint32_t		flags;
#define WPI_FLAG_BUSY		(1 << 0)

	/* Shared area. */
	struct wpi_dma_info	shared_dma;
	struct wpi_shared	*shared;

	struct wpi_tx_ring	txq[WPI_NTXQUEUES];
	struct wpi_rx_ring	rxq;

	/* TX Thermal Callibration. */
	struct callout		calib_to;
	int			calib_cnt;

	/* Watch dog timers. */
	struct callout		watchdog_to;
	struct callout		watchdog_rfkill;

	/* Firmware image. */
	struct wpi_fw_info	fw;
	uint32_t		errptr;

	struct resource		*irq;
	struct resource		*mem;
	bus_space_tag_t		sc_st;
	bus_space_handle_t	sc_sh;
	void			*sc_ih;
	bus_size_t		sc_sz;
	int			sc_cap_off;	/* PCIe Capabilities. */

	struct wpi_rxon		rxon;
	int			temp;
	uint32_t		qfullmsk;
	uint32_t		nodesmsk;

	int			sc_tx_timer;
	int			sc_scan_timer;

	void			(*sc_node_free)(struct ieee80211_node *);
	void			(*sc_scan_curchan)(struct ieee80211_scan_state *,
				    unsigned long);

	struct wpi_rx_radiotap_header sc_rxtap;
	struct wpi_tx_radiotap_header sc_txtap;

	/* Firmware image. */
	const struct firmware	*fw_fp;

	/* Firmware DMA transfer. */
	struct wpi_dma_info	fw_dma;

	/* Tasks used by the driver. */
	struct task		sc_reinittask;
	struct task		sc_radiooff_task;
	struct task		sc_radioon_task;

	/* Eeprom info. */
	uint8_t			cap;
	uint16_t		rev;
	uint8_t			type;
	struct wpi_eeprom_chan
	    eeprom_channels[WPI_CHAN_BANDS_COUNT][WPI_MAX_CHAN_PER_BAND];
	struct wpi_power_group	groups[WPI_POWER_GROUPS_COUNT];
	int8_t			maxpwr[IEEE80211_CHAN_MAX];
	char			domain[4];	/* Regulatory domain. */
};

#define WPI_LOCK_INIT(_sc) \
	mtx_init(&(_sc)->sc_mtx, device_get_nameunit((_sc)->sc_dev), \
	    MTX_NETWORK_LOCK, MTX_DEF)
#define WPI_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define WPI_UNLOCK(_sc)		mtx_unlock(&(_sc)->sc_mtx)
#define WPI_LOCK_ASSERT(sc)	mtx_assert(&(sc)->sc_mtx, MA_OWNED)
#define WPI_LOCK_DESTROY(_sc)	mtx_destroy(&(_sc)->sc_mtx)
