/* $Id: ispmbox.h,v 1.7 1998/09/14 23:23:26 mjacob Exp $ */
/*
 * Mailbox and Queue Entry Definitions for for Qlogic ISP SCSI adapters.
 *
 *---------------------------------------
 * Copyright (c) 1997, 1998 by Matthew Jacob
 * NASA/Ames Research Center
 * All rights reserved.
 *---------------------------------------
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
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
					/*   9 */
					/*   a */
					/*   b */
					/*   c */
					/*   d */
#define MBOX_CHECK_FIRMWARE		0x000e
					/*   f */
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
#define MBOX_GET_TARGET_PARAMS		0x0028
#define MBOX_GET_DEV_QUEUE_PARAMS	0x0029
					/*  2a */
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
#define MBOX_SET_ACTIVE_NEG_STATE	0x0035
#define MBOX_SET_ASYNC_DATA_SETUP_TIME	0x0036
#define MBOX_SET_SBUS_CONTROL_PARAMS	0x0037
#define		MBOX_SET_PCI_PARAMETERS	0x0037
#define MBOX_SET_TARGET_PARAMS		0x0038
#define MBOX_SET_DEV_QUEUE_PARAMS	0x0039
					/*  3a */
					/*  3b */
					/*  3c */
					/*  3d */
					/*  3e */
					/*  3f */
#define	MBOX_RETURN_BIOS_BLOCK_ADDR	0x0040
#define	MBOX_WRITE_FOUR_RAM_WORDS	0x0041
#define	MBOX_EXEC_BIOS_IOCB		0x0042

/* These are for the ISP2100 FC cards */
#define	MBOX_GET_LOOP_ID		0x20
#define	MBOX_EXEC_COMMAND_IOCB_A64	0x54
#define	MBOX_INIT_FIRMWARE		0x60
#define	MBOX_GET_INIT_CONTROL_BLOCK	0x61
#define	MBOX_INIT_LIP			0x62
#define	MBOX_GET_FC_AL_POSITION_MAP	0x63
#define	MBOX_GET_PORT_DB		0x64
#define	MBOX_CLEAR_ACA			0x65
#define	MBOX_TARGET_RESET		0x66
#define	MBOX_CLEAR_TASK_SET		0x67
#define	MBOX_ABORT_TASK_SET		0x68
#define	MBOX_GET_FW_STATE		0x69
#define	MBOX_GET_LINK_STATUS		0x6a
#define	MBOX_INIT_LIP_RESET		0x6c
#define	MBOX_INIT_LIP_LOGIN		0x72

#define	ISP2100_SET_PCI_PARAM		0x00ff

#define	MBOX_BUSY			0x04

typedef struct {
	u_int16_t param[8];
} mbreg_t;

/*
 * Mailbox Command Complete Status Codes
 */
#define	MBOX_COMMAND_COMPLETE		0x4000
#define	MBOX_INVALID_COMMAND		0x4001
#define	MBOX_HOST_INTERFACE_ERROR	0x4002
#define	MBOX_TEST_FAILED		0x4003
#define	MBOX_COMMAND_ERROR		0x4005
#define	MBOX_COMMAND_PARAM_ERROR	0x4006

/*
 * Asynchronous event status codes
 */
#define	ASYNC_BUS_RESET			0x8001
#define	ASYNC_SYSTEM_ERROR		0x8002
#define	ASYNC_RQS_XFER_ERR		0x8003
#define	ASYNC_RSP_XFER_ERR		0x8004
#define	ASYNC_QWAKEUP			0x8005
#define	ASYNC_TIMEOUT_RESET		0x8006
#define	ASYNC_UNSPEC_TMODE		0x8007
#define	ASYNC_EXTMSG_UNDERRUN		0x800A
#define	ASYNC_SCAM_INT			0x800B
#define	ASYNC_HUNG_SCSI			0x800C
#define	ASYNC_KILLED_BUS		0x800D
#define	ASYNC_BUS_TRANSIT		0x800E	/* LVD -> HVD, eg. */
#define	ASYNC_CMD_CMPLT			0x8020
#define	ASYNC_CTIO_DONE			0x8021

/* for ISP2100 only */
#define	ASYNC_LIP_OCCURRED		0x8010
#define	ASYNC_LOOP_UP			0x8011
#define	ASYNC_LOOP_DOWN			0x8012
#define	ASYNC_LOOP_RESET		0x8013
#define	ASYNC_PDB_CHANGED		0x8014	/* Port Database Changed */
#define	ASYNC_CHANGE_NOTIFY		0x8015

/*
 * Command Structure Definitions
 */

typedef struct {
	u_int32_t	ds_base;
	u_int32_t	ds_count;
} ispds_t;

typedef struct {
#if BYTE_ORDER == BIG_ENDIAN
	u_int8_t	rqs_entry_count;
	u_int8_t	rqs_entry_type;
	u_int8_t	rqs_flags;
	u_int8_t	rqs_seqno;
#else
	u_int8_t	rqs_entry_type;
	u_int8_t	rqs_entry_count;
	u_int8_t	rqs_seqno;
	u_int8_t	rqs_flags;
#endif
} isphdr_t;

/* RQS Flag definitions */
#define	RQSFLAG_CONTINUATION	0x01
#define	RQSFLAG_FULL		0x02
#define	RQSFLAG_BADHEADER	0x04
#define	RQSFLAG_BADPACKET	0x08

/* RQS entry_type definitions */
#define	RQSTYPE_REQUEST		0x01
#define	RQSTYPE_DATASEG		0x02
#define	RQSTYPE_RESPONSE	0x03
#define	RQSTYPE_MARKER		0x04
#define	RQSTYPE_CMDONLY		0x05
#define	RQSTYPE_ATIO		0x06	/* Target Mode */
#define	RQSTYPE_CTIO0		0x07	/* Target Mode */
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

#define	RQSTYPE_T4RQS		0x15
#define	RQSTYPE_ATIO2		0x16
#define	RQSTYPE_CTIO2		0x17
#define	RQSTYPE_CSET0		0x18
#define	RQSTYPE_T3RQS		0x19

#define	RQSTYPE_CTIO3		0x1f


#define	ISP_RQDSEG	4
typedef struct {
	isphdr_t	req_header;
	u_int32_t	req_handle;
#if BYTE_ORDER == BIG_ENDIAN
	u_int8_t	req_target;
	u_int8_t	req_lun_trn;
#else
	u_int8_t	req_lun_trn;
	u_int8_t	req_target;
#endif
	u_int16_t	req_cdblen;
#define	req_modifier	req_cdblen	/* marker packet */
	u_int16_t	req_flags;
	u_int16_t	_res1;
	u_int16_t	req_time;
	u_int16_t	req_seg_count;
	u_int8_t	req_cdb[12];
	ispds_t		req_dataseg[ISP_RQDSEG];
} ispreq_t;

#define	ISP_RQDSEG_T2	3
typedef struct {
	isphdr_t	req_header;
	u_int32_t	req_handle;
#if BYTE_ORDER == BIG_ENDIAN
	u_int8_t	req_target;
	u_int8_t	req_lun_trn;
#else
	u_int8_t	req_lun_trn;
	u_int8_t	req_target;
#endif
	u_int16_t	_res1;
	u_int16_t	req_flags;
	u_int16_t	_res2;
	u_int16_t	req_time;
	u_int16_t	req_seg_count;
	u_int32_t	req_cdb[4];
	u_int32_t	req_totalcnt;
	ispds_t		req_dataseg[ISP_RQDSEG_T2];
} ispreqt2_t;

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
	u_int32_t	req_handle;
#if	BYTE_ORDER == BIG_ENDIAN
	u_int8_t	req_target;
	u_int8_t	req_lun_trn;
#else
	u_int8_t	req_lun_trn;
	u_int8_t	req_target;
#endif
	u_int16_t	req_cdblen;
	u_int16_t	req_flags;
	u_int16_t	_res1;
	u_int16_t	req_time;
	u_int16_t	req_seg_count;
	u_int8_t	req_cdb[44];
} ispextreq_t;

#define	ISP_CDSEG	7
typedef struct {
	isphdr_t	req_header;
	u_int32_t	_res1;
	ispds_t		req_dataseg[ISP_CDSEG];
} ispcontreq_t;

typedef struct {
	isphdr_t	req_header;
	u_int32_t	_res1;
#if	BYTE_ORDER == BIG_ENDIAN
	u_int8_t	req_target;
	u_int8_t	req_lun_trn;
	u_int8_t	_res2;
	u_int8_t	req_modifier;
#else
	u_int8_t	req_lun_trn;
	u_int8_t	req_target;
	u_int8_t	req_modifier;
	u_int8_t	_res2;
#endif
} ispmarkreq_t;

#define SYNC_DEVICE	0
#define SYNC_TARGET	1
#define SYNC_ALL	2

typedef struct {
	isphdr_t	req_header;
	u_int32_t	req_handle;
	u_int16_t	req_scsi_status;
	u_int16_t	req_completion_status;
	u_int16_t	req_state_flags;
	u_int16_t	req_status_flags;
	u_int16_t	req_time;
	u_int16_t	req_sense_len;
	u_int32_t	req_resid;
	u_int8_t	_res1[8];
	u_int8_t	req_sense_data[32];
} ispstatusreq_t;

/* 
 * For Qlogic 2100, the high order byte of SCSI status has
 * additional meaning.
 */
#define	RQCS_RU	0x800	/* Residual Under */
#define	RQCS_RO	0x400	/* Residual Over */
#define	RQCS_SV	0x200	/* Sense Length Valid */
#define	RQCS_RV	0x100	/* Residual Valid */

/* 
 * Completion Status Codes.
 */
#define RQCS_COMPLETE			0x0000
#define RQCS_INCOMPLETE			0x0001
#define RQCS_DMA_ERROR			0x0002
#define RQCS_TRANSPORT_ERROR		0x0003
#define RQCS_RESET_OCCURRED		0x0004
#define RQCS_ABORTED			0x0005
#define RQCS_TIMEOUT			0x0006
#define RQCS_DATA_OVERRUN		0x0007
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
#define RQCS_DATA_UNDERRUN		0x0015
#define	RQCS_XACT_ERR1			0x0018
#define	RQCS_XACT_ERR2			0x0019
#define	RQCS_XACT_ERR3			0x001A
#define	RQCS_BAD_ENTRY			0x001B
#define	RQCS_QUEUE_FULL			0x001C
#define	RQCS_PHASE_SKIPPED		0x001D
#define	RQCS_ARQS_FAILED		0x001E
#define	RQCS_WIDE_FAILED		0x001F
#define	RQCS_SYNCXFER_FAILED		0x0020
#define	RQCS_LVD_BUSERR			0x0021

/* 2100 Only Completion Codes */
#define	RQCS_PORT_UNAVAILABLE		0x0028
#define	RQCS_PORT_LOGGED_OUT		0x0029
#define	RQCS_PORT_CHANGED		0x002A
#define	RQCS_PORT_BUSY			0x002B

/*
 * State Flags (not applicable to 2100)
 */
#define RQSF_GOT_BUS			0x0100
#define RQSF_GOT_TARGET			0x0200
#define RQSF_SENT_CDB			0x0400
#define RQSF_XFRD_DATA			0x0800
#define RQSF_GOT_STATUS			0x1000
#define RQSF_GOT_SENSE			0x2000
#define	RQSF_XFER_COMPLETE		0x4000

/*
 * Status Flags (not applicable to 2100)
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
 * Target Mode Structures
 */
/*
 * Used for Enable LUN and Modify Lun types.
 * (for FC, pre-1.14 FW layout revision).
 */
typedef struct {
	isphdr_t	req_header;
	u_int32_t	req_handle;
#if	BYTE_ORDER == BIG_ENDIAN
	u_int8_t	_reserved0;
	u_int8_t	req_lun;	/* HOST->FW: LUN to enable */
#else
	u_int8_t	req_lun;	/* HOST->FW: LUN to enable */
	u_int8_t	_reserved0;
#endif
	u_int16_t	_reserved1[3];
#if	BYTE_ORDER == BIG_ENDIAN
	u_int8_t	_reserved2;
	u_int8_t	req_status;	/* FW->HOST: Status of Request */
	u_int8_t	req_imcount;	/* HOST->FW: Immediate Notify Count */
	u_int8_t	req_cmdcount;	/* HOST->FW: ATIO Count */
#else
	u_int8_t	req_status;	/* FW->HOST: Status of Request */
	u_int8_t	_reserved2;
	u_int8_t	req_cmdcount;	/* HOST->FW: ATIO Count */
	u_int8_t	req_imcount;	/* HOST->FW: Immediate Notify Count */
#endif
	u_int16_t	_reserved3;
	u_int16_t	req_timeout;	/* HOST->FW: Lun timeout value */
} isplun_t;
/* inbound status */
#define	LUN_OKAY	0x01
#define	LUN_ERR		0x04
#define	LUN_NOCAP	0x16
#define	LUN_ENABLED	0x3e
/* outbound flags */
#define	LUN_INCR_CMD	0x0001
#define	LUN_DECR_CMD	0x0002
#define	LUN_INCR_IMMED	0x0100
#define	LUN_DECR_IMMED	0x0200

typedef struct {
	isphdr_t	req_header;
	u_int32_t	req_handle;
#if	BYTE_ORDER == BIG_ENDIAN
	u_int8_t	req_initiator;
	u_int8_t	req_lun;
#else
	u_int8_t	req_lun;
	u_int8_t	req_initiator;
#endif
	u_int16_t	req_flags;	/* NOTIFY_ACK only */
	u_int16_t	_reserved1[2];
	u_int16_t	req_status;
	u_int16_t	req_task_flags;
	u_int16_t	req_sequence;
} ispnotify_t;

#define	IN_NOCAP	0x16
#define	IN_IDE_RECEIVED	0x33
#define	IN_RSRC_UNAVAIL	0x34
#define	IN_MSG_RECEIVED	0x36

typedef struct {
	isphdr_t	req_header;
	u_int32_t	req_handle;
#if	BYTE_ORDER == BIG_ENDIAN
	u_int8_t	req_initiator;
	u_int8_t	req_lun;
#else
	u_int8_t	req_lun;
	u_int8_t	req_initiator;
#endif
	u_int16_t	req_rxid;
	u_int16_t	req_flags;
	u_int16_t	req_status;
#if	BYTE_ORDER == BIG_ENDIAN
	u_int8_t	req_taskcodes;
	u_int8_t	_reserved0;
	u_int8_t	req_execodes;
	u_int8_t	req_taskflags;
#else
	u_int8_t	_reserved0;
	u_int8_t	req_taskcodes;
	u_int8_t	req_taskflags;
	u_int8_t	req_execodes;
#endif
	u_int8_t	req_cdb[16];
	u_int32_t	req_datalen;
	u_int16_t	req_scclun;
	u_int16_t	_reserved1;;
	u_int16_t	req_scsi_status;
	u_int8_t	req_sense[18];
} ispatiot2_t;

#define	ATIO_PATH_INVALID	0x07
#define	ATIO_PHASE_ERROR	0x14
#define	ATIO_NOCAP		0x16
#define	ATIO_BDR_MSG		0x17
#define	ATIO_CDB_RECEIVED	0x3d

#define	ATIO_SENSEVALID		0x80

/*
 * Continue Target I/O, type 2
 */
typedef struct {
	isphdr_t	req_header;
	u_int32_t	req_handle;
#if	BYTE_ORDER == BIG_ENDIAN
	u_int8_t	req_initiator;
	u_int8_t	req_lun;
#else
	u_int8_t	req_lun;
	u_int8_t	req_initiator;
#endif
	u_int16_t	req_rxid;
	u_int16_t	req_flags;
	u_int16_t	req_status;
	u_int16_t	req_timeout;
	u_int16_t	req_seg_count;	/* data segment count */
	u_int8_t	req_reloff[4];	/* relative offset */
	u_int8_t	req_resid[4];	/* residual */
	u_int8_t	_reserved0[4];
	union {				/* should be offset 0x20 */
		struct {
			u_int16_t _reserved0;
			u_int16_t req_scsi_status;
			u_int32_t req_datalen;
			ispds_t	req_dataseg[ISP_RQDSEG_T2];
		} mode0;
		struct {
			u_int16_t req_sense_len;
			u_int16_t req_scsi_status;
			u_int32_t req_response_length;
			u_int8_t req_response[26];
		} mode1;
		struct {
			u_int16_t _reserved0[2];
			u_int32_t req_datalen;
			ispds_t	req_fcpiudata;
		} mode2;
	} req_m;
} ispctiot2_t;

#define	CTIO_SEND_STATUS	0x8000
#define	CTIO_SEND_DATA		0x0040	/* To initiator */
#define	CTIO_RECV_DATA		0x0080
#define	CTIO_NODATA		0x00C0

#define	CTIO2_SMODE0		0x0000
#define	CTIO2_SMODE1		0x0001
#define	CTIO2_SMODE2		0x0002

#define	CTIO2_RESP_VALID	0x0100
#define	CTIO2_STATUS_VALID	0x0200
#define	CTIO2_RSPOVERUN		0x0400
#define	CTIO2_RSPUNDERUN	0x0800


/*
 * FC (ISP2100) specific data structures
 */

/*
 * Initialization Control Block
 *
 * Version One format.
 */
typedef struct {
#if BYTE_ORDER == BIG_ENDIAN
	u_int8_t	_reserved0;
	u_int8_t	icb_version;
#else
	u_int8_t	icb_version;
	u_int8_t	_reserved0;
#endif
        u_int16_t	icb_fwoptions;
        u_int16_t	icb_maxfrmlen;
	u_int16_t	icb_maxalloc;
	u_int16_t	icb_execthrottle;
#if BYTE_ORDER == BIG_ENDIAN
	u_int8_t	icb_retry_delay;
	u_int8_t	icb_retry_count;
#else
	u_int8_t	icb_retry_count;
	u_int8_t	icb_retry_delay;
#endif
        u_int8_t	icb_nodename[8];
	u_int16_t	icb_hardaddr;
#if BYTE_ORDER == BIG_ENDIAN
	u_int8_t	_reserved1;
	u_int8_t	icb_iqdevtype;
#else
	u_int8_t	icb_iqdevtype;
	u_int8_t	_reserved1;
#endif
        u_int8_t	icb_portname[8];
	u_int16_t	icb_rqstout;
	u_int16_t	icb_rspnsin;
        u_int16_t	icb_rqstqlen;
        u_int16_t	icb_rsltqlen;
        u_int16_t	icb_rqstaddr[4];
        u_int16_t	icb_respaddr[4];
} isp_icb_t;
#define	ICB_VERSION1	1

#define	ICBOPT_HARD_ADDRESS	(1<<0)
#define	ICBOPT_FAIRNESS		(1<<1)
#define	ICBOPT_FULL_DUPLEX	(1<<2)
#define	ICBOPT_FAST_POST	(1<<3)
#define	ICBOPT_TGT_ENABLE	(1<<4)
#define	ICBOPT_INI_DISABLE	(1<<5)
#define	ICBOPT_INI_ADISC	(1<<6)
#define	ICBOPT_INI_TGTTYPE	(1<<7)
#define	ICBOPT_PDBCHANGE_AE	(1<<8)
#define	ICBOPT_NOLIP		(1<<9)
#define	ICBOPT_SRCHDOWN		(1<<10)
#define	ICBOPT_PREVLOOP		(1<<11)
#define	ICBOPT_STOP_ON_QFULL	(1<<12)
#define	ICBOPT_FULL_LOGIN	(1<<13)
#define	ICBOPT_USE_PORTNAME	(1<<14)


#define	ICB_MIN_FRMLEN		256
#define	ICB_MAX_FRMLEN		2112
#define	ICB_DFLT_FRMLEN		1024

#define	RQRSP_ADDR0015	0
#define	RQRSP_ADDR1631	1
#define	RQRSP_ADDR3247	2
#define	RQRSP_ADDR4863	3


#if BYTE_ORDER == BIG_ENDIAN
#define	ICB_NNM0	6
#define	ICB_NNM1	7
#define	ICB_NNM2	4
#define	ICB_NNM3	5
#define	ICB_NNM4	2
#define	ICB_NNM5	3
#define	ICB_NNM6	0
#define	ICB_NNM7	1
#else
#define	ICB_NNM0	7
#define	ICB_NNM1	6
#define	ICB_NNM2	5
#define	ICB_NNM3	4
#define	ICB_NNM4	3
#define	ICB_NNM5	2
#define	ICB_NNM6	1
#define	ICB_NNM7	0
#endif

#define	MAKE_NODE_NAME_FROM_WWN(array, wwn)	\
	array[ICB_NNM0] = (u_int8_t) ((wwn >>  0) & 0xff), \
	array[ICB_NNM1] = (u_int8_t) ((wwn >>  8) & 0xff), \
	array[ICB_NNM2] = (u_int8_t) ((wwn >> 16) & 0xff), \
	array[ICB_NNM3] = (u_int8_t) ((wwn >> 24) & 0xff), \
	array[ICB_NNM4] = (u_int8_t) ((wwn >> 32) & 0xff), \
	array[ICB_NNM5] = (u_int8_t) ((wwn >> 40) & 0xff), \
	array[ICB_NNM6] = (u_int8_t) ((wwn >> 48) & 0xff), \
	array[ICB_NNM7] = (u_int8_t) ((wwn >> 56) & 0xff)

#endif	/* _ISPMBOX_H */
