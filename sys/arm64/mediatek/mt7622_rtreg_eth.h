/*-
* SPDX-License-Identifier: BSD-2-Clause-FreeBSD
*
* Copyright (c) 2009, Aleksandr Rybalko
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
* 1. Redistributions of source code must retain the above copyright
*    notice unmodified, this list of conditions, and the following
*    disclaimer.
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
*
* $FreeBSD$
*/

#ifndef _MT7622_RTREG_H_
#define	_MT7622_RTREG_H_

#define	RT_READ(sc, reg)				\
       bus_space_read_4((sc)->bst, (sc)->bsh, reg)

#define	RT_WRITE(sc, reg, val)				\
       bus_space_write_4((sc)->bst, (sc)->bsh, reg, val)

#define	RT_GDMA1_BASE		0x500
#define RT_GDMA2_BASE		RT_GDMA1_BASE + 0x1000
#define RT_GDM_BASE(gmac)	((gmac == 0) ? RT_GDMA1_BASE : RT_GDMA2_BASE)

#define	RT_GDM_IG_CTRL(gmac)	(RT_GDM_BASE(gmac) + 0x00)
#define	    INSV_EN		(1<<25)
#define	    STAG_EN		(1<<24)
#define	    GDM_ICS_EN		(1<<22)
#define	    GDM_TCS_EN		(1<<21)
#define	    GDM_UCS_EN		(1<<20)
#define	    GDM_DROP_256B	(1<<19)
#define	    GDM_STRPCRC		(1<<16)
#define	    GDM_UFRC_P_SHIFT	12
#define	    GDM_BFRC_P_SHIFT	8
#define	    GDM_MFRC_P_SHIFT	4
#define	    GDM_OFRC_P_SHIFT	0
#define	    GDM_XFRC_P_MASK	0x07
#define	    GDM_DST_PORT_CPU	0
#define	    GDM_DST_PORT_GDMA1	1
#define	    GDM_DST_PORT_GDMA2	2
#define     GDM_DST_PORT_PPE	4
#define     GDM_DST_PORT_QDMA	5
#define	    GDM_DST_PORT_DISCARD 7

#define RT_GDM_MAC_LSB(gmac)  (RT_GDM_BASE(gmac) + 0x08)
#define RT_GDM_MAC_MSB(gmac)  (RT_GDM_BASE(gmac) + 0x0c)

#define RT5350_PDMA_BASE 0x0800

#define        RT5350_TX_BASE_PTR(qid)			\
       ((qid) * 0x10 + RT5350_PDMA_BASE + 0x000)
#define        RT5350_TX_MAX_CNT(qid)			\
       ((qid) * 0x10 + RT5350_PDMA_BASE + 0x004)
#define        RT5350_TX_CTX_IDX(qid)			\
       ((qid) * 0x10 + RT5350_PDMA_BASE + 0x008)
#define        RT5350_TX_DTX_IDX(qid)			\
       ((qid) * 0x10 + RT5350_PDMA_BASE + 0x00C)

#define        RT5350_RX_BASE_PTR(qid)			\
       ((qid) * 0x10 + RT5350_PDMA_BASE + 0x100)
#define        RT5350_RX_MAX_CNT(qid)			\
       ((qid) * 0x10 + RT5350_PDMA_BASE + 0x104)
#define        RT5350_RX_CALC_IDX(qid)			\
       ((qid) * 0x10 + RT5350_PDMA_BASE + 0x108)
#define        RT5350_RX_DRX_IDX(qid)			\
       ((qid) * 0x10 + RT5350_PDMA_BASE + 0x10C)

#define RT5350_PDMA_GLO_CFG	(RT5350_PDMA_BASE + 0x204)
#define	    FE_RX_2B_OFFSET	(1<<31)
#define	    FE_TX_WB_DDONE	(1<<6)
#define	    FE_DMA_BT_SIZE4	(0<<4)
#define	    FE_DMA_BT_SIZE8	(1<<4)
#define	    FE_DMA_BT_SIZE16	(2<<4)
#define	    FE_RX_DMA_BUSY	(1<<3)
#define	    FE_RX_DMA_EN	(1<<2)
#define	    FE_TX_DMA_BUSY	(1<<1)
#define	    FE_TX_DMA_EN	(1<<0)

#define RT5350_PDMA_RST_IDX 	(RT5350_PDMA_BASE + 0x208)
#define	    FE_RST_DRX_IDX1	(1<<17)
#define	    FE_RST_DRX_IDX0	(1<<16)
#define	    FE_RST_DTX_IDX3	(1<<3)
#define	    FE_RST_DTX_IDX2	(1<<2)
#define	    FE_RST_DTX_IDX1	(1<<1)
#define	    FE_RST_DTX_IDX0	(1<<0)

#define RT5350_DELAY_INT_CFG	(RT5350_PDMA_BASE + 0x20C)
#define	    TXDLY_INT_EN 	(1<<31)
#define	    TXMAX_PINT_SHIFT	24
#define	    TXMAX_PTIME_SHIFT	16
#define	    RXDLY_INT_EN	(1<<15)
#define	    RXMAX_PINT_SHIFT	8
#define	    RXMAX_PTIME_SHIFT	0

#define RT5350_PDMA_INT_STATUS    (RT5350_PDMA_BASE + 0x220)
#define	    RT5350_INT_RX_COHERENT      (1<<31)
#define	    RT5350_RX_DLY_INT           (1<<30)
#define	    RT5350_INT_TX_COHERENT      (1<<29)
#define	    RT5350_TX_DLY_INT           (1<<28)
#define	    RT5350_INT_RXQ3_DONE	       (1<<19)
#define	    RT5350_INT_RXQ2_DONE        (1<<18)
#define	    RT5350_INT_RXQ1_DONE	       (1<<17)
#define	    RT5350_INT_RXQ0_DONE        (1<<16)
#define	    RT5350_INT_TXQ3_DONE        (1<<3)
#define	    RT5350_INT_TXQ2_DONE        (1<<2)
#define	    RT5350_INT_TXQ1_DONE        (1<<1)
#define	    RT5350_INT_TXQ0_DONE        (1<<0)
#define RT5350_PDMA_INT_ENABLE	(RT5350_PDMA_BASE + 0x228)
#define RT5350_PDMA_SCH_CFG0	(RT5350_PDMA_BASE + 0x280)

#define	CNTR_BASE 0x2400
#define CNTR_GDM(gmac)		(CNTR_BASE + (((gmac) == 0 ) ? 0x00 : 0x40))
#define	    GDM_RX_GBCNT_LSB(gmac)	(CNTR_GDM(gmac) + 0x00)
#define	    GDM_RX_GBCNT_MSB(gmac)	(CNTR_GDM(gmac) + 0x04)
#define	    GDM_RX_GPCNT(gmac)		(CNTR_GDM(gmac) + 0x08)
#define	    GDM_RX_OERCNT(gmac)		(CNTR_GDM(gmac) + 0x10)
#define	    GDM_RX_FERCNT(gmac)		(CNTR_GDM(gmac) + 0x14)
#define	    GDM_RX_SHORT_ERCNT(gmac) 	(CNTR_GDM(gmac) + 0x18)
#define	    GDM_RX_LONG_ERCNT(gmac)	(CNTR_GDM(gmac) + 0x1C)
#define	    GDM_RX_CSUM_ERCNT(gmac)	(CNTR_GDM(gmac) + 0x20)
#define     GDM_RX_FCCNT(gmac)		(CNTR_GDM(gmac) + 0x24)
#define	    GDM_TX_SKIPCNT(gmac)	(CNTR_GDM(gmac) + 0x28)
#define	    GDM_TX_COLCNT(gmac)		(CNTR_GDM(gmac) + 0x2C)
#define	    GDM_TX_GBCNT_LSB(gmac)	(CNTR_GDM(gmac) + 0x30)
#define	    GDM_TX_GBCNT_MSB(gmac)	(CNTR_GDM(gmac) + 0x34)
#define	    GDM_TX_GPCNT(gmac)		(CNTR_GDM(gmac) + 0x38)

#define	GE_PORT_BASE 0x10000
#define	MDIO_ACCESS	GE_PORT_BASE + 0x04
#define	    MDIO_CMD_ONGO		(1<<31)
#define	    MDIO_PHYREG_ADDR_MASK	0x3e000000
#define	    MDIO_PHYREG_ADDR_SHIFT	25
#define	    MDIO_PHY_ADDR_MASK		0x01f00000
#define	    MDIO_PHY_ADDR_SHIFT		20
#define	    MDIO_CMD_MASK		0x000c0000
#define	    MDIO_CMD_SHIFT		18
#define	    MDIO_CMD_WRITE		0x1
#define	    MDIO_CMD_READ		0x2
#define	    MDIO_CMD_READ_C45		0x3
#define	    MDIO_ST_MASK		0x30000
#define	    MDIO_ST_SHIFT		16
#define	    MDIO_ST_C45			0x0
#define	    MDIO_ST_C22			0x1
#define	    MDIO_PHY_DATA_MASK		0x0000ffff
#define	    MDIO_PHY_DATA_SHIFT		0

#define MAC_P_MCR(gmac)		(GE_PORT_BASE + 0x100 + (gmac) * 0x100)
#define	   MAX_RX_JUMBO_MASK	0xf0000000
#define	   MAX_RX_JUMBO_SHIFT	28
#define	   MAX_RX_JUMBO_2K	0x2	/*2 Kbytes (maxi. length on FE/GDM) */
#define	   MAC_RX_PKT_LEN_MASK	0x03000000
#define	   MAC_RX_PKT_LEN_SHIFT	18
#define	   MAC_RX_PKT_LEN_1518	0x0
#define	   MAC_RX_PKT_LEN_1536	0x1
#define	   MAC_RX_PKT_LEN_1552	0x2
#define	   MAC_RX_PKT_LEN_JUMBO	0x3	/* MAX_RX_JUMBO */
#define	   MTCC_LMT_MASK	0x00F00000
#define	   MTCC_LMT_SHIFT	20
#define	   IPG_CFG_MASK		0x000c0000
#define	   IPG_CFG_SHIFT	18
#define	   IPG_CFG_96BIT	0x0
#define	   IPG_CFG_96BIT_WS_IFG	0x2
#define	   IPG_CFG_64BIT	0x3
#define	   MAC_MODE		(1 << 16)
#define	   FORCE_MODE		(1 << 15)
#define	   MAC_TX_EN 		(1 << 14)
#define	   MAC_RX_EN		(1 << 13)
#define	   PRMBL_LMT_EN		(1 << 10)
#define	   BKOFF_EN		(1 << 9)
#define	   BACKPR_EN		(1 << 8)
#define	   FORCE_EEE1G		(1 << 7)
#define	   FORCE_EEE100		(1 << 6)
#define	   FORCE_RX_FC		(1 << 5)
#define	   FORCE_TX_FC		(1 << 4)
#define	   FORCE_SPD_MASK	0x0000000c
#define	   FORCE_SPD_SHIFT	2
#define	   FORCE_SPD_10M	0x0
#define	   FORCE_SPD_100M	0x1
#define	   FORCE_SPD_1000M	0x2
#define	   FORCE_DPX		(1 << 1)
#define	   FORCE_LINK		(1 << 0)

#endif /* _IF_RTREG_H_ */

