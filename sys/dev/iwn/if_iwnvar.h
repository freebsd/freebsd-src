/*	$FreeBSD$	*/
/*	$OpenBSD: if_iwnvar.h,v 1.18 2010/04/30 16:06:46 damien Exp $	*/

/*-
 * Copyright (c) 2007, 2008
 *	Damien Bergamini <damien.bergamini@free.fr>
 * Copyright (c) 2008 Sam Leffler, Errno Consulting
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

struct iwn_rx_radiotap_header {
	struct ieee80211_radiotap_header wr_ihdr;
	uint64_t	wr_tsft;
	uint8_t		wr_flags;
	uint8_t		wr_rate;
	uint16_t	wr_chan_freq;
	uint16_t	wr_chan_flags;
	int8_t		wr_dbm_antsignal;
	int8_t		wr_dbm_antnoise;
} __packed;

#define IWN_RX_RADIOTAP_PRESENT						\
	((1 << IEEE80211_RADIOTAP_TSFT) |				\
	 (1 << IEEE80211_RADIOTAP_FLAGS) |				\
	 (1 << IEEE80211_RADIOTAP_RATE) |				\
	 (1 << IEEE80211_RADIOTAP_CHANNEL) |				\
	 (1 << IEEE80211_RADIOTAP_DBM_ANTSIGNAL) |			\
	 (1 << IEEE80211_RADIOTAP_DBM_ANTNOISE))

struct iwn_tx_radiotap_header {
	struct ieee80211_radiotap_header wt_ihdr;
	uint8_t		wt_flags;
	uint8_t		wt_rate;
	uint16_t	wt_chan_freq;
	uint16_t	wt_chan_flags;
} __packed;

#define IWN_TX_RADIOTAP_PRESENT						\
	((1 << IEEE80211_RADIOTAP_FLAGS) |				\
	 (1 << IEEE80211_RADIOTAP_RATE) |				\
	 (1 << IEEE80211_RADIOTAP_CHANNEL))

struct iwn_dma_info {
	bus_dma_tag_t		tag;
	bus_dmamap_t		map;
	bus_dma_segment_t	seg;
	bus_addr_t		paddr;
	caddr_t			vaddr;
	bus_size_t		size;
};

struct iwn_tx_data {
	bus_dmamap_t		map;
	bus_addr_t		cmd_paddr;
	bus_addr_t		scratch_paddr;
	struct mbuf		*m;
	struct ieee80211_node	*ni;
};

struct iwn_tx_ring {
	struct iwn_dma_info	desc_dma;
	struct iwn_dma_info	cmd_dma;
	struct iwn_tx_desc	*desc;
	struct iwn_tx_cmd	*cmd;
	struct iwn_tx_data	data[IWN_TX_RING_COUNT];
	bus_dma_tag_t		data_dmat;
	int			qid;
	int			queued;
	int			cur;
};

struct iwn_softc;

struct iwn_rx_data {
	struct mbuf	*m;
	bus_dmamap_t	map;
};

struct iwn_rx_ring {
	struct iwn_dma_info	desc_dma;
	struct iwn_dma_info	stat_dma;
	uint32_t		*desc;
	struct iwn_rx_status	*stat;
	struct iwn_rx_data	data[IWN_RX_RING_COUNT];
	bus_dma_tag_t		data_dmat;
	int			cur;
};

struct iwn_node {
	struct	ieee80211_node		ni;	/* must be the first */
	uint16_t			disable_tid;
	uint8_t				id;
	uint8_t				ridx[IEEE80211_RATE_MAXSIZE];
};

struct iwn_calib_state {
	uint8_t		state;
#define IWN_CALIB_STATE_INIT	0
#define IWN_CALIB_STATE_ASSOC	1
#define IWN_CALIB_STATE_RUN	2

	u_int		nbeacons;
	uint32_t	noise[3];
	uint32_t	rssi[3];
	uint32_t	ofdm_x1;
	uint32_t	ofdm_mrc_x1;
	uint32_t	ofdm_x4;
	uint32_t	ofdm_mrc_x4;
	uint32_t	cck_x4;
	uint32_t	cck_mrc_x4;
	uint32_t	bad_plcp_ofdm;
	uint32_t	fa_ofdm;
	uint32_t	bad_plcp_cck;
	uint32_t	fa_cck;
	uint32_t	low_fa;
	uint8_t		cck_state;
#define IWN_CCK_STATE_INIT	0
#define IWN_CCK_STATE_LOFA	1
#define IWN_CCK_STATE_HIFA	2

	uint8_t		noise_samples[20];
	u_int		cur_noise_sample;
	uint8_t		noise_ref;
	uint32_t	energy_samples[10];
	u_int		cur_energy_sample;
	uint32_t	energy_cck;
};

struct iwn_calib_info {
	uint8_t		*buf;
	u_int		len;
};

struct iwn_fw_part {
	const uint8_t	*text;
	uint32_t	textsz;
	const uint8_t	*data;
	uint32_t	datasz;
};

struct iwn_fw_info {
	const uint8_t		*data;
	size_t			size;
	struct iwn_fw_part	init;
	struct iwn_fw_part	main;
	struct iwn_fw_part	boot;
};

struct iwn_hal {
	int		(*load_firmware)(struct iwn_softc *);
	void		(*read_eeprom)(struct iwn_softc *);
	int		(*post_alive)(struct iwn_softc *);
	int		(*nic_config)(struct iwn_softc *);
	void		(*update_sched)(struct iwn_softc *, int, int, uint8_t,
			    uint16_t);
	int		(*get_temperature)(struct iwn_softc *);
	int		(*get_rssi)(struct iwn_softc *, struct iwn_rx_stat *);
	int		(*set_txpower)(struct iwn_softc *,
			    struct ieee80211_channel *, int);
	int		(*init_gains)(struct iwn_softc *);
	int		(*set_gains)(struct iwn_softc *);
	int		(*add_node)(struct iwn_softc *, struct iwn_node_info *,
			    int);
	void		(*tx_done)(struct iwn_softc *, struct iwn_rx_desc *,
			    struct iwn_rx_data *);
#if 0	/* HT */
	void		(*ampdu_tx_start)(struct iwn_softc *,
			    struct ieee80211_node *, uint8_t, uint16_t);
	void		(*ampdu_tx_stop)(struct iwn_softc *, uint8_t,
			    uint16_t);
#endif
	int		ntxqs;
	int		ndmachnls;
	uint8_t		broadcast_id;
	int		rxonsz;
	int		schedsz;
	uint32_t	fw_text_maxsz;
	uint32_t	fw_data_maxsz;
	uint32_t	fwsz;
	bus_size_t	sched_txfact_addr;
};

struct iwn_vap {
	struct ieee80211vap	iv_vap;
	uint8_t			iv_ridx;

	int			(*iv_newstate)(struct ieee80211vap *,
				    enum ieee80211_state, int);
};
#define	IWN_VAP(_vap)	((struct iwn_vap *)(_vap))

struct iwn_softc {
	struct ifnet		*sc_ifp;
	int			sc_debug;

	/* Locks */
	struct mtx		sc_mtx;

	/* Bus */
	device_t 		sc_dev;
	int			mem_rid;
	int			irq_rid;
	struct resource 	*mem;
	struct resource		*irq;

	u_int			sc_flags;
#define IWN_FLAG_HAS_OTPROM	(1 << 1)
#define IWN_FLAG_CALIB_DONE	(1 << 2)
#define IWN_FLAG_USE_ICT	(1 << 3)
#define IWN_FLAG_INTERNAL_PA	(1 << 4)

	uint8_t 		hw_type;
	const struct iwn_hal	*sc_hal;
	const char		*fwname;
	const struct iwn_sensitivity_limits
				*limits;

	/* TX scheduler rings. */
	struct iwn_dma_info	sched_dma;
	uint16_t		*sched;
	uint32_t		sched_base;

	/* "Keep Warm" page. */
	struct iwn_dma_info	kw_dma;

	/* Firmware image. */
	const struct firmware	*fw_fp;

	/* Firmware DMA transfer. */
	struct iwn_dma_info	fw_dma;

	/* ICT table. */
	struct iwn_dma_info	ict_dma;
	uint32_t		*ict;
	int			ict_cur;

	/* TX/RX rings. */
	struct iwn_tx_ring	txq[IWN5000_NTXQUEUES];
	struct iwn_rx_ring	rxq;

	bus_space_tag_t		sc_st;
	bus_space_handle_t	sc_sh;
	void 			*sc_ih;
	bus_size_t		sc_sz;
	int			sc_cap_off;	/* PCIe Capabilities. */

	/* Tasks used by the driver */
	struct task             sc_reinit_task;
	struct task		sc_radioon_task;
	struct task		sc_radiooff_task;

	int			calib_cnt;
	struct iwn_calib_state	calib;
	u_int			calib_init;
	u_int			calib_runtime;
#define	IWN_CALIB_XTAL			(1 << IWN_CALIB_IDX_XTAL)
#define	IWN_CALIB_DC			(1 << IWN_CALIB_IDX_DC)
#define	IWN_CALIB_LO			(1 << IWN_CALIB_IDX_LO)
#define	IWN_CALIB_TX_IQ			(1 << IWN_CALIB_IDX_TX_IQ)
#define	IWN_CALIB_TX_IQ_PERIODIC	(1 << IWN_CALIB_IDX_TX_IQ_PERIODIC)
#define	IWN_CALIB_BASE_BAND		(1 << IWN_CALIB_IDX_BASE_BAND)
#define	IWN_CALIB_NUM			6
	struct iwn_calib_info	calib_results[IWN_CALIB_NUM];
#define	IWN_CALIB_IDX_XTAL		0
#define	IWN_CALIB_IDX_DC		1
#define	IWN_CALIB_IDX_LO		2
#define	IWN_CALIB_IDX_TX_IQ		3
#define	IWN_CALIB_IDX_TX_IQ_PERIODIC	4
#define	IWN_CALIB_IDX_BASE_BAND		5

	struct iwn_fw_info	fw;
	uint32_t		errptr;

	struct iwn_rx_stat	last_rx_stat;
	int			last_rx_valid;
	struct iwn_ucode_info	ucode_info;
	struct iwn_rxon		rxon;
	uint32_t		rawtemp;
	int			temp;
	int			noise;
	uint32_t		qfullmsk;

	uint32_t		prom_base;
	struct iwn4965_eeprom_band
				bands[IWN_NBANDS];
	struct iwn_eeprom_chan	eeprom_channels[IWN_NBANDS][IWN_MAX_CHAN_PER_BAND];
	uint16_t		rfcfg;
	uint8_t			calib_ver;
	char			eeprom_domain[4];
	int16_t			eeprom_voltage;
	int8_t			maxpwr2GHz;
	int8_t			maxpwr5GHz;
	int8_t			maxpwr[IEEE80211_CHAN_MAX];
	int8_t			enh_maxpwr[35];

	int32_t			temp_off;
	uint32_t		int_mask;
	uint8_t			ntxchains;
	uint8_t			nrxchains;
	uint8_t			txchainmask;
	uint8_t			rxchainmask;
	uint8_t			chainmask;

	struct callout		sc_timer_to;
	int			sc_tx_timer;

	struct iwn_rx_radiotap_header sc_rxtap;
	struct iwn_tx_radiotap_header sc_txtap;
};

#define IWN_LOCK_INIT(_sc) \
	mtx_init(&(_sc)->sc_mtx, device_get_nameunit((_sc)->sc_dev), \
	    MTX_NETWORK_LOCK, MTX_DEF)
#define IWN_LOCK(_sc)			mtx_lock(&(_sc)->sc_mtx)
#define IWN_LOCK_ASSERT(_sc)		mtx_assert(&(_sc)->sc_mtx, MA_OWNED)
#define IWN_UNLOCK(_sc)			mtx_unlock(&(_sc)->sc_mtx)
#define IWN_LOCK_DESTROY(_sc)		mtx_destroy(&(_sc)->sc_mtx)
