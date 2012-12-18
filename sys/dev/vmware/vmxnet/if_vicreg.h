/*-
 * Copyright (c) 2006 Reyk Floeter <reyk@openbsd.org>
 * Copyright (c) 2006 David Gwynne <dlg@openbsd.org>
 * Copyright (c) 2012 Bryan Venteicher <bryanv@freebsd.org>
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
 *
 * $OpenBSD: if_vic.c,v 1.77 2011/11/29 11:53:25 jsing Exp $
 *
 * $FreeBSD$
 */

#ifndef _IF_VICREG_H
#define _IF_VICREG_H

#define VIC_PCI_BAR		0

#define VIC_LANCE_SIZE		0x20
#define VIC_MORPH_SIZE		0x04
#define  VIC_MORPH_MASK			0xffff
#define  VIC_MORPH_LANCE		0x2934
#define  VIC_MORPH_VMXNET		0x4392
#define VIC_VMXNET_SIZE		0x40
#define VIC_LANCE_MINLEN	(VIC_LANCE_SIZE + VIC_MORPH_SIZE + \
				    VIC_VMXNET_SIZE)

#define VIC_MAGIC		0xbabe864f

/* Register address offsets */
#define VIC_DATA_ADDR		0x0000		/* Shared data address */
#define VIC_DATA_LENGTH		0x0004		/* Shared data length */
#define VIC_Tx_ADDR		0x0008		/* Tx pointer address */

/* Command register */
#define VIC_CMD			0x000c		/* Command register */
#define  VIC_CMD_INTR_ACK	0x0001	/* Acknowledge interrupt */
#define  VIC_CMD_MCASTFIL	0x0002	/* Multicast address filter */
#define   VIC_CMD_MCASTFIL_LENGTH	2
#define  VIC_CMD_IFF		0x0004	/* Interface flags */
#define   VIC_CMD_IFF_PROMISC	0x0001		/* Promiscuous enabled */
#define   VIC_CMD_IFF_BROADCAST	0x0002		/* Broadcast enabled */
#define   VIC_CMD_IFF_MULTICAST	0x0004		/* Multicast enabled */
#define  VIC_CMD_INTR_DISABLE	0x0020	/* Disable interrupts */
#define  VIC_CMD_INTR_ENABLE	0x0040	/* Enable interrupts */
#define  VIC_CMD_Tx_DONE	0x0100	/* Tx done register */
#define  VIC_CMD_NUM_Rx_BUF	0x0200	/* Number of Rx buffers */
#define  VIC_CMD_NUM_Tx_BUF	0x0400	/* Number of Tx buffers */
#define  VIC_CMD_NUM_PINNED_BUF	0x0800	/* Number of pinned buffers */
#define  VIC_CMD_HWCAP		0x1000	/* Capability register */
#define   VIC_CMD_HWCAP_SG		0x000001 /* Scatter-gather transmits */
#define   VIC_CMD_HWCAP_CSUM_IPv4	0x000002 /* TCP/UDP cksum */
#define   VIC_CMD_HWCAP_CSUM_ALL	0x000004 /* Hardware cksum */
#define   VIC_CMD_HWCAP_DMA_HIGH	0x000008 /* High DMA mapping */
#define   VIC_CMD_HWCAP_TOE		0x000010 /* TCP offload engine */
#define   VIC_CMD_HWCAP_TSO		0x000020 /* TCP segmentation offload */
#define   VIC_CMD_HWCAP_TSO_SW		0x000040 /* Software TCP segmentation */
#define   VIC_CMD_HWCAP_VPROM		0x000080 /* Virtual PROM available */
#define   VIC_CMD_HWCAP_VLAN_Tx		0x000100 /* Hardware VLAN MTU Rx */
#define   VIC_CMD_HWCAP_VLAN_Rx		0x000200 /* Hardware VLAN MTU Tx */
#define   VIC_CMD_HWCAP_VLAN_SW		0x000400 /* Software VLAN MTU */
#define   VIC_CMD_HWCAP_WOL		0x000800 /* Wake on Lan */
#define   VIC_CMD_HWCAP_ENB_INTR_INLINE	0x001000 /* XXX Not sure */
#define   VIC_CMD_HWCAP_ENB_HEADER_COPY	0x002000 /* XXX Not sure */
#define   VIC_CMD_HWCAP_TX_CHAIN	0x004000 /* Tx with multiple desc */
#define   VIC_CMD_HWCAP_RX_CHAIN	0x008000 /* Rx with multiple desc */
#define   VIC_CMD_HWCAP_LPD		0x010000 /* Large packet delivery */
#define   VIC_CMD_HWCAP_BPF		0x020000 /* XXX Not sure */
#define   VIC_CMD_HWCAP_SG_SPAN_PAGES	0x040000 /* XXX Not sure */
#define   VIC_CMD_HWCAP_CSUM_IPv6	0x080000 /* IPv6 cksum */
#define   VIC_CMD_HWCAP_TSO_IPv6	0X100000 /* IPv6 TSO */

#define  VIC_CMD_HWCAP_BITS 						 \
    "\20\1SG\2CSUM4\3CSUM\4HDMA\5TOE\6TSO\7TSOSW\10VPROM\11VLANTx"	 \
    "\12VLANRx\13VLANSW\14WOL\15INTRINLINE\16HDRCPY\17TxCHAIN\20RxCHAIN" \
    "\21LPD\22BPF\23SGSPAN\24CSUM6\25TSO6"

#define VIC_CMD_HWCAP_CSUM (VIC_CMD_HWCAP_CSUM_IPv4 | VIC_CMD_HWCAP_CSUM_ALL)
#define VIC_CMD_HWCAP_VLAN (VIC_CMD_HWCAP_VLAN_Tx | VIC_CMD_HWCAP_VLAN_Rx | \
				VIC_CMD_HWCAP_VLAN_SW)

#define  VIC_CMD_FEATURE	0x2000	/* Additional feature register */
#define   VIC_CMD_FEATURE_0_Tx		0x0001
#define   VIC_CMD_FEATURE_TSO		0x0002
#define   VIC_CMD_FEATURE_JUMBO		0x0004
#define   VIC_CMD_FEATURE_LPD		0x0008

#define  VIC_CMD_FEATURE_BITS	"\20\1ZEROTx\2TSO\3JUMBO\4LPD"

#define VIC_LLADDR		0x0010		/* MAC address register */
#define VIC_VERSION_MINOR	0x0018		/* Minor version register */
#define VIC_VERSION_MAJOR	0x001c		/* Major version register */
#define VIC_VERSION_MAJOR_M	0xffff0000

/* Status register */
#define VIC_STATUS		0x0020
#define  VIC_STATUS_CONNECTED		0x01
#define  VIC_STATUS_ENABLED		0x02

#define VIC_TOE_ADDR		0x0024		/* TCP offload address */

/* Virtual PROM address */
#define VIC_VPROM		0x0028
#define VIC_VPROM_LENGTH	6

/* Shared DMA data structures */

struct vic_sg {
	uint32_t	sg_addr_low;
	uint16_t	sg_addr_high;
	uint16_t	sg_length;
} __packed;

#define VIC_SG_MAX		6
#define VIC_SG_ADDR_MACH	0
#define VIC_SG_ADDR_PHYS	1
#define VIC_SG_ADDR_VIRT	3

struct vic_sgarray {
	uint16_t	sa_addr_type;
	uint16_t	sa_length;
	struct vic_sg	sa_sg[VIC_SG_MAX];
} __packed;

struct vic_rxdesc {
	uint64_t	rx_physaddr;
	uint32_t	rx_buflength;
	uint32_t	rx_length;
	uint16_t	rx_owner;
	uint16_t	rx_flags;
	uint32_t	rx_priv;
} __packed;

#define VIC_RX_FLAGS_CSUMHW_OK	0x0001
#define VIC_RX_FLAGS_FRAG	0x0002 /* Rest of packet in second ring */
#define VIC_RX_FLAGS_FRAG_EOP	0x0004

struct vic_txdesc {
	uint16_t		tx_flags;
	uint16_t		tx_owner;
	uint32_t		tx_priv;
	uint32_t		tx_tsomss;
	struct vic_sgarray	tx_sa;
} __packed;

#define VIC_TX_FLAGS_KEEP	0x0001
#define VIC_TX_FLAGS_TXURN	0x0002
#define VIC_TX_FLAGS_CSUMHW	0x0004
#define VIC_TX_FLAGS_TSO	0x0008
#define VIC_TX_FLAGS_PINNED	0x0010
#define VIC_TX_FLAGS_CHAINED	0x0020
#define VIC_TX_FLAGS_QRETRY	0x1000

struct vic_stats {
	uint32_t		vs_tx_count;
	uint32_t		vs_tx_packets;
	uint32_t		vs_tx_0copy;
	uint32_t		vs_tx_copy;
	uint32_t		vs_tx_maxpending;
	uint32_t		vs_tx_stopped;
	uint32_t		vs_tx_overrun;
	uint32_t		vs_intr;
	uint32_t		vs_rx_packets;
	uint32_t		vs_rx_underrun;
} __packed;

#define VIC_NRXRINGS		2
#define VIC_FRAG_RXRING_IDX	1

struct vic_data {
	uint32_t		vd_magic;

	struct {
		uint32_t	length;
		uint32_t	nextidx;
	}			vd_rx[VIC_NRXRINGS];

	uint32_t		vd_irq;
	uint32_t		vd_iff;
	uint32_t		vd_mcastfil[VIC_CMD_MCASTFIL_LENGTH];
	uint32_t		vd_reserved1[1];
	uint32_t		vd_tx_length;
	uint32_t		vd_tx_curidx;
	uint32_t		vd_tx_nextidx;
	uint32_t		vd_tx_stopped;
	uint32_t		vd_tx_triggerlvl;
	uint32_t		vd_tx_queued;
	uint32_t		vd_reserved2[1];
	uint32_t		vd_rx_total_bufs;
	uint64_t		vd_rx_physaddr;
	uint32_t		vd_reserved3[2];
	uint16_t		vd_tx_maxfrags;
	uint16_t		vd_features;
	uint32_t		vd_rx_saved_nextidx[VIC_NRXRINGS];
	uint32_t		vd_tx_saved_nextidx;
	uint32_t		vd_length;
	uint32_t		vd_rx_offset[VIC_NRXRINGS];
	uint32_t		vd_tx_offset;
	uint32_t		vd_debug;
	uint32_t		vd_tx_physaddr;
	uint32_t		vd_tx_physaddr_length;
	uint32_t		vd_tx_maxlength;

	struct vic_stats	vd_stats;
} __packed;

#define VIC_OWNER_DRIVER	0
#define VIC_OWNER_DRIVER_PEND	1
#define VIC_OWNER_NIC		2
#define VIC_OWNER_NIC_PEND	3
#define VIC_OWNER_NIC_FRAG	4
#define VIC_OWNER_DRIVER_FRAG	5

#define VIC_JUMBO_FRAMELEN	9018
#define VIC_JUMBO_MTU		(VIC_JUMBO_FRAMELEN - ETHER_HDR_LEN - \
				    ETHER_CRC_LEN)

#define VIC_NBUF		128
#define VIC_ENHANCED_NRXBUF	256
#define VIC_ENHANCED_NTXBUF	256
#define VIC_INC(_x, _y)		(_x) = ((_x) + 1) % (_y)

#define VIC_MIN_FRAMELEN	(ETHER_MIN_LEN - ETHER_CRC_LEN)
#define VIC_TXURN_WARN(_sc)	((_sc)->vic_tx_pending >= \
				    ((_sc)->vic_tx_nbufs - 5))
#define VIC_TXURN(_sc)		((_sc)->vic_tx_pending >= (_sc)->vic_tx_nbufs)

#endif /* _IF_VICREG_H */
