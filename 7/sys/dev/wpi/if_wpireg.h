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

#define WPI_TX_RING_COUNT	256
#define WPI_CMD_RING_COUNT	256
#define WPI_RX_RING_COUNT	64

/*
 * Rings must be aligned on a 16K boundary.
 */
#define WPI_RING_DMA_ALIGN	0x4000

/* maximum scatter/gather */
#define WPI_MAX_SCATTER	4

/* maximum Rx buffer size */
#define WPI_RBUF_SIZE ( 3 * 1024 ) /* XXX 3000 but must be aligned */

/*
 * Control and status registers.
 */
#define WPI_HWCONFIG		0x000
#define WPI_INTR		0x008
#define WPI_MASK		0x00c
#define WPI_INTR_STATUS		0x010
#define WPI_GPIO_STATUS		0x018
#define WPI_RESET		0x020
#define WPI_GPIO_CTL		0x024
#define WPI_EEPROM_CTL		0x02c
#define WPI_EEPROM_STATUS	0x030
#define WPI_UCODE_SET		0x058
#define WPI_UCODE_CLR		0x05c
#define WPI_TEMPERATURE		0x060
#define WPI_CHICKEN		0x100
#define WPI_PLL_CTL		0x20c
#define WPI_WRITE_MEM_ADDR	0x444
#define WPI_READ_MEM_ADDR	0x448
#define WPI_WRITE_MEM_DATA	0x44c
#define WPI_READ_MEM_DATA	0x450
#define WPI_TX_WIDX		0x460
#define WPI_TX_CTL(qid)		(0x940 + (qid) * 8)
#define WPI_TX_BASE(qid)	(0x944 + (qid) * 8)
#define WPI_TX_DESC(qid)	(0x980 + (qid) * 80)
#define WPI_RX_CONFIG		0xc00
#define WPI_RX_BASE		0xc04
#define WPI_RX_WIDX		0xc20
#define WPI_RX_RIDX_PTR		0xc24
#define WPI_RX_CTL		0xcc0
#define WPI_RX_STATUS		0xcc4
#define WPI_TX_CONFIG(qid)	(0xd00 + (qid) * 32)
#define WPI_TX_CREDIT(qid)	(0xd04 + (qid) * 32)
#define WPI_TX_STATE(qid)	(0xd08 + (qid) * 32)
#define WPI_TX_BASE_PTR		0xe80
#define WPI_MSG_CONFIG		0xe88
#define WPI_TX_STATUS		0xe90


/*
 * NIC internal memory offsets.
 */
#define WPI_MEM_MODE		0x2e00
#define WPI_MEM_RA		0x2e04
#define WPI_MEM_TXCFG		0x2e10
#define WPI_MEM_MAGIC4		0x2e14
#define WPI_MEM_MAGIC5		0x2e20
#define WPI_MEM_BYPASS1		0x2e2c
#define WPI_MEM_BYPASS2		0x2e30
#define WPI_MEM_CLOCK1		0x3004
#define WPI_MEM_CLOCK2		0x3008
#define WPI_MEM_POWER		0x300c
#define WPI_MEM_PCIDEV		0x3010
#define WPI_MEM_HW_RADIO_OFF	0x3014
#define WPI_MEM_UCODE_CTL	0x3400
#define WPI_MEM_UCODE_SRC	0x3404
#define WPI_MEM_UCODE_DST	0x3408
#define WPI_MEM_UCODE_SIZE	0x340c
#define WPI_MEM_UCODE_BASE	0x3800

#define WPI_MEM_TEXT_BASE	0x3490
#define WPI_MEM_TEXT_SIZE	0x3494
#define WPI_MEM_DATA_BASE	0x3498
#define WPI_MEM_DATA_SIZE	0x349c


/* possible flags for register WPI_HWCONFIG */
#define WPI_HW_ALM_MB	(1 << 8)
#define WPI_HW_ALM_MM	(1 << 9)
#define WPI_HW_SKU_MRC	(1 << 10)
#define WPI_HW_REV_D	(1 << 11)
#define WPI_HW_TYPE_B	(1 << 12)

/* possible flags for registers WPI_READ_MEM_ADDR/WPI_WRITE_MEM_ADDR */
#define WPI_MEM_4	((sizeof (uint32_t) - 1) << 24)

/* possible values for WPI_MEM_UCODE_DST */
#define WPI_FW_TEXT	0x00000000

/* possible flags for WPI_GPIO_STATUS */
#define WPI_POWERED		(1 << 9)

/* possible flags for register WPI_RESET */
#define WPI_NEVO_RESET		(1 << 0)
#define WPI_SW_RESET		(1 << 7)
#define WPI_MASTER_DISABLED	(1 << 8)
#define WPI_STOP_MASTER		(1 << 9)

/* possible flags for register WPI_GPIO_CTL */
#define WPI_GPIO_CLOCK		(1 << 0)
#define WPI_GPIO_INIT		(1 << 2)
#define WPI_GPIO_MAC		(1 << 3)
#define WPI_GPIO_SLEEP		(1 << 4)
#define WPI_GPIO_PWR_STATUS	0x07000000
#define WPI_GPIO_PWR_SLEEP	(4 << 24)

/* possible flags for register WPI_CHICKEN */
#define WPI_CHICKEN_RXNOLOS	(1 << 23)

/* possible flags for register WPI_PLL_CTL */
#define WPI_PLL_INIT		(1 << 24)

/* possible flags for register WPI_UCODE_CLR */
#define WPI_RADIO_OFF		(1 << 1)
#define WPI_DISABLE_CMD		(1 << 2)

/* possible flags for WPI_RX_STATUS */
#define	WPI_RX_IDLE	(1 << 24)

/* possible flags for register WPI_UC_CTL */
#define WPI_UC_ENABLE	(1 << 30)
#define WPI_UC_RUN	(1 << 31)

/* possible flags for register WPI_INTR_CSR */
#define WPI_ALIVE_INTR	(1 << 0)
#define WPI_WAKEUP_INTR	(1 << 1)
#define WPI_SW_ERROR	(1 << 25)
#define WPI_TX_INTR	(1 << 27)
#define WPI_HW_ERROR	(1 << 29)
#define WPI_RX_INTR	(1 << 31)

#define WPI_INTR_MASK							\
	(WPI_SW_ERROR | WPI_HW_ERROR | WPI_TX_INTR | WPI_RX_INTR |	\
	 WPI_ALIVE_INTR | WPI_WAKEUP_INTR)

/* possible flags for register WPI_TX_STATUS */
#define WPI_TX_IDLE(qid)	(1 << ((qid) + 24) | 1 << ((qid) + 16))

/* possible flags for register WPI_EEPROM_CTL */
#define WPI_EEPROM_READY	(1 << 0)

/* possible flags for register WPI_EEPROM_STATUS */
#define WPI_EEPROM_VERSION	0x00000007
#define WPI_EEPROM_LOCKED	0x00000180


struct wpi_shared {
	uint32_t	txbase[8];
	uint32_t	next;
	uint32_t	reserved[2];
} __packed;

#define WPI_MAX_SEG_LEN	65520
struct wpi_tx_desc {
	uint32_t	flags;
#define WPI_PAD32(x)	(roundup2(x, 4) - (x))

	struct {
		uint32_t	addr;
		uint32_t	len;
	} __attribute__((__packed__))	segs[WPI_MAX_SCATTER];
	uint8_t		reserved[28];
} __packed;

struct wpi_tx_stat {
	uint8_t		nrts;
	uint8_t		ntries;
	uint8_t		nkill;
	uint8_t		rate;
	uint32_t	duration;
	uint32_t	status;
} __packed;

struct wpi_rx_desc {
	uint32_t	len;
	uint8_t		type;
#define WPI_UC_READY		  1
#define WPI_RX_DONE		 27
#define WPI_TX_DONE		 28
#define WPI_START_SCAN		130
#define WPI_SCAN_RESULTS	131
#define WPI_STOP_SCAN		132
#define WPI_STATE_CHANGED	161
#define WPI_MISSED_BEACON	162

	uint8_t		flags;
	uint8_t		idx;
	uint8_t		qid;
} __packed;

struct wpi_rx_stat {
	uint8_t		len;
#define WPI_STAT_MAXLEN	20

	uint8_t		id;
	uint8_t		rssi;	/* received signal strength */
#define WPI_RSSI_OFFSET	95

	uint8_t		agc;	/* access gain control */
	uint16_t	signal;
	uint16_t	noise;
} __packed;

struct wpi_rx_head {
	uint16_t	chan;
	uint16_t	flags;
	uint8_t		reserved;
	uint8_t		rate;
	uint16_t	len;
} __packed;

struct wpi_rx_tail {
	uint32_t	flags;
#if 0
#define WPI_RX_NO_CRC_ERR	(1 << 0)
#define WPI_RX_NO_OVFL_ERR	(1 << 1)
/* shortcut for the above */
#define WPI_RX_NOERROR		(WPI_RX_NO_CRC_ERR | WPI_RX_NO_OVFL_ERR)
#endif
	uint64_t	tstamp;
	uint32_t	tbeacon;
} __packed;

struct wpi_tx_cmd {
	uint8_t	code;
#define WPI_CMD_CONFIGURE	 16
#define WPI_CMD_ASSOCIATE	 17
#define WPI_CMD_SET_WME          19
#define WPI_CMD_TSF		 20
#define WPI_CMD_ADD_NODE	 24
#define WPI_CMD_TX_DATA		 28
#define WPI_CMD_MRR_SETUP	 71
#define WPI_CMD_SET_LED		 72
#define WPI_CMD_SET_POWER_MODE	119
#define WPI_CMD_SCAN		128
#define WPI_CMD_SET_BEACON	145
#define WPI_CMD_TXPOWER		151
#define WPI_CMD_BLUETOOTH	155

	uint8_t	flags;
	uint8_t	idx;
	uint8_t	qid;
	uint8_t	data[360];
} __packed;

/* structure for WPI_CMD_CONFIGURE */
struct wpi_config {
	uint8_t		myaddr[IEEE80211_ADDR_LEN];
	uint16_t	reserved1;
	uint8_t		bssid[IEEE80211_ADDR_LEN];
	uint16_t	reserved2;
	uint8_t		wlap_bssid_addr[6];
	uint16_t	reserved3;
	uint8_t		mode;
#define WPI_MODE_HOSTAP		1
#define WPI_MODE_STA		3
#define WPI_MODE_IBSS		4
#define WPI_MODE_MONITOR	6

	uint8_t		air_propogation;
	uint16_t	reserved4;
	uint8_t		ofdm_mask;
	uint8_t		cck_mask;
	uint16_t	associd;
	uint32_t	flags;
#define WPI_CONFIG_24GHZ	(1 << 0)
#define WPI_CONFIG_CCK		(1 << 1)
#define WPI_CONFIG_AUTO		(1 << 2)
#define WPI_CONFIG_SHSLOT	(1 << 4)
#define WPI_CONFIG_SHPREAMBLE	(1 << 5)
#define WPI_CONFIG_NODIVERSITY	(1 << 7)
#define WPI_CONFIG_ANTENNA_A	(1 << 8)
#define WPI_CONFIG_ANTENNA_B	(1 << 9)
#define WPI_CONFIG_TSF		(1 << 15)

	uint32_t	filter;
#define WPI_FILTER_PROMISC	(1 << 0)
#define WPI_FILTER_CTL		(1 << 1)
#define WPI_FILTER_MULTICAST	(1 << 2)
#define WPI_FILTER_NODECRYPT	(1 << 3)
#define WPI_FILTER_BSS		(1 << 5)
#define WPI_FILTER_BEACON	(1 << 6)

	uint8_t		chan;
	uint16_t	reserved6;
} __packed;

/* structure for command WPI_CMD_ASSOCIATE */
struct wpi_assoc {
	uint32_t	flags;
	uint32_t	filter;
	uint8_t		ofdm_mask;
	uint8_t		cck_mask;
	uint16_t	reserved;
} __packed;

/* structure for command WPI_CMD_SET_WME */
struct wpi_wme_setup {
	uint32_t	flags;
	struct {
		uint16_t	cwmin;
		uint16_t	cwmax;
		uint8_t		aifsn;
		uint8_t		reserved;
		uint16_t	txop;
	} __packed	ac[WME_NUM_AC];
} __packed;

/* structure for command WPI_CMD_TSF */
struct wpi_cmd_tsf {
	uint64_t	tstamp;
	uint16_t	bintval;
	uint16_t	atim;
	uint32_t	binitval;
	uint16_t	lintval;
	uint16_t	reserved;
} __packed;

/* structure for WPI_CMD_ADD_NODE */
struct wpi_node_info {
	uint8_t		control;
#define WPI_NODE_UPDATE	(1 << 0)

	uint8_t		reserved1[3];
	uint8_t		bssid[IEEE80211_ADDR_LEN];
	uint16_t	reserved2;
	uint8_t		id;
#define WPI_ID_BSS		0
#define WPI_ID_BROADCAST	24

	uint8_t		flags;
	uint16_t	reserved3;
	uint16_t	key_flags;
	uint8_t		tkip;
	uint8_t		reserved4;
	uint16_t	ttak[5];
	uint16_t	reserved5;
	uint8_t		key[IEEE80211_KEYBUF_SIZE];
	uint32_t	action;
#define WPI_ACTION_SET_RATE	4
	uint32_t	mask;
	uint16_t	tid;
	uint8_t		rate;
	uint8_t		antenna;
#define WPI_ANTENNA_A	(1<<6)
#define WPI_ANTENNA_B	(1<<7)
#define WPI_ANTENNA_BOTH	(WPI_ANTENNA_A|WPI_ANTENNA_B)
	uint8_t		add_imm;
	uint8_t		del_imm;
	uint16_t	add_imm_start;
} __packed;

/* structure for command WPI_CMD_TX_DATA */
struct wpi_cmd_data {
	uint16_t	len;
	uint16_t	lnext;
	uint32_t	flags;
#define WPI_TX_NEED_RTS		(1 <<  1)
#define WPI_TX_NEED_CTS         (1 <<  2)
#define WPI_TX_NEED_ACK		(1 <<  3)
#define WPI_TX_FULL_TXOP	(1 <<  7)
#define WPI_TX_BT_DISABLE	(1 << 12) /* bluetooth coexistence */
#define WPI_TX_AUTO_SEQ		(1 << 13)
#define WPI_TX_INSERT_TSTAMP	(1 << 16)

	uint8_t		rate;
	uint8_t		id;
	uint8_t		tid;
	uint8_t		security;
	uint8_t		key[IEEE80211_KEYBUF_SIZE];
	uint8_t		tkip[IEEE80211_WEP_MICLEN];
	uint32_t	fnext;
	uint32_t	lifetime;
#define WPI_LIFETIME_INFINITE	0xffffffff
	uint8_t		ofdm_mask;
	uint8_t		cck_mask;
	uint8_t		rts_ntries;
	uint8_t		data_ntries;
	uint16_t	timeout;
	uint16_t	txop;
	struct          ieee80211_frame wh;
} __packed;

/* structure for command WPI_CMD_SET_BEACON */
struct wpi_cmd_beacon {
	uint16_t	len;
	uint16_t	reserved1;
	uint32_t	flags;	/* same as wpi_cmd_data */
	uint8_t		rate;
	uint8_t		id;
	uint8_t		reserved2[30];
	uint32_t	lifetime;
	uint8_t		ofdm_mask;
	uint8_t		cck_mask;
	uint16_t	reserved3[3];
	uint16_t	tim;
	uint8_t		timsz;
	uint8_t		reserved4;
	struct		ieee80211_frame wh;
} __packed;

/* structure for notification WPI_MISSED_BEACON */
struct wpi_missed_beacon {
    uint32_t consecutive;
    uint32_t total;
    uint32_t expected;
    uint32_t received;
} __packed;


/* structure for WPI_CMD_MRR_SETUP */
struct wpi_mrr_setup {
	uint8_t	which;
#define WPI_MRR_CTL	0
#define WPI_MRR_DATA	1

	uint8_t		reserved[3];

	struct {
		uint8_t	signal;
		uint8_t	flags;
		uint8_t	ntries;
		uint8_t	next;
#define WPI_OFDM6	0
#define WPI_OFDM54	7
#define WPI_CCK1	8
#define WPI_CCK2	9
#define WPI_CCK11	11

	} __attribute__((__packed__))	rates[WPI_CCK11 + 1];
} __packed;

/* structure for WPI_CMD_SET_LED */
struct wpi_cmd_led {
	uint32_t	unit;	/* multiplier (in usecs) */
	uint8_t		which;
#define WPI_LED_ACTIVITY	1
#define WPI_LED_LINK		2

	uint8_t		off;
	uint8_t		on;
	uint8_t		reserved;
} __packed;

/* structure for WPI_CMD_SET_POWER_MODE */
struct wpi_power {
	uint32_t	flags;
#define WPI_POWER_CAM	0	/* constantly awake mode */
	uint32_t	rx_timeout;
	uint32_t	tx_timeout;
	uint32_t	sleep[5];
} __packed;

/* structure for command WPI_CMD_SCAN */
struct wpi_scan_hdr {
	uint16_t	len;
	uint8_t		reserved1;
	uint8_t		nchan;
	uint16_t	quiet;
	uint16_t	threshold;
	uint16_t	promotion;
	uint16_t	reserved2;
	uint32_t	maxtimeout;
	uint32_t	suspend;
	uint32_t	flags;
	uint32_t	filter;

struct {
	uint16_t	len;
	uint16_t	lnext;
	uint32_t	flags;
	uint8_t		rate;
	uint8_t		id;
	uint8_t		tid;
	uint8_t		security;
	uint8_t		key[IEEE80211_KEYBUF_SIZE];
	uint8_t		tkip[IEEE80211_WEP_MICLEN];
	uint32_t	fnext;
	uint32_t	lifetime;
	uint8_t		ofdm_mask;
	uint8_t		cck_mask;
	uint8_t		rts_ntries;
	uint8_t		data_ntries;
	uint16_t	timeout;
	uint16_t	txop;
}	tx __attribute__((__packed__));

#define WPI_SCAN_MAX_ESSIDS	4
	struct {
	    uint8_t		id;
	    uint8_t		esslen;
	    uint8_t		essid[32];
	}scan_essids[WPI_SCAN_MAX_ESSIDS];
	/* followed by probe request body */
	/* followed by nchan x wpi_scan_chan */
} __packed;

struct wpi_scan_chan {
	uint8_t		flags;
	uint8_t		chan;
#define WPI_CHAN_ACTIVE	(1 << 0)
#define WPI_CHAN_DIRECT (1 << 1)
	uint8_t		gain_radio;
	uint8_t		gain_dsp;
	uint16_t	active;		/* msecs */
	uint16_t	passive;	/* msecs */
} __packed;

/* structure for WPI_CMD_BLUETOOTH */
struct wpi_bluetooth {
	uint8_t		flags;
	uint8_t		lead;
	uint8_t		kill;
	uint8_t		reserved;
	uint32_t	ack;
	uint32_t	cts;
} __packed;

/* structure for command WPI_CMD_TXPOWER */
struct wpi_cmd_txpower {

	uint8_t		band;
#define WPI_RATE_5GHZ	0
#define WPI_RATE_2GHZ	1
	uint8_t		reserved;
	uint16_t	channel;

#define WPI_RATE_MAPPING_COUNT 12
	struct {
	    uint8_t	rate;
	    uint8_t	gain_radio;
	    uint8_t	gain_dsp;
	    uint8_t	reserved;
	} __packed rates [WPI_RATE_MAPPING_COUNT];

} __packed;



#define WPI_FW_MAIN_TEXT_MAXSZ (80 * 1024 )
#define WPI_FW_MAIN_DATA_MAXSZ (32 * 1024 )
#define WPI_FW_INIT_TEXT_MAXSZ (80 * 1024 )
#define WPI_FW_INIT_DATA_MAXSZ (32 * 1024 )
#define WPI_FW_BOOT_TEXT_MAXSZ 1024

#define WPI_FW_UPDATED	(1 << 31 )

/* firmware image header */
struct wpi_firmware_hdr {

#define WPI_FW_MINVERSION 2144

	uint32_t	version;
	uint32_t	rtextsz;
	uint32_t	rdatasz;
	uint32_t	itextsz;
	uint32_t	idatasz;
	uint32_t	btextsz;
} __packed;

/* structure for WPI_UC_READY notification */
struct wpi_ucode_info {
	uint32_t	version;
	uint8_t		revision[8];
	uint8_t		type;
	uint8_t		subtype;
	uint16_t	reserved;
	uint32_t	logptr;
	uint32_t	errorptr;
	uint32_t	timestamp;
	uint32_t	valid;
} __packed;

/* structure for WPI_START_SCAN notification */
struct wpi_start_scan {
	uint64_t	tstamp;
	uint32_t	tbeacon;
	uint8_t		chan;
	uint8_t		band;
	uint16_t	reserved;
	uint32_t	status;
} __packed;

/* structure for WPI_STOP_SCAN notification */
struct wpi_stop_scan {
	uint8_t		nchan;
	uint8_t		status;
	uint8_t		reserved;
	uint8_t		chan;
	uint64_t	tsf;
} __packed;

#define WPI_EEPROM_MAC		0x015
#define WPI_EEPROM_REVISION	0x035
#define WPI_EEPROM_CAPABILITIES	0x045
#define WPI_EEPROM_TYPE		0x04a
#define WPI_EEPROM_DOMAIN	0x060
#define WPI_EEPROM_BAND1	0x063
#define WPI_EEPROM_BAND2	0x072
#define WPI_EEPROM_BAND3	0x080
#define WPI_EEPROM_BAND4	0x08d
#define WPI_EEPROM_BAND5	0x099
#define WPI_EEPROM_POWER_GRP	0x100

struct wpi_eeprom_chan {
	uint8_t	flags;
#define WPI_EEPROM_CHAN_VALID	(1<<0)
#define	WPI_EEPROM_CHAN_IBSS	(1<<1)
#define WPI_EEPROM_CHAN_ACTIVE	(1<<3)
#define WPI_EEPROM_CHAN_RADAR	(1<<4)

	int8_t	maxpwr;
} __packed;

struct wpi_eeprom_sample {
    uint8_t	index;
    int8_t	power;
    uint16_t	volt;
};

#define WPI_POWER_GROUPS_COUNT	5

struct wpi_eeprom_group {
    struct	wpi_eeprom_sample samples[5];
    int32_t	coef[5];
    int32_t	corr[5];
    int8_t	maxpwr;
    uint8_t	chan;
    int16_t	temp;
} __packed;

#define WPI_CHAN_BANDS_COUNT	5
#define WPI_MAX_CHAN_PER_BAND	14

static const struct wpi_chan_band {
    uint32_t	addr;	/* offset in EEPROM */
    uint8_t	nchan;
    uint8_t	chan[WPI_MAX_CHAN_PER_BAND];
} wpi_bands[5] = {
    { WPI_EEPROM_BAND1, 14,
	{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14 }},
    { WPI_EEPROM_BAND2, 13,
	{ 183, 184, 185, 187, 188, 189, 192, 196, 7, 8, 11, 12, 16 }},
    { WPI_EEPROM_BAND3, 12,
	{ 34, 36, 38, 40, 42, 44, 46, 48, 52, 56, 60, 64 }},
    { WPI_EEPROM_BAND4, 11,
	{ 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140 }},
    { WPI_EEPROM_BAND5, 6,
	{ 145, 149, 153, 157, 161, 165 }}
};

#define WPI_MAX_PWR_INDEX	77

/*
 * RF Tx gain values from highest to lowest power (values obtained from
 * the reference driver.)
 */
static const uint8_t wpi_rf_gain_2ghz[WPI_MAX_PWR_INDEX + 1] = {
    0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xbb, 0xbb, 0xbb,
    0xbb, 0xf3, 0xf3, 0xf3, 0xf3, 0xf3, 0xd3, 0xd3, 0xb3, 0xb3, 0xb3,
    0x93, 0x93, 0x93, 0x93, 0x93, 0x93, 0x93, 0x73, 0xeb, 0xeb, 0xeb,
    0xcb, 0xcb, 0xcb, 0xcb, 0xcb, 0xcb, 0xcb, 0xab, 0xab, 0xab, 0x8b,
    0xe3, 0xe3, 0xe3, 0xe3, 0xe3, 0xe3, 0xc3, 0xc3, 0xc3, 0xc3, 0xa3,
    0xa3, 0xa3, 0xa3, 0x83, 0x83, 0x83, 0x83, 0x63, 0x63, 0x63, 0x63,
    0x43, 0x43, 0x43, 0x43, 0x23, 0x23, 0x23, 0x23, 0x03, 0x03, 0x03,
    0x03
};

static const uint8_t wpi_rf_gain_5ghz[WPI_MAX_PWR_INDEX + 1] = {
    0xfb, 0xfb, 0xfb, 0xdb, 0xdb, 0xbb, 0xbb, 0x9b, 0x9b, 0x7b, 0x7b,
    0x7b, 0x7b, 0x5b, 0x3b, 0x3b, 0x3b, 0x3b, 0x3b, 0x3b, 0x1b, 0x1b,
    0x1b, 0x73, 0x73, 0x73, 0x53, 0x53, 0x53, 0x53, 0x53, 0x33, 0x33,
    0x33, 0x33, 0x13, 0x13, 0x13, 0x13, 0x13, 0xab, 0xab, 0xab, 0x8b,
    0x8b, 0x8b, 0x8b, 0x6b, 0x6b, 0x6b, 0x6b, 0x4b, 0x4b, 0x4b, 0x4b,
    0x2b, 0x2b, 0x2b, 0x2b, 0x0b, 0x0b, 0x0b, 0x0b, 0x83, 0x83, 0x63,
    0x63, 0x63, 0x63, 0x43, 0x43, 0x43, 0x43, 0x23, 0x23, 0x23, 0x23,
    0x03
};

/*
 * DSP pre-DAC gain values from highest to lowest power (values obtained
 * from the reference driver.)
 */
static const uint8_t wpi_dsp_gain_2ghz[WPI_MAX_PWR_INDEX + 1] = {
    0x7f, 0x7f, 0x7f, 0x7f, 0x7d, 0x6e, 0x69, 0x62, 0x7d, 0x73, 0x6c,
    0x63, 0x77, 0x6f, 0x69, 0x61, 0x5c, 0x6a, 0x64, 0x78, 0x71, 0x6b,
    0x7d, 0x77, 0x70, 0x6a, 0x65, 0x61, 0x5b, 0x6b, 0x79, 0x73, 0x6d,
    0x7f, 0x79, 0x73, 0x6c, 0x66, 0x60, 0x5c, 0x6e, 0x68, 0x62, 0x74,
    0x7d, 0x77, 0x71, 0x6b, 0x65, 0x60, 0x71, 0x6a, 0x66, 0x5f, 0x71,
    0x6a, 0x66, 0x5f, 0x71, 0x6a, 0x66, 0x5f, 0x71, 0x6a, 0x66, 0x5f,
    0x71, 0x6a, 0x66, 0x5f, 0x71, 0x6a, 0x66, 0x5f, 0x71, 0x6a, 0x66,
    0x5f
};

static const uint8_t wpi_dsp_gain_5ghz[WPI_MAX_PWR_INDEX + 1] = {
    0x7f, 0x78, 0x72, 0x77, 0x65, 0x71, 0x66, 0x72, 0x67, 0x75, 0x6b,
    0x63, 0x5c, 0x6c, 0x7d, 0x76, 0x6d, 0x66, 0x60, 0x5a, 0x68, 0x62,
    0x5c, 0x76, 0x6f, 0x68, 0x7e, 0x79, 0x71, 0x69, 0x63, 0x76, 0x6f,
    0x68, 0x62, 0x74, 0x6d, 0x66, 0x62, 0x5d, 0x71, 0x6b, 0x63, 0x78,
    0x71, 0x6b, 0x63, 0x78, 0x71, 0x6b, 0x63, 0x78, 0x71, 0x6b, 0x63,
    0x78, 0x71, 0x6b, 0x63, 0x78, 0x71, 0x6b, 0x63, 0x6b, 0x63, 0x78,
    0x71, 0x6b, 0x63, 0x78, 0x71, 0x6b, 0x63, 0x78, 0x71, 0x6b, 0x63,
    0x78
};


#define WPI_READ(sc, reg)						\
    bus_space_read_4((sc)->sc_st, (sc)->sc_sh, (reg))

#define WPI_WRITE(sc, reg, val)						\
    bus_space_write_4((sc)->sc_st, (sc)->sc_sh, (reg), (val))

#define WPI_WRITE_REGION_4(sc, offset, datap, count)			\
    bus_space_write_region_4((sc)->sc_st, (sc)->sc_sh, (offset),	\
			     (datap), (count))
