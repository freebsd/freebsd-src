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
 *	$Id: if_pnreg.h,v 1.4.2.7 1999/05/06 15:39:36 wpaul Exp $
 */

/*
 * PNIC register definitions.
 */

#define PN_BUSCTL		0x00	/* bus control */
#define PN_TXSTART		0x08	/* tx start demand */
#define PN_RXSTART		0x10	/* rx start demand */
#define PN_RXADDR		0x18	/* rx descriptor list start addr */
#define PN_TXADDR		0x20	/* tx descriptor list start addr */
#define PN_ISR			0x28	/* interrupt status register */
#define PN_NETCFG		0x30	/* network config register */
#define PN_IMR			0x38	/* interrupt mask */
#define PN_FRAMESDISCARDED	0x40	/* # of discarded frames */
#define PN_SIO			0x48	/* MII and ROM/EEPROM access */
#define PN_GEN			0x60	/* general purpose register */
#define PN_ENDEC		0x78	/* ENDEC general register */
#define PN_SIOPWR		0x90	/* serial eeprom power up */
#define PN_SIOCTL		0x98	/* EEPROM control register */
#define PN_MII			0xA0	/* MII access register */
#define PN_NWAY			0xB8	/* Internal NWAY register */

/*
 * Bus control bits.
 */
#define PN_BUSCTL_RESET		0x00000001
#define PN_BUSCTL_ARBITRATION	0x00000002
#define PN_BUSCTL_SKIPLEN	0x0000007C
#define PN_BUSCTL_BUF_BIGENDIAN	0x00000080
#define PN_BUSCTL_BURSTLEN	0x00003F00
#define PN_BUSCTL_CACHEALIGN	0x0000C000
#define PN_BUSCTL_TXPOLL	0x000E0000
#define PN_BUSCTL_MUSTBEONE	0x04000000

#define PN_SKIPLEN_1LONG	0x00000004
#define PN_SKIPLEN_2LONG	0x00000008
#define PN_SKIPLEN_3LONG	0x00000010
#define PN_SKIPLEN_4LONG	0x00000020
#define PN_SKIPLEN_5LONG	0x00000040

#define PN_CACHEALIGN_NONE	0x00000000
#define PN_CACHEALIGN_8LONG	0x00004000
#define PN_CACHEALIGN_16LONG	0x00008000
#define PN_CACHEALIGN_32LONG	0x0000C000

#define PN_BURSTLEN_USECA	0x00000000
#define PN_BURSTLEN_1LONG	0x00000100
#define PN_BURSTLEN_2LONG	0x00000200
#define PN_BURSTLEN_4LONG	0x00000400
#define PN_BURSTLEN_8LONG	0x00000800
#define PN_BURSTLEN_16LONG	0x00001000
#define PN_BURSTLEN_32LONG	0x00002000

#define PN_TXPOLL_OFF		0x00000000
#define PN_TXPOLL_200U		0x00020000
#define PN_TXPOLL_800U		0x00040000
#define PN_TXPOLL_1600U		0x00060000
#define PN_TXPOLL_12_8M		0x00080000
#define PN_TXPOLL_25_6M		0x000A0000
#define PN_TXPOLL_51_2M		0x000C0000
#define PN_TXPOLL_102_4M	0x000E0000

#define PN_BUSCTL_CONFIG	\
	(PN_CACHEALIGN_8LONG|PN_BURSTLEN_8LONG)

/*
 * Interrupt status bits.
 */
#define PN_ISR_TX_OK		0x00000001	/* packet tx ok */
#define PN_ISR_TX_IDLE		0x00000002	/* tx stopped */
#define PN_ISR_TX_NOBUF		0x00000004	/* no tx buffer available */
#define PN_ISR_TX_JABTIMEO	0x00000008	/* jabber timeout */
#define PN_ISR_LINKPASS		0x00000010	/* link test pass */
#define PN_ISR_TX_UNDERRUN	0x00000020	/* transmit underrun */
#define PN_ISR_RX_OK		0x00000040	/* packet rx ok */
#define PN_ISR_RX_NOBUF		0x00000080	/* rx buffer unavailable */
#define PN_ISR_RX_IDLE		0x00000100	/* rx stopped */
#define PN_ISR_RX_WATCHDOG	0x00000200	/* rx watchdog timeo */
#define PN_ISR_TX_EARLY		0x00000400	/* rx watchdog timeo */
#define PN_ISR_LINKFAIL		0x00001000
#define PN_ISR_BUS_ERR		0x00002000
#define PN_ISR_ABNORMAL		0x00008000
#define PN_ISR_NORMAL		0x00010000
#define PN_ISR_RX_STATE		0x000E0000
#define PN_ISR_TX_STATE		0x00700000
#define PN_ISR_BUSERRTYPE	0x03800000
#define PN_ISR_TXABORT		0x04000000	/* tx abort */

#define PN_RXSTATE_STOPPED	0x00000000	/* 000 - Stopped */
#define PN_RXSTATE_FETCH	0x00020000	/* 001 - Fetching descriptor */
#define PN_RXSTATE_ENDCHECK	0x00040000	/* 010 - check for rx end */
#define PN_RXSTATE_WAIT		0x00060000	/* 011 - waiting for packet */
#define PN_RXSTATE_SUSPEND	0x00080000	/* 100 - suspend rx */
#define PN_RXSTATE_CLOSE	0x000A0000	/* 101 - close rx desc */
#define PN_RXSTATE_FLUSH	0x000C0000	/* 110 - flush from FIFO */
#define PN_RXSTATE_DEQUEUE	0x000E0000	/* 111 - dequeue from FIFO */

#define PN_TXSTATE_RESET	0x00000000	/* 000 - reset */
#define PN_TXSTATE_FETCH	0x00100000	/* 001 - fetching descriptor */
#define PN_TXSTATE_WAITEND	0x00200000	/* 010 - wait for tx end */
#define PN_TXSTATE_READING	0x00300000	/* 011 - read and enqueue */
#define PN_TXSTATE_RSVD		0x00400000	/* 100 - reserved */
#define PN_TXSTATE_SETUP	0x00500000	/* 101 - setup packet */
#define PN_TXSTATE_SUSPEND	0x00600000	/* 110 - suspend tx */
#define PN_TXSTATE_CLOSE	0x00700000	/* 111 - close tx desc */

#define PN_BUSERR_PARITY	0x00000000
#define PN_BUSERR_MASTABRT	0x00800000
#define PN_BUSERR_TGTABRT	0x01000000
#define PN_BUSERR_RSVD1		0x01800000
#define PN_BUSERR_RSVD2		0x02000000

/*
 * Network config bits.
 */
#define PN_NETCFG_HASHPERF	0x00000001	/* 0 == perf, 1 == hash */
#define PN_NETCFG_RX_ON		0x00000002
#define PN_NETCFG_HASHONLY	0x00000004	/* 1 == allhash */
#define PN_NETCFG_RX_PASSERR	0x00000008
#define PN_NETCFG_INVERSFILT	0x00000010
#define PN_NETCFG_BACKOFF	0x00000020
#define PN_NETCFG_RX_PROMISC	0x00000040
#define PN_NETCFG_RX_ALLMULTI	0x00000080
#define PN_NETCFG_FLAKYOSC	0x00000100
#define PN_NETCFG_FULLDUPLEX	0x00000200
#define PN_NETCFG_OPERMODE	0x00000C00
#define PN_NETCFG_FORCECOLL	0x00001000
#define PN_NETCFG_TX_ON		0x00002000
#define PN_NETCFG_TX_THRESH	0x0000C000
#define PN_NETCFG_TX_BACKOFF	0x00020000
#define PN_NETCFG_MIIENB	0x00040000	/* 1 == MII, 0 == internal */
#define PN_NETCFG_HEARTBEAT	0x00080000	/* 1 == disabled */
#define PN_NETCFG_TX_IMMEDIATE	0x00100000
#define PN_NETCFG_STORENFWD	0x00200000
#define PN_NETCFG_SPEEDSEL	0x00400000	/* 1 == 10Mbps 0 == 100Mbps */
#define PN_NETCFG_PCS		0x00800000	/* 1 == 100baseTX */
#define PN_NETCFG_SCRAMBLER	0x01000000
#define PN_NETCFG_NO_RXCRC	0x20000000
#define PN_NETCFG_EXT_ENDEC	0x40000000	/* 1 == ext, 0 == int PHY */

#define PN_OPMODE_NORM		0x00000000
#define PN_OPMODE_INTLOOP	0x00000400
#define PN_OPMODE_EXTLOOP	0x00000800

#define PN_TXTHRESH_72BYTES	0x00000000
#define PN_TXTHRESH_96BYTES	0x00004000
#define PN_TXTHRESH_128BYTES	0x00008000
#define PN_TXTHRESH_160BYTES	0x0000C000

/*
 * Interrupt mask bits.
 */
#define PN_IMR_TX_OK		0x00000001	/* packet tx ok */
#define PN_IMR_TX_IDLE		0x00000002	/* tx stopped */
#define PN_IMR_TX_NOBUF		0x00000004	/* no tx buffer available */
#define PN_IMR_TX_JABTIMEO	0x00000008	/* jabber timeout */
#define PN_IMR_LINKPASS		0x00000010	/* link test pass */
#define PN_IMR_TX_UNDERRUN	0x00000020	/* transmit underrun */
#define PN_IMR_RX_OK		0x00000040	/* packet rx ok */
#define PN_IMR_RX_NOBUF		0x00000080	/* rx buffer unavailable */
#define PN_IMR_RX_IDLE		0x00000100	/* rx stopped */
#define PN_IMR_RX_WATCHDOG	0x00000200	/* rx watchdog timeo */
#define PN_IMR_TX_EARLY		0x00000400	/* rx watchdog timeo */
#define PN_IMR_BUS_ERR		0x00002000
#define PN_IMR_ABNORMAL		0x00008000
#define PN_IMR_NORMAL		0x00010000
#define PN_ISR_TXABORT		0x04000000	/* tx abort */

#define PN_INTRS							\
	(PN_IMR_RX_OK|PN_IMR_TX_OK|PN_IMR_RX_NOBUF|			\
	PN_IMR_TX_NOBUF|PN_IMR_TX_UNDERRUN|PN_IMR_BUS_ERR|		\
	PN_IMR_ABNORMAL|PN_IMR_NORMAL)

/*
 * Serial I/O (EEPROM/ROM) bits.
 */
#define PN_SIO_DATA		0x0000003F
#define PN_SIO_OPCODE		0x00000300
#define PN_SIO_BUSY		0x80000000

/*
 * SIOCTL/EEPROM bits
 */
#define PN_EE_READ		0x600

/*
 * General purpose register bits.
 */
#define PN_GEN_CTL		0x000000F0
#define PN_GEN_100TX_LINK	0x00000008
#define PN_GEN_BNC_ENB		0x00000004
#define PN_GEN_100TX_LOOP	0x00000002	/* 1 == normal, 0 == loop */
#define PN_GEN_SPEEDSEL		0x00000001	/* 1 == 100Mbps, 0 == 10Mbps */
#define PN_GEN_MUSTBEONE	0x00000030

/*
 * General ENDEC bits.
 */
#define PN_ENDEC_JABBERDIS	0x000000001	/* 1 == disable, 0 == enable */

/*
 * MII bits.
 */
#define PN_MII_DATA		0x0000FFFF
#define PN_MII_REGADDR		0x007C0000
#define PN_MII_PHYADDR		0x0F800000
#define PN_MII_OPCODE		0x30000000
#define PN_MII_RESERVED		0x00020000
#define PN_MII_BUSY		0x80000000

#define PN_MII_READ		0x60020000 /* read PHY command */
#define PN_MII_WRITE		0x50020000 /* write PHY command */

/*
 * Internal PHY NWAY register bits.
 */
#define PN_NWAY_RESET		0x00000001	/* reset */
#define PN_NWAY_PDOWN		0x00000002	/* power down */
#define PN_NWAY_BYPASS		0x00000004	/* bypass */
#define PN_NWAY_AUILOWCUR	0x00000008	/* AUI low current */
#define PN_NWAY_TPEXTEND	0x00000010	/* low squelch voltage */
#define PN_NWAY_POLARITY	0x00000020	/* 0 == on, 1 == off */
#define PN_NWAY_TP		0x00000040	/* 1 == tp, 0 == AUI */
#define PN_NWAY_AUIVOLT		0x00000080	/* 1 == full, 0 == half */
#define PN_NWAY_DUPLEX		0x00000100	/* 1 == full, 0 == half */
#define PN_NWAY_LINKTEST	0x00000200	/* 0 == on, 1 == off */
#define PN_NWAY_AUTODETECT	0x00000400	/* 1 == off, 0 == on */
#define PN_NWAY_SPEEDSEL	0x00000800	/* 0 == 10, 1 == 100 */
#define PN_NWAY_NWAY_ENB	0x00001000	/* 0 == off, 1 == on */
#define PN_NWAY_CAP10HALF	0x00002000
#define PN_NWAY_CAP10FULL	0x00004000
#define	PN_NWAY_CAP100FULL	0x00008000
#define PN_NWAY_CAP100HALF	0x00010000
#define PN_NWAY_CAP100T4	0x00020000
#define PN_NWAY_AUTONEGRSTR	0x02000000
#define PN_NWAY_REMFAULT	0x04000000
#define PN_NWAY_LPAR10HALF	0x08000000
#define PN_NWAY_LPAR10FULL	0x10000000
#define PN_NWAY_LPAR100FULL	0x20000000
#define PN_NWAY_LPAR100HALF	0x40000000
#define PN_NWAY_LPAR100T4	0x80000000

/*
 * Nway register bits that must be set to turn on to initiate
 * an autoneg session with all modes advertized and AUI disabled.
 */
#define PN_NWAY_AUTOENB							\
	(PN_NWAY_AUILOWCUR|PN_NWAY_TPEXTEND|PN_NWAY_POLARITY|PN_NWAY_TP	\
	 |PN_NWAY_NWAY_ENB|PN_NWAY_CAP10HALF|PN_NWAY_CAP10FULL|		\
	 PN_NWAY_CAP100FULL|PN_NWAY_CAP100HALF|PN_NWAY_CAP100T4|	\
	 PN_NWAY_AUTONEGRSTR)

#define PN_NWAY_MODE_10HD						\
	(PN_NWAY_CAP10HALF|PN_NWAY_CAP10FULL|		\
	 PN_NWAY_CAP100FULL|PN_NWAY_CAP100HALF|PN_NWAY_CAP100T4|	\
	 PN_NWAY_AUILOWCUR|PN_NWAY_TPEXTEND|PN_NWAY_POLARITY|		\
	 PN_NWAY_TP)

#define PN_NWAY_MODE_10FD						\
	(PN_NWAY_CAP10HALF|PN_NWAY_CAP10FULL|		\
	 PN_NWAY_CAP100FULL|PN_NWAY_CAP100HALF|PN_NWAY_CAP100T4|	\
	 PN_NWAY_AUILOWCUR|PN_NWAY_TPEXTEND|PN_NWAY_POLARITY|		\
	 PN_NWAY_TP|PN_NWAY_DUPLEX)

#define PN_NWAY_MODE_100HD						\
	(PN_NWAY_CAP10HALF|PN_NWAY_CAP10FULL|		\
	 PN_NWAY_CAP100FULL|PN_NWAY_CAP100HALF|PN_NWAY_CAP100T4|	\
	 PN_NWAY_AUILOWCUR|PN_NWAY_TPEXTEND|PN_NWAY_POLARITY|		\
	 PN_NWAY_TP|PN_NWAY_SPEEDSEL)

#define PN_NWAY_MODE_100FD						\
	(PN_NWAY_CAP10HALF|PN_NWAY_CAP10FULL|		\
	 PN_NWAY_CAP100FULL|PN_NWAY_CAP100HALF|PN_NWAY_CAP100T4|	\
	 PN_NWAY_AUILOWCUR|PN_NWAY_TPEXTEND|PN_NWAY_POLARITY|		\
	 PN_NWAY_TP|PN_NWAY_SPEEDSEL|PN_NWAY_DUPLEX)

#define PN_NWAY_MODE_100T4 PN_NWAY_MODE_100HD

#define PN_NWAY_LPAR							\
	(PN_NWAY_LPAR10HALF|PN_NWAY_LPAR10FULL|PN_NWAY_LPAR100HALF|	\
	 PN_NWAY_LPAR100FULL|PN_NWAY_LPAR100T4)

/*
 * Size of a setup frame.
 */
#define PN_SFRAME_LEN		192

/*
 * PNIC TX/RX list structure.
 */

struct pn_desc {
	u_int32_t		pn_status;
	u_int32_t		pn_ctl;
	u_int32_t		pn_ptr1;
	u_int32_t		pn_ptr2;
};

#define pn_data		pn_ptr1
#define pn_next		pn_ptr2


#define RX_RXSTAT_FIFOOFLOW	0x00000001
#define PN_RXSTAT_CRCERR	0x00000002
#define PN_RXSTAT_DRIBBLE	0x00000004
#define PN_RXSTAT_WATCHDOG	0x00000010
#define PN_RXSTAT_FRAMETYPE	0x00000020	/* 0 == IEEE 802.3 */
#define PN_RXSTAT_COLLSEEN	0x00000040
#define PN_RXSTAT_GIANT		0x00000080
#define PN_RXSTAT_LASTFRAG	0x00000100
#define PN_RXSTAT_FIRSTFRAG	0x00000200
#define PN_RXSTAT_MULTICAST	0x00000400
#define PN_RXSTAT_RUNT		0x00000800
#define PN_RXSTAT_RXTYPE	0x00003000
#define PN_RXSTAT_RXERR		0x00008000
#define PN_RXSTAT_RXLEN		0x7FFF0000
#define PN_RXSTAT_OWN		0x80000000

#define PN_RXBYTES(x)		((x & PN_RXSTAT_RXLEN) >> 16)
#define PN_RXSTAT (PN_RXSTAT_FIRSTFRAG|PN_RXSTAT_LASTFRAG|PN_RXSTAT_OWN)

#define PN_RXCTL_BUFLEN1	0x00000FFF
#define PN_RXCTL_BUFLEN2	0x00FFF000
#define PN_RXCTL_RLINK		0x01000000
#define PN_RXCTL_RLAST		0x02000000

#define PN_TXSTAT_DEFER		0x00000001
#define PN_TXSTAT_UNDERRUN	0x00000002
#define PN_TXSTAT_LINKFAIL	0x00000003
#define PN_TXSTAT_COLLCNT	0x00000078
#define PN_TXSTAT_SQE		0x00000080
#define PN_TXSTAT_EXCESSCOLL	0x00000100
#define PN_TXSTAT_LATECOLL	0x00000200
#define PN_TXSTAT_NOCARRIER	0x00000400
#define PN_TXSTAT_CARRLOST	0x00000800
#define PN_TXSTAT_JABTIMEO	0x00004000
#define PN_TXSTAT_ERRSUM	0x00008000
#define PN_TXSTAT_OWN		0x80000000

#define PN_TXCTL_BUFLEN1	0x000007FF
#define PN_TXCTL_BUFLEN2	0x003FF800
#define PN_TXCTL_FILTTYPE0	0x00400000
#define PN_TXCTL_PAD		0x00800000
#define PN_TXCTL_TLINK		0x01000000
#define PN_TXCTL_TLAST		0x02000000
#define PN_TXCTL_NOCRC		0x04000000
#define PN_TXCTL_SETUP		0x08000000
#define PN_TXCTL_FILTTYPE1	0x10000000
#define PN_TXCTL_FIRSTFRAG	0x20000000
#define PN_TXCTL_LASTFRAG	0x40000000
#define PN_TXCTL_FINT		0x80000000

#define PN_FILTER_PERFECT	0x00000000
#define PN_FILTER_HASHPERF	0x00400000
#define PN_FILTER_INVERSE	0x10000000
#define PN_FILTER_HASHONLY	0x10400000

#define PN_MAXFRAGS		16
#define PN_RX_LIST_CNT		64
#define PN_TX_LIST_CNT		128
#define PN_MIN_FRAMELEN		60
#define PN_FRAMELEN		1536
#define PN_RXLEN		1518

/*
 * A tx 'super descriptor' is actually 16 regular descriptors
 * back to back.
 */
struct pn_txdesc {
	struct pn_desc		pn_frag[PN_MAXFRAGS];
};

#define PN_TXNEXT(x)	x->pn_ptr->pn_frag[x->pn_lastdesc].pn_next
#define PN_TXSTATUS(x)	x->pn_ptr->pn_frag[x->pn_lastdesc].pn_status
#define PN_TXCTL(x)	x->pn_ptr->pn_frag[x->pn_lastdesc].pn_ctl
#define PN_TXDATA(x)	x->pn_ptr->pn_frag[x->pn_lastdesc].pn_data

#define PN_TXOWN(x)	x->pn_ptr->pn_frag[0].pn_status

struct pn_list_data {
	struct pn_desc		pn_rx_list[PN_RX_LIST_CNT];
	struct pn_txdesc	pn_tx_list[PN_TX_LIST_CNT];
};

struct pn_chain {
	struct pn_txdesc	*pn_ptr;
	struct mbuf		*pn_mbuf;
	struct pn_chain		*pn_nextdesc;
	u_int8_t		pn_lastdesc;
};

struct pn_chain_onefrag {
	struct pn_desc		*pn_ptr;
	struct mbuf		*pn_mbuf;
	struct pn_chain_onefrag	*pn_nextdesc;
};

struct pn_chain_data {
	struct pn_desc		pn_sframe;
	u_int32_t		pn_sbuf[PN_SFRAME_LEN/sizeof(u_int32_t)];
	struct pn_chain_onefrag	pn_rx_chain[PN_RX_LIST_CNT];
	struct pn_chain		pn_tx_chain[PN_TX_LIST_CNT];

	struct pn_chain_onefrag	*pn_rx_head;

	struct pn_chain		*pn_tx_head;
	struct pn_chain		*pn_tx_tail;
	struct pn_chain		*pn_tx_free;
};

struct pn_type {
	u_int16_t		pn_vid;
	u_int16_t		pn_did;
	char			*pn_name;
};

struct pn_mii_frame {
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
#define PN_MII_STARTDELIM	0x01
#define PN_MII_READOP		0x02
#define PN_MII_WRITEOP		0x01
#define PN_MII_TURNAROUND	0x02

#define PN_FLAG_FORCEDELAY	1
#define PN_FLAG_SCHEDDELAY	2
#define PN_FLAG_DELAYTIMEO	3	

struct pn_softc {
	struct arpcom		arpcom;		/* interface info */
	struct ifmedia		ifmedia;	/* media info */
	bus_space_handle_t	pn_bhandle;	/* bus space handle */
	bus_space_tag_t		pn_btag;	/* bus space tag */
	struct pn_type		*pn_info;	/* PNIC adapter info */
	struct pn_type		*pn_pinfo;	/* phy info */
	u_int8_t		pn_unit;	/* interface number */
	u_int8_t		pn_type;
	u_int8_t		pn_phy_addr;	/* PHY address */
	u_int8_t		pn_tx_pend;	/* TX pending */
	u_int8_t		pn_want_auto;
	u_int8_t		pn_autoneg;
	caddr_t			pn_ldata_ptr;
#ifdef PN_RX_BUG_WAR
#define PN_168_REV	16
#define PN_169_REV	32
#define PN_169B_REV	33
	u_int8_t		pn_rx_war;
	u_int8_t		pn_cachesize;
	struct pn_chain_onefrag	*pn_rx_bug_save;
	unsigned char           *pn_rx_buf;
#endif
	struct pn_list_data	*pn_ldata;
	struct pn_chain_data	pn_cdata;
};

/*
 * register space access macros
 */
#define CSR_WRITE_4(sc, reg, val)	\
	bus_space_write_4(sc->pn_btag, sc->pn_bhandle, reg, val)
#define CSR_WRITE_2(sc, reg, val)	\
	bus_space_write_2(sc->pn_btag, sc->pn_bhandle, reg, val)
#define CSR_WRITE_1(sc, reg, val)	\
	bus_space_write_1(sc->pn_btag, sc->pn_bhandle, reg, val)

#define CSR_READ_4(sc, reg)		\
	bus_space_read_4(sc->pn_btag, sc->pn_bhandle, reg)
#define CSR_READ_2(sc, reg)		\
	bus_space_read_2(sc->pn_btag, sc->pn_bhandle, reg)
#define CSR_READ_1(sc, reg)		\
	bus_space_read_1(sc->pn_btag, sc->pn_bhandle, reg)

#define PN_TIMEOUT		1000

/*
 * General constants that are fun to know.
 *
 * Lite-On PNIC PCI vendor ID
 */
#define	PN_VENDORID		0x11AD

/*
 * Lite-On PNIC PCI device ID.
 */
#define	PN_DEVICEID_PNIC	0x0002

/*
 * The 82c168 chip has the same PCI vendor/device ID as the
 * 82c169, but a different revision. Assume that any revision
 * between 0x10 an 0x1F is an 82c168.
 */
#define PN_REVMASK		0xF0
#define PN_REVID_82C168		0x10
#define PN_REVID_82C169		0x20

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

#define PN_PCI_VENDOR_ID	0x00
#define PN_PCI_DEVICE_ID	0x02
#define PN_PCI_COMMAND		0x04
#define PN_PCI_STATUS		0x06
#define PN_PCI_REVISION		0x08
#define PN_PCI_CLASSCODE	0x09
#define PN_PCI_CACHELEN		0x0C
#define PN_PCI_LATENCY_TIMER	0x0D
#define PN_PCI_HEADER_TYPE	0x0E
#define PN_PCI_LOIO		0x10
#define PN_PCI_LOMEM		0x14
#define PN_PCI_BIOSROM		0x30
#define PN_PCI_INTLINE		0x3C
#define PN_PCI_INTPIN		0x3D
#define PN_PCI_MINGNT		0x3E
#define PN_PCI_MINLAT		0x0F
#define PN_PCI_RESETOPT		0x48
#define PN_PCI_EEPROM_DATA	0x4C

/* power management registers */
#define PN_PCI_CAPID		0xDC /* 8 bits */
#define PN_PCI_NEXTPTR		0xDD /* 8 bits */
#define PN_PCI_PWRMGMTCAP	0xDE /* 16 bits */
#define PN_PCI_PWRMGMTCTRL	0xE0 /* 16 bits */

#define PN_PSTATE_MASK		0x0003
#define PN_PSTATE_D0		0x0000
#define PN_PSTATE_D1		0x0002
#define PN_PSTATE_D2		0x0002
#define PN_PSTATE_D3		0x0003
#define PN_PME_EN		0x0010
#define PN_PME_STATUS		0x8000

#define PHY_UNKNOWN		6

#define PN_PHYADDR_MIN		0x00
#define PN_PHYADDR_MAX		0x1F

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
