/*
 * Copyright (c) 1997, 1998
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
 *	$Id: if_mxreg.h,v 1.16 1999/04/08 17:35:38 wpaul Exp $
 */

/*
 * Macronix register definitions.
 */

#define MX_BUSCTL		0x00	/* bus control */
#define MX_TXSTART		0x08	/* tx start demand */
#define MX_RXSTART		0x10	/* rx start demand */
#define MX_RXADDR		0x18	/* rx descriptor list start addr */
#define MX_TXADDR		0x20	/* tx descriptor list start addr */
#define MX_ISR			0x28	/* interrupt status register */
#define MX_NETCFG		0x30	/* network config register */
#define MX_IMR			0x38	/* interrupt mask */
#define MX_FRAMESDISCARDED	0x40	/* # of discarded frames */
#define MX_SIO			0x48	/* MII and ROM/EEPROM access */
#define MX_RESERVED		0x50
#define MX_TIMER		0x58	/* general timer */
#define MX_10BTSTAT		0x60
#define MX_SIARESET		0x68
#define MX_10BTCTRL		0x70
#define MX_WATCHDOG		0x78
#define MX_MAGICPACKET		0x80
#define MX_NWAYSTAT		0xA0

/*
 * These are magic values that must be written into CSR16
 * (MX_MAGICPACKET) in order to put the chip into proper
 * operating mode. The magic numbers are documented in the
 * Macronix 98715 application notes.
 */
#define MX_MAGIC_98713		0x0F370000
#define MX_MAGIC_98713A		0x0B3C0000
#define MX_MAGIC_98715		0x0B3C0000
#define MX_MAGIC_98725		0x0B3C0000

#define MX_REVISION_98713	0x00
#define MX_REVISION_98713A	0x10
#define MX_REVISION_98715	0x20
#define MX_REVISION_98725	0x30

/*
 * As far as the driver is concerned, there are two 'types' of
 * chips to be concerned with. One is a 98713 with an external
 * PHY on the MII. The other covers pretty much everything else,
 * since all the other Macronix chips have built-in transceivers.
 * This type setting governs what which mode selection routines
 * we use (MII or built-in). It also govers which of the 'magic'
 * numbers we write into CSR16.
 */
#define MX_TYPE_98713		0x1
#define MX_TYPE_98713A		0x2
#define MX_TYPE_987x5		0x3

/*
 * Bus control bits.
 */
#define MX_BUSCTL_RESET		0x00000001
#define MX_BUSCTL_ARBITRATION	0x00000002
#define MX_BUSCTL_SKIPLEN	0x0000007C
#define MX_BUSCTL_BUF_BIGENDIAN	0x00000080
#define MX_BUSCTL_BURSTLEN	0x00003F00
#define MX_BUSCTL_CACHEALIGN	0x0000C000
#define MX_BUSCTL_XMITPOLL	0x00060000

#define MX_SKIPLEN_1LONG	0x00000004
#define MX_SKIPLEN_2LONG	0x00000008
#define MX_SKIPLEN_3LONG	0x00000010
#define MX_SKIPLEN_4LONG	0x00000020
#define MX_SKIPLEN_5LONG	0x00000040

#define MX_CACHEALIGN_8LONG	0x00004000
#define MX_CACHEALIGN_16LONG	0x00008000
#define MX_CACHEALIGN_32LONG	0x0000C000

#define MX_BURSTLEN_USECA	0x00000000
#define MX_BURSTLEN_1LONG	0x00000100
#define MX_BURSTLEN_2LONG	0x00000200
#define MX_BURSTLEN_4LONG	0x00000400
#define MX_BURSTLEN_8LONG	0x00000800
#define MX_BURSTLEN_16LONG	0x00001000
#define MX_BURSTLEN_32LONG	0x00002000

#define MX_TXPOLL_OFF		0x00000000
#define MX_TXPOLL_200U		0x00020000
#define MX_TXPOLL_800U		0x00040000
#define MX_TXPOLL_1600U		0x00060000

#define MX_BUSCTL_CONFIG	(MX_BUSCTL_ARBITRATION|MX_CACHEALIGN_8LONG| \
					MX_BURSTLEN_8LONG)

/*
 * Interrupt status bits.
 */
#define MX_ISR_TX_OK		0x00000001
#define MX_ISR_TX_IDLE		0x00000002
#define MX_ISR_TX_NOBUF		0x00000004
#define MX_ISR_TX_JABBERTIMEO	0x00000008
#define MX_ISR_LINKGOOD		0x00000010
#define MX_ISR_TX_UNDERRUN	0x00000020
#define MX_ISR_RX_OK		0x00000040
#define MX_ISR_RX_NOBUF		0x00000080
#define MX_ISR_RX_READ		0x00000100
#define MX_ISR_RX_WATDOGTIMEO	0x00000200
#define MX_ISR_TX_EARLY		0x00000400
#define MX_ISR_TIMER_EXPIRED	0x00000800
#define MX_ISR_LINKFAIL		0x00001000
#define MX_ISR_BUS_ERR		0x00002000
#define MX_ISR_RX_EARLY		0x00004000
#define MX_ISR_ABNORMAL		0x00008000
#define MX_ISR_NORMAL		0x00010000
#define MX_ISR_RX_STATE		0x000E0000
#define MX_ISR_TX_STATE		0x00700000
#define MX_ISR_BUSERRTYPE	0x03800000
#define MX_ISR_100MBPSLINK	0x08000000
#define MX_ISR_MAGICKPACK	0x10000000

#define MX_RXSTATE_STOPPED	0x00000000	/* 000 - Stopped */
#define MX_RXSTATE_FETCH	0x00020000	/* 001 - Fetching descriptor */
#define MX_RXSTATE_ENDCHECK	0x00040000	/* 010 - check for rx end */
#define MX_RXSTATE_WAIT		0x00060000	/* 011 - waiting for packet */
#define MX_RXSTATE_SUSPEND	0x00080000	/* 100 - suspend rx */
#define MX_RXSTATE_CLOSE	0x000A0000	/* 101 - close tx desc */
#define MX_RXSTATE_FLUSH	0x000C0000	/* 110 - flush from FIFO */
#define MX_RXSTATE_DEQUEUE	0x000E0000	/* 111 - dequeue from FIFO */

#define MX_TXSTATE_RESET	0x00000000	/* 000 - reset */
#define MX_TXSTATE_FETCH	0x00100000	/* 001 - fetching descriptor */
#define MX_TXSTATE_WAITEND	0x00200000	/* 010 - wait for tx end */
#define MX_TXSTATE_READING	0x00300000	/* 011 - read and enqueue */
#define MX_TXSTATE_RSVD		0x00400000	/* 100 - reserved */
#define MX_TXSTATE_SETUP	0x00500000	/* 101 - setup packet */
#define MX_TXSTATE_SUSPEND	0x00600000	/* 110 - suspend tx */
#define MX_TXSTATE_CLOSE	0x00700000	/* 111 - close tx desc */

/*
 * Network config bits.
 */
#define MX_NETCFG_RX_HASHPERF	0x00000001
#define MX_NETCFG_RX_ON		0x00000002
#define MX_NETCFG_RX_HASHONLY	0x00000004
#define MX_NETCFG_RX_BADFRAMES	0x00000008
#define MX_NETCFG_RX_INVFILT	0x00000010
#define MX_NETCFG_BACKOFFCNT	0x00000020
#define MX_NETCFG_RX_PROMISC	0x00000040
#define MX_NETCFG_RX_ALLMULTI	0x00000080
#define MX_NETCFG_FULLDUPLEX	0x00000200
#define MX_NETCFG_LOOPBACK	0x00000C00
#define MX_NETCFG_FORCECOLL	0x00001000
#define MX_NETCFG_TX_ON		0x00002000
#define MX_NETCFG_TX_THRESH	0x0000C000
#define MX_NETCFG_TX_BACKOFF	0x00020000
#define MX_NETCFG_PORTSEL	0x00040000	/* 0 == 10, 1 == 100 */
#define MX_NETCFG_HEARTBEAT	0x00080000
#define MX_NETCFG_STORENFWD	0x00200000
#define MX_NETCFG_SPEEDSEL	0x00400000	/* 1 == 10, 0 == 100 */
#define MX_NETCFG_PCS		0x00800000
#define MX_NETCFG_SCRAMBLER	0x01000000
#define MX_NETCFG_NO_RXCRC	0x02000000

#define MX_OPMODE_NORM		0x00000000
#define MX_OPMODE_INTLOOP	0x00000400
#define MX_OPMODE_EXTLOOP	0x00000800

#define MX_TXTHRESH_72BYTES	0x00000000
#define MX_TXTHRESH_96BYTES	0x00004000
#define MX_TXTHRESH_128BYTES	0x00008000
#define MX_TXTHRESH_160BYTES	0x0000C000


/*
 * Interrupt mask bits.
 */
#define MX_IMR_TX_OK		0x00000001
#define MX_IMR_TX_IDLE		0x00000002
#define MX_IMR_TX_NOBUF		0x00000004
#define MX_IMR_TX_JABBERTIMEO	0x00000008
#define MX_IMR_LINKGOOD		0x00000010
#define MX_IMR_TX_UNDERRUN	0x00000020
#define MX_IMR_RX_OK		0x00000040
#define MX_IMR_RX_NOBUF		0x00000080
#define MX_IMR_RX_READ		0x00000100
#define MX_IMR_RX_WATDOGTIMEO	0x00000200
#define MX_IMR_TX_EARLY		0x00000400
#define MX_IMR_TIMER_EXPIRED	0x00000800
#define MX_IMR_LINKFAIL		0x00001000
#define MX_IMR_BUS_ERR		0x00002000
#define MX_IMR_RX_EARLY		0x00004000
#define MX_IMR_ABNORMAL		0x00008000
#define MX_IMR_NORMAL		0x00010000
#define MX_IMR_100MBPSLINK	0x08000000
#define MX_IMR_MAGICKPACK	0x10000000

#define MX_INTRS	\
	(MX_IMR_RX_OK|MX_IMR_TX_OK|MX_IMR_RX_NOBUF|MX_IMR_RX_WATDOGTIMEO|\
	MX_IMR_TX_NOBUF|MX_IMR_TX_UNDERRUN|MX_IMR_BUS_ERR|		\
	MX_IMR_ABNORMAL|MX_IMR_NORMAL/*|MX_IMR_TX_EARLY*/)
/*
 * Serial I/O (EEPROM/ROM) bits.
 */
#define MX_SIO_EE_CS		0x00000001	/* EEPROM chip select */
#define MX_SIO_EE_CLK		0x00000002	/* EEPROM clock */
#define MX_SIO_EE_DATAIN	0x00000004	/* EEPROM data output */
#define MX_SIO_EE_DATAOUT	0x00000008	/* EEPROM data input */
#define MX_SIO_ROMDATA4		0x00000010
#define MX_SIO_ROMDATA5		0x00000020
#define MX_SIO_ROMDATA6		0x00000040
#define MX_SIO_ROMDATA7		0x00000080
#define MX_SIO_EESEL		0x00000800
#define MX_SIO_ROMSEL		0x00001000
#define MX_SIO_ROMCTL_WRITE	0x00002000
#define MX_SIO_ROMCTL_READ	0x00004000
#define MX_SIO_MII_CLK		0x00010000	/* MDIO clock */
#define MX_SIO_MII_DATAOUT	0x00020000	/* MDIO data out */
#define MX_SIO_MII_DIR		0x00040000	/* MDIO dir */
#define MX_SIO_MII_DATAIN	0x00080000	/* MDIO data in */

#define MX_EECMD_WRITE		0x140
#define MX_EECMD_READ		0x180
#define MX_EECMD_ERASE		0x1c0

#define MX_EE_NODEADDR_OFFSET	0x70
#define MX_EE_NODEADDR		10

/*
 * General purpose timer register
 */
#define MX_TIMER_VALUE		0x0000FFFF
#define MX_TIMER_CONTINUUS	0x00010000

/*
 * 10baseT status register
 */
#define MX_TSTAT_LS100		0x00000002 /* link status of 100baseTX */
#define MX_TSTAT_LS10		0x00000004 /* link status of 10baseT */
#define MX_TSTAT_AUTOPOLARITY	0x00000008
#define MX_TSTAT_REMFAULT	0x00000800
#define MX_TSTAT_ANEGSTAT	0x00007000
#define MX_TSTAT_LP_CAN_NWAY	0x00008000 /* link partner supports NWAY */
#define MX_TSTAT_LPCODEWORD	0xFFFF0000 /* link partner's code word */

#define MX_ASTAT_DISABLE	0x00000000
#define MX_ASTAT_TXDISABLE	0x00001000
#define MX_ASTAT_ABDETECT	0x00002000
#define MX_ASTAT_ACKDETECT	0x00003000
#define MX_ASTAT_CMPACKDETECT	0x00004000
#define MX_ASTAT_AUTONEGCMP	0x00005000
#define MX_ASTAT_LINKCHECK	0x00006000

/*
 * PHY reset register
 */
#define MX_SIA_RESET_NWAY	0x00000001
#define MX_SIA_RESET_100TX	0x00000002

/*
 * 10baseT control register
 */
#define MX_TCTL_LOOPBACK	0x00000002
#define MX_TCTL_POWERDOWN	0x00000004
#define MX_TCTL_HALFDUPLEX	0x00000040
#define MX_TCTL_AUTONEGENBL	0x00000080
#define MX_TCTL_RX_SQUELCH	0x00000100
#define MX_TCTL_LINKTEST	0x00001000
#define MX_TCTL_100BTXHALF	0x00010000
#define MX_TCTL_100BTXFULL	0x00020000
#define MX_TCTL_100BT4		0x00040000

/*
 * Watchdog timer register
 */
#define MX_WDOG_JABBERDIS	0x00000001
#define MX_WDOG_HOSTUNJAB	0x00000002
#define MX_WDOG_JABBERCLK	0x00000004
#define MX_WDOG_RXWDOGDIS	0x00000010
#define MX_WDOG_RXWDOGCLK	0x00000020
#define MX_WDOG_MUSTBEZERO	0x00000100

/*
 * Magic packet register
 */
#define MX_MPACK_DISABLE	0x00400000

/*
 * NWAY status register.
 */
#define MX_NWAY_10BTHALF	0x08000000
#define MX_NWAY_10BTFULL	0x10000000
#define MX_NWAY_100BTHALF	0x20000000
#define MX_NWAY_100BTFULL	0x40000000
#define MX_NWAY_100BT4		0x80000000

/*
 * Size of a setup frame.
 */
#define MX_SFRAME_LEN		192

/*
 * Macronix TX/RX list structure.
 */

struct mx_desc {
	u_int32_t		mx_status;
	u_int32_t		mx_ctl;
	u_int32_t		mx_ptr1;
	u_int32_t		mx_ptr2;
};

#define mx_data		mx_ptr1
#define mx_next		mx_ptr2

#define MX_RXSTAT_FIFOOFLOW	0x00000001
#define MX_RXSTAT_CRCERR	0x00000002
#define MX_RXSTAT_DRIBBLE	0x00000004
#define MX_RXSTAT_WATCHDOG	0x00000010
#define MX_RXSTAT_FRAMETYPE	0x00000020	/* 0 == IEEE 802.3 */
#define MX_RXSTAT_COLLSEEN	0x00000040
#define MX_RXSTAT_GIANT		0x00000080
#define MX_RXSTAT_LASTFRAG	0x00000100
#define MX_RXSTAT_FIRSTFRAG	0x00000200
#define MX_RXSTAT_MULTICAST	0x00000400
#define MX_RXSTAT_RUNT		0x00000800
#define MX_RXSTAT_RXTYPE	0x00003000
#define MX_RXSTAT_RXERR		0x00008000
#define MX_RXSTAT_RXLEN		0x3FFF0000
#define MX_RXSTAT_OWN		0x80000000

#define MX_RXBYTES(x)		((x & MX_RXSTAT_RXLEN) >> 16)
#define MX_RXSTAT (MX_RXSTAT_FIRSTFRAG|MX_RXSTAT_LASTFRAG|MX_RXSTAT_OWN)

#define MX_RXCTL_BUFLEN1	0x00000FFF
#define MX_RXCTL_BUFLEN2	0x00FFF000
#define MX_RXCTL_RLINK		0x01000000
#define MX_RXCTL_RLAST		0x02000000

#define MX_TXSTAT_DEFER		0x00000001
#define MX_TXSTAT_UNDERRUN	0x00000002
#define MX_TXSTAT_LINKFAIl	0x00000003
#define MX_TXSTAT_COLLCNT	0x00000078
#define MX_TXSTAT_SQE		0x00000080
#define MX_TXSTAT_EXCESSCOLL	0x00000100
#define MX_TXSTAT_LATECOLL	0x00000200
#define MX_TXSTAT_NOCARRIER	0x00000400
#define MX_TXSTAT_CARRLOST	0x00000800
#define MX_TXSTAT_JABTIMEO	0x00004000
#define MX_TXSTAT_ERRSUM	0x00008000
#define MX_TXSTAT_OWN		0x80000000

#define MX_TXCTL_BUFLEN1	0x000007FF
#define MX_TXCTL_BUFLEN2	0x003FF800
#define MX_TXCTL_FILTTYPE0	0x00400000
#define MX_TXCTL_PAD		0x00800000
#define MX_TXCTL_TLINK		0x01000000
#define MX_TXCTL_TLAST		0x02000000
#define MX_TXCTL_NOCRC		0x04000000
#define MX_TXCTL_SETUP		0x08000000
#define MX_TXCTL_FILTTYPE1	0x10000000
#define MX_TXCTL_FIRSTFRAG	0x20000000
#define MX_TXCTL_LASTFRAG	0x40000000
#define MX_TXCTL_FINT		0x80000000

#define MX_FILTER_PERFECT	0x00000000
#define MX_FILTER_HASHPERF	0x00400000
#define MX_FILTER_INVERSE	0x10000000
#define MX_FILTER_HASHONLY	0x10400000

#define MX_MAXFRAGS		16
#define MX_RX_LIST_CNT		64
#define MX_TX_LIST_CNT		64
#define MX_MIN_FRAMELEN		60

/*
 * A tx 'super descriptor' is actually 16 regular descriptors
 * back to back.
 */
struct mx_txdesc {
	struct mx_desc		mx_frag[MX_MAXFRAGS];
};

#define MX_TXNEXT(x)	x->mx_ptr->mx_frag[x->mx_lastdesc].mx_next
#define MX_TXSTATUS(x)	x->mx_ptr->mx_frag[x->mx_lastdesc].mx_status
#define MX_TXCTL(x)	x->mx_ptr->mx_frag[x->mx_lastdesc].mx_ctl
#define MX_TXDATA(x)	x->mx_ptr->mx_frag[x->mx_lastdesc].mx_data

#define MX_TXOWN(x)	x->mx_ptr->mx_frag[0].mx_status

#define MX_UNSENT	0x12341234

struct mx_list_data {
	struct mx_desc		mx_rx_list[MX_RX_LIST_CNT];
	struct mx_txdesc	mx_tx_list[MX_TX_LIST_CNT];
};

struct mx_chain {
	struct mx_txdesc	*mx_ptr;
	struct mbuf		*mx_mbuf;
	struct mx_chain		*mx_nextdesc;
	u_int8_t		mx_lastdesc;
};

struct mx_chain_onefrag {
	struct mx_desc		*mx_ptr;
	struct mbuf		*mx_mbuf;
	struct mx_chain_onefrag	*mx_nextdesc;
};

struct mx_chain_data {
	struct mx_desc		mx_sframe;
	u_int32_t		mx_sbuf[MX_SFRAME_LEN/sizeof(u_int32_t)];
	u_int8_t		mx_pad[MX_MIN_FRAMELEN];
	struct mx_chain_onefrag	mx_rx_chain[MX_RX_LIST_CNT];
	struct mx_chain		mx_tx_chain[MX_TX_LIST_CNT];

	struct mx_chain_onefrag	*mx_rx_head;

	struct mx_chain		*mx_tx_head;
	struct mx_chain		*mx_tx_tail;
	struct mx_chain		*mx_tx_free;
};

struct mx_type {
	u_int16_t		mx_vid;
	u_int16_t		mx_did;
	char			*mx_name;
};

struct mx_mii_frame {
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
#define MX_MII_STARTDELIM	0x01
#define MX_MII_READOP		0x02
#define MX_MII_WRITEOP		0x01
#define MX_MII_TURNAROUND	0x02

#define MX_FLAG_FORCEDELAY	1
#define MX_FLAG_SCHEDDELAY	2
#define MX_FLAG_DELAYTIMEO	3	

struct mx_softc {
	struct arpcom		arpcom;		/* interface info */
	struct ifmedia		ifmedia;	/* media info */
	bus_space_handle_t	mx_bhandle;	/* bus space handle */
	bus_space_tag_t		mx_btag;	/* bus space tag */
	struct mx_type		*mx_info;	/* Macronix adapter info */
	struct mx_type		*mx_pinfo;	/* phy info */
	u_int8_t		mx_unit;	/* interface number */
	u_int8_t		mx_type;
	u_int8_t		mx_phy_addr;	/* PHY address */
	u_int8_t		mx_tx_pend;	/* TX pending */
	u_int8_t		mx_want_auto;
	u_int8_t		mx_autoneg;
	u_int8_t		mx_singlebuf;
	caddr_t			mx_ldata_ptr;
	struct mx_list_data	*mx_ldata;
	struct mx_chain_data	mx_cdata;
};

/*
 * register space access macros
 */
#define CSR_WRITE_4(sc, reg, val)	\
	bus_space_write_4(sc->mx_btag, sc->mx_bhandle, reg, val)
#define CSR_WRITE_2(sc, reg, val)	\
	bus_space_write_2(sc->mx_btag, sc->mx_bhandle, reg, val)
#define CSR_WRITE_1(sc, reg, val)	\
	bus_space_write_1(sc->mx_btag, sc->mx_bhandle, reg, val)

#define CSR_READ_4(sc, reg)		\
	bus_space_read_4(sc->mx_btag, sc->mx_bhandle, reg)
#define CSR_READ_2(sc, reg)		\
	bus_space_read_2(sc->mx_btag, sc->mx_bhandle, reg)
#define CSR_READ_1(sc, reg)		\
	bus_space_read_1(sc->mx_btag, sc->mx_bhandle, reg)

#define MX_TIMEOUT		1000

/*
 * General constants that are fun to know.
 *
 * Macronix PCI vendor ID
 */
#define	MX_VENDORID		0x10D9

/*
 * Macronix PMAC device IDs.
 */
#define MX_DEVICEID_98713	0x0512
#define MX_DEVICEID_987x5	0x0531

/*
 * Compex PCI vendor ID.
 */
#define CP_VENDORID		0x11F6

/*
 * Compex PMAC PCI device IDs.
 */
#define CP_DEVICEID_98713	0x9881

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

#define MX_PCI_VENDOR_ID	0x00
#define MX_PCI_DEVICE_ID	0x02
#define MX_PCI_COMMAND		0x04
#define MX_PCI_STATUS		0x06
#define MX_PCI_REVID		0x08
#define MX_PCI_CLASSCODE	0x09
#define MX_PCI_LATENCY_TIMER	0x0D
#define MX_PCI_HEADER_TYPE	0x0E
#define MX_PCI_LOIO		0x10
#define MX_PCI_LOMEM		0x14
#define MX_PCI_BIOSROM		0x30
#define MX_PCI_INTLINE		0x3C
#define MX_PCI_INTPIN		0x3D
#define MX_PCI_MINGNT		0x3E
#define MX_PCI_MINLAT		0x0F
#define MX_PCI_RESETOPT		0x48
#define MX_PCI_EEPROM_DATA	0x4C

/* power management registers */
#define MX_PCI_CAPID		0x44 /* 8 bits */
#define MX_PCI_NEXTPTR		0x45 /* 8 bits */
#define MX_PCI_PWRMGMTCAP	0x46 /* 16 bits */
#define MX_PCI_PWRMGMTCTRL	0x48 /* 16 bits */

#define MX_PSTATE_MASK		0x0003
#define MX_PSTATE_D0		0x0000
#define MX_PSTATE_D1		0x0001
#define MX_PSTATE_D2		0x0002
#define MX_PSTATE_D3		0x0003
#define MX_PME_EN		0x0010
#define MX_PME_STATUS		0x8000

#define PHY_UNKNOWN		6

#define MX_PHYADDR_MIN		0x00
#define MX_PHYADDR_MAX		0x1F

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
