/* $Id: ispmbox.h,v 1.3 1998/04/14 17:51:32 mjacob Exp $ */
/*
 * Mailbox and Command Definitions for for Qlogic ISP SCSI adapters.
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

#define	ISP2100_SET_PCI_PARAM		0x00ff

#define	MBOX_BUSY			0x04

typedef struct {
	u_int16_t param[8];
} mbreg_t;

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
#define	RQSTYPE_REQUEST		1
#define	RQSTYPE_DATASEG		2
#define	RQSTYPE_RESPONSE	3
#define	RQSTYPE_MARKER		4
#define	RQSTYPE_CMDONLY		5
#define	RQSTYPE_T2RQS		17
#define	RQSTYPE_T3RQS		25
#define	RQSTYPE_T1DSEG		10


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
 * FC (ISP2100) specific data structures
 */

/*
 * Initialization Control Block
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
        u_int16_t	icb_nodename[4];
	u_int16_t	icb_hardaddr;
        u_int16_t	_reserved1[5];
	u_int16_t	icb_rqstout;
	u_int16_t	icb_rspnsin;
        u_int16_t	icb_rqstqlen;
        u_int16_t	icb_rsltqlen;
        u_int16_t	icb_rqstaddr[4];
        u_int16_t	icb_respaddr[4];
} isp_icb_t;

#define	ICB_DFLT_FRMLEN	1024
#define	MAKE_NODE_NAME(isp, icbp) \
	(icbp)->icb_nodename[0] = 0, (icbp)->icb_nodename[1] = 0x5355,\
	(icbp)->icb_nodename[2] = 0x4E57, (icbp)->icb_nodename[3] = 0

#endif	/* _ISPMBOX_H */
