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
 * COMET register definitions.
 */

#define AL_BUSCTL		0x00	/* bus control */
#define AL_TXSTART		0x08	/* tx start demand */
#define AL_RXSTART		0x10	/* rx start demand */
#define AL_RXADDR		0x18	/* rx descriptor list start addr */
#define AL_TXADDR		0x20	/* tx descriptor list start addr */
#define AL_ISR			0x28	/* interrupt status register */
#define AL_NETCFG		0x30	/* network config register */
#define AL_IMR			0x38	/* interrupt mask */
#define AL_FRAMESDISCARDED	0x40	/* # of discarded frames */
#define AL_SIO			0x48	/* MII and ROM/EEPROM access */
#define AL_RESERVED		0x50
#define AL_GENTIMER		0x58	/* general timer */
#define AL_GENPORT		0x60	/* general purpose port */
#define AL_WAKEUP_CTL		0x68	/* wake-up control/status register */
#define AL_WAKEUP_PAT		0x70	/* wake-up pattern data register */
#define AL_WATCHDOG		0x78	/* watchdog timer */
#define AL_ISR2			0x80	/* ISR assist register */
#define AL_IMR2			0x84	/* IRM assist register */
#define AL_COMMAND		0x88	/* command register */
#define AL_PCIPERF		0x8C	/* pci perf counter */
#define AL_PWRMGMT		0x90	/* pwr management command/status */
#define AL_TXBURST		0x9C	/* tx burst counter/timeout */
#define AL_FLASHPROM		0xA0	/* flash(boot) PROM port */
#define AL_PAR0			0xA4	/* station address */
#define AL_PAR1			0xA8	/* station address */
#define AL_MAR0			0xAC	/* multicast hash filter */
#define AL_MAR1			0xB0	/* multicast hash filter */
#define AL_BMCR			0xB4	/* built in PHY control */
#define AL_BMSR			0xB8	/* built in PHY status */
#define AL_VENID		0xBC	/* built in PHY ID0 */
#define AL_DEVID		0xC0	/* built in PHY ID1 */
#define AL_ANAR			0xC4	/* built in PHY autoneg advert */
#define AL_LPAR			0xC8	/* bnilt in PHY link part. ability */
#define AL_ANER			0xCC	/* built in PHY autoneg expansion */
#define AL_PHY_MODECTL		0xD0	/* mode control */
#define AL_PHY_CONFIG		0xD4	/* config info and inter status */
#define AL_PHY_INTEN		0xD8	/* interrupto enable */
#define AL_PHY_MODECTL_100TX	0xDC	/* 100baseTX control/status */

/*
 * Bus control bits.
 */
#define AL_BUSCTL_RESET		0x00000001
#define AL_BUSCTL_ARBITRATION	0x00000002
#define AL_BUSCTL_SKIPLEN	0x0000007C
#define AL_BUSCTL_BIGENDIAN	0x00000080
#define AL_BUSCTL_BURSTLEN	0x00003F00
#define AL_BUSCTL_CACHEALIGN	0x0000C000
#define AL_BUSCTL_XMITPOLL	0x00060000
#define AL_BUSCTL_BUF_BIGENDIAN	0x00100000
#define AL_BUSCTL_READMULTI	0x00200000
#define AL_BUSCTL_READLINE	0x00800000
#define AL_BUSCTL_WRITEINVAL	0x01000000

#define AL_SKIPLEN_1LONG	0x00000004
#define AL_SKIPLEN_2LONG	0x00000008
#define AL_SKIPLEN_3LONG	0x00000010
#define AL_SKIPLEN_4LONG	0x00000020
#define AL_SKIPLEN_5LONG	0x00000040

#define AL_BURSTLEN_UNLIMIT	0x00000000
#define AL_BURSTLEN_1LONG	0x00000100
#define AL_BURSTLEN_2LONG	0x00000200
#define AL_BURSTLEN_4LONG	0x00000400
#define AL_BURSTLEN_8LONG	0x00000800
#define AL_BURSTLEN_16LONG	0x00001000
#define AL_BURSTLEN_32LONG	0x00002000

#define AL_CACHEALIGN_NONE	0x00000000
#define AL_CACHEALIGN_8LONG	0x00004000
#define AL_CACHEALIGN_16LONG	0x00008000
#define AL_CACHEALIGN_32LONG	0x0000C000

#define AL_TXPOLL_OFF		0x00000000
#define AL_TXPOLL_200U		0x00020000
#define AX_TXPOLL_800U		0x00040000
#define AL_TXPOLL_1600U		0x00060000

/*
 * Interrupt status bits.
 */
#define AL_ISR_TX_OK		0x00000001
#define AL_ISR_TX_IDLE		0x00000002
#define AL_ISR_TX_NOBUF		0x00000004
#define AL_ISR_TX_JABBERTIMEO	0x00000008
#define AL_ISR_TX_UNDERRUN	0x00000020
#define AL_ISR_RX_OK		0x00000040
#define AL_ISR_RX_NOBUF		0x00000080
#define AL_ISR_RX_IDLE		0x00000100
#define AL_ISR_RX_WATDOGTIMEO	0x00000200
#define AL_ISR_TIMER_EXPIRED	0x00000800
#define AL_ISR_BUS_ERR		0x00002000
#define AL_ISR_ABNORMAL		0x00008000
#define AL_ISR_NORMAL		0x00010000
#define AL_ISR_RX_STATE		0x000E0000
#define AL_ISR_TX_STATE		0x00700000
#define AL_ISR_BUSERRTYPE	0x03800000

#define AL_RXSTATE_STOPPED	0x00000000	/* 000 - Stopped */
#define AL_RXSTATE_FETCH	0x00020000	/* 001 - Fetching descriptor */
#define AL_RXSTATE_ENDCHECK	0x00040000	/* 010 - check for rx end */
#define AL_RXSTATE_WAIT		0x00060000	/* 011 - waiting for packet */
#define AL_RXSTATE_SUSPEND	0x00080000	/* 100 - suspend rx */
#define AL_RXSTATE_CLOSE	0x000A0000	/* 101 - close tx desc */
#define AL_RXSTATE_FLUSH	0x000C0000	/* 110 - flush from FIFO */
#define AL_RXSTATE_DEQUEUE	0x000E0000	/* 111 - dequeue from FIFO */

#define AL_TXSTATE_RESET	0x00000000	/* 000 - reset */
#define AL_TXSTATE_FETCH	0x00100000	/* 001 - fetching descriptor */
#define AL_TXSTATE_WAITEND	0x00200000	/* 010 - wait for tx end */
#define AL_TXSTATE_READING	0x00300000	/* 011 - read and enqueue */
#define AL_TXSTATE_RSVD		0x00400000	/* 100 - reserved */
#define AL_TXSTATE_SETUP	0x00500000	/* 101 - setup packet */
#define AL_TXSTATE_SUSPEND	0x00600000	/* 110 - suspend tx */
#define AL_TXSTATE_CLOSE	0x00700000	/* 111 - close tx desc */

/*
 * Network config bits.
 */
#define AL_NETCFG_RX_ON		0x00000002
#define AL_NETCFG_RX_BADFRAMES	0x00000008
#define AL_NETCFG_RX_BACKOFF	0x00000020
#define AL_NETCFG_RX_PROMISC	0x00000040
#define AL_NETCFG_RX_ALLMULTI	0x00000080
#define AL_NETCFG_OPMODE	0x00000C00
#define AL_NETCFG_FORCECOLL	0x00001000
#define AL_NETCFG_TX_ON		0x00002000
#define AL_NETCFG_TX_THRESH	0x0000C000
#define AL_NETCFG_HEARTBEAT	0x00080000	/* 0 == ON, 1 == OFF */
#define AL_NETCFG_STORENFWD	0x00200000

#define AL_OPMODE_NORM		0x00000000
#define AL_OPMODE_INTLOOP	0x00000400
#define AL_OPMODE_EXTLOOP	0x00000800

#define AL_TXTHRESH_72BYTES	0x00000000
#define AL_TXTHRESH_96BYTES	0x00004000
#define AL_TXTHRESH_128BYTES	0x00008000
#define AL_TXTHRESH_160BYTES	0x0000C000

/*
 * Interrupt mask bits.
 */
#define AL_IMR_TX_OK		0x00000001
#define AL_IMR_TX_IDLE		0x00000002
#define AL_IMR_TX_NOBUF		0x00000004
#define AL_IMR_TX_JABBERTIMEO	0x00000008
#define AL_IMR_TX_UNDERRUN	0x00000020
#define AL_IMR_RX_OK		0x00000040
#define AL_IMR_RX_NOBUF		0x00000080
#define AL_IMR_RX_IDLE		0x00000100
#define AL_IMR_RX_WATDOGTIMEO	0x00000200
#define AL_IMR_TIMER_EXPIRED	0x00000800
#define AL_IMR_BUS_ERR		0x00002000
#define AL_IMR_ABNORMAL		0x00008000
#define AL_IMR_NORMAL		0x00010000

#define AL_INTRS	\
	(AL_IMR_RX_OK|AL_IMR_TX_OK|AL_IMR_RX_NOBUF|AL_IMR_RX_WATDOGTIMEO|\
	AL_IMR_TX_NOBUF|AL_IMR_TX_UNDERRUN|AL_IMR_BUS_ERR|		\
	AL_IMR_ABNORMAL|AL_IMR_NORMAL|AL_IMR_TX_IDLE|AL_IMR_RX_IDLE)

/*
 * Missed packer register.
 */
#define AL_MISSEDPKT_CNT	0x0000FFFF
#define AL_MISSEDPKT_OFLOW	0x00010000

/*
 * Serial I/O (EEPROM/ROM) bits.
 */
#define AL_SIO_EE_CS		0x00000001	/* EEPROM chip select */
#define AL_SIO_EE_CLK		0x00000002	/* EEPROM clock */
#define AL_SIO_EE_DATAIN	0x00000004	/* EEPROM data output */
#define AL_SIO_EE_DATAOUT	0x00000008	/* EEPROM data input */
#define AL_SIO_EESEL		0x00000800
#define AL_SIO_ROMCTL_WRITE	0x00002000
#define AL_SIO_ROMCTL_READ	0x00004000
#define AL_SIO_MII_CLK		0x00010000	/* MDIO clock */
#define AL_SIO_MII_DATAOUT	0x00020000	/* MDIO data out */
#define AL_SIO_MII_DIR		0x00040000	/* MDIO dir */
#define AL_SIO_MII_DATAIN	0x00080000	/* MDIO data in */

#define AL_EECMD_WRITE		0x140
#define AL_EECMD_READ		0x180
#define AL_EECMD_ERASE		0x1c0

#define AL_EE_NODEADDR_OFFSET	0x70
#define AL_EE_NODEADDR		4

/*
 * General purpose timer register
 */
#define AL_TIMER_VALUE		0x0000FFFF
#define AL_TIMER_CONTINUOUS	0x00010000

/*
 * Wakeup control/status register.
 */
#define AL_WU_LINKSTS		0x00000001	/* link status changed */
#define AL_WU_MAGICPKT		0x00000002	/* magic packet received */
#define AL_WU_WUPKT		0x00000004	/* wake up pkt received */
#define AL_WU_LINKSTS_ENB	0x00000100	/* enable linksts event */
#define AL_WU_MAGICPKT_ENB	0x00000200	/* enable magicpkt event */
#define AL_WU_WUPKT_ENB		0x00000400	/* enable wakeup pkt event */
#define AL_WU_LINKON_ENB	0x00010000	/* enable link on detect  */
#define AL_WU_LINKOFF_ENB	0x00020000	/* enable link off detect */
#define AL_WU_WKUPMATCH_PAT5	0x02000000	/* enable wkup pat 5 match */
#define AL_WU_WKUPMATCH_PAT4	0x04000000	/* enable wkup pat 4 match */
#define AL_WU_WKUPMATCH_PAT3	0x08000000	/* enable wkup pat 3 match */
#define AL_WU_WKUPMATCH_PAT2	0x10000000	/* enable wkup pat 2 match */
#define AL_WU_WKUPMATCH_PAT1	0x20000000	/* enable wkup pat 1 match */
#define AL_WU_CRCTYPE		0x40000000	/* crc: 0=0000, 1=ffff */

/*
 * Wakeup pattern structure.
 */
struct al_wu_pattern {
	u_int32_t		al_wu_bits[4];
};

struct al_wakeup {
	struct al_wu_pattern	al_wu_pat;
	u_int16_t		al_wu_crc1;
	u_int16_t		al_wu_offset1;
};

struct al_wakup_record {
	struct al_wakeup	al_wakeup[5];
};

/*
 * Watchdog timer register.
 */
#define AL_WDOG_JABDISABLE	0x00000001
#define AL_WDOG_NONJABBER	0x00000002
#define AL_WDOG_JABCLK		0x00000004
#define AL_WDOG_RXWDOG_DIS	0x00000010
#define AL_WDOG_RXWDOG_REL	0x00000020

/*
 * Assistant status register.
 */
#define AL_ISR2_ABNORMAL	0x00008000
#define AL_ISR2_NORMAL		0x00010000
#define AL_ISR2_RX_STATE	0x000E0000
#define AL_ISR2_TX_STATE	0x00700000
#define AL_ISR2_BUSERRTYPE	0x03800000
#define AL_ISR2_PAUSE		0x04000000	/* PAUSE frame received */
#define AL_ISR2_TX_DEFER	0x10000000
#define AL_ISR2_XCVR_INT	0x20000000
#define AL_ISR2_RX_EARLY	0x40000000
#define AL_ISR2_TX_EARLY	0x80000000

/*
 * Assistant mask register.
 */
#define AL_IMR2_ABNORMAL	0x00008000
#define AL_IMR2_NORMAL		0x00010000
#define AL_IMR2_PAUSE		0x04000000	/* PAUSE frame received */
#define AL_IMR2_TX_DEFER	0x10000000
#define AL_IMR2_XCVR_INT	0x20000000
#define AL_IMR2_RX_EARLY	0x40000000
#define AL_IMR2_TX_EARLY	0x80000000

/*
 * Command register, some bits loaded from EEPROM.
 */
#define AL_CMD_TXURUN_REC	0x00000001 /* enable TX underflow recovery */
#define AL_CMD_SOFTWARE_INT	0x00000002 /* software interrupt */
#define AL_CMD_DRT		0x0000000C /* drain receive threshold */
#define AL_CMD_RXTHRESH_ENB	0x00000010 /* rx threshold enable */
#define AL_CMD_PAUSE		0x00000020
#define AL_CMD_RST_WU_PTR	0x00000040 /* reset wakeup pattern reg. */
/* Values below loaded from EEPROM. */
#define AL_CMD_WOL_ENB		0x00040000 /* WOL enable */
#define AL_CMD_PM_ENB		0x00080000 /* pwr mgmt enable */
#define AL_CMD_RX_FIFO		0x00300000
#define AL_CMD_LED_MODE		0x00400000
#define AL_CMD_CURRENT_MODE	0x70000000
#define AL_CMD_D3COLD		0x80000000

/*
 * PCI performance counter.
 */
#define AL_PCI_DW_CNT		0x000000FF
#define AL_PCI_CLK		0xFFFF0000

/*
 * Power management command and status.
 */
#define AL_PWRM_PWR_STATE	0x00000003
#define AL_PWRM_PME_EN		0x00000100
#define AL_PWRM_DSEL		0x00001E00
#define AL_PWRM_DSCALE		0x00006000
#define AL_PWRM_PME_STAT	0x00008000

/*
 * TX burst count / timeout register.
 */
#define AL_TXB_TIMEO		0x00000FFF
#define AL_TXB_BURSTCNT		0x0000F000

/*
 * Flash PROM register.
 */
#define AL_PROM_DATA		0x0000000F
#define AL_PROM_ADDR		0x01FFFFF0
#define AL_PROM_WR_ENB		0x04000000
#define AL_PROM_BRA16_ON	0x80000000

/*
 * COMET TX/RX list structure.
 */

struct al_desc {
	u_int32_t		al_status;
	u_int32_t		al_ctl;
	u_int32_t		al_ptr1;
	u_int32_t		al_ptr2;
	/* Driver specific stuff. */
#ifdef __i386__
	u_int32_t		al_pad;
#endif
	struct mbuf		*al_mbuf;
	struct al_desc		*al_nextdesc;
};

#define al_data		al_ptr1
#define al_next		al_ptr2

#define AL_RXSTAT_FIFOOFLOW	0x00000001
#define AL_RXSTAT_CRCERR	0x00000002
#define AL_RXSTAT_DRIBBLE	0x00000004
#define AL_RXSTAT_WATCHDOG	0x00000010
#define AL_RXSTAT_FRAMETYPE	0x00000020	/* 0 == IEEE 802.3 */
#define AL_RXSTAT_COLLSEEN	0x00000040
#define AL_RXSTAT_GIANT		0x00000080
#define AL_RXSTAT_LASTFRAG	0x00000100
#define AL_RXSTAT_FIRSTFRAG	0x00000200
#define AL_RXSTAT_MULTICAST	0x00000400
#define AL_RXSTAT_RUNT		0x00000800
#define AL_RXSTAT_RXTYPE	0x00003000
#define AL_RXSTAT_RXERR		0x00008000
#define AL_RXSTAT_RXLEN		0x3FFF0000
#define AL_RXSTAT_OWN		0x80000000

#define AL_RXBYTES(x)		((x & AL_RXSTAT_RXLEN) >> 16)
#define AL_RXSTAT (AL_RXSTAT_FIRSTFRAG|AL_RXSTAT_LASTFRAG|AL_RXSTAT_OWN)

#define AL_RXCTL_BUFLEN1	0x00000FFF
#define AL_RXCTL_BUFLEN2	0x00FFF000
#define AL_RXCTL_RLINK		0x01000000
#define AL_RXCTL_RLAST		0x02000000

#define AL_TXSTAT_DEFER		0x00000001
#define AL_TXSTAT_UNDERRUN	0x00000002
#define AL_TXSTAT_LINKFAIL	0x00000003
#define AL_TXSTAT_COLLCNT	0x00000078
#define AL_TXSTAT_SQE		0x00000080
#define AL_TXSTAT_EXCESSCOLL	0x00000100
#define AL_TXSTAT_LATECOLL	0x00000200
#define AL_TXSTAT_NOCARRIER	0x00000400
#define AL_TXSTAT_CARRLOST	0x00000800
#define AL_TXSTAT_JABTIMEO	0x00004000
#define AL_TXSTAT_ERRSUM	0x00008000
#define AL_TXSTAT_OWN		0x80000000

#define AL_TXCTL_BUFLEN1	0x000007FF
#define AL_TXCTL_BUFLEN2	0x003FF800
#define AL_TXCTL_PAD		0x00800000
#define AL_TXCTL_TLINK		0x01000000
#define AL_TXCTL_TLAST		0x02000000
#define AL_TXCTL_NOCRC		0x04000000
#define AL_TXCTL_FIRSTFRAG	0x20000000
#define AL_TXCTL_LASTFRAG	0x40000000
#define AL_TXCTL_FINT		0x80000000

#define AL_MAXFRAGS		16
#define AL_RX_LIST_CNT		64
#define AL_TX_LIST_CNT		128
#define AL_MIN_FRAMELEN		60
#define AL_RXLEN		1536

#define AL_INC(x, y)	(x) = (x + 1) % y

struct al_list_data {
	struct al_desc		al_rx_list[AL_RX_LIST_CNT];
	struct al_desc		al_tx_list[AL_TX_LIST_CNT];
};

struct al_chain_data {
	int			al_tx_prod;
	int			al_tx_cons;
	int			al_tx_cnt;
	int			al_rx_prod;
};

struct al_type {
	u_int16_t		al_vid;
	u_int16_t		al_did;
	char			*al_name;
};

struct al_mii_frame {
	u_int8_t		mii_stdelim;
	u_int8_t		mii_opcode;
	u_int8_t		mii_phyaddr;
	u_int8_t		mii_regaddr;
	u_int8_t		mii_turnaround;
	u_int16_t		mii_data;
};

#define AL_MII_STARTDELIM	0x01
#define AL_MII_READOP		0x02
#define AL_MII_WRITEOP		0x01
#define AL_MII_TURNAROUND	0x02

struct al_softc {
	struct arpcom		arpcom;		/* interface info */
	struct ifmedia		ifmedia;	/* media info */
	bus_space_handle_t	al_bhandle;	/* bus space handle */
	bus_space_tag_t		al_btag;	/* bus space tag */
	struct resource		*al_res;
	struct resource		*al_irq;
	void			*al_intrhand;
	device_t		al_miibus;
	struct al_type		*al_info;	/* COMET adapter info */
	int			al_did;
	u_int8_t		al_unit;	/* interface number */
	struct al_list_data	*al_ldata;
	struct al_chain_data	al_cdata;
	u_int8_t		al_cachesize;
	struct callout_handle	al_stat_ch;
};

/*
 * register space access macros
 */
#define CSR_WRITE_4(sc, reg, val)	\
	bus_space_write_4(sc->al_btag, sc->al_bhandle, reg, val)
#define CSR_WRITE_2(sc, reg, val)	\
	bus_space_write_2(sc->al_btag, sc->al_bbhandle, reg, val)
#define CSR_WRITE_1(sc, reg, val)	\
	bus_space_write_1(sc->al_btag, sc->al_bhandle, reg, val)

#define CSR_READ_4(sc, reg)		\
	bus_space_read_4(sc->al_btag, sc->al_bhandle, reg)
#define CSR_READ_2(sc, reg)		\
	bus_space_read_2(sc->al_btag, sc->al_bhandle, reg)
#define CSR_READ_1(sc, reg)		\
	bus_space_read_1(sc->al_btag, sc->al_bhandle, reg)

#define AL_TIMEOUT		1000
#define ETHER_ALIGN		2

/*
 * General constants that are fun to know.
 *
 * ADMtek PCI vendor ID
 */
#define	AL_VENDORID		0x1317

/*
 * AL981 device IDs.
 */
#define AL_DEVICEID_AL981	0x0981

/*
 * AN985 device IDs.
 */
#define AL_DEVICEID_AN985	0x0985

/*
 * PCI low memory base and low I/O base register, and
 * other PCI registers.
 */

#define AL_PCI_VENDOR_ID	0x00
#define AL_PCI_DEVICE_ID	0x02
#define AL_PCI_COMMAND		0x04
#define AL_PCI_STATUS		0x06
#define AL_PCI_REVID		0x08
#define AL_PCI_CLASSCODE	0x09
#define AL_PCI_CACHELEN		0x0C
#define AL_PCI_LATENCY_TIMER	0x0D
#define AL_PCI_HEADER_TYPE	0x0E
#define AL_PCI_LOIO		0x10
#define AL_PCI_LOMEM		0x14
#define AL_PCI_BIOSROM		0x30
#define AL_PCI_INTLINE		0x3C
#define AL_PCI_INTPIN		0x3D
#define AL_PCI_MINGNT		0x3E
#define AL_PCI_MINLAT		0x0F
#define AL_PCI_RESETOPT		0x48
#define AL_PCI_EEPROM_DATA	0x4C

/* power management registers */
#define AL_PCI_CAPID		0x44 /* 8 bits */
#define AL_PCI_NEXTPTR		0x45 /* 8 bits */
#define AL_PCI_PWRMGMTCAP	0x46 /* 16 bits */
#define AL_PCI_PWRMGMTCTRL	0x48 /* 16 bits */

#define AL_PSTATE_MASK		0x0003
#define AL_PSTATE_D0		0x0000
#define AL_PSTATE_D1		0x0001
#define AL_PSTATE_D2		0x0002
#define AL_PSTATE_D3		0x0003
#define AL_PME_EN		0x0010
#define AL_PME_STATUS		0x8000

#ifdef __alpha__
#undef vtophys
#define vtophys(va)		alpha_XXX_dmamap((vm_offset_t)va)
#endif
