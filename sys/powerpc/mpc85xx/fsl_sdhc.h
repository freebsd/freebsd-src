/*-
 * Copyright (c) 2011-2012 Semihalf
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

#ifndef FSL_SDHC_H_
#define FSL_SDHC_H_

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/taskqueue.h>

#include <machine/bus.h>

#include <dev/mmc/bridge.h>
#include <dev/mmc/mmcreg.h>
#include <dev/mmc/mmcvar.h>
#include <dev/mmc/mmcbrvar.h>

#include "mmcbr_if.h"


/*****************************************************************************
 * Private defines
 *****************************************************************************/
struct slot {
	uint32_t	clock;
};

struct fsl_sdhc_softc {
	device_t		self;
	device_t		child;

	bus_space_handle_t	bsh;
	bus_space_tag_t		bst;

	struct resource		*mem_resource;
	int			mem_rid;
	struct resource		*irq_resource;
	int			irq_rid;
	void			*ihl;

	bus_dma_tag_t		dma_tag;
	bus_dmamap_t		dma_map;
	uint32_t*		dma_mem;
	bus_addr_t		dma_phys;

	struct mtx		mtx;

	struct task		card_detect_task;
	struct callout		card_detect_callout;

	struct mmc_host		mmc_host;

	struct slot		slot;
	uint32_t		bus_busy;
	uint32_t		platform_clock;

	struct mmc_request	*request;
	int			data_done;
	int			command_done;
	int			use_dma;
	uint32_t*		data_ptr;
	uint32_t		data_offset;
};

#define FSL_SDHC_RESET_DELAY 50

#define	FSL_SDHC_BASE_CLOCK_DIV		(2)
#define	FSL_SDHC_MAX_DIV		(FSL_SDHC_BASE_CLOCK_DIV * 256 * 16)
#define	FSL_SDHC_MIN_DIV		(FSL_SDHC_BASE_CLOCK_DIV * 2)
#define	FSL_SDHC_MAX_CLOCK		(50000000)

#define	FSL_SDHC_MAX_BLOCK_COUNT	(65535)
#define	FSL_SDHC_MAX_BLOCK_SIZE		(4096)

#define FSL_SDHC_FIFO_BUF_SIZE		(64)	/* Water-mark level. */
#define FSL_SDHC_FIFO_BUF_WORDS		(FSL_SDHC_FIFO_BUF_SIZE / 4)

#define FSL_SDHC_DMA_SEGMENT_SIZE	(1024)
#define	FSL_SDHC_DMA_ALIGNMENT		(4)
#define	FSL_SDHC_DMA_BLOCK_SIZE		FSL_SDHC_MAX_BLOCK_SIZE


/*
 * Offsets of SD HC registers
 */
enum sdhc_reg_off {
	SDHC_DSADDR	= 0x000,
	SDHC_BLKATTR	= 0x004,
	SDHC_CMDARG	= 0x008,
	SDHC_XFERTYP	= 0x00c,
	SDHC_CMDRSP0	= 0x010,
	SDHC_CMDRSP1	= 0x014,
	SDHC_CMDRSP2	= 0x018,
	SDHC_CMDRSP3	= 0x01c,
	SDHC_DATPORT	= 0x020,
	SDHC_PRSSTAT	= 0x024,
	SDHC_PROCTL	= 0x028,
	SDHC_SYSCTL	= 0x02c,
	SDHC_IRQSTAT	= 0x030,
	SDHC_IRQSTATEN	= 0x034,
	SDHC_IRQSIGEN	= 0x038,
	SDHC_AUTOC12ERR	= 0x03c,
	SDHC_HOSTCAPBLT	= 0x040,
	SDHC_WML	= 0x044,
	SDHC_FEVT	= 0x050,
	SDHC_HOSTVER	= 0x0fc,
	SDHC_DCR	= 0x40c
};

enum sysctl_bit {
	SYSCTL_INITA	= 0x08000000,
	SYSCTL_RSTD	= 0x04000000,
	SYSCTL_RSTC	= 0x02000000,
	SYSCTL_RSTA	= 0x01000000,
	SYSCTL_DTOCV	= 0x000f0000,
	SYSCTL_SDCLKFS	= 0x0000ff00,
	SYSCTL_DVS	= 0x000000f0,
	SYSCTL_PEREN	= 0x00000004,
	SYSCTL_HCKEN	= 0x00000002,
	SYSCTL_IPGEN	= 0x00000001
};

#define HEX_LEFT_SHIFT(x)	(4 * x)
enum sysctl_shift {
	SHIFT_DTOCV	= HEX_LEFT_SHIFT(4),
	SHIFT_SDCLKFS	= HEX_LEFT_SHIFT(2),
	SHIFT_DVS	= HEX_LEFT_SHIFT(1)
};

enum proctl_bit {
	PROCTL_WECRM	= 0x04000000,
	PROCTL_WECINS	= 0x02000000,
	PROCTL_WECINT	= 0x01000000,
	PROCTL_RWCTL	= 0x00040000,
	PROCTL_CREQ	= 0x00020000,
	PROCTL_SABGREQ	= 0x00010000,
	PROCTL_CDSS	= 0x00000080,
	PROCTL_CDTL	= 0x00000040,
	PROCTL_EMODE	= 0x00000030,
	PROCTL_D3CD	= 0x00000008,
	PROCTL_DTW	= 0x00000006
};

enum dtw {
	DTW_1	= 0x00000000,
	DTW_4	= 0x00000002,
	DTW_8	= 0x00000004
};

enum prsstat_bit {
	PRSSTAT_DLSL	= 0xff000000,
	PRSSTAT_CLSL	= 0x00800000,
	PRSSTAT_WPSPL	= 0x00080000,
	PRSSTAT_CDPL	= 0x00040000,
	PRSSTAT_CINS	= 0x00010000,
	PRSSTAT_BREN	= 0x00000800,
	PRSSTAT_BWEN	= 0x00000400,
	PRSSTAT_RTA	= 0x00000200,
	PRSSTAT_WTA	= 0x00000100,
	PRSSTAT_SDOFF	= 0x00000080,
	PRSSTAT_PEROFF	= 0x00000040,
	PRSSTAT_HCKOFF	= 0x00000020,
	PRSSTAT_IPGOFF	= 0x00000010,
	PRSSTAT_DLA	= 0x00000004,
	PRSSTAT_CDIHB	= 0x00000002,
	PRSSTAT_CIHB	= 0x00000001

};

enum irq_bits {
	IRQ_DMAE	= 0x10000000,
	IRQ_AC12E	= 0x01000000,
	IRQ_DEBE	= 0x00400000,
	IRQ_DCE		= 0x00200000,
	IRQ_DTOE	= 0x00100000,
	IRQ_CIE		= 0x00080000,
	IRQ_CEBE	= 0x00040000,
	IRQ_CCE		= 0x00020000,
	IRQ_CTOE	= 0x00010000,
	IRQ_CINT	= 0x00000100,
	IRQ_CRM		= 0x00000080,
	IRQ_CINS	= 0x00000040,
	IRQ_BRR		= 0x00000020,
	IRQ_BWR		= 0x00000010,
	IRQ_DINT	= 0x00000008,
	IRQ_BGE		= 0x00000004,
	IRQ_TC		= 0x00000002,
	IRQ_CC		= 0x00000001
};

enum irq_masks {
	IRQ_ERROR_DATA_MASK	= IRQ_DMAE | IRQ_DEBE | IRQ_DCE | IRQ_DTOE,
	IRQ_ERROR_CMD_MASK	= IRQ_AC12E | IRQ_CIE | IRQ_CTOE | IRQ_CCE |
				  IRQ_CEBE
};

enum dcr_bits {
	DCR_PRI			= 0x0000c000,
	DCR_SNOOP		= 0x00000040,
	DCR_AHB2MAG_BYPASS	= 0x00000020,
	DCR_RD_SAFE		= 0x00000004,
	DCR_RD_PFE		= 0x00000002,
	DCR_RD_PF_SIZE		= 0x00000001
};

#define	DCR_PRI_SHIFT	(14)

enum xfertyp_bits {
	XFERTYP_CMDINX	= 0x3f000000,
	XFERTYP_CMDTYP	= 0x00c00000,
	XFERTYP_DPSEL	= 0x00200000,
	XFERTYP_CICEN	= 0x00100000,
	XFERTYP_CCCEN	= 0x00080000,
	XFERTYP_RSPTYP	= 0x00030000,
	XFERTYP_MSBSEL	= 0x00000020,
	XFERTYP_DTDSEL	= 0x00000010,
	XFERTYP_AC12EN	= 0x00000004,
	XFERTYP_BCEN	= 0x00000002,
	XFERTYP_DMAEN	= 0x00000001
};

#define	CMDINX_SHIFT	(24)

enum xfertyp_cmdtyp {
	CMDTYP_NORMAL	= 0x00000000,
	CMDYTP_SUSPEND	= 0x00400000,
	CMDTYP_RESUME	= 0x00800000,
	CMDTYP_ABORT	= 0x00c00000
};

enum xfertyp_rsptyp {
	RSPTYP_NONE	= 0x00000000,
	RSPTYP_136	= 0x00010000,
	RSPTYP_48	= 0x00020000,
	RSPTYP_48_BUSY	= 0x00030000
};

enum blkattr_bits {
	BLKATTR_BLKSZE	= 0x00001fff,
	BLKATTR_BLKCNT	= 0xffff0000
};
#define	BLKATTR_BLOCK_COUNT(x)	(x << 16)

enum wml_bits {
	WR_WML	= 0x00ff0000,
	RD_WML	= 0x000000ff,
};

enum sdhc_bit_mask {
	MASK_CLOCK_CONTROL	= 0x0000ffff,
	MASK_IRQ_ALL		= IRQ_DMAE | IRQ_AC12E | IRQ_DEBE | IRQ_DCE |
				  IRQ_DTOE | IRQ_CIE | IRQ_CEBE | IRQ_CCE |
				  IRQ_CTOE | IRQ_CINT | IRQ_CRM | IRQ_CINS |
				  IRQ_BRR | IRQ_BWR | IRQ_DINT | IRQ_BGE |
				  IRQ_TC | IRQ_CC,
};

enum sdhc_line {
	SDHC_DAT_LINE	= 0x2,
	SDHC_CMD_LINE	= 0x1
};

#endif /* FSL_SDHC_H_ */
