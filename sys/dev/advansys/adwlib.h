/*
 * Definitions for low level routines and data structures
 * for the Advanced Systems Inc. SCSI controllers chips.
 *
 * Copyright (c) 1998 Justin T. Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
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
 *      $Id$
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

/*
 * Board Register offsets.
 */
#define ADW_INTR_STATUS_REG			0x0000
#define		ADW_INTR_STATUS_INTRA		0x01
#define		ADW_INTR_STATUS_INTRB		0x02
#define		ADW_INTR_STATUS_INTRC		0x04


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
#define		ADW_DMA_CFG0_IFO_THRESH_48B	0x30
#define		ADW_DMA_CFG0_IFO_THRESH_64B	0x40
#define		ADW_DMA_CFG0_IFO_THRESH_80B	0x50
#define		ADW_DMA_CFG0_IFO_THRESH_96B	0x60
#define		ADW_DMA_CFG0_IFO_THRESH_112B	0x70
#define		ADW_DMA_CFG0_START_CTL_MASK	0x0C
#define		ADW_DMA_CFG0_START_CTL_TH	0x00 /* Start on thresh */
#define		ADW_DMA_CFG0_START_CTL_IDLE	0x04 /* Start when idle */
#define		ADW_DMA_CFG0_START_CTL_TH_IDLE	0x08 /* Either */
#define		ADW_DMA_CFG0_START_CTL_EM_FU	0x0C /* Start on full/empty */
#define		ADW_DMA_CFG0_READ_CMD_MASK	0x03
#define		ADW_DMA_CFG0_READ_CMD_MR	0x00
#define		ADW_DMA_CFG0_READ_CMD_MRL	0x02
#define		ADW_DMA_CFG0_READ_CMD_MRM	0x03

/* Program Counter */
#define ADW_PC					0x2A

#define ADW_SCSI_CTRL				0x0034
#define		ADW_SCSI_CTRL_RSTOUT		0x2000

#define	ADW_SCSI_RESET_HOLD_TIME_US		60

/* LRAM Constants */
#define ADW_CONDOR_MEMSIZE	0x2000 /* 8 KB Internal Memory */
#define ADW_MC_BIOSMEM		0x0040 /* BIOS RISC Memory Start */
#define ADW_MC_BIOSLEN		0x0050 /* BIOS RISC Memory Length */

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
	u_int8_t  first_entry_no;  /* starting entry number */
	u_int8_t  last_entry_no;   /* last entry number */
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
	QHSTA_M_WTM_TIMEOUT	    = 0x41,
	QHSTA_M_BAD_CMPL_STATUS_IN  = 0x42,
	QHSTA_M_NO_AUTO_REQ_SENSE   = 0x43,
	QHSTA_M_AUTO_REQ_SENSE_FAIL = 0x44,
	QHSTA_M_INVALID_DEVICE	    = 0x45 /* Bad target ID */
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
	u_int8_t  sg_entry_cnt;	  /* SG element count. Zero for no SG. */
	u_int8_t  target_id;	  /* Device target identifier. */
	u_int8_t  target_lun;	  /* Device target logical unit number. */
	u_int32_t data_addr;	  /* Data buffer physical address. */
	u_int32_t data_cnt;	  /* Data count. Ucode sets to residual. */
	u_int32_t sense_addr;	  /* Sense buffer physical address. */
	u_int32_t srb_ptr;	  /* Driver request pointer. */
	u_int8_t  a_flag;	  /* Adv Library flag field. */
	u_int8_t  sense_len;	  /* Auto-sense length. Residual on complete. */
	u_int8_t  cdb_len;	  /* SCSI CDB length. */
	u_int8_t  tag_code;	  /* SCSI-2 Tag Queue Code: 00, 20-22. */
	u_int8_t  done_status;	  /* Completion status. */
	u_int8_t  scsi_status;	  /* SCSI status byte. */
	u_int8_t  host_status;	  /* Ucode host status. */

	u_int8_t  ux_sg_ix;       /* Ucode working SG variable. */
	u_int8_t  cdb[12];        /* SCSI command block. */
	u_int32_t sg_real_addr;   /* SG list physical address. */ 
	u_int32_t free_scsiq_link;/* Unused */
	u_int32_t ux_wk_data_cnt; /* Saved data count at disconnection. */
	u_int32_t scsi_req_baddr; /* Bus address of this request. */
	u_int32_t sg_block_index; /* sg_block tag (Unused) */
};

typedef enum {
	ACB_FREE		= 0x00,
	ACB_ACTIVE		= 0x01,
	ACB_RELEASE_SIMQ	= 0x02
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

typedef struct {
	u_int16_t bios_init_dis    :1,/* don't act as initiator. */
	  	  bios_ext_trans   :1,/* > 1 GB support */
		  bios_more_2disk  :1,/* > 2 Disk Support */
		  bios_no_removable:1,/* don't support removables */
		  bios_cd_boot     :1,/* support bootable CD */
				   :1,
		  bios_multi_lun   :1,/* support multiple LUNs */
		  bios_message     :1,/* display BIOS message */
				   :1,
		  bios_reset_sb    :1,/* Reset SCSI bus during init. */
				   :1,
		  bios_quiet	   :1,/* No verbose initialization. */
		  bios_scsi_par_en :1,/* SCSI parity enabled */
				   :3;
} adw_bios_ctrl;

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

				/* bit 13 set - Term Polarity Control */
				/* bit 14 set - BIOS Enable */
				/* bit 15 set - Big Endian Mode */
	u_int16_t cfg_msw;	/* unused */
	u_int16_t disc_enable;
	u_int16_t wdtr_able;
	u_int16_t sdtr_able;
	u_int16_t start_motor;
	u_int16_t tagqng_able;
	u_int16_t bios_scan;
	u_int16_t scam_tolerant;
 
	u_int8_t  adapter_scsi_id;
	u_int8_t  bios_boot_delay;
 
	u_int8_t  scsi_reset_delay;
	u_int8_t  bios_id_lun;	/*    high nibble is lun */  
				/*    low nibble is scsi id */

	u_int8_t  termination;	/* 0 - automatic */
#define		ADW_EEPROM_TERM_AUTO 		0
#define		ADW_EEPROM_TERM_OFF		1
#define		ADW_EEPROM_TERM_HIGH_ON		2
#define		ADW_EEPROM_TERM_BOTH_ON		3

	u_int8_t  reserved1;	/*    reserved byte (not used) */                                  
	adw_bios_ctrl bios_ctrl;

	u_int16_t ultra_able;	/* 13 ULTRA speed able */ 
	u_int16_t reserved2;	/* 14 reserved */
	u_int8_t  max_host_qng;	/* 15 maximum host queuing */
	u_int8_t  max_dvc_qng;	/*    maximum per device queuing */
	u_int16_t dvc_cntl;	/* 16 control bit for driver */
	u_int16_t bug_fix;	/* 17 control bit for bug fix */
	u_int16_t serial_number[3];
	u_int16_t checksum;
	u_int8_t  oem_name[16];
	u_int16_t dvc_err_code;
	u_int16_t adv_err_code;
	u_int16_t adv_err_addr;
	u_int16_t saved_dvc_err_code;
	u_int16_t saved_adv_err_code;
	u_int16_t saved_adv_err_addr;
	u_int16_t num_of_err;
};

/* EEProm Addresses */
#define	ADW_EEP_DVC_CFG_BEGIN		0x00
#define	ADW_EEP_DVC_CFG_END	(offsetof(struct adw_eeprom, checksum)/2)
#define	ADW_EEP_DVC_CTL_BEGIN	(offsetof(struct adw_eeprom, oem_name)/2)
#define	ADW_EEP_MAX_WORD_ADDR	(sizeof(struct adw_eeprom)/2)

typedef enum {
	ADW_STATE_NORMAL	= 0x00,
	ADW_RESOURCE_SHORTAGE	= 0x01
} adw_state;

struct adw_softc
{
	bus_space_tag_t		  tag;
	bus_space_handle_t	  bsh;
	adw_state		  state;
	bus_dma_tag_t		  buffer_dmat;
	struct acb	         *acbs;
	LIST_HEAD(, ccb_hdr)	  pending_ccbs;
	SLIST_HEAD(, acb)	  free_acb_list;
	bus_dma_tag_t		  parent_dmat;
	bus_dma_tag_t		  acb_dmat;	/* dmat for our ccb array */
	bus_dmamap_t		  acb_dmamap;
	bus_dma_tag_t		  sg_dmat;	/* dmat for our sg maps */
	SLIST_HEAD(, sg_map_node) sg_maps;
	bus_addr_t		  acb_busbase;
	struct cam_path		 *path;
	struct cam_sim		 *sim;
	u_int			  max_acbs;
	u_int			  num_acbs;
	u_int			  initiator_id;
	u_int			  init_level;
	u_int			  unit;
	char*			  name;
	cam_status		  last_reset;	/* Last reset type */
	adw_bios_ctrl		  bios_ctrl;
	adw_idle_cmd_t		  idle_cmd;
	u_int			  idle_cmd_param;
	volatile int		  idle_command_cmp;
	u_int16_t		  user_wdtr;
	u_int16_t		  user_sdtr;
	u_int16_t		  user_ultra;
	u_int16_t		  user_tagenb;
	u_int16_t		  tagenb;
	u_int16_t		  user_discenb;
	u_int16_t		  serial_number[3];
};

extern struct adw_eeprom adw_default_eeprom;

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

/* Intialization */
int		adw_find_signature(bus_space_tag_t tag, bus_space_handle_t bsh);
void		adw_reset_chip(struct adw_softc *adw);
u_int16_t	adw_eeprom_read(struct adw_softc *adw, struct adw_eeprom *buf);
void		adw_eeprom_write(struct adw_softc *adw, struct adw_eeprom *buf);
int		adw_init_chip(struct adw_softc *adw, u_int term_scsicfg1);

/* Idle Commands */
void			adw_idle_cmd_send(struct adw_softc *adw, u_int cmd,
					  u_int parameter);
adw_idle_cmd_status_t	adw_idle_cmd_wait(struct adw_softc *adw);

/* SCSI Transaction Processing */
static __inline void	adw_send_acb(struct adw_softc *adw, struct acb *acb,
				     u_int32_t acb_baddr);

static __inline void
adw_send_acb(struct adw_softc *adw, struct acb *acb, u_int32_t acb_baddr)
{
	u_int next_queue;

	/* Determine the next free queue. */
	next_queue = adw_lram_read_8(adw, ADW_MC_HOST_NEXT_READY);
	next_queue = ADW_MC_RISC_Q_LIST_BASE
		   + (next_queue * ADW_MC_RISC_Q_LIST_SIZE);

	/*
	 * Write the physical address of the host Q to the free Q.
	 */
    	adw_lram_write_32(adw, next_queue + RQL_PHYADDR, acb_baddr);

	adw_lram_write_8(adw, next_queue + RQL_TID, acb->queue.target_id);

	/*
	 * Set the ADW_MC_HOST_NEXT_READY (0x128) microcode variable to
	 * the 'next_queue' request forward pointer.
	 *
	 * Do this *before* changing the 'next_queue' queue to QS_READY.
	 * After the state is changed to QS_READY 'RQL_FWD' will be changed
	 * by the microcode.
	 *
	 */
	adw_lram_write_8(adw, ADW_MC_HOST_NEXT_READY,
			 adw_lram_read_8(adw, next_queue + RQL_FWD));

	/*
	 * Change the state of 'next_queue' request from QS_FREE to
	 * QS_READY which will cause the microcode to pick it up and
	 * execute it.
	 *  
	 * Can't reference 'next_queue' after changing the request
	 * state to QS_READY. The microcode now owns the request.
	 */
	adw_lram_write_8(adw, next_queue + RQL_STATE, ADW_MC_QS_READY);
}
     
#endif /* _ADWLIB_H_ */
