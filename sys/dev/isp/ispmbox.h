/* $FreeBSD: src/sys/dev/isp/ispmbox.h,v 1.59 2007/03/10 02:39:54 mjacob Exp $ */
/*-
 *  Copyright (c) 1997-2007 by Matthew Jacob
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
 */
/*
 * Mailbox and Queue Entry Definitions for for Qlogic ISP SCSI adapters.
 */
#ifndef	_ISPMBOX_H
#define	_ISPMBOX_H

/*
 * Mailbox Command Opcodes
 */
#define MBOX_NO_OP			0x0000
#define MBOX_LOAD_RAM			0x0001
#define MBOX_EXEC_FIRMWARE		0x0002
#define MBOX_DUMP_RAM			0x0003
#define MBOX_WRITE_RAM_WORD		0x0004
#define MBOX_READ_RAM_WORD		0x0005
#define MBOX_MAILBOX_REG_TEST		0x0006
#define MBOX_VERIFY_CHECKSUM		0x0007
#define MBOX_ABOUT_FIRMWARE		0x0008
#define	MBOX_LOAD_RISC_RAM_2100		0x0009
					/*   a */
#define	MBOX_LOAD_RISC_RAM		0x000b
					/*   c */
#define MBOX_WRITE_RAM_WORD_EXTENDED	0x000d
#define MBOX_CHECK_FIRMWARE		0x000e
#define	MBOX_READ_RAM_WORD_EXTENDED	0x000f
#define MBOX_INIT_REQ_QUEUE		0x0010
#define MBOX_INIT_RES_QUEUE		0x0011
#define MBOX_EXECUTE_IOCB		0x0012
#define MBOX_WAKE_UP			0x0013
#define MBOX_STOP_FIRMWARE		0x0014
#define MBOX_ABORT			0x0015
#define MBOX_ABORT_DEVICE		0x0016
#define MBOX_ABORT_TARGET		0x0017
#define MBOX_BUS_RESET			0x0018
#define MBOX_STOP_QUEUE			0x0019
#define MBOX_START_QUEUE		0x001a
#define MBOX_SINGLE_STEP_QUEUE		0x001b
#define MBOX_ABORT_QUEUE		0x001c
#define MBOX_GET_DEV_QUEUE_STATUS	0x001d
					/*  1e */
#define MBOX_GET_FIRMWARE_STATUS	0x001f
#define MBOX_GET_INIT_SCSI_ID		0x0020
#define MBOX_GET_SELECT_TIMEOUT		0x0021
#define MBOX_GET_RETRY_COUNT		0x0022
#define MBOX_GET_TAG_AGE_LIMIT		0x0023
#define MBOX_GET_CLOCK_RATE		0x0024
#define MBOX_GET_ACT_NEG_STATE		0x0025
#define MBOX_GET_ASYNC_DATA_SETUP_TIME	0x0026
#define MBOX_GET_SBUS_PARAMS		0x0027
#define		MBOX_GET_PCI_PARAMS	MBOX_GET_SBUS_PARAMS
#define MBOX_GET_TARGET_PARAMS		0x0028
#define MBOX_GET_DEV_QUEUE_PARAMS	0x0029
#define	MBOX_GET_RESET_DELAY_PARAMS	0x002a
					/*  2b */
					/*  2c */
					/*  2d */
					/*  2e */
					/*  2f */
#define MBOX_SET_INIT_SCSI_ID		0x0030
#define MBOX_SET_SELECT_TIMEOUT		0x0031
#define MBOX_SET_RETRY_COUNT		0x0032
#define MBOX_SET_TAG_AGE_LIMIT		0x0033
#define MBOX_SET_CLOCK_RATE		0x0034
#define MBOX_SET_ACT_NEG_STATE		0x0035
#define MBOX_SET_ASYNC_DATA_SETUP_TIME	0x0036
#define MBOX_SET_SBUS_CONTROL_PARAMS	0x0037
#define		MBOX_SET_PCI_PARAMETERS	0x0037
#define MBOX_SET_TARGET_PARAMS		0x0038
#define MBOX_SET_DEV_QUEUE_PARAMS	0x0039
#define	MBOX_SET_RESET_DELAY_PARAMS	0x003a
					/*  3b */
					/*  3c */
					/*  3d */
					/*  3e */
					/*  3f */
#define	MBOX_RETURN_BIOS_BLOCK_ADDR	0x0040
#define	MBOX_WRITE_FOUR_RAM_WORDS	0x0041
#define	MBOX_EXEC_BIOS_IOCB		0x0042
#define	MBOX_SET_FW_FEATURES		0x004a
#define	MBOX_GET_FW_FEATURES		0x004b
#define		FW_FEATURE_FAST_POST	0x1
#define		FW_FEATURE_LVD_NOTIFY	0x2
#define		FW_FEATURE_RIO_32BIT	0x4
#define		FW_FEATURE_RIO_16BIT	0x8

#define	MBOX_INIT_REQ_QUEUE_A64		0x0052
#define	MBOX_INIT_RES_QUEUE_A64		0x0053

#define	MBOX_ENABLE_TARGET_MODE		0x0055
#define		ENABLE_TARGET_FLAG	0x8000
#define		ENABLE_TQING_FLAG	0x0004
#define		ENABLE_MANDATORY_DISC	0x0002
#define	MBOX_GET_TARGET_STATUS		0x0056

/* These are for the ISP2X00 FC cards */
#define	MBOX_GET_LOOP_ID		0x0020
#define	MBOX_GET_FIRMWARE_OPTIONS	0x0028
#define	MBOX_SET_FIRMWARE_OPTIONS	0x0038
#define	MBOX_GET_RESOURCE_COUNT		0x0042
#define	MBOX_REQUEST_OFFLINE_MODE	0x0043
#define	MBOX_ENHANCED_GET_PDB		0x0047
#define	MBOX_EXEC_COMMAND_IOCB_A64	0x0054
#define	MBOX_INIT_FIRMWARE		0x0060
#define	MBOX_GET_INIT_CONTROL_BLOCK	0x0061
#define	MBOX_INIT_LIP			0x0062
#define	MBOX_GET_FC_AL_POSITION_MAP	0x0063
#define	MBOX_GET_PORT_DB		0x0064
#define	MBOX_CLEAR_ACA			0x0065
#define	MBOX_TARGET_RESET		0x0066
#define	MBOX_CLEAR_TASK_SET		0x0067
#define	MBOX_ABORT_TASK_SET		0x0068
#define	MBOX_GET_FW_STATE		0x0069
#define	MBOX_GET_PORT_NAME		0x006A
#define	MBOX_GET_LINK_STATUS		0x006B
#define	MBOX_INIT_LIP_RESET		0x006C
#define	MBOX_SEND_SNS			0x006E
#define	MBOX_FABRIC_LOGIN		0x006F
#define	MBOX_SEND_CHANGE_REQUEST	0x0070
#define	MBOX_FABRIC_LOGOUT		0x0071
#define	MBOX_INIT_LIP_LOGIN		0x0072
#define	MBOX_LUN_RESET			0x007E

#define	MBOX_DRIVER_HEARTBEAT		0x005B
#define	MBOX_FW_HEARTBEAT		0x005C

#define	MBOX_GET_SET_DATA_RATE		0x005D	/* 24XX/23XX only */
#define		MBGSD_GET_RATE		0
#define		MBGSD_SET_RATE		1
#define		MBGSD_SET_RATE_NOW	2	/* 24XX only */
#define		MBGSD_ONEGB	0
#define		MBGSD_TWOGB	1
#define		MBGSD_AUTO	2
#define		MBGSD_FOURGB	3		/* 24XX only */


#define	ISP2100_SET_PCI_PARAM		0x00ff

#define	MBOX_BUSY			0x04

/*
 * Mailbox Command Complete Status Codes
 */
#define	MBOX_COMMAND_COMPLETE		0x4000
#define	MBOX_INVALID_COMMAND		0x4001
#define	MBOX_HOST_INTERFACE_ERROR	0x4002
#define	MBOX_TEST_FAILED		0x4003
#define	MBOX_COMMAND_ERROR		0x4005
#define	MBOX_COMMAND_PARAM_ERROR	0x4006
#define	MBOX_PORT_ID_USED		0x4007
#define	MBOX_LOOP_ID_USED		0x4008
#define	MBOX_ALL_IDS_USED		0x4009
#define	MBOX_NOT_LOGGED_IN		0x400A
/* pseudo mailbox completion codes */
#define	MBOX_REGS_BUSY			0x6000	/* registers in use */
#define	MBOX_TIMEOUT			0x6001	/* command timed out */

#define	MBLOGALL			0x000f
#define	MBLOGNONE			0x0000
#define	MBLOGMASK(x)			((x) & 0xf)

/*
 * Asynchronous event status codes
 */
#define	ASYNC_BUS_RESET			0x8001
#define	ASYNC_SYSTEM_ERROR		0x8002
#define	ASYNC_RQS_XFER_ERR		0x8003
#define	ASYNC_RSP_XFER_ERR		0x8004
#define	ASYNC_QWAKEUP			0x8005
#define	ASYNC_TIMEOUT_RESET		0x8006
#define	ASYNC_DEVICE_RESET		0x8007
#define	ASYNC_EXTMSG_UNDERRUN		0x800A
#define	ASYNC_SCAM_INT			0x800B
#define	ASYNC_HUNG_SCSI			0x800C
#define	ASYNC_KILLED_BUS		0x800D
#define	ASYNC_BUS_TRANSIT		0x800E	/* LVD -> HVD, eg. */
#define	ASYNC_LIP_OCCURRED		0x8010
#define	ASYNC_LOOP_UP			0x8011
#define	ASYNC_LOOP_DOWN			0x8012
#define	ASYNC_LOOP_RESET		0x8013
#define	ASYNC_PDB_CHANGED		0x8014
#define	ASYNC_CHANGE_NOTIFY		0x8015
#define	ASYNC_LIP_F8			0x8016
#define	ASYNC_LIP_ERROR			0x8017
#define	ASYNC_SECURITY_UPDATE		0x801B
#define	ASYNC_CMD_CMPLT			0x8020
#define	ASYNC_CTIO_DONE			0x8021
#define	ASYNC_IP_XMIT_DONE		0x8022
#define	ASYNC_IP_RECV_DONE		0x8023
#define	ASYNC_IP_BROADCAST		0x8024
#define	ASYNC_IP_RCVQ_LOW		0x8025
#define	ASYNC_IP_RCVQ_EMPTY		0x8026
#define	ASYNC_IP_RECV_DONE_ALIGNED	0x8027
#define	ASYNC_PTPMODE			0x8030
#define	ASYNC_RIO1			0x8031
#define	ASYNC_RIO2			0x8032
#define	ASYNC_RIO3			0x8033
#define	ASYNC_RIO4			0x8034
#define	ASYNC_RIO5			0x8035
#define	ASYNC_CONNMODE			0x8036
#define		ISP_CONN_LOOP		1
#define		ISP_CONN_PTP		2
#define		ISP_CONN_BADLIP		3
#define		ISP_CONN_FATAL		4
#define		ISP_CONN_LOOPBACK	5
#define	ASYNC_RIO_RESP			0x8040
#define	ASYNC_RIO_COMP			0x8042
#define	ASYNC_RCV_ERR			0x8048

/*
 * 2.01.31 2200 Only. Need Bit 13 in Mailbox 1 for Set Firmware Options
 * mailbox command to enable this.
 */
#define	ASYNC_QFULL_SENT		0x8049

/*
 * 24XX only
 */
#define	ASYNC_RJT_SENT			0x8049

/*
 * All IOCB Queue entries are this size
 */
#define	QENTRY_LEN			64

/*
 * Command Structure Definitions
 */

typedef struct {
	uint32_t	ds_base;
	uint32_t	ds_count;
} ispds_t;

typedef struct {
	uint32_t	ds_base;
	uint32_t	ds_basehi;
	uint32_t	ds_count;
} ispds64_t;

#define	DSTYPE_32BIT	0
#define	DSTYPE_64BIT	1
typedef struct {
	uint16_t	ds_type;	/* 0-> ispds_t, 1-> ispds64_t */
	uint32_t	ds_segment;	/* unused */
	uint32_t	ds_base;	/* 32 bit address of DSD list */
} ispdslist_t;


/*
 * These elements get swizzled around for SBus instances.
 */
#define	ISP_SWAP8(a, b)	{		\
	uint8_t tmp;			\
	tmp = a;			\
	a = b;				\
	b = tmp;			\
}
typedef struct {
	uint8_t		rqs_entry_type;
	uint8_t		rqs_entry_count;
	uint8_t		rqs_seqno;
	uint8_t		rqs_flags;
} isphdr_t;

/* RQS Flag definitions */
#define	RQSFLAG_CONTINUATION	0x01
#define	RQSFLAG_FULL		0x02
#define	RQSFLAG_BADHEADER	0x04
#define	RQSFLAG_BADPACKET	0x08
#define	RQSFLAG_MASK		0x0f

/* RQS entry_type definitions */
#define	RQSTYPE_REQUEST		0x01
#define	RQSTYPE_DATASEG		0x02
#define	RQSTYPE_RESPONSE	0x03
#define	RQSTYPE_MARKER		0x04
#define	RQSTYPE_CMDONLY		0x05
#define	RQSTYPE_ATIO		0x06	/* Target Mode */
#define	RQSTYPE_CTIO		0x07	/* Target Mode */
#define	RQSTYPE_SCAM		0x08
#define	RQSTYPE_A64		0x09
#define	RQSTYPE_A64_CONT	0x0a
#define	RQSTYPE_ENABLE_LUN	0x0b	/* Target Mode */
#define	RQSTYPE_MODIFY_LUN	0x0c	/* Target Mode */
#define	RQSTYPE_NOTIFY		0x0d	/* Target Mode */
#define	RQSTYPE_NOTIFY_ACK	0x0e	/* Target Mode */
#define	RQSTYPE_CTIO1		0x0f	/* Target Mode */
#define	RQSTYPE_STATUS_CONT	0x10
#define	RQSTYPE_T2RQS		0x11
#define	RQSTYPE_CTIO7		0x12
#define	RQSTYPE_IP_XMIT		0x13
#define	RQSTYPE_TSK_MGMT	0x14
#define	RQSTYPE_T4RQS		0x15
#define	RQSTYPE_ATIO2		0x16	/* Target Mode */
#define	RQSTYPE_CTIO2		0x17	/* Target Mode */
#define	RQSTYPE_T7RQS		0x18
#define	RQSTYPE_T3RQS		0x19
#define	RQSTYPE_IP_XMIT_64	0x1b
#define	RQSTYPE_CTIO4		0x1e	/* Target Mode */
#define	RQSTYPE_CTIO3		0x1f	/* Target Mode */
#define	RQSTYPE_RIO1		0x21
#define	RQSTYPE_RIO2		0x22
#define	RQSTYPE_IP_RECV		0x23
#define	RQSTYPE_IP_RECV_CONT	0x24
#define	RQSTYPE_CT_PASSTHRU	0x29
#define	RQSTYPE_MS_PASSTHRU	0x29
#define	RQSTYPE_ABORT_IO	0x33
#define	RQSTYPE_T6RQS		0x48
#define	RQSTYPE_LOGIN		0x52
#define	RQSTYPE_ABTS_RCVD	0x54	/* 24XX only */
#define	RQSTYPE_ABTS_RSP	0x55	/* 24XX only */


#define	ISP_RQDSEG	4
typedef struct {
	isphdr_t	req_header;
	uint32_t	req_handle;
	uint8_t		req_lun_trn;
	uint8_t		req_target;
	uint16_t	req_cdblen;
	uint16_t	req_flags;
	uint16_t	req_reserved;
	uint16_t	req_time;
	uint16_t	req_seg_count;
	uint8_t		req_cdb[12];
	ispds_t		req_dataseg[ISP_RQDSEG];
} ispreq_t;
#define	ISP_RQDSEG_A64	2

typedef struct {
	isphdr_t	mrk_header;
	uint32_t	mrk_handle;
	uint8_t		mrk_reserved0;
	uint8_t		mrk_target;
	uint16_t	mrk_modifier;
	uint16_t	mrk_flags;
	uint16_t	mrk_lun;
	uint8_t		mrk_reserved1[48];
} isp_marker_t;
	
typedef struct {
	isphdr_t	mrk_header;
	uint32_t	mrk_handle;
	uint16_t	mrk_nphdl;
	uint8_t		mrk_modifier;
	uint8_t		mrk_reserved0;
	uint8_t		mrk_reserved1;
	uint8_t		mrk_vphdl;
	uint16_t	mrk_reserved2;
	uint8_t		mrk_lun[8];
	uint8_t		mrk_reserved3[40];
} isp_marker_24xx_t;
	

#define SYNC_DEVICE	0
#define SYNC_TARGET	1
#define SYNC_ALL	2
#define SYNC_LIP	3

#define	ISP_RQDSEG_T2		3
typedef struct {
	isphdr_t	req_header;
	uint32_t	req_handle;
	uint8_t		req_lun_trn;
	uint8_t		req_target;
	uint16_t	req_scclun;
	uint16_t	req_flags;
	uint16_t	req_reserved;
	uint16_t	req_time;
	uint16_t	req_seg_count;
	uint8_t		req_cdb[16];
	uint32_t	req_totalcnt;
	ispds_t		req_dataseg[ISP_RQDSEG_T2];
} ispreqt2_t;

typedef struct {
	isphdr_t	req_header;
	uint32_t	req_handle;
	uint16_t	req_target;
	uint16_t	req_scclun;
	uint16_t	req_flags;
	uint16_t	req_reserved;
	uint16_t	req_time;
	uint16_t	req_seg_count;
	uint8_t		req_cdb[16];
	uint32_t	req_totalcnt;
	ispds_t		req_dataseg[ISP_RQDSEG_T2];
} ispreqt2e_t;

#define	ISP_RQDSEG_T3		2
typedef struct {
	isphdr_t	req_header;
	uint32_t	req_handle;
	uint8_t		req_lun_trn;
	uint8_t		req_target;
	uint16_t	req_scclun;
	uint16_t	req_flags;
	uint16_t	req_reserved;
	uint16_t	req_time;
	uint16_t	req_seg_count;
	uint8_t		req_cdb[16];
	uint32_t	req_totalcnt;
	ispds64_t	req_dataseg[ISP_RQDSEG_T3];
} ispreqt3_t;
#define	ispreq64_t	ispreqt3_t	/* same as.... */

typedef struct {
	isphdr_t	req_header;
	uint32_t	req_handle;
	uint16_t	req_target;
	uint16_t	req_scclun;
	uint16_t	req_flags;
	uint16_t	req_reserved;
	uint16_t	req_time;
	uint16_t	req_seg_count;
	uint8_t		req_cdb[16];
	uint32_t	req_totalcnt;
	ispds64_t	req_dataseg[ISP_RQDSEG_T3];
} ispreqt3e_t;

/* req_flag values */
#define	REQFLAG_NODISCON	0x0001
#define	REQFLAG_HTAG		0x0002
#define	REQFLAG_OTAG		0x0004
#define	REQFLAG_STAG		0x0008
#define	REQFLAG_TARGET_RTN	0x0010

#define	REQFLAG_NODATA		0x0000
#define	REQFLAG_DATA_IN		0x0020
#define	REQFLAG_DATA_OUT	0x0040
#define	REQFLAG_DATA_UNKNOWN	0x0060

#define	REQFLAG_DISARQ		0x0100
#define	REQFLAG_FRC_ASYNC	0x0200
#define	REQFLAG_FRC_SYNC	0x0400
#define	REQFLAG_FRC_WIDE	0x0800
#define	REQFLAG_NOPARITY	0x1000
#define	REQFLAG_STOPQ		0x2000
#define	REQFLAG_XTRASNS		0x4000
#define	REQFLAG_PRIORITY	0x8000

typedef struct {
	isphdr_t	req_header;
	uint32_t	req_handle;
	uint8_t		req_lun_trn;
	uint8_t		req_target;
	uint16_t	req_cdblen;
	uint16_t	req_flags;
	uint16_t	req_reserved;
	uint16_t	req_time;
	uint16_t	req_seg_count;
	uint8_t		req_cdb[44];
} ispextreq_t;

/* 24XX only */
typedef struct {
	uint16_t	fcd_length;
	uint16_t	fcd_a1500;
	uint16_t	fcd_a3116;
	uint16_t	fcd_a4732;
	uint16_t	fcd_a6348;
} fcp_cmnd_ds_t;

typedef struct {
	isphdr_t	req_header;
	uint32_t	req_handle;
	uint16_t	req_nphdl;
	uint16_t	req_time;
	uint16_t	req_seg_count;
	uint16_t	req_fc_rsp_dsd_length;
	uint8_t		req_lun[8];
	uint16_t	req_flags;
	uint16_t	req_fc_cmnd_dsd_length;
	uint16_t	req_fc_cmnd_dsd_a1500;
	uint16_t	req_fc_cmnd_dsd_a3116;
	uint16_t	req_fc_cmnd_dsd_a4732;
	uint16_t	req_fc_cmnd_dsd_a6348;
	uint16_t	req_fc_rsp_dsd_a1500;
	uint16_t	req_fc_rsp_dsd_a3116;
	uint16_t	req_fc_rsp_dsd_a4732;
	uint16_t	req_fc_rsp_dsd_a6348;
	uint32_t	req_totalcnt;
	uint16_t	req_tidlo;
	uint8_t		req_tidhi;
	uint8_t		req_vpidx;
	ispds64_t	req_dataseg;
} ispreqt6_t;

typedef struct {
	isphdr_t	req_header;
	uint32_t	req_handle;
	uint16_t	req_nphdl;
	uint16_t	req_time;
	uint16_t	req_seg_count;
	uint16_t	req_reserved;
	uint8_t		req_lun[8];
	uint8_t		req_alen_datadir;
	uint8_t		req_task_management;
	uint8_t		req_task_attribute;
	uint8_t		req_crn;
	uint8_t		req_cdb[16];
	uint32_t	req_dl;
	uint16_t	req_tidlo;
	uint8_t		req_tidhi;
	uint8_t		req_vpidx;
	ispds64_t	req_dataseg;
} ispreqt7_t;

/* I/O Abort Structure */
typedef struct {
	isphdr_t	abrt_header;
	uint32_t	abrt_handle;
	uint16_t	abrt_nphdl;
	uint16_t	abrt_options;
	uint32_t	abrt_cmd_handle;
	uint8_t		abrt_reserved[32];
	uint16_t	abrt_tidlo;
	uint8_t		abrt_tidhi;
	uint8_t		abrt_vpidx;
	uint8_t		abrt_reserved1[12];
} isp24xx_abrt_t;
#define	ISP24XX_ABRT_NO_ABTS	0x01	/* don't actually send an ABTS */
#define	ISP24XX_ABRT_ENXIO	0x31	/* in nphdl on return */

#define	ISP_CDSEG	7
typedef struct {
	isphdr_t	req_header;
	uint32_t	req_reserved;
	ispds_t		req_dataseg[ISP_CDSEG];
} ispcontreq_t;

#define	ISP_CDSEG64	5
typedef struct {
	isphdr_t	req_header;
	ispds64_t	req_dataseg[ISP_CDSEG64];
} ispcontreq64_t;

typedef struct {
	isphdr_t	req_header;
	uint32_t	req_handle;
	uint16_t	req_scsi_status;
	uint16_t	req_completion_status;
	uint16_t	req_state_flags;
	uint16_t	req_status_flags;
	uint16_t	req_time;
#define	req_response_len	req_time	/* FC only */
	uint16_t	req_sense_len;
	uint32_t	req_resid;
	uint8_t		req_response[8];	/* FC only */
	uint8_t		req_sense_data[32];
} ispstatusreq_t;

/*
 * Status Continuation
 */
typedef struct {
	isphdr_t	req_header;
	uint8_t		req_sense_data[60];
} ispstatus_cont_t;

/*
 * 24XX Type 0 status
 */
typedef struct {
	isphdr_t	req_header;
	uint32_t	req_handle;
	uint16_t	req_completion_status;
	uint16_t	req_oxid;
	uint32_t	req_resid;
	uint16_t	req_reserved0;
	uint16_t	req_state_flags;
	uint16_t	req_reserved1;
	uint16_t	req_scsi_status;
	uint32_t	req_fcp_residual;
	uint32_t	req_sense_len;
	uint32_t	req_response_len;
	uint8_t		req_rsp_sense[28];
} isp24xx_statusreq_t;

/* 
 * For Qlogic 2X00, the high order byte of SCSI status has
 * additional meaning.
 */
#define	RQCS_RU	0x800	/* Residual Under */
#define	RQCS_RO	0x400	/* Residual Over */
#define	RQCS_RESID	(RQCS_RU|RQCS_RO)
#define	RQCS_SV	0x200	/* Sense Length Valid */
#define	RQCS_RV	0x100	/* FCP Response Length Valid */

/*
 * CT Passthru IOCB
 */
typedef struct {
	isphdr_t	ctp_header;
	uint32_t	ctp_handle;
	uint16_t	ctp_status;
	uint16_t	ctp_nphdl;	/* n-port handle */
	uint16_t	ctp_cmd_cnt;	/* Command DSD count */
	uint16_t	ctp_vpidx;	/* low 8 bits */
	uint16_t	ctp_time;
	uint16_t	ctp_reserved0;
	uint16_t	ctp_rsp_cnt;	/* Response DSD count */
	uint16_t	ctp_reserved1[5];
	uint32_t	ctp_rsp_bcnt;	/* Response byte count */
	uint32_t	ctp_cmd_bcnt;	/* Command byte count */
	ispds64_t	ctp_dataseg[2];
} isp_ct_pt_t;

/*
 * MS Passthru IOCB
 */
typedef struct {
	isphdr_t	ms_header;
	uint32_t	ms_handle;
	uint16_t	ms_nphdl;	/* handle in high byte for !2k f/w */
	uint16_t	ms_status;
	uint16_t	ms_flags;
	uint16_t	ms_reserved1;	/* low 8 bits */
	uint16_t	ms_time;
	uint16_t	ms_cmd_cnt;	/* Command DSD count */
	uint16_t	ms_tot_cnt;	/* Total DSD Count */
	uint8_t		ms_type;	/* MS type */
	uint8_t		ms_r_ctl;	/* R_CTL */
	uint16_t	ms_rxid;	/* RX_ID */
	uint16_t	ms_reserved2;
	uint32_t	ms_handle2;
	uint32_t	ms_rsp_bcnt;	/* Response byte count */
	uint32_t	ms_cmd_bcnt;	/* Command byte count */
	ispds64_t	ms_dataseg[2];
} isp_ms_t;

/* 
 * Completion Status Codes.
 */
#define RQCS_COMPLETE			0x0000
#define RQCS_DMA_ERROR			0x0002
#define RQCS_RESET_OCCURRED		0x0004
#define RQCS_ABORTED			0x0005
#define RQCS_TIMEOUT			0x0006
#define RQCS_DATA_OVERRUN		0x0007
#define RQCS_DATA_UNDERRUN		0x0015
#define	RQCS_QUEUE_FULL			0x001C

/* 1X00 Only Completion Codes */
#define RQCS_INCOMPLETE			0x0001
#define RQCS_TRANSPORT_ERROR		0x0003
#define RQCS_COMMAND_OVERRUN		0x0008
#define RQCS_STATUS_OVERRUN		0x0009
#define RQCS_BAD_MESSAGE		0x000a
#define RQCS_NO_MESSAGE_OUT		0x000b
#define RQCS_EXT_ID_FAILED		0x000c
#define RQCS_IDE_MSG_FAILED		0x000d
#define RQCS_ABORT_MSG_FAILED		0x000e
#define RQCS_REJECT_MSG_FAILED		0x000f
#define RQCS_NOP_MSG_FAILED		0x0010
#define RQCS_PARITY_ERROR_MSG_FAILED	0x0011
#define RQCS_DEVICE_RESET_MSG_FAILED	0x0012
#define RQCS_ID_MSG_FAILED		0x0013
#define RQCS_UNEXP_BUS_FREE		0x0014
#define	RQCS_XACT_ERR1			0x0018
#define	RQCS_XACT_ERR2			0x0019
#define	RQCS_XACT_ERR3			0x001A
#define	RQCS_BAD_ENTRY			0x001B
#define	RQCS_PHASE_SKIPPED		0x001D
#define	RQCS_ARQS_FAILED		0x001E
#define	RQCS_WIDE_FAILED		0x001F
#define	RQCS_SYNCXFER_FAILED		0x0020
#define	RQCS_LVD_BUSERR			0x0021

/* 2X00 Only Completion Codes */
#define	RQCS_PORT_UNAVAILABLE		0x0028
#define	RQCS_PORT_LOGGED_OUT		0x0029
#define	RQCS_PORT_CHANGED		0x002A
#define	RQCS_PORT_BUSY			0x002B

/* 24XX Only Completion Codes */
#define	RQCS_24XX_DRE			0x0011	/* data reassembly error */
#define	RQCS_24XX_TABORT		0x0013	/* aborted by target */
#define	RQCS_24XX_ENOMEM		0x002C	/* f/w resource unavailable */
#define	RQCS_24XX_TMO			0x0030	/* task management overrun */


/*
 * 1X00 specific State Flags 
 */
#define RQSF_GOT_BUS			0x0100
#define RQSF_GOT_TARGET			0x0200
#define RQSF_SENT_CDB			0x0400
#define RQSF_XFRD_DATA			0x0800
#define RQSF_GOT_STATUS			0x1000
#define RQSF_GOT_SENSE			0x2000
#define	RQSF_XFER_COMPLETE		0x4000

/*
 * 2X00 specific State Flags
 * (same as 1X00 except RQSF_GOT_BUS/RQSF_GOT_TARGET are not available)
 */
#define	RQSF_DATA_IN			0x0020
#define	RQSF_DATA_OUT			0x0040
#define	RQSF_STAG			0x0008
#define	RQSF_OTAG			0x0004
#define	RQSF_HTAG			0x0002
/*
 * 1X00 Status Flags
 */
#define RQSTF_DISCONNECT		0x0001
#define RQSTF_SYNCHRONOUS		0x0002
#define RQSTF_PARITY_ERROR		0x0004
#define RQSTF_BUS_RESET			0x0008
#define RQSTF_DEVICE_RESET		0x0010
#define RQSTF_ABORTED			0x0020
#define RQSTF_TIMEOUT			0x0040
#define RQSTF_NEGOTIATION		0x0080

/*
 * 2X00 specific state flags
 */
/* RQSF_SENT_CDB	*/
/* RQSF_XFRD_DATA	*/
/* RQSF_GOT_STATUS	*/
/* RQSF_XFER_COMPLETE	*/

/*
 * 2X00 specific status flags
 */
/* RQSTF_ABORTED */
/* RQSTF_TIMEOUT */
#define	RQSTF_DMA_ERROR			0x0080
#define	RQSTF_LOGOUT			0x2000

/*
 * Miscellaneous
 */
#ifndef	ISP_EXEC_THROTTLE
#define	ISP_EXEC_THROTTLE	16
#endif

/*
 * About Firmware returns an 'attribute' word in mailbox 6.
 * These attributes are for 2200 and 2300.
 */
#define	ISP_FW_ATTR_TMODE	0x01
#define	ISP_FW_ATTR_SCCLUN	0x02
#define	ISP_FW_ATTR_FABRIC	0x04
#define	ISP_FW_ATTR_CLASS2	0x08
#define	ISP_FW_ATTR_FCTAPE	0x10
#define	ISP_FW_ATTR_IP		0x20
#define	ISP_FW_ATTR_VI		0x40
#define	ISP_FW_ATTR_VI_SOLARIS	0x80
#define	ISP_FW_ATTR_2KLOGINS	0x100	/* XXX: just a guess */

/* and these are for the 2400 */
#define	ISP2400_FW_ATTR_CLASS2	(1 << 0)
#define	ISP2400_FW_ATTR_IP	(1 << 1)
#define	ISP2400_FW_ATTR_MULTIID	(1 << 2)
#define	ISP2400_FW_ATTR_SB2	(1 << 3)
#define	ISP2400_FW_ATTR_T10CRC	(1 << 4)
#define	ISP2400_FW_ATTR_VI	(1 << 5)
#define	ISP2400_FW_ATTR_EXPFW	(1 << 13)

/*
 * Reduced Interrupt Operation Response Queue Entreis
 */

typedef struct {
	isphdr_t	req_header;
	uint32_t	req_handles[15];
} isp_rio1_t;

typedef struct {
	isphdr_t	req_header;
	uint16_t	req_handles[30];
} isp_rio2_t;

/*
 * FC (ISP2100/ISP2200/ISP2300/ISP2400) specific data structures
 */

/*
 * Initialization Control Block
 *
 * Version One (prime) format.
 */
typedef struct {
	uint8_t		icb_version;
	uint8_t		icb_reserved0;
	uint16_t	icb_fwoptions;
	uint16_t	icb_maxfrmlen;
	uint16_t	icb_maxalloc;
	uint16_t	icb_execthrottle;
	uint8_t		icb_retry_count;
	uint8_t		icb_retry_delay;
	uint8_t		icb_portname[8];
	uint16_t	icb_hardaddr;
	uint8_t		icb_iqdevtype;
	uint8_t		icb_logintime;
	uint8_t		icb_nodename[8];
	uint16_t	icb_rqstout;
	uint16_t	icb_rspnsin;
	uint16_t	icb_rqstqlen;
	uint16_t	icb_rsltqlen;
	uint16_t	icb_rqstaddr[4];
	uint16_t	icb_respaddr[4];
	uint16_t	icb_lunenables;
	uint8_t		icb_ccnt;
	uint8_t		icb_icnt;
	uint16_t	icb_lunetimeout;
	uint16_t	icb_reserved1;
	uint16_t	icb_xfwoptions;
	uint8_t		icb_racctimer;
	uint8_t		icb_idelaytimer;
	uint16_t	icb_zfwoptions;
	uint16_t	icb_reserved2[13];
} isp_icb_t;

#define	ICB_VERSION1	1

#define	ICBOPT_EXTENDED		0x8000
#define	ICBOPT_BOTH_WWNS	0x4000
#define	ICBOPT_FULL_LOGIN	0x2000
#define	ICBOPT_STOP_ON_QFULL	0x1000	/* 2200/2100 only */
#define	ICBOPT_PREVLOOP		0x0800
#define	ICBOPT_SRCHDOWN		0x0400
#define	ICBOPT_NOLIP		0x0200
#define	ICBOPT_PDBCHANGE_AE	0x0100
#define	ICBOPT_INI_TGTTYPE	0x0080
#define	ICBOPT_INI_ADISC	0x0040
#define	ICBOPT_INI_DISABLE	0x0020
#define	ICBOPT_TGT_ENABLE	0x0010
#define	ICBOPT_FAST_POST	0x0008
#define	ICBOPT_FULL_DUPLEX	0x0004
#define	ICBOPT_FAIRNESS		0x0002
#define	ICBOPT_HARD_ADDRESS	0x0001

#define	ICBXOPT_NO_LOGOUT	0x8000	/* no logout on link failure */
#define	ICBXOPT_FCTAPE_CCQ	0x4000	/* FC-Tape Command Queueing */
#define	ICBXOPT_FCTAPE_CONFIRM	0x2000
#define	ICBXOPT_FCTAPE		0x1000
#define	ICBXOPT_CLASS2_ACK0	0x0200
#define	ICBXOPT_CLASS2		0x0100
#define	ICBXOPT_NO_PLAY		0x0080	/* don't play if can't get hard addr */
#define	ICBXOPT_TOPO_MASK	0x0070
#define	ICBXOPT_LOOP_ONLY	0x0000
#define	ICBXOPT_PTP_ONLY	0x0010
#define	ICBXOPT_LOOP_2_PTP	0x0020
#define	ICBXOPT_PTP_2_LOOP	0x0030
/*
 * The lower 4 bits of the xfwoptions field are the OPERATION MODE bits.
 * RIO is not defined for the 23XX cards (just 2200)
 */
#define	ICBXOPT_RIO_OFF		0
#define	ICBXOPT_RIO_16BIT	1
#define	ICBXOPT_RIO_32BIT	2
#define	ICBXOPT_RIO_16BIT_IOCB	3
#define	ICBXOPT_RIO_32BIT_IOCB	4
#define	ICBXOPT_ZIO		5	
#define	ICBXOPT_TIMER_MASK	0x7

#define	ICBZOPT_RATE_MASK	0xC000
#define	ICBZOPT_RATE_ONEGB	0x0000
#define	ICBZOPT_RATE_AUTO	0x8000
#define	ICBZOPT_RATE_TWOGB	0x4000
#define	ICBZOPT_50_OHM		0x2000
#define	ICBZOPT_ENA_OOF		0x0040	/* out of order frame handling */
#define	ICBZOPT_RSPSZ_MASK	0x0030
#define	ICBZOPT_RSPSZ_24	0x0000
#define	ICBZOPT_RSPSZ_12	0x0010
#define	ICBZOPT_RSPSZ_24A	0x0020
#define	ICBZOPT_RSPSZ_32	0x0030
#define	ICBZOPT_SOFTID		0x0002
#define	ICBZOPT_ENA_RDXFR_RDY	0x0001

/* 2400 F/W options */
#define	ICB2400_OPT1_BOTH_WWNS		0x00004000
#define	ICB2400_OPT1_FULL_LOGIN		0x00002000
#define	ICB2400_OPT1_PREVLOOP		0x00000800
#define	ICB2400_OPT1_SRCHDOWN		0x00000400
#define	ICB2400_OPT1_NOLIP		0x00000200
#define	ICB2400_OPT1_INI_DISABLE	0x00000020
#define	ICB2400_OPT1_TGT_ENABLE		0x00000010
#define	ICB2400_OPT1_FULL_DUPLEX	0x00000004
#define	ICB2400_OPT1_FAIRNESS		0x00000002
#define	ICB2400_OPT1_HARD_ADDRESS	0x00000001

#define	ICB2400_OPT2_FCTAPE		0x00001000
#define	ICB2400_OPT2_CLASS2_ACK0	0x00000200
#define	ICB2400_OPT2_CLASS2		0x00000100
#define	ICB2400_OPT2_NO_PLAY		0x00000080
#define	ICB2400_OPT2_TOPO_MASK		0x00000070
#define	ICB2400_OPT2_LOOP_ONLY		0x00000000
#define	ICB2400_OPT2_PTP_ONLY		0x00000010
#define	ICB2400_OPT2_LOOP_2_PTP		0x00000020
#define	ICB2400_OPT2_PTP_2_LOOP		0x00000030
#define	ICB2400_OPT2_TIMER_MASK		0x00000007
#define	ICB2400_OPT2_ZIO		0x00000005
#define	ICB2400_OPT2_ZIO1		0x00000006

#define	ICB2400_OPT3_75_OHM		0x00010000
#define	ICB2400_OPT3_RATE_MASK		0x0000E000
#define	ICB2400_OPT3_RATE_ONEGB		0x00000000
#define	ICB2400_OPT3_RATE_TWOGB		0x00002000
#define ICB2400_OPT3_RATE_AUTO		0x00004000
#define	ICB2400_OPT3_RATE_FOURGB	0x00006000
#define	ICB2400_OPT3_ENA_OOF_XFRDY	0x00000200
#define	ICB2400_OPT3_NO_LOCAL_PLOGI	0x00000080
#define	ICB2400_OPT3_ENA_OOF		0x00000040
/* note that a response size flag of zero is reserved! */
#define	ICB2400_OPT3_RSPSZ_MASK		0x00000030
#define	ICB2400_OPT3_RSPSZ_12		0x00000010
#define	ICB2400_OPT3_RSPSZ_24		0x00000020
#define	ICB2400_OPT3_RSPSZ_32		0x00000030
#define	ICB2400_OPT3_SOFTID		0x00000002

#define	ICB_MIN_FRMLEN		256
#define	ICB_MAX_FRMLEN		2112
#define	ICB_DFLT_FRMLEN		1024
#define	ICB_DFLT_ALLOC		256
#define	ICB_DFLT_THROTTLE	16
#define	ICB_DFLT_RDELAY		5
#define	ICB_DFLT_RCOUNT		3

#define	ICB_LOGIN_TOV		30
#define	ICB_LUN_ENABLE_TOV	180


/*
 * And somebody at QLogic had a great idea that you could just change
 * the structure *and* keep the version number the same as the other cards.
 */
typedef struct {
	uint16_t	icb_version;
	uint16_t	icb_reserved0;
	uint16_t	icb_maxfrmlen;
	uint16_t	icb_execthrottle;
	uint16_t	icb_xchgcnt;
	uint16_t	icb_hardaddr;
	uint8_t		icb_portname[8];
	uint8_t		icb_nodename[8];
	uint16_t	icb_rspnsin;
	uint16_t	icb_rqstout;
	uint16_t	icb_retry_count;
	uint16_t	icb_priout;
	uint16_t	icb_rsltqlen;
	uint16_t	icb_rqstqlen;
	uint16_t	icb_ldn_nols;
	uint16_t	icb_prqstqlen;
	uint16_t	icb_rqstaddr[4];
	uint16_t	icb_respaddr[4];
	uint16_t	icb_priaddr[4];	
	uint16_t	icb_reserved1[4];
	uint16_t	icb_atio_in;
	uint16_t	icb_atioqlen;
	uint16_t	icb_atioqaddr[4];
	uint16_t	icb_idelaytimer;
	uint16_t	icb_logintime;
	uint32_t	icb_fwoptions1;
	uint32_t	icb_fwoptions2;
	uint32_t	icb_fwoptions3;
	uint16_t	icb_reserved2[12];
} isp_icb_2400_t;

#define	RQRSP_ADDR0015	0
#define	RQRSP_ADDR1631	1
#define	RQRSP_ADDR3247	2
#define	RQRSP_ADDR4863	3


#define	ICB_NNM0	7
#define	ICB_NNM1	6
#define	ICB_NNM2	5
#define	ICB_NNM3	4
#define	ICB_NNM4	3
#define	ICB_NNM5	2
#define	ICB_NNM6	1
#define	ICB_NNM7	0

#define	MAKE_NODE_NAME_FROM_WWN(array, wwn)	\
	array[ICB_NNM0] = (uint8_t) ((wwn >>  0) & 0xff), \
	array[ICB_NNM1] = (uint8_t) ((wwn >>  8) & 0xff), \
	array[ICB_NNM2] = (uint8_t) ((wwn >> 16) & 0xff), \
	array[ICB_NNM3] = (uint8_t) ((wwn >> 24) & 0xff), \
	array[ICB_NNM4] = (uint8_t) ((wwn >> 32) & 0xff), \
	array[ICB_NNM5] = (uint8_t) ((wwn >> 40) & 0xff), \
	array[ICB_NNM6] = (uint8_t) ((wwn >> 48) & 0xff), \
	array[ICB_NNM7] = (uint8_t) ((wwn >> 56) & 0xff)

#define	MAKE_WWN_FROM_NODE_NAME(wwn, array)	\
	wwn =	((uint64_t) array[ICB_NNM0]) | \
		((uint64_t) array[ICB_NNM1] <<  8) | \
		((uint64_t) array[ICB_NNM2] << 16) | \
		((uint64_t) array[ICB_NNM3] << 24) | \
		((uint64_t) array[ICB_NNM4] << 32) | \
		((uint64_t) array[ICB_NNM5] << 40) | \
		((uint64_t) array[ICB_NNM6] << 48) | \
		((uint64_t) array[ICB_NNM7] << 56)

/*
 * Port Data Base Element
 */

typedef struct {
	uint16_t	pdb_options;
	uint8_t		pdb_mstate;
	uint8_t		pdb_sstate;
	uint8_t		pdb_hardaddr_bits[4];
	uint8_t		pdb_portid_bits[4];
	uint8_t		pdb_nodename[8];
	uint8_t		pdb_portname[8];
	uint16_t	pdb_execthrottle;
	uint16_t	pdb_exec_count;
	uint8_t		pdb_retry_count;
	uint8_t		pdb_retry_delay;
	uint16_t	pdb_resalloc;
	uint16_t	pdb_curalloc;
	uint16_t	pdb_qhead;
	uint16_t	pdb_qtail;
	uint16_t	pdb_tl_next;
	uint16_t	pdb_tl_last;
	uint16_t	pdb_features;	/* PLOGI, Common Service */
	uint16_t	pdb_pconcurrnt;	/* PLOGI, Common Service */
	uint16_t	pdb_roi;	/* PLOGI, Common Service */
	uint8_t		pdb_target;
	uint8_t		pdb_initiator;	/* PLOGI, Class 3 Control Flags */
	uint16_t	pdb_rdsiz;	/* PLOGI, Class 3 */
	uint16_t	pdb_ncseq;	/* PLOGI, Class 3 */
	uint16_t	pdb_noseq;	/* PLOGI, Class 3 */
	uint16_t	pdb_labrtflg;
	uint16_t	pdb_lstopflg;
	uint16_t	pdb_sqhead;
	uint16_t	pdb_sqtail;
	uint16_t	pdb_ptimer;
	uint16_t	pdb_nxt_seqid;
	uint16_t	pdb_fcount;
	uint16_t	pdb_prli_len;
	uint16_t	pdb_prli_svc0;
	uint16_t	pdb_prli_svc3;
	uint16_t	pdb_loopid;
	uint16_t	pdb_il_ptr;
	uint16_t	pdb_sl_ptr;
} isp_pdb_21xx_t;

#define	PDB_OPTIONS_XMITTING	(1<<11)
#define	PDB_OPTIONS_LNKXMIT	(1<<10)
#define	PDB_OPTIONS_ABORTED	(1<<9)
#define	PDB_OPTIONS_ADISC	(1<<1)

#define	PDB_STATE_DISCOVERY	0
#define	PDB_STATE_WDISC_ACK	1
#define	PDB_STATE_PLOGI		2
#define	PDB_STATE_PLOGI_ACK	3
#define	PDB_STATE_PRLI		4
#define	PDB_STATE_PRLI_ACK	5
#define	PDB_STATE_LOGGED_IN	6
#define	PDB_STATE_PORT_UNAVAIL	7
#define	PDB_STATE_PRLO		8
#define	PDB_STATE_PRLO_ACK	9
#define	PDB_STATE_PLOGO		10
#define	PDB_STATE_PLOG_ACK	11

#define		SVC3_TGT_ROLE		0x10
#define 	SVC3_INI_ROLE		0x20
#define			SVC3_ROLE_MASK	0x30
#define			SVC3_ROLE_SHIFT	4

#define	BITS2WORD(x)		((x)[0] << 16 | (x)[3] << 8 | (x)[2])
#define	BITS2WORD_24XX(x)	((x)[0] << 16 | (x)[1] << 8 | (x)[2])

/*
 * Port Data Base Element- 24XX cards
 */
typedef struct {
	uint16_t	pdb_flags;
	uint8_t		pdb_curstate;
	uint8_t		pdb_laststate;
	uint8_t		pdb_hardaddr_bits[4];
	uint8_t		pdb_portid_bits[4];
#define		pdb_nxt_seqid_2400	pdb_portid_bits[3]
	uint16_t	pdb_retry_timer;
	uint16_t	pdb_handle;
	uint16_t	pdb_rcv_dsize;
	uint16_t	pdb_reserved0;
	uint16_t	pdb_prli_svc0;
	uint16_t	pdb_prli_svc3;
	uint8_t		pdb_portname[8];
	uint8_t		pdb_nodename[8];
	uint8_t		pdb_reserved1[24];
} isp_pdb_24xx_t;

#define	PDB2400_TID_SUPPORTED	0x4000
#define	PDB2400_FC_TAPE		0x0080
#define	PDB2400_CLASS2_ACK0	0x0040
#define	PDB2400_FCP_CONF	0x0020
#define	PDB2400_CLASS2		0x0010
#define	PDB2400_ADDR_VALID	0x0002

/*
 * Common elements from the above two structures that are actually useful to us.
 */
typedef struct {
	uint16_t	handle;
	uint16_t	reserved;
	uint32_t	s3_role	: 8,
			portid	: 24;
	uint8_t		portname[8];
	uint8_t		nodename[8];
} isp_pdb_t;

/*
 * Genericized Port Login/Logout software structure
 */
typedef struct {
	uint16_t	handle;
	uint32_t
		flags	: 8,
		portid	: 24;
} isp_plcmd_t;
/* the flags to use are those for PLOGX_FLG_* below */

/*
 * ISP24XX- Login/Logout Port IOCB
 */
typedef struct {
	isphdr_t	plogx_header;
	uint32_t	plogx_handle;
	uint16_t	plogx_status;
	uint16_t	plogx_nphdl;
	uint16_t	plogx_flags;
	uint16_t	plogx_vphdl;		/* low 8 bits */
	uint16_t	plogx_portlo;		/* low 16 bits */
	uint16_t	plogx_rspsz_porthi;
	struct {
		uint16_t	lo16;
		uint16_t	hi16;
	} plogx_ioparm[11];
} isp_plogx_t;

#define	PLOGX_STATUS_OK		0x00
#define	PLOGX_STATUS_UNAVAIL	0x28
#define	PLOGX_STATUS_LOGOUT	0x29
#define	PLOGX_STATUS_IOCBERR	0x31

#define	PLOGX_IOCBERR_NOLINK	0x01
#define	PLOGX_IOCBERR_NOIOCB	0x02
#define	PLOGX_IOCBERR_NOXGHG	0x03
#define	PLOGX_IOCBERR_FAILED	0x04	/* further info in IOPARM 1 */
#define	PLOGX_IOCBERR_NOFABRIC	0x05
#define	PLOGX_IOCBERR_NOTREADY	0x07
#define	PLOGX_IOCBERR_NOLOGIN	0x08	/* further info in IOPARM 1 */
#define	PLOGX_IOCBERR_NOPCB	0x0a
#define	PLOGX_IOCBERR_REJECT	0x18	/* further info in IOPARM 1 */
#define	PLOGX_IOCBERR_EINVAL	0x19	/* further info in IOPARM 1 */
#define	PLOGX_IOCBERR_PORTUSED	0x1a	/* further info in IOPARM 1 */
#define	PLOGX_IOCBERR_HNDLUSED	0x1b	/* further info in IOPARM 1 */
#define	PLOGX_IOCBERR_NOHANDLE	0x1c
#define	PLOGX_IOCBERR_NOFLOGI	0x1f	/* further info in IOPARM 1 */

#define	PLOGX_FLG_CMD_MASK	0xf
#define	PLOGX_FLG_CMD_PLOGI	0
#define	PLOGX_FLG_CMD_PRLI	1
#define	PLOGX_FLG_CMD_PDISC	2
#define	PLOGX_FLG_CMD_LOGO	8
#define	PLOGX_FLG_CMD_PRLO	9
#define	PLOGX_FLG_CMD_TPRLO	10

#define	PLOGX_FLG_COND_PLOGI		0x10	/* if with PLOGI */
#define	PLOGX_FLG_IMPLICIT		0x10	/* if with LOGO, PRLO, TPRLO */
#define	PLOGX_FLG_SKIP_PRLI		0x20	/* if with PLOGI */
#define	PLOGX_FLG_IMPLICIT_LOGO_ALL	0x20	/* if with LOGO */
#define	PLOGX_FLG_EXPLICIT_LOGO		0x40	/* if with LOGO */
#define	PLOGX_FLG_COMMON_FEATURES	0x80	/* if with PLOGI */
#define	PLOGX_FLG_FREE_NPHDL		0x80	/* if with with LOGO */

#define	PLOGX_FLG_CLASS2		0x100	/* if with PLOGI */
#define	PLOGX_FLG_FCP2_OVERRIDE		0x200	/* if with PRLOG, PRLI */

/*
 * Simple Name Server Data Structures
 */
#define	SNS_GA_NXT	0x100
#define	SNS_GPN_ID	0x112
#define	SNS_GNN_ID	0x113
#define	SNS_GFF_ID	0x11F
#define	SNS_GID_FT	0x171
#define	SNS_RFT_ID	0x217
typedef struct {
	uint16_t	snscb_rblen;	/* response buffer length (words) */
	uint16_t	snscb_reserved0;
	uint16_t	snscb_addr[4];	/* response buffer address */
	uint16_t	snscb_sblen;	/* subcommand buffer length (words) */
	uint16_t	snscb_reserved1;
	uint16_t	snscb_data[1];	/* variable data */
} sns_screq_t;	/* Subcommand Request Structure */

typedef struct {
	uint16_t	snscb_rblen;	/* response buffer length (words) */
	uint16_t	snscb_reserved0;
	uint16_t	snscb_addr[4];	/* response buffer address */
	uint16_t	snscb_sblen;	/* subcommand buffer length (words) */
	uint16_t	snscb_reserved1;
	uint16_t	snscb_cmd;
	uint16_t	snscb_reserved2;
	uint32_t	snscb_reserved3;
	uint32_t	snscb_port;
} sns_ga_nxt_req_t;
#define	SNS_GA_NXT_REQ_SIZE	(sizeof (sns_ga_nxt_req_t))

typedef struct {
	uint16_t	snscb_rblen;	/* response buffer length (words) */
	uint16_t	snscb_reserved0;
	uint16_t	snscb_addr[4];	/* response buffer address */
	uint16_t	snscb_sblen;	/* subcommand buffer length (words) */
	uint16_t	snscb_reserved1;
	uint16_t	snscb_cmd;
	uint16_t	snscb_reserved2;
	uint32_t	snscb_reserved3;
	uint32_t	snscb_portid;
} sns_gxn_id_req_t;
#define	SNS_GXN_ID_REQ_SIZE	(sizeof (sns_gxn_id_req_t))

typedef struct {
	uint16_t	snscb_rblen;	/* response buffer length (words) */
	uint16_t	snscb_reserved0;
	uint16_t	snscb_addr[4];	/* response buffer address */
	uint16_t	snscb_sblen;	/* subcommand buffer length (words) */
	uint16_t	snscb_reserved1;
	uint16_t	snscb_cmd;
	uint16_t	snscb_mword_div_2;
	uint32_t	snscb_reserved3;
	uint32_t	snscb_fc4_type;
} sns_gid_ft_req_t;
#define	SNS_GID_FT_REQ_SIZE	(sizeof (sns_gid_ft_req_t))

typedef struct {
	uint16_t	snscb_rblen;	/* response buffer length (words) */
	uint16_t	snscb_reserved0;
	uint16_t	snscb_addr[4];	/* response buffer address */
	uint16_t	snscb_sblen;	/* subcommand buffer length (words) */
	uint16_t	snscb_reserved1;
	uint16_t	snscb_cmd;
	uint16_t	snscb_reserved2;
	uint32_t	snscb_reserved3;
	uint32_t	snscb_port;
	uint32_t	snscb_fc4_types[8];
} sns_rft_id_req_t;
#define	SNS_RFT_ID_REQ_SIZE	(sizeof (sns_rft_id_req_t))

typedef struct {
	ct_hdr_t	snscb_cthdr;
	uint8_t		snscb_port_type;
	uint8_t		snscb_port_id[3];
	uint8_t		snscb_portname[8];
	uint16_t	snscb_data[1];	/* variable data */
} sns_scrsp_t;	/* Subcommand Response Structure */

typedef struct {
	ct_hdr_t	snscb_cthdr;
	uint8_t		snscb_port_type;
	uint8_t		snscb_port_id[3];
	uint8_t		snscb_portname[8];
	uint8_t		snscb_pnlen;		/* symbolic port name length */
	uint8_t		snscb_pname[255];	/* symbolic port name */
	uint8_t		snscb_nodename[8];
	uint8_t		snscb_nnlen;		/* symbolic node name length */
	uint8_t		snscb_nname[255];	/* symbolic node name */
	uint8_t		snscb_ipassoc[8];
	uint8_t		snscb_ipaddr[16];
	uint8_t		snscb_svc_class[4];
	uint8_t		snscb_fc4_types[32];
	uint8_t		snscb_fpname[8];
	uint8_t		snscb_reserved;
	uint8_t		snscb_hardaddr[3];
} sns_ga_nxt_rsp_t;	/* Subcommand Response Structure */
#define	SNS_GA_NXT_RESP_SIZE	(sizeof (sns_ga_nxt_rsp_t))

typedef struct {
	ct_hdr_t	snscb_cthdr;
	uint8_t		snscb_wwn[8];
} sns_gxn_id_rsp_t;
#define	SNS_GXN_ID_RESP_SIZE	(sizeof (sns_gxn_id_rsp_t))

typedef struct {
	ct_hdr_t	snscb_cthdr;
	uint32_t	snscb_fc4_features[32];
} sns_gff_id_rsp_t;
#define	SNS_GFF_ID_RESP_SIZE	(sizeof (sns_gff_id_rsp_t))

typedef struct {
	ct_hdr_t	snscb_cthdr;
	struct {
		uint8_t		control;
		uint8_t		portid[3];
	} snscb_ports[1];
} sns_gid_ft_rsp_t;
#define	SNS_GID_FT_RESP_SIZE(x)	((sizeof (sns_gid_ft_rsp_t)) + ((x - 1) << 2))
#define	SNS_RFT_ID_RESP_SIZE	(sizeof (ct_hdr_t))

/*
 * Other Misc Structures
 */

/* ELS Pass Through */
typedef struct {
	isphdr_t	els_hdr;
	uint32_t	els_handle;
	uint16_t	els_status;
	uint16_t	els_nphdl;
	uint16_t	els_xmit_dsd_count;	/* outgoing only */
	uint8_t		els_vphdl;
	uint8_t		els_sof;
	uint32_t	els_rxid;
	uint16_t	els_recv_dsd_count;	/* outgoing only */
	uint8_t		els_opcode;
	uint8_t		els_reserved1;
	uint8_t		els_did_lo;
	uint8_t		els_did_mid;
	uint8_t		els_did_hi;
	uint8_t		els_reserved2;
	uint16_t	els_reserved3;
	uint16_t	els_ctl_flags;
	union {
		struct {
			uint32_t	_els_bytecnt;
			uint32_t	_els_subcode1;
			uint32_t	_els_subcode2;
			uint8_t		_els_reserved4[20];
		} in;
		struct {
			uint32_t	_els_recv_bytecnt;
			uint32_t	_els_xmit_bytecnt;
			uint32_t	_els_xmit_dsd_length;
			uint16_t	_els_xmit_dsd_a1500;
			uint16_t	_els_xmit_dsd_a3116;
			uint16_t	_els_xmit_dsd_a4732;
			uint16_t	_els_xmit_dsd_a6348;
			uint32_t	_els_recv_dsd_length;
			uint16_t	_els_recv_dsd_a1500;
			uint16_t	_els_recv_dsd_a3116;
			uint16_t	_els_recv_dsd_a4732;
			uint16_t	_els_recv_dsd_a6348;
		} out;
	} inout;
#define	els_bytecnt		inout.in._els_bytecnt
#define	els_subcode1		inout.in._els_subcode1
#define	els_subcode2		inout.in._els_subcode2
#define	els_reserved4		inout.in._els_reserved4
#define	els_recv_bytecnt	inout.out._els_recv_bytecnt
#define	els_xmit_bytecnt	inout.out._els_xmit_bytecnt
#define	els_xmit_dsd_length	inout.out._els_xmit_dsd_length
#define	els_xmit_dsd_a1500	inout.out._els_xmit_dsd_a1500
#define	els_xmit_dsd_a3116	inout.out._els_xmit_dsd_a3116
#define	els_xmit_dsd_a4732	inout.out._els_xmit_dsd_a4732
#define	els_xmit_dsd_a6348	inout.out._els_xmit_dsd_a6348
#define	els_recv_dsd_length	inout.out._els_recv_dsd_length
#define	els_recv_dsd_a1500	inout.out._els_recv_dsd_a1500
#define	els_recv_dsd_a3116	inout.out._els_recv_dsd_a3116
#define	els_recv_dsd_a4732	inout.out._els_recv_dsd_a4732
#define	els_recv_dsd_a6348	inout.out._els_recv_dsd_a6348
} els_t;

/*
 * A handy package structure for running FC-SCSI commands via RUN IOCB A64.
 */
typedef struct {
	uint16_t	handle;
	uint16_t	lun;
	uint32_t	portid;
	uint32_t	timeout;
	union {
		struct {
			uint32_t data_length;
			uint8_t do_read;
			uint8_t pad[3];
			uint8_t cdb[16];
			void *data_ptr;
		} beg;
		struct {
			uint32_t data_residual;
			uint8_t status;
			uint8_t pad;
			uint16_t sense_length;
			uint8_t sense_data[32];
		} end;
	} fcd;
} isp_xcmd_t;
#endif	/* _ISPMBOX_H */
