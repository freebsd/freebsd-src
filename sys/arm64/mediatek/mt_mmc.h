/*-
 * Copyright (c) 2019 - 2021 Priit Trees <trees@neti.ee>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD:
 */

#ifndef __MT_MMC_H__
#define __MT_MMC_H__

/* MediaTek SD driver Register */
#define	MTK_MSDC_CFG		(0x0)
#define  MTK_MSDC_CFG_MSDC			(1U << 0)
#define  MTK_MSDC_CFG_CCKPD			(1U << 1)
#define  MTK_MSDC_CFG_RST			(1U << 2)
#define  MTK_MSDC_CFG_PIO			(1U << 3)
#define  MTK_MSDC_CFG_CCKDRVE			(1U << 4)
#define  MTK_MSDC_CFG_BV18SDT 			(1U << 5)
#define  MTK_MSDC_CFG_BV18PSS			(1U << 6)
#define  MTK_MSDC_CFG_CCKSB			(1U << 7)
#define  MTK_MSDC_CFG_CCKDIV_SHIFT		8
#define	 MTK_MSDC_CFG_CCKDIV_MASK		(0xFFF << 8)
#if 0
#define  MTK_MSDC_CFG_CCKDIV_MASK		(0xFF << 8)
#define  MTK_MSDC_CFG_CCKMD			(1U << 16)
#define  MTK_MSDC_CFG_DDR			(1U << 17)
#endif
#define  MTK_MSDC_CFG_CCKMD			(1U << 20)
#define  MTK_MSDC_CFG_DDR			(1U << 21)
#define	 MTK_MSDC_CFG_HS400_CK_MODE_EXTRA	(1U << 22)
#define	MTK_MSDC_IOCON		(0x04)
#define  MTK_MSDC_IOCON_SDR104CKS		(1U << 0)
#define  MTK_MSDC_IOCON_RSPL			(1U << 1)
#define  MTK_MSDC_IOCON_RDSPL			(1U << 2)
#define  MTK_MSDC_IOCON_DDLSEL			(1U << 3)
#define  MTK_MSDC_IOCON_RDSPLSEL		(1U << 5)
#define  MTK_MSDC_IOCON_WDSPL			(1U << 8)
#define  MTK_MSDC_IOCON_WDSPLSEL		(1U << 9)
#define  MTK_MSDC_IOCON_WD0SPL			(1U << 10)
#define  MTK_MSDC_IOCON_WD1SPL			(1U << 11)
#define  MTK_MSDC_IOCON_WD2SPL			(1U << 12)
#define  MTK_MSDC_IOCON_WD3SPL			(1U << 13)
#define  MTK_MSDC_IOCON_RD0SPL			(1U << 16)
#define  MTK_MSDC_IOCON_RD1SPL			(1U << 17)
#define  MTK_MSDC_IOCON_RD2SPL			(1U << 18)
#define  MTK_MSDC_IOCON_RD3SPL			(1U << 19)
#define  MTK_MSDC_IOCON_RD4SPL			(1U << 20)
#define  MTK_MSDC_IOCON_RD5SPL			(1U << 21)
#define  MTK_MSDC_IOCON_RD6SPL			(1U << 22)
#define  MTK_MSDC_IOCON_RD7SPL			(1U << 23)
#define	MTK_MSDC_PS		(0x08)
#define  MTK_MSDC_PS_CDEN			(1U << 0)
#define  MTK_MSDC_PS_CDSTS			(1U << 1)
#define  MTK_MSDC_PS_CDDBCE_SHIFT		12
#define  MTK_MSDC_PS_CDDBCE_MASK		(0xF << 12)
#define  MTK_MSDC_PS_DAT_SHIFT			16
#define  MTK_MSDC_PS_DAT_MASK			(0xFF << 16)
#define  MTK_MSDC_PS_CMD			(1U << 24)
#define  MTK_MSDC_PS_SDWP			(1U << 31)
#define	MTK_MSDC_INT		(0x0c)
#define  MTK_MSDC_INT_MMCIRQ			(1U << 0)
#define  MTK_MSDC_INT_MSDCCDSC			(1U << 1)
#define  MTK_MSDC_INT_SDACDCRDY			(1U << 3)
#define  MTK_MSDC_INT_SDACDCTO			(1U << 4)
#define  MTK_MSDC_INT_SDACDRCRCER		(1U << 5)
#define  MTK_MSDC_INT_DMAQEPTY			(1U << 6)
#define  MTK_MSDC_INT_SDIOIRQ			(1U << 7)
#define  MTK_MSDC_INT_SDCRDY			(1U << 8)
#define  MTK_MSDC_INT_SDCTO			(1U << 9)
#define  MTK_MSDC_INT_SDRCRCER			(1U << 10)
#define  MTK_MSDC_INT_SDCSTA			(1U << 11)
#define  MTK_MSDC_INT_SDXFCPL			(1U << 12)
#define  MTK_MSDC_INT_DMAXFDNE			(1U << 13)
#define  MTK_MSDC_INT_SDDTO			(1U << 14)
#define  MTK_MSDC_INT_SDDCRCERR			(1U << 15)
#define  MTK_MSDC_INT_BDCSERR			(1U << 17)
#define  MTK_MSDC_INT_GPDCSERR			(1U << 18)
#define  MTK_MSDC_INT_DMAPROTECT		(1U << 19)
#define	MTK_MSDC_INTEN		(0x10)
#define  MTK_MSDC_INTEN_ENMMCIRQ		(1U << 0)
#define  MTK_MSDC_INTEN_ENMSDCCDSC		(1U << 1)
#define  MTK_MSDC_INTEN_ENSDACDCRDY		(1U << 3)
#define  MTK_MSDC_INTEN_ENSDACDCTO		(1U << 4)
#define  MTK_MSDC_INTEN_ENSDACDRCRCER		(1U << 5)
#define  MTK_MSDC_INTEN_ENDMAQEPTY		(1U << 6)
#define  MTK_MSDC_INTEN_ENSDIOIRQ		(1U << 7)
#define  MTK_MSDC_INTEN_ENSDCRDY		(1U << 8)
#define  MTK_MSDC_INTEN_ENSDCTO			(1U << 9)
#define  MTK_MSDC_INTEN_ENSDRCRCER		(1U << 10)
#define  MTK_MSDC_INTEN_ENSDCSTA		(1U << 11)
#define  MTK_MSDC_INTEN_ENSDXFCPL		(1U << 12)
#define  MTK_MSDC_INTEN_ENDMAXFDNE		(1U << 13)
#define  MTK_MSDC_INTEN_ENSDDTO			(1U << 14)
#define  MTK_MSDC_INTEN_ENSDDCRCERR		(1U << 15)
#define  MTK_MSDC_INTEN_ENBDCSERR		(1U << 17)
#define  MTK_MSDC_INTEN_ENGPDCSERR		(1U << 18)
#define  MTK_MSDC_INTEN_ENDMAPROTECT		(1U << 19)
#define	MTK_MSDC_FIFOCS		(0x14)
#define  MTK_MSDC_FIFOCS_RXFIFOCNT_SHIFT	0
#define  MTK_MSDC_FIFOCS_RXFIFOCNT_MASK		(0xFF << 0)
#define  MTK_MSDC_FIFOCS_TXFIFOCNT_SHIFT	16
#define  MTK_MSDC_FIFOCS_TXFIFOCNT_MASK		(0xFF << 16)
#define  MTK_MSDC_FIFOCS_FIFOCLR		(1U << 31)
#define	MTK_MSDC_TXDATA		(0x18)
#define	MTK_MSDC_RXDATA		(0x1c)
#define	MTK_SDC_CFG		(0x30)
#define  MTK_SDC_CFG_ENWKUPSDIOINT		(1U << 0)
#define  MTK_SDC_CFG_ENWKUPINS			(1U << 1)
#define  MTK_SDC_CFG_BUSWD_1BIT			(0x0 << 16)
#define  MTK_SDC_CFG_BUSWD_4BIT			(0x1 << 16)
#define  MTK_SDC_CFG_BUSWD_8BIT			(0x2 << 16)
#define  MTK_SDC_CFG_BUSWD_MASK			(0x3 << 16)
#define  MTK_SDC_CFG_SDIO			(1U << 19)
#define  MTK_SDC_CFG_SDIOIDE			(1U << 20)
#define  MTK_SDC_CFG_INTBGP			(1U << 21)
#define  MTK_SDC_CFG_DTOC_SHIFT			24
#define  MTK_SDC_CFG_DTOC_MASK			(0xFF << 24)
#define	MTK_SDC_CMD		(0x34)
#define  MTK_SDC_CMD_CMD_MASK			(0x1F << 0)
#define  MTK_SDC_CMD_BREAK			(1U << 6)
#define  MTK_SDC_CMD_RSPTYP_R1			(0x1 << 7)
#define  MTK_SDC_CMD_RSPTYP_R2			(0x2 << 7)
#define  MTK_SDC_CMD_RSPTYP_R3			(0x3 << 7)
#define  MTK_SDC_CMD_RSPTYP_R4			(0x4 << 7)
#define  MTK_SDC_CMD_RSPTYP_R1B			(0x7 << 7)
#define  MTK_SDC_CMD_DTYPE_SINGLE		(0x1 << 11)
#define  MTK_SDC_CMD_DTYPE_MULTI		(0x2 << 11)
#define  MTK_SDC_CMD_DTYPE_STREAM		(0x3 << 11)
#define  MTK_SDC_CMD_RW				(1U << 13)
#define  MTK_SDC_CMD_STOP			(1U << 14)
#define  MTK_SDC_CMD_GOIRQ			(1U << 15)
#define  MTK_SDC_CMD_LEN_SHIFT			16
#define  MTK_SDC_CMD_LEN_MASK			(0xFFF << 16)
#define  MTK_SDC_CMD_ACMD			(1U << 28)
#define	MTK_SDC_ARG		(0x38)
#define	MTK_SDC_STS		(0x3c)
#define  MTK_SDC_STS_SDCBSY			(1U << 0)
#define  MTK_SDC_STS_CMDBSY			(1U << 1)
#define  MTK_SDC_STS_CMD_WR_BUSY		(1U << 16)
#define  MTK_SDC_STS_MMCSWRCPL			(1U << 31)
#define	MTK_SDC_RESP0		(0x40)
#define	MTK_SDC_RESP1		(0x44)
#define	MTK_SDC_RESP2		(0x48)
#define	MTK_SDC_RESP3		(0x4c)
#define	MTK_SDC_BLK_NUM		(0x50)
#define	MTK_SDC_CSTS		(0x58)
#define	MTK_SDC_CSTS_EN		(0x5c)
#define	MTK_SDC_DATCRC_STS	(0x60)
#define  MTK_SDC_DATCRC_STS_DCSSP_MASK		(0xFF << 0)
#define	MTK_MSDC_DMA_SA		(0x90)
#define	MTK_MSDC_DMA_CA		(0x94)
#define	MTK_MSDC_DMA_CTRL	(0x98)
#define  MTK_MSDC_DMA_CTRL_DMASTART		(1U << 0)
#define  MTK_MSDC_DMA_CTRL_DMASTOP		(1U << 1)
#define  MTK_MSDC_DMA_CTRL_DMARSM		(1U << 2)
#define  MTK_MSDC_DMA_CTRL_DMAMOD		(1U << 8)
#define  MTK_MSDC_DMA_CTRL_DMAALIGN		(1U << 9)
#define  MTK_MSDC_DMA_CTRL_LASTBF		(1U << 10)
#define  MTK_MSDC_DMA_CTRL_SPLIT1K		(1U << 11)
#define  MTK_MSDC_DMA_CTRL_BSTSZ_8B		(0x3 << 12)
#define  MTK_MSDC_DMA_CTRL_BSTSZ_16B 		(0x4 << 12)
#define  MTK_MSDC_DMA_CTRL_BSTSZ_32B 		(0x5 << 12)
#define  MTK_MSDC_DMA_CTRL_BSTSZ_64B 		(0x6 << 12)
#define	MTK_MSDC_DMA_CFG	(0x9c)
#define  MTK_MSDC_DMA_CFG_DMASTS		(1U << 0)
#define  MTK_MSDC_DMA_CFG_DSCPCSEN		(1U << 1)
#define  MTK_MSDC_DMA_CFG_AHBHPROT2EN_MASK	(0x03 << 8)
#define  MTK_MSDC_DMA_CFG_AHBHPROT2EN_NUL	(0x01 << 8)
#define  MTK_MSDC_DMA_CFG_AHBHPROT2EN_ONE	(0x02 << 8)
#define  MTK_MSDC_DMA_CFG_MSDCACTIVEEN_MASK	(0x03 << 12)
#define  MTK_MSDC_DMA_CFG_MSDCACTIVEEN_NUL	(0x01 << 12)
#define  MTK_MSDC_DMA_CFG_MSDCACTIVEEN_ONE	(0x02 << 12)
#define  MTK_MSDC_DMA_CFG_DMACHKSUM12B		(1U << 16)
#define	MTK_MSDC_DBG_SEL	(0xa0)
#define	MTK_MSDC_DBG_OUT	(0xa4)
#define	MTK_MSDC_DMA_LENGTH	(0xa8)
#define	MTK_MSDC_PATCH_BIT0	(0xb0)
#define  MTK_MSDC_PATCH_BIT0_PTCH01		(1U << 1)
#define  MTK_MSDC_PATCH_BIT0_PTCH02		(1U << 2)
#define  MTK_MSDC_PATCH_BIT0_INTCKS_MASK	(0x07 << 7)
#define  MTK_MSDC_PATCH_BIT0_PTCH15		(1U << 15)
#define  MTK_MSDC_PATCH_BIT0_PTCH17		(1U << 17)
#define  MTK_MSDC_PATCH_BIT0_PTCH18_MASK	(0x0F << 18)
#define  MTK_MSDC_PATCH_BIT0_PTCH22_MASK	(0x0F << 22)
#define  MTK_MSDC_PATCH_BIT0_PTCH26		(1U << 26)
#define  MTK_MSDC_PATCH_BIT0_PTCH27		(1U << 27)
#define  MTK_MSDC_PATCH_BIT0_PTCH28		(1U << 28)
#define  MTK_MSDC_PATCH_BIT0_PTCH29		(1U << 29)
#define  MTK_MSDC_PATCH_BIT0_PTCH30		(1U << 30)
#define  MTK_MSDC_PATCH_BIT0_PTCH31		(1U << 31)
#define	MTK_MSDC_PATCH_BIT1	(0xb4)
#define  MTK_MSDC_PATCH_BIT1_WRTA_MASK		(0x07 << 0)
#define  MTK_MSDC_PATCH_BIT1_CMDTA_MASK		(0x07 << 3)
#define  MTK_MSDC_PATCH_BIT1_GETBUSYMARGIN	(1U << 6)
#define  MTK_MSDC_PATCH_BIT1_GETCRCMARGIN	(1U << 7)
#define  MTK_MSDC_PATCH_BIT1_BIAS28R2_MASK	(0x0F << 8)
#define  MTK_MSDC_PATCH_BIT1_BIAS28R1		(1U << 12)
#define  MTK_MSDC_PATCH_BIT1_BIAS28R0		(1U << 13)
#define  MTK_MSDC_PATCH_BIT1_HGDMACKEN		(1U << 23)
#define  MTK_MSDC_PATCH_BIT1_MSPCCKEN		(1U << 24)
#define  MTK_MSDC_PATCH_BIT1_MPSCCKEN		(1U << 25)
#define  MTK_MSDC_PATCH_BIT1_MVOLDTCKEN		(1U << 26)
#define  MTK_MSDC_PATCH_BIT1_MACMDCKEN		(1U << 27)
#define  MTK_MSDC_PATCH_BIT1_MSDCKEN		(1U << 28)
#define  MTK_MSDC_PATCH_BIT1_MWCTLCKEN		(1U << 29)
#define  MTK_MSDC_PATCH_BIT1_MRCTLCKEN		(1U << 30)
#define  MTK_MSDC_PATCH_BIT1_MSHBFCKEN		(1U << 31)
#if 0
#define	MTK_MSDC_PAD_TUNE	(0xec)
#define  MTK_MSDC_PAD_TUNE_DATWRDLY_MASK	(0x1F << 0)
#define  MTK_MSDC_PAD_TUNE_DATRRDLY_MASK	(0x1F << 8)
#define  MTK_MSDC_PAD_TUNE_CMDRDLY_MASK		(0x1F << 16)
#define  MTK_MSDC_PAD_TUNE_CMDRRDLY_MASK	(0x1F << 22)
#define  MTK_MSDC_PAD_TUNE_CLKTDLY_MASK		(0x1F << 27)
#define	MTK_MSDC_DAT_RDDLY0	(0xf0)
#define  MTK_MSDC_DAT_RDDLY0_DAT3RDDLY_MASK	(0x1F << 0)
#define  MTK_MSDC_DAT_RDDLY0_DAT2RDDLY_MASK	(0x1F << 8)
#define  MTK_MSDC_DAT_RDDLY0_DAT1RDDLY_MASK	(0x1F << 16)
#define  MTK_MSDC_DAT_RDDLY0_DAT0RDDLY_MASK	(0x1F << 0)
#define	MTK_MSDC_DAT_RDDLY1	(0xf4)
#define  MTK_MSDC_DAT_RDDLY1_DAT7RDDLY_MASK	(0x1F << 0)
#define  MTK_MSDC_DAT_RDDLY1_DAT6RDDLY_MASK	(0x1F << 8)
#define  MTK_MSDC_DAT_RDDLY1_DAT5RDDLY_MASK	(0x1F << 16)
#define  MTK_MSDC_DAT_RDDLY1_DAT4RDDLY_MASK	(0x1F << 24)
#define	MTK_MSDC_HW_DBG		(0xf8)
#define  MTK_MSDC_HW_DBG_DBG3SEL_MASK		(0xFF << 7)
#define  MTK_MSDC_HW_DBG_DBG2SEL_MASK		(0x3F << 8)
#define  MTK_MSDC_HW_DBG_DBG1SEL_MASK		(0x3F << 16)
#define  MTK_MSDC_HW_DBG_DBGWTSEL_MASK		(0x03 << 22)
#define  MTK_MSDC_HW_DBG_DBG0SEL_MASK		(0x3F << 24)
#define  MTK_MSDC_HW_DBG_DBGWSEL		(1U << 30)
#endif
#define	MTK_MSDC_PAD_TUNE	(0xf0)
#define  MTK_MSDC_PAD_TUNE_DATWRDLY_MASK	(0x1F << 0)
#define	 MTK_MSDC_PAD_TUNE_DELAYEN		(1U << 7)
#define  MTK_MSDC_PAD_TUNE_DATRRDLY_MASK	(0x1F << 8)
#define	 MTK_MSDC_PAD_TUNE_DATRRDLYSEL		(1U << 13)
#define	 MTK_MSDC_PAD_TUNE_RXDLYSEL		(1U << 15)
#define  MTK_MSDC_PAD_TUNE_CMDRDLY_MASK		(0x1F << 16)
#define	 MTK_MSDC_PAD_TUNE_CMDRRDLYSEL		(1U << 21)
#define  MTK_MSDC_PAD_TUNE_CMDRRDLY_MASK	(0x1F << 22)
#define  MTK_MSDC_PAD_TUNE_CLKTDLY_MASK		(0x1F << 27)

#define	MTK_MSDC_VERSION	(0x100)
#define	MTK_MSDC_ECO_VER	(0x104)

/* DMA Generic Packet Descriptor (GPD) Format */
struct mtk_mmc_dma_gpd {
	uint8_t			gpd_cfg1;
#define	MTK_GPD_HWO		(1U << 0)	/* Hardware Own */
#define	MTK_GPD_BDP		(1U << 1)	/* Buffer Descriptor Present */
	uint8_t			gpd_chksum;	/* GPD Checksum */
	uint16_t		gpd_cfg2;
#define	MTK_GPD_INT		(1U << 1)	/* Interrupt Generation Mask */
	uint32_t		next_gpd;	/* Next DMA GPD Pointer */
	uint32_t		buf_addr;	/* Data Buffer Pointer/
				 DMA BD pointer */
	uint16_t		buf_len;	/* Data Buffer Length */
	uint8_t			desc_ext_len;	/* Descriptor Extension len */
	uint8_t			resv;
	uint32_t		arg;
	uint32_t		block_num;	/* SD BLOCK_NUMBER */
	uint32_t		cmd;
};

/* DMA Buffer Descriptor (BD) Format */
struct mtk_mmc_dma_bd {
	uint8_t			bd_cfg1;
#define	MTK_BD_EOL		(1U << 0)	/* End of List */
	uint8_t			bd_chksum;	/* Buffer Descriptor Checksum */
	uint16_t		bd_cfg2;
#define	MTK_BD_B		(1U << 1)	/* Block Padding */
#define	MTK_BD_D		(1U << 2)
	uint32_t		next_bd;	/* Next BD Pointer */
	uint32_t		buf_addr;	/* Data Buffer Pointer */
	uint16_t		buf_len;	/* Data Buffer Length */
	uint16_t		resv;
};

#endif
