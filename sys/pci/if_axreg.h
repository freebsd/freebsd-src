/*
 * Copyright (c) 1997, 1998, 1999
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$Id: if_axreg.h,v 1.2.2.3 1999/04/08 17:45:22 wpaul Exp $
 */

/*
 * ASIX register definitions.
 */

#define AX_BUSCTL		0x00	/* bus control */
#define AX_TXSTART		0x08	/* tx start demand */
#define AX_RXSTART		0x10	/* rx start demand */
#define AX_RXADDR		0x18	/* rx descriptor list start addr */
#define AX_TXADDR		0x20	/* tx descriptor list start addr */
#define AX_ISR			0x28	/* interrupt status register */
#define AX_NETCFG		0x30	/* network config register */
#define AX_IMR			0x38	/* interrupt mask */
#define AX_FRAMESDISCARDED	0x40	/* # of discarded frames */
#define AX_SIO			0x48	/* MII and ROM/EEPROM access */
#define AX_RESERVED		0x50
#define AX_GENTIMER		0x58	/* general timer */
#define AX_GENPORT		0x60	/* general purpose port */
#define AX_FILTIDX		0x68	/* RX filter index */
#define AX_FILTDATA		0x70	/* RX filter data */

/*
 * Bus control bits.
 */
#define AX_BUSCTL_RESET		0x00000001
#define AX_BUSCTL_ARBITRATION	0x00000002
#define AX_BUSCTL_BIGENDIAN	0x00000080
#define AX_BUSCTL_BURSTLEN	0x00003F00
#define AX_BUSCTL_BUF_BIGENDIAN	0x00100000
#define AX_BISCTL_READMULTI	0x00200000

#define AX_BURSTLEN_UNLIMIT	0x00000000
#define AX_BURSTLEN_1LONG	0x00000100
#define AX_BURSTLEN_2LONG	0x00000200
#define AX_BURSTLEN_4LONG	0x00000400
#define AX_BURSTLEN_8LONG	0x00000800
#define AX_BURSTLEN_16LONG	0x00001000
#define AX_BURSTLEN_32LONG	0x00002000

#define AX_BUSCTL_CONFIG	(AX_BUSCTL_ARBITRATION|AX_BURSTLEN_8LONG|AX_BURSTLEN_8LONG)

/*
 * Interrupt status bits.
 */
#define AX_ISR_TX_OK		0x00000001
#define AX_ISR_TX_IDLE		0x00000002
#define AX_ISR_TX_NOBUF		0x00000004
#define AX_ISR_TX_JABBERTIMEO	0x00000008
#define AX_ISR_TX_UNDERRUN	0x00000020
#define AX_ISR_RX_OK		0x00000040
#define AX_ISR_RX_NOBUF		0x00000080
#define AX_ISR_RX_IDLE		0x00000100
#define AX_ISR_RX_WATDOGTIMEO	0x00000200
#define AX_ISR_TX_EARLY		0x00000400
#define AX_ISR_TIMER_EXPIRED	0x00000800
#define AX_ISR_BUS_ERR		0x00002000
#define AX_ISR_ABNORMAL		0x00008000
#define AX_ISR_NORMAL		0x00010000
#define AX_ISR_RX_STATE		0x000E0000
#define AX_ISR_TX_STATE		0x00700000
#define AX_ISR_BUSERRTYPE	0x03800000

#define AX_RXSTATE_STOPPED	0x00000000	/* 000 - Stopped */
#define AX_RXSTATE_FETCH	0x00020000	/* 001 - Fetching descriptor */
#define AX_RXSTATE_ENDCHECK	0x00040000	/* 010 - check for rx end */
#define AX_RXSTATE_WAIT		0x00060000	/* 011 - waiting for packet */
#define AX_RXSTATE_SUSPEND	0x00080000	/* 100 - suspend rx */
#define AX_RXSTATE_CLOSE	0x000A0000	/* 101 - close tx desc */
#define AX_RXSTATE_FLUSH	0x000C0000	/* 110 - flush from FIFO */
#define AX_RXSTATE_DEQUEUE	0x000E0000	/* 111 - dequeue from FIFO */

#define AX_TXSTATE_RESET	0x00000000	/* 000 - reset */
#define AX_TXSTATE_FETCH	0x00100000	/* 001 - fetching descriptor */
#define AX_TXSTATE_WAITEND	0x00200000	/* 010 - wait for tx end */
#define AX_TXSTATE_READING	0x00300000	/* 011 - read and enqueue */
#define AX_TXSTATE_RSVD		0x00400000	/* 100 - reserved */
#define AX_TXSTATE_SETUP	0x00500000	/* 101 - setup packet */
#define AX_TXSTATE_SUSPEND	0x00600000	/* 110 - suspend tx */
#define AX_TXSTATE_CLOSE	0x00700000	/* 111 - close tx desc */

/*
 * Network config bits.
 */
#define AX_NETCFG_LINKSTAT_PCS	0x00000001
#define AX_NETCFG_RX_ON		0x00000002
#define AX_NETCFG_RX_BADFRAMES	0x00000008
#define AX_NETCFG_RX_PROMISC	0x00000040
#define AX_NETCFG_RX_ALLMULTI	0x00000080
#define AX_NETCFG_RX_BROAD	0x00000100
#define AX_NETCFG_FULLDUPLEX	0x00000200
#define AX_NETCFG_LOOPBACK	0x00000C00
#define AX_NETCFG_FORCECOLL	0x00001000
#define AX_NETCFG_TX_ON		0x00002000
#define AX_NETCFG_TX_THRESH	0x0000C000
#define AX_NETCFG_PORTSEL	0x00040000	/* 0 == SRL, 1 == MII/SYM */
#define AX_NETCFG_HEARTBEAT	0x00080000	/* 0 == ON, 1 == OFF */
#define AX_NETCFG_STORENFWD	0x00200000
#define AX_NETCFG_SPEEDSEL	0x00400000	/* 1 == 10, 0 == 100 */
#define AX_NETCFG_PCS		0x00800000
#define AX_NETCFG_SCRAMBLER	0x01000000
#define AX_NETCFG_RX_ALL	0x40000000

#define AX_OPMODE_NORM		0x00000000
#define AX_OPMODE_INTLOOP	0x00000400
#define AX_OPMODE_EXTLOOP	0x00000800

#define AX_TXTHRESH_72BYTES	0x00000000
#define AX_TXTHRESH_96BYTES	0x00004000
#define AX_TXTHRESH_128BYTES	0x00008000
#define AX_TXTHRESH_160BYTES	0x0000C000

/*
 * Interrupt mask bits.
 */
#define AX_IMR_TX_OK		0x00000001
#define AX_IMR_TX_IDLE		0x00000002
#define AX_IMR_TX_NOBUF		0x00000004
#define AX_IMR_TX_JABBERTIMEO	0x00000008
#define AX_IMR_TX_UNDERRUN	0x00000020
#define AX_IMR_RX_OK		0x00000040
#define AX_IMR_RX_NOBUF		0x00000080
#define AX_IMR_RX_IDLE		0x00000100
#define AX_IMR_RX_WATDOGTIMEO	0x00000200
#define AX_IMR_TX_EARLY		0x00000400
#define AX_IMR_TIMER_EXPIRED	0x00000800
#define AX_IMR_BUS_ERR		0x00002000
#define AX_IMR_RX_EARLY		0x00004000
#define AX_IMR_ABNORMAL		0x00008000
#define AX_IMR_NORMAL		0x00010000

#define AX_INTRS	\
	(AX_IMR_RX_OK|AX_IMR_TX_OK|AX_IMR_RX_NOBUF|AX_IMR_RX_WATDOGTIMEO|\
	AX_IMR_TX_NOBUF|AX_IMR_TX_UNDERRUN|AX_IMR_BUS_ERR|		\
	AX_IMR_ABNORMAL|AX_IMR_NORMAL|/*AX_IMR_TX_EARLY*/		\
	AX_IMR_TX_IDLE|AX_IMR_RX_IDLE)

/*
 * Serial I/O (EEPROM/ROM) bits.
 */
#define AX_SIO_EE_CS		0x00000001	/* EEPROM chip select */
#define AX_SIO_EE_CLK		0x00000002	/* EEPROM clock */
#define AX_SIO_EE_DATAIN	0x00000004	/* EEPROM data output */
#define AX_SIO_EE_DATAOUT	0x00000008	/* EEPROM data input */
#define AX_SIO_EESEL		0x00000800
#define AX_SIO_ROMSEL		0x00001000
#define AX_SIO_ROMCTL_WRITE	0x00002000
#define AX_SIO_ROMCTL_READ	0x00004000
#define AX_SIO_MII_CLK		0x00010000	/* MDIO clock */
#define AX_SIO_MII_DATAOUT	0x00020000	/* MDIO data out */
#define AX_SIO_MII_DIR		0x00040000	/* MDIO dir */
#define AX_SIO_MII_DATAIN	0x00080000	/* MDIO data in */

#define AX_EECMD_WRITE		0x140
#define AX_EECMD_READ		0x180
#define AX_EECMD_ERASE		0x1c0

#define AX_EE_NODEADDR_OFFSET	0x70
#define AX_EE_NODEADDR		10

/*
 * General purpose timer register
 */
#define AX_TIMER_VALUE		0x0000FFFF
#define AX_TIMER_CONTINUOUS	0x00010000

/*
 * RX Filter Index Register values
 */
#define AX_FILTIDX_PAR0		0x00000000
#define AX_FILTIDX_PAR1		0x00000001
#define AX_FILTIDX_MAR0		0x00000002
#define AX_FILTIDX_MAR1		0x00000003

/*
 * ASIX TX/RX list structure.
 */

struct ax_desc {
	volatile u_int32_t	ax_status;
	volatile u_int32_t	ax_ctl;
	volatile u_int32_t	ax_ptr1;
	volatile u_int32_t	ax_ptr2;
};

#define ax_data		ax_ptr1
#define ax_next		ax_ptr2

#define AX_RXSTAT_FIFOOFLOW	0x00000001
#define AX_RXSTAT_CRCERR	0x00000002
#define AX_RXSTAT_DRIBBLE	0x00000004
#define AX_RXSTAT_WATCHDOG	0x00000010
#define AX_RXSTAT_FRAMETYPE	0x00000020	/* 0 == IEEE 802.3 */
#define AX_RXSTAT_COLLSEEN	0x00000040
#define AX_RXSTAT_GIANT		0x00000080
#define AX_RXSTAT_LASTFRAG	0x00000100
#define AX_RXSTAT_FIRSTFRAG	0x00000200
#define AX_RXSTAT_MULTICAST	0x00000400
#define AX_RXSTAT_RUNT		0x00000800
#define AX_RXSTAT_RXTYPE	0x00003000
#define AX_RXSTAT_RXERR		0x00008000
#define AX_RXSTAT_RXLEN		0x3FFF0000
#define AX_RXSTAT_OWN		0x80000000

#define AX_RXBYTES(x)		((x & AX_RXSTAT_RXLEN) >> 16)
#define AX_RXSTAT (AX_RXSTAT_FIRSTFRAG|AX_RXSTAT_LASTFRAG|AX_RXSTAT_OWN)

#define AX_RXCTL_BUFLEN1	0x00000FFF
#define AX_RXCTL_BUFLEN2	0x00FFF000
#define AX_RXCTL_RLAST		0x02000000

#define AX_TXSTAT_DEFER		0x00000001
#define AX_TXSTAT_UNDERRUN	0x00000002
#define AX_TXSTAT_LINKFAIL	0x00000003
#define AX_TXSTAT_COLLCNT	0x00000078
#define AX_TXSTAT_SQE		0x00000080
#define AX_TXSTAT_EXCESSCOLL	0x00000100
#define AX_TXSTAT_LATECOLL	0x00000200
#define AX_TXSTAT_NOCARRIER	0x00000400
#define AX_TXSTAT_CARRLOST	0x00000800
#define AX_TXSTAT_JABTIMEO	0x00004000
#define AX_TXSTAT_ERRSUM	0x00008000
#define AX_TXSTAT_OWN		0x80000000

#define AX_TXCTL_BUFLEN1	0x000007FF
#define AX_TXCTL_BUFLEN2	0x003FF800
#define AX_TXCTL_PAD		0x00800000
#define AX_TXCTL_TLAST		0x02000000
#define AX_TXCTL_NOCRC		0x04000000
#define AX_TXCTL_FIRSTFRAG	0x20000000
#define AX_TXCTL_LASTFRAG	0x40000000
#define AX_TXCTL_FINT		0x80000000

#define AX_MAXFRAGS		16
#define AX_RX_LIST_CNT		64
#define AX_TX_LIST_CNT		128
#define AX_MIN_FRAMELEN		60

/*
 * A tx 'super descriptor' is actually 16 regular descriptors
 * back to back.
 */
struct ax_txdesc {
	volatile struct ax_desc	ax_frag[AX_MAXFRAGS];
};

#define AX_TXNEXT(x)	x->ax_ptr->ax_frag[x->ax_lastdesc].ax_next
#define AX_TXSTATUS(x)	x->ax_ptr->ax_frag[x->ax_lastdesc].ax_status
#define AX_TXCTL(x)	x->ax_ptr->ax_frag[x->ax_lastdesc].ax_ctl
#define AX_TXDATA(x)	x->ax_ptr->ax_frag[x->ax_lastdesc].ax_data

#define AX_TXOWN(x)	x->ax_ptr->ax_frag[0].ax_status

#define AX_UNSENT	0x12341234

struct ax_list_data {
	volatile struct ax_desc		ax_rx_list[AX_RX_LIST_CNT];
	volatile struct ax_txdesc	ax_tx_list[AX_TX_LIST_CNT];
};

struct ax_chain {
	volatile struct ax_txdesc	*ax_ptr;
	struct mbuf			*ax_mbuf;
	struct ax_chain			*ax_nextdesc;
	u_int8_t			ax_lastdesc;
};

struct ax_chain_onefrag {
	volatile struct ax_desc	*ax_ptr;
	struct mbuf		*ax_mbuf;
	struct ax_chain_onefrag	*ax_nextdesc;
};

struct ax_chain_data {
	struct ax_chain_onefrag	ax_rx_chain[AX_RX_LIST_CNT];
	struct ax_chain		ax_tx_chain[AX_TX_LIST_CNT];

	struct ax_chain_onefrag	*ax_rx_head;

	struct ax_chain		*ax_tx_head;
	struct ax_chain		*ax_tx_tail;
	struct ax_chain		*ax_tx_free;
};

struct ax_type {
	u_int16_t		ax_vid;
	u_int16_t		ax_did;
	char			*ax_name;
};

struct ax_mii_frame {
	u_int8_t		mii_stdelim;
	u_int8_t		mii_opcode;
	u_int8_t		mii_phyaddr;
	u_int8_t		mii_regaddr;
	u_int8_t		mii_turnaround;
	u_int16_t		mii_data;
};

/*
 * MII constants
 */
#define AX_MII_STARTDELIM	0x01
#define AX_MII_READOP		0x02
#define AX_MII_WRITEOP		0x01
#define AX_MII_TURNAROUND	0x02

#define AX_FLAG_FORCEDELAY	1
#define AX_FLAG_SCHEDDELAY	2
#define AX_FLAG_DELAYTIMEO	3	

struct ax_softc {
	struct arpcom		arpcom;		/* interface info */
	struct ifmedia		ifmedia;	/* media info */
	bus_space_handle_t	ax_bhandle;	/* bus space handle */
	bus_space_tag_t		ax_btag;	/* bus space tag */
	struct ax_type		*ax_info;	/* ASIX adapter info */
	struct ax_type		*ax_pinfo;	/* phy info */
	u_int8_t		ax_unit;	/* interface number */
	u_int8_t		ax_type;
	u_int8_t		ax_phy_addr;	/* PHY address */
	u_int8_t		ax_tx_pend;	/* TX pending */
	u_int8_t		ax_want_auto;
	u_int8_t		ax_autoneg;
	caddr_t			ax_ldata_ptr;
	struct ax_list_data	*ax_ldata;
	struct ax_chain_data	ax_cdata;
};

/*
 * register space access macros
 */
#define CSR_WRITE_4(sc, reg, val)	\
	bus_space_write_4(sc->ax_btag, sc->ax_bhandle, reg, val)
#define CSR_WRITE_2(sc, reg, val)	\
	bus_space_write_2(sc->ax_btag, sc->ax_bbhandle, reg, val)
#define CSR_WRITE_1(sc, reg, val)	\
	bus_space_write_1(sc->ax_btag, sc->ax_bhandle, reg, val)

#define CSR_READ_4(sc, reg)		\
	bus_space_read_4(sc->ax_btag, sc->ax_bhandle, reg)
#define CSR_READ_2(sc, reg)		\
	bus_space_read_2(sc->ax_btag, sc->ax_bhandle, reg)
#define CSR_READ_1(sc, reg)		\
	bus_space_read_1(sc->ax_btag, sc->ax_bhandle, reg)

#define AX_TIMEOUT		1000

/*
 * General constants that are fun to know.
 *
 * ASIX PCI vendor ID
 */
#define	AX_VENDORID		0x125B

/*
 * ASIX device IDs.
 */
#define AX_DEVICEID_AX88140A	0x1400

/*
 * The ASIX AX88140 and ASIX AX88141 have the same vendor and
 * device IDs but different revision values.
 */
#define AX_REVISION_88140	0x00
#define AX_REVISION_88141	0x10

/*
 * Texas Instruments PHY identifiers
 */
#define TI_PHY_VENDORID		0x4000
#define TI_PHY_10BT		0x501F
#define TI_PHY_100VGPMI		0x502F

/*
 * These ID values are for the NS DP83840A 10/100 PHY
 */
#define NS_PHY_VENDORID		0x2000
#define NS_PHY_83840A		0x5C0F

/*
 * Level 1 10/100 PHY
 */
#define LEVEL1_PHY_VENDORID	0x7810
#define LEVEL1_PHY_LXT970	0x000F

/*
 * Intel 82555 10/100 PHY
 */
#define INTEL_PHY_VENDORID	0x0A28
#define INTEL_PHY_82555		0x015F

/*
 * SEEQ 80220 10/100 PHY
 */
#define SEEQ_PHY_VENDORID	0x0016
#define SEEQ_PHY_80220		0xF83F


/*
 * PCI low memory base and low I/O base register, and
 * other PCI registers.
 */

#define AX_PCI_VENDOR_ID	0x00
#define AX_PCI_DEVICE_ID	0x02
#define AX_PCI_COMMAND		0x04
#define AX_PCI_STATUS		0x06
#define AX_PCI_REVID		0x08
#define AX_PCI_CLASSCODE	0x09
#define AX_PCI_LATENCY_TIMER	0x0D
#define AX_PCI_HEADER_TYPE	0x0E
#define AX_PCI_LOIO		0x10
#define AX_PCI_LOMEM		0x14
#define AX_PCI_BIOSROM		0x30
#define AX_PCI_INTLINE		0x3C
#define AX_PCI_INTPIN		0x3D
#define AX_PCI_MINGNT		0x3E
#define AX_PCI_MINLAT		0x0F
#define AX_PCI_RESETOPT		0x48
#define AX_PCI_EEPROM_DATA	0x4C

/* power management registers */
#define AX_PCI_CAPID		0x44 /* 8 bits */
#define AX_PCI_NEXTPTR		0x45 /* 8 bits */
#define AX_PCI_PWRMGMTCAP	0x46 /* 16 bits */
#define AX_PCI_PWRMGMTCTRL	0x48 /* 16 bits */

#define AX_PSTATE_MASK		0x0003
#define AX_PSTATE_D0		0x0000
#define AX_PSTATE_D1		0x0001
#define AX_PSTATE_D2		0x0002
#define AX_PSTATE_D3		0x0003
#define AX_PME_EN		0x0010
#define AX_PME_STATUS		0x8000

#define PHY_UNKNOWN		6

#define AX_PHYADDR_MIN		0x00
#define AX_PHYADDR_MAX		0x1F

#define PHY_BMCR		0x00
#define PHY_BMSR		0x01
#define PHY_VENID		0x02
#define PHY_DEVID		0x03
#define PHY_ANAR		0x04
#define PHY_LPAR		0x05
#define PHY_ANEXP		0x06

#define PHY_ANAR_NEXTPAGE	0x8000
#define PHY_ANAR_RSVD0		0x4000
#define PHY_ANAR_TLRFLT		0x2000
#define PHY_ANAR_RSVD1		0x1000
#define PHY_ANAR_RSVD2		0x0800
#define PHY_ANAR_RSVD3		0x0400
#define PHY_ANAR_100BT4		0x0200
#define PHY_ANAR_100BTXFULL	0x0100
#define PHY_ANAR_100BTXHALF	0x0080
#define PHY_ANAR_10BTFULL	0x0040
#define PHY_ANAR_10BTHALF	0x0020
#define PHY_ANAR_PROTO4		0x0010
#define PHY_ANAR_PROTO3		0x0008
#define PHY_ANAR_PROTO2		0x0004
#define PHY_ANAR_PROTO1		0x0002
#define PHY_ANAR_PROTO0		0x0001

/*
 * These are the register definitions for the PHY (physical layer
 * interface chip).
 */
/*
 * PHY BMCR Basic Mode Control Register
 */
#define PHY_BMCR_RESET			0x8000
#define PHY_BMCR_LOOPBK			0x4000
#define PHY_BMCR_SPEEDSEL		0x2000
#define PHY_BMCR_AUTONEGENBL		0x1000
#define PHY_BMCR_RSVD0			0x0800	/* write as zero */
#define PHY_BMCR_ISOLATE		0x0400
#define PHY_BMCR_AUTONEGRSTR		0x0200
#define PHY_BMCR_DUPLEX			0x0100
#define PHY_BMCR_COLLTEST		0x0080
#define PHY_BMCR_RSVD1			0x0040	/* write as zero, don't care */
#define PHY_BMCR_RSVD2			0x0020	/* write as zero, don't care */
#define PHY_BMCR_RSVD3			0x0010	/* write as zero, don't care */
#define PHY_BMCR_RSVD4			0x0008	/* write as zero, don't care */
#define PHY_BMCR_RSVD5			0x0004	/* write as zero, don't care */
#define PHY_BMCR_RSVD6			0x0002	/* write as zero, don't care */
#define PHY_BMCR_RSVD7			0x0001	/* write as zero, don't care */
/*
 * RESET: 1 == software reset, 0 == normal operation
 * Resets status and control registers to default values.
 * Relatches all hardware config values.
 *
 * LOOPBK: 1 == loopback operation enabled, 0 == normal operation
 *
 * SPEEDSEL: 1 == 100Mb/s, 0 == 10Mb/s
 * Link speed is selected byt his bit or if auto-negotiation if bit
 * 12 (AUTONEGENBL) is set (in which case the value of this register
 * is ignored).
 *
 * AUTONEGENBL: 1 == Autonegotiation enabled, 0 == Autonegotiation disabled
 * Bits 8 and 13 are ignored when autoneg is set, otherwise bits 8 and 13
 * determine speed and mode. Should be cleared and then set if PHY configured
 * for no autoneg on startup.
 *
 * ISOLATE: 1 == isolate PHY from MII, 0 == normal operation
 *
 * AUTONEGRSTR: 1 == restart autonegotiation, 0 = normal operation
 *
 * DUPLEX: 1 == full duplex mode, 0 == half duplex mode
 *
 * COLLTEST: 1 == collision test enabled, 0 == normal operation
 */

/* 
 * PHY, BMSR Basic Mode Status Register 
 */   
#define PHY_BMSR_100BT4			0x8000
#define PHY_BMSR_100BTXFULL		0x4000
#define PHY_BMSR_100BTXHALF		0x2000
#define PHY_BMSR_10BTFULL		0x1000
#define PHY_BMSR_10BTHALF		0x0800
#define PHY_BMSR_RSVD1			0x0400	/* write as zero, don't care */
#define PHY_BMSR_RSVD2			0x0200	/* write as zero, don't care */
#define PHY_BMSR_RSVD3			0x0100	/* write as zero, don't care */
#define PHY_BMSR_RSVD4			0x0080	/* write as zero, don't care */
#define PHY_BMSR_MFPRESUP		0x0040
#define PHY_BMSR_AUTONEGCOMP		0x0020
#define PHY_BMSR_REMFAULT		0x0010
#define PHY_BMSR_CANAUTONEG		0x0008
#define PHY_BMSR_LINKSTAT		0x0004
#define PHY_BMSR_JABBER			0x0002
#define PHY_BMSR_EXTENDED		0x0001

#ifdef __alpha__
#undef vtophys
#define vtophys(va)		(pmap_kextract(((vm_offset_t) (va))) \
					+ 1*1024*1024*1024)
#endif
