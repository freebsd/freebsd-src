/*
 * Exported interface to downloadable microcode for AdvanSys SCSI Adapters
 *
 * $FreeBSD$
 *
 * Obtained from:
 *
 * Copyright (c) 1995-1999 Advanced System Products, Inc.
 * All Rights Reserved.
 *   
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that redistributions of source
 * code retain the above copyright notice and this comment without
 * modification.
 */

#ifndef _ADMCODE_H_
#define _ADMCODE_H_

struct adw_mcode
{
	const u_int8_t*	mcode_buf;
	const u_int32_t	mcode_chksum;
	const u_int16_t mcode_size;
};

extern const struct adw_mcode adw_asc3550_mcode_data;
extern const struct adw_mcode adw_asc38C0800_mcode_data;

/*
 * Fixed LRAM locations of microcode operating variables.
 */
#define ADW_MC_CODE_BEGIN_ADDR		0x0028 /* microcode start address */
#define ADW_MC_CODE_END_ADDR		0x002A /* microcode end address */
#define ADW_MC_CODE_CHK_SUM		0x002C /* microcode code checksum */
#define ADW_MC_VERSION_DATE		0x0038 /* microcode version */
#define ADW_MC_VERSION_NUM		0x003A /* microcode number */
#define ADW_MC_BIOSMEM			0x0040 /* BIOS RISC Memory Start */
#define ADW_MC_BIOSLEN			0x0050 /* BIOS RISC Memory Length */
#define ADW_MC_BIOS_SIGNATURE		0x0058 /* BIOS Signature 0x55AA */
#define ADW_MC_BIOS_VERSION		0x005A /* BIOS Version (2 Bytes) */
#define ADW_MC_SDTR_SPEED1		0x0090 /* SDTR Speed for TID 0-3 */
#define ADW_MC_SDTR_SPEED2		0x0092 /* SDTR Speed for TID 4-7 */
#define ADW_MC_SDTR_SPEED3		0x0094 /* SDTR Speed for TID 8-11 */
#define ADW_MC_SDTR_SPEED4		0x0096 /* SDTR Speed for TID 12-15 */
#define ADW_MC_CHIP_TYPE		0x009A
#define ADW_MC_INTRB_CODE		0x009B
#define		ADW_ASYNC_RDMA_FAILURE		0x01 /* Fatal RDMA failure. */
#define		ADW_ASYNC_SCSI_BUS_RESET_DET	0x02 /* Detected Bus Reset. */
#define		ADW_ASYNC_CARRIER_READY_FAILURE 0x03 /* Carrier Ready failure.*/
#define		ADW_ASYNC_HOST_SCSI_BUS_RESET	0x80 /*
						      * Host Initiated
						      * SCSI Bus Reset.
						      */
#define ADW_MC_WDTR_ABLE_BIOS_31	0x0120
#define ADW_MC_WDTR_ABLE		0x009C
#define ADW_MC_SDTR_ABLE		0x009E
#define ADW_MC_TAGQNG_ABLE		0x00A0
#define ADW_MC_DISC_ENABLE		0x00A2
#define ADW_MC_IDLE_CMD_STATUS		0x00A4
#define ADW_MC_IDLE_CMD			0x00A6
#define ADW_MC_IDLE_CMD_PARAMETER	0x00A8
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
#define		ADW_HSHK_CFG_RATE_MASK	0x7F00
#define		ADW_HSHK_CFG_RATE_SHIFT	8
#define		ADW_HSHK_CFG_OFFSET	0x001F
#define ADW_MC_CONTROL_FLAG		0x0122 /* Microcode control flag. */
#define		ADW_MC_CONTROL_IGN_PERR 0x0001 /* Ignore DMA Parity Errors */
#define ADW_MC_WDTR_DONE		0x0124
#define	ADW_MC_CAM_MODE_MASK		0x015E /* CAM mode TID bitmask. */
#define ADW_MC_ICQ			0x0160
#define ADW_MC_IRQ			0x0164 

/* ADW_SCSI_REQ_Q 'cntl' field values */
#define ADW_QC_DATA_CHECK	0x01 /* Require ADW_QC_DATA_OUT set or clear. */
#define ADW_QC_DATA_OUT		0x02 /* Data out DMA transfer. */
#define ADW_QC_START_MOTOR	0x04 /* Send auto-start motor before request. */
#define ADW_QC_NO_OVERRUN	0x08 /* Don't report overrun. */
#define ADW_QC_FREEZE_TIDQ	0x10 /* Freeze TID queue after request.XXXTBD */

#define ADW_QSC_NO_DISC		0x01 /* Don't allow disconnect for request.  */
#define ADW_QSC_NO_TAGMSG	0x02 /* Don't allow tag queuing for request. */
#define ADW_QSC_NO_SYNC		0x04 /* Don't use Synch. transfer on request.*/
#define ADW_QSC_NO_WIDE		0x08 /* Don't use Wide transfer on request.  */
#define ADW_QSC_REDO_DTR	0x10 /* Renegotiate WDTR/SDTR before request.*/
/*
 * Note: If a Tag Message is to be sent and neither ADW_QSC_HEAD_TAG or
 * ADW_QSC_ORDERED_TAG is set, then a Simple Tag Message (0x20) is used.
 */
#define ADW_QSC_HEAD_TAG	0x40 /* Use Head Tag Message (0x21). */
#define ADW_QSC_ORDERED_TAG	0x80 /* Use Ordered Tag Message (0x22). */

struct adw_carrier
{
	u_int32_t carr_offset;	/* Carrier byte offset into our array */
	u_int32_t carr_ba;	/* Carrier Bus Address */
	u_int32_t areq_ba;	/* SCSI Req Queue Bus Address */
	u_int32_t next_ba;
#define		ADW_RQ_DONE		0x00000001
#define		ADW_CQ_STOPPER		0x00000000
#define		ADW_NEXT_BA_MASK	0xFFFFFFF0
};

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
	ADW_IDLE_CMD_SCSI_RESET_START	= 0x0020,
	ADW_IDLE_CMD_SCSI_RESET_END	= 0x0040,
	ADW_IDLE_CMD_SCSIREQ		= 0x0080
} adw_idle_cmd_t;

typedef enum {
	ADW_IDLE_CMD_FAILURE		= 0x0000,
	ADW_IDLE_CMD_SUCCESS		= 0x0001
} adw_idle_cmd_status_t;


#endif /* _ADMCODE_H_ */
