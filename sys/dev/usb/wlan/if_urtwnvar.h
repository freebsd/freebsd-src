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

#define URTWN_RX_LIST_COUNT		1
#define URTWN_TX_LIST_COUNT		8
#define URTWN_HOST_CMD_RING_COUNT	32

#define URTWN_RXBUFSZ	(16 * 1024)
#define URTWN_TXBUFSZ	(sizeof(struct r92c_tx_desc) + IEEE80211_MAX_LEN)
#define	URTWN_RX_DESC_SIZE	(sizeof(struct r92c_rx_stat))
#define	URTWN_TX_DESC_SIZE	(sizeof(struct r92c_tx_desc))

#define URTWN_TX_TIMEOUT	5000	/* ms */

#define URTWN_LED_LINK	0
#define URTWN_LED_DATA	1

struct urtwn_rx_radiotap_header {
	struct ieee80211_radiotap_header wr_ihdr;
	uint8_t		wr_flags;
	uint8_t		wr_rate;
	uint16_t	wr_chan_freq;
	uint16_t	wr_chan_flags;
	int8_t		wr_dbm_antsignal;
	int8_t		wr_dbm_antnoise;
} __packed __aligned(8);

#define URTWN_RX_RADIOTAP_PRESENT			\
	(1 << IEEE80211_RADIOTAP_FLAGS |		\
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

struct urtwn_cmdq {
	void			*arg0;
	void			*arg1;
	void			(*func)(void *);
	struct ieee80211_key	*k;
	struct ieee80211_key	key;
	uint8_t			mac[IEEE80211_ADDR_LEN];
	uint8_t			wcid;
};

struct urtwn_fw_info {
	const uint8_t		*data;
	size_t			size;
};

struct urtwn_vap {
	struct ieee80211vap	vap;

	struct r92c_tx_desc	bcn_desc;
	struct mbuf		*bcn_mbuf;
	struct task		tsf_task_adhoc;

	int			(*newstate)(struct ieee80211vap *,
				    enum ieee80211_state, int);
	void			(*recv_mgmt)(struct ieee80211_node *,
				    struct mbuf *, int,
				    const struct ieee80211_rx_stats *,
				    int, int);
};
#define	URTWN_VAP(vap)	((struct urtwn_vap *)(vap))

struct urtwn_host_cmd {
	void	(*cb)(struct urtwn_softc *, void *);
	uint8_t	data[256];
};

struct urtwn_cmd_newstate {
	enum ieee80211_state	state;
	int			arg;
};

struct urtwn_cmd_key {
	struct ieee80211_key	key;
	uint16_t		associd;
};

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
	uint8_t				r88e_rom[URTWN_EFUSE_MAX_LEN];
};

struct urtwn_softc {
	struct ieee80211com		sc_ic;
	struct mbufq			sc_snd;
	device_t			sc_dev;
	struct usb_device		*sc_udev;

	int				ac2idx[WME_NUM_AC];
	u_int				sc_flags;
#define URTWN_FLAG_CCK_HIPWR	0x01
#define URTWN_DETACHED		0x02
#define	URTWN_RUNNING		0x04

	u_int				chip;
#define	URTWN_CHIP_92C		0x01
#define	URTWN_CHIP_92C_1T2R	0x02
#define	URTWN_CHIP_UMC		0x04
#define	URTWN_CHIP_UMC_A_CUT	0x08
#define	URTWN_CHIP_88E		0x10

	void				(*sc_rf_write)(struct urtwn_softc *,
					    int, uint8_t, uint32_t);
	int				(*sc_power_on)(struct urtwn_softc *);
	int				(*sc_dma_init)(struct urtwn_softc *);

	uint8_t				board_type;
	uint8_t				regulatory;
	uint8_t				pa_setting;
	int				avg_pwdb;
	int				thcal_state;
	int				thcal_lctemp;
	int				ntxchains;
	int				nrxchains;
	int				ledlink;
	int				sc_txtimer;

	int				fwcur;
	struct urtwn_data		sc_rx[URTWN_RX_LIST_COUNT];
	urtwn_datahead			sc_rx_active;
	urtwn_datahead			sc_rx_inactive;
	struct urtwn_data		sc_tx[URTWN_TX_LIST_COUNT];
	urtwn_datahead			sc_tx_active;
	urtwn_datahead			sc_tx_inactive;
	urtwn_datahead			sc_tx_pending;

	const char			*fwname;
	const struct firmware		*fw_fp;
	struct urtwn_fw_info		fw;
	void				*fw_virtaddr;

	union urtwn_rom			rom;
	uint8_t				cck_tx_pwr[6];
	uint8_t				ht40_tx_pwr[5];
	int8_t				bw20_tx_pwr_diff;
	int8_t				ofdm_tx_pwr_diff;
	uint16_t			last_rom_addr;
		
	struct callout			sc_watchdog_ch;
	struct mtx			sc_mtx;

/* need to be power of 2, otherwise URTWN_CMDQ_GET fails */
#define	URTWN_CMDQ_MAX	16
#define	URTWN_CMDQ_MASQ	(URTWN_CMDQ_MAX - 1)
	struct urtwn_cmdq		cmdq[URTWN_CMDQ_MAX];
	struct task			cmdq_task;
	uint32_t			cmdq_store;
	uint8_t                         cmdq_exec;
	uint8_t                         cmdq_run;
	uint8_t                         cmdq_key_set;
#define	URTWN_CMDQ_ABORT	0
#define	URTWN_CMDQ_GO		1

	uint32_t			rf_chnlbw[R92C_MAX_CHAINS];
	struct usb_xfer			*sc_xfer[URTWN_N_TRANSFER];

	struct urtwn_rx_radiotap_header	sc_rxtap;
	struct urtwn_tx_radiotap_header	sc_txtap;
};

#define	URTWN_LOCK(sc)			mtx_lock(&(sc)->sc_mtx)
#define	URTWN_UNLOCK(sc)		mtx_unlock(&(sc)->sc_mtx)
#define	URTWN_ASSERT_LOCKED(sc)		mtx_assert(&(sc)->sc_mtx, MA_OWNED)
