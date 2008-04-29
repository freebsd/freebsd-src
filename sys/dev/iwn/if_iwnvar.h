/*	$FreeBSD$	*/
/*-
 * Copyright (c) 2007
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

struct iwn_rx_data {
	bus_dmamap_t		map;
	struct mbuf		*m;
};

struct iwn_rx_ring {
	struct iwn_dma_info	desc_dma;
	uint32_t		*desc;
	struct iwn_rx_data	data[IWN_RX_RING_COUNT];
	bus_dma_tag_t		data_dmat;
	int			cur;
};

struct iwn_node {
	struct	ieee80211_node		ni;	/* must be the first */
	struct	ieee80211_amrr_node	amn;
};
#define	IWN_NODE(_ni)	((struct iwn_node *)(_ni))

struct iwn_calib_state {
	uint8_t		state;
#define IWN_CALIB_STATE_INIT	0
#define IWN_CALIB_STATE_ASSOC	1
#define IWN_CALIB_STATE_RUN	2
	u_int		nbeacons;
	uint32_t	noise[3];
	uint32_t	rssi[3];
	uint32_t	corr_ofdm_x1;
	uint32_t	corr_ofdm_mrc_x1;
	uint32_t	corr_ofdm_x4;
	uint32_t	corr_ofdm_mrc_x4;
	uint32_t	corr_cck_x4;
	uint32_t	corr_cck_mrc_x4;
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

struct iwn_vap {
	struct ieee80211vap	iv_vap;
	struct ieee80211_amrr	iv_amrr;
	struct callout		iv_amrr_to;

	int			(*iv_newstate)(struct ieee80211vap *,
				    enum ieee80211_state, int);
};
#define	IWN_VAP(_vap)	((struct iwn_vap *)(_vap))

struct iwn_softc {
	struct ifnet		*sc_ifp;
	int			sc_debug;
	struct callout		sc_timer_to;	/* calib+watchdog timer */
	int			sc_tx_timer;	/* tx watchdog timer/counter */
	const struct ieee80211_channel *sc_curchan;

        struct iwn_rx_radiotap_header sc_rxtap;
        int                     sc_rxtap_len;
        struct iwn_tx_radiotap_header sc_txtap;
        int                     sc_txtap_len;

	/* locks */
	struct mtx		sc_mtx;

	/* bus */
	device_t 		sc_dev;
	int			mem_rid;
	int			irq_rid;
	struct resource 	*mem;
	struct resource		*irq;

	/* shared area */
	struct iwn_dma_info	shared_dma;
	struct iwn_shared	*shared;

	/* "keep warm" page */
	struct iwn_dma_info	kw_dma;

	/* firmware image */
	const struct firmware	*fw_fp;

	/* firmware DMA transfer */
	struct iwn_dma_info	fw_dma;

	/* rings */
	struct iwn_tx_ring	txq[IWN_NTXQUEUES];
	struct iwn_rx_ring	rxq;

	bus_space_tag_t		sc_st;
	bus_space_handle_t	sc_sh;
	void 			*sc_ih;
	bus_size_t		sc_sz;

        /* command queue related variables */
#define IWN_SCAN_START		(1<<0)
#define IWN_SCAN_CURCHAN	(1<<1)
#define IWN_SCAN_STOP		(1<<2)
#define IWN_SET_CHAN		(1<<3)
#define IWN_AUTH		(1<<4)
#define IWN_SCAN_NEXT		(1<<5)
#define IWN_RUN			(1<<6)
#define IWN_RADIO_ENABLE	(1<<7)
#define IWN_RADIO_DISABLE	(1<<8)
#define IWN_REINIT		(1<<9)
#define IWN_CMD_MAXOPS		10
	/* command queuing request type */
#define IWN_QUEUE_NORMAL	0
#define IWN_QUEUE_CLEAR		1
        int                     sc_cmd[IWN_CMD_MAXOPS];
        int                     sc_cmd_arg[IWN_CMD_MAXOPS];
        int                     sc_cmd_cur;    /* current queued scan task */
        int                     sc_cmd_next;   /* last queued scan task */
        struct mtx              sc_cmdlock;

	/* Task queues used to control the driver */
	struct taskqueue         *sc_tq; /* Main command task queue */

	/* Tasks used by the driver */
	struct task             sc_ops_task;	/* deferred ops */
	struct task		sc_bmiss_task;	/* beacon miss */

	/* Thermal calibration */
	int			calib_cnt;
	struct iwn_calib_state	calib;

	struct iwn_rx_stat	last_rx_stat;
	int			last_rx_valid;
	struct iwn_ucode_info	ucode_info;
	struct iwn_config	config;
	uint32_t		rawtemp;
	int			temp;
	int			noise;
	uint8_t			antmsk;

	struct iwn_eeprom_band	bands[IWN_NBANDS];
	int16_t			eeprom_voltage;
	int8_t			maxpwr2GHz;
	int8_t			maxpwr5GHz;
};

#define IWN_LOCK_INIT(_sc) \
	mtx_init(&(_sc)->sc_mtx, device_get_nameunit((_sc)->sc_dev), \
	     MTX_NETWORK_LOCK, MTX_DEF)
#define IWN_LOCK(_sc)			mtx_lock(&(_sc)->sc_mtx)
#define IWN_LOCK_ASSERT(_sc)		mtx_assert(&(_sc)->sc_mtx, MA_OWNED)
#define IWN_UNLOCK(_sc)			mtx_unlock(&(_sc)->sc_mtx)
#define IWN_LOCK_DESTROY(_sc)		mtx_destroy(&(_sc)->sc_mtx)
#define IWN_CMD_LOCK_INIT(_sc) \
	mtx_init(&(_sc)->sc_cmdlock, device_get_nameunit((_sc)->sc_dev), \
	     NULL, MTX_DEF);
#define IWN_CMD_LOCK_DESTROY(_sc)	mtx_destroy(&(_sc)->sc_cmdlock)
#define IWN_CMD_LOCK(_sc)		mtx_lock(&(_sc)->sc_cmdlock)
#define IWN_CMD_UNLOCK(_sc)		mtx_unlock(&(_sc)->sc_cmdlock)
