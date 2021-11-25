/*-
 * Copyright (c) 2021 Alstom Group.
 * Copyright (c) 2021 Semihalf.
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DEV_FLASH_FLEX_SPI_H_
#define	DEV_FLASH_FLEX_SPI_H_

#define	BIT(x)				(1 << (x))

/* Registers used by the driver */
#define	FSPI_MCR0			0x00
#define	FSPI_MCR0_AHB_TIMEOUT(x)	((x) << 24)
#define	FSPI_MCR0_IP_TIMEOUT(x)		((x) << 16)
#define	FSPI_MCR0_LEARN_EN		BIT(15)
#define	FSPI_MCR0_SCRFRUN_EN		BIT(14)
#define	FSPI_MCR0_OCTCOMB_EN		BIT(13)
#define	FSPI_MCR0_DOZE_EN		BIT(12)
#define	FSPI_MCR0_HSEN			BIT(11)
#define	FSPI_MCR0_SERCLKDIV		BIT(8)
#define	FSPI_MCR0_ATDF_EN		BIT(7)
#define	FSPI_MCR0_ARDF_EN		BIT(6)
#define	FSPI_MCR0_RXCLKSRC(x)		((x) << 4)
#define	FSPI_MCR0_END_CFG(x)		((x) << 2)
#define	FSPI_MCR0_MDIS			BIT(1)
#define	FSPI_MCR0_SWRST			BIT(0)

#define	FSPI_MCR1			0x04
#define	FSPI_MCR1_SEQ_TIMEOUT(x)	((x) << 16)
#define	FSPI_MCR1_AHB_TIMEOUT(x)	(x)

#define	FSPI_MCR2			0x08
#define	FSPI_MCR2_IDLE_WAIT(x)		((x) << 24)
#define	FSPI_MCR2_SAMEDEVICEEN		BIT(15)
#define	FSPI_MCR2_CLRLRPHS		BIT(14)
#define	FSPI_MCR2_ABRDATSZ		BIT(8)
#define	FSPI_MCR2_ABRLEARN		BIT(7)
#define	FSPI_MCR2_ABR_READ		BIT(6)
#define	FSPI_MCR2_ABRWRITE		BIT(5)
#define	FSPI_MCR2_ABRDUMMY		BIT(4)
#define	FSPI_MCR2_ABR_MODE		BIT(3)
#define	FSPI_MCR2_ABRCADDR		BIT(2)
#define	FSPI_MCR2_ABRRADDR		BIT(1)
#define	FSPI_MCR2_ABR_CMD		BIT(0)

#define	FSPI_AHBCR			0x0c
#define	FSPI_AHBCR_RDADDROPT		BIT(6)
#define	FSPI_AHBCR_PREF_EN		BIT(5)
#define	FSPI_AHBCR_BUFF_EN		BIT(4)
#define	FSPI_AHBCR_CACH_EN		BIT(3)
#define	FSPI_AHBCR_CLRTXBUF		BIT(2)
#define	FSPI_AHBCR_CLRRXBUF		BIT(1)
#define	FSPI_AHBCR_PAR_EN		BIT(0)

#define	FSPI_INTEN			0x10
#define	FSPI_INTEN_SCLKSBWR		BIT(9)
#define	FSPI_INTEN_SCLKSBRD		BIT(8)
#define	FSPI_INTEN_DATALRNFL		BIT(7)
#define	FSPI_INTEN_IPTXWE		BIT(6)
#define	FSPI_INTEN_IPRXWA		BIT(5)
#define	FSPI_INTEN_AHBCMDERR		BIT(4)
#define	FSPI_INTEN_IPCMDERR		BIT(3)
#define	FSPI_INTEN_AHBCMDGE		BIT(2)
#define	FSPI_INTEN_IPCMDGE		BIT(1)
#define	FSPI_INTEN_IPCMDDONE		BIT(0)

#define	FSPI_INTR			0x14
#define	FSPI_INTR_SCLKSBWR		BIT(9)
#define	FSPI_INTR_SCLKSBRD		BIT(8)
#define	FSPI_INTR_DATALRNFL		BIT(7)
#define	FSPI_INTR_IPTXWE		BIT(6)
#define	FSPI_INTR_IPRXWA		BIT(5)
#define	FSPI_INTR_AHBCMDERR		BIT(4)
#define	FSPI_INTR_IPCMDERR		BIT(3)
#define	FSPI_INTR_AHBCMDGE		BIT(2)
#define	FSPI_INTR_IPCMDGE		BIT(1)
#define	FSPI_INTR_IPCMDDONE		BIT(0)

#define	FSPI_LUTKEY			0x18
#define	FSPI_LUTKEY_VALUE		0x5AF05AF0

#define	FSPI_LCKCR			0x1C

#define	FSPI_LCKER_LOCK			0x1
#define	FSPI_LCKER_UNLOCK		0x2

#define	FSPI_BUFXCR_INVALID_MSTRID	0xE
#define	FSPI_AHBRX_BUF0CR0		0x20
#define	FSPI_AHBRX_BUF1CR0		0x24
#define	FSPI_AHBRX_BUF2CR0		0x28
#define	FSPI_AHBRX_BUF3CR0		0x2C
#define	FSPI_AHBRX_BUF4CR0		0x30
#define	FSPI_AHBRX_BUF5CR0		0x34
#define	FSPI_AHBRX_BUF6CR0		0x38
#define	FSPI_AHBRX_BUF7CR0		0x3C
#define	FSPI_AHBRXBUF0CR7_PREF		BIT(31)

#define	FSPI_AHBRX_BUF0CR1		0x40
#define	FSPI_AHBRX_BUF1CR1		0x44
#define	FSPI_AHBRX_BUF2CR1		0x48
#define	FSPI_AHBRX_BUF3CR1		0x4C
#define	FSPI_AHBRX_BUF4CR1		0x50
#define	FSPI_AHBRX_BUF5CR1		0x54
#define	FSPI_AHBRX_BUF6CR1		0x58
#define	FSPI_AHBRX_BUF7CR1		0x5C

#define	FSPI_FLSHA1CR0			0x60
#define	FSPI_FLSHA2CR0			0x64
#define	FSPI_FLSHB1CR0			0x68
#define	FSPI_FLSHB2CR0			0x6C
#define	FSPI_FLSHXCR0_SZ_KB		10
#define	FSPI_FLSHXCR0_SZ(x)		((x) >> FSPI_FLSHXCR0_SZ_KB)

#define	FSPI_FLSHA1CR1			0x70
#define	FSPI_FLSHA2CR1			0x74
#define	FSPI_FLSHB1CR1			0x78
#define	FSPI_FLSHB2CR1			0x7C
#define	FSPI_FLSHXCR1_CSINTR(x)		((x) << 16)
#define	FSPI_FLSHXCR1_CAS(x)		((x) << 11)
#define	FSPI_FLSHXCR1_WA		BIT(10)
#define	FSPI_FLSHXCR1_TCSH(x)		((x) << 5)
#define	FSPI_FLSHXCR1_TCSS(x)		(x)

#define	FSPI_FLSHA1CR2			0x80
#define	FSPI_FLSHA2CR2			0x84
#define	FSPI_FLSHB1CR2			0x88
#define	FSPI_FLSHB2CR2			0x8C
#define	FSPI_FLSHXCR2_CLRINSP		BIT(24)
#define	FSPI_FLSHXCR2_AWRWAIT		BIT(16)
#define	FSPI_FLSHXCR2_AWRSEQN_SHIFT	13
#define	FSPI_FLSHXCR2_AWRSEQI_SHIFT	8
#define	FSPI_FLSHXCR2_ARDSEQN_SHIFT	5
#define	FSPI_FLSHXCR2_ARDSEQI_SHIFT	0

#define	FSPI_IPCR0			0xA0

#define	FSPI_IPCR1			0xA4
#define	FSPI_IPCR1_IPAREN		BIT(31)
#define	FSPI_IPCR1_SEQNUM_SHIFT		24
#define	FSPI_IPCR1_SEQID_SHIFT		16
#define	FSPI_IPCR1_IDATSZ(x)		(x)

#define	FSPI_IPCMD			0xB0
#define	FSPI_IPCMD_TRG			BIT(0)

#define	FSPI_DLPR			0xB4

#define	FSPI_IPRXFCR			0xB8
#define	FSPI_IPRXFCR_CLR		BIT(0)
#define	FSPI_IPRXFCR_DMA_EN		BIT(1)
#define	FSPI_IPRXFCR_WMRK(x)		((x) << 2)

#define	FSPI_IPTXFCR			0xBC
#define	FSPI_IPTXFCR_CLR		BIT(0)
#define	FSPI_IPTXFCR_DMA_EN		BIT(1)
#define	FSPI_IPTXFCR_WMRK(x)		((x) << 2)

#define	FSPI_DLLACR			0xC0
#define	FSPI_DLLACR_OVRDEN		BIT(8)

#define	FSPI_DLLBCR			0xC4
#define	FSPI_DLLBCR_OVRDEN		BIT(8)

#define	FSPI_STS0			0xE0
#define	FSPI_STS0_DLPHB(x)		((x) << 8)
#define	FSPI_STS0_DLPHA(x)		((x) << 4)
#define	FSPI_STS0_CMD_SRC(x)		((x) << 2)
#define	FSPI_STS0_ARB_IDLE		BIT(1)
#define	FSPI_STS0_SEQ_IDLE		BIT(0)

#define	FSPI_STS1			0xE4
#define	FSPI_STS1_IP_ERRCD(x)		((x) << 24)
#define	FSPI_STS1_IP_ERRID(x)		((x) << 16)
#define	FSPI_STS1_AHB_ERRCD(x)		((x) << 8)
#define	FSPI_STS1_AHB_ERRID(x)		(x)

#define	FSPI_AHBSPNST			0xEC
#define	FSPI_AHBSPNST_DATLFT(x)		((x) << 16)
#define	FSPI_AHBSPNST_BUFID(x)		((x) << 1)
#define	FSPI_AHBSPNST_ACTIVE		BIT(0)

#define	FSPI_IPRXFSTS			0xF0
#define	FSPI_IPRXFSTS_RDCNTR(x)		((x) << 16)
#define	FSPI_IPRXFSTS_FILL(x)		(x)

#define	FSPI_IPTXFSTS			0xF4
#define	FSPI_IPTXFSTS_WRCNTR(x)		((x) << 16)
#define	FSPI_IPTXFSTS_FILL(x)		(x)

#define	FSPI_RFDR			0x100
#define	FSPI_TFDR			0x180

#define	FSPI_LUT_BASE			0x200
#define	FSPI_LUT_REG(idx) \
	(FSPI_LUT_BASE + (idx) * 0x10)

/*
 * Commands
 */
#define	FSPI_CMD_WRITE_ENABLE		0x06
#define	FSPI_CMD_WRITE_DISABLE		0x04
#define	FSPI_CMD_READ_IDENT		0x9F
#define	FSPI_CMD_READ_STATUS		0x05
#define	FSPI_CMD_WRITE_STATUS		0x01
#define	FSPI_CMD_READ			0x03
#define	FSPI_CMD_FAST_READ		0x0B
#define	FSPI_CMD_PAGE_PROGRAM		0x02
#define	FSPI_CMD_SECTOR_ERASE		0xD8
#define	FSPI_CMD_BULK_ERASE		0xC7
#define	FSPI_CMD_BLOCK_4K_ERASE		0x20
#define	FSPI_CMD_BLOCK_32K_ERASE	0x52
#define	FSPI_CMD_ENTER_4B_MODE		0xB7
#define	FSPI_CMD_EXIT_4B_MODE		0xE9
#define	FSPI_CMD_READ_CTRL_REG		0x35
#define	FSPI_CMD_BANK_REG_WRITE		0x17	/* (spansion) */
#define	FSPI_CMD_SECTOR_ERASE_4B	0xDC
#define	FSPI_CMD_BLOCK_4K_ERASE_4B	0x21
#define	FSPI_CMD_BLOCK_32K_ERASE_4B	0x5C
#define	FSPI_CMD_PAGE_PROGRAM_4B	0x12
#define	FSPI_CMD_FAST_READ_4B		0x0C



/* register map end */

/* Instruction set for the LUT register. */
#define	LUT_STOP			0x00
#define	LUT_CMD				0x01
#define	LUT_ADDR			0x02
#define	LUT_CADDR_SDR			0x03
#define	LUT_MODE			0x04
#define	LUT_MODE2			0x05
#define	LUT_MODE4			0x06
#define	LUT_MODE8			0x07
#define	LUT_NXP_WRITE			0x08
#define	LUT_NXP_READ			0x09
#define	LUT_LEARN_SDR			0x0A
#define	LUT_DATSZ_SDR			0x0B
#define	LUT_DUMMY			0x0C
#define	LUT_DUMMY_RWDS_SDR		0x0D
#define	LUT_JMP_ON_CS			0x1F
#define	LUT_CMD_DDR			0x21
#define	LUT_ADDR_DDR			0x22
#define	LUT_CADDR_DDR			0x23
#define	LUT_MODE_DDR			0x24
#define	LUT_MODE2_DDR			0x25
#define	LUT_MODE4_DDR			0x26
#define	LUT_MODE8_DDR			0x27
#define	LUT_WRITE_DDR			0x28
#define	LUT_READ_DDR			0x29
#define	LUT_LEARN_DDR			0x2A
#define	LUT_DATSZ_DDR			0x2B
#define	LUT_DUMMY_DDR			0x2C
#define	LUT_DUMMY_RWDS_DDR		0x2D

/* LUT to operation mapping */
#define	LUT_FLASH_CMD_READ		0
#define	LUT_FLASH_CMD_JEDECID		1
#define	LUT_FLASH_CMD_STATUS_READ	2
#define	LUT_FLASH_CMD_PAGE_PROGRAM	3
#define	LUT_FLASH_CMD_WRITE_ENABLE	4
#define	LUT_FLASH_CMD_WRITE_DISABLE	5
#define	LUT_FLASH_CMD_SECTOR_ERASE	6


/*
 * Calculate number of required PAD bits for LUT register.
 *
 * The pad stands for the number of IO lines [0:7].
 * For example, the octal read needs eight IO lines,
 * so you should use LUT_PAD(8). This macro
 * returns 3 i.e. use eight (2^3) IP lines for read.
 */
#define	LUT_PAD(x) (fls(x) - 1)

/*
 * Macro for constructing the LUT entries with the following
 * register layout:
 *
 *  ---------------------------------------------------
 *  | INSTR1 | PAD1 | OPRND1 | INSTR0 | PAD0 | OPRND0 |
 *  ---------------------------------------------------
 */
#define	PAD_SHIFT		8
#define	INSTR_SHIFT		10
#define	OPRND_SHIFT		16

/* Macros for constructing the LUT register. */
#define	LUT_DEF(idx, ins, pad, opr)			  \
	((((ins) << INSTR_SHIFT) | ((pad) << PAD_SHIFT) | \
	(opr)) << (((idx) % 2) * OPRND_SHIFT))

#define	POLL_TOUT		5000
#define	NXP_FSPI_MAX_CHIPSELECT		4
#define	NXP_FSPI_MIN_IOMAP	SZ_4M

#define	DCFG_RCWSR1		0x100

/* Access flash memory using IP bus only */
#define	FSPI_QUIRK_USE_IP_ONLY	BIT(0)

#define	FLASH_SECTORSIZE	512

#define	TSTATE_STOPPED		0
#define	TSTATE_STOPPING		1
#define	TSTATE_RUNNING		2

#define	STATUS_SRWD		BIT(7)
#define	STATUS_BP2		BIT(4)
#define	STATUS_BP1		BIT(3)
#define	STATUS_BP0		BIT(2)
#define	STATUS_WEL		BIT(1)
#define	STATUS_WIP		BIT(0)


#endif /* DEV_FLASH_FLEX_SPI_H_ */
