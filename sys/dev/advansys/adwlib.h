/*
 * Definitions for low level routines and data structures
 * for the Advanced Systems Inc. SCSI controllers chips.
 *
 * Copyright (c) 1998, 1999, 2000 Justin T. Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */
/*
 * Ported from:
 * advansys.c - Linux Host Driver for AdvanSys SCSI Adapters
 *     
 * Copyright (c) 1995-1998 Advanced System Products, Inc.
 * All Rights Reserved.
 *   
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that redistributions of source
 * code retain the above copyright notice and this comment without
 * modification.
 */

#ifndef _ADWLIB_H_
#define _ADWLIB_H_

#include "opt_adw.h"

#include <stddef.h>	/* for offsetof */

#include <dev/advansys/adwmcode.h>

#define ADW_DEF_MAX_HOST_QNG	253
#define ADW_DEF_MIN_HOST_QNG	16
#define ADW_DEF_MAX_DVC_QNG	63
#define ADW_DEF_MIN_DVC_QNG	4

#define ADW_MAX_TID		15
#define ADW_MAX_LUN		7

#define	ADW_ALL_TARGETS		0xFFFF

#define ADW_TARGET_GROUP(tid)		((tid) & ~0x3)
#define ADW_TARGET_GROUP_SHIFT(tid)	(((tid) & 0x3) * 4)
#define ADW_TARGET_GROUP_MASK(tid)	(0xF << ADW_TARGET_GROUP_SHIFT(tid))

/*
 * Board Register offsets.
 */
#define ADW_INTR_STATUS_REG			0x0000
#define		ADW_INTR_STATUS_INTRA		0x01
#define		ADW_INTR_STATUS_INTRB		0x02
#define		ADW_INTR_STATUS_INTRC		0x04
#define		ADW_INTR_STATUS_INTRALL		0x07


#define ADW_SIGNATURE_WORD			0x0000
#define		 ADW_CHIP_ID_WORD		0x04C1

#define	ADW_SIGNATURE_BYTE			0x0001
#define		 ADW_CHIP_ID_BYTE		0x25	

#define	ADW_INTR_ENABLES			0x0002	/*8 bit */
#define		ADW_INTR_ENABLE_HOST_INTR	0x01
#define		ADW_INTR_ENABLE_SEL_INTR	0x02
#define		ADW_INTR_ENABLE_DPR_INTR	0x04
#define		ADW_INTR_ENABLE_RTA_INTR	0x08
#define		ADW_INTR_ENABLE_RMA_INTR	0x10
#define		ADW_INTR_ENABLE_RST_INTR	0x20
#define		ADW_INTR_ENABLE_DPE_INTR	0x40
#define		ADW_INTR_ENABLE_GLOBAL_INTR	0x80

#define ADW_CTRL_REG				0x0002  /*16 bit*/
#define		ADW_CTRL_REG_HOST_INTR		0x0100
#define		ADW_CTRL_REG_SEL_INTR		0x0200
#define		ADW_CTRL_REG_DPR_INTR		0x0400
#define		ADW_CTRL_REG_RTA_INTR		0x0800
#define		ADW_CTRL_REG_RMA_INTR		0x1000
#define		ADW_CTRL_REG_RES_BIT14		0x2000
#define		ADW_CTRL_REG_DPE_INTR		0x4000
#define		ADW_CTRL_REG_POWER_DONE		0x8000
#define		ADW_CTRL_REG_ANY_INTR		0xFF00
#define		ADW_CTRL_REG_CMD_RESET		0x00C6
#define		ADW_CTRL_REG_CMD_WR_IO_REG	0x00C5
#define		ADW_CTRL_REG_CMD_RD_IO_REG	0x00C4
#define		ADW_CTRL_REG_CMD_WR_PCI_CFG	0x00C3
#define		ADW_CTRL_REG_CMD_RD_PCI_CFG	0x00C2

#define ADW_RAM_ADDR				0x0004
#define ADW_RAM_DATA				0x0006

#define ADW_RISC_CSR				0x000A
#define		ADW_RISC_CSR_STOP		0x0000
#define		ADW_RISC_TEST_COND		0x2000
#define		ADW_RISC_CSR_RUN		0x4000
#define		ADW_RISC_CSR_SINGLE_STEP	0x8000

#define ADW_SCSI_CFG0				0x000C
#define		ADW_SCSI_CFG0_TIMER_MODEAB	0xC000  /*
							 * Watchdog, Second,
							 * and Selto timer CFG
							 */
#define		ADW_SCSI_CFG0_PARITY_EN		0x2000
#define		ADW_SCSI_CFG0_EVEN_PARITY	0x1000
#define		ADW_SCSI_CFG0_WD_LONG		0x0800  /*
							 * Watchdog Interval,
							 * 1: 57 min, 0: 13 sec
							 */
#define		ADW_SCSI_CFG0_QUEUE_128		0x0400  /*
							 * Queue Size,
							 * 1: 128 byte,
							 * 0: 64 byte
							 */
#define		ADW_SCSI_CFG0_PRIM_MODE		0x0100
#define		ADW_SCSI_CFG0_SCAM_EN		0x0080
#define		ADW_SCSI_CFG0_SEL_TMO_LONG	0x0040  /*
							 * Sel/Resel Timeout,
							 * 1: 400 ms,
							 * 0: 1.6 ms
							 */
#define		ADW_SCSI_CFG0_CFRM_ID		0x0020  /* SCAM id sel. */
#define		ADW_SCSI_CFG0_OUR_ID_EN		0x0010
#define		ADW_SCSI_CFG0_OUR_ID		0x000F


#define ADW_SCSI_CFG1				0x000E
#define		ADW_SCSI_CFG1_BIG_ENDIAN	0x8000
#define		ADW_SCSI_CFG1_TERM_POL		0x2000
#define		ADW_SCSI_CFG1_SLEW_RATE		0x1000
#define		ADW_SCSI_CFG1_FILTER_MASK	0x0C00
#define		ADW_SCSI_CFG1_FLTR_DISABLE	0x0000
#define		ADW_SCSI_CFG1_FLTR_11_TO_20NS	0x0800
#define		ADW_SCSI_CFG1_FLTR_21_TO_39NS	0x0C00
#define		ADW_SCSI_CFG1_DIS_ACTIVE_NEG	0x0200
#define		ADW_SCSI_CFG1_DIFF_MODE		0x0100
#define		ADW_SCSI_CFG1_DIFF_SENSE	0x0080
#define		ADW_SCSI_CFG1_TERM_CTL_MANUAL	0x0040  /* Global Term Switch */
#define		ADW_SCSI_CFG1_TERM_CTL_MASK	0x0030
#define		ADW_SCSI_CFG1_TERM_CTL_H	0x0020  /* Enable SCSI-H */
#define		ADW_SCSI_CFG1_TERM_CTL_L	0x0010  /* Enable SCSI-L */
#define		ADW_SCSI_CFG1_CABLE_DETECT	0x000F
#define		ADW_SCSI_CFG1_EXT16_MASK	0x0008	/* Ext16 cable pres */
#define		ADW_SCSI_CFG1_EXT8_MASK		0x0004	/* Ext8 cable pres */
#define		ADW_SCSI_CFG1_INT8_MASK		0x0002	/* Int8 cable pres */
#define		ADW_SCSI_CFG1_INT16_MASK	0x0001	/* Int16 cable pres */
#define		ADW_SCSI_CFG1_ILLEGAL_CABLE_CONF_A_MASK	\
(ADW_SCSI_CFG1_EXT16_MASK|ADW_SCSI_CFG1_INT8_MASK|ADW_SCSI_CFG1_INT16_MASK)
#define		ADW_SCSI_CFG1_ILLEGAL_CABLE_CONF_B_MASK	\
(ADW_SCSI_CFG1_EXT8_MASK|ADW_SCSI_CFG1_INT8_MASK|ADW_SCSI_CFG1_INT16_MASK)

/*
 * Addendum for ASC-38C0800 Chip
 */
#define		ADW2_SCSI_CFG1_DIS_TERM_DRV	0x4000	/*
							 * The Terminators
							 * must be disabled
							 * in order to detect
							 * cable presence
							 */

#define		ADW2_SCSI_CFG1_DEV_DETECT	0x1C00
#define		ADW2_SCSI_CFG1_DEV_DETECT_HVD	0x1000
#define		ADW2_SCSI_CFG1_DEV_DETECT_LVD	0x0800
#define		ADW2_SCSI_CFG1_DEV_DETECT_SE	0x0400

#define		ADW2_SCSI_CFG1_TERM_CTL_LVD	0x00C0	/* Ultra2 Only */
#define		ADW2_SCSI_CFG1_TERM_LVD_HI	0x0080 
#define		ADW2_SCSI_CFG1_TERM_LVD_LO	0x0040
#define		ADW2_SCSI_CFG1_EXTLVD_MASK	0x0008	/* ExtLVD cable pres */
#define		ADW2_SCSI_CFG1_INTLVD_MASK	0x0004	/* IntLVD cable pres */

#define ADW_MEM_CFG				0x0010
#define 	ADW_MEM_CFG_BIOS_EN		0x40
#define		ADW_MEM_CFG_FAST_EE_CLK		0x20	/* Diagnostic Bit */
#define		ADW_MEM_CFG_RAM_SZ_MASK		0x1C	/* RISC RAM Size */
#define		ADW_MEM_CFG_RAM_SZ_2KB		0x00
#define		ADW_MEM_CFG_RAM_SZ_4KB		0x04
#define		ADW_MEM_CFG_RAM_SZ_8KB		0x08
#define		ADW_MEM_CFG_RAM_SZ_16KB		0x0C
#define		ADW_MEM_CFG_RAM_SZ_32KB		0x10
#define		ADW_MEM_CFG_RAM_SZ_64KB		0x14

#define	ADW_GPIO_CNTL				0x0011
#define	ADW_GPIO_DATA				0x0012

#define	ADW_COMMA				0x0014
#define ADW_COMMB				0x0018  

#define ADW_EEP_CMD				0x001A
#define		ADW_EEP_CMD_READ		0x0080	/* or in address */
#define		ADW_EEP_CMD_WRITE		0x0040	/* or in address */
#define		ADW_EEP_CMD_WRITE_ABLE		0x0030
#define		ADW_EEP_CMD_WRITE_DISABLE	0x0000
#define		ADW_EEP_CMD_DONE		0x0200
#define		ADW_EEP_CMD_DONE_ERR		0x0001
#define		ADW_EEP_DELAY_MS                100

#define ADW_EEP_DATA				0x001C

#define ADW_DMA_CFG0				0x0020
#define		ADW_DMA_CFG0_BC_THRESH_ENB	0x80
#define		ADW_DMA_CFG0_FIFO_THRESH	0x70
#define		ADW_DMA_CFG0_FIFO_THRESH_16B	0x00
#define		ADW_DMA_CFG0_FIFO_THRESH_32B	0x20
#define		ADW_DMA_CFG0_FIFO_THRESH_48B	0x30
#define		ADW_DMA_CFG0_FIFO_THRESH_64B	0x40
#define		ADW_DMA_CFG0_FIFO_THRESH_80B	0x50
#define		ADW_DMA_CFG0_FIFO_THRESH_96B	0x60
#define		ADW_DMA_CFG0_FIFO_THRESH_112B	0x70
#define		ADW_DMA_CFG0_START_CTL_MASK	0x0C
#define		ADW_DMA_CFG0_START_CTL_TH	0x00 /* Start on thresh */
#define		ADW_DMA_CFG0_START_CTL_IDLE	0x04 /* Start when idle */
#define		ADW_DMA_CFG0_START_CTL_TH_IDLE	0x08 /* Either */
#define		ADW_DMA_CFG0_START_CTL_EM_FU	0x0C /* Start on full/empty */
#define		ADW_DMA_CFG0_READ_CMD_MASK	0x03
#define		ADW_DMA_CFG0_READ_CMD_MR	0x00
#define		ADW_DMA_CFG0_READ_CMD_MRL	0x02
#define		ADW_DMA_CFG0_READ_CMD_MRM	0x03

#define ADW_TICKLE				0x0022
#define		ADW_TICKLE_NOP			0x00
#define		ADW_TICKLE_A			0x01
#define		ADW_TICKLE_B			0x02
#define		ADW_TICKLE_C			0x03

/* Program Counter */
#define ADW_PC					0x2A

#define ADW_SCSI_CTRL				0x0034
#define		ADW_SCSI_CTRL_RSTOUT		0x2000

/*
 * ASC-38C0800 RAM BIST Register bit definitions
 */
#define ADW_RAM_BIST				0x0038
#define		ADW_RAM_BIST_RAM_TEST_MODE	0x80
#define		ADW_RAM_BIST_PRE_TEST_MODE	0x40
#define		ADW_RAM_BIST_NORMAL_MODE	0x00
#define		ADW_RAM_BIST_RAM_TEST_DONE	0x10
#define		ADW_RAM_BIST_RAM_TEST_STATUS	0x0F
#define		ADW_RAM_BIST_RAM_TEST_HOST_ERR	0x08
#define		ADW_RAM_BIST_RAM_TEST_RAM_ERR	0x04
#define		ADW_RAM_BIST_RAM_TEST_RISC_ERR	0x02
#define		ADW_RAM_BIST_RAM_TEST_SCSI_ERR	0x01
#define		ADW_RAM_BIST_RAM_TEST_SUCCESS	0x00
#define		ADW_RAM_BIST_PRE_TEST_VALUE	0x05
#define		ADW_RAM_BIST_NORMAL_VALUE	0x00 
#define ADW_PLL_TEST				0x0039

#define	ADW_SCSI_RESET_HOLD_TIME_US		60

/* LRAM Constants */
#define ADW_3550_MEMSIZE	0x2000	/* 8 KB Internal Memory */
#define ADW_3550_IOLEN		0x40	/* I/O Port Range in bytes */

#define ADW_38C0800_MEMSIZE	0x4000	/* 16 KB Internal Memory */
#define ADW_38C0800_IOLEN	0x100	/* I/O Port Range in bytes */

#define ADW_38C1600_MEMSIZE	0x4000	/* 16 KB Internal Memory */
#define ADW_38C1600_IOLEN	0x100	/* I/O Port Range in bytes */
#define ADW_38C1600_MEMLEN	0x1000	/* Memory Range 4KB */

#define ADW_MC_BIOSMEM		0x0040	/* BIOS RISC Memory Start */
#define ADW_MC_BIOSLEN		0x0050	/* BIOS RISC Memory Length */

#define	PCI_ID_ADVANSYS_3550		0x230010CD00000000ull
#define	PCI_ID_ADVANSYS_38C0800_REV1	0x250010CD00000000ull
#define	PCI_ID_ADVANSYS_38C1600_REV1	0x270010CD00000000ull
#define PCI_ID_ALL_MASK             	0xFFFFFFFFFFFFFFFFull
#define PCI_ID_DEV_VENDOR_MASK      	0xFFFFFFFF00000000ull

/* ====================== SCSI Request Structures =========================== */

#define ADW_NO_OF_SG_PER_BLOCK	15

/*
 * Although the adapter can deal with S/G lists of indefinite size,
 * we limit the list to 30 to conserve space as the kernel can only send
 * us buffers of at most 64KB currently.
 */
#define ADW_SG_BLOCKCNT		2
#define ADW_SGSIZE		(ADW_NO_OF_SG_PER_BLOCK * ADW_SG_BLOCKCNT)

struct adw_sg_elm {
	u_int32_t sg_addr;
	u_int32_t sg_count;
};

/* sg block structure used by the microcode */
struct adw_sg_block {   
	u_int8_t  reserved1;
	u_int8_t  reserved2;
	u_int8_t  reserved3;
	u_int8_t  sg_cnt;	/* Valid entries in this block */
	u_int32_t sg_busaddr_next; /* link to the next sg block */
	struct	  adw_sg_elm sg_list[ADW_NO_OF_SG_PER_BLOCK];
};

/* Structure representing a single allocation block of adw sg blocks */
struct sg_map_node {
	bus_dmamap_t		 sg_dmamap;
	bus_addr_t		 sg_physaddr;
	struct adw_sg_block*	 sg_vaddr;
	SLIST_ENTRY(sg_map_node) links;
};

typedef enum {
	QHSTA_NO_ERROR		    = 0x00,
	QHSTA_M_SEL_TIMEOUT	    = 0x11,
	QHSTA_M_DATA_OVER_RUN	    = 0x12,
	QHSTA_M_UNEXPECTED_BUS_FREE = 0x13,
	QHSTA_M_QUEUE_ABORTED	    = 0x15,
	QHSTA_M_SXFR_SDMA_ERR	    = 0x16, /* SCSI DMA Error */
	QHSTA_M_SXFR_SXFR_PERR	    = 0x17, /* SCSI Bus Parity Error */
	QHSTA_M_RDMA_PERR	    = 0x18, /* RISC PCI DMA parity error */
	QHSTA_M_SXFR_OFF_UFLW	    = 0x19, /* Offset Underflow */
	QHSTA_M_SXFR_OFF_OFLW	    = 0x20, /* Offset Overflow */
	QHSTA_M_SXFR_WD_TMO	    = 0x21, /* Watchdog Timeout */
	QHSTA_M_SXFR_DESELECTED	    = 0x22, /* Deselected */
	QHSTA_M_SXFR_XFR_PH_ERR	    = 0x24, /* Transfer Phase Error */
	QHSTA_M_SXFR_UNKNOWN_ERROR  = 0x25, /* SXFR_STATUS Unknown Error */
	QHSTA_M_SCSI_BUS_RESET	    = 0x30, /* Request aborted from SBR */
	QHSTA_M_SCSI_BUS_RESET_UNSOL= 0x31, /* Request aborted from unsol. SBR*/
	QHSTA_M_BUS_DEVICE_RESET    = 0x32, /* Request aborted from BDR */
	QHSTA_M_DIRECTION_ERR	    = 0x35, /* Data Phase mismatch */
	QHSTA_M_DIRECTION_ERR_HUNG  = 0x36, /* Data Phase mismatch - bus hang */
	QHSTA_M_WTM_TIMEOUT	    = 0x41,
	QHSTA_M_BAD_CMPL_STATUS_IN  = 0x42,
	QHSTA_M_NO_AUTO_REQ_SENSE   = 0x43,
	QHSTA_M_AUTO_REQ_SENSE_FAIL = 0x44,
	QHSTA_M_INVALID_DEVICE	    = 0x45, /* Bad target ID */
	QHSTA_M_FROZEN_TIDQ	    = 0x46, /* TID Queue frozen. */
	QHSTA_M_SGBACKUP_ERROR	    = 0x47  /* Scatter-Gather backup error */
} host_status_t;

typedef enum {
	QD_NO_STATUS	   = 0x00, /* Request not completed yet. */
	QD_NO_ERROR	   = 0x01,
	QD_ABORTED_BY_HOST = 0x02,
	QD_WITH_ERROR	   = 0x04
} done_status_t;

/*
 * Microcode request structure
 *
 * All fields in this structure are used by the microcode so their
 * size and ordering cannot be changed.
 */
struct adw_scsi_req_q {
	u_int8_t  cntl;		  /* Ucode flags and state. */
	u_int8_t  target_cmd;
	u_int8_t  target_id;	  /* Device target identifier. */
	u_int8_t  target_lun;	  /* Device target logical unit number. */
	u_int32_t data_addr;	  /* Data buffer physical address. */
	u_int32_t data_cnt;	  /* Data count. Ucode sets to residual. */
	u_int32_t sense_baddr;	  /* Sense buffer bus address. */
	u_int32_t carrier_baddr;  /* Carrier bus address. */
	u_int8_t  mflag;	  /* microcode flag field. */
	u_int8_t  sense_len;	  /* Auto-sense length. Residual on complete. */
	u_int8_t  cdb_len;	  /* SCSI CDB length. */
	u_int8_t  scsi_cntl;	  /* SCSI command control flags (tags, nego) */
#define		ADW_QSC_NO_DISC		0x01
#define		ADW_QSC_NO_TAGMSG	0x02
#define		ADW_QSC_NO_SYNC		0x04
#define		ADW_QSC_NO_WIDE		0x08
#define		ADW_QSC_REDO_DTR	0x10 /* Renegotiate WDTR/SDTR */
#define		ADW_QSC_SIMPLE_Q_TAG	0x00
#define		ADW_QSC_HEAD_OF_Q_TAG	0x40
#define		ADW_QSC_ORDERED_Q_TAG	0x80
	u_int8_t  done_status;	  /* Completion status. */
	u_int8_t  scsi_status;	  /* SCSI status byte. */
	u_int8_t  host_status;	  /* Ucode host status. */
	u_int8_t  sg_wk_ix;	  /* Microcode working SG index. */
	u_int8_t  cdb[12];        /* SCSI command block. */
	u_int32_t sg_real_addr;   /* SG list physical address. */ 
	u_int32_t scsi_req_baddr; /* Bus address of this structure. */
	u_int32_t sg_wk_data_cnt; /* Saved data count at disconnection. */
	/*
	 * The 'tokens' placed in these two fields are
	 * used to identify the scsi request and the next
	 * carrier in the response queue, *not* physical
	 * addresses.  This driver uses byte offsets for
	 * portability and speed of mapping back to either
	 * a virtual or physical address.
	 */
	u_int32_t scsi_req_bo;	  /* byte offset of this structure */
	u_int32_t carrier_bo;	  /* byte offst of our carrier. */
};

typedef enum {
	ACB_FREE		= 0x00,
	ACB_ACTIVE		= 0x01,
	ACB_RELEASE_SIMQ	= 0x02,
	ACB_RECOVERY_ACB	= 0x04
} acb_state;

struct acb {
	struct		adw_scsi_req_q queue;
	bus_dmamap_t	dmamap;
	acb_state	state;
	union		ccb *ccb;
	struct		adw_sg_block* sg_blocks;
	bus_addr_t	sg_busaddr;
	struct		scsi_sense_data sense_data;
	SLIST_ENTRY(acb) links;
};

/*
 * EEPROM configuration format
 *
 * Field naming convention: 
 *
 *  *_enable indicates the field enables or disables the feature. The
 *  value is never reset.
 *
 *  *_able indicates both whether a feature should be enabled or disabled
 *  and whether a device is capable of the feature. At initialization
 *  this field may be set, but later if a device is found to be incapable
 *  of the feature, the field is cleared.
 *
 * Default values are maintained in a_init.c in the structure
 * Default_EEPROM_Config.
 */
struct adw_eeprom
{                              
	u_int16_t cfg_lsw;	/* 00 power up initialization */
#define		ADW_EEPROM_BIG_ENDIAN	0x8000
#define		ADW_EEPROM_BIOS_ENABLE	0x4000
#define		ADW_EEPROM_TERM_POL	0x2000
#define		ADW_EEPROM_CIS_LD	0x1000

				/* bit 13 set - Term Polarity Control */
				/* bit 14 set - BIOS Enable */
				/* bit 15 set - Big Endian Mode */
	u_int16_t cfg_msw;	/* unused */
	u_int16_t disc_enable;
	u_int16_t wdtr_able;
	union {
		/*
		 * sync enable bits for UW cards,
		 * actual sync rate for TID 0-3
		 * on U2W and U160 cards.
		 */
		u_int16_t sync_enable;
		u_int16_t sdtr1;
	} sync1;
	u_int16_t start_motor;
	u_int16_t tagqng_able;
	u_int16_t bios_scan;
	u_int16_t scam_tolerant;
 
	u_int8_t  adapter_scsi_id;
	u_int8_t  bios_boot_delay;
 
	u_int8_t  scsi_reset_delay;
	u_int8_t  bios_id_lun;	/*    high nibble is lun */  
				/*    low nibble is scsi id */

	u_int8_t  termination_se;	/* 0 - automatic */
#define		ADW_EEPROM_TERM_AUTO 		0
#define		ADW_EEPROM_TERM_OFF		1
#define		ADW_EEPROM_TERM_HIGH_ON		2
#define		ADW_EEPROM_TERM_BOTH_ON		3

	u_int8_t  termination_lvd;
	u_int16_t bios_ctrl;
#define		ADW_BIOS_INIT_DIS     0x0001 /* Don't act as initiator */
#define		ADW_BIOS_EXT_TRANS    0x0002 /* > 1 GB support */
#define		ADW_BIOS_MORE_2DISK   0x0004 /* > 1 GB support */
#define		ADW_BIOS_NO_REMOVABLE 0x0008 /* don't support removable media */
#define		ADW_BIOS_CD_BOOT      0x0010 /* support bootable CD */
#define		ADW_BIOS_SCAN_EN      0x0020 /* BIOS SCAN enabled */
#define		ADW_BIOS_MULTI_LUN    0x0040 /* probe luns */
#define		ADW_BIOS_MESSAGE      0x0080 /* display BIOS message */
#define		ADW_BIOS_RESET_BUS    0x0200 /* reset SCSI bus durint init */
#define		ADW_BIOS_QUIET        0x0800 /* No verbose initialization */
#define		ADW_BIOS_SCSI_PAR_EN  0x1000 /* SCSI parity enabled */

	union {
		/* 13
		 * ultra enable bits for UW cards,
		 * actual sync rate for TID 4-7
		 * on U2W and U160 cards.
		 */
		u_int16_t ultra_enable;
		u_int16_t sdtr2;
	} sync2;
	union {
		/* 14
		 * reserved for UW cards,
		 * actual sync rate for TID 8-11
		 * on U2W and U160 cards.
		 */
		u_int16_t reserved;
		u_int16_t sdtr3;
	} sync3;
	u_int8_t  max_host_qng;	/* 15 maximum host queuing */
	u_int8_t  max_dvc_qng;	/*    maximum per device queuing */
	u_int16_t dvc_cntl;	/* 16 control bit for driver */
	union {
		/* 17
		 * reserved for UW cards,
		 * actual sync rate for TID 12-15
		 * on U2W and U160 cards.
		 */
		u_int16_t reserved;
		u_int16_t sdtr4;
	} sync4;
	u_int16_t serial_number[3]; /* 18-20 */
	u_int16_t checksum;	/* 21 */
	u_int8_t  oem_name[16];	/* 22 - 29 */
	u_int16_t dvc_err_code;	/* 30 */
	u_int16_t adv_err_code;	/* 31 */
	u_int16_t adv_err_addr;	/* 32 */
	u_int16_t saved_dvc_err_code; /* 33 */
	u_int16_t saved_adv_err_code; /* 34 */
	u_int16_t saved_adv_err_addr; /* 35 */
	u_int16_t reserved[20];	      /* 36 - 55 */
	u_int16_t cisptr_lsw;	/* 56 CIS data */
	u_int16_t cisptr_msw;	/* 57 CIS data */
	u_int32_t subid;	/* 58-59 SubSystem Vendor/Dev ID */
	u_int16_t reserved2[4];
};

/* EEProm Addresses */
#define	ADW_EEP_DVC_CFG_BEGIN		0x00
#define	ADW_EEP_DVC_CFG_END	(offsetof(struct adw_eeprom, checksum)/2)
#define	ADW_EEP_DVC_CTL_BEGIN	(offsetof(struct adw_eeprom, oem_name)/2)
#define	ADW_EEP_MAX_WORD_ADDR	(sizeof(struct adw_eeprom)/2)

#define ADW_BUS_RESET_HOLD_DELAY_US 100

typedef enum {
	ADW_CHIP_NONE,
	ADW_CHIP_ASC3550,	/* Ultra-Wide IC */
	ADW_CHIP_ASC38C0800,	/* Ultra2-Wide/LVD IC */
	ADW_CHIP_ASC38C1600	/* Ultra3-Wide/LVD2 IC */
} adw_chip;

typedef enum {
	ADW_FENONE	  = 0x0000,
	ADW_ULTRA	  = 0x0001,	/* Supports 20MHz Transfers */
	ADW_ULTRA2	  = 0x0002,	/* Supports 40MHz Transfers */
	ADW_DT		  = 0x0004,	/* Supports Double Transistion REQ/ACK*/
	ADW_WIDE  	  = 0x0008,	/* Wide Channel */
	ADW_ASC3550_FE	  = ADW_ULTRA,	
	ADW_ASC38C0800_FE = ADW_ULTRA2,
	ADW_ASC38C1600_FE = ADW_ULTRA2|ADW_DT
} adw_feature;

typedef enum {
	ADW_FNONE	  = 0x0000,
	ADW_EEPROM_FAILED = 0x0001
} adw_flag;

typedef enum {
	ADW_STATE_NORMAL	= 0x00,
	ADW_RESOURCE_SHORTAGE	= 0x01
} adw_state;

typedef enum {
	ADW_MC_SDTR_ASYNC,
	ADW_MC_SDTR_5,
	ADW_MC_SDTR_10,
	ADW_MC_SDTR_20,
	ADW_MC_SDTR_40,
	ADW_MC_SDTR_80
} adw_mc_sdtr;

struct adw_syncrate
{
	adw_mc_sdtr mc_sdtr;
	u_int8_t    period;
	char       *rate;
};

/* We have an input and output queue for our carrier structures */
#define ADW_OUTPUT_QUEUE 0	/* Offset into carriers member */
#define ADW_INPUT_QUEUE 1	/* Offset into carriers member */
#define ADW_NUM_CARRIER_QUEUES 2
struct adw_softc
{
	bus_space_tag_t		  tag;
	bus_space_handle_t	  bsh;
	adw_state		  state;
	bus_dma_tag_t		  buffer_dmat;
	struct acb	         *acbs;
	struct adw_carrier	 *carriers;
	struct adw_carrier	 *free_carriers;
	struct adw_carrier	 *commandq;
	struct adw_carrier	 *responseq;
	LIST_HEAD(, ccb_hdr)	  pending_ccbs;
	SLIST_HEAD(, acb)	  free_acb_list;
	bus_dma_tag_t		  parent_dmat;
	bus_dma_tag_t		  carrier_dmat;	/* dmat for our acb carriers*/
	bus_dmamap_t		  carrier_dmamap;
	bus_dma_tag_t		  acb_dmat;	/* dmat for our ccb array */
	bus_dmamap_t		  acb_dmamap;
	bus_dma_tag_t		  sg_dmat;	/* dmat for our sg maps */
	SLIST_HEAD(, sg_map_node) sg_maps;
	bus_addr_t		  acb_busbase;
	bus_addr_t		  carrier_busbase;
	adw_chip		  chip;
	adw_feature		  features;
	adw_flag		  flags;
	u_int			  memsize;
	char			  channel;
	struct cam_path		 *path;
	struct cam_sim		 *sim;
	struct resource		 *regs;
	struct resource		 *irq;
	void			 *ih;
	const struct adw_mcode	 *mcode_data;
	const struct adw_eeprom	 *default_eeprom;
	device_t		  device;
	int			  regs_res_type;
	int			  regs_res_id;
	int			  irq_res_type;
	u_int			  max_acbs;
	u_int			  num_acbs;
	u_int			  initiator_id;
	u_int			  init_level;
	u_int			  unit;
	char*			  name;
	cam_status		  last_reset;	/* Last reset type */
	u_int16_t		  bios_ctrl;
	u_int16_t		  user_wdtr;
	u_int16_t		  user_sdtr[4];	/* A nibble per-device */
	u_int16_t		  user_tagenb;
	u_int16_t		  tagenb;
	u_int16_t		  user_discenb;
	u_int16_t		  serial_number[3];
};

extern const struct adw_eeprom adw_asc3550_default_eeprom;
extern const struct adw_eeprom adw_asc38C0800_default_eeprom;
extern const struct adw_syncrate adw_syncrates[];
extern const int adw_num_syncrates;

#define adw_inb(adw, port)				\
	bus_space_read_1((adw)->tag, (adw)->bsh, port)
#define adw_inw(adw, port)				\
	bus_space_read_2((adw)->tag, (adw)->bsh, port)
#define adw_inl(adw, port)				\
	bus_space_read_4((adw)->tag, (adw)->bsh, port)

#define adw_outb(adw, port, value)			\
	bus_space_write_1((adw)->tag, (adw)->bsh, port, value)
#define adw_outw(adw, port, value)			\
	bus_space_write_2((adw)->tag, (adw)->bsh, port, value)
#define adw_outl(adw, port, value)			\
	bus_space_write_4((adw)->tag, (adw)->bsh, port, value)

#define adw_set_multi_2(adw, port, value, count)	\
	bus_space_set_multi_2((adw)->tag, (adw)->bsh, port, value, count)

static __inline const char*	adw_name(struct adw_softc *adw);
static __inline u_int	adw_lram_read_8(struct adw_softc *adw, u_int addr);
static __inline u_int	adw_lram_read_16(struct adw_softc *adw, u_int addr);
static __inline u_int	adw_lram_read_32(struct adw_softc *adw, u_int addr);
static __inline void	adw_lram_write_8(struct adw_softc *adw, u_int addr,
					 u_int value);
static __inline void	adw_lram_write_16(struct adw_softc *adw, u_int addr,
					  u_int value);
static __inline void	adw_lram_write_32(struct adw_softc *adw, u_int addr,
					  u_int value);

static __inline u_int32_t	acbvtobo(struct adw_softc *adw,
					   struct acb *acb);
static __inline u_int32_t	acbvtob(struct adw_softc *adw,
					   struct acb *acb);
static __inline struct acb *	acbbotov(struct adw_softc *adw,
					u_int32_t busaddr);
static __inline struct acb *	acbbtov(struct adw_softc *adw,
					u_int32_t busaddr);
static __inline u_int32_t	carriervtobo(struct adw_softc *adw,
					     struct adw_carrier *carrier);
static __inline u_int32_t	carriervtob(struct adw_softc *adw,
					    struct adw_carrier *carrier);
static __inline struct adw_carrier *
				carrierbotov(struct adw_softc *adw,
					     u_int32_t byte_offset);
static __inline struct adw_carrier *
				carrierbtov(struct adw_softc *adw,
					    u_int32_t baddr);

static __inline const char*
adw_name(struct adw_softc *adw)
{
	return (adw->name);
}

static __inline u_int
adw_lram_read_8(struct adw_softc *adw, u_int addr)
{
	adw_outw(adw, ADW_RAM_ADDR, addr);
	return (adw_inb(adw, ADW_RAM_DATA));
}

static __inline u_int
adw_lram_read_16(struct adw_softc *adw, u_int addr)
{
	adw_outw(adw, ADW_RAM_ADDR, addr);
	return (adw_inw(adw, ADW_RAM_DATA));
}

static __inline u_int
adw_lram_read_32(struct adw_softc *adw, u_int addr)
{
	u_int retval;

	adw_outw(adw, ADW_RAM_ADDR, addr);
	retval = adw_inw(adw, ADW_RAM_DATA);
	retval |= (adw_inw(adw, ADW_RAM_DATA) << 16);
	return (retval);
}

static __inline void
adw_lram_write_8(struct adw_softc *adw, u_int addr, u_int value)
{
	adw_outw(adw, ADW_RAM_ADDR, addr);
	adw_outb(adw, ADW_RAM_DATA, value);
}

static __inline void
adw_lram_write_16(struct adw_softc *adw, u_int addr, u_int value)
{
	adw_outw(adw, ADW_RAM_ADDR, addr);
	adw_outw(adw, ADW_RAM_DATA, value);
}

static __inline void
adw_lram_write_32(struct adw_softc *adw, u_int addr, u_int value)
{
	adw_outw(adw, ADW_RAM_ADDR, addr);
	adw_outw(adw, ADW_RAM_DATA, value);
	adw_outw(adw, ADW_RAM_DATA, value >> 16);
}

static __inline u_int32_t
acbvtobo(struct adw_softc *adw, struct acb *acb)
{
	return ((u_int32_t)((caddr_t)acb - (caddr_t)adw->acbs));
}

static __inline u_int32_t
acbvtob(struct adw_softc *adw, struct acb *acb)
{
	return (adw->acb_busbase + acbvtobo(adw, acb));
}

static __inline struct acb *
acbbotov(struct adw_softc *adw, u_int32_t byteoffset)
{
	return ((struct acb *)((caddr_t)adw->acbs + byteoffset));
}

static __inline struct acb *
acbbtov(struct adw_softc *adw, u_int32_t busaddr)
{
	return (acbbotov(adw, busaddr - adw->acb_busbase));
}

/*
 * Return the byte offset for a carrier relative to our array of carriers.
 */
static __inline u_int32_t
carriervtobo(struct adw_softc *adw, struct adw_carrier *carrier)
{
	return ((u_int32_t)((caddr_t)carrier - (caddr_t)adw->carriers));
}

static __inline u_int32_t
carriervtob(struct adw_softc *adw, struct adw_carrier *carrier)
{
	return (adw->carrier_busbase + carriervtobo(adw, carrier));
}

static __inline struct adw_carrier *
carrierbotov(struct adw_softc *adw, u_int32_t byte_offset)
{
	return ((struct adw_carrier *)((caddr_t)adw->carriers + byte_offset));
}

static __inline struct adw_carrier *
carrierbtov(struct adw_softc *adw, u_int32_t baddr)
{
	return (carrierbotov(adw, baddr - adw->carrier_busbase));
}

/* Intialization */
int		adw_find_signature(struct adw_softc *adw);
void		adw_reset_chip(struct adw_softc *adw);
int		adw_reset_bus(struct adw_softc *adw);
u_int16_t	adw_eeprom_read(struct adw_softc *adw, struct adw_eeprom *buf);
void		adw_eeprom_write(struct adw_softc *adw, struct adw_eeprom *buf);
int		adw_init_chip(struct adw_softc *adw, u_int term_scsicfg1);
void		adw_set_user_sdtr(struct adw_softc *adw,
				  u_int tid, u_int mc_sdtr);
u_int		adw_get_user_sdtr(struct adw_softc *adw, u_int tid);
void		adw_set_chip_sdtr(struct adw_softc *adw, u_int tid, u_int sdtr);
u_int		adw_get_chip_sdtr(struct adw_softc *adw, u_int tid);
u_int		adw_find_sdtr(struct adw_softc *adw, u_int period);
u_int		adw_find_period(struct adw_softc *adw, u_int mc_sdtr);
u_int		adw_hshk_cfg_period_factor(u_int tinfo);

/* Idle Commands */
adw_idle_cmd_status_t	adw_idle_cmd_send(struct adw_softc *adw, u_int cmd,
					  u_int parameter);

/* SCSI Transaction Processing */
static __inline void	adw_send_acb(struct adw_softc *adw, struct acb *acb,
				     u_int32_t acb_baddr);

static __inline void	adw_tickle_risc(struct adw_softc *adw, u_int value)
{
	/*
	 * Tickle the RISC to tell it to read its Command Queue Head pointer.
	 */
	adw_outb(adw, ADW_TICKLE, value);
	if (adw->chip == ADW_CHIP_ASC3550) {
		/*
		 * Clear the tickle value. In the ASC-3550 the RISC flag
		 * command 'clr_tickle_a' does not work unless the host
		 * value is cleared.
		 */
		adw_outb(adw, ADW_TICKLE, ADW_TICKLE_NOP);
	}
}

static __inline void
adw_send_acb(struct adw_softc *adw, struct acb *acb, u_int32_t acb_baddr)
{
	struct adw_carrier *new_cq;

	new_cq = adw->free_carriers;
	adw->free_carriers = carrierbotov(adw, new_cq->next_ba);
	new_cq->next_ba = ADW_CQ_STOPPER;

	acb->queue.carrier_baddr = adw->commandq->carr_ba;
	acb->queue.carrier_bo = adw->commandq->carr_offset;
	adw->commandq->areq_ba = acbvtob(adw, acb);
	adw->commandq->next_ba = new_cq->carr_ba;
#if 0
	printf("EnQ 0x%x 0x%x 0x%x 0x%x\n",
	       adw->commandq->carr_offset,
	       adw->commandq->carr_ba,
	       adw->commandq->areq_ba,
	       adw->commandq->next_ba);
#endif
	adw->commandq = new_cq;

	
	adw_tickle_risc(adw, ADW_TICKLE_A);
}
     
#endif /* _ADWLIB_H_ */
