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
 * $FreeBSD$
 */

/*
 * Davicom register definitions.
 */

#define DM_BUSCTL		0x00	/* bus control */
#define DM_TXSTART		0x08	/* tx start demand */
#define DM_RXSTART		0x10	/* rx start demand */
#define DM_RXADDR		0x18	/* rx descriptor list start addr */
#define DM_TXADDR		0x20	/* tx descriptor list start addr */
#define DM_ISR			0x28	/* interrupt status register */
#define DM_NETCFG		0x30	/* network config register */
#define DM_IMR			0x38	/* interrupt mask */
#define DM_FRAMESDISCARDED	0x40	/* # of discarded frames */
#define DM_SIO			0x48	/* MII and ROM/EEPROM access */
#define DM_RESERVED		0x50
#define DM_GENTIMER		0x58	/* general timer */
#define DM_GENPORT		0x60	/* general purpose port */

/*
 * Bus control bits.
 */
#define DM_BUSCTL_RESET		0x00000001
#define DM_BUSCTL_ARBITRATION	0x00000002
#define WB_BUSCTL_SKIPLEN	0x0000007C
#define DM_BUSCTL_BIGENDIAN	0x00000080
#define DM_BUSCTL_BURSTLEN	0x00003F00
#define DM_BUSCTL_CACHEALIGN	0x0000C000
#define DM_BUSCTL_BUF_BIGENDIAN	0x00100000
#define DM_BUSCTL_READMULTI	0x00200000

#define DM_SKIPLEN_1LONG	0x00000004
#define DM_SKIPLEN_2LONG	0x00000008
#define DM_SKIPLEN_3LONG	0x00000010
#define DM_SKIPLEN_4LONG	0x00000020
#define DM_SKIPLEN_5LONG	0x00000040

#define DM_CACHEALIGN_NONE	0x00000000
#define DM_CACHEALIGN_8LONG	0x00004000
#define DM_CACHEALIGN_16LONG	0x00008000
#define DM_CACHEALIGN_32LONG	0x0000C000

#define DM_BURSTLEN_UNLIMIT	0x00000000
#define DM_BURSTLEN_1LONG	0x00000100
#define DM_BURSTLEN_2LONG	0x00000200
#define DM_BURSTLEN_4LONG	0x00000400
#define DM_BURSTLEN_8LONG	0x00000800
#define DM_BURSTLEN_16LONG	0x00001000
#define DM_BURSTLEN_32LONG	0x00002000

/*
 * Interrupt status bits.
 */
#define DM_ISR_TX_OK		0x00000001
#define DM_ISR_TX_IDLE		0x00000002
#define DM_ISR_TX_NOBUF		0x00000004
#define DM_ISR_TX_JABBERTIMEO	0x00000008
#define DM_ISR_TX_UNDERRUN	0x00000020
#define DM_ISR_RX_OK		0x00000040
#define DM_ISR_RX_NOBUF		0x00000080
#define DM_ISR_RX_IDLE		0x00000100
#define DM_ISR_RX_WATDOGTIMEO	0x00000200
#define DM_ISR_TX_EARLY		0x00000400
#define DM_ISR_TIMER_EXPIRED	0x00000800
#define DM_ISR_BUS_ERR		0x00002000
#define DM_ISR_ABNORMAL		0x00008000
#define DM_ISR_NORMAL		0x00010000
#define DM_ISR_RX_STATE		0x000E0000
#define DM_ISR_TX_STATE		0x00700000
#define DM_ISR_BUSERRTYPE	0x03800000

#define DM_RXSTATE_STOPPED	0x00000000	/* 000 - Stopped */
#define DM_RXSTATE_FETCH	0x00020000	/* 001 - Fetching descriptor */
#define DM_RXSTATE_ENDCHECK	0x00040000	/* 010 - check for rx end */
#define DM_RXSTATE_WAIT		0x00060000	/* 011 - waiting for packet */
#define DM_RXSTATE_SUSPEND	0x00080000	/* 100 - suspend rx */
#define DM_RXSTATE_CLOSE	0x000A0000	/* 101 - close tx desc */
#define DM_RXSTATE_FLUSH	0x000C0000	/* 110 - flush from FIFO */
#define DM_RXSTATE_DEQUEUE	0x000E0000	/* 111 - dequeue from FIFO */

#define DM_TXSTATE_RESET	0x00000000	/* 000 - reset */
#define DM_TXSTATE_FETCH	0x00100000	/* 001 - fetching descriptor */
#define DM_TXSTATE_WAITEND	0x00200000	/* 010 - wait for tx end */
#define DM_TXSTATE_READING	0x00300000	/* 011 - read and enqueue */
#define DM_TXSTATE_RSVD		0x00400000	/* 100 - reserved */
#define DM_TXSTATE_SETUP	0x00500000	/* 101 - setup packet */
#define DM_TXSTATE_SUSPEND	0x00600000	/* 110 - suspend tx */
#define DM_TXSTATE_CLOSE	0x00700000	/* 111 - close tx desc */

/*
 * Network config bits.
 */
#define DM_NETCFG_LINKSTAT_PCS	0x00000001
#define DM_NETCFG_RX_ON		0x00000002
#define DM_NETCFG_RX_BADFRAMES	0x00000008
#define DM_NETCFG_RX_PROMISC	0x00000040
#define DM_NETCFG_RX_ALLMULTI	0x00000080
#define DM_NETCFG_RX_BROAD	0x00000100
#define DM_NETCFG_FULLDUPLEX	0x00000200
#define DM_NETCFG_LOOPBACK	0x00000C00
#define DM_NETCFG_FORCECOLL	0x00001000
#define DM_NETCFG_TX_ON		0x00002000
#define DM_NETCFG_TX_THRESH	0x0000C000
#define DM_NETCFG_PORTSEL	0x00040000	/* 0 == SRL, 1 == MII/SYM */
#define DM_NETCFG_HEARTBEAT	0x00080000	/* 0 == ON, 1 == OFF */
#define DM_NETCFG_STORENFWD	0x00200000
#define DM_NETCFG_SPEEDSEL	0x00400000	/* 1 == 10, 0 == 100 */
#define DM_NETCFG_PCS		0x00800000
#define DM_NETCFG_SCRAMBLER	0x01000000
#define DM_NETCFG_RX_ALL	0x40000000

#define DM_OPMODE_NORM		0x00000000
#define DM_OPMODE_INTLOOP	0x00000400
#define DM_OPMODE_EXTLOOP	0x00000800

#define DM_TXTHRESH_72BYTES	0x00000000
#define DM_TXTHRESH_96BYTES	0x00004000
#define DM_TXTHRESH_128BYTES	0x00008000
#define DM_TXTHRESH_160BYTES	0x0000C000

/*
 * Interrupt mask bits.
 */
#define DM_IMR_TX_OK		0x00000001
#define DM_IMR_TX_IDLE		0x00000002
#define DM_IMR_TX_NOBUF		0x00000004
#define DM_IMR_TX_JABBERTIMEO	0x00000008
#define DM_IMR_TX_UNDERRUN	0x00000020
#define DM_IMR_RX_OK		0x00000040
#define DM_IMR_RX_NOBUF		0x00000080
#define DM_IMR_RX_IDLE		0x00000100
#define DM_IMR_RX_WATDOGTIMEO	0x00000200
#define DM_IMR_TX_EARLY		0x00000400
#define DM_IMR_TIMER_EXPIRED	0x00000800
#define DM_IMR_BUS_ERR		0x00002000
#define DM_IMR_RX_EARLY		0x00004000
#define DM_IMR_ABNORMAL		0x00008000
#define DM_IMR_NORMAL		0x00010000

#define DM_INTRS	\
	(DM_IMR_RX_OK|DM_IMR_TX_OK|DM_IMR_RX_NOBUF|DM_IMR_RX_WATDOGTIMEO|\
	DM_IMR_TX_NOBUF|DM_IMR_TX_UNDERRUN|DM_IMR_BUS_ERR|		\
	DM_IMR_ABNORMAL|DM_IMR_NORMAL|/*DM_IMR_TX_EARLY*/		\
	DM_IMR_TX_IDLE|DM_IMR_RX_IDLE)

/*
 * Serial I/O (EEPROM/ROM) bits.
 */
#define DM_SIO_EE_CS		0x00000001	/* EEPROM chip select */
#define DM_SIO_EE_CLK		0x00000002	/* EEPROM clock */
#define DM_SIO_EE_DATAIN	0x00000004	/* EEPROM data output */
#define DM_SIO_EE_DATAOUT	0x00000008	/* EEPROM data input */
#define DM_SIO_EESEL		0x00000800
#define DM_SIO_ROMSEL		0x00001000
#define DM_SIO_ROMCTL_WRITE	0x00002000
#define DM_SIO_ROMCTL_READ	0x00004000
#define DM_SIO_MII_CLK		0x00010000	/* MDIO clock */
#define DM_SIO_MII_DATAOUT	0x00020000	/* MDIO data out */
#define DM_SIO_MII_DIR		0x00040000	/* MDIO dir */
#define DM_SIO_MII_DATAIN	0x00080000	/* MDIO data in */

#define DM_EECMD_WRITE		0x140
#define DM_EECMD_READ		0x180
#define DM_EECMD_ERASE		0x1c0

#define DM_EE_NODEADDR_OFFSET	0x70
#define DM_EE_NODEADDR		10

/*
 * General purpose timer register
 */
#define DM_TIMER_VALUE		0x0000FFFF
#define DM_TIMER_CONTINUOUS	0x00010000

/*
 * Size of a setup frame.       
 */
#define DM_SFRAME_LEN		192

/*
 * Davicom TX/RX list structure.
 */

struct dm_desc {
	u_int32_t		dm_status;
	u_int32_t		dm_ctl;
	u_int32_t		dm_ptr1;
	u_int32_t		dm_ptr2;
	struct mbuf		*dm_mbuf;
	struct dm_desc		*dm_nextdesc;
};

#define dm_data		dm_ptr1
#define dm_next		dm_ptr2

#define DM_RXSTAT_FIFOOFLOW	0x00000001
#define DM_RXSTAT_CRCERR	0x00000002
#define DM_RXSTAT_DRIBBLE	0x00000004
#define DM_RXSTAT_WATCHDOG	0x00000010
#define DM_RXSTAT_FRAMETYPE	0x00000020	/* 0 == IEEE 802.3 */
#define DM_RXSTAT_COLLSEEN	0x00000040
#define DM_RXSTAT_GIANT		0x00000080
#define DM_RXSTAT_LASTFRAG	0x00000100
#define DM_RXSTAT_FIRSTFRAG	0x00000200
#define DM_RXSTAT_MULTICAST	0x00000400
#define DM_RXSTAT_RUNT		0x00000800
#define DM_RXSTAT_RXTYPE	0x00003000
#define DM_RXSTAT_RXERR		0x00008000
#define DM_RXSTAT_RXLEN		0x3FFF0000
#define DM_RXSTAT_OWN		0x80000000

#define DM_RXBYTES(x)		((x & DM_RXSTAT_RXLEN) >> 16)
#define DM_RXSTAT (DM_RXSTAT_FIRSTFRAG|DM_RXSTAT_LASTFRAG|DM_RXSTAT_OWN)

#define DM_RXCTL_BUFLEN1	0x00000FFF
#define DM_RXCTL_BUFLEN2	0x00FFF000
#define DM_RXCTL_RLINK		0x01000000
#define DM_RXCTL_RLAST		0x02000000

#define DM_TXSTAT_DEFER		0x00000001
#define DM_TXSTAT_UNDERRUN	0x00000002
#define DM_TXSTAT_LINKFAIL	0x00000003
#define DM_TXSTAT_COLLCNT	0x00000078
#define DM_TXSTAT_SQE		0x00000080
#define DM_TXSTAT_EXCESSCOLL	0x00000100
#define DM_TXSTAT_LATECOLL	0x00000200
#define DM_TXSTAT_NOCARRIER	0x00000400
#define DM_TXSTAT_CARRLOST	0x00000800
#define DM_TXSTAT_JABTIMEO	0x00004000
#define DM_TXSTAT_ERRSUM	0x00008000
#define DM_TXSTAT_OWN		0x80000000

#define DM_TXCTL_BUFLEN1	0x000007FF
#define DM_TXCTL_BUFLEN2	0x003FF800
#define DM_TXCTL_FILTTYPE0	0x00400000
#define DM_TXCTL_PAD		0x00800000
#define DM_TXCTL_TLINK		0x01000000
#define DM_TXCTL_TLAST		0x02000000
#define DM_TXCTL_NOCRC		0x04000000
#define DM_TXCTL_SETUP		0x08000000
#define DM_TXCTL_FILTTYPE1	0x10000000
#define DM_TXCTL_FIRSTFRAG	0x20000000
#define DM_TXCTL_LASTFRAG	0x40000000
#define DM_TXCTL_FINT		0x80000000

#define DM_FILTER_PERFECT	0x00000000
#define DM_FILTER_HASHPERF	0x00400000
#define DM_FILTER_INVERSE	0x10000000
#define DM_FILTER_HASHONLY	0x10400000

#define DM_MAXFRAGS		16
#define DM_RX_LIST_CNT		64
#define DM_TX_LIST_CNT		128
#define DM_MIN_FRAMELEN		60
#define DM_RXLEN		1536

#define DM_INC(x, y)	(x) = (x + 1) % y

struct dm_list_data {
	struct dm_desc		dm_rx_list[DM_RX_LIST_CNT];
	struct dm_desc		dm_tx_list[DM_TX_LIST_CNT];
	struct dm_desc		dm_sframe;
};

struct dm_chain_data {
	u_int32_t		dm_sbuf[DM_SFRAME_LEN/sizeof(u_int32_t)];
	u_int8_t		dm_pad[DM_MIN_FRAMELEN];
	int			dm_tx_prod;
	int			dm_tx_cons;
	int			dm_tx_cnt;
	int			dm_rx_prod;
};

struct dm_type {
	u_int16_t		dm_vid;
	u_int16_t		dm_did;
	char			*dm_name;
};

struct dm_mii_frame {
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
#define DM_MII_STARTDELIM	0x01
#define DM_MII_READOP		0x02
#define DM_MII_WRITEOP		0x01
#define DM_MII_TURNAROUND	0x02

struct dm_softc {
	struct arpcom		arpcom;		/* interface info */
	bus_space_handle_t	dm_bhandle;	/* bus space handle */
	bus_space_tag_t		dm_btag;	/* bus space tag */
	void			*dm_intrhand;
	struct resource		*dm_irq;
	struct resource		*dm_res;
	device_t		dm_miibus;
	u_int8_t		dm_unit;	/* interface number */
	int			dm_cachesize;
	struct dm_list_data	*dm_ldata;
	struct dm_chain_data	dm_cdata;
	struct callout_handle	dm_stat_ch;
};

/*
 * register space access macros
 */
#define CSR_WRITE_4(sc, reg, val)	\
	bus_space_write_4(sc->dm_btag, sc->dm_bhandle, reg, val)
#define CSR_WRITE_2(sc, reg, val)	\
	bus_space_write_2(sc->dm_btag, sc->dm_bbhandle, reg, val)
#define CSR_WRITE_1(sc, reg, val)	\
	bus_space_write_1(sc->dm_btag, sc->dm_bhandle, reg, val)

#define CSR_READ_4(sc, reg)		\
	bus_space_read_4(sc->dm_btag, sc->dm_bhandle, reg)
#define CSR_READ_2(sc, reg)		\
	bus_space_read_2(sc->dm_btag, sc->dm_bhandle, reg)
#define CSR_READ_1(sc, reg)		\
	bus_space_read_1(sc->dm_btag, sc->dm_bhandle, reg)

#define DM_TIMEOUT		1000
#define ETHER_ALIGN		2

/*
 * General constants that are fun to know.
 *
 * Davicom PCI vendor ID
 */
#define	DM_VENDORID		0x1282

/*
 * Davicom DM9102 device ID.
 */
#define DM_DEVICEID_DM9102	0x9102
#define DM_DEVICEID_DM9100	0x9100

/*
 * PCI low memory base and low I/O base register, and
 * other PCI registers.
 */

#define DM_PCI_VENDOR_ID	0x00
#define DM_PCI_DEVICE_ID	0x02
#define DM_PCI_COMMAND		0x04
#define DM_PCI_STATUS		0x06
#define DM_PCI_REVID		0x08
#define DM_PCI_CLASSCODE	0x09
#define DM_PCI_CACHELEN		0x0C
#define DM_PCI_LATENCY_TIMER	0x0D
#define DM_PCI_HEADER_TYPE	0x0E
#define DM_PCI_LOIO		0x10
#define DM_PCI_LOMEM		0x14
#define DM_PCI_BIOSROM		0x30
#define DM_PCI_INTLINE		0x3C
#define DM_PCI_INTPIN		0x3D
#define DM_PCI_MINGNT		0x3E
#define DM_PCI_MINLAT		0x0F
#define DM_PCI_RESETOPT		0x48
#define DM_PCI_EEPROM_DATA	0x4C

/* power management registers */
#define DM_PCI_CAPID		0x50 /* 8 bits */
#define DM_PCI_NEXTPTR		0x51 /* 8 bits */
#define DM_PCI_PWRMGMTCAP	0x52 /* 16 bits */
#define DM_PCI_PWRMGMTCTRL	0x54 /* 16 bits */

#define DM_PSTATE_MASK		0x0003
#define DM_PSTATE_D0		0x0000
#define DM_PSTATE_D1		0x0001
#define DM_PSTATE_D2		0x0002
#define DM_PSTATE_D3		0x0003
#define DM_PME_EN		0x0010
#define DM_PME_STATUS		0x8000

#ifdef __alpha__
#undef vtophys
#define vtophys(va)		alpha_XXX_dmamap((vm_offset_t)va)
#endif
