/*	$OpenBSD: if_mtwvar.h,v 1.1 2021/12/20 13:59:02 hastings Exp $	*/
/*
 * Copyright (c) 2008,2009 Damien Bergamini <damien.bergamini@free.fr>
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

#define MTW_MAX_RXSZ			\
	4096
#if 0
	(sizeof (uint32_t) +		\
	 sizeof (struct mtw_rxwi) +	\
	 sizeof (uint16_t) +		\
	 MCLBYTES +			\
	 sizeof (struct mtw_rxd))
#endif

#define MTW_TX_TIMEOUT	5000	/* ms */
#define	MTW_VAP_MAX		8
#define MTW_RX_RING_COUNT	1
#define MTW_TX_RING_COUNT	32

#define MTW_RXQ_COUNT		2
#define MTW_TXQ_COUNT		6

#define MTW_WCID_MAX		64
#define MTW_AID2WCID(aid)	(1 + ((aid) & 0x7))

struct mtw_rx_radiotap_header {
	struct ieee80211_radiotap_header wr_ihdr;
        uint64_t	wr_tsf;
        uint8_t		wr_flags;
	uint8_t		wr_rate;
	uint16_t	wr_chan_freq;
	uint16_t	wr_chan_flags;
	uint8_t		wr_dbm_antsignal;
	uint8_t		wr_antenna;
	uint8_t		wr_antsignal;
} __packed;
#define MTW_RATECTL_OFF 0
#define MTW_RX_RADIOTAP_PRESENT				\
	(1 << IEEE80211_RADIOTAP_FLAGS |		\
	 1 << IEEE80211_RADIOTAP_RATE |			\
	 1 << IEEE80211_RADIOTAP_CHANNEL |		\
	 1 << IEEE80211_RADIOTAP_DBM_ANTSIGNAL |	\
	 1 << IEEE80211_RADIOTAP_ANTENNA |		\
	 1 << IEEE80211_RADIOTAP_DB_ANTSIGNAL)
struct mtw_tx_radiotap_header {
	struct ieee80211_radiotap_header wt_ihdr;
	uint8_t		wt_flags;
  uint8_t		wt_rate;
	uint16_t	wt_chan_freq;
	uint16_t	wt_chan_flags;
  uint8_t		wt_hwqueue;
} __packed;

#define MTW_TX_RADIOTAP_PRESENT				\
	(1 << IEEE80211_RADIOTAP_FLAGS |		\
	 1 << IEEE80211_RADIOTAP_RATE |			\
	 1 << IEEE80211_RADIOTAP_CHANNEL)

struct mtw_softc;

struct mtw_fw_data {
  	uint16_t	len;
	uint16_t	flags;

  uint8_t *buf;
  uint32_t buflen;

};
struct mtw_tx_desc {
	uint32_t	flags;
#define RT2573_TX_BURST			(1 << 0)
#define RT2573_TX_VALID			(1 << 1)
#define RT2573_TX_MORE_FRAG		(1 << 2)
#define RT2573_TX_NEED_ACK		(1 << 3)
#define RT2573_TX_TIMESTAMP		(1 << 4)
#define RT2573_TX_OFDM			(1 << 5)
#define RT2573_TX_IFS_SIFS		(1 << 6)
#define RT2573_TX_LONG_RETRY		(1 << 7)
#define RT2573_TX_TKIPMIC		(1 << 8)
#define RT2573_TX_KEY_PAIR		(1 << 9)
#define RT2573_TX_KEY_ID(id)		(((id) & 0x3f) << 10)
#define RT2573_TX_CIP_MODE(m)		((m) << 29)

	uint16_t	wme;
#define RT2573_QID(v)		(v)
#define RT2573_AIFSN(v)		((v) << 4)
#define RT2573_LOGCWMIN(v)	((v) << 8)
#define RT2573_LOGCWMAX(v)	((v) << 12)

	uint8_t		hdrlen;
	uint8_t		xflags;
#define RT2573_TX_HWSEQ		(1 << 4)

	uint8_t		plcp_signal;
	uint8_t		plcp_service;
#define RT2573_PLCP_LENGEXT	0x80

	uint8_t		plcp_length_lo;
	uint8_t		plcp_length_hi;

	uint32_t	iv;
	uint32_t	eiv;

	uint8_t		offset;
	uint8_t		qid;
	uint8_t		txpower;
#define RT2573_DEFAULT_TXPOWER	0

	uint8_t		reserved;
} __packed;

struct mtw_tx_data {
	STAILQ_ENTRY(mtw_tx_data)	next;
	struct mbuf		*m;
	struct mtw_softc	*sc;
	struct usbd_xfer	*xfer;
	uint8_t			qid;
	uint8_t			ridx;
  uint32_t			buflen;
  //struct mtw_tx_desc desc;
	struct ieee80211_node	*ni;
  //struct mtw_txd desc;
	uint8_t	desc[sizeof(struct mtw_txd)+sizeof(struct mtw_txwi)];

};

struct mtw_rx_data {
	STAILQ_ENTRY(mtw_rx_data)	next;
	struct mtw_softc	*sc;
	struct usbd_xfer	*xfer;

	uint8_t			*buf;
};

struct mtw_tx_ring {
	struct mtw_tx_data	data[MTW_TX_RING_COUNT];
	struct usbd_pipe	*pipeh;
	int			cur;
	int			queued;
	uint8_t			pipe_no;
};

struct mtw_rx_ring {
	struct mtw_rx_data	data[MTW_RX_RING_COUNT];
	struct usbd_pipe	*pipeh;
	uint8_t			pipe_no;
};

struct mtw_vap {
	struct ieee80211vap             vap;
	struct mbuf			*beacon_mbuf;

	int                             (*newstate)(struct ieee80211vap *,
      enum ieee80211_state, int);
	void				(*recv_mgmt)(struct ieee80211_node *,
					    struct mbuf *, int,
					    const struct ieee80211_rx_stats *,
					    int, int);

	uint8_t				rvp_id;
};
#define	MTW_VAP(vap)	((struct mtw_vap *)(vap))
struct mtw_host_cmd {
	void	(*cb)(struct mtw_softc *, void *);
	uint8_t	data[256];
};

struct mtw_cmd_newstate {
	enum ieee80211_state	state;
	int			arg;
};

struct mtw_cmd_key {
	struct ieee80211_key	key;
	struct ieee80211_node	*ni;
};

#define MTW_HOST_CMD_RING_COUNT	32
struct mtw_host_cmd_ring {
	struct mtw_host_cmd	cmd[MTW_HOST_CMD_RING_COUNT];
	int			cur;
	int			next;
	int			queued;
};



struct mtw_node {
	struct ieee80211_node	ni;
       uint8_t			mgt_ridx;
      uint8_t			amrr_ridx;
	uint8_t			fix_ridx;

};
#define MTW_NODE(ni)		((struct mtw_node *)(ni))

struct mtw_mcu_tx {
	struct mtw_softc	*sc;
	struct usbd_xfer	*xfer;
	struct usbd_pipe	*pipeh;
	uint8_t			 pipe_no;
	uint8_t			*buf;
	int8_t			 seq;
};

#define MTW_MCU_IVB_LEN		0x40
struct mtw_ucode_hdr {
	uint32_t		ilm_len;
	uint32_t		dlm_len;
	uint16_t		build_ver;
	uint16_t		fw_ver;
	uint8_t			pad[4];
	char			build_time[16];
} __packed;

struct mtw_ucode {
	struct mtw_ucode_hdr	hdr;
	uint8_t			ivb[MTW_MCU_IVB_LEN];
	uint8_t			data[];
} __packed;

STAILQ_HEAD(mtw_tx_data_head, mtw_tx_data);
struct mtw_endpoint_queue {
	struct mtw_tx_data		tx_data[MTW_TX_RING_COUNT];
	struct mtw_tx_data_head		tx_qh;
	struct mtw_tx_data_head		tx_fh;
	uint32_t			tx_nfree;
};

struct mtw_cmdq {
	void			*arg0;
	void			*arg1;
	void			(*func)(void *);
	struct ieee80211_key	*k;
	struct ieee80211_key	key;
	uint8_t			mac[IEEE80211_ADDR_LEN];
	uint8_t			wcid;
};
enum {
	MTW_BULK_RX,		/* = WME_AC_BK */
	//MTW_BULK_RX1,
       MTW_BULK_TX_BE,		/* = WME_AC_BE */
       MTW_BULK_TX_VI,		/* = WME_AC_VI */
	MTW_BULK_TX_VO,		/* = WME_AC_VO */
	MTW_BULK_TX_HCCA,
	MTW_BULK_TX_PRIO,
	MTW_BULK_TX_BK,
	MTW_BULK_FW_CMD,
	MTW_BULK_RAW_TX,
	MTW_N_XFER,
};
#define	MTW_TXCNT	0
#define	MTW_SUCCESS	1
#define	MTW_RETRY	2
#define	MTW_EP_QUEUES   6
#define	MTW_FLAG_FWLOAD_NEEDED		0x01
#define	MTW_RUNNING			0x02
struct mtw_softc {
	device_t			sc_dev;
  int                             sc_idx;
	struct ieee80211com		sc_ic;
        struct ieee80211_ratectl_tx_stats sc_txs;
	int				(*sc_newstate)(struct ieee80211com *,
					    enum ieee80211_state, int);
	int				(*sc_srom_read)(struct mtw_softc *,
	    uint16_t, uint16_t *);
#define	MTW_CMDQ_MAX	16
#define	MTW_CMDQ_MASQ	(MTW_CMDQ_MAX - 1)
#define	MTW_CMDQ_ABORT	0
#define	MTW_CMDQ_GO	1
	struct mbuf			*rx_m;
        uint8_t				runbmap;
        uint8_t				running;
        uint8_t				ap_running;
	uint8_t				adhoc_running;
        uint8_t				sta_running;
	uint8_t fwloading;
        uint16_t			wcid_stats[MTW_WCID_MAX + 1][3];
        struct mbufq			sc_snd;
        uint8_t				cmdq_exec;
        uint8_t				fifo_cnt;
	uint32_t                        sc_flags;
        uint8_t				rvp_cnt;
        uint8_t				cmdq_run;
        uint8_t				rvp_bmap;
        struct mtw_cmdq			cmdq[MTW_CMDQ_MAX];
	struct task			cmdq_task;
	uint8_t				cmdq_mtw;
	uint8_t				cmdq_key_set;
	struct usb_device		*sc_udev;
	struct usb_interface		*sc_iface;
	uint32_t			cmdq_store;
	struct mtx                      sc_mtx;
  uint32_t                              sc_mcu_xferlen;
        struct usb_xfer			*sc_xfer[MTW_N_XFER];
	uint16_t			asic_ver;
	uint16_t			asic_rev;
	uint16_t			mac_ver;
	uint16_t			mac_rev;
	uint16_t			rf_rev;
        int ridx;
  int amrr_ridx;
	uint8_t				freq;
	uint8_t				ntxchains;
	uint8_t				nrxchains;

  struct mtw_txd_fw *txd_fw[4];
  int sc_sent;
  uint8_t sc_ivb_1[MTW_MCU_IVB_LEN];
	struct mtw_endpoint_queue	sc_epq[MTW_BULK_RX];
	uint8_t				rfswitch;
	uint8_t				ext_2ghz_lna;
	uint8_t				ext_5ghz_lna;
	uint8_t				calib_2ghz;
	uint8_t				calib_5ghz;
	uint8_t				txmixgain_2ghz;
	uint8_t				txmixgain_5ghz;
	int8_t				txpow1[54];
	int8_t				txpow2[54];
	int8_t				txpow3[54];
	int8_t				rssi_2ghz[3];
	int8_t				rssi_5ghz[3];
	uint8_t				lna[4];

	uint8_t				leds;
	uint16_t			led[3];
	uint32_t			txpow20mhz[5];
	uint32_t			txpow40mhz_2ghz[5];
	uint32_t			txpow40mhz_5ghz[5];

	int8_t				bbp_temp;
	uint8_t				rf_freq_offset;
	uint32_t			rf_pa_mode[2];
	int				sc_rf_calibrated;
	int				sc_bw_calibrated;
	int				sc_chan_group;




	uint8_t				cmd_seq;
	uint8_t				sc_detached;
	struct mtw_tx_ring		sc_mcu;
	struct mtw_rx_ring		rxq[MTW_RXQ_COUNT];
	struct mtw_tx_ring		txq[MTW_TXQ_COUNT];
	struct task                     ratectl_task;
	struct usb_callout              ratectl_ch;
	uint8_t				ratectl_run;
	//struct mtw_host_cmd_ring	cmdq;
	uint8_t				qfullmsk;
	int				sc_tx_timer;

	uint8_t				sc_bssid[IEEE80211_ADDR_LEN];

	union {
		struct mtw_rx_radiotap_header th;
		uint8_t	pad[64];
	}				sc_rxtapu;
#define sc_rxtap	sc_rxtapu.th
	int				sc_rxtap_len;

	union {
		struct mtw_tx_radiotap_header th;
		uint8_t	pad[64];
		uint8_t		wt_hwqueue;

	}				sc_txtapu;
#define sc_txtap	sc_txtapu.th
	int				sc_txtap_len;
	int				sc_key_tasks;
};
#define	MTW_LOCK(sc)		mtx_lock(&(sc)->sc_mtx)
#define MTW_UNLOCK(sc)		mtx_unlock(&(sc)->sc_mtx)
#define	MTW_LOCK_ASSERT(sc, t)	mtx_assert(&(sc)->sc_mtx, t)
