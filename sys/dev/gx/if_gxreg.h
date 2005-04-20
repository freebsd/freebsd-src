/*-
 * Copyright (c) 1999,2000,2001 Jonathan Lemon
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$FreeBSD$
 */

/*
 * Intel vendor ID
 */
#define INTEL_VENDORID		0x8086

/*
 * Intel gigabit ethernet device ID
 */
#define DEVICEID_WISEMAN		0x1000
#define DEVICEID_LIVINGOOD_FIBER	0x1001
#define DEVICEID_LIVINGOOD_COPPER	0x1004
#define DEVICEID_CORDOVA_COPPER		0x1008
#define DEVICEID_CORDOVA_FIBER		0x1009
#define DEVICEID_CORDOVA2_COPPER	0x100D

/*
 * chip register offsets. These are memory mapped registers
 * which can be accessed with the CSR_READ_4()/CSR_WRITE_4() macros.
 * Each register must be accessed using 32 bit operations.
 */

#define GX_CTRL			0x0000	/* control register */
#define GX_STATUS		0x0008	/* status register */
#define GX_EEPROM_CTRL		0x0010	/* EEPROM/Flash control/data */
#define GX_CTRL_EXT		0x0018	/* extended device control */
#define GX_MDIC			0x0020	/* MDI control */
#define GX_FLOW_CTRL_BASE	0x0028	/* flow control address low/high */
#define GX_FLOW_CTRL_TYPE	0x0030	/* flow control type */
#define GX_VET			0x0038	/* VLAN ethertype */
#define GX_RX_ADDR_BASE		0x0040	/* 16 pairs of receive address low/high */

#define GX_INT_READ		0x00C0	/* read interrupts */
#define GX_INT_FORCE		0x00C8	/* force an interrupt */
#define GX_INT_MASK_SET		0x00D0	/* interrupt mask set/read */
#define GX_INT_MASK_CLR		0x00D8	/* interrupt mask clear */

#define GX_RX_CONTROL		0x0100	/* RX control */

/* 82542 and older 82543 chips */
#define GX_RX_OLD_INTR_DELAY	0x0108	/* RX delay timer */
#define GX_RX_OLD_RING_BASE	0x0110	/* RX descriptor base address */
#define GX_RX_OLD_RING_LEN	0x0118	/* RX descriptor length */
#define GX_RX_OLD_RING_HEAD	0x0120	/* RX descriptor head */
#define GX_RX_OLD_RING_TAIL	0x0128	/* RX descriptor tail */

/* 82542 and older 82543 chips */
#define GX_OLD_FCRTH		0x0160	/* flow control rcv threshhold high */
#define GX_OLD_FCRTL		0x0168	/* flow control rcv threshhold low */

#define GX_FCTTV		0x0170	/* flow control xmit timer value */
#define GX_TX_CONFIG		0x0178	/* xmit configuration (tbi mode) */
#define GX_RX_CONFIG		0x0180	/* recv configuration word */

#define GX_MULTICAST_BASE	0x0200	/* multicast table array base */

#define GX_TX_CONTROL		0x0400	/* TX control */
#define GX_TX_IPG		0x0410	/* TX interpacket gap */

/* 82542 and older 82543 chips */
#define GX_TX_OLD_RING_BASE	0x0420	/* TX descriptor base address */
#define GX_TX_OLD_RING_LEN	0x0428	/* TX descriptor length */
#define GX_TX_OLD_RING_HEAD	0x0430	/* TX descriptor head */
#define GX_TX_OLD_RING_TAIL	0x0438	/* TX descriptor tail */
#define GX_TX_OLD_INTR_DELAY	0x0440	/* TX interrupt delay value */

#define GX_TBT			0x0448	/* TX burst timer */
#define GX_AIT			0x0458	/* adaptive IFS throttle */

#define GX_VFTA_BASE		0x0600	/* VLAN filter table array base */

#define GX_PKT_BUFFER_ALLOC	0x1000	/* Packet buffer allocation */
#define GX_ERT			0x2000	/* Early receive threshold */
#define GX_RX_OLD_DMA_CTRL	0x2028	/* RX descriptor control */

/* newer 82543 chips */
#define GX_FCRTH		0x2160	/* flow control rcv threshhold high */
#define GX_FCRTL		0x2168	/* flow control rcv threshhold low */

/* newer 82543 chips */
#define GX_RX_RING_BASE		0x2800	/* RX descriptor base address */
#define GX_RX_RING_LEN		0x2808	/* RX descriptor length */
#define GX_RX_RING_HEAD		0x2810	/* RX descriptor head */
#define GX_RX_RING_TAIL		0x2818	/* RX descriptor tail */
#define GX_RX_INTR_DELAY	0x2820	/* RX delay timer */
#define GX_RX_DMA_CTRL		0x2820	/* RX descriptor control */

#define GX_EARLY_TX_THRESH	0x3000	/* early transmit threshold */
#define GX_TX_OLD_DMA_CTRL	0x3028	/* TX descriptor control */

/* newer 82543 chips */
#define GX_TX_RING_BASE		0x3800	/* TX descriptor base address */
#define GX_TX_RING_LEN		0x3808	/* TX descriptor length */
#define GX_TX_RING_HEAD		0x3810	/* TX descriptor head */
#define GX_TX_RING_TAIL		0x3818	/* TX descriptor tail */
#define GX_TX_INTR_DELAY	0x3820	/* TX interrupt delay value */
#define GX_TX_DMA_CTRL		0x3828	/* TX descriptor control */

#define GX_CRCERRS		0x4000	/* CRC error count */
#define GX_ALGNERRC		0x4004	/* alignment error count */
#define GX_SYMERRS		0x4008	/* symbol error count */
#define GX_RXERRC		0x400C	/* RX error count */
#define GX_MPC			0x4010	/* missed packets count */
#define GX_SCC			0x4014	/* single collision count */
#define GX_ECOL			0x4018	/* excessive collision count */
#define GX_MCC			0x401C	/* multiple collision count */
#define GX_LATECOL		0x4020	/* late collision count */
#define GX_COLC			0x4020	/* collision count */
#define GX_TUC			0x402C	/* transmit underrun count */
#define GX_DC			0x4030	/* defer count */
#define GX_TNCRS		0x4034	/* transmit - no CRS */
#define GX_SEC			0x4038	/* sequence error count */
#define GX_CEXTERR		0x403C	/* carrier extension error count */
#define GX_RLEC			0x4040	/* receive length error count */
#define GX_RDMAUC		0x4044	/* receive DMA underrun count */
#define GX_XONRXC		0x4048	/* XON received count */
#define GX_XONTXC		0x404C	/* XON transmitted count */
#define GX_XOFFRXC		0x4050	/* XOFF received count */
#define GX_XOFFTXC		0x4054	/* XOFF transmitted count */
#define GX_FCRUC		0x4058	/* FC received unsupported count */
#define GX_PRC64		0x405C	/* packets rcvd (64 bytes) */
#define GX_PRC127		0x4060	/* packets rcvd (65 - 127 bytes) */
#define GX_PRC255		0x4064	/* packets rcvd (128 - 255 bytes) */
#define GX_PRC511		0x4068	/* packets rcvd (256 - 511 bytes) */
#define GX_PRC1023		0x406C	/* packets rcvd (512 - 1023 bytes) */
#define GX_PRC1522		0x4070	/* packets rcvd (1023 - 1522 bytes) */
#define GX_GPRC			0x4074	/* good packets received */
#define GX_BPRC			0x4078	/* broadcast packets received */
#define GX_MPRC			0x407C	/* multicast packets received */
#define GX_GPTC			0x4080	/* good packets transmitted */
#define GX_GORC			0x4088	/* good octets received (low/high) */
#define GX_GOTC			0x4090	/* good octets transmitted (low/high) */
#define GX_RNBC			0x40A0	/* receive no buffers count */
#define GX_RUC			0x40A4	/* receive undersize count */
#define GX_RFC			0x40A8	/* receive fragment count */
#define GX_ROC			0x40AC	/* receive oversize count */
#define GX_RJC			0x40B0	/* receive jabber count */
#define GX_TOR			0x40C0	/* total octets received (low/high) */
#define GX_TOT			0x40C8	/* total octets transmitted (low/high) */
#define GX_TPR			0x40D0	/* total packets received */
#define GX_TPT			0x40D4	/* total packets transmitted */
#define GX_PTC64		0x40D8	/* packets transmitted (64 B) */
#define GX_PTC127		0x40DC	/* packets xmitted (65 - 127 B) */
#define GX_PTC255		0x40E0	/* packets xmitted (128 - 255 B) */
#define GX_PTC511		0x40E4	/* packets xmitted (256 - 511 B) */
#define GX_PTC1023		0x40E8	/* packets xmitted (512 - 1023 B) */
#define GX_PTC1522		0x40EC	/* packets xmitted (1023 - 1522 B) */
#define GX_MPTC			0x40F0	/* multicast packets transmitted */
#define GX_BPTC			0x40F4	/* broadcast packets transmitted */
#define GX_TSCTC		0x40F8	/* TCP segmentation context xmitted */
#define GX_TSCTFC		0x40FC	/* TCP segmentation context fail */

#define GX_RX_CSUM_CONTROL	0x5000	/* receive checksum control */

#define GX_RDFH			0x8000	/* RX data fifo head */
#define GX_RDFT			0x8008	/* RX data fifo tail */
#define GX_TDFH			0x8010	/* TX data fifo head */
#define GX_TDFT			0x8018	/* TX data fifo tail */

#define GX_RBM_BASE		0x10000	/* packet buffer memory */

/* GX_RX_CSUM_CONTROL */
#define GX_CSUM_START_MASK	0x000ff
#define GX_CSUM_IP		0x00100
#define GX_CSUM_TCP		0x00200

/* GX_CTRL register */
#define GX_CTRL_DUPLEX		0x00000001	/* full duplex */
#define GX_CTRL_BIGENDIAN	0x00000002	/* 1 == big endian */
#define GX_CTRL_PCI_PRIORITY	0x00000004	/* 1 == fairness */
#define GX_CTRL_LINK_RESET	0x00000008
#define GX_CTRL_TEST_MODE	0x00000010
#define GX_CTRL_AUTOSPEED	0x00000020
#define GX_CTRL_SET_LINK_UP	0x00000040
#define GX_CTRL_INVERT_LOS	0x00000080	/* invert loss of signal */
#define GX_CTRL_SPEEDMASK	0x00000300	/* 2 bits */
#define GX_CTRL_FORCESPEED	0x00000800
#define GX_CTRL_FORCEDUPLEX	0x00001000
#define GX_CTRL_GPIO_0		0x00040000	/* Software defined pin #0 */
#define GX_CTRL_GPIO_1		0x00080000	/* Software defined pin #1 */
#define GX_CTRL_GPIO_2		0x00100000	/* Software defined pin #2 */
#define GX_CTRL_GPIO_3		0x00200000	/* Software defined pin #3 */
#define GX_CTRL_GPIO_DIR_0	0x00400000	/* Pin is Input(0)/Output(1) */
#define GX_CTRL_GPIO_DIR_1	0x00800000	/* Pin is Input(0)/Output(1) */
#define GX_CTRL_GPIO_DIR_2	0x01000000	/* Pin is Input(0)/Output(1) */
#define GX_CTRL_GPIO_DIR_3	0x02000000	/* Pin is Input(0)/Output(1) */
#define GX_CTRL_DEVICE_RESET	0x04000000
#define GX_CTRL_RX_FLOWCTRL	0x08000000	/* RX flowcontrol enable */
#define GX_CTRL_TX_FLOWCTRL	0x10000000	/* TX flowcontrol enable */
#define GX_CTRL_VLAN_ENABLE	0x40000000

/* GX_STATUS register */
#define GX_STAT_DUPLEX		0x00000001
#define GX_STAT_LINKUP		0x00000002
#define GX_STAT_XMITCLK_OK	0x00000004
#define GX_STAT_RECVCLK_OK	0x00000008
#define GX_STAT_XMIT_OFF	0x00000010
#define GX_STAT_TBIMODE		0x00000020
#define GX_STAT_SPEED_MASK	0x000000C0	/* 2 bits, not valid w/TBI */
#define GX_STAT_AUTOSPEED_MASK	0x00000300	/* 2 bits, not valid w/TBI */
#define GX_STAT_MTXCLK_OK	0x00000400
#define GX_STAT_PCI66		0x00000800
#define GX_STAT_BUS64		0x00001000

#define GX_SPEED_10MB		0x00000000
#define GX_SPEED_100MB		0x00000040
#define GX_SPEED_1000MB		0x00000080

/* GX_EEPROM_CTRL register */
#define GX_EE_CLOCK		0x0001		/* software clock */
#define GX_EE_SELECT		0x0002		/* chip select */
#define GX_EE_DATA_IN		0x0004
#define GX_EE_DATA_OUT		0x0008
#define GX_EE_FLASH_CTRL	0x0030		/* 0x02 == enable writes */

/* serial EEPROM opcodes */
#define GX_EE_OPC_WRITE    	0x5
#define GX_EE_OPC_READ     	0x6
#define GX_EE_OPC_ERASE    	0x7

#define GX_EE_OPC_SIZE		3		/* bits of opcode */
#define GX_EE_ADDR_SIZE		6		/* bits of address */

/* EEPROM map offsets */
#define GX_EEMAP_MAC		0x00		/* station address (6 bytes) */
#define GX_EEMAP_INIT1		0x0A		/* init control (2 bytes) */

/* GX_CTRL_EXT register */
#define GX_CTRLX_GPIO_4		0x00000010	/* Software defined pin #4 */
#define GX_CTRLX_GPIO_5		0x00000020	/* Software defined pin #5 */
#define GX_CTRLX_GPIO_6		0x00000040	/* Software defined pin #6 */
#define GX_CTRLX_GPIO_7		0x00000080	/* Software defined pin #7 */
#define GX_CTRLX_GPIO_DIR_4	0x00000100	/* Pin is Input(0)/Output(1) */
#define GX_CTRLX_GPIO_DIR_5	0x00000200	/* Pin is Input(0)/Output(1) */
#define GX_CTRLX_GPIO_DIR_6	0x00000400	/* Pin is Input(0)/Output(1) */
#define GX_CTRLX_GPIO_DIR_7	0x00000800	/* Pin is Input(0)/Output(1) */
#define GX_CTRLX_EEPROM_RESET	0x00002000	/* PCI_RST type EEPROM reset */
#define GX_CTRLX_SPEED_BYPASS	0x00008000	/* use CTRL.SPEED setting */

/*
 * Defines for MII/GMII PHY.
 *
 * GPIO bits 0-3 are controlled by GX_CTRL, 4-7 by GX_CTRL_EXT.
 */
#define GX_CTRL_GPIO_DIR	(GX_CTRL_GPIO_DIR_3)
#define GX_CTRL_GPIO_DIR_MASK	(GX_CTRL_GPIO_DIR_0 | GX_CTRL_GPIO_DIR_1 | \
				 GX_CTRL_GPIO_DIR_2 | GX_CTRL_GPIO_DIR_3)
#define GX_CTRL_PHY_IO		GX_CTRL_GPIO_2
#define GX_CTRL_PHY_IO_DIR	GX_CTRL_GPIO_DIR_2
#define GX_CTRL_PHY_CLK		GX_CTRL_GPIO_3

#define GX_CTRLX_GPIO_DIR	(GX_CTRLX_GPIO_DIR_4)
#define GX_CTRLX_GPIO_DIR_MASK	(GX_CTRLX_GPIO_DIR_4 | GX_CTRLX_GPIO_DIR_5 | \
				 GX_CTRLX_GPIO_DIR_6 | GX_CTRLX_GPIO_DIR_7)
#define GX_CTRLX_PHY_RESET	GX_CTRLX_GPIO_4

#define GX_PHY_PREAMBLE		0xffffffff
#define GX_PHY_PREAMBLE_LEN	32
#define GX_PHY_SOF		0x01
#define GX_PHY_TURNAROUND	0x02
#define GX_PHY_OP_WRITE	0x01
#define GX_PHY_OP_READ		0x02
#define GX_PHY_READ_LEN		14
#define GX_PHY_WRITE_LEN	32

/* GX_RX_ADDR registers */
#define GX_RA_VALID		0x80000000

/* GX_TX_CONFIG register */
#define GX_TXCFG_AUTONEG	0x80000000
#define GX_TXCFG_SWCONFIG	0x80000000

/* GX_RX_CONFIG register */
#define GX_RXCFG_INVALID	0x08000000

/* GX_RX_CONTROL register */
#define GX_RXC_RESET		0x00000001
#define GX_RXC_ENABLE		0x00000002
#define GX_RXC_STORE_BAD_PKT	0x00000004
#define GX_RXC_UNI_PROMISC	0x00000008
#define GX_RXC_MULTI_PROMISC	0x00000010
#define GX_RXC_LONG_PKT_ENABLE	0x00000020
#define GX_RXC_LOOPBACK		0x000000C0
#define GX_RXC_RX_THOLD_MASK	0x00000300
#define GX_RXC_MCAST_OFF_MASK	0x00003000
#define GX_RXC_BCAST_ACCEPT	0x00008000
#define GX_RXC_RX_BSIZE_MASK	0x00030000
#define GX_RXC_VLAN_ENABLE	0x00040000
#define GX_RXC_CFI_ENABLE	0x00080000	/* canonical form enable */
#define GX_RXC_CFI		0x00100000
#define GX_RXC_DISCARD_PAUSE	0x00400000
#define GX_RXC_PASS_MAC		0x00800000
#define GX_RXC_RX_BSIZE_SCALE	0x02000000	/* multiply BSIZE by 16 */
#define GX_RXC_STRIP_ETHERCRC	0x04000000

/* bits for GX_RXC_RX_THOLD */
#define GX_RXC_RX_THOLD_HALF	0x00000000
#define GX_RXC_RX_THOLD_QUARTER	0x00000100
#define GX_RXC_RX_THOLD_EIGHTH	0x00000200

/* bits for GX_RXC_RX_BSIZE_MASK */
#define GX_RXC_RX_BSIZE_2K	0x00000000
#define GX_RXC_RX_BSIZE_1K	0x00010000
#define GX_RXC_RX_BSIZE_512	0x00020000
#define GX_RXC_RX_BSIZE_256	0x00030000

/* GX_TX_CONTROL register */
#define GX_TXC_RESET		0x00000001
#define GX_TXC_ENABLE		0x00000002
#define GX_TXC_PAD_SHORT_PKTS	0x00000008
#define GX_TXC_COLL_RETRY_MASK	0x00000FF0
#define GX_TXC_COLL_TIME_MASK	0x003FF000
#define GX_TXC_XMIT_XOFF	0x00400000
#define GX_TXC_PKT_BURST_ENABLE	0x00800000
#define GX_TXC_REXMT_LATE_COLL	0x01000000
#define GX_TXC_NO_REXMT_UNDERRN	0x02000000

/* bits for GX_TXC_COLL_RETRY_MASK */
#define GX_TXC_COLL_RETRY_16	0x000000F0	/* 16 attempts at retransmit */

/* bits for GX_TXC_COLL_TIME_MASK */
#define GX_TXC_COLL_TIME_HDX	0x00200000
#define GX_TXC_COLL_TIME_FDX	0x00040000

/* GX_INT bits */
#define GX_INT_XMIT_DONE	0x00000001
#define GX_INT_XMIT_EMPTY	0x00000002
#define GX_INT_LINK_CHANGE	0x00000004
#define GX_INT_RCV_SEQ_ERR	0x00000008
#define GX_INT_RCV_THOLD	0x00000010
#define GX_INT_RCV_OVERRUN	0x00000040
#define GX_INT_RCV_TIMER	0x00000080
#define GX_INT_MDIO_DONE	0x00000200
#define GX_INT_C_SETS		0x00000400
#define GX_INT_GPI_MASK		0x00007800

#define GX_INT_ALL \
	(GX_INT_XMIT_DONE | GX_INT_XMIT_EMPTY | GX_INT_LINK_CHANGE | \
	GX_INT_RCV_SEQ_ERR | GX_INT_RCV_THOLD | GX_INT_RCV_OVERRUN | \
	GX_INT_RCV_TIMER | GX_INT_MDIO_DONE | GX_INT_C_SETS | GX_INT_GPI_MASK)

#if 0
#define GX_INT_WANTED \
	(GX_INT_XMIT_DONE | /*GX_INT_XMIT_EMPTY |*/ GX_INT_LINK_CHANGE | \
	GX_INT_RCV_SEQ_ERR | GX_INT_RCV_THOLD | GX_INT_RCV_OVERRUN | \
	GX_INT_RCV_TIMER | GX_INT_C_SETS)
#else
#define GX_INT_WANTED \
	(GX_INT_XMIT_DONE | GX_INT_RCV_THOLD | GX_INT_RCV_TIMER | \
	GX_INT_LINK_CHANGE)
#endif

/* PCI space */
#define GX_PCI_VENDOR_ID	0x0000
#define GX_PCI_DEVICE_ID	0x0002
#define GX_PCI_COMMAND		0x0004
#define GX_PCI_STATUS		0x0006
#define GX_PCI_REVID		0x0008
#define GX_PCI_CLASSCODE	0x0009
#define GX_PCI_CACHELEN		0x000C
#define GX_PCI_LATENCY_TIMER	0x000D
#define GX_PCI_HEADER_TYPE	0x000E
#define GX_PCI_LOMEM		0x0010
#define GX_PCI_SUBVEN_ID	0x002C
#define GX_PCI_SYBSYS_ID	0x002E
#define GX_PCI_BIOSROM		0x0030
#define GX_PCI_CAPABILITY_PTR	0x0034
#define GX_PCI_INTLINE		0x003C
#define GX_PCI_INTPIN		0x003D
#define GX_PCI_MINGNT		0x003E
#define GX_PCI_MINLAT		0x003F

/* generic TX descriptor */
struct gx_tx_desc {
	u_int64_t	:64;
	u_int16_t	:16;
	u_int8_t	:4,
			tx_type:4,
	u_int8_t	:5,
			tx_extended:1,
			:2;
	u_int32_t	:32;
};

/* legacy TX descriptor */
struct gx_tx_desc_old {
	u_int64_t	tx_addr;
	u_int16_t	tx_len;
	u_int8_t	tx_csum_offset;
	u_int8_t	tx_command;
	u_int8_t	tx_status;
	u_int8_t	tx_csum_start;
	u_int16_t	tx_vlan;
};

#define GX_TXOLD_END_OF_PKT	0x01	/* end of packet */
#define GX_TXOLD_ETHER_CRC	0x02	/* insert ethernet CRC */
#define GX_TXOLD_INSERT_CSUM	0x04	/* insert checksum */
#define GX_TXOLD_REPORT_STATUS	0x08	/* report packet status */
#define GX_TXOLD_REPORT_SENT	0x10	/* report packet sent */
#define GX_TXOLD_EXTENSION	0x20	/* extended format */
#define GX_TXOLD_VLAN_ENABLE	0x40	/* use vlan */
#define GX_TXOLD_INT_DELAY	0x80	/* delay interrupt */

/* bits for tx_status */
#define GX_TXSTAT_DONE		0x01	/* descriptor done */
#define GX_TXSTAT_EXCESS_COLL	0x02	/* excess collisions */
#define GX_TXSTAT_LATE_COLL	0x04	/* late collision */
#define GX_TXSTAT_UNDERRUN	0x08	/* transmit underrun */

/* TX descriptor for checksum offloading context */
struct gx_tx_desc_ctx {
	u_int8_t	tx_ip_csum_start;
	u_int8_t	tx_ip_csum_offset;
	u_int16_t	tx_ip_csum_end;
	u_int8_t	tx_tcp_csum_start;
	u_int8_t	tx_tcp_csum_offset;
	u_int16_t	tx_tcp_csum_end;
	u_int32_t	tx_len:20,
			tx_type:4,
			tx_command:8;
	u_int8_t	tx_status;
	u_int8_t	tx_hdrlen;
	u_int16_t	tx_mss;
};

#define GX_TXCTX_TCP_PKT	0x01	/* its a TCP packet */
#define GX_TXCTX_IP_PKT		0x02	/* its an IP packet */
#define GX_TXCTX_TCP_SEG_EN	0x04	/* TCP segmentation enable */
#define GX_TXCTX_REPORT_STATUS	0x08	/* report packet status */
#define GX_TXCTX_EXTENSION	0x20	/* extended format */
#define GX_TXCTX_INT_DELAY	0x80	/* delay interrupt */

/* TX descriptor for data */
struct gx_tx_desc_data {
	u_int64_t	tx_addr;
	u_int32_t	tx_len:20,
			tx_type:4,
			tx_command:8;
	u_int8_t	tx_status;
	u_int8_t	tx_options;
	u_int16_t	tx_vlan;
};

#define GX_TXTCP_END_OF_PKT	0x01	/* end of packet */
#define GX_TXTCP_ETHER_CRC	0x02	/* insert ethernet CRC */
#define GX_TXTCP_TCP_SEG_EN	0x04	/* TCP segmentation enable */
#define GX_TXTCP_REPORT_STATUS	0x08	/* report packet status */
#define GX_TXTCP_REPORT_SENT	0x10	/* report packet sent */
#define GX_TXTCP_EXTENSION	0x20	/* extended format */
#define GX_TXTCP_VLAN_ENABLE	0x40	/* use vlan */
#define GX_TXTCP_INT_DELAY	0x80	/* delay interrupt */

/* bits for tx_options */
#define GX_TXTCP_OPT_IP_CSUM	0x01	/* insert IP checksum */
#define GX_TXTCP_OPT_TCP_CSUM	0x02	/* insert UDP/TCP checksum */

/* RX descriptor data structure */
struct gx_rx_desc {
	u_int64_t	rx_addr;
	u_int16_t	rx_len;
	u_int16_t	rx_csum;
	u_int16_t	rx_staterr;	/* status + error fields */
	u_int16_t	rx_special;
};

/* bits for rx_status portion of rx_staterr */
#define GX_RXSTAT_COMPLETED	0x01	/* completed */
#define GX_RXSTAT_END_OF_PACKET	0x02	/* end of this packet */
#define GX_RXSTAT_IGNORE_CSUM	0x04	/* ignore computed checksum */
#define GX_RXSTAT_VLAN_PKT	0x08	/* matched vlan */
#define GX_RXSTAT_HAS_TCP_CSUM	0x20	/* TCP checksum calculated */
#define GX_RXSTAT_HAS_IP_CSUM	0x40	/* IP checksum calculated */
#define GX_RXSTAT_INEXACT_MATCH	0x80	/* must check address */

/* bits for rx_error portion of rx_staterr */
#define GX_RXERR_CRC		0x0100	/* CRC or alignment error */
#define GX_RXERR_SYMBOL		0x0200	/* symbol error */
#define GX_RXERR_SEQUENCE	0x0400	/* sequence error */
#define GX_RXERR_CARRIER	0x1000	/* carrier extension error */
#define GX_RXERR_TCP_CSUM	0x2000	/* TCP/UDP checksum error */
#define GX_RXERR_IP_CSUM	0x4000	/* IP checksum error */
#define GX_RXERR_RX_DATA	0x8000	/* RX data error */

/* drop packet on these errors */
#define GX_INPUT_ERROR \
	(GX_RXERR_CRC | GX_RXERR_SYMBOL | GX_RXERR_SEQUENCE | \
	GX_RXERR_CARRIER | GX_RXERR_RX_DATA)
