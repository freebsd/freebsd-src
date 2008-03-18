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
#include <net80211/ieee80211_amrr.h>

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
};

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
	uint8_t		wt_hwqueue;
};

#define WPI_TX_RADIOTAP_PRESENT						\
	((1 << IEEE80211_RADIOTAP_FLAGS) |				\
	 (1 << IEEE80211_RADIOTAP_RATE) |				\
	 (1 << IEEE80211_RADIOTAP_CHANNEL))

struct wpi_dma_info {
	bus_dma_tag_t		tag;
	bus_dmamap_t            map;
	bus_addr_t		paddr;	      /* aligned p address */
	bus_addr_t		paddr_start;  /* possibly unaligned p start*/
	caddr_t			vaddr;	      /* aligned v address */
	caddr_t			vaddr_start;  /* possibly unaligned v start */
	bus_size_t		size;
};

struct wpi_tx_data {
	bus_dmamap_t		map;
	struct mbuf		*m;
	struct ieee80211_node	*ni;
};

struct wpi_tx_ring {
	struct wpi_dma_info	desc_dma;
	struct wpi_dma_info	cmd_dma;
	struct wpi_tx_desc	*desc;
	struct wpi_tx_cmd	*cmd;
	struct wpi_tx_data	*data;
	bus_dma_tag_t		data_dmat;
	int			qid;
	int			count;
	int			queued;
	int			cur;
};

#define WPI_RBUF_COUNT ( WPI_RX_RING_COUNT + 16 )

struct wpi_rx_data {
	bus_dmamap_t		map;
	struct mbuf		*m;
};

struct wpi_rx_ring {
	struct wpi_dma_info	desc_dma;
	uint32_t		*desc;
	struct wpi_rx_data	data[WPI_RX_RING_COUNT];
	bus_dma_tag_t		data_dmat;
	int			cur;
};

struct wpi_amrr {
	struct	ieee80211_node ni;	/* must be the first */
	int	txcnt;
	int	retrycnt;
	int	success;
	int	success_threshold;
	int	recovery;
};

struct wpi_node {
        struct  ieee80211_node ni;      /* must be the first */
        struct  ieee80211_amrr_node     amn;
};

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

struct wpi_softc {
	device_t		sc_dev;
	struct ifnet		*sc_ifp;

	/* net80211 driver specifics */
	struct ieee80211com	sc_ic;
	int			(*sc_newstate)(struct ieee80211com *,
				    enum ieee80211_state, int);
	unsigned long		maxdwell; /* Max dwell time whilst scanning */

	struct mtx		sc_mtx;

	struct ieee80211_amrr	amrr;

	/* Flags indicating the current state the driver
	 * expects the hardware to be in
	 */
	uint32_t		flags;
#define WPI_FLAG_HW_RADIO_OFF	(1 << 0)
#define WPI_FLAG_SCANNING	(1 << 1)
#define WPI_FLAG_BUSY		(1 << 2)
#define WPI_FLAG_AUTH		(1 << 3)

	/* shared area */
	struct wpi_dma_info	shared_dma;
	struct wpi_shared	*shared;

	struct wpi_tx_ring	txq[WME_NUM_AC];
	struct wpi_tx_ring	cmdq;
	struct wpi_rx_ring	rxq;

	/* TX Thermal Callibration */
	struct callout		calib_to;
	int			calib_cnt;

	/* Watch dog timer */
	struct callout		watchdog_to;
	/* Hardware switch polling timer */
	struct callout		hwswitch_to;

	struct resource		*irq;
	struct resource		*mem;
	bus_space_tag_t		sc_st;
	bus_space_handle_t	sc_sh;
	void			*sc_ih;
	int			mem_rid;
	int			irq_rid;

	struct wpi_config	config;
	int			temp;


	int			sc_tx_timer;
	int			sc_scan_timer;

	struct bpf_if		*sc_drvbpf;

	struct wpi_rx_radiotap_header sc_rxtap;
	int			sc_rxtap_len;
	struct wpi_tx_radiotap_header sc_txtap;
	int			sc_txtap_len;

	/* firmware image */
	const struct firmware	*fw_fp;

	/* firmware DMA transfer */
	struct wpi_dma_info	fw_dma;

	/* command queue related variables */
#define WPI_SCAN_START		(1<<0)
#define WPI_SCAN_CURCHAN	(1<<1)
#define WPI_SCAN_STOP		(1<<2)
#define WPI_SET_CHAN		(1<<3)
#define WPI_AUTH		(1<<4)
#define WPI_RUN			(1<<5)
#define WPI_SCAN_NEXT		(1<<6)
#define WPI_RESTART		(1<<7)
#define WPI_RF_RESTART		(1<<8)
#define WPI_CMD_MAXOPS		10
	/* command queuing request type */
#define WPI_QUEUE_NORMAL	0
#define WPI_QUEUE_CLEAR		1
	int                     sc_cmd[WPI_CMD_MAXOPS];
	int                     sc_cmd_arg[WPI_CMD_MAXOPS];
	int                     sc_cmd_cur;    /* current queued scan task */
	int                     sc_cmd_next;   /* last queued scan task */
	struct mtx              sc_cmdlock;

       /* Task queues used to control the driver */
       struct taskqueue		*sc_tq; /* Main command task queue */
       struct taskqueue		*sc_tq2;/* firmware reset task queue */

       /* Tasks used by the driver */
       struct task		sc_radioontask; /* enable rf transmitter task*/
       struct task		sc_radioofftask;/* disable rf transmitter task*/
	struct task             sc_opstask; /* operation handling task */
	struct task		sc_restarttask; /* reset firmware task */

       /* Eeprom info */
	uint8_t			cap;
	uint16_t		rev;
	uint8_t			type;
	struct wpi_power_group	groups[WPI_POWER_GROUPS_COUNT];
	int8_t			maxpwr[IEEE80211_CHAN_MAX];
	char			domain[4]; //reglatory domain //XXX
};
#define WPI_LOCK_INIT(_sc) \
	mtx_init(&(_sc)->sc_mtx, device_get_nameunit((_sc)->sc_dev), \
            MTX_NETWORK_LOCK, MTX_DEF)
#define WPI_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define WPI_UNLOCK(_sc)		mtx_unlock(&(_sc)->sc_mtx)
#define WPI_LOCK_ASSERT(sc)     mtx_assert(&(sc)->sc_mtx, MA_OWNED)
#define WPI_LOCK_OWNED(_sc)	mtx_owned(&(_sc)->sc_mtx)
#define WPI_LOCK_DESTROY(_sc)	mtx_destroy(&(_sc)->sc_mtx)

#define WPI_CMD_LOCK_INIT(_sc)  \
        mtx_init(&(_sc)->sc_cmdlock, device_get_nameunit((_sc)->sc_dev), \
	    NULL, MTX_DEF)
#define WPI_CMD_LOCK_DESTROY(_sc)        mtx_destroy(&(_sc)->sc_cmdlock)
#define WPI_CMD_LOCK(_sc)                mtx_lock(&(_sc)->sc_cmdlock)
#define WPI_CMD_UNLOCK(_sc)              mtx_unlock(&(_sc)->sc_cmdlock)
