/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 *  Copyright (c) 2009-2020 Alexander Motin <mav@FreeBSD.org>
 *  Copyright (c) 1997-2009 by Matthew Jacob
 *  All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 * 
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 
 *  THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 *  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 *  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *  OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *  SUCH DAMAGE.
 * 
 */
/*
 * Machine Independent (well, as best as possible) register
 * definitions for Qlogic ISP SCSI adapters.
 */
#ifndef	_ISPREG_H
#define	_ISPREG_H

/*
 * Hardware definitions for the Qlogic ISP  registers.
 */

/*
 * This defines types of access to various registers.
 *
 *  	R:		Read Only
 *	W:		Write Only
 *	RW:		Read/Write
 *
 *	R*, W*, RW*:	Read Only, Write Only, Read/Write, but only
 *			if RISC processor in ISP is paused.
 */

/*
 * NB:	The *_BLOCK definitions have no specific hardware meaning.
 *	They serve simply to note to the MD layer which block of
 *	registers offsets are being accessed.
 */
#define	_NREG_BLKS	2
#define	_BLK_REG_SHFT	7
#define	_BLK_REG_MASK	(1 << _BLK_REG_SHFT)
#define	BIU_BLOCK	(0 << _BLK_REG_SHFT)
#define	MBOX_BLOCK	(1 << _BLK_REG_SHFT)

#define	BIU_R2HST_INTR		(1 << 15)	/* RISC to Host Interrupt */
#define	BIU_R2HST_PAUSED	(1 <<  8)	/* RISC paused */
#define	BIU_R2HST_ISTAT_MASK	0xff		/* intr information && status */
#define		ISPR2HST_ROM_MBX_OK	0x1	/* ROM mailbox cmd done ok */
#define		ISPR2HST_ROM_MBX_FAIL	0x2	/* ROM mailbox cmd done fail */
#define		ISPR2HST_MBX_OK		0x10	/* mailbox cmd done ok */
#define		ISPR2HST_MBX_FAIL	0x11	/* mailbox cmd done fail */
#define		ISPR2HST_ASYNC_EVENT	0x12	/* Async Event */
#define		ISPR2HST_RSPQ_UPDATE	0x13	/* Response Queue Update */
#define		ISPR2HST_RSPQ_UPDATE2	0x14	/* Response Queue Update */
#define		ISPR2HST_RIO_16		0x15	/* RIO 1-16 */
#define		ISPR2HST_FPOST		0x16	/* Low 16 bits fast post */
#define		ISPR2HST_FPOST_CTIO	0x17	/* Low 16 bits fast post ctio */
#define		ISPR2HST_ATIO_UPDATE	0x1C	/* ATIO Queue Update */
#define		ISPR2HST_ATIO_RSPQ_UPDATE 0x1D	/* ATIO & Request Update */
#define		ISPR2HST_ATIO_UPDATE2	0x1E	/* ATIO Queue Update */

/*
 * 2400 Interface Offsets and Register Definitions
 * 
 * The 2400 looks quite different in terms of registers from other QLogic cards.
 * It is getting to be a genuine pain and challenge to keep the same model
 * for all.
 */
#define	BIU2400_FLASH_ADDR	(BIU_BLOCK+0x00) /* Flash Access Address */
#define	BIU2400_FLASH_DATA	(BIU_BLOCK+0x04) /* Flash Data */
#define	BIU2400_CSR		(BIU_BLOCK+0x08) /* ISP Control/Status */
#define	BIU2400_ICR		(BIU_BLOCK+0x0C) /* ISP to PCI Interrupt Control */
#define	BIU2400_ISR		(BIU_BLOCK+0x10) /* ISP to PCI Interrupt Status */

#define	BIU2400_REQINP		(BIU_BLOCK+0x1C) /* Request Queue In */
#define	BIU2400_REQOUTP		(BIU_BLOCK+0x20) /* Request Queue Out */
#define	BIU2400_RSPINP		(BIU_BLOCK+0x24) /* Response Queue In */
#define	BIU2400_RSPOUTP		(BIU_BLOCK+0x28) /* Response Queue Out */

#define	BIU2400_PRI_REQINP 	(BIU_BLOCK+0x2C) /* Priority Request Q In */
#define	BIU2400_PRI_REQOUTP 	(BIU_BLOCK+0x30) /* Priority Request Q Out */

#define	BIU2400_ATIO_RSPINP	(BIU_BLOCK+0x3C) /* ATIO Queue In */
#define	BIU2400_ATIO_RSPOUTP	(BIU_BLOCK+0x40) /* ATIO Queue Out */

#define	BIU2400_R2HSTS		(BIU_BLOCK+0x44) /* RISC to Host Status */

#define	BIU2400_HCCR		(BIU_BLOCK+0x48) /* Host Command and Control Status */
#define	BIU2400_GPIOD		(BIU_BLOCK+0x4C) /* General Purpose I/O Data */
#define	BIU2400_GPIOE		(BIU_BLOCK+0x50) /* General Purpose I/O Enable */
#define	BIU2400_IOBBA		(BIU_BLOCK+0x54) /* I/O Bus Base Address */
#define	BIU2400_HSEMA		(BIU_BLOCK+0x58) /* Host-to-Host Semaphore */

/* BIU2400_FLASH_ADDR definitions */
#define	BIU2400_FLASH_DFLAG	(1 << 30)

/* BIU2400_CSR definitions */
#define	BIU2400_NVERR		(1 << 18)
#define	BIU2400_DMA_ACTIVE	(1 << 17)		/* RO */
#define	BIU2400_DMA_STOP	(1 << 16)
#define	BIU2400_FUNCTION	(1 << 15)		/* RO */
#define	BIU2400_PCIX_MODE(x)	(((x) >> 8) & 0xf)	/* RO */
#define	BIU2400_CSR_64BIT	(1 << 2)		/* RO */
#define	BIU2400_FLASH_ENABLE	(1 << 1)
#define	BIU2400_SOFT_RESET	(1 << 0)

/* BIU2400_ICR definitions */
#define	BIU2400_ICR_ENA_RISC_INT	0x8
#define	BIU2400_IMASK			(BIU2400_ICR_ENA_RISC_INT)

/* BIU2400_ISR definitions */
#define	BIU2400_ISR_RISC_INT		0x8

/* BIU2400_HCCR definitions */
#define	HCCR_2400_CMD_NOP		0x00000000
#define	HCCR_2400_CMD_RESET		0x10000000
#define	HCCR_2400_CMD_CLEAR_RESET	0x20000000
#define	HCCR_2400_CMD_PAUSE		0x30000000
#define	HCCR_2400_CMD_RELEASE		0x40000000
#define	HCCR_2400_CMD_SET_HOST_INT	0x50000000
#define	HCCR_2400_CMD_CLEAR_HOST_INT	0x60000000
#define	HCCR_2400_CMD_CLEAR_RISC_INT	0xA0000000

#define	HCCR_2400_RISC_ERR(x)		(((x) >> 12) & 0x7)	/* RO */
#define	HCCR_2400_RISC2HOST_INT		(1 << 6)		/* RO */
#define	HCCR_2400_RISC_RESET		(1 << 5)		/* RO */


/*
 * Mailbox Block Register Offsets
 */
#define	INMAILBOX0	(MBOX_BLOCK+0x0)
#define	INMAILBOX1	(MBOX_BLOCK+0x2)
#define	INMAILBOX2	(MBOX_BLOCK+0x4)
#define	INMAILBOX3	(MBOX_BLOCK+0x6)
#define	INMAILBOX4	(MBOX_BLOCK+0x8)
#define	INMAILBOX5	(MBOX_BLOCK+0xA)
#define	INMAILBOX6	(MBOX_BLOCK+0xC)
#define	INMAILBOX7	(MBOX_BLOCK+0xE)

#define	OUTMAILBOX0	(MBOX_BLOCK+0x0)
#define	OUTMAILBOX1	(MBOX_BLOCK+0x2)
#define	OUTMAILBOX2	(MBOX_BLOCK+0x4)
#define	OUTMAILBOX3	(MBOX_BLOCK+0x6)
#define	OUTMAILBOX4	(MBOX_BLOCK+0x8)
#define	OUTMAILBOX5	(MBOX_BLOCK+0xA)
#define	OUTMAILBOX6	(MBOX_BLOCK+0xC)
#define	OUTMAILBOX7	(MBOX_BLOCK+0xE)

#define	MBOX_OFF(n)	(MBOX_BLOCK + ((n) << 1))
#define	ISP_NMBOX(isp)	32
#define	MAX_MAILBOX	32

/* if timeout == 0, then default timeout is picked */
#define	MBCMD_DEFAULT_TIMEOUT	100000	/* 100 ms */
typedef struct {
	uint16_t param[MAX_MAILBOX];
	uint32_t ibits;	/* bits to add for register copyin */
	uint32_t obits;	/* bits to add for register copyout */
	uint32_t ibitm;	/* bits to mask for register copyin */
	uint32_t obitm;	/* bits to mask for register copyout */
	uint32_t logval;	/* Bitmask of status codes to log */
	uint32_t timeout;
	uint32_t lineno;
	const char *func;
} mbreg_t;
#define	MBSINIT(mbxp, code, loglev, timo)	\
	ISP_MEMZERO((mbxp), sizeof (mbreg_t));	\
	(mbxp)->ibitm = ~0;			\
	(mbxp)->obitm = ~0;			\
	(mbxp)->param[0] = code;		\
	(mbxp)->lineno = __LINE__;		\
	(mbxp)->func = __func__;		\
	(mbxp)->logval = loglev;		\
	(mbxp)->timeout = timo

/*
 * Defines for Interrupts
 */
#define	ISP_INTS_ENABLED(isp)						\
   (ISP_READ(isp, BIU2400_ICR) & BIU2400_IMASK)

#define	ISP_ENABLE_INTS(isp)						\
    ISP_WRITE(isp, BIU2400_ICR, BIU2400_IMASK)

#define	ISP_DISABLE_INTS(isp)						\
    ISP_WRITE(isp, BIU2400_ICR, 0)

/*
 * NVRAM Definitions (PCI cards only)
 */

/*
 * Qlogic 2400 NVRAM is an array of 512 bytes with a 32 bit checksum.
 */
#define	ISP2400_NVRAM_PORT_ADDR(c)	(0x100 * (c) + 0x80)
#define	ISP2400_NVRAM_SIZE		512

#define	ISP2400_NVRAM_VERSION(c)		((c)[4] | ((c)[5] << 8))
#define	ISP2400_NVRAM_MAXFRAMELENGTH(c)		(((c)[12]) | ((c)[13] << 8))
#define	ISP2400_NVRAM_HARDLOOPID(c)		((c)[18] | ((c)[19] << 8))

#define	ISP2400_NVRAM_PORT_NAME(c)	(\
		(((uint64_t)(c)[20]) << 56) | \
		(((uint64_t)(c)[21]) << 48) | \
		(((uint64_t)(c)[22]) << 40) | \
		(((uint64_t)(c)[23]) << 32) | \
		(((uint64_t)(c)[24]) << 24) | \
		(((uint64_t)(c)[25]) << 16) | \
		(((uint64_t)(c)[26]) <<  8) | \
		(((uint64_t)(c)[27]) <<  0))

#define	ISP2400_NVRAM_NODE_NAME(c)	(\
		(((uint64_t)(c)[28]) << 56) | \
		(((uint64_t)(c)[29]) << 48) | \
		(((uint64_t)(c)[30]) << 40) | \
		(((uint64_t)(c)[31]) << 32) | \
		(((uint64_t)(c)[32]) << 24) | \
		(((uint64_t)(c)[33]) << 16) | \
		(((uint64_t)(c)[34]) <<  8) | \
		(((uint64_t)(c)[35]) <<  0))

#define	ISP2400_NVRAM_LOGIN_RETRY_CNT(c)	((c)[36] | ((c)[37] << 8))
#define	ISP2400_NVRAM_LINK_DOWN_ON_NOS(c)	((c)[38] | ((c)[39] << 8))
#define	ISP2400_NVRAM_INTERRUPT_DELAY(c)	((c)[40] | ((c)[41] << 8))
#define	ISP2400_NVRAM_LOGIN_TIMEOUT(c)		((c)[42] | ((c)[43] << 8))

#define	ISP2400_NVRAM_FIRMWARE_OPTIONS1(c)	\
	((c)[44] | ((c)[45] << 8) | ((c)[46] << 16) | ((c)[47] << 24))
#define	ISP2400_NVRAM_FIRMWARE_OPTIONS2(c)	\
	((c)[48] | ((c)[49] << 8) | ((c)[50] << 16) | ((c)[51] << 24))
#define	ISP2400_NVRAM_FIRMWARE_OPTIONS3(c)	\
	((c)[52] | ((c)[53] << 8) | ((c)[54] << 16) | ((c)[55] << 24))

/*
 * Qlogic FLT
 */
#define ISP24XX_BASE_ADDR	0x7ff00000
#define ISP24XX_FLT_ADDR	0x11400

#define ISP25XX_BASE_ADDR	ISP24XX_BASE_ADDR
#define ISP25XX_FLT_ADDR	0x50400

#define ISP27XX_BASE_ADDR	0x7f800000
#define ISP27XX_FLT_ADDR	(0x3F1000 / 4)

#define ISP28XX_BASE_ADDR	0x7f7d0000
#define ISP28XX_FLT_ADDR	(0x11000 / 4)

#define FLT_HEADER_SIZE		8
#define FLT_REGION_SIZE		16
#define FLT_MAX_REGIONS		0xFF
#define FLT_REGIONS_SIZE	(FLT_REGION_SIZE * FLT_MAX_REGIONS)

#define ISP2XXX_FLT_VERSION(c)		((c)[0] | ((c)[1] << 8))
#define ISP2XXX_FLT_LENGTH(c)		((c)[2] | ((c)[3] << 8))
#define ISP2XXX_FLT_CSUM(c)		((c)[4] | ((c)[5] << 8))
#define ISP2XXX_FLT_REG_CODE(c, o)	\
	((c)[0 + FLT_REGION_SIZE * o] | ((c)[1 + FLT_REGION_SIZE * o] << 8))
#define ISP2XXX_FLT_REG_ATTR(c, o)	((c)[2 + FLT_REGION_SIZE * o])
#define ISP2XXX_FLT_REG_RES(c, o)	((c)[3 + FLT_REGION_SIZE * o])
#define ISP2XXX_FLT_REG_SIZE(c, o)	(\
		((uint32_t)(c)[4 + FLT_REGION_SIZE * o] << 0) | \
		((uint32_t)(c)[5 + FLT_REGION_SIZE * o] << 8) | \
		((uint32_t)(c)[6 + FLT_REGION_SIZE * o] << 16) | \
		((uint32_t)(c)[7 + FLT_REGION_SIZE * o] << 24))
#define ISP2XXX_FLT_REG_START(c, o)	(\
		((uint32_t)(c)[8 + FLT_REGION_SIZE * o] << 0) | \
		((uint32_t)(c)[9 + FLT_REGION_SIZE * o] << 8) | \
		((uint32_t)(c)[10 + FLT_REGION_SIZE * o] << 16) | \
		((uint32_t)(c)[11 + FLT_REGION_SIZE * o] << 24))
#define ISP2XXX_FLT_REG_END(c, o)	(\
		((uint32_t)(c)[12 + FLT_REGION_SIZE * o] << 0) | \
		((uint32_t)(c)[13 + FLT_REGION_SIZE * o] << 8) | \
		((uint32_t)(c)[14 + FLT_REGION_SIZE * o] << 16) | \
		((uint32_t)(c)[15 + FLT_REGION_SIZE * o] << 24))

struct flt_region {
	uint16_t  code;
	uint8_t attribute;
	uint8_t reserved;
	uint32_t size;
	uint32_t start;
	uint32_t end;
};

#define FLT_REG_FW		0x01
#define FLT_REG_BOOT_CODE	0x07
#define FLT_REG_VPD_0		0x14
#define FLT_REG_NVRAM_0		0x15
#define FLT_REG_VPD_1		0x16
#define FLT_REG_NVRAM_1		0x17
#define FLT_REG_VPD_2		0xd4
#define FLT_REG_NVRAM_2		0xd5
#define FLT_REG_VPD_3		0xd6
#define FLT_REG_NVRAM_3		0xd7
#define FLT_REG_FDT		0x1a
#define FLT_REG_FLT		0x1c
#define FLT_REG_NPIV_CONF_0	0x29
#define FLT_REG_NPIV_CONF_1	0x2a
#define FLT_REG_GOLD_FW		0x2f
#define FLT_REG_FCP_PRIO_0	0x87
#define FLT_REG_FCP_PRIO_1	0x88

/* 27xx */
#define FLT_REG_IMG_PRI_27XX	0x95
#define FLT_REG_IMG_SEC_27XX	0x96
#define FLT_REG_FW_SEC_27XX	0x02
#define FLT_REG_BOOTLOAD_SEC_27XX	0x9
#define FLT_REG_VPD_SEC_27XX_0	0x50
#define FLT_REG_VPD_SEC_27XX_1	0x52
#define FLT_REG_VPD_SEC_27XX_2	0xd8
#define FLT_REG_VPD_SEC_27XX_3	0xda

/* 28xx */
#define FLT_REG_AUX_IMG_PRI_28XX	0x125
#define FLT_REG_AUX_IMG_SEC_28XX	0x126
#define FLT_REG_NVRAM_SEC_28XX_0	0x10d
#define FLT_REG_NVRAM_SEC_28XX_1	0x10f
#define FLT_REG_NVRAM_SEC_28XX_2	0x111
#define FLT_REG_NVRAM_SEC_28XX_3	0x113
#define FLT_REG_VPD_SEC_28XX_0		0x10c
#define FLT_REG_VPD_SEC_28XX_1		0x10e
#define FLT_REG_VPD_SEC_28XX_2		0x110
#define FLT_REG_VPD_SEC_28XX_3		0x112

#endif	/* _ISPREG_H */
