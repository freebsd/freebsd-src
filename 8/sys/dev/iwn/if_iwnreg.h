/*	$FreeBSD$	*/
/*	$OpenBSD: if_iwnreg.h,v 1.9 2007/11/27 20:59:40 damien Exp $	*/

/*-
 * Copyright (c) 2007
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

#define	EDCA_NUM_AC  4

#define IWN_TX_RING_COUNT	256
#define IWN_RX_RING_COUNT	64

#define IWN_NTXQUEUES		16
#define IWN_NTXCHAINS		2

/*
 * Rings must be aligned on a 256-byte boundary.
 */
#define IWN_RING_DMA_ALIGN	256

/* maximum scatter/gather */
#define IWN_MAX_SCATTER	20

/* Rx buffers must be large enough to hold a full 4K A-MPDU */
#define IWN_RBUF_SIZE	(4 * 1024)

/*
 * Control and status registers.
 */
#define IWN_HWCONFIG		0x000
#define IWN_INTR_MIT		0x004
#define IWN_INTR		0x008
#define IWN_MASK		0x00c
#define IWN_INTR_STATUS		0x010
#define IWN_RESET		0x020
#define IWN_GPIO_CTL		0x024
#define IWN_EEPROM_CTL		0x02c
#define IWN_UCODE_CLR		0x05c
#define IWN_CHICKEN		0x100
#define IWN_QUEUE_OFFSET(qid)	(0x380 + (qid) * 8)
#define IWN_MEM_WADDR		0x410
#define IWN_MEM_WDATA		0x418
#define IWN_WRITE_MEM_ADDR  	0x444
#define IWN_READ_MEM_ADDR   	0x448
#define IWN_WRITE_MEM_DATA  	0x44c
#define IWN_READ_MEM_DATA   	0x450
#define IWN_TX_WIDX		0x460

#define IWN_KW_BASE		0x197c
#define IWN_TX_BASE(qid)	(0x19d0 + (qid) * 4)
#define IWN_RW_WIDX_PTR		0x1bc0
#define IWN_RX_BASE		0x1bc4
#define IWN_RX_WIDX		0x1bc8
#define IWN_RX_CONFIG		0x1c00
#define IWN_RX_STATUS		0x1c44
#define IWN_TX_CONFIG(qid)	(0x1d00 + (qid) * 32)
#define IWN_TX_STATUS		0x1eb0

#define IWN_SRAM_BASE		0xa02c00
#define IWN_TX_ACTIVE		(IWN_SRAM_BASE + 0x01c)
#define IWN_QUEUE_RIDX(qid)	(IWN_SRAM_BASE + 0x064 + (qid) * 4)
#define IWN_SELECT_QCHAIN	(IWN_SRAM_BASE + 0x0d0)
#define IWN_QUEUE_INTR_MASK	(IWN_SRAM_BASE + 0x0e4)
#define IWN_TXQ_STATUS(qid)	(IWN_SRAM_BASE + 0x104 + (qid) * 4)

/*
 * NIC internal memory offsets.
 */
#define IWN_CLOCK_CTL		0x3000
#define IWN_MEM_CLOCK2		0x3008
#define IWN_MEM_POWER		0x300c
#define IWN_MEM_PCIDEV		0x3010
#define IWN_MEM_UCODE_CTL	0x3400
#define IWN_MEM_UCODE_SRC	0x3404
#define IWN_MEM_UCODE_DST	0x3408
#define IWN_MEM_UCODE_SIZE	0x340c
#define IWN_MEM_TEXT_BASE	0x3490
#define IWN_MEM_TEXT_SIZE	0x3494
#define IWN_MEM_DATA_BASE	0x3498
#define IWN_MEM_DATA_SIZE	0x349c
#define IWN_MEM_UCODE_BASE	0x3800


/* possible flags for register IWN_HWCONFIG */
#define IWN_HW_EEPROM_LOCKED	(1 << 21)

/* possible flags for registers IWN_READ_MEM_ADDR/IWN_WRITE_MEM_ADDR */
#define IWN_MEM_4	((sizeof (uint32_t) - 1) << 24)

/* possible values for IWN_MEM_UCODE_DST */
#define IWN_FW_TEXT	0x00000000

/* possible flags for register IWN_RESET */
#define IWN_NEVO_RESET		(1 << 0)
#define IWN_SW_RESET		(1 << 7)
#define IWN_MASTER_DISABLED	(1 << 8)
#define IWN_STOP_MASTER		(1 << 9)

/* possible flags for register IWN_GPIO_CTL */
#define IWN_GPIO_CLOCK		(1 << 0)
#define IWN_GPIO_INIT		(1 << 2)
#define IWN_GPIO_MAC		(1 << 3)
#define IWN_GPIO_SLEEP		(1 << 4)
#define IWN_GPIO_PWR_STATUS	0x07000000
#define IWN_GPIO_PWR_SLEEP	(4 << 24)
#define IWN_GPIO_RF_ENABLED	(1 << 27)

/* possible flags for register IWN_CHICKEN */
#define IWN_CHICKEN_DISLOS	(1 << 29)

/* possible flags for register IWN_UCODE_CLR */
#define IWN_RADIO_OFF		(1 << 1)
#define IWN_DISABLE_CMD		(1 << 2)
#define IWN_CTEMP_STOP_RF	(1 << 3)

/* possible flags for IWN_RX_STATUS */
#define	IWN_RX_IDLE	(1 << 24)

/* possible flags for register IWN_UC_CTL */
#define IWN_UC_ENABLE	(1 << 30)
#define IWN_UC_RUN	(1 << 31)

/* possible flags for register IWN_INTR */
#define IWN_ALIVE_INTR	(1 <<  0)
#define IWN_WAKEUP_INTR	(1 <<  1)
#define IWN_SW_RX_INTR	(1 <<  3)
#define IWN_CT_REACHED	(1 <<  6)
#define IWN_RF_TOGGLED	(1 <<  7)
#define IWN_SW_ERROR	(1 << 25)
#define IWN_TX_INTR	(1 << 27)
#define IWN_HW_ERROR	(1 << 29)
#define IWN_RX_INTR	(1 << 31)

#define	IWN_INTR_BITS	"\20\1ALIVE\2WAKEUP\3SW_RX\6CT_REACHED\7RF_TOGGLED" \
	"\32SW_ERROR\34TX_INTR\36HW_ERROR\40RX_INTR"

#define IWN_INTR_MASK							\
	(IWN_SW_ERROR | IWN_HW_ERROR | IWN_TX_INTR | IWN_RX_INTR |	\
	 IWN_ALIVE_INTR | IWN_WAKEUP_INTR | IWN_SW_RX_INTR |		\
	 IWN_CT_REACHED | IWN_RF_TOGGLED)

/* possible flags for register IWN_INTR_STATUS */
#define IWN_STATUS_TXQ(x)	(1 << (x))
#define IWN_STATUS_RXQ(x)	(1 << ((x) + 16))
#define IWN_STATUS_PRI		(1 << 30)
/* shortcuts for the above */
#define IWN_TX_STATUS_INTR						\
	(IWN_STATUS_TXQ(0) | IWN_STATUS_TXQ(1) | IWN_STATUS_TXQ(6))
#define IWN_RX_STATUS_INTR						\
	(IWN_STATUS_RXQ(0) | IWN_STATUS_RXQ(1) | IWN_STATUS_RXQ(2) |	\
	 IWN_STATUS_PRI)

/* possible flags for register IWN_TX_STATUS */
#define IWN_TX_IDLE(qid)	(1 << ((qid) + 24) | 1 << ((qid) + 16))

/* possible flags/masks for register IWN_EEPROM_CTL */
#define IWN_EEPROM_READY	(1 << 0)
#define IWN_EEPROM_MSK		(1 << 1)

/* possible flags for register IWN_TXQ_STATUS */
#define IWN_TXQ_STATUS_ACTIVE	0x0007fc01

/* possible flags for register IWN_MEM_POWER */
#define IWN_POWER_RESET	(1 << 26)

/* possible flags for register IWN_MEM_TEXT_SIZE */
#define IWN_FW_UPDATED	(1 << 31)

/* possible flags for device-specific PCI register 0xe8 */
#define IWN_DIS_NOSNOOP	(1 << 11)

/* possible flags for device-specific PCI register 0xf0 */
#define IWN_ENA_L1	(1 << 1)


#define IWN_TX_WINDOW	64
struct iwn_shared {
	uint16_t	len[IWN_NTXQUEUES][512];	/* 16KB total */
	uint16_t	closed_count;
	uint16_t	closed_rx_count;
	uint16_t	finished_count;
	uint16_t	finished_rx_count;
	uint32_t	reserved[2];
} __packed;

struct iwn_tx_desc {
	uint32_t	flags;
	struct {
		uint32_t	w1;
		uint32_t	w2;
		uint32_t	w3;
	} __packed	segs[IWN_MAX_SCATTER / 2];
	/* pad to 128 bytes */
	uint32_t	reserved;
} __packed;

#define IWN_SET_DESC_NSEGS(d, x)					\
	(d)->flags = htole32(((x) & 0x1f) << 24)

/* set a segment physical address and length in a Tx descriptor */
#define IWN_SET_DESC_SEG(d, n, addr, size) do {				\
	if ((n) & 1) {							\
		(d)->segs[(n) / 2].w2 |=				\
		    htole32(((addr) & 0xffff) << 16);			\
		(d)->segs[(n) / 2].w3 =					\
		    htole32((((addr) >> 16) & 0xffff) | (size) << 20);	\
	} else {							\
		(d)->segs[(n) / 2].w1 = htole32(addr);			\
		(d)->segs[(n) / 2].w2 = htole32((size) << 4);		\
	}								\
} while (0)

struct iwn_rx_desc {
	uint32_t	len;
	uint8_t		type;
#define IWN_UC_READY		  1
#define IWN_ADD_NODE_DONE	 24
#define IWN_TX_DONE		 28
#define IWN_START_SCAN		130
#define IWN_STOP_SCAN		132
#define IWN_RX_STATISTICS	156
#define IWN_BEACON_STATISTICS	157
#define IWN_STATE_CHANGED	161
#define IWN_BEACON_MISSED	162
#define IWN_AMPDU_RX_START	192
#define IWN_AMPDU_RX_DONE	193
#define IWN_RX_DONE		195

	uint8_t		flags;
	uint8_t		idx;
	uint8_t		qid;
} __packed;

/* possible Rx status flags */
#define IWN_RX_NO_CRC_ERR	(1 << 0)
#define IWN_RX_NO_OVFL_ERR	(1 << 1)
/* shortcut for the above */
#define IWN_RX_NOERROR	(IWN_RX_NO_CRC_ERR | IWN_RX_NO_OVFL_ERR)

struct iwn_tx_cmd {
	uint8_t	code;
#define IWN_CMD_CONFIGURE		0x10	/* REPLY_RXON */
#define IWN_CMD_ASSOCIATE		0x11	/* REPLY_RXON_ASSOC */
#define IWN_CMD_EDCA_PARAMS		0x13	/* REPLY_QOS_PARAM */
#define IWN_CMD_TSF			0x14	/* REPLY_RXON_TIMING */
#define IWN_CMD_ADD_NODE		0x18	/* REPLY_ADD_STA */
#define IWN_CMD_TX_DATA			0x1c	/* REPLY_TX */
#define IWN_CMD_TX_LINK_QUALITY		0x4e	/* REPLY_TX_LINK_QUALITY_CMD */
#define IWN_CMD_SET_LED			0x48	/* REPLY_LEDS_CMD */
#define IWN_CMD_SET_POWER_MODE		0x77	/* POWER_TABLE_CMD */
#define IWN_CMD_SCAN			0x80	/* REPLY_SCAN_CMD */
#define IWN_CMD_TXPOWER			0x97	/* REPLY_TX_PWR_TABLE_CMD */
#define IWN_CMD_BLUETOOTH		0x9b	/* REPLY_BT_CONFIG */
#define IWN_CMD_GET_STATISTICS		0x9c	/* REPLY_STATISTICS_CMD */
#define IWN_CMD_SET_CRITICAL_TEMP	0xa4	/* REPLY_CT_KILL_CONFIG_CMD */
#define IWN_SENSITIVITY			0xa8	/* SENSITIVITY_CMD */
#define IWN_PHY_CALIB			0xb0	/* REPLY_PHY_CALIBRATION_CMD */
	uint8_t	flags;
	uint8_t	idx;
	uint8_t	qid;
	uint8_t	data[136];
} __packed;

/* structure for command IWN_CMD_CONFIGURE (aka RXON) */
struct iwn_config {
	uint8_t		myaddr[IEEE80211_ADDR_LEN];
	uint16_t	reserved1;
	uint8_t		bssid[IEEE80211_ADDR_LEN];
	uint16_t	reserved2;
	uint8_t		wlap[IEEE80211_ADDR_LEN];
	uint16_t	reserved3;
	uint8_t		mode;
#define IWN_MODE_HOSTAP		1
#define IWN_MODE_STA		3
#define IWN_MODE_IBSS		4
#define IWN_MODE_MONITOR	6
	uint8_t		unused4;	/* air propagation */
	uint16_t	rxchain;
#define	IWN_RXCHAIN_VALID	0x000e	/* which antennae are valid */
#define IWN_RXCHAIN_VALID_S	1
#define	IWN_RXCHAIN_FORCE	0x0070
#define	IWN_RXCHAIN_FORCE_S	4
#define	IWN_RXCHAIN_FORCE_MIMO	0x0380
#define	IWN_RXCHAIN_FORCE_MIMO_S	7
#define	IWN_RXCHAIN_CNT		0x0c00
#define	IWN_RXCHAIN_CNT_S	10
#define	IWN_RXCHAIN_MIMO_CNT	0x3000
#define	IWN_RXCHAIN_MIMO_CNT_S	12
#define IWN_RXCHAIN_MIMO_FORCE	0x4000
#define IWN_RXCHAIN_MIMO_FORCE_S	14
	uint8_t		ofdm_mask;	/* basic rates */
	uint8_t		cck_mask;	/* basic rates */
	uint16_t	associd;
	uint32_t	flags;
#define	IWN_CONFIG_24GHZ	0x00000001	/* band */
#define	IWN_CONFIG_CCK		0x00000002	/* modulation */
#define	IWN_CONFIG_AUTO		0x00000004	/* 2.4-only auto-detect */
#define	IWN_CONFIG_HTPROT	0x00000008	/* xmit with HT protection */
#define	IWN_CONFIG_SHSLOT	0x00000010	/* short slot time */
#define	IWN_CONFIG_SHPREAMBLE	0x00000020	/* short premable */
#define	IWN_CONFIG_NODIVERSITY	0x00000080	/* disable antenna diversity */
#define	IWN_CONFIG_ANTENNA_A	0x00000100
#define	IWN_CONFIG_ANTENNA_B	0x00000200
#define	IWN_CONFIG_RADAR	0x00001000	/* enable radar detect */
#define	IWN_CONFIG_NARROW	0x00002000	/* MKK narrow band select */
#define	IWN_CONFIG_TSF		0x00008000
#define	IWN_CONFIG_HT		0x06400000
#define	IWN_CONFIG_HT20		0x02000000
#define	IWN_CONFIG_HT40U	0x04000000
#define	IWN_CONFIG_HT40D	0x04400000
	uint32_t	filter;
#define IWN_FILTER_PROMISC	(1 << 0)	/* pass all data frames */
#define IWN_FILTER_CTL		(1 << 1)	/* pass ctl+mgt frames */
#define IWN_FILTER_MULTICAST	(1 << 2)	/* pass multi-cast frames */
#define IWN_FILTER_NODECRYPT	(1 << 3)	/* pass unicast undecrypted */
#define IWN_FILTER_BSS		(1 << 5)	/* station is associated */
#define IWN_FILTER_ALLBEACONS	(1 << 6)	/* pass overlapping bss beacons
						   (must be associated) */
	uint16_t	chan;		/* IEEE channel # of control/primary */
	uint8_t		ht_single_mask;	/* single-stream basic rates */
	uint8_t		ht_dual_mask;	/* dual-stream basic rates */
} __packed;

/* structure for command IWN_CMD_ASSOCIATE */
struct iwn_assoc {
	uint32_t	flags;
	uint32_t	filter;
	uint8_t		ofdm_mask;
	uint8_t		cck_mask;
	uint16_t	reserved;
} __packed;

/* structure for command IWN_CMD_EDCA_PARAMS */
struct iwn_edca_params {
	uint32_t	flags;
#define IWN_EDCA_UPDATE	(1 << 0)
#define IWN_EDCA_TXOP	(1 << 4)

	struct {
		uint16_t	cwmin;
		uint16_t	cwmax;
		uint8_t		aifsn;
		uint8_t		reserved;
		uint16_t	txoplimit;
	} __packed	ac[EDCA_NUM_AC];
} __packed;

/* structure for command IWN_CMD_TSF */
struct iwn_cmd_tsf {
	uint64_t	tstamp;
	uint16_t	bintval;
	uint16_t	atim;
	uint32_t	binitval;
	uint16_t	lintval;
	uint16_t	reserved;
} __packed;

/* structure for command IWN_CMD_ADD_NODE */
struct iwn_node_info {
	uint8_t		control;
#define IWN_NODE_UPDATE		(1 << 0)
	uint8_t		reserved1[3];
	uint8_t		macaddr[IEEE80211_ADDR_LEN];
	uint16_t	reserved2;
	uint8_t		id;
#define IWN_ID_BSS		 0
#define IWN_ID_BROADCAST	31
	uint8_t		flags;
#define IWN_FLAG_SET_KEY	(1 << 0)
	uint16_t	reserved3;
	uint16_t	security;
	uint8_t		tsc2;	/* TKIP TSC2 */
	uint8_t		reserved4;
	uint16_t	ttak[5];
	uint16_t	reserved5;
	uint8_t		key[IEEE80211_KEYBUF_SIZE];
	uint32_t	htflags;
#define IWN_MAXRXAMPDU_S	19
#define IWN_MPDUDENSITY_S	23
	uint32_t	mask;
	uint16_t	tid;
	uint8_t		rate;		/* legacy rate/MCS */
#define	IWN_RATE_MCS	0x08		/* or'd to indicate MCS */
	uint8_t		rflags;
#define	IWN_RFLAG_HT	(1 << 0)	/* use HT modulation */
#define IWN_RFLAG_CCK	(1 << 1)	/* use CCK modulation */
#define	IWN_RFLAG_HT40	(1 << 3)	/* use dual-stream */
#define	IWN_RFLAG_SGI	(1 << 5)	/* use short GI */
#define IWN_RFLAG_ANT_A	(1 << 6)	/* start on antenna port A */
#define IWN_RFLAG_ANT_B	(1 << 7)	/* start on antenna port B */
	uint8_t		add_imm;
	uint8_t		del_imm;
	uint16_t	add_imm_start;
	uint32_t	reserved6;
} __packed;

/* structure for command IWN_CMD_TX_DATA */
struct iwn_cmd_data {
	uint16_t	len;
	uint16_t	lnext;
	uint32_t	flags;
#define IWN_TX_NEED_RTS		(1 <<  1)
#define IWN_TX_NEED_CTS		(1 <<  2)
#define IWN_TX_NEED_ACK		(1 <<  3)
#define IWN_TX_USE_NODE_RATE	(1 <<  4)
#define IWN_TX_FULL_TXOP	(1 <<  7)
#define IWN_TX_BT_DISABLE	(1 << 12)	/* bluetooth coexistence */
#define IWN_TX_AUTO_SEQ		(1 << 13)
#define IWN_TX_INSERT_TSTAMP	(1 << 16)
#define IWN_TX_NEED_PADDING	(1 << 20)

	uint8_t		ntries;
	uint8_t		bluetooth;
	uint16_t	reserved1;
	uint8_t		rate;
	uint8_t		rflags;
	uint16_t	xrflags;
	uint8_t		id;
	uint8_t		security;
#define IWN_CIPHER_WEP40	1
#define IWN_CIPHER_CCMP		2
#define IWN_CIPHER_TKIP		3
#define IWN_CIPHER_WEP104	9

	uint8_t		ridx;
	uint8_t		reserved2;
	uint8_t		key[IEEE80211_KEYBUF_SIZE];
	uint16_t	fnext;
	uint16_t	reserved3;
	uint32_t	lifetime;
#define IWN_LIFETIME_INFINITE	0xffffffff

	uint32_t	loaddr;
	uint8_t		hiaddr;
	uint8_t		rts_ntries;
	uint8_t		data_ntries;
	uint8_t		tid;
	uint16_t	timeout;
	uint16_t	txop;
} __packed;

/* structure for command IWN_CMD_TX_LINK_QUALITY */
#define IWN_MAX_TX_RETRIES	16
struct iwn_cmd_link_quality {
	uint8_t		id;
	uint8_t		reserved1;
	uint16_t	ctl;
	uint8_t		flags;
	uint8_t		mimo;		/* MIMO delimiter */
	uint8_t		ssmask;		/* single stream antenna mask */
	uint8_t		dsmask;		/* dual stream antenna mask */
	uint8_t		ridx[EDCA_NUM_AC];/* starting rate index */
	uint16_t	ampdu_limit;	/* tx aggregation time limit */
	uint8_t		ampdu_disable;
	uint8_t		ampdu_max;	/* frame count limit */
	uint32_t	reserved2;
	struct {
		uint8_t		rate;
#define IWN_RATE_CCK1	 0
#define IWN_RATE_CCK11	 3
#define IWN_RATE_OFDM6	 4
#define IWN_RATE_OFDM54	11
		uint8_t		rflags;
		uint16_t	xrflags;
	} table[IWN_MAX_TX_RETRIES];
	uint32_t	reserved3;
} __packed;

/* structure for command IWN_CMD_SET_LED */
struct iwn_cmd_led {
	uint32_t	unit;	/* multiplier (in usecs) */
	uint8_t		which;
#define IWN_LED_ACTIVITY	1
#define IWN_LED_LINK		2

	uint8_t		off;
	uint8_t		on;
	uint8_t		reserved;
} __packed;

/* structure for command IWN_CMD_SET_POWER_MODE */
struct iwn_power {
	uint16_t	flags;
#define IWN_POWER_CAM	0	/* constantly awake mode */

	uint8_t		alive;
	uint8_t		debug;
	uint32_t	rx_timeout;
	uint32_t	tx_timeout;
	uint32_t	sleep[5];
	uint32_t	beacons;
} __packed;

/* structures for command IWN_CMD_SCAN */
struct iwn_scan_essid {
	uint8_t	id;
	uint8_t	len;
	uint8_t	data[IEEE80211_NWID_LEN];
} __packed;

struct iwn_scan_hdr {
	uint16_t	len;
	uint8_t		reserved1;
	uint8_t		nchan;
	uint16_t	quiet;
	uint16_t	plcp_threshold;
	uint16_t	crc_threshold;
	uint16_t	rxchain;
	uint32_t	max_svc;	/* background scans */
	uint32_t	pause_svc;	/* background scans */
	uint32_t	flags;
	uint32_t	filter;

	/* followed by a struct iwn_cmd_data */
	/* followed by an array of 4x struct iwn_scan_essid */
	/* followed by probe request body */
	/* followed by nchan x struct iwn_scan_chan */
} __packed;

struct iwn_scan_chan {
	uint8_t		flags;
#define IWN_CHAN_ACTIVE	(1 << 0)
#define IWN_CHAN_DIRECT	(1 << 1)

	uint8_t		chan;
	uint8_t		rf_gain;
	uint8_t		dsp_gain;
	uint16_t	active;		/* msecs */
	uint16_t	passive;	/* msecs */
} __packed;

/* structure for command IWN_CMD_TXPOWER */
#define IWN_RIDX_MAX	32
struct iwn_cmd_txpower {
	uint8_t	band;
	uint8_t	reserved1;
	uint8_t	chan;
	uint8_t	reserved2;
	struct {
		uint8_t	rf_gain[IWN_NTXCHAINS];
		uint8_t	dsp_gain[IWN_NTXCHAINS];
	}	power[IWN_RIDX_MAX + 1];
} __packed;

/* structure for command IWN_CMD_BLUETOOTH */
struct iwn_bluetooth {
	uint8_t		flags;
	uint8_t		lead;
	uint8_t		kill;
	uint8_t		reserved;
	uint32_t	ack;
	uint32_t	cts;
} __packed;

/* structure for command IWN_CMD_SET_CRITICAL_TEMP */
struct iwn_critical_temp {
	uint32_t	reserved;
	uint32_t	tempM;
	uint32_t	tempR;
/* degK <-> degC conversion macros */
#define IWN_CTOK(c)	((c) + 273)
#define IWN_KTOC(k)	((k) - 273)
#define IWN_CTOMUK(c)	(((c) * 1000000) + 273150000)
} __packed;

/* structure for command IWN_SENSITIVITY */
struct iwn_sensitivity_cmd {
	uint16_t	which;
#define IWN_SENSITIVITY_DEFAULTTBL	0
#define IWN_SENSITIVITY_WORKTBL		1

	uint16_t	energy_cck;
	uint16_t	energy_ofdm;
	uint16_t	corr_ofdm_x1;
	uint16_t	corr_ofdm_mrc_x1;
	uint16_t	corr_cck_mrc_x4;
	uint16_t	corr_ofdm_x4;
	uint16_t	corr_ofdm_mrc_x4;
	uint16_t	corr_barker;
	uint16_t	corr_barker_mrc;
	uint16_t	corr_cck_x4;
	uint16_t	energy_ofdm_th;
} __packed;

/* structure for command IWN_PHY_CALIB */
struct iwn_phy_calib_cmd {
	uint8_t		code;
#define IWN_SET_DIFF_GAIN	7

	uint8_t		flags;
	uint16_t	reserved1;
	int8_t		gain[3];
#define IWN_GAIN_SET	(1 << 2)

	uint8_t		reserved2;
} __packed;


/* structure for IWN_UC_READY notification */
#define IWN_NATTEN_GROUPS	5
struct iwn_ucode_info {
	uint8_t		minor;
	uint8_t		major;
	uint16_t	reserved1;
	uint8_t		revision[8];
	uint8_t		type;
	uint8_t		subtype;
#define IWN_UCODE_RUNTIME	0
#define IWN_UCODE_INIT		9

	uint16_t	reserved2;
	uint32_t	logptr;
	uint32_t	errorptr;
	uint32_t	tstamp;
	uint32_t	valid;

	/* the following fields are for UCODE_INIT only */
	int32_t		volt;
	struct {
		int32_t	chan20MHz;
		int32_t	chan40MHz;
	} __packed	temp[4];
	int32_t		atten[IWN_NATTEN_GROUPS][IWN_NTXCHAINS];
} __packed;

/* structure for IWN_TX_DONE notification */
struct iwn_tx_stat {
	uint8_t		nframes;
	uint8_t		nkill;
	uint8_t		nrts;
	uint8_t		ntries;
	uint8_t		rate;
	uint8_t		rflags;
	uint16_t	xrflags;
	uint16_t	duration;
	uint16_t	reserved;
	uint32_t	power[2];
	uint32_t	status;
#define	IWN_TX_SUCCESS			0x00
#define	IWN_TX_FAIL			0x80	/* all failures have 0x80 set */
#define	IWN_TX_FAIL_SHORT_LIMIT		0x82	/* too many RTS retries */
#define	IWN_TX_FAIL_LONG_LIMIT		0x83	/* too many retries */
#define	IWN_TX_FAIL_FIFO_UNDERRRUN	0x84	/* tx fifo not kept running */
#define	IWN_TX_FAIL_DEST_IN_PS		0x88	/* sta found in power save */
#define	IWN_TX_FAIL_TX_LOCKED		0x90	/* waiting to see traffic */
} __packed;

/* structure for IWN_BEACON_MISSED notification */
struct iwn_beacon_missed {
	uint32_t	consecutive;
	uint32_t	total;
	uint32_t	expected;
	uint32_t	received;
} __packed;

/* structure for IWN_AMPDU_RX_DONE notification */
struct iwn_rx_ampdu {
	uint16_t	len;
	uint16_t	reserved;
} __packed;

/* structure for IWN_RX_DONE and IWN_AMPDU_RX_START notifications */
struct iwn_rx_stat {
	uint8_t		phy_len;
	uint8_t		cfg_phy_len;
#define IWN_STAT_MAXLEN	20

	uint8_t		id;
	uint8_t		reserved1;
	uint64_t	tstamp;
	uint32_t	beacon;
	uint16_t	flags;
	uint16_t	chan;
	uint16_t	antenna;
	uint16_t	agc;
	uint8_t		rssi[6];
#define IWN_RSSI_TO_DBM	44

	uint8_t		reserved2[22];
	uint8_t		rate;
	uint8_t		rflags;
	uint16_t	xrflags;
	uint16_t	len;
	uint16_t	reserve3;
} __packed;

/* structure for IWN_START_SCAN notification */
struct iwn_start_scan {
	uint64_t	tstamp;
	uint32_t	tbeacon;
	uint8_t		chan;
	uint8_t		band;
	uint16_t	reserved;
	uint32_t	status;
} __packed;

/* structure for IWN_STOP_SCAN notification */
struct iwn_stop_scan {
	uint8_t		nchan;
	uint8_t		status;
	uint8_t		reserved;
	uint8_t		chan;
	uint64_t	tsf;
} __packed;

/* structure for IWN_{RX,BEACON}_STATISTICS notification */
struct iwn_rx_phy_stats {
	uint32_t	ina;
	uint32_t	fina;
	uint32_t	bad_plcp;
	uint32_t	bad_crc32;
	uint32_t	overrun;
	uint32_t	eoverrun;
	uint32_t	good_crc32;
	uint32_t	fa;
	uint32_t	bad_fina_sync;
	uint32_t	sfd_timeout;
	uint32_t	fina_timeout;
	uint32_t	no_rts_ack;
	uint32_t	rxe_limit;
	uint32_t	ack;
	uint32_t	cts;
	uint32_t	ba_resp;
	uint32_t	dsp_kill;
	uint32_t	bad_mh;
	uint32_t	rssi_sum;
	uint32_t	reserved;
} __packed;

struct iwn_rx_general_stats {
	uint32_t	bad_cts;
	uint32_t	bad_ack;
	uint32_t	not_bss;
	uint32_t	filtered;
	uint32_t	bad_chan;
	uint32_t	beacons;
	uint32_t	missed_beacons;
	uint32_t	adc_saturated;	/* time in 0.8us */
	uint32_t	ina_searched;	/* time in 0.8us */
	uint32_t	noise[3];
	uint32_t	flags;
	uint32_t	load;
	uint32_t	fa;
	uint32_t	rssi[3];
	uint32_t	energy[3];
} __packed;

struct iwn_rx_ht_phy_stats {
	uint32_t	bad_plcp;
	uint32_t	overrun;
	uint32_t	eoverrun;
	uint32_t	good_crc32;
	uint32_t	bad_crc32;
	uint32_t	bad_mh;
	uint32_t	good_ampdu_crc32;
	uint32_t	ampdu;
	uint32_t	fragment;
	uint32_t	reserved;
} __packed;

struct iwn_rx_stats {
	struct iwn_rx_phy_stats		ofdm;
	struct iwn_rx_phy_stats		cck;
	struct iwn_rx_general_stats	general;
	struct iwn_rx_ht_phy_stats	ht;
} __packed;

struct iwn_tx_stats {
	uint32_t	preamble;
	uint32_t	rx_detected;
	uint32_t	bt_defer;
	uint32_t	bt_kill;
	uint32_t	short_len;
	uint32_t	cts_timeout;
	uint32_t	ack_timeout;
	uint32_t	exp_ack;
	uint32_t	ack;
	uint32_t	msdu;
	uint32_t	busrt_err1;
	uint32_t	burst_err2;
	uint32_t	cts_collision;
	uint32_t	ack_collision;
	uint32_t	ba_timeout;
	uint32_t	ba_resched;
	uint32_t	query_ampdu;
	uint32_t	query;
	uint32_t	query_ampdu_frag;
	uint32_t	query_mismatch;
	uint32_t	not_ready;
	uint32_t	underrun;
	uint32_t	bt_ht_kill;
	uint32_t	rx_ba_resp;
	uint32_t	reserved[2];
} __packed;

struct iwn_general_stats {
	uint32_t	temp;
	uint32_t	temp_m;
	uint32_t	burst_check;
	uint32_t	burst;
	uint32_t	reserved1[4];
	uint32_t	sleep;
	uint32_t	slot_out;
	uint32_t	slot_idle;
	uint32_t	ttl_tstamp;
	uint32_t	tx_ant_a;
	uint32_t	tx_ant_b;
	uint32_t	exec;
	uint32_t	probe;
	uint32_t	reserved2[2];
	uint32_t	rx_enabled;
	uint32_t	reserved3[3];
} __packed;

struct iwn_stats {
	uint32_t			flags;
	struct iwn_rx_stats		rx;
	struct iwn_tx_stats		tx;
	struct iwn_general_stats	general;
} __packed;


/* firmware image header */
struct iwn_firmware_hdr {
	uint32_t	version;
	uint32_t	main_textsz;
	uint32_t	main_datasz;
	uint32_t	init_textsz;
	uint32_t	init_datasz;
	uint32_t	boot_textsz;
} __packed;

#define IWN_FW_MAIN_TEXT_MAXSZ	(96 * 1024)
#define IWN_FW_MAIN_DATA_MAXSZ	(40 * 1024)
#define IWN_FW_INIT_TEXT_MAXSZ	(96 * 1024)
#define IWN_FW_INIT_DATA_MAXSZ	(40 * 1024)
#define IWN_FW_BOOT_TEXT_MAXSZ	1024


/*
 * Offsets into EEPROM.
 */
#define IWN_EEPROM_MAC		0x015
#define IWN_EEPROM_DOMAIN	0x060
#define IWN_EEPROM_BAND1	0x063
#define IWN_EEPROM_BAND2	0x072
#define IWN_EEPROM_BAND3	0x080
#define IWN_EEPROM_BAND4	0x08d
#define IWN_EEPROM_BAND5	0x099
#define IWN_EEPROM_BAND6	0x0a0
#define IWN_EEPROM_BAND7	0x0a8
#define IWN_EEPROM_MAXPOW	0x0e8
#define IWN_EEPROM_VOLTAGE	0x0e9
#define IWN_EEPROM_BANDS	0x0ea

struct iwn_eeprom_chan {
	uint8_t	flags;
#define IWN_EEPROM_CHAN_VALID	(1 << 0)
#define IWN_EEPROM_CHAN_IBSS	(1 << 1)	/* adhoc permitted */
/* NB: bit 2 is reserved */
#define IWN_EEPROM_CHAN_ACTIVE	(1 << 3)	/* active/passive scan */
#define IWN_EEPROM_CHAN_RADAR	(1 << 4)	/* DFS required */
#define IWN_EEPROM_CHAN_WIDE	(1 << 5)	/* HT40 */
#define IWN_EEPROM_CHAN_NARROW	(1 << 6)	/* HT20 */

	int8_t	maxpwr;
} __packed;

#define IWN_NSAMPLES	3
struct iwn_eeprom_chan_samples {
	uint8_t	num;
	struct {
		uint8_t temp;
		uint8_t	gain;
		uint8_t	power;
		int8_t	pa_det;
	}	samples[IWN_NTXCHAINS][IWN_NSAMPLES];
} __packed;

#define IWN_NBANDS	8
struct iwn_eeprom_band {
	uint8_t	lo;	/* low channel number */
	uint8_t	hi;	/* high channel number */
	struct	iwn_eeprom_chan_samples chans[2];
} __packed;

#define IWN_MAX_PWR_INDEX	107

/*
 * RF Tx gain values from highest to lowest power (values obtained from
 * the reference driver.)
 */
static const uint8_t iwn_rf_gain_2ghz[IWN_MAX_PWR_INDEX + 1] = {
	0x3f, 0x3f, 0x3f, 0x3e, 0x3e, 0x3e, 0x3d, 0x3d, 0x3d, 0x3c, 0x3c,
	0x3c, 0x3b, 0x3b, 0x3b, 0x3a, 0x3a, 0x3a, 0x39, 0x39, 0x39, 0x38,
	0x38, 0x38, 0x37, 0x37, 0x37, 0x36, 0x36, 0x36, 0x35, 0x35, 0x35,
	0x34, 0x34, 0x34, 0x33, 0x33, 0x33, 0x32, 0x32, 0x32, 0x31, 0x31,
	0x31, 0x30, 0x30, 0x30, 0x06, 0x06, 0x06, 0x05, 0x05, 0x05, 0x04,
	0x04, 0x04, 0x03, 0x03, 0x03, 0x02, 0x02, 0x02, 0x01, 0x01, 0x01,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t iwn_rf_gain_5ghz[IWN_MAX_PWR_INDEX + 1] = {
	0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3e, 0x3e, 0x3e, 0x3d, 0x3d, 0x3d,
	0x3c, 0x3c, 0x3c, 0x3b, 0x3b, 0x3b, 0x3a, 0x3a, 0x3a, 0x39, 0x39,
	0x39, 0x38, 0x38, 0x38, 0x37, 0x37, 0x37, 0x36, 0x36, 0x36, 0x35,
	0x35, 0x35, 0x34, 0x34, 0x34, 0x33, 0x33, 0x33, 0x32, 0x32, 0x32,
	0x31, 0x31, 0x31, 0x30, 0x30, 0x30, 0x25, 0x25, 0x25, 0x24, 0x24,
	0x24, 0x23, 0x23, 0x23, 0x22, 0x18, 0x18, 0x17, 0x17, 0x17, 0x16,
	0x16, 0x16, 0x15, 0x15, 0x15, 0x14, 0x14, 0x14, 0x13, 0x13, 0x13,
	0x12, 0x08, 0x08, 0x07, 0x07, 0x07, 0x06, 0x06, 0x06, 0x05, 0x05,
	0x05, 0x04, 0x04, 0x04, 0x03, 0x03, 0x03, 0x02, 0x02, 0x02, 0x01,
	0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/*
 * DSP pre-DAC gain values from highest to lowest power (values obtained
 * from the reference driver.)
 */
static const uint8_t iwn_dsp_gain_2ghz[IWN_MAX_PWR_INDEX + 1] = {
	0x6e, 0x68, 0x62, 0x6e, 0x68, 0x62, 0x6e, 0x68, 0x62, 0x6e, 0x68,
	0x62, 0x6e, 0x68, 0x62, 0x6e, 0x68, 0x62, 0x6e, 0x68, 0x62, 0x6e,
	0x68, 0x62, 0x6e, 0x68, 0x62, 0x6e, 0x68, 0x62, 0x6e, 0x68, 0x62,
	0x6e, 0x68, 0x62, 0x6e, 0x68, 0x62, 0x6e, 0x68, 0x62, 0x6e, 0x68,
	0x62, 0x6e, 0x68, 0x62, 0x6e, 0x68, 0x62, 0x6e, 0x68, 0x62, 0x6e,
	0x68, 0x62, 0x6e, 0x68, 0x62, 0x6e, 0x68, 0x62, 0x6e, 0x68, 0x62,
	0x6e, 0x68, 0x62, 0x61, 0x60, 0x5f, 0x5e, 0x5d, 0x5c, 0x5b, 0x5a,
	0x59, 0x58, 0x57, 0x56, 0x55, 0x54, 0x53, 0x52, 0x51, 0x50, 0x4f,
	0x4e, 0x4d, 0x4c, 0x4b, 0x4a, 0x49, 0x48, 0x47, 0x46, 0x45, 0x44,
	0x43, 0x42, 0x41, 0x40, 0x3f, 0x3e, 0x3d, 0x3c, 0x3b
};

static const uint8_t iwn_dsp_gain_5ghz[IWN_MAX_PWR_INDEX + 1] = {
	0x7b, 0x75, 0x6e, 0x68, 0x62, 0x6e, 0x68, 0x62, 0x6e, 0x68, 0x62,
	0x6e, 0x68, 0x62, 0x6e, 0x68, 0x62, 0x6e, 0x68, 0x62, 0x6e, 0x68,
	0x62, 0x6e, 0x68, 0x62, 0x6e, 0x68, 0x62, 0x6e, 0x68, 0x62, 0x6e,
	0x68, 0x62, 0x6e, 0x68, 0x62, 0x6e, 0x68, 0x62, 0x6e, 0x68, 0x62,
	0x6e, 0x68, 0x62, 0x6e, 0x68, 0x62, 0x6e, 0x68, 0x62, 0x6e, 0x68,
	0x62, 0x6e, 0x68, 0x62, 0x6e, 0x68, 0x62, 0x6e, 0x68, 0x62, 0x6e,
	0x68, 0x62, 0x6e, 0x68, 0x62, 0x6e, 0x68, 0x62, 0x6e, 0x68, 0x62,
	0x6e, 0x68, 0x62, 0x6e, 0x68, 0x62, 0x6e, 0x68, 0x62, 0x6e, 0x68,
	0x62, 0x6e, 0x68, 0x62, 0x6e, 0x68, 0x62, 0x6e, 0x68, 0x62, 0x6e,
	0x68, 0x62, 0x6e, 0x68, 0x62, 0x5d, 0x58, 0x53, 0x4e
};

#define IWN_READ(sc, reg)						\
	bus_space_read_4((sc)->sc_st, (sc)->sc_sh, (reg))

#define IWN_WRITE(sc, reg, val)						\
	bus_space_write_4((sc)->sc_st, (sc)->sc_sh, (reg), (val))

#define IWN_WRITE_REGION_4(sc, offset, datap, count)			\
	bus_space_write_region_4((sc)->sc_st, (sc)->sc_sh, (offset),	\
	    (datap), (count))
