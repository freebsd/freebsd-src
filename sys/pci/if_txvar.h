/*	$OpenBSD: if_txvar.h,v 1.7 1999/11/17 05:21:19 jason Exp $	*/
/* $FreeBSD$ */

/*-
 * Copyright (c) 1997 Semen Ustimenko
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
 */

/*
 * Configuration
 */
/*#define	EPIC_DEBUG	1*/
/*#define	EPIC_USEIOSPACE	1*/
#define	EARLY_RX	1

#ifndef ETHER_MAX_LEN
#define ETHER_MAX_LEN		1518
#endif
#ifndef ETHER_MIN_LEN
#define ETHER_MIN_LEN		64
#endif
#ifndef ETHER_CRC_LEN
#define ETHER_CRC_LEN		4
#endif
#define TX_RING_SIZE		16		/* Leave this a power of 2 */
#define RX_RING_SIZE		16		/* And this too, to do not */
						/* confuse RX(TX)_RING_MASK */
#define TX_RING_MASK		(TX_RING_SIZE - 1)
#define RX_RING_MASK		(RX_RING_SIZE - 1)
#define ETHER_MAX_FRAME_LEN	(ETHER_MAX_LEN + ETHER_CRC_LEN)

/* PCI aux configuration registers */
#if defined(__FreeBSD__)
#define	PCIR_BASEIO	(PCIR_MAPS + 0x0)	/* Base IO Address */
#define	PCIR_BASEMEM	(PCIR_MAPS + 0x4)	/* Base Memory Address */
#else /* __OpenBSD__ */
#define	PCI_BASEIO	(PCI_MAPS + 0x0)	/* Base IO Address */
#define	PCI_BASEMEM	(PCI_MAPS + 0x4)	/* Base Memory Address */
#endif /* __FreeBSD__ */

/* PCI identification */
#define SMC_VENDORID		0x10B8
#define SMC_DEVICEID_83C170	0x0005

/* EPIC's registers */
#define	COMMAND		0x0000
#define	INTSTAT		0x0004		/* Interrupt status. See below */
#define	INTMASK		0x0008		/* Interrupt mask. See below */
#define	GENCTL		0x000C
#define	NVCTL		0x0010
#define	EECTL		0x0014		/* EEPROM control **/
#define	TEST1		0x001C		/* XXXXX */
#define	CRCCNT		0x0020		/* CRC error counter */
#define	ALICNT		0x0024		/* FrameTooLang error counter */
#define	MPCNT		0x0028		/* MissedFrames error counters */
#define	MIICTL		0x0030
#define	MIIDATA		0x0034
#define	MIICFG		0x0038
#define IPG		0x003C
#define	LAN0		0x0040		/* MAC address */
#define	LAN1		0x0044		/* MAC address */
#define	LAN2		0x0048		/* MAC address */
#define	ID_CHK		0x004C
#define	MC0		0x0050		/* Multicast filter table */
#define	MC1		0x0054		/* Multicast filter table */
#define	MC2		0x0058		/* Multicast filter table */
#define	MC3		0x005C		/* Multicast filter table */
#define	RXCON		0x0060		/* Rx control register */
#define	TXCON		0x0070		/* Tx control register */
#define	TXSTAT		0x0074
#define	PRCDAR		0x0084		/* RxRing bus address */
#define	PRSTAT		0x00A4
#define	PRCPTHR		0x00B0
#define	PTCDAR		0x00C4		/* TxRing bus address */
#define	ETXTHR		0x00DC

#define	COMMAND_STOP_RX		0x01
#define	COMMAND_START_RX	0x02
#define	COMMAND_TXQUEUED	0x04
#define	COMMAND_RXQUEUED	0x08
#define	COMMAND_NEXTFRAME	0x10
#define	COMMAND_STOP_TDMA	0x20
#define	COMMAND_STOP_RDMA	0x40
#define	COMMAND_TXUGO		0x80

/* Interrupt register bits */
#define INTSTAT_RCC	0x00000001
#define INTSTAT_HCC	0x00000002
#define INTSTAT_RQE	0x00000004
#define INTSTAT_OVW	0x00000008	
#define INTSTAT_RXE	0x00000010	
#define INTSTAT_TXC	0x00000020
#define INTSTAT_TCC	0x00000040	
#define INTSTAT_TQE	0x00000080	
#define INTSTAT_TXU	0x00000100
#define INTSTAT_CNT	0x00000200
#define INTSTAT_PREI	0x00000400
#define INTSTAT_RCT	0x00000800	
#define INTSTAT_FATAL	0x00001000	/* One of DPE,APE,PMA,PTA happend */	
#define INTSTAT_UNUSED1	0x00002000
#define INTSTAT_UNUSED2	0x00004000	
#define INTSTAT_GP2	0x00008000	/* PHY Event */	
#define INTSTAT_INT_ACTV 0x00010000
#define INTSTAT_RXIDLE	0x00020000
#define INTSTAT_TXIDLE	0x00040000
#define INTSTAT_RCIP	0x00080000	
#define INTSTAT_TCIP	0x00100000	
#define INTSTAT_RBE	0x00200000
#define INTSTAT_RCTS	0x00400000	
#define	INTSTAT_RSV	0x00800000
#define	INTSTAT_DPE	0x01000000	/* PCI Fatal error */
#define	INTSTAT_APE	0x02000000	/* PCI Fatal error */
#define	INTSTAT_PMA	0x04000000	/* PCI Fatal error */
#define	INTSTAT_PTA	0x08000000	/* PCI Fatal error */

#define	GENCTL_SOFT_RESET		0x00000001
#define	GENCTL_ENABLE_INTERRUPT		0x00000002
#define	GENCTL_SOFTWARE_INTERRUPT	0x00000004
#define	GENCTL_POWER_DOWN		0x00000008
#define	GENCTL_ONECOPY			0x00000010
#define	GENCTL_BIG_ENDIAN		0x00000020
#define	GENCTL_RECEIVE_DMA_PRIORITY	0x00000040
#define	GENCTL_TRANSMIT_DMA_PRIORITY	0x00000080
#define	GENCTL_RECEIVE_FIFO_THRESHOLD128	0x00000300
#define	GENCTL_RECEIVE_FIFO_THRESHOLD96	0x00000200
#define	GENCTL_RECEIVE_FIFO_THRESHOLD64	0x00000100
#define	GENCTL_RECEIVE_FIFO_THRESHOLD32	0x00000000
#define	GENCTL_MEMORY_READ_LINE		0x00000400
#define	GENCTL_MEMORY_READ_MULTIPLE	0x00000800
#define	GENCTL_SOFTWARE1		0x00001000
#define	GENCTL_SOFTWARE2		0x00002000
#define	GENCTL_RESET_PHY		0x00004000

#define	NVCTL_ENABLE_MEMORY_MAP		0x00000001
#define	NVCTL_CLOCK_RUN_SUPPORTED	0x00000002
#define	NVCTL_GP1_OUTPUT_ENABLE		0x00000004
#define	NVCTL_GP2_OUTPUT_ENABLE		0x00000008
#define	NVCTL_GP1			0x00000010
#define	NVCTL_GP2			0x00000020
#define	NVCTL_CARDBUS_MODE		0x00000040
#define	NVCTL_IPG_DELAY_MASK(x)		((x&0xF)<<7)

#define	RXCON_SAVE_ERRORED_PACKETS	0x00000001
#define	RXCON_RECEIVE_RUNT_FRAMES	0x00000002
#define	RXCON_RECEIVE_BROADCAST_FRAMES	0x00000004
#define	RXCON_RECEIVE_MULTICAST_FRAMES	0x00000008
#define	RXCON_RECEIVE_INVERSE_INDIVIDUAL_ADDRESS_FRAMES	0x00000010
#define	RXCON_PROMISCUOUS_MODE		0x00000020
#define	RXCON_MONITOR_MODE		0x00000040
#define	RXCON_EARLY_RECEIVE_ENABLE	0x00000080
#define	RXCON_EXTERNAL_BUFFER_DISABLE	0x00000000
#define	RXCON_EXTERNAL_BUFFER_16K	0x00000100
#define	RXCON_EXTERNAL_BUFFER_32K	0x00000200
#define	RXCON_EXTERNAL_BUFFER_128K	0x00000300

#define TXCON_EARLY_TRANSMIT_ENABLE	0x00000001
#define TXCON_LOOPBACK_DISABLE		0x00000000
#define TXCON_LOOPBACK_MODE_INT		0x00000002
#define TXCON_LOOPBACK_MODE_PHY		0x00000004
#define TXCON_LOOPBACK_MODE		0x00000006
#define TXCON_FULL_DUPLEX		0x00000006
#define TXCON_SLOT_TIME			0x00000078

#define	MIICFG_SERIAL_ENABLE		0x00000001
#define	MIICFG_694_ENABLE		0x00000002
#define	MIICFG_694_STATUS		0x00000004
#define	MIICFG_PHY_PRESENT		0x00000008
#define	MIICFG_SMI_ENABLE		0x00000010

#define	TEST1_CLOCK_TEST		0x00000008

/*
 * Some default values
 */
#define TXCON_DEFAULT		(TXCON_SLOT_TIME | TXCON_EARLY_TRANSMIT_ENABLE)
#define TRANSMIT_THRESHOLD	0x300

#if defined(EARLY_RX)
#define RXCON_EARLY		(RXCON_EARLY_RECEIVE_ENABLE | \
				 RXCON_SAVE_ERRORED_PACKETS)
#else
#define RXCON_EARLY		(0)
#endif

#define	RXCON_DEFAULT		(RXCON_EARLY | \
				 RXCON_RECEIVE_MULTICAST_FRAMES | \
				 RXCON_RECEIVE_BROADCAST_FRAMES)
/*
 * EEPROM structure
 * SMC9432* eeprom is organized by words and only first 8 words
 * have distinctive meaning (according to datasheet)
 */
#define	EEPROM_MAC0		0x0000	/* Byte 0 / Byte 1 */
#define	EEPROM_MAC1		0x0001	/* Byte 2 / Byte 3 */
#define	EEPROM_MAC2		0x0002	/* Byte 4 / Byte 5 */
#define	EEPROM_BID_CSUM		0x0003	/* Board Id / Check Sum */
#define	EEPROM_NVCTL		0x0004	/* NVCTL (bits 0-5) / nothing */
#define	EEPROM_PCI_MGD_MLD	0x0005	/* PCI MinGrant / MaxLatency. Desired */
#define	EEPROM_SSVENDID		0x0006	/* Subsystem Vendor Id */
#define	EEPROM_SSID		0x0006	/* Subsystem Id */

/*
 * Structures definition and Functions prototypes
 */

/* EPIC's hardware descriptors, must be aligned on dword in memory */
/* NB: to make driver happy, this two structures MUST have thier sizes */
/* be divisor of PAGE_SIZE */
struct epic_tx_desc {
	volatile u_int16_t	status;
	volatile u_int16_t	txlength;
	volatile u_int32_t	bufaddr;
	volatile u_int16_t	buflength;
	volatile u_int16_t	control;
	volatile u_int32_t	next;
};
struct epic_rx_desc {
	volatile u_int16_t	status;
	volatile u_int16_t	rxlength;
	volatile u_int32_t	bufaddr;
	volatile u_int32_t	buflength;
	volatile u_int32_t	next;
};

/* This structure defines EPIC's fragment list, maximum number of frags */
/* is 63. Let use maximum, becouse size of struct MUST be divisor of */
/* PAGE_SIZE, and sometimes come mbufs with more then 30 frags */
#define EPIC_MAX_FRAGS 63
struct epic_frag_list {
	volatile u_int32_t		numfrags;
	struct {
		volatile u_int32_t	fragaddr;
		volatile u_int32_t	fraglen;
	} frag[EPIC_MAX_FRAGS]; 
	volatile u_int32_t		pad;		/* align on 256 bytes */
};

/* This is driver's structure to define EPIC descriptors */
struct epic_rx_buffer {
	struct mbuf *		mbuf;		/* mbuf receiving packet */
};

struct epic_tx_buffer {
	struct mbuf *		mbuf;		/* mbuf contained packet */
};

/*
 * NB: ALIGN OF ABOVE STRUCTURES
 * epic_rx_desc, epic_tx_desc, epic_frag_list - must be aligned on dword
 */

/* PHY, known by tx driver */
#define	EPIC_UNKN_PHY		0x0000
#define	EPIC_QS6612_PHY		0x0001
#define	EPIC_AC101_PHY		0x0002
#define	EPIC_LXT970_PHY		0x0003
#define	EPIC_SERIAL		0x0004

#define	SMC9432DMT		0xA010
#define	SMC9432TX		0xA011
#define	SMC9032TXM		0xA012
#define	SMC9032TX		0xA013
#define	SMC9432TXPWR		0xA014
#define	SMC9432BTX		0xA015
#define	SMC9432FTX		0xA016
#define	SMC9432FTX_SC		0xA017
#define	SMC9432TX_XG_ADHOC	0xA020
#define	SMC9434TX_XG_ADHOC	0xA021
#define	SMC9432FTX_ADHOC	0xA022
#define	SMC9432BTX1		0xA024

/* Driver status structure */
typedef struct {
	struct arpcom		arpcom;
#if defined(__OpenBSD__)
	mii_data_t		sc_mii;
	struct device		dev;
#else /* __FreeBSD__ */
	struct resource		*res;
	struct resource		*irq;

	device_t		miibus;
	device_t		dev;
	struct callout_handle	stat_ch;

	u_int32_t		unit;
#endif
	void			*sc_ih;
	bus_space_tag_t		sc_st;
	bus_space_handle_t	sc_sh;

	struct epic_rx_buffer	rx_buffer[RX_RING_SIZE];
	struct epic_tx_buffer	tx_buffer[TX_RING_SIZE];

	/* Each element of array MUST be aligned on dword  */
	/* and bounded on PAGE_SIZE 			   */
	struct epic_rx_desc	*rx_desc;
	struct epic_tx_desc	*tx_desc;
	struct epic_frag_list	*tx_flist;
	u_int32_t		flags;
	u_int32_t		tx_threshold;
	u_int32_t		txcon;
	u_int32_t		miicfg;
	u_int32_t		cur_tx;
	u_int32_t		cur_rx;
	u_int32_t		dirty_tx;
	u_int32_t		pending_txs;
	u_int16_t		cardvend;
	u_int16_t		cardid;
	struct mii_softc 	*physc;
	u_int32_t		phyid;
	int			serinst;
	void 			*pool;
} epic_softc_t;

struct epic_type {
	u_int16_t	ven_id;
	u_int16_t	dev_id;
	char		*name;
};

#if defined(EPIC_DEBUG)
#define dprintf(a) printf a
#else
#define dprintf(a)
#endif

#if defined(__FreeBSD__)
#define EPIC_FORMAT	"tx%d"
#define EPIC_ARGS(sc)	(sc->unit)
#define EPIC_BPFTAP_ARG(ifp)    ifp
#else /* __OpenBSD__ */
#define EPIC_FORMAT	"%s"
#define EPIC_ARGS(sc)	(sc->sc_dev.dv_xname)
#define EPIC_BPFTAP_ARG(ifp)	(ifp)->if_bpf
#endif

#define sc_if arpcom.ac_if
#define sc_macaddr arpcom.ac_enaddr

#define CSR_WRITE_4(sc,reg,val) 					\
	bus_space_write_4( (sc)->sc_st, (sc)->sc_sh, (reg), (val) )
#define CSR_WRITE_2(sc,reg,val) 					\
	bus_space_write_2( (sc)->sc_st, (sc)->sc_sh, (reg), (val) )
#define CSR_WRITE_1(sc,reg,val) 					\
	bus_space_write_1( (sc)->sc_st, (sc)->sc_sh, (reg), (val) )
#define CSR_READ_4(sc,reg) 						\
	bus_space_read_4( (sc)->sc_st, (sc)->sc_sh, (reg) )
#define CSR_READ_2(sc,reg) 						\
	bus_space_read_2( (sc)->sc_st, (sc)->sc_sh, (reg) )
#define CSR_READ_1(sc,reg) 						\
	bus_space_read_1( (sc)->sc_st, (sc)->sc_sh, (reg) )

#define	PHY_READ_2(sc,phy,reg)						\
	epic_read_phy_reg((sc),(phy),(reg))
#define	PHY_WRITE_2(sc,phy,reg,val)					\
	epic_write_phy_reg((sc),(phy),(reg),(val))

/* Macro to get either mbuf cluster or nothing */
#define EPIC_MGETCLUSTER(m)						\
	{ MGETHDR((m),M_DONTWAIT,MT_DATA);				\
	  if (m) {							\
	    MCLGET((m),M_DONTWAIT);					\
	    if( 0 == ((m)->m_flags & M_EXT) ) {				\
	      m_freem(m);						\
	      (m) = NULL;						\
	    }								\
	  }								\
	}

