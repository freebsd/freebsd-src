/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 *       Copyright (c) 1997 by Simon Shapiro
 *       All Rights Reserved
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
 * $FreeBSD$
 */

/*
 *
 *  dpt.h:	Definitions and constants used by the SCSI side of the DPT
 *
 *  credits:	Mike Neuffer;	DPT low level code and in other areas as well.
 *		Mark Salyzyn; 	Many vital bits of info and diagnostics.
 *		Justin Gibbs;	FreeBSD API, debugging and style
 *		Ron McDaniels;	SCSI Software Interrupts
 *		FreeBSD.ORG;	Great O/S to work on and for.
 */

#ifndef _DPT_H
#define _DPT_H

#include <sys/ioccom.h>


#undef DPT_USE_DLM_SWI

#define DPT_RELEASE				1
#define DPT_VERSION				4
#define DPT_PATCH				5
#define DPT_MONTH				8
#define DPT_DAY					3
#define DPT_YEAR				18	/* 1998 - 1980 */

#define DPT_CTL_RELEASE			1
#define DPT_CTL_VERSION			0
#define DPT_CTL_PATCH			6

#ifndef PAGESIZ
#define PAGESIZ					4096
#endif

#ifndef physaddr
typedef void *physaddr;
#endif

#undef	DPT_INQUIRE_DEVICES	  /* We have no buyers for this function */
#define DPT_SUPPORT_POLLING	  /* Use polled mode at boot (must be ON!) */
#define DPT_OPENNINGS		8 /* Commands-in-progress per device */

#define DPT_RETRIES		5 /* Times to retry failed commands */
#undef	DPT_DISABLE_SG
#define DPT_HAS_OPEN

/* Arguments to dpt_run_queue() can be: */

#define DPT_MAX_TARGET_MODE_BUFFER_SIZE		8192
#define DPT_FREE_LIST_INCREMENT			64
#define DPT_CMD_LEN	      	 		12

/*
 * How many segments do we want in a Scatter/Gather list?
 * Some HBA's can  do 16, Some 8192. Since we pre-allocate
 * them in fixed increments, we need to put a practical limit on
 * these. A passed parameter (from kernel boot or lkm) would help
 */
#define DPT_MAX_SEGS		      	 	32

/* Debug levels */

#undef	DPT_DEBUG_PCI
#undef	DPT_DEBUG_INIT
#undef	DPT_DEBUG_SETUP
#undef	DPT_DEBUG_STATES
#undef	DPT_DEBUG_CONFIG
#undef	DPT_DEBUG_QUEUES
#undef	DPT_DEBUG_SCSI_CMD
#undef	DPT_DEBUG_SOFTINTR
#undef	DPT_DEBUG_HARDINTR
#undef	DPT_DEBUG_HEX_DUMPS
#undef	DPT_DEBUG_POLLING
#undef	DPT_DEBUG_INQUIRE
#undef	DPT_DEBUG_COMPLETION
#undef	DPT_DEBUG_COMPLETION_ERRORS
#define	DPT_DEBUG_MINPHYS
#undef	DPT_DEBUG_SG
#undef	DPT_DEBUG_SG_SHOW_DATA
#undef	DPT_DEBUG_SCSI_CMD_NAME
#undef	DPT_DEBUG_CONTROL
#undef	DPT_DEBUG_TIMEOUTS
#undef	DPT_DEBUG_SHUTDOWN
#define	DPT_DEBUG_USER_CMD

/*
 * Misc. definitions
 */
#undef TRUE
#undef FALSE
#define TRUE	 		1
#define FALSE			0

#define MAX_CHANNELS	3
#define MAX_TARGETS		16
#define MAX_LUNS        8

/* Map minor numbers to device identity */
#define TARGET_MASK			0x000f
#define BUS_MASK			0x0030
#define HBA_MASK			0x01c0
#define LUN_MASK			0x0e00

#define minor2target(minor)		( minor & TARGET_MASK )
#define minor2bus(minor)		( (minor & BUS_MASK) >> 4 )
#define minor2hba(minor)		( (minor & HBA_MASK) >> 6 )
#define minor2lun(minor)		( (minor & LUN_MASK) >> 9 )

/*
 * Valid values for cache_type
 */
#define DPT_NO_CACHE		       	0
#define DPT_CACHE_WRITETHROUGH		1
#define DPT_CACHE_WRITEBACK		2

#define min(a,b) ((a<b)?(a):(b))

#define MAXISA			       	4
#define MAXPCI		       		16
#define MAXIRQ	       			16
#define MAXTARGET		      	16

#define IS_ISA				'I'
#define IS_PCI				'P'

#define BUSMASTER			0xff
#define PIO			       	0xfe

#define EATA_SIGNATURE			0x41544145 /* little ENDIAN "EATA" */
#define	DPT_BLINK_INDICATOR		0x42445054

#define DPT_ID1				0x12
#define DPT_ID2				0x1
#define ATT_ID1				0x06
#define ATT_ID2				0x94
#define ATT_ID3				0x0

#define NEC_ID1				0x38
#define NEC_ID2				0xa3
#define NEC_ID3				0x82

#define MAX_PCI_DEVICES			32 /* Maximum # Of Devices Per Bus */
#define MAX_METHOD_2			16 /* Max Devices For Method 2 */
#define MAX_PCI_BUS			16 /* Maximum # Of Busses Allowed */

#define DPT_MAX_RETRIES			2

#define READ		       		0
#define WRITE	       			1
#define OTHER			       	2

#define HD(cmd)	((hostdata *)&(cmd->host->hostdata))
#define CD(cmd)	((struct eata_ccb *)(cmd->host_scribble))
#define SD(host) ((hostdata *)&(host->hostdata))

/*
 * EATA Command & Register definitions
 */
    
#define PCI_REG_DPTconfig		       	0x40
#define PCI_REG_PumpModeAddress			0x44
#define PCI_REG_PumpModeData			0x48
#define PCI_REG_ConfigParam1			0x50
#define PCI_REG_ConfigParam2			0x54

#define EATA_CMD_RESET			       	0xf9
#define EATA_COLD_BOOT                          0x06 /* Last resort only! */

#define EATA_CMD_IMMEDIATE		       	0xfa

#define EATA_CMD_DMA_READ_CONFIG		0xfd
#define EATA_CMD_DMA_SET_CONFIG			0xfe
#define EATA_CMD_DMA_SEND_CP			0xff

#define ECS_EMULATE_SENSE		       	0xd4

/*
 * Immediate Commands
 * Beware of this enumeration.	Not all commands are in sequence!
 */

enum dpt_immediate_cmd {
    EATA_GENERIC_ABORT,
    EATA_SPECIFIC_RESET,
    EATA_BUS_RESET,
    EATA_SPECIFIC_ABORT,
    EATA_QUIET_INTR,
    EATA_SMART_ROM_DL_EN,
    EATA_COLD_BOOT_HBA,	/* Only as a last resort	*/
    EATA_FORCE_IO,
    EATA_SCSI_BUS_OFFLINE,
    EATA_RESET_MASKED_BUS,
    EATA_POWER_OFF_WARN
};

#define HA_CTRLREG		0x206 /* control register for HBA */
#define HA_CTRL_DISINT		0x02  /* CTRLREG: disable interrupts */
#define HA_CTRL_RESCPU		0x04  /* CTRLREG: reset processo */
#define HA_CTRL_8HEADS		0x08  /*
				       * CTRLREG: set for drives with
				       * >=8 heads
				       * (WD1003 rudimentary :-)
				       */

#define HA_WCOMMAND		0x07  /* command register offset	*/
#define HA_WIFC		       	0x06  /* immediate command offset	*/
#define HA_WCODE	       	0x05
#define HA_WCODE2	       	0x04
#define HA_WDMAADDR		0x02  /* DMA address LSB offset	*/
#define HA_RERROR	       	0x01  /* Error Register, offset 1 from base */
#define HA_RAUXSTAT		0x08  /* aux status register offset */
#define HA_RSTATUS		0x07  /* status register offset	*/
#define HA_RDATA	       	0x00  /* data register (16bit)	*/
#define HA_WDATA	       	0x00  /* data register (16bit)	*/

#define HA_ABUSY	       	0x01  /* aux busy bit		*/
#define HA_AIRQ			0x02  /* aux IRQ pending bit	*/
#define HA_SERROR	       	0x01  /* pr. command ended in error */
#define HA_SMORE	       	0x02  /* more data soon to come	*/
#define HA_SCORR	       	0x04  /* datio_addra corrected	*/
#define HA_SDRQ		       	0x08  /* data request active	*/
#define HA_SSC		       	0x10  /* seek complete		*/
#define HA_SFAULT	       	0x20  /* write fault		*/
#define HA_SREADY	       	0x40  /* drive ready		*/
#define HA_SBUSY	       	0x80  /* drive busy		*/
#define HA_SDRDY	       	(HA_SSC|HA_SREADY|HA_SDRQ)

/*
 * Message definitions	
 */

enum dpt_message {
	HA_NO_ERROR,		/* No Error				*/
	HA_ERR_SEL_TO,		/* Selection Timeout			*/
	HA_ERR_CMD_TO,		/* Command Timeout			*/
	HA_SCSIBUS_RESET,
	HA_HBA_POWER_UP,	/* Initial Controller Power-up		*/
	HA_UNX_BUSPHASE,	/* Unexpected Bus Phase			*/
	HA_UNX_BUS_FREE,	/* Unexpected Bus Free			*/
	HA_BUS_PARITY,		/* Bus Parity Error			*/
	HA_SCSI_HUNG,		/* SCSI Hung				*/
	HA_UNX_MSGRJCT,		/* Unexpected Message Rejected		*/
	HA_RESET_STUCK,		/* SCSI Bus Reset Stuck			*/
	HA_RSENSE_FAIL,		/* Auto Request-Sense Failed		*/
	HA_PARITY_ERR,		/* Controller Ram Parity Error		*/
	HA_CP_ABORT_NA,		/* Abort Message sent to non-active cmd */
	HA_CP_ABORTED,		/* Abort Message sent to active cmd	*/
	HA_CP_RESET_NA,		/* Reset Message sent to non-active cmd */
	HA_CP_RESET,		/* Reset Message sent to active cmd	*/
	HA_ECC_ERR,		/* Controller Ram ECC Error		*/
	HA_PCI_PARITY,		/* PCI Parity Error			*/
	HA_PCI_MABORT,		/* PCI Master Abort			*/
	HA_PCI_TABORT,		/* PCI Target Abort			*/
	HA_PCI_STABORT		/* PCI Signaled Target Abort		*/
};

#define HA_STATUS_MASK  	0x7F
#define HA_IDENTIFY_MSG 	0x80
#define HA_DISCO_RECO   	0x40            /* Disconnect/Reconnect         */

#define DPT_RW_BUFF_HEART	0X00
#define DPT_RW_BUFF_DLM		0x02
#define DPT_RW_BUFF_ACCESS	0x03

#define HA_INTR_OFF		1
#define HA_INTR_ON	       	0

/* This is really a one-time shot through some black magic */
#define DPT_EATA_REVA 0x1c
#define DPT_EATA_REVB 0x1e
#define DPT_EATA_REVC 0x22
#define DPT_EATA_REVZ 0x24


/* IOCTL List */

#define DPT_RW_CMD_LEN 			32
#define DPT_RW_CMD_DUMP_SOFTC		"dump softc"
#define DPT_RW_CMD_DUMP_SYSINFO		"dump sysinfo"
#define DPT_RW_CMD_DUMP_METRICS		"dump metrics"
#define DPT_RW_CMD_CLEAR_METRICS	"clear metrics"
#define DPT_RW_CMD_SHOW_LED		"show LED"

#define DPT_IOCTL_INTERNAL_METRICS	_IOR('D',  1, dpt_perf_t)
#define DPT_IOCTL_SOFTC		       	_IOR('D',  2, dpt_user_softc_t) 
#define DPT_IOCTL_SEND		       	_IOWR('D', 3, eata_pt_t)
#define SDI_SEND			0x40044444 /* Observed from dptmgr */

/*
 *	Other	definitions
 */

#define DPT_HCP_LENGTH(page)	(ntohs(*(int16_t *)(void *)(&page[2]))+4)
#define DPT_HCP_FIRST(page) 	(&page[4])
#define DPT_HCP_NEXT(param) 	(&param[3 + param[3] + 1])
#define DPT_HCP_CODE(param)	(ntohs(*(int16_t *)(void *)param))


/* Possible return values from dpt_register_buffer() */

#define SCSI_TM_READ_BUFFER	0x3c
#define SCSI_TM_WRITE_BUFFER	0x3b

#define SCSI_TM_MODE_MASK	0x07  /* Strip off reserved and LUN */
#define SCSI_TM_LUN_MASK	0xe0  /* Strip off reserved and LUN */

typedef enum {
	SUCCESSFULLY_REGISTERED,
	DRIVER_DOWN,
	ALREADY_REGISTERED,
	REGISTERED_TO_ANOTHER,
	NOT_REGISTERED,	
	INVALID_UNIT,
	INVALID_SENDER,
	INVALID_CALLBACK,
	NO_RESOURCES
} dpt_rb_t;

typedef enum {
	REGISTER_BUFFER,
	RELEASE_BUFFER
} dpt_rb_op_t;

/*
 * New way for completion routines to reliably copmplete processing.
 * Should take properly typed dpt_softc_t and dpt_ccb_t,
 * but interdependencies preclude that.
 */
typedef void (*ccb_callback)(void *dpt, int bus, void *ccb);

typedef void (*buff_wr_done)(int unit, u_int8_t channel, u_int8_t target,
			     u_int8_t lun, u_int16_t offset, u_int16_t length,
			     int result);

typedef void (*dpt_rec_buff)(int unit, u_int8_t channel, u_int8_t target,
			     u_int8_t lun, void *buffer, u_int16_t offset,
			     u_int16_t length);

/* HBA's Status port (register) bitmap */
typedef struct reg_bit {   /* reading this one will clear the interrupt */
	u_int8_t error :1, /* previous command ended in an error */
		 more  :1, /* More DATA coming soon Poll BSY & DRQ (PIO) */
		 corr  :1, /* data read was successfully corrected with ECC */
		 drq   :1, /* data request active */
		 sc    :1, /* seek complete */
		 fault :1, /* write fault */
		 ready :1, /* drive ready */
		 busy  :1; /* controller busy */
} dpt_status_reg_t;

/* HBA's Auxiliary status port (register) bitmap */
typedef struct reg_abit {  /* reading this won't clear the interrupt */
	u_int8_t abusy :1, /* auxiliary busy */
		 irq   :1, /* set when drive interrupt is asserted */
		       :6;
} dpt_aux_status_t;

/* The EATA Register Set as a structure */
typedef struct eata_register {
	u_int8_t data_reg[2];	/* R, couldn't figure this one out */
	u_int8_t cp_addr[4];	/* W, CP address register */
	union {
		u_int8_t command; /*
				   * W, command code:
				   * [read|set] conf, send CP
				   */
		struct	 reg_bit status; /* R, see register_bit1 */
		u_int8_t statusbyte;
	} ovr;
	struct reg_abit aux_stat; /* R, see register_bit2 */
} eata_reg_t;

/*
 * Holds the results of a READ_CONFIGURATION command
 * Beware of data items which are larger than 1 byte.
 * these come from the DPT in network order.
 * On an Intel ``CPU'' they will be upside down and backwards!
 * The dpt_get_conf function is normally responsible for flipping
 * Everything back.
 */
typedef struct get_conf {  /* Read Configuration Array */
	union {
		struct {
			u_int8_t foo_DevType;
			u_int8_t foo_PageCode;
			u_int8_t foo_Reserved0; 
			u_int8_t foo_len;
		} foo;
		u_int32_t foo_length;	/* Should return 0x22, 0x24, etc */
	} bar;
#define gcs_length	       	bar.foo_length
#define gcs_PageCode		bar.foo.foo_DevType
#define gcs_reserved0		bar.foo.foo_Reserved0
#define gcs_len		       	bar.foo.foo_len

	u_int32_t signature;	/* Signature MUST be "EATA".	ntohl()`ed */

	u_int8_t  version2 :4,
		  version  :4;	/* EATA Version level */

	u_int8_t  OCS_enabled :1, /* Overlap Command Support enabled */
		  TAR_support :1, /* SCSI Target Mode supported */
		  TRNXFR      :1, /* Truncate Transfer Cmd Used in PIO Mode */
		  MORE_support:1, /* MORE supported (PIO Mode Only) */
		  DMA_support :1, /* DMA supported */
		  DMA_valid   :1, /* DRQ value in Byte 30 is valid */
		  ATA	      :1, /* ATA device connected (not supported) */
		  HAA_valid   :1; /* Hostadapter Address is valid */

	u_int16_t cppadlen; /*
			     * Number of pad bytes send after CD data set
			     * to zero for DMA commands. Ntohl()`ed
			     */
	u_int8_t  scsi_idS; /* SCSI ID of controller 2-0 Byte 0 res. */
	u_int8_t  scsi_id2; /* If not, zero is returned */
	u_int8_t  scsi_id1;
	u_int8_t  scsi_id0;
	u_int32_t cplen;    /* CP length: number of valid cp bytes */

	u_int32_t splen;    /* Returned bytes for a received SP command */
	u_int16_t queuesiz; /* max number of queueable CPs */

	u_int16_t dummy;
	u_int16_t SGsiz;	/* max number of SG table entrie */

	u_int8_t  IRQ	     :4,/* IRQ used this HBA */
		  IRQ_TR     :1,/* IRQ Trigger: 0=edge, 1=level	 */
		  SECOND     :1,/* This is a secondary controller */
		  DMA_channel:2;/* DRQ index, DRQ is 2comp of DRQX */

	u_int8_t  sync;		/* 0-7 sync active bitmask (deprecated) */
	u_int8_t  DSBLE   :1,	/* ISA i/o addressing is disabled */
		  FORCADR :1,	/* i/o address has been forced */
		  SG_64K  :1,
		  SG_UAE  :1,
			  :4;

	u_int8_t  MAX_ID   :5,	/* Max number of SCSI target IDs */
		  MAX_CHAN :3;	/* Number of SCSI busses on HBA	 */

	u_int8_t  MAX_LUN;	/* Max number of LUNs */
	u_int8_t	  :3,
		  AUTOTRM :1,
		  M1_inst :1,
		  ID_qest :1,	/* Raidnum ID is questionable */
		  is_PCI  :1,	/* HBA is PCI */
		  is_EISA :1;	/* HBA is EISA */

	u_int8_t  RAIDNUM;	/* unique HBA identifier */
	u_int8_t  unused[4];	/* When doing PIO, you	GET 512 bytes */
    
	/* >>------>>	End of The DPT structure	<<------<< */

	u_int32_t length;	/* True length, after ntohl conversion	*/
} dpt_conf_t;

/* Scatter-Gather list entry */
typedef struct dpt_sg_segment {
	u_int32_t seg_addr;	/* All fields in network byte order */
	u_int32_t seg_len;
} dpt_sg_t;


/* Status Packet */
typedef struct eata_sp {
	u_int8_t  hba_stat :7,	/* HBA status */
		  EOC	  :1;	/* True if command finished */

	u_int8_t  scsi_stat;	/* Target SCSI status */

	u_int8_t  reserved[2];

	u_int32_t residue_len;	/* Number of bytes not transferred */

	u_int32_t ccb_busaddr;

	u_int8_t  sp_ID_Message;
	u_int8_t  sp_Que_Message;
	u_int8_t  sp_Tag_Message;
	u_int8_t  msg[9];
} dpt_sp_t;

/*
 * A strange collection of O/S-Hardware releated bits and pieces.
 * Used by the dpt_ioctl() entry point to return DPT_SYSINFO command.
 */
typedef struct dpt_drive_parameters {
	u_int16_t cylinders; /* Up to 1024 */
	u_int8_t  heads;     /* Up to 255  */
	u_int8_t  sectors;   /* Up to 63   */
} dpt_drive_t;

typedef struct driveParam_S driveParam_T;

#define SI_CMOS_Valid           0x0001
#define SI_NumDrivesValid       0x0002
#define SI_ProcessorValid       0x0004
#define SI_MemorySizeValid      0x0008
#define SI_DriveParamsValid     0x0010
#define SI_SmartROMverValid     0x0020
#define SI_OSversionValid       0x0040
#define SI_OSspecificValid      0x0080	
#define SI_BusTypeValid         0x0100

#define SI_ALL_VALID        	0x0FFF
#define SI_NO_SmartROM     	0x8000

#define SI_ISA_BUS	       	0x00
#define SI_PCI_BUS	       	0x04

#define HBA_BUS_ISA		0x00
#define HBA_BUS_PCI		0x02

typedef struct dpt_sysinfo {
	u_int8_t    drive0CMOS;			/* CMOS Drive 0 Type */
	u_int8_t    drive1CMOS;			/* CMOS Drive 1 Type */
	u_int8_t    numDrives;			/* 0040:0075 contents */
	u_int8_t    processorFamily;		/* Same as DPTSIG definition */
	u_int8_t    processorType;		/* Same as DPTSIG definition */
	u_int8_t    smartROMMajorVersion;
	u_int8_t    smartROMMinorVersion;	/* SmartROM version */
	u_int8_t    smartROMRevision;
	u_int16_t   flags;			/* See bit definitions above */
	u_int16_t   conventionalMemSize;	/* in KB */
	u_int32_t   extendedMemSize;		/* in KB */
	u_int32_t   osType;			/* Same as DPTSIG definition */
	u_int8_t    osMajorVersion;
	u_int8_t    osMinorVersion;		/* The OS version */
	u_int8_t    osRevision;
	u_int8_t    osSubRevision;
	u_int8_t    busType;			/* See defininitions above */
	u_int8_t    pad[3];			/* For alignment */
	dpt_drive_t drives[16];			/* SmartROM Logical Drives */
} dpt_sysinfo_t;

/* SEND_COMMAND packet structure */
typedef struct eata_ccb {
	u_int8_t SCSI_Reset   :1, /* Cause a SCSI Bus reset on the cmd */
		 HBA_Init     :1, /* Cause Controller to reinitialize */
		 Auto_Req_Sen :1, /* Do Auto Request Sense on errors */
		 scatter      :1, /* Data Ptr points to a SG Packet */
		 Quick	      :1, /* Set this one for NO Status PAcket */
		 Interpret    :1, /* Interpret the SCSI cdb for own use */
		 DataOut      :1, /* Data Out phase with command */
		 DataIn	      :1; /* Data In phase with command */

	u_int8_t reqlen;	  /* Request Sense Length, if Auto_Req_Sen=1 */
	u_int8_t unused[3];
	u_int8_t FWNEST  :1,	  /* send cmd to phys RAID component */
		 unused2 :7;

	u_int8_t Phsunit	:1, /* physical unit on mirrored pair */
		 I_AT		:1, /* inhibit address translation  */
		 Disable_Cache	:1, /* HBA inhibit caching */
				:5;

	u_int8_t cp_id		:5, /* SCSI Device ID of target */
		 cp_channel	:3; /* SCSI Channel # of HBA */

	u_int8_t cp_LUN		:5,
		 cp_luntar	:1, /* CP is for target ROUTINE */
		 cp_dispri	:1, /* Grant disconnect privilege */
		 cp_identify	:1; /* Always TRUE */

	u_int8_t cp_msg[3];	/* Message bytes 0-3 */

	union {
		struct {
			u_int8_t x_scsi_cmd; /* Partial SCSI CDB def */

			u_int8_t x_extent  :1,
				 x_bytchk  :1,
				 x_reladr  :1,
				 x_cmplst  :1,
				 x_fmtdata :1,
				 x_lun	   :3;

			u_int8_t x_page;
			u_int8_t reserved4;
			u_int8_t x_len;
			u_int8_t x_link	   :1;
			u_int8_t x_flag	   :1;
			u_int8_t reserved5 :4;
			u_int8_t x_vendor  :2;
		} x;
		u_int8_t z[12];	/* Command Descriptor Block (= 12) */
	} cp_w;

#define cp_cdb		cp_w.z
#define cp_scsi_cmd	cp_w.x.x_scsi_cmd
#define cp_extent      	cp_w.x.x_extent
#define cp_lun 		cp_w.x.x_lun
#define cp_page	       	cp_w.x.x_page 
#define cp_len	       	cp_w.x.x_len

#define MULTIFUNCTION_CMD	0x0e	/* SCSI Multi Function Cmd */
#define BUS_QUIET		0x04	/* Quite Scsi Bus Code     */
#define BUS_UNQUIET		0x05	/* Un Quiet Scsi Bus Code  */

	u_int32_t cp_datalen;	/*
				 * Data Transfer Length. If scatter=1 len (IN
				 * BYTES!) of the S/G array
				 */

	u_int32_t cp_busaddr;	/* Unique identifier.  Busaddr works well */
	u_int32_t cp_dataDMA;	/*
				 * Data Address, if scatter=1 then it is the
				 * address of scatter packet
				 */
	u_int32_t cp_statDMA;	/* address for Status Packet */
	u_int32_t cp_reqDMA;	/*
				 * Request Sense Address, used if CP command
				 * ends with error
				 */
	u_int8_t  CP_OpCode;
	
} eata_ccb_t;

/* 
 * DPT Signature Structure.
 * Used by /dev/dpt to directly pass commands to the HBA
 * We have more information here than we care for...
 */

/* Current Signature Version - sigBYTE dsSigVersion; */
#define SIG_VERSION 1

/* 
 * Processor Family - sigBYTE dsProcessorFamily;	DISTINCT VALUE 
 *
 * What type of processor the file is meant to run on.
 * This will let us know whether to read sigWORDs as high/low or low/high.
 */
#define PROC_INTEL	0x00	/* Intel 80x86 */
#define PROC_MOTOROLA	0x01	/* Motorola 68K */
#define PROC_MIPS4000	0x02	/* MIPS RISC 4000 */
#define PROC_ALPHA	0x03	/* DEC Alpha */

/* 
 * Specific Minimim Processor - sigBYTE dsProcessor; FLAG BITS 
 *
 * Different bit definitions dependent on processor_family
 */

/* PROC_INTEL: */
#define PROC_8086	0x01   	/* Intel 8086 */
#define PROC_286	0x02   	/* Intel 80286 */
#define PROC_386	0x04	/* Intel 80386 */
#define PROC_486	0x08	/* Intel 80486 */
#define PROC_PENTIUM	0x10	/* Intel 586 aka P5 aka Pentium */
#define PROC_P6		0x20	/* Intel 686 aka P6 */

/* PROC_MOTOROLA: */
#define PROC_68000	0x01   	/* Motorola 68000 */
#define PROC_68020	0x02   	/* Motorola 68020 */
#define PROC_68030	0x04   	/* Motorola 68030 */
#define PROC_68040	0x08	/* Motorola 68040 */

/* Filetype - sigBYTE dsFiletype; DISTINCT VALUES */
#define FT_EXECUTABLE	0      	/* Executable Program */
#define FT_SCRIPT	1      	/* Script/Batch File??? */
#define FT_HBADRVR	2     	/* HBA Driver */
#define FT_OTHERDRVR	3	/* Other Driver */
#define FT_IFS		4	/* Installable Filesystem Driver */
#define FT_ENGINE	5	/* DPT Engine */
#define FT_COMPDRVR	6	/* Compressed Driver Disk */
#define FT_LANGUAGE	7	/* Foreign Language file */
#define FT_FIRMWARE	8	/* Downloadable or actual Firmware */
#define FT_COMMMODL	9	/* Communications Module */
#define FT_INT13       	10    	/* INT 13 style HBA Driver */
#define FT_HELPFILE	11	/* Help file */
#define FT_LOGGER	12	/* Event Logger */
#define FT_INSTALL	13	/* An Install Program */
#define FT_LIBRARY	14	/* Storage Manager Real-Mode Calls */
#define FT_RESOURCE	15	/* Storage Manager Resource File */
#define FT_MODEM_DB	16	/* Storage Manager Modem Database */

/* Filetype flags - sigBYTE dsFiletypeFlags;		FLAG BITS */
#define FTF_DLL	       	0x01	/* Dynamic Link Library */
#define FTF_NLM		0x02	/* Netware Loadable Module */
#define FTF_OVERLAYS	0x04	/* Uses overlays */
#define FTF_DEBUG	0x08	/* Debug version */
#define FTF_TSR		0x10	/* TSR */
#define FTF_SYS		0x20	/* DOS Lodable driver */
#define FTF_PROTECTED	0x40	/* Runs in protected mode */
#define FTF_APP_SPEC	0x80	/* Application Specific */

/* OEM - sigBYTE dsOEM;	DISTINCT VALUES */
#define OEM_DPT		0	/* DPT */
#define OEM_ATT		1	/* ATT */
#define OEM_NEC		2	/* NEC */
#define OEM_ALPHA	3	/* Alphatronix */
#define OEM_AST		4	/* AST */
#define OEM_OLIVETTI	5	/* Olivetti */
#define OEM_SNI		6	/* Siemens/Nixdorf */

/* Operating System	- sigLONG dsOS;		FLAG BITS */
#define OS_DOS			0x00000001 /* PC/MS-DOS */
#define OS_WINDOWS		0x00000002 /* Microsoft Windows 3.x */
#define OS_WINDOWS_NT		0x00000004 /* Microsoft Windows NT */
#define OS_OS2M			0x00000008 /* OS/2 1.2.x,MS 1.3.0,IBM 1.3.x */
#define OS_OS2L			0x00000010 /* Microsoft OS/2 1.301 - LADDR */
#define OS_OS22x		0x00000020 /* IBM OS/2 2.x */
#define OS_NW286		0x00000040 /* Novell NetWare 286 */
#define OS_NW386		0x00000080 /* Novell NetWare 386 */
#define OS_GEN_UNIX		0x00000100 /* Generic Unix */
#define OS_SCO_UNIX		0x00000200 /* SCO Unix */
#define OS_ATT_UNIX		0x00000400 /* ATT Unix */
#define OS_UNIXWARE		0x00000800 /* UnixWare Unix */
#define OS_INT_UNIX		0x00001000 /* Interactive Unix */
#define OS_SOLARIS		0x00002000 /* SunSoft Solaris */
#define OS_QN			0x00004000 /* QNX for Tom Moch */
#define OS_NEXTSTEP		0x00008000 /* NeXTSTEP */
#define OS_BANYAN		0x00010000 /* Banyan Vines */
#define OS_OLIVETTI_UNIX	0x00020000 /* Olivetti Unix */
#define OS_FREEBSD     		0x00040000 /* FreeBSD 2.2 and later */
#define OS_OTHER		0x80000000 /* Other */

/* Capabilities - sigWORD dsCapabilities; FLAG BITS */
#define CAP_RAID0	0x0001	/* RAID-0 */
#define CAP_RAID1	0x0002	/* RAID-1 */
#define CAP_RAID3	0x0004	/* RAID-3 */
#define CAP_RAID5	0x0008	/* RAID-5 */
#define CAP_SPAN	0x0010	/* Spanning */
#define CAP_PASS	0x0020	/* Provides passthrough */
#define CAP_OVERLAP	0x0040	/* Passthrough supports overlapped commands */
#define CAP_ASPI	0x0080	/* Supports ASPI Command Requests */
#define CAP_ABOVE16MB	0x0100	/* ISA Driver supports greater than 16MB */
#define CAP_EXTEND	0x8000	/* Extended info appears after description */

/* Devices Supported - sigWORD dsDeviceSupp;		FLAG BITS */
#define DEV_DASD	0x0001	/* DASD (hard drives) */
#define DEV_TAPE	0x0002	/* Tape drives */
#define DEV_PRINTER	0x0004	/* Printers */
#define DEV_PROC	0x0008	/* Processors */
#define DEV_WORM	0x0010	/* WORM drives */
#define DEV_CDROM	0x0020	/* CD-ROM drives */
#define DEV_SCANNER	0x0040	/* Scanners */
#define DEV_OPTICAL	0x0080	/* Optical Drives */
#define DEV_JUKEBOX	0x0100	/* Jukebox */
#define DEV_COMM	0x0200	/* Communications Devices */
#define DEV_OTHER	0x0400	/* Other Devices */
#define DEV_ALL		0xFFFF	/* All SCSI Devices */

/* Adapters Families Supported - sigWORD dsAdapterSupp; FLAG BITS */
#define ADF_2001	0x0001	/* PM2001 */
#define ADF_2012A	0x0002	/* PM2012A */
#define ADF_PLUS_ISA	0x0004	/* PM2011,PM2021 */
#define ADF_SC3_ISA	0x0010	/* PM2021 */
#define ADF_SC3_PCI	0x0040	/* SmartCache III PCI */
#define ADF_SC4_ISA	0x0080	/* SmartCache IV ISA */
#define ADF_SC4_PCI	0x0200	/* SmartCache IV PCI */
#define ADF_ALL_MASTER	0xFFFE	/* All bus mastering */
#define ADF_ALL_CACHE	0xFFFC	/* All caching */
#define ADF_ALL		0xFFFF	/* ALL DPT adapters */

/* Application - sigWORD dsApplication;				FLAG BITS */
#define APP_DPTMGR	0x0001	/* DPT Storage Manager */
#define APP_ENGINE	0x0002	/* DPT Engine */
#define APP_SYTOS	0x0004	/* Sytron Sytos Plus */
#define APP_CHEYENNE	0x0008	/* Cheyenne ARCServe + ARCSolo */
#define APP_MSCDEX	0x0010	/* Microsoft CD-ROM extensions */
#define APP_NOVABACK	0x0020	/* NovaStor Novaback */
#define APP_AIM		0x0040	/* Archive Information Manager */

/* Requirements - sigBYTE dsRequirements;	FLAG BITS */
#define REQ_SMARTROM	0x01   	/* Requires SmartROM to be present */
#define REQ_DPTDDL	0x02   	/* Requires DPTDDL.SYS to be loaded */
#define REQ_HBA_DRIVER	0x04   	/* Requires an HBA driver to be loaded	*/
#define REQ_ASPI_TRAN	0x08   	/* Requires an ASPI Transport Modules	*/
#define REQ_ENGINE	0x10   	/* Requires a DPT Engine to be loaded	*/
#define REQ_COMM_ENG	0x20	/* Requires a DPT Communications Engine */

typedef struct dpt_sig {
	char	  dsSignature[6];  /* ALWAYS "dPtSiG" */
	u_int8_t  SigVersion;	   /* signature version (currently 1) */
	u_int8_t  ProcessorFamily; /* what type of processor */
	u_int8_t  Processor;	   /* precise processor */
	u_int8_t  Filetype;	   /* type of file */
	u_int8_t  FiletypeFlags;   /* flags to specify load type, etc. */
	u_int8_t  OEM;		   /* OEM file was created for */
	u_int32_t OS;		   /* which Operating systems */
	u_int16_t Capabilities;	   /* RAID levels, etc. */
	u_int16_t DeviceSupp;	   /* Types of SCSI devices supported */
	u_int16_t AdapterSupp;	   /* DPT adapter families supported */
	u_int16_t Application;	   /* applications file is for */
	u_int8_t  Requirements;	   /* Other driver dependencies */
	u_int8_t  Version;	   /* 1 */
	u_int8_t  Revision;	   /* 'J' */
	u_int8_t  SubRevision;	   /* '9', ' ' if N/A */
	u_int8_t  Month;	   /* creation month */
	u_int8_t  Day;		   /* creation day */
	u_int8_t  Year;		   /* creation year since 1980  */
	char	 *Description;	   /* description (NULL terminated) */
} dpt_sig_t;

/* 32 bytes minimum - with no description. Put NULL at description[0] */
/* 81 bytes maximum - with 49 character description plus NULL. */

/* This line added at Roycroft's request */
/* Microsoft's NT compiler gets confused if you do a pack and don't */
/* restore it. */
typedef struct eata_pass_through {
	u_int8_t    eataID[4];
	u_int32_t   command;

#define EATAUSRCMD	(('D'<<8)|65)  	/* EATA PassThrough Command	*/
#define DPT_SIGNATURE	(('D'<<8)|67)  	/* Get Signature Structure */
#define DPT_NUMCTRLS   	(('D'<<8)|68)	/* Get Number Of DPT Adapters */
#define DPT_CTRLINFO   	(('D'<<8)|69)  	/* Get Adapter Info Structure */ 
#define DPT_SYSINFO    	(('D'<<8)|72)  	/* Get System Info Structure	*/
#define DPT_BLINKLED   	(('D'<<8)|75)	/* Get The BlinkLED Status */

	u_int8_t   *command_buffer;
	eata_ccb_t  command_packet;
	u_int32_t   timeout;
	u_int8_t    host_status;
	u_int8_t    target_status;
	u_int8_t    retries;
} eata_pt_t;

typedef enum {
	DCCB_FREE		= 0x00,
	DCCB_ACTIVE		= 0x01,
	DCCB_RELEASE_SIMQ	= 0x02
} dccb_state;

typedef struct dpt_ccb {
	eata_ccb_t	 eata_ccb;
	bus_dmamap_t	 dmamap;
	struct callout	 timer;
	dpt_sg_t	*sg_list;
	u_int32_t	 sg_busaddr;
	dccb_state	 state;
	union		 ccb *ccb;
	struct		 scsi_sense_data sense_data;
	u_int8_t	 tag;
	u_int8_t	 retries;
	u_int8_t	 status; /* status of this queueslot */
	u_int8_t	*cmd;	 /* address of cmd */

	u_int32_t	 transaction_id;
	u_int32_t	 result;
	caddr_t		 data;
	SLIST_ENTRY(dpt_ccb) links;

#ifdef DPT_MEASURE_PERFORMANCE
	u_int32_t	 submitted_time;
	struct		 timeval command_started;
	struct		 timeval command_ended;
#endif
} dpt_ccb_t;

/*
 * This is provided for compatibility with UnixWare only.
 * Some of the fields may be bogus.
 * Others may have a totally different meaning.
 */
typedef struct dpt_scsi_ha {
    u_int32_t	 ha_state;		/* Operational state */
    u_int8_t	 ha_id[MAX_CHANNELS];	/* Host adapter SCSI ids */
    int32_t	 ha_base;		/* Base I/O address */
    int		 ha_max_jobs;		/* Max number of Active Jobs */
    int		 ha_cache:2;		/* Cache parameters */
    int		 ha_cachesize:30;	/* In meg, only if cache present*/
    int		 ha_nbus;		/* Number Of Busses on HBA */
    int		 ha_ntargets;		/* Number Of Targets Supported */
    int		 ha_nluns;		/* Number Of LUNs Supported */
    int		 ha_tshift;		/* Shift value for target */
    int		 ha_bshift;		/* Shift value for bus */
    int		 ha_npend;		/* # of jobs sent to HBA */
    int		 ha_active_jobs;	/* Number Of Active Jobs */
    char	 ha_fw_version[4];	/* Firmware Revision Level */
    void	*ha_ccb;		/* Controller command blocks */
    void	*ha_cblist;		/* Command block free list */
    void	*ha_dev;		/* Logical unit queues */
    void	*ha_StPkt_lock;		/* Status Packet Lock */
    void	*ha_ccb_lock;		/* CCB Lock */
    void	*ha_LuQWaiting;		/* Lu Queue Waiting List */
    void	*ha_QWait_lock;		/* Device Que Waiting Lock */
    int		 ha_QWait_opri;		/* Saved Priority Level */
#ifdef DPT_TARGET_MODE
    dpt_ccb_t	*target_ccb[MAX_CHANNELS]; /* Command block waiting writebuf */
#endif
} dpt_compat_ha_t;

/*
 * Describe the Inquiry Data returned on Page 0 from the Adapter. The
 * Page C1 Inquiry Data is described in the DptConfig_t structure above.
 */
typedef struct {
    u_int8_t	deviceType;
    u_int8_t	rm_dtq;
    u_int8_t	otherData[6];
    u_int8_t	vendor[8];
    u_int8_t	modelNum[16];
    u_int8_t	firmware[4];
    u_int8_t	protocol[4];
} dpt_inq_t;

/*
 * sp_EOC is not `safe', so I will check sp_Messages[0] instead!
 */
#define DptStat_BUSY(x)	 ((x)->sp_ID_Message)
#define DptStat_Reset_BUSY(x)			\
 ((x)->msg[0] = 0xA5, (x)->EOC = 0,		\
  (x)->ccb_busaddr = ~0)

#ifdef DPT_MEASURE_PERFORMANCE
#define BIG_ENOUGH	0x8fffffff	
typedef struct dpt_metrics {
	u_int32_t command_count[256]; /* We assume MAX 256 SCSI commands */
	u_int32_t max_command_time[256];
	u_int32_t min_command_time[256];

	u_int32_t min_intr_time;
	u_int32_t max_intr_time;
	u_int32_t aborted_interrupts;
	u_int32_t spurious_interrupts;

	u_int32_t max_waiting_count;
	u_int32_t max_submit_count;
	u_int32_t max_complete_count;
    
	u_int32_t min_waiting_time;
	u_int32_t min_submit_time;
	u_int32_t min_complete_time;
    
	u_int32_t max_waiting_time; 
	u_int32_t max_submit_time;
	u_int32_t max_complete_time;
    
	u_int32_t command_collisions;
	u_int32_t command_too_busy;
	u_int32_t max_eata_tries;
	u_int32_t min_eata_tries;

	u_int32_t read_by_size_count[10];
	u_int32_t write_by_size_count[10];
	u_int32_t read_by_size_min_time[10];
	u_int32_t read_by_size_max_time[10];
	u_int32_t write_by_size_min_time[10];
	u_int32_t write_by_size_max_time[10];

#define SIZE_512	0
#define SIZE_1K		1
#define SIZE_2K		2
#define SIZE_4K		3
#define SIZE_8K		4
#define SIZE_16K	5
#define SIZE_32K	6
#define SIZE_64K	7
#define SIZE_BIGGER	8
#define SIZE_OTHER	9

	struct	  timeval intr_started;

	u_int32_t warm_starts;
	u_int32_t cold_boots;
} dpt_perf_t;
#endif

struct sg_map_node {
	bus_dmamap_t		 sg_dmamap;
	bus_addr_t		 sg_physaddr;
	dpt_sg_t*		 sg_vaddr;
	SLIST_ENTRY(sg_map_node) links;
};

/* Main state machine and interface structure */
typedef struct dpt_softc {
	device_t		dev;
	struct mtx		lock;

	struct resource *	io_res;
	int			io_rid;
	int			io_type;
	int			io_offset;

	struct resource *	irq_res;
	int			irq_rid;
	void *			ih;

	struct resource *	drq_res;
	int			drq_rid;

	bus_dma_tag_t	   buffer_dmat;		/* dmat for buffer I/O */
	dpt_ccb_t	  *dpt_dccbs;		/* Array of dpt ccbs */
	bus_addr_t	   dpt_ccb_busbase;	/* phys base address of array */
	bus_addr_t	   dpt_ccb_busend;	/* phys end address of array */

	u_int32_t handle_interrupts   :1, /* Are we ready for real work? */
		  target_mode_enabled :1,
		  resource_shortage   :1,
		  cache_type	      :2,
		  spare		      :28;

	int	  total_dccbs;
	int	  free_dccbs;
	int	  pending_ccbs;
	int	  completed_ccbs;

	SLIST_HEAD(, dpt_ccb)	 free_dccb_list;
	LIST_HEAD(, ccb_hdr)     pending_ccb_list;

	bus_dma_tag_t		  parent_dmat;
	bus_dma_tag_t		  dccb_dmat;	/* dmat for our ccb array */
	bus_dmamap_t		  dccb_dmamap;
	bus_dma_tag_t		  sg_dmat;	/* dmat for our sg maps */
	SLIST_HEAD(, sg_map_node) sg_maps;

	struct cam_sim		  *sims[MAX_CHANNELS];
	struct cam_path		  *paths[MAX_CHANNELS];
	u_int32_t commands_processed;
	u_int32_t lost_interrupts;

	/*
	 * These three parameters can be used to allow for wide scsi, and
	 * for host adapters that support multiple busses. The first two
	 * should be set to 1 more than the actual max id or lun (i.e. 8 for
	 * normal systems).
	 *
	 * There is a FAT assumption here;  We assume that these will never 
	 * exceed MAX_CHANNELS, MAX_TARGETS, MAX_LUNS
	 */
	u_int	  channels;	/* # of avail scsi chan. */
	u_int32_t max_id;
	u_int32_t max_lun;

	u_int8_t  irq;
	u_int8_t  dma_channel;

	TAILQ_ENTRY(dpt_softc) links;
	int	  init_level;

	/*
	 * Every object on a unit can have a receiver, if it treats
	 * us as a target.  We do that so that separate and independent
	 * clients can consume received buffers.
	 */

#define DPT_RW_BUFFER_SIZE	(8 * 1024)
	dpt_ccb_t	*target_ccb[MAX_CHANNELS][MAX_TARGETS][MAX_LUNS];
	u_int8_t	*rw_buffer[MAX_CHANNELS][MAX_TARGETS][MAX_LUNS];
	dpt_rec_buff	 buffer_receiver[MAX_CHANNELS][MAX_TARGETS][MAX_LUNS];
	
	dpt_inq_t	 board_data;
	u_int8_t	 EATA_revision;
	u_int8_t	 bustype;	/* bustype of HBA	 */
	u_int32_t	 state;		/* state of HBA		 */

#define DPT_HA_FREE	       	0x00000000
#define DPT_HA_OK	       	0x00000000
#define DPT_HA_NO_TIMEOUT      	0x00000000
#define DPT_HA_BUSY	       	0x00000001
#define DPT_HA_TIMEOUT	       	0x00000002
#define DPT_HA_RESET	       	0x00000004
#define DPT_HA_LOCKED	       	0x00000008
#define DPT_HA_ABORTED	       	0x00000010
#define DPT_HA_CONTROL_ACTIVE  	0x00000020
#define DPT_HA_SHUTDOWN_ACTIVE  0x00000040
#define DPT_HA_COMMAND_ACTIVE  	0x00000080
#define DPT_HA_QUIET            0x00000100

	u_int8_t  primary;	/* true if primary */	

	u_int8_t  more_support		:1,	/* HBA supports MORE flag */
		  immediate_support	:1,	/* HBA supports IMMEDIATE */
		  spare2		:6;

	u_int8_t  resetlevel[MAX_CHANNELS];
	u_int32_t last_ccb;	/* Last used ccb */
	u_int32_t cplen;		/* size of CP in words */
	u_int16_t cppadlen;	/* pad length of cp */
	u_int16_t max_dccbs;
	u_int16_t sgsize;		/* Entries in the SG list */
	u_int8_t  hostid[MAX_CHANNELS];	/* SCSI ID of HBA */
	u_int32_t cache_size;

	volatile   dpt_sp_t *sp;		/* status packet */
	/* Copied from the status packet during interrupt handler */
	u_int8_t   hba_stat;
	u_int8_t   scsi_stat;	/* Target SCSI status */
	u_int32_t  residue_len;	/* Number of bytes not transferred */
	bus_addr_t sp_physaddr;		/* phys address of status packet */

	/*
	 * We put ALL conditional elements at the tail for the structure.
	 * If we do not, then userland code will crash or trash based on which
	 * kernel it is running with.
	 * This isi most visible with usr/sbin/dpt_softc(8)
	 */

#ifdef DPT_MEASURE_PERFORMANCE
	dpt_perf_t performance;
#endif

#ifdef DPT_RESET_HBA
	struct timeval last_contact;
#endif
} dpt_softc_t;

/*
 * This structure is used to pass dpt_softc contents to userland via the 
 * ioctl DPT_IOCTL_SOFTC.  The reason for this maddness, is that FreeBSD
 * (all BSDs ?) chose to actually assign a nasty meaning to the IOCTL word,
 * encoding 13 bits of it as size.  As dpt_softc_t is somewhere between
 * 8,594 and 8,600 (depends on options), we have to copy the data to
 * something less than 4KB long. This siliness also solves the problem of
 * varying definition of dpt_softc_t, As the variants are exluded from
 * dpt_user_softc.
 *
 * See dpt_softc_t above for enumerations, comments and such.
 */
typedef struct dpt_user_softc {
	int	  unit;
	u_int32_t handle_interrupts   :1, /* Are we ready for real work? */
		  target_mode_enabled :1,
		  spare		      :30;

	int	  total_ccbs_count;
	int	  free_ccbs_count;
	int	  waiting_ccbs_count; 
	int	  submitted_ccbs_count;
	int	  completed_ccbs_count;

	u_int32_t queue_status;
	u_int32_t free_lock;
	u_int32_t waiting_lock;
	u_int32_t submitted_lock; 
	u_int32_t completed_lock;

	u_int32_t commands_processed;
	u_int32_t lost_interrupts;

	u_int8_t  channels;
	u_int32_t max_id;
	u_int32_t max_lun;

	u_int16_t io_base; 
	u_int8_t *v_membase;
	u_int8_t *p_membase;

	u_int8_t  irq;
	u_int8_t  dma_channel;

	dpt_inq_t board_data;
	u_int8_t  EATA_revision;
	u_int8_t  bustype;
	u_int32_t state;

	u_int8_t  primary;
	u_int8_t  more_support 	    :1,
		  immediate_support :1,
		  spare2	    :6;

	u_int8_t  resetlevel[MAX_CHANNELS];
	u_int32_t last_ccb;
	u_int32_t cplen;
	u_int16_t cppadlen;
	u_int16_t queuesize;
	u_int16_t sgsize;
	u_int8_t  hostid[MAX_CHANNELS];
	u_int32_t cache_type :2,
		  cache_size :30;
} dpt_user_softc_t;

/*
 * Externals:
 * These all come from dpt_scsi.c
 *
 */
#ifdef _KERNEL
/* This function gets the current hi-res time and returns it to the caller */
static __inline struct timeval
dpt_time_now(void)
{
	struct timeval now;

	microtime(&now);
	return(now);
}

/*
 * Given a minor device number, get its SCSI Unit.
 */
static __inline int
dpt_minor2unit(int minor)
{
	return(minor2hba(minor));
}

dpt_softc_t *dpt_minor2softc(int minor_no);

#endif /* _KERNEL */

/*
 * This function subtracts one timval structure from another,
 * Returning the result in usec.
 * It assumes that less than 4 billion usecs passed form start to end.
 * If times are sensless, ~0 is returned.
 */
static __inline u_int32_t
dpt_time_delta(struct timeval start,
	       struct timeval end)
{
    if (start.tv_sec > end.tv_sec)
	return(~0);

    if ( (start.tv_sec == end.tv_sec) && (start.tv_usec > end.tv_usec) )
	return(~0);
	
    return ( (end.tv_sec - start.tv_sec) * 1000000 +
	     (end.tv_usec - start.tv_usec) );
}

extern devclass_t	dpt_devclass;

#ifdef _KERNEL
void			dpt_alloc(device_t);
int			dpt_detach(device_t);
int			dpt_alloc_resources(device_t);
void			dpt_release_resources(device_t);
#endif
void			dpt_free(struct dpt_softc *dpt);
int			dpt_init(struct dpt_softc *dpt);
int			dpt_attach(dpt_softc_t * dpt);
void			dpt_intr(void *arg);

#if 0
extern void		hex_dump(u_char * data, int length,
				 char *name, int no);
extern char		*i2bin(unsigned int no, int length);
extern char		*scsi_cmd_name(u_int8_t cmd);

extern dpt_conf_t	*dpt_get_conf(dpt_softc_t *dpt, u_int8_t page,
				      u_int8_t target, u_int8_t size,
				      int extent);

extern int		dpt_setup(dpt_softc_t * dpt, dpt_conf_t * conf);
extern int		dpt_attach(dpt_softc_t * dpt);
extern void		dpt_shutdown(int howto, dpt_softc_t *dpt);
extern void		dpt_detect_cache(dpt_softc_t *dpt);

extern int		dpt_user_cmd(dpt_softc_t *dpt, eata_pt_t *user_cmd,
				     caddr_t cmdarg, int minor_no);

extern u_int8_t	dpt_blinking_led(dpt_softc_t *dpt);

extern dpt_rb_t	dpt_register_buffer(int unit, u_int8_t channel, u_int8_t target,
				    u_int8_t lun, u_int8_t mode,
				    u_int16_t length, u_int16_t offset, 
				    dpt_rec_buff callback, dpt_rb_op_t op);

extern int	dpt_send_buffer(int unit, u_int8_t channel, u_int8_t target,
				u_int8_t lun, u_int8_t mode, u_int16_t length,
				u_int16_t offset, void *data,
				buff_wr_done callback);



void dpt_reset_performance(dpt_softc_t *dpt);
#endif

#endif /* _DPT_H */
