/*
 * Exported interface to downloadable microcode for AdvanSys SCSI Adapters
 *
 *	$Id: advmcode.h,v 1.4 1998/09/15 07:03:34 gibbs Exp $
 *
 * Obtained from:
 *
 * Copyright (c) 1995-1998 Advanced System Products, Inc.
 * All Rights Reserved.
 *   
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that redistributions of source
 * code retain the above copyright notice and this comment without
 * modification.
 */

#ifndef _ADMCODE_H_
#define _ADMCODE_H_

extern u_int16_t adw_mcode[];
extern u_int16_t adw_mcode_size;
extern u_int32_t adw_mcode_chksum;

/*
 * Fixed LRAM locations of microcode operating variables.
 */
#define ADW_MC_CODE_BEGIN_ADDR		0x0028 /* microcode start address */
#define ADW_MC_CODE_END_ADDR		0x002A /* microcode end address */
#define ADW_MC_CODE_CHK_SUM		0x002C /* microcode code checksum */
#define ADW_MC_STACK_BEGIN		0x002E /* microcode stack begin */
#define ADW_MC_STACK_END		0x0030 /* microcode stack end */
#define ADW_MC_VERSION_DATE		0x0038 /* microcode version */
#define ADW_MC_VERSION_NUM		0x003A /* microcode number */
#define ADW_MC_BIOSMEM			0x0040 /* BIOS RISC Memory Start */
#define ADW_MC_BIOSLEN			0x0050 /* BIOS RISC Memory Length */
#define ADW_MC_HALTCODE			0x0094 /* microcode halt code */
#define ADW_MC_CALLERPC			0x0096 /* microcode halt caller PC */
#define ADW_MC_ADAPTER_SCSI_ID		0x0098 /* one ID byte + reserved */
#define ADW_MC_ULTRA_ABLE		0x009C
#define ADW_MC_SDTR_ABLE		0x009E
#define ADW_MC_TAGQNG_ABLE		0x00A0
#define ADW_MC_DISC_ENABLE		0x00A2
#define ADW_MC_IDLE_CMD			0x00A6
#define ADW_MC_IDLE_PARA_STAT		0x00A8
#define ADW_MC_DEFAULT_SCSI_CFG0	0x00AC
#define ADW_MC_DEFAULT_SCSI_CFG1	0x00AE
#define ADW_MC_DEFAULT_MEM_CFG		0x00B0
#define ADW_MC_DEFAULT_SEL_MASK		0x00B2
#define ADW_MC_RISC_NEXT_READY		0x00B4
#define ADW_MC_RISC_NEXT_DONE		0x00B5
#define ADW_MC_SDTR_DONE		0x00B6
#define ADW_MC_NUMBER_OF_QUEUED_CMD	0x00C0
#define ADW_MC_NUMBER_OF_MAX_CMD	0x00D0
#define ADW_MC_DEVICE_HSHK_CFG_TABLE	0x0100
#define 	ADW_HSHK_CFG_WIDE_XFR	0x8000
#define		ADW_HSHK_CFG_RATE_MASK	0x0F00
#define		ADW_HSHK_CFG_RATE_SHIFT	8
#define ADW_HSHK_CFG_PERIOD_FACTOR(cfg_val)	\
((((((cfg_val) & ADW_HSHK_CFG_RATE_MASK) >> ADW_HSHK_CFG_RATE_SHIFT) \
								* 25) + 50)/4)
#define		ADW_HSHK_CFG_OFFSET	0x001F
#define ADW_MC_WDTR_ABLE		0x0120 /* Wide Transfer TID bitmask. */
#define ADW_MC_CONTROL_FLAG		0x0122 /* Microcode control flag. */
#define		ADW_MC_CONTROL_IGN_PERR 0x0001 /* Ignore DMA Parity Errors */
#define ADW_MC_WDTR_DONE		0x0124
#define ADW_MC_HOST_NEXT_READY		0x0128 /* Host Next Ready RQL Entry. */
#define ADW_MC_HOST_NEXT_DONE		0x0129 /* Host Next Done RQL Entry. */

/*
 * LRAM RISC Queue Lists (LRAM addresses 0x1200 - 0x19FF)
 *
 * Each of the 255 Adv Library/Microcode RISC queue lists or mailboxes 
 * starting at LRAM address 0x1200 is 8 bytes and has the following
 * structure. Only 253 of these are actually used for command queues.
 */
#define ADW_MC_RISC_Q_LIST_BASE		0x1200
#define ADW_MC_RISC_Q_LIST_SIZE		0x0008
#define ADW_MC_RISC_Q_TOTAL_CNT		0x00FF /* Num. queue slots in LRAM. */
#define ADW_MC_RISC_Q_FIRST		0x0001
#define ADW_MC_RISC_Q_LAST		0x00FF

/* RISC Queue List structure - 8 bytes */
#define RQL_FWD		0 /* forward pointer (1 byte) */
#define RQL_BWD		1 /* backward pointer (1 byte) */
#define RQL_STATE	2 /* state byte - free, ready, done, aborted (1 byte) */
#define RQL_TID		3 /* request target id (1 byte) */
#define RQL_PHYADDR	4 /* request physical pointer (4 bytes) */
     
/* RISC Queue List state values */
#define ADW_MC_QS_FREE			0x00
#define ADW_MC_QS_READY			0x01
#define ADW_MC_QS_DONE			0x40
#define ADW_MC_QS_ABORTED		0x80

/* RISC Queue List pointer values */
#define ADW_MC_NULL_Q			0x00
#define ADW_MC_BIOS_Q			0xFF

/* ADW_SCSI_REQ_Q 'cntl' field values */
#define ADW_MC_QC_START_MOTOR		0x02	/* Issue start motor. */
#define ADW_MC_QC_NO_OVERRUN		0x04	/* Don't report overrun. */
#define ADW_MC_QC_FIRST_DMA		0x08	/* Internal microcode flag. */
#define ADW_MC_QC_ABORTED		0x10	/* Request aborted by host. */
#define ADW_MC_QC_REQ_SENSE		0x20	/* Auto-Request Sense. */
#define ADW_MC_QC_DOS_REQ		0x80	/* Request issued by DOS. */

/*
 * Microcode idle loop commands
 */
typedef enum {
	ADW_IDLE_CMD_COMPLETED		= 0x0000,
	ADW_IDLE_CMD_STOP_CHIP		= 0x0001,
	ADW_IDLE_CMD_STOP_CHIP_SEND_INT	= 0x0002,
	ADW_IDLE_CMD_SEND_INT		= 0x0004,
	ADW_IDLE_CMD_ABORT		= 0x0008,
	ADW_IDLE_CMD_DEVICE_RESET	= 0x0010,
	ADW_IDLE_CMD_SCSI_RESET		= 0x0020
} adw_idle_cmd_t;

typedef enum {
	ADW_IDLE_CMD_FAILURE		= 0x0000,
	ADW_IDLE_CMD_SUCCESS		= 0x0001
} adw_idle_cmd_status_t;


#endif /* _ADMCODE_H_ */
