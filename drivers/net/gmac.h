/*
 * Definitions for the GMAC ethernet chip, used in the
 * Apple G4 powermac.
 */


/* 
 * GMAC register definitions
 * 
 * Note: We encode the register size the same way Apple does. I didn't copy
 *       Apple's source as-is to avoid licence issues however. That's really
 *       painful to re-define all those registers ...
 *       The constants themselves were partially found in OF code, in Sun
 *       GEM driver and in Apple's Darwin GMAC driver
 */

#define REG_SZ_8			0x00000000
#define REG_SZ_16			0x40000000
#define REG_SZ_32			0x80000000
#define REG_MASK			0x0FFFFFFF

	/*
	 * Global registers
	 */

/* -- 0x0004	RW	Global configuration
 * d: 0x00000042
 */
#define GM_GCONF			(0x0004 | REG_SZ_16)
#define GM_GCONF_BURST_SZ		0x0001		/* 1: 64 bytes/burst, 0: infinite */
#define GM_GCONF_TXDMA_LIMIT_MASK	0x003e		/* 5-1: No of 64 bytes transfers */
#define GM_GCONF_TXDMA_LIMIT_SHIFT	1
#define GM_GCONF_RXDMA_LIMIT_MASK	0x07c0		/* 10-6: No of 64 bytes transfers */
#define GM_GCONF_RXDMA_LIMIT_SHIFT	6

/* -- 0x000C	R-C	Global Interrupt status. 
 * d: 0x00000000	bits 0-6 cleared on read (C)
 */
#define GM_IRQ_STATUS			(0x000c | REG_SZ_32)
#define GM_IRQ_TX_INT_ME		0x00000001	/* C Frame with INT_ME bit set in fifo */
#define GM_IRQ_TX_ALL			0x00000002	/* C TX descriptor ring empty */
#define GM_IRQ_TX_DONE			0x00000004	/* C moved from host to TX fifo */
#define GM_IRQ_RX_DONE			0x00000010	/* C moved from RX fifo to host */
#define GM_IRQ_RX_NO_BUF		0x00000020	/* C No RX buffer available */
#define GM_IRQ_RX_TAG_ERR		0x00000040	/* C RX tag error */
#define GM_IRQ_PCS			0x00002000	/* PCS interrupt ? */
#define GM_IRQ_MAC_TX			0x00004000	/* MAC tx register set */
#define GM_IRQ_MAC_RX			0x00008000	/* MAC rx register set  */
#define GM_IRQ_MAC_CTRL			0x00010000	/* MAC control register set  */
#define GM_IRQ_MIF			0x00020000	/* MIF status register set */
#define GM_IRQ_BUS_ERROR		0x00040000	/* Bus error status register set */
#define GM_IRQ_TX_COMP			0xfff80000	/* TX completion mask */
	
/* -- 0x0010	RW	Interrupt mask. 
 * d: 0xFFFFFFFF
 */
#define GM_IRQ_MASK			(0x0010 | REG_SZ_32)

/* -- 0x0014	WO	Interrupt ack.
 * 			Ack. "high" interrupts
 */
#define GM_IRQ_ACK			(0x0014 | REG_SZ_32)

/* -- 0x001C	WO	Alias of status register (no auto-clear of "low" interrupts)
 */
#define GM_IRQ_ALT_STAT			(0x001C | REG_SZ_32)

/* -- 0x1000	R-C	PCI Error status register
 */
#define GM_PCI_ERR_STAT			(0x1000 | REG_SZ_8)
#define GM_PCI_ERR_BAD_ACK		0x01		/* Bad Ack64 */
#define GM_PCI_ERR_TIMEOUT		0x02		/* Transaction timeout */
#define GM_PCI_ERR_OTHER		0x04		/* Any other PCI error */
	
/* -- 0x1004	RW	PCI Error mask register
 * d: 0xFFFFFFFF
 */
#define GM_PCI_ERR_MASK			(0x1004 | REG_SZ_8)

/* -- 0x1008	RW	BIF Configuration
 * d: 0x00000000
 */
#define GM_BIF_CFG			(0x1008 | REG_SZ_8)
#define	GM_BIF_CFG_SLOWCLK		0x01		/* for parity error timing */
#define	GM_BIF_CFG_HOST_64		0x02		/* 64-bit host */
#define	GM_BIF_CFG_B64D_DIS		0x04		/* no 64-bit wide data cycle */
#define	GM_BIF_CFG_M66EN		0x08		/* Read-only: sense if configured for 66MHz */
	
/* -- 0x100C	RW	BIF Diagnostic ???
 */
#define GM_BIF_DIAG			(0x100C | REG_SZ_32)
#define GM_BIF_DIAG_BURST_STATE		0x007F0000
#define GM_BIF_DIAG_STATE_MACH		0xFF000000

/* -- 0x1010	RW	Software reset
 *			Lower two bits reset TX and RX, both reset whole gmac. They come back
 *			to 0 when reset is complete.
 *			bit 2 force RSTOUT# pin when set (PHY reset)
 */
#define GM_RESET			(0x1010 | REG_SZ_8)
#define	GM_RESET_TX			0x01
#define	GM_RESET_RX			0x02
#define	GM_RESET_RSTOUT			0x04		/* PHY reset */


	/*
	 * Tx DMA Registers
	 */

/* -- 0x2000	RW	Tx Kick
 * d: 0x00000000	Written by the host with the last tx descriptor number +1 to send
 */
#define GM_TX_KICK			(0x2000 | REG_SZ_16)

/* -- 0x2004	RW	Tx configuration
 * d: 0x118010		Controls operation of Tx DMA channel
 */

#define GM_TX_CONF			(0x2004 | REG_SZ_32)
#define	GM_TX_CONF_DMA_EN		0x00000001	/* Tx DMA enable */
#define	GM_TX_CONF_RING_SZ_MASK		0x0000001e	/* Tx desc ring size */
#define GM_TX_CONF_RING_SZ_SHIFT	1		/* Tx desc ring size shift */
#define GM_TX_CONF_FIFO_PIO		0x00000020	/* Tx fifo PIO select ??? */
#define	GM_TX_CONF_FIFO_THR_MASK	0x001ffc00	/* Tx fifo threshold */
#define GM_TX_CONF_FIFO_THR_SHIFT	10		/* Tx fifo threshold shift */
#define GM_TX_CONF_FIFO_THR_DEFAULT	0x7ff		/* Tx fifo threshold default */
#define	GM_TX_CONF_PACED_MODE		0x00100000	/* 1: tx_all irq after last descriptor */
							/* 0: tx_all irq when tx fifo empty */
#define	GM_TX_RING_SZ_32		(0 << 1)
#define	GM_TX_RING_SZ_64		(1 << 1)
#define	GM_TX_RING_SZ_128		(2 << 1)
#define	GM_TX_RING_SZ_256		(3 << 1)
#define	GM_TX_RING_SZ_512		(4 << 1)
#define	GM_TX_RING_SZ_1024		(5 << 1)
#define	GM_TX_RING_SZ_2048		(6 << 1)
#define	GM_TX_RING_SZ_4086		(7 << 1)
#define	GM_TX_RING_SZ_8192		(8 << 1)

/* -- 0x2008	RW	Tx descriptor ring base low
 * -- 0x200C	RW	Tx descriptor ring base high
 *
 * Base of tx ring, must be 2k aligned
 */
#define GM_TX_DESC_LO			(0x2008 | REG_SZ_32)
#define GM_TX_DESC_HI			(0x200C | REG_SZ_32)
 
/* -- 0x2100	RW	Tx Completion
 * d: 0x00000000	Written by the gmac with the last tx descriptor number +1 sent
 */
#define GM_TX_COMP			(0x2100 | REG_SZ_16)


	/*
	 * Rx DMA registers
	 */


/* -- 0x4000	RW	Rx configuration
 * d: 0x1000010		Controls operation of Rx DMA channel
 */

#define GM_RX_CONF			(0x4000 | REG_SZ_32)
#define	GM_RX_CONF_DMA_EN		0x00000001	/* Rx DMA enable */
#define	GM_RX_CONF_RING_SZ_MASK		0x0000001e	/* Rx desc ring size */
#define GM_RX_CONF_RING_SZ_SHIFT	1
#define	GM_RX_CONF_BATCH_DIS		0x00000020	/* Rx batch disable */
#define	GM_RX_CONF_FBYTE_OFF_MASK	0x00001c00	/* First byte offset (10-12) */
#define GM_RX_CONF_FBYTE_OFF_SHIFT	10
#define	GM_RX_CONF_CHK_START_MASK	0x000FE000	/* Checksum start offset */
#define GM_RX_CONF_CHK_START_SHIFT	13
#define	GM_RX_CONF_DMA_THR_MASK		0x07000000	/* Rx DMA threshold */
#define GM_RX_CONF_DMA_THR_SHIFT	24		/* Rx DMA threshold shift */
#define GM_RX_CONF_DMA_THR_DEFAULT	1		/* Rx DMA threshold default */

#define	GM_RX_RING_SZ_32		(0 << 1)
#define	GM_RX_RING_SZ_64		(1 << 1)
#define	GM_RX_RING_SZ_128		(2 << 1)
#define	GM_RX_RING_SZ_256		(3 << 1)
#define	GM_RX_RING_SZ_512		(4 << 1)
#define	GM_RX_RING_SZ_1024		(5 << 1)
#define	GM_RX_RING_SZ_2048		(6 << 1)
#define	GM_RX_RING_SZ_4086		(7 << 1)
#define	GM_RX_RING_SZ_8192		(8 << 1)

/* -- 0x4004	RW	Rx descriptor ring base low
 * -- 0x4008	RW	Rx descriptor ring base high
 *
 * Base of rx ring
 */
#define GM_RX_DESC_LO			(0x4004 | REG_SZ_32)
#define GM_RX_DESC_HI			(0x4008 | REG_SZ_32)

/* -- 0x4020	RW	Rx pause threshold
 * d: 0x000000f8	
 *
 * Two PAUSE thresholds are used to define when PAUSE flow control frames are
 * emitted by GEM. The granularity of these thresholds is in 64 byte increments.
 * XOFF PAUSE frames use the pause_time value pre-programmed in the
 * Send PAUSE MAC Register.
 * XON PAUSE frames use a pause_time of 0.
 */
#define GM_RX_PTH			(0x4020 | REG_SZ_32)
			/*
			 * 0-8: XOFF PAUSE emitted when RX FIFO
			 * occupancy rises above this value (times 64 bytes)
			 */
#define	GM_RX_PTH_OFF_MASK		0x000001ff
#define GM_RX_PTH_OFF_SHIFT		0
			/*
			 * 12-20: XON PAUSE emitted when RX FIFO
			 * occupancy falls below this value (times 64 bytes)
			 */
#define	GM_RX_PTH_ON_MASK		0x001ff000
#define	GM_RX_PTH_ON_SHIFT		12

#define GM_RX_PTH_UNITS			64

/* -- 0x4100	RW	Rx Kick
 * d: 0x00000000	The last valid RX descriptor is the one right before the value of the
 *                      register. Initially set to 0 on reset. RX descriptors must be posted
 *                      in multiples of 4. The first descriptor should be cache-line aligned
 *                      for best performance.
 */
#define GM_RX_KICK			(0x4100 | REG_SZ_16)

/* -- 0x4104	RW	Rx Completion
 * d: 0x00000000	All descriptors upto but excluding the register value are ready to be
 *                      processed by the host.
 */
#define GM_RX_COMP			(0x4104 | REG_SZ_16)
 
/* -- 0x4108	RW	Rx Blanking
 * d: 0x00000000	Written by the gmac with the last tx descriptor number +1 sent
 *
 * Defines the values used for receive interrupt blanking.
 * For INTR_TIME field, every count is 2048 PCI clock time. For 66 Mhz, each
 * count is about 15 ns.
 */
#define GM_RX_BLANK			(0x4108 | REG_SZ_32)
			/*
			 * 0-8:no.of pkts to be recvd since the last RX_DONE
			 * interrupt, before a new interrupt
			 */
#define	GM_RX_BLANK_INTR_PACKETS_MASK	0x000001ff
#define	GM_RX_BLANK_INTR_PACKETS_SHIFT	0
			/*
			 * 12-19 : no. of clocks to be counted since the last
			 * RX_DONE interrupt, before a new interrupt
			 */
#define	GM_RX_BLANK_INTR_TIME_MASK	0x000ff000
#define	GM_RX_BLANK_INTR_TIME_SHIFT	12

#define GM_RX_BLANK_UNITS		2048

/* -- 0x4120	RO	Rx fifo size
 *
 * This 11-bit RO register indicates the size, in 64-byte multiples, of the
 * RX FIFO. Software should use it to properly configure the PAUSE thresholds.
 * The value read is 0x140, indicating a 20kbyte RX FIFO.
 * -------------------------------------------------------------------------
 */
#define GM_RX_FIFO_SIZE			(0x4120 | REG_SZ_16)
#define GM_RZ_FIFO_SIZE_UNITS		64


	/*
	 * MAC regisers
	 */

/* -- 0x6000		MAC Tx reset control
 */
#define GM_MAC_TX_RESET			(0x6000 | REG_SZ_8)
#define GM_MAC_TX_RESET_NOW		0x01

/* -- 0x6004		MAC Rx reset control
 */
#define GM_MAC_RX_RESET			(0x6004 | REG_SZ_8)
#define GM_MAC_RX_RESET_NOW		0x01

/* -- 0x6008		Send Pause command register
 */
#define GM_MAC_SND_PAUSE		(0x6008 | REG_SZ_32)
#define GM_MAC_SND_PAUSE_TIME_MASK	0x0000ffff
#define GM_MAC_SND_PAUSE_TIME_SHIFT	0
#define GM_MAC_SND_PAUSE_NOW		0x00010000
#define GM_MAC_SND_PAUSE_DEFAULT	0x00001bf0

/* -- 0x6010		MAC transmit status
 */
#define GM_MAC_TX_STATUS		(0x6010 | REG_SZ_16)
#define GM_MAC_TX_STAT_SENT		0x0001
#define GM_MAC_TX_STAT_UNDERRUN		0x0002
#define GM_MAC_TX_STAT_MAX_PKT_ERR	0x0004
#define GM_MAC_TX_STAT_NORM_COLL_OVF	0x0008
#define GM_MAC_TX_STAT_EXCS_COLL_OVF	0x0010
#define GM_MAC_TX_STAT_LATE_COLL_OVF	0x0020
#define GM_MAC_TX_STAT_FIRS_COLL_OVF	0x0040
#define GM_MAC_TX_STAT_DEFER_TIMER_OVF	0x0080
#define GM_MAC_TX_STAT_PEAK_ATTMP_OVF	0x0100

/* -- 0x6014		MAC receive status
 */
#define GM_MAC_RX_STATUS		(0x6014 | REG_SZ_16)
#define GM_MAC_RX_STAT_RECEIVED		0x0001
#define GM_MAC_RX_STAT_FIFO_OVF		0x0002
#define GM_MAC_RX_STAT_FRAME_CTR_OVF	0x0004
#define GM_MAC_RX_STAT_ALIGN_ERR_OVF	0x0008
#define GM_MAC_RX_STAT_CRC_ERR_OVF	0x0010
#define GM_MAC_RX_STAT_LEN_ERR_OVF	0x0020
#define GM_MAC_RX_STAT_CODE_ERR_OVF	0x0040

/* -- 0x6018		MAC control & status
 */
#define GM_MAC_CTRLSTAT			(0x6018 | REG_SZ_32)
#define GM_MAC_CTRLSTAT_PAUSE_RCVD	0x00000001
#define GM_MAC_CTRLSTAT_PAUSE_STATE	0x00000002
#define GM_MAC_CTRLSTAT_PAUSE_NOT	0x00000004
#define GM_MAC_CTRLSTAT_PAUSE_TIM_MASK	0xffff0000
#define GM_MAC_CTRLSTAT_PAUSE_TIM_SHIFT	16

/* -- 0x6020		MAC Tx mask
 * 			Same bits as MAC Tx status
 */
#define GM_MAC_TX_MASK			(0x6020 | REG_SZ_16)

/* -- 0x6024		MAC Rx mask
 * 			Same bits as MAC Rx status
 */
#define GM_MAC_RX_MASK			(0x6024 | REG_SZ_16)

/* -- 0x6028		MAC Control/Status mask
 * 			Same bits as MAC control/status low order byte
 */
#define GM_MAC_CTRLSTAT_MASK		(0x6024 | REG_SZ_8)

/* -- 0x6030		MAC Tx configuration
 */
#define GM_MAC_TX_CONFIG		(0x6030 | REG_SZ_16)
#define GM_MAC_TX_CONF_ENABLE		0x0001
#define GM_MAC_TX_CONF_IGNORE_CARRIER	0x0002
#define GM_MAC_TX_CONF_IGNORE_COLL	0x0004
#define GM_MAC_TX_CONF_ENABLE_IPG0	0x0008
#define GM_MAC_TX_CONF_DONT_GIVEUP	0x0010
#define GM_MAC_TX_CONF_DONT_GIVEUP_NLMT	0x0020
#define GM_MAC_TX_CONF_NO_BACKOFF	0x0040
#define GM_MAC_TX_CONF_SLOWDOWN		0x0080
#define GM_MAC_TX_CONF_NO_FCS		0x0100
#define GM_MAC_TX_CONF_CARRIER_EXT	0x0200

/* -- 0x6034		MAC Rx configuration
 */
#define GM_MAC_RX_CONFIG		(0x6034 | REG_SZ_16)
#define GM_MAC_RX_CONF_ENABLE		0x0001
#define GM_MAC_RX_CONF_STRIP_PAD	0x0002
#define GM_MAC_RX_CONF_STIP_FCS		0x0004
#define GM_MAC_RX_CONF_RX_ALL		0x0008
#define GM_MAC_RX_CONF_RX_ALL_MULTI	0x0010
#define GM_MAC_RX_CONF_HASH_ENABLE	0x0020
#define GM_MAC_RX_CONF_ADDR_FLTR_ENABLE	0x0040
#define GM_MAC_RX_CONF_PASS_ERROR_FRAM	0x0080
#define GM_MAC_RX_CONF_CARRIER_EXT	0x0100

/* -- 0x6038		MAC control configuration
 */
#define GM_MAC_CTRL_CONFIG		(0x6038 | REG_SZ_8)
#define GM_MAC_CTRL_CONF_SND_PAUSE_EN	0x01
#define GM_MAC_CTRL_CONF_RCV_PAUSE_EN	0x02
#define GM_MAC_CTRL_CONF_PASS_CTRL_FRAM	0x04

/* -- 0x603c		MAC XIF configuration */
#define GM_MAC_XIF_CONFIG		(0x603c | REG_SZ_8)
#define GM_MAC_XIF_CONF_TX_MII_OUT_EN	0x01
#define GM_MAC_XIF_CONF_MII_INT_LOOP	0x02
#define GM_MAC_XIF_CONF_DISABLE_ECHO	0x04
#define GM_MAC_XIF_CONF_GMII_MODE	0x08
#define GM_MAC_XIF_CONF_MII_BUFFER_EN	0x10
#define GM_MAC_XIF_CONF_LINK_LED	0x20
#define GM_MAC_XIF_CONF_FULL_DPLX_LED	0x40

/* -- 0x6040		MAC inter-packet GAP 0
 */
#define GM_MAC_INTR_PKT_GAP0		(0x6040 | REG_SZ_8)
#define GM_MAC_INTR_PKT_GAP0_DEFAULT	0x00

/* -- 0x6044		MAC inter-packet GAP 1
 */
#define GM_MAC_INTR_PKT_GAP1		(0x6044 | REG_SZ_8)
#define GM_MAC_INTR_PKT_GAP1_DEFAULT	0x08

/* -- 0x6048		MAC inter-packet GAP 2
 */
#define GM_MAC_INTR_PKT_GAP2		(0x6048 | REG_SZ_8)
#define GM_MAC_INTR_PKT_GAP2_DEFAULT	0x04

/* -- 604c		MAC slot time
 */
#define GM_MAC_SLOT_TIME		(0x604C | REG_SZ_16)
#define GM_MAC_SLOT_TIME_DEFAULT	0x0040

/* -- 6050		MAC minimum frame size
 */
#define GM_MAC_MIN_FRAME_SIZE		(0x6050 | REG_SZ_16)
#define GM_MAC_MIN_FRAME_SIZE_DEFAULT	0x0040

/* -- 6054		MAC maximum frame size
 */
#define GM_MAC_MAX_FRAME_SIZE		(0x6054 | REG_SZ_16)
#define GM_MAC_MAX_FRAME_SIZE_DEFAULT	0x05ee
#define GM_MAC_MAX_FRAME_SIZE_ALIGN	0x5f0

/* -- 6058		MAC preamble length
 */
#define GM_MAC_PREAMBLE_LEN		(0x6058 | REG_SZ_16)
#define GM_MAC_PREAMBLE_LEN_DEFAULT	0x0007

/* -- 605c		MAC jam size
 */
#define GM_MAC_JAM_SIZE			(0x605c | REG_SZ_8)
#define GM_MAC_JAM_SIZE_DEFAULT		0x04

/* -- 6060		MAC attempt limit
 */
#define GM_MAC_ATTEMPT_LIMIT		(0x6060 | REG_SZ_8)
#define GM_MAC_ATTEMPT_LIMIT_DEFAULT	0x10

/* -- 6064		MAC control type
 */
#define GM_MAC_CONTROL_TYPE		(0x6064 | REG_SZ_16)
#define GM_MAC_CONTROL_TYPE_DEFAULT	0x8808

/* -- 6080		MAC address 15..0
 * -- 6084		MAC address 16..31
 * -- 6088		MAC address 32..47
 */
#define GM_MAC_ADDR_NORMAL0		(0x6080 | REG_SZ_16)
#define GM_MAC_ADDR_NORMAL1		(0x6084 | REG_SZ_16)
#define GM_MAC_ADDR_NORMAL2		(0x6088 | REG_SZ_16)

/* -- 608c		MAC alternate address 15..0
 * -- 6090		MAC alternate address 16..31
 * -- 6094		MAC alternate address 32..47
 */
#define GM_MAC_ADDR_ALT0		(0x608c | REG_SZ_16)
#define GM_MAC_ADDR_ALT1		(0x6090 | REG_SZ_16)
#define GM_MAC_ADDR_ALT2		(0x6094 | REG_SZ_16)

/* -- 6098		MAC control address 15..0
 * -- 609c		MAC control address 16..31
 * -- 60a0		MAC control address 32..47
 */
#define GM_MAC_ADDR_CTRL0		(0x6098 | REG_SZ_16)
#define GM_MAC_ADDR_CTRL1		(0x609c | REG_SZ_16)
#define GM_MAC_ADDR_CTRL2		(0x60a0 | REG_SZ_16)

/* -- 60a4		MAC address filter (0_0)
 * -- 60a8		MAC address filter (0_1)
 * -- 60ac		MAC address filter (0_2)
 */
#define GM_MAC_ADDR_FILTER0		(0x60a4 | REG_SZ_16)
#define GM_MAC_ADDR_FILTER1		(0x60a8 | REG_SZ_16)
#define GM_MAC_ADDR_FILTER2		(0x60ac | REG_SZ_16)

/* -- 60b0		MAC address filter mask 1,2
 */
#define GM_MAC_ADDR_FILTER_MASK1_2	(0x60b0 | REG_SZ_8)

/* -- 60b4		MAC address filter mask 0
 */
#define GM_MAC_ADDR_FILTER_MASK0	(0x60b4 | REG_SZ_16)

/* -- [60c0 .. 60fc]	MAC hash table
 */
#define GM_MAC_ADDR_FILTER_HASH0	(0x60c0 | REG_SZ_16)

/* -- 6100		MAC normal collision counter
 */
#define GM_MAC_COLLISION_CTR		(0x6100 | REG_SZ_16)

/* -- 6104		MAC 1st successful collision counter
 */
#define GM_MAC_FIRST_COLLISION_CTR	(0x6104 | REG_SZ_16)

/* -- 6108		MAC excess collision counter
 */
#define GM_MAC_EXCS_COLLISION_CTR	(0x6108 | REG_SZ_16)

/* -- 610c		MAC late collision counter
 */
#define GM_MAC_LATE_COLLISION_CTR	(0x610c | REG_SZ_16)

/* -- 6110		MAC defer timer counter
 */
#define GM_MAC_DEFER_TIMER_COUNTER	(0x6110 | REG_SZ_16)

/* -- 6114		MAC peak attempts
 */
#define GM_MAC_PEAK_ATTEMPTS		(0x6114 | REG_SZ_16)

/* -- 6118		MAC Rx frame counter
 */
#define GM_MAC_RX_FRAME_CTR		(0x6118 | REG_SZ_16)

/* -- 611c		MAC Rx length error counter
 */
#define GM_MAC_RX_LEN_ERR_CTR		(0x611c | REG_SZ_16)

/* -- 6120		MAC Rx alignment error counter
 */
#define GM_MAC_RX_ALIGN_ERR_CTR		(0x6120 | REG_SZ_16)

/* -- 6124		MAC Rx CRC error counter
 */
#define GM_MAC_RX_CRC_ERR_CTR		(0x6124 | REG_SZ_16)

/* -- 6128		MAC Rx code violation error counter
 */
#define GM_MAC_RX_CODE_VIOLATION_CTR	(0x6128 | REG_SZ_16)

/* -- 6130		MAC random number seed
 */
#define GM_MAC_RANDOM_SEED		(0x6130 | REG_SZ_16)

/* -- 6134		MAC state machine
 */
#define GM_MAC_STATE_MACHINE		(0x6134 | REG_SZ_8)


	/*
	 * MIF registers
	 */


/* -- 0x6200	RW	MIF bit bang clock
 */
#define GM_MIF_BB_CLOCK			(0x6200 | REG_SZ_8)

/* -- 0x6204	RW	MIF bit bang data
 */
#define GM_MIF_BB_DATA			(0x6204 | REG_SZ_8)

/* -- 0x6208	RW	MIF bit bang output enable
 */
#define GM_MIF_BB_OUT_ENABLE		(0x6208 | REG_SZ_8)

/* -- 0x620c	RW	MIF frame control & data
 */
#define GM_MIF_FRAME_CTL_DATA		(0x620c | REG_SZ_32)
#define GM_MIF_FRAME_START_MASK		0xc0000000
#define GM_MIF_FRAME_START_SHIFT	30
#define GM_MIF_FRAME_OPCODE_MASK	0x30000000
#define GM_MIF_FRAME_OPCODE_SHIFT	28
#define GM_MIF_FRAME_PHY_ADDR_MASK	0x0f800000
#define GM_MIF_FRAME_PHY_ADDR_SHIFT	23
#define GM_MIF_FRAME_REG_ADDR_MASK	0x007c0000
#define GM_MIF_FRAME_REG_ADDR_SHIFT	18
#define GM_MIF_FRAME_TURNAROUND_HI	0x00020000
#define GM_MIF_FRAME_TURNAROUND_LO	0x00010000
#define GM_MIF_FRAME_DATA_MASK		0x0000ffff
#define GM_MIF_FRAME_DATA_SHIFT		0

/* -- 0x6210	RW	MIF config reg
 */
#define GM_MIF_CFG			(0x6210 | REG_SZ_16)
#define	GM_MIF_CFGPS			0x00000001	/* PHY Select */
#define	GM_MIF_CFGPE			0x00000002	/* Poll Enable */
#define	GM_MIF_CFGBB			0x00000004	/* Bit Bang Enable */
#define	GM_MIF_CFGPR_MASK		0x000000f8	/* Poll Register address */
#define	GM_MIF_CFGPR_SHIFT		3
#define	GM_MIF_CFGM0			0x00000100	/* MDIO_0 Data / MDIO_0 attached */
#define	GM_MIF_CFGM1			0x00000200	/* MDIO_1 Data / MDIO_1 attached */
#define	GM_MIF_CFGPD_MASK		0x00007c00	/* Poll Device PHY address */
#define	GM_MIF_CFGPD_SHIFT		10

#define	GM_MIF_POLL_DELAY		200

#define	GM_INTERNAL_PHYAD		1		/* PHY address for int. transceiver */
#define	GM_EXTERNAL_PHYAD		0		/* PHY address for ext. transceiver */

/* -- 0x6214	RW	MIF interrupt mask reg
 *			same as basic/status Register
 */
#define GM_MIF_IRQ_MASK			(0x6214 | REG_SZ_16)

/* -- 0x6218	RW	MIF basic/status reg
 *			The Basic portion of this register indicates the last
 *			value of the register read indicated in the POLL REG field
 *			of the Configuration Register.
 *			The Status portion indicates bit(s) that have changed.
 *			The MIF Mask register is corresponding to this register in
 *			terms of the bit(s) that need to be masked for generating
 *			interrupt on the MIF Interrupt Bit of the Global Status Rgister.
 */
#define GM_MIF_STATUS			(0x6218 | REG_SZ_32)

#define	GM_MIF_STATUS_MASK		0x0000ffff	/* 0-15 : Status */
#define	GM_MIF_BASIC_MASK		0xffff0000	/* 16-31 : Basic register */

	/*
	 * PCS link registers
	 */

/* -- 0x9000	RW	PCS mii control reg
 */
#define GM_PCS_CONTROL			(0x9000 | REG_SZ_16)

/* -- 0x9004	RW	PCS mii status reg
 */
#define GM_PCS_STATUS			(0x9004 | REG_SZ_16)

/* -- 0x9008	RW	PCS mii advertisement
 */
#define GM_PCS_ADVERTISEMENT		(0x9008 | REG_SZ_16)

/* -- 0x900c	RW	PCS mii LP ability
 */
#define GM_PCS_ABILITY			(0x900c | REG_SZ_16)

/* -- 0x9010	RW	PCS config
 */
#define GM_PCS_CONFIG			(0x9010 | REG_SZ_8)

/* -- 0x9014	RW	PCS state machine
 */
#define GM_PCS_STATE_MACHINE		(0x9014 | REG_SZ_32)

/* -- 0x9018	RW	PCS interrupt status
 */
#define GM_PCS_IRQ_STATUS		(0x9018 | REG_SZ_8)

/* -- 0x9050	RW	PCS datapath mode
 */
#define GM_PCS_DATAPATH_MODE		(0x9050 | REG_SZ_8)
#define GM_PCS_DATAPATH_INTERNAL	0x01	/* Internal serial link */
#define GM_PCS_DATAPATH_SERDES		0x02	/* 10-bit Serdes interface */
#define GM_PCS_DATAPATH_MII		0x04	/* Select mii/gmii mode */
#define GM_PCS_DATAPATH_GMII_OUT	0x08	/* serial mode only, copy data to gmii */

/* -- 0x9054	RW	PCS serdes control
 */
#define GM_PCS_SERDES_CTRL		(0x9054 | REG_SZ_8)

/* -- 0x9058	RW	PCS serdes output select
 */
#define GM_PCS_SERDES_SELECT		(0x9058 | REG_SZ_8)

/* -- 0x905c	RW	PCS serdes state
 */
#define GM_PCS_SERDES_STATE		(0x905c | REG_SZ_8)


	/*
	 * PHY registers
	 */

/*
 * Standard PHY registers (from de4x5.h)
 */
#define MII_CR       0x00          /* MII Management Control Register */
#define MII_SR       0x01          /* MII Management Status Register */
#define MII_ID0      0x02          /* PHY Identifier Register 0 */
#define MII_ID1      0x03          /* PHY Identifier Register 1 */
#define MII_ANA      0x04          /* Auto Negotiation Advertisement */
#define MII_ANLPA    0x05          /* Auto Negotiation Link Partner Ability */
#define MII_ANE      0x06          /* Auto Negotiation Expansion */
#define MII_ANP      0x07          /* Auto Negotiation Next Page TX */

/*
** MII Management Control Register
*/
#define MII_CR_RST	 0x8000         /* RESET the PHY chip */
#define MII_CR_LPBK	 0x4000         /* Loopback enable */
#define MII_CR_SPD 	 0x2000         /* 0: 10Mb/s; 1: 100Mb/s */
#define MII_CR_10  	 0x0000         /* Set 10Mb/s */
#define MII_CR_100 	 0x2000         /* Set 100Mb/s */
#define MII_CR_ASSE	 0x1000         /* Auto Speed Select Enable */
#define MII_CR_PD  	 0x0800         /* Power Down */
#define MII_CR_ISOL	 0x0400         /* Isolate Mode */
#define MII_CR_RAN 	 0x0200         /* Restart Auto Negotiation */
#define MII_CR_FDM 	 0x0100         /* Full Duplex Mode */
#define MII_CR_CTE 	 0x0080         /* Collision Test Enable */
#define MII_CR_SPEEDSEL2 0x0040		/* Speed selection 2 on BCM */
/*
** MII Management Status Register
*/
#define MII_SR_T4C  0x8000         /* 100BASE-T4 capable */
#define MII_SR_TXFD 0x4000         /* 100BASE-TX Full Duplex capable */
#define MII_SR_TXHD 0x2000         /* 100BASE-TX Half Duplex capable */
#define MII_SR_TFD  0x1000         /* 10BASE-T Full Duplex capable */
#define MII_SR_THD  0x0800         /* 10BASE-T Half Duplex capable */
#define MII_SR_ASSC 0x0020         /* Auto Speed Selection Complete*/
#define MII_SR_RFD  0x0010         /* Remote Fault Detected */
#define MII_SR_ANC  0x0008         /* Auto Negotiation capable */
#define MII_SR_LKS  0x0004         /* Link Status */
#define MII_SR_JABD 0x0002         /* Jabber Detect */
#define MII_SR_XC   0x0001         /* Extended Capabilities */

/*
** MII Management Auto Negotiation Advertisement Register
*/
#define MII_ANA_TAF  0x03e0        /* Technology Ability Field */
#define MII_ANA_T4AM 0x0200        /* T4 Technology Ability Mask */
#define MII_ANA_TXAM 0x0180        /* TX Technology Ability Mask */
#define MII_ANA_FDAM 0x0140        /* Full Duplex Technology Ability Mask */
#define MII_ANA_HDAM 0x02a0        /* Half Duplex Technology Ability Mask */
#define MII_ANA_100M 0x0380        /* 100Mb Technology Ability Mask */
#define MII_ANA_10M  0x0060        /* 10Mb Technology Ability Mask */
#define MII_ANA_CSMA 0x0001        /* CSMA-CD Capable */

/*
** MII Management Auto Negotiation Remote End Register
*/
#define MII_ANLPA_NP   0x8000      /* Next Page (Enable) */
#define MII_ANLPA_ACK  0x4000      /* Remote Acknowledge */
#define MII_ANLPA_RF   0x2000      /* Remote Fault */
#define MII_ANLPA_TAF  0x03e0      /* Technology Ability Field */
#define MII_ANLPA_T4AM 0x0200      /* T4 Technology Ability Mask */
#define MII_ANLPA_TXAM 0x0180      /* TX Technology Ability Mask */
#define MII_ANLPA_FDAM 0x0140      /* Full Duplex Technology Ability Mask */
#define MII_ANLPA_HDAM 0x02a0      /* Half Duplex Technology Ability Mask */
#define MII_ANLPA_100M 0x0380      /* 100Mb Technology Ability Mask */
#define MII_ANLPA_10M  0x0060      /* 10Mb Technology Ability Mask */
#define MII_ANLPA_CSMA 0x0001      /* CSMA-CD Capable */
#define MII_ANLPA_PAUS 0x0400 

/* Generic PHYs
 * 
 * These GENERIC values assumes that the PHY devices follow 802.3u and
 * allow parallel detection to set the link partner ability register.
 * Detection of 100Base-TX [H/F Duplex] and 100Base-T4 is supported.
 */

/*
 * Model-specific PHY registers
 *
 * Note: Only the BCM5201 is described here for now. I'll add the 5400 once
 *       I see a machine using it in real world.
 */

/* Supported PHYs (phy_type field ) */
#define PHY_B5400	0x5400
#define PHY_B5401	0x5401
#define PHY_B5411	0x5411
#define PHY_B5201	0x5201
#define PHY_B5221	0x5221
#define PHY_LXT971	0x0971
#define PHY_UNKNOWN	0

/* Identification (for multi-PHY) */
#define MII_BCM5201_OUI                         0x001018
#define MII_BCM5201_MODEL                       0x21
#define MII_BCM5201_REV                         0x01
#define MII_BCM5201_ID                          ((MII_BCM5201_OUI << 10) | (MII_BCM5201_MODEL << 4))
#define MII_BCM5201_MASK                        0xfffffff0
#define MII_BCM5221_OUI                         0x001018
#define MII_BCM5221_MODEL                       0x1e
#define MII_BCM5221_REV                         0x00
#define MII_BCM5221_ID                          ((MII_BCM5221_OUI << 10) | (MII_BCM5221_MODEL << 4))
#define MII_BCM5221_MASK                        0xfffffff0
#define MII_BCM5400_OUI                         0x000818
#define MII_BCM5400_MODEL                       0x04
#define MII_BCM5400_REV                         0x01
#define MII_BCM5400_ID                          ((MII_BCM5400_OUI << 10) | (MII_BCM5400_MODEL << 4))
#define MII_BCM5400_MASK                        0xfffffff0
#define MII_BCM5401_OUI                         0x000818
#define MII_BCM5401_MODEL                       0x05
#define MII_BCM5401_REV                         0x01
#define MII_BCM5401_ID                          ((MII_BCM5401_OUI << 10) | (MII_BCM5401_MODEL << 4))
#define MII_BCM5401_MASK                        0xfffffff0
#define MII_BCM5411_OUI                         0x000818
#define MII_BCM5411_MODEL                       0x07
#define MII_BCM5411_REV                         0x01
#define MII_BCM5411_ID                          ((MII_BCM5411_OUI << 10) | (MII_BCM5411_MODEL << 4))
#define MII_BCM5411_MASK                        0xfffffff0
#define MII_LXT971_OUI                          0x0004de
#define MII_LXT971_MODEL                        0x0e
#define MII_LXT971_REV                          0x00
#define MII_LXT971_ID                           ((MII_LXT971_OUI << 10) | (MII_LXT971_MODEL << 4))
#define MII_LXT971_MASK                         0xfffffff0

/* BCM5201 AUX STATUS register */
#define MII_BCM5201_AUXCTLSTATUS		0x18
#define MII_BCM5201_AUXCTLSTATUS_DUPLEX		0x0001
#define MII_BCM5201_AUXCTLSTATUS_SPEED		0x0002

/* MII BCM5201 MULTIPHY interrupt register */
#define MII_BCM5201_INTERRUPT			0x1A
#define MII_BCM5201_INTERRUPT_INTENABLE		0x4000

#define MII_BCM5201_AUXMODE2			0x1B
#define MII_BCM5201_AUXMODE2_LOWPOWER		0x0008

#define MII_BCM5201_MULTIPHY                    0x1E

/* MII BCM5201 MULTIPHY register bits */
#define MII_BCM5201_MULTIPHY_SERIALMODE         0x0002
#define MII_BCM5201_MULTIPHY_SUPERISOLATE       0x0008

/* MII BCM5400 1000-BASET Control register */
#define MII_BCM5400_GB_CONTROL			0x09
#define MII_BCM5400_GB_CONTROL_FULLDUPLEXCAP	0x0200

/* MII BCM5400 AUXCONTROL register */
#define MII_BCM5400_AUXCONTROL                  0x18
#define MII_BCM5400_AUXCONTROL_PWR10BASET       0x0004

/* MII BCM5400 AUXSTATUS register */
#define MII_BCM5400_AUXSTATUS                   0x19
#define MII_BCM5400_AUXSTATUS_LINKMODE_MASK     0x0700
#define MII_BCM5400_AUXSTATUS_LINKMODE_SHIFT    8  

/* MII LXT971 STATUS2 register */
#define MII_LXT971_STATUS2			0x11
#define MII_LXT971_STATUS2_SPEED		0x4000
#define MII_LXT971_STATUS2_LINK			0x0400
#define MII_LXT971_STATUS2_FULLDUPLEX		0x0200
#define MII_LXT971_STATUS2_AUTONEG_COMPLETE	0x0080


	/*
	 * DMA descriptors
	 */


/* 
 * Descriptor counts and buffer sizes
 */
#define NTX			64		/* must be power of 2 */
#define NTX_CONF		GM_TX_RING_SZ_64
#define NRX			64		/* must be power of 2 */
#define NRX_CONF		GM_RX_RING_SZ_64
#define RX_COPY_THRESHOLD	256
#define GMAC_BUFFER_ALIGN	32		/* Align on a cache line */
#define RX_BUF_ALLOC_SIZE	(ETH_FRAME_LEN + GMAC_BUFFER_ALIGN + 2)
#define RX_OFFSET		2

/*
 * Definitions of Rx and Tx descriptors
 */

struct gmac_dma_desc {
	unsigned int	size;		/* data size and OWN bit */
	unsigned int	flags;		/* flags */
	unsigned int	lo_addr;	/* phys addr, low 32 bits */
	unsigned int	hi_addr;
};

/*
 * Rx bits
 */
 
/* Bits in size */
#define RX_SZ_OWN		0x80000000	/* 1 = owned by chip */
#define RX_SZ_MASK		0x7FFF0000
#define RX_SZ_SHIFT		16
#define RX_SZ_CKSUM_MASK	0x0000FFFF

/* Bits in flags */
#define RX_FL_CRC_ERROR		0x40000000
#define RX_FL_ALT_ADDR		0x20000000	/* Packet rcv. from alt MAC address */

/* 
 * Tx bits
 */

/* Bits in size */
#define TX_SZ_MASK		0x00007FFF
#define TX_SZ_CRC_MASK		0x00FF8000
#define TX_SZ_CRC_STUFF		0x1F000000
#define TX_SZ_CRC_ENABLE	0x20000000
#define TX_SZ_EOP		0x40000000
#define TX_SZ_SOP		0x80000000
/* Bits in flags */
#define TX_FL_INTERRUPT		0x00000001
#define TX_FL_NO_CRC		0x00000002

	/*
	 * Other stuffs
	 */
	 
struct gmac {
	volatile unsigned int		*regs;	/* hardware registers, virtual addr */
	struct net_device		*dev;
	struct device_node		*of_node;
	unsigned long			tx_desc_page;	/* page for DMA descriptors */
	unsigned long			rx_desc_page;	/* page for DMA descriptors */
	volatile struct			gmac_dma_desc *rxring;
	struct sk_buff			*rx_buff[NRX];
	int				next_rx;
	volatile struct	gmac_dma_desc	*txring;
	struct sk_buff			*tx_buff[NTX];
	int				next_tx;
	int				tx_gone;
	int				phy_addr;
	unsigned int			phy_id;
	int				phy_type;
	int				phy_status;	/* Cached PHY status */
	int				full_duplex;	/* Current set to full duplex */
	int				gigabit;	/* Current set to 1000BT */
	struct net_device_stats		stats;
	u8				pci_bus;
	u8				pci_devfn;
	spinlock_t			lock;
	int				opened;
	int				sleeping;
	struct net_device		*next_gmac;
};


/* Register access macros. We hope the preprocessor will be smart enough
 * to optimize them into one single access instruction
 */
#define GM_OUT(reg, v)		(((reg) & REG_SZ_32) ? out_le32(gm->regs + \
					(((reg) & REG_MASK)>>2), (v))  \
				: (((reg) & REG_SZ_16) ? out_le16((volatile u16 *) \
					(gm->regs + (((reg) & REG_MASK)>>2)), (v))  \
				: out_8((volatile u8 *)(gm->regs + \
					(((reg) & REG_MASK)>>2)), (v))))
#define GM_IN(reg)		(((reg) & REG_SZ_32) ? in_le32(gm->regs + \
					(((reg) & REG_MASK)>>2))  \
				: (((reg) & REG_SZ_16) ? in_le16((volatile u16 *) \
					(gm->regs + (((reg) & REG_MASK)>>2)))  \
				: in_8((volatile u8 *)(gm->regs + \
					(((reg) & REG_MASK)>>2)))))
#define GM_BIS(r, v)		GM_OUT((r), GM_IN(r) | (v))
#define GM_BIC(r, v)		GM_OUT((r), GM_IN(r) & ~(v))

/* Wrapper to alloc_skb to test various alignements */
#define GMAC_ALIGNED_RX_SKB_ADDR(addr) \
        ((((unsigned long)(addr) + GMAC_BUFFER_ALIGN - 1) & \
        ~(GMAC_BUFFER_ALIGN - 1)) - (unsigned long)(addr))
        
static inline struct sk_buff *
gmac_alloc_skb(unsigned int length, int gfp_flags)
{
	struct sk_buff *skb;

	skb = alloc_skb(length + GMAC_BUFFER_ALIGN, gfp_flags);
	if(skb) {
		int offset = GMAC_ALIGNED_RX_SKB_ADDR(skb->data);

		if(offset)
			skb_reserve(skb, offset);
	}
	return skb;
}

