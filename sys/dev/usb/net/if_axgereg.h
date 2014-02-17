/*-
 * Copyright (c) 2013 Kevin Lo
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

#define	AX88179_PHY_ID			0x03
#define	AXGE_MCAST_FILTER_SIZE		8
#define	AXGE_MAXGE_MCAST		64
#define	AXGE_EEPROM_LEN			0x40
#define	AXGE_RX_CHECKSUM		1
#define	AXGE_TX_CHECKSUM		2

#define	AXGE_ACCESS_MAC			0x01
#define	AXGE_ACCESS_PHY			0x02
#define	AXGE_ACCESS_WAKEUP		0x03
#define	AXGE_ACCESS_EEPROM		0x04
#define	AXGE_ACCESS_EFUSE		0x05
#define	AXGE_RELOAD_EEPROM_EFUSE	0x06
#define	AXGE_WRITE_EFUSE_EN		0x09
#define	AXGE_WRITE_EFUSE_DIS		0x0A
#define	AXGE_ACCESS_MFAB		0x10

#define	AXGE_LINK_STATUS		0x02
#define	AXGE_LINK_STATUS_USB_FS		0x01
#define	AXGE_LINK_STATUS_USB_HS		0x02
#define	AXGE_LINK_STATUS_USB_SS		0x04

#define	AXGE_SROM_ADDR			0x07
#define	AXGE_SROM_DATA_LOW		0x08
#define	AXGE_SROM_DATA_HIGH		0x09
#define	AXGE_SROM_CMD			0x0a
#define	AXGE_SROM_CMD_RD		0x04	/* EEprom read command */
#define	AXGE_SROM_CMD_WR		0x08	/* EEprom write command */
#define	AXGE_SROM_CMD_BUSY		0x10	/* EEprom access module busy */

#define	AXGE_RX_CTL			0x0b
#define	AXGE_RX_CTL_DROPCRCERR		0x0100 /* Drop CRC error packet */
#define	AXGE_RX_CTL_IPE			0x0200 /* 4-byte IP header alignment */
#define	AXGE_RX_CTL_TXPADCRC		0x0400 /* Csum value in rx header 3 */
#define	AXGE_RX_CTL_START		0x0080 /* Ethernet MAC start */
#define	AXGE_RX_CTL_AP			0x0020 /* Accept physical address from
						  multicast array */
#define	AXGE_RX_CTL_AM			0x0010
#define	AXGE_RX_CTL_AB			0x0008
#define	AXGE_RX_CTL_HA8B		0x0004
#define	AXGE_RX_CTL_AMALL		0x0002 /* Accept all multicast frames */
#define	AXGE_RX_CTL_PRO			0x0001 /* Promiscuous Mode */
#define	AXGE_RX_CTL_STOP		0x0000 /* Stop MAC */

#define	AXGE_NODE_ID			0x10
#define	AXGE_MULTI_FILTER_ARRY		0x16

#define	AXGE_MEDIUM_STATUS_MODE		0x22
#define	AXGE_MEDIUM_GIGAMODE		0x0001
#define	AXGE_MEDIUM_FULL_DUPLEX		0x0002
#define	AXGE_MEDIUM_ALWAYS_ONE		0x0004
#define	AXGE_MEDIUM_EN_125MHZ		0x0008
#define	AXGE_MEDIUM_RXFLOW_CTRLEN	0x0010
#define	AXGE_MEDIUM_TXFLOW_CTRLEN	0x0020
#define	AXGE_MEDIUM_RECEIVE_EN		0x0100
#define	AXGE_MEDIUM_PS			0x0200
#define	AXGE_MEDIUM_JUMBO_EN		0x8040

#define	AXGE_MONITOR_MODE		0x24
#define	AXGE_MONITOR_MODE_RWLC		0x02
#define	AXGE_MONITOR_MODE_RWMP		0x04
#define	AXGE_MONITOR_MODE_RWWF		0x08
#define	AXGE_MONITOR_MODE_RW_FLAG	0x10
#define	AXGE_MONITOR_MODE_PMEPOL	0x20
#define	AXGE_MONITOR_MODE_PMETYPE	0x40

#define	AXGE_GPIO_CTRL			0x25
#define	AXGE_GPIO_CTRL_GPIO3EN		0x80
#define	AXGE_GPIO_CTRL_GPIO2EN		0x40
#define	AXGE_GPIO_CTRL_GPIO1EN		0x20

#define	AXGE_PHYPWR_RSTCTL		0x26
#define	AXGE_PHYPWR_RSTCTL_BZ		0x0010
#define	AXGE_PHYPWR_RSTCTL_IPRL		0x0020
#define	AXGE_PHYPWR_RSTCTL_AUTODETACH	0x1000

#define	AXGE_RX_BULKIN_QCTRL		0x2e
#define	AXGE_RX_BULKIN_QCTRL_TIME	0x01
#define	AXGE_RX_BULKIN_QCTRL_IFG	0x02
#define	AXGE_RX_BULKIN_QCTRL_SIZE	0x04

#define	AXGE_RX_BULKIN_QTIMR_LOW	0x2f
#define	AXGE_RX_BULKIN_QTIMR_HIGH	0x30
#define	AXGE_RX_BULKIN_QSIZE		0x31
#define	AXGE_RX_BULKIN_QIFG		0x32

#define	AXGE_CLK_SELECT			0x33
#define	AXGE_CLK_SELECT_BCS		0x01
#define	AXGE_CLK_SELECT_ACS		0x02
#define	AXGE_CLK_SELECT_ACSREQ		0x10
#define	AXGE_CLK_SELECT_ULR		0x08

#define	AXGE_RXCOE_CTL			0x34
#define	AXGE_RXCOE_IP			0x01
#define	AXGE_RXCOE_TCP			0x02
#define	AXGE_RXCOE_UDP			0x04
#define	AXGE_RXCOE_ICMP			0x08
#define	AXGE_RXCOE_IGMP			0x10
#define	AXGE_RXCOE_TCPV6		0x20
#define	AXGE_RXCOE_UDPV6		0x40
#define	AXGE_RXCOE_ICMV6		0x80

#define	AXGE_TXCOE_CTL			0x35
#define	AXGE_TXCOE_IP			0x01
#define	AXGE_TXCOE_TCP			0x02
#define	AXGE_TXCOE_UDP			0x04
#define	AXGE_TXCOE_ICMP			0x08
#define	AXGE_TXCOE_IGMP			0x10
#define	AXGE_TXCOE_TCPV6		0x20
#define	AXGE_TXCOE_UDPV6		0x40
#define	AXGE_TXCOE_ICMV6		0x80

#define	AXGE_PAUSE_WATERLVL_HIGH	0x54
#define	AXGE_PAUSE_WATERLVL_LOW		0x55

#define	AXGE_EEP_EFUSE_CORRECT		0x00
#define	AX88179_EEPROM_MAGIC		0x17900b95

#define	AXGE_CONFIG_IDX			0	/* config number 1 */
#define	AXGE_IFACE_IDX			0

#define	AXGE_RXHDR_CRC_ERR		0x80000000
#define	AXGE_RXHDR_L4_ERR		(1 << 8)
#define	AXGE_RXHDR_L3_ERR		(1 << 9)

#define	AXGE_RXHDR_L4_TYPE_ICMP		2
#define	AXGE_RXHDR_L4_TYPE_IGMP		3
#define	AXGE_RXHDR_L4_TYPE_TCMPV6	5

#define	AXGE_RXHDR_L3_TYPE_IP		1
#define	AXGE_RXHDR_L3_TYPE_IPV6		2

#define	AXGE_RXHDR_L4_TYPE_MASK		0x1c
#define	AXGE_RXHDR_L4_TYPE_UDP		4
#define	AXGE_RXHDR_L4_TYPE_TCP		16
#define	AXGE_RXHDR_L3CSUM_ERR		2
#define	AXGE_RXHDR_L4CSUM_ERR		1
#define	AXGE_RXHDR_CRC_ERR		0x80000000
#define	AXGE_RXHDR_DROP_ERR		0x40000000

struct axge_csum_hdr {
	uint16_t cstatus;
#define	AXGE_CSUM_HDR_L4_CSUM_ERR	0x0001
#define	AXGE_CSUM_HDR_L3_CSUM_ERR	0x0002
#define	AXGE_CSUM_HDR_L4_TYPE_UDP	0x0004
#define	AXGE_CSUM_HDR_L4_TYPE_ICMP	0x0008
#define	AXGE_CSUM_HDR_L4_TYPE_IGMP	0x000C
#define	AXGE_CSUM_HDR_L4_TYPE_TCP	0x0010
#define	AXGE_CSUM_HDR_L4_TYPE_TCPV6	0x0014
#define	AXGE_CSUM_HDR_L4_TYPE_MASK	0x001C
#define	AXGE_CSUM_HDR_L3_TYPE_IPV4	0x0020
#define	AXGE_CSUM_HDR_L3_TYPE_IPV6	0x0040
#define	AXGE_CSUM_HDR_VLAN_MASK		0x0700
	uint16_t len;
#define	AXGE_CSUM_HDR_LEN_MASK		0x1FFF
#define	AXGE_CSUM_HDR_CRC_ERR		0x2000
#define	AXGE_CSUM_HDR_MII_ERR		0x4000
#define	AXGE_CSUM_HDR_DROP		0x8000
} __packed;

#define	AXGE_CSUM_RXBYTES(x)	((x) & AXGE_CSUM_HDR_LEN_MASK)

#define	GET_MII(sc)		uether_getmii(&(sc)->sc_ue)

/* The interrupt endpoint is currently unused by the ASIX part. */
enum {
	AXGE_BULK_DT_WR,
	AXGE_BULK_DT_RD,
	AXGE_N_TRANSFER,
};

struct axge_softc {
	struct usb_ether	sc_ue;
	struct mtx		sc_mtx;
	struct usb_xfer		*sc_xfer[AXGE_N_TRANSFER];
	int			sc_phyno;

	int			sc_flags;
#define	AXGE_FLAG_LINK		0x0001	/* got a link */
};

#define	AXGE_LOCK(_sc)			mtx_lock(&(_sc)->sc_mtx)
#define	AXGE_UNLOCK(_sc)		mtx_unlock(&(_sc)->sc_mtx)
#define	AXGE_LOCK_ASSERT(_sc, t)	mtx_assert(&(_sc)->sc_mtx, t)
