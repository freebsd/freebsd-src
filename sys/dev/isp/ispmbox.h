/* $Id: ispmbox.h,v 1.9 1999/04/04 01:32:09 mjacob Exp $ */
/* release_5_11_99 */
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
#define		FW_FEATURE_LVD_NOTIFY	0x2
#define		FW_FEATURE_FAST_POST	0x1

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
#define	MBOX_GET_PORT_NAME		0x6a
#define	MBOX_GET_LINK_STATUS		0x6b
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
#define	ASYNC_DEVICE_RESET		0x8007
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
#define	ASYNC_PDB_CHANGED		0x8014
#define	ASYNC_CHANGE_NOTIFY		0x8015

/*
 * Command Structure Definitions
 */

typedef struct {
	u_int32_t	ds_base;
	u_int32_t	ds_count;
} ispds_t;

#define	_ISP_SWAP8(a, b)	{	\
	u_int8_t tmp;			\
	tmp = a;			\
	a = b;				\
	b = tmp;			\
}

/*
 * These elements get swizzled around for SBus instances.
 */
typedef struct {
	u_int8_t	rqs_entry_type;
	u_int8_t	rqs_entry_count;
	u_int8_t	rqs_seqno;
	u_int8_t	rqs_flags;
} isphdr_t;
/*
 * There are no (for all intents and purposes) non-sparc SBus machines
 */
#ifdef	__sparc__
#define	ISP_SBUSIFY_ISPHDR(isp, hdrp)					\
    if ((isp)->isp_bustype == ISP_BT_SBUS) {				\
	_ISP_SWAP8((hdrp)->rqs_entry_count, (hdrp)->rqs_entry_type);	\
	_ISP_SWAP8((hdrp)->rqs_flags, (hdrp)->rqs_seqno);		\
    }
#else
#define	ISP_SBUSIFY_ISPHDR(a, b)
#endif

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
	u_int8_t	req_lun_trn;
	u_int8_t	req_target;
	u_int16_t	req_cdblen;
#define	req_modifier	req_cdblen	/* marker packet */
	u_int16_t	req_flags;
	u_int16_t	req_reserved;
	u_int16_t	req_time;
	u_int16_t	req_seg_count;
	u_int8_t	req_cdb[12];
	ispds_t		req_dataseg[ISP_RQDSEG];
} ispreq_t;

/*
 * A request packet can also be a marker packet.
 */
#define SYNC_DEVICE	0
#define SYNC_TARGET	1
#define SYNC_ALL	2

/*
 * There are no (for all intents and purposes) non-sparc SBus machines
 */
#ifdef	__sparc__
#define	ISP_SBUSIFY_ISPREQ(isp, rqp)					\
    if ((isp)->isp_bustype == ISP_BT_SBUS) {				\
	_ISP_SWAP8((rqp)->req_target, (rqp)->req_lun_trn);		\
    }
#else
#define	ISP_SBUSIFY_ISPREQ(a, b)
#endif

#define	ISP_RQDSEG_T2	3
typedef struct {
	isphdr_t	req_header;
	u_int32_t	req_handle;
	u_int8_t	req_lun_trn;
	u_int8_t	req_target;
	u_int16_t	req_scclun;
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
	u_int8_t	req_lun_trn;
	u_int8_t	req_target;
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
 * FC (ISP2100) specific data structures
 */

/*
 * Initialization Control Block
 *
 * Version One format.
 */
typedef struct {
	u_int8_t	icb_version;
	u_int8_t	_reserved0;
        u_int16_t	icb_fwoptions;
        u_int16_t	icb_maxfrmlen;
	u_int16_t	icb_maxalloc;
	u_int16_t	icb_execthrottle;
	u_int8_t	icb_retry_count;
	u_int8_t	icb_retry_delay;
        u_int8_t	icb_nodename[8];
	u_int16_t	icb_hardaddr;
	u_int8_t	icb_iqdevtype;
	u_int8_t	_reserved1;
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


#define	ICB_NNM0	7
#define	ICB_NNM1	6
#define	ICB_NNM2	5
#define	ICB_NNM3	4
#define	ICB_NNM4	3
#define	ICB_NNM5	2
#define	ICB_NNM6	1
#define	ICB_NNM7	0

#define	MAKE_NODE_NAME_FROM_WWN(array, wwn)	\
	array[ICB_NNM0] = (u_int8_t) ((wwn >>  0) & 0xff), \
	array[ICB_NNM1] = (u_int8_t) ((wwn >>  8) & 0xff), \
	array[ICB_NNM2] = (u_int8_t) ((wwn >> 16) & 0xff), \
	array[ICB_NNM3] = (u_int8_t) ((wwn >> 24) & 0xff), \
	array[ICB_NNM4] = (u_int8_t) ((wwn >> 32) & 0xff), \
	array[ICB_NNM5] = (u_int8_t) ((wwn >> 40) & 0xff), \
	array[ICB_NNM6] = (u_int8_t) ((wwn >> 48) & 0xff), \
	array[ICB_NNM7] = (u_int8_t) ((wwn >> 56) & 0xff)

/*
 * Port Data Base Element
 */

typedef struct {
	u_int16_t	pdb_options;
	u_int8_t	pdb_mstate;
	u_int8_t	pdb_sstate;
#define	BITS2WORD(x)	(x)[0] << 16 | (x)[3] << 8 | (x)[2]
	u_int8_t	pdb_hardaddr_bits[4];
	u_int8_t	pdb_portid_bits[4];
	u_int8_t	pdb_nodename[8];
	u_int8_t	pdb_portname[8];
	u_int16_t	pdb_execthrottle;
	u_int16_t	pdb_exec_count;
	u_int8_t	pdb_retry_count;
	u_int8_t	pdb_retry_delay;
	u_int16_t	pdb_resalloc;
	u_int16_t	pdb_curalloc;
	u_int16_t	pdb_qhead;
	u_int16_t	pdb_qtail;
	u_int16_t	pdb_tl_next;
	u_int16_t	pdb_tl_last;
	u_int16_t	pdb_features;	/* PLOGI, Common Service */
	u_int16_t	pdb_pconcurrnt;	/* PLOGI, Common Service */
	u_int16_t	pdb_roi;	/* PLOGI, Common Service */
	u_int8_t	pdb_target;
	u_int8_t	pdb_initiator;	/* PLOGI, Class 3 Control Flags */
	u_int16_t	pdb_rdsiz;	/* PLOGI, Class 3 */
	u_int16_t	pdb_ncseq;	/* PLOGI, Class 3 */
	u_int16_t	pdb_noseq;	/* PLOGI, Class 3 */
	u_int16_t	pdb_labrtflg;
	u_int16_t	pdb_lstopflg;
	u_int16_t	pdb_sqhead;
	u_int16_t	pdb_sqtail;
	u_int16_t	pdb_ptimer;
	u_int16_t	pdb_nxt_seqid;
	u_int16_t	pdb_fcount;
	u_int16_t	pdb_prli_len;
	u_int16_t	pdb_prli_svc0;
	u_int16_t	pdb_prli_svc3;
	u_int16_t	pdb_loopid;
	u_int16_t	pdb_il_ptr;
	u_int16_t	pdb_sl_ptr;
} isp_pdb_t;

#define	INVALID_PDB_OPTIONS	0xDEAD

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

/*
 * Target Mode Structures
 */
#define TGTSVALID	0x80	/* scsi status & sense data valid */
#define	SUGGSENSELEN	18

/*
 * Structure for Enable Lun and Modify Lun queue entries
 */
typedef struct {
	isphdr_t		le_header;
	u_int32_t		le_reserved2;
	u_int8_t		le_lun;
	u_int8_t		le_rsvd;
	u_int8_t		le_ops;		/* Modify LUN only */
	u_int8_t		le_tgt;		/* Not for FC */
	u_int32_t		le_flags;	/* Not for FC */
	u_int8_t		le_status;
	u_int8_t		le_rsvd2;
	u_int8_t		le_cmd_count;
	u_int8_t		le_in_count;
	u_int8_t		le_cdb6len;	/* Not for FC */
	u_int8_t		le_cdb7len;	/* Not for FC */
	u_int16_t		le_timeout;
	u_int16_t		le_reserved[20];
} lun_entry_t;

/*
 * le_flags values
 */
#define LUN_TQAE	0x00000001	/* Tagged Queue Action Enable */
#define LUN_DSSM	0x01000000	/* Disable Sending SDP Message */
#define LUN_DM		0x40000000	/* Disconnects Mandatory */

/*
 * le_ops values
 */
#define LUN_CCINCR	0x01	/* increment command count */
#define LUN_CCDECR	0x02	/* decrement command count */
#define LUN_ININCR	0x40	/* increment immed. notify count */
#define LUN_INDECR	0x80	/* decrement immed. notify count */

/*
 * le_status values
 */
#define LUN_ERR		0x04	/* request completed with error */
#define LUN_INVAL	0x06	/* invalid request */
#define LUN_NOCAP	0x16	/* can't provide requested capability */
#define LUN_ENABLED	0x3E	/* LUN already enabled */

/*
 * Immediate Notify Entry structure
 */
#define IN_MSGLEN	8	/* 8 bytes */
#define IN_RSVDLEN	8	/* 8 words */
typedef struct {
	isphdr_t	in_header;
	u_int32_t	in_reserved2;
	u_int8_t	in_lun;			/* lun */
	u_int8_t	in_iid;			/* initiator */
	u_int8_t	in_rsvd;
	u_int8_t	in_tgt;			/* target */
	u_int32_t	in_flags;
	u_int8_t	in_status;
	u_int8_t	in_rsvd2;
	u_int8_t	in_tag_val;		/* tag value */
	u_int8_t	in_tag_type;		/* tag type */
	u_int16_t	in_seqid;		/* sequence id */
	u_int8_t	in_msg[IN_MSGLEN];	/* SCSI message bytes */
	u_int16_t	in_reserved[IN_RSVDLEN];
	u_int8_t	in_sense[SUGGSENSELEN];	/* suggested sense data */
} in_entry_t;

typedef struct {
	isphdr_t	in_header;
	u_int32_t	in_reserved2;
	u_int8_t	in_lun;		/* lun */
	u_int8_t	in_iid;		/* initiator */
	u_int16_t	in_rsvd;
	u_int32_t	in_rsvd2;
	u_int16_t	in_status;
	u_int16_t	in_task_flags;
	u_int16_t	in_seqid;	/* sequence id */
} in_fcentry_t;

/*
 * Values for the in_status field
 */
#define IN_NO_RCAP	0x16	/* requested capability not available */
#define IN_IDE_RECEIVED	0x33	/* Initiator Detected Error msg received */
#define IN_RSRC_UNAVAIL	0x34	/* resource unavailable */
#define IN_MSG_RECEIVED	0x36	/* SCSI message received */
#define	IN_PORT_LOGOUT	0x29	/* port has logged out (FC) */
#define	IN_ABORT_TASK	0x20	/* task named in RX_ID is being aborted (FC) */

/*
 * Notify Acknowledge Entry structure
 */
#define NA_RSVDLEN	22
typedef struct {
	isphdr_t	na_header;
	u_int32_t	na_reserved2;
	u_int8_t	na_lun;		/* lun */
	u_int8_t	na_iid;		/* initiator */
	u_int8_t	na_rsvd;
	u_int8_t	na_tgt;		/* target */
	u_int32_t	na_flags;
	u_int8_t	na_status;
	u_int8_t	na_event;
	u_int16_t	na_seqid;	/* sequence id */
	u_int16_t	na_reserved[NA_RSVDLEN];
} na_entry_t;

/*
 * Value for the na_event field
 */
#define NA_RST_CLRD	0x80	/* Clear an async event notification */

#define	NA2_RSVDLEN	21
typedef struct {
	isphdr_t	na_header;
	u_int32_t	na_reserved2;
	u_int8_t	na_lun;		/* lun */
	u_int8_t	na_iid;		/* initiator */
	u_int16_t	na_rsvd;
	u_int16_t	na_flags;
	u_int16_t	na_rsvd2;
	u_int16_t	na_status;
	u_int16_t	na_task_flags;
	u_int16_t	na_seqid;	/* sequence id */
	u_int16_t	na_reserved[NA2_RSVDLEN];
} na_fcentry_t;
#define	NAFC_RST_CLRD	0x40

/*
 * Value for the na_event field
 */
#define NA_RST_CLRD	0x80	/* Clear an async event notification */
/*
 * Accept Target I/O Entry structure
 */
#define ATIO_CDBLEN	26

typedef struct {
	isphdr_t	at_header;
	u_int32_t	at_reserved2;
	u_int8_t	at_lun;			/* lun */
	u_int8_t	at_iid;			/* initiator */
	u_int8_t	at_cdblen;	 	/* cdb length */
	u_int8_t	at_tgt;			/* target */
	u_int32_t	at_flags;	
	u_int8_t	at_status;		/* firmware status */
	u_int8_t	at_scsi_status;		/* scsi status */
	u_int8_t	at_tag_val;		/* tag value */
	u_int8_t	at_tag_type;		/* tag type */
	u_int8_t	at_cdb[ATIO_CDBLEN];	/* received CDB */
	u_int8_t	at_sense[SUGGSENSELEN];	/* suggested sense data */
} at_entry_t;

/*
 * at_flags values
 */
#define AT_NODISC	0x00008000	/* disconnect disabled */
#define AT_TQAE		0x00000001	/* Tagged Queue Action enabled */

/*
 * at_status values
 */
#define AT_PATH_INVALID	0x07	/* ATIO sent to firmware for disabled lun */
#define AT_PHASE_ERROR	0x14	/* Bus phase sequence error */
#define AT_NOCAP	0x16	/* Requested capability not available */
#define AT_BDR_MSG	0x17	/* Bus Device Reset msg received */
#define AT_CDB		0x3D	/* CDB received */

/*
 * Accept Target I/O Entry structure, Type 2
 */
#define ATIO2_CDBLEN	16

typedef struct {
	isphdr_t	at_header;	
	u_int32_t	at_reserved2;
	u_int8_t	at_lun;			/* lun */
	u_int8_t	at_iid;			/* initiator */
	u_int16_t	at_rxid;	 	/* response ID */
	u_int16_t	at_flags;
	u_int16_t	at_status;		/* firmware status */
	u_int8_t	at_reserved1;
	u_int8_t	at_taskcodes;
	u_int8_t	at_taskflags;
	u_int8_t	at_execodes;
	u_int8_t	at_cdb[ATIO2_CDBLEN];	/* received CDB */
	u_int32_t	at_datalen;		/* allocated data len */
	u_int16_t	at_scclun;
	u_int16_t	at_reserved3;
	u_int16_t	at_scsi_status;
	u_int8_t	at_sense[SUGGSENSELEN];	/* suggested sense data */
} at2_entry_t;

#define	ATIO2_TC_ATTR_MASK	0x7
#define	ATIO2_TC_ATTR_SIMPLEQ	0
#define	ATIO2_TC_ATTR_HEADOFQ	1
#define	ATIO2_TC_ATTR_ORDERED	2
#define	ATIO2_TC_ATTR_ACAQ	4
#define	ATIO2_TC_ATTR_UNTAGGED	5
#define	TC2TT(code)	\
	(((code) == ATIO2_TC_ATTR_SIMPLEQ)? 0x20 : \
	(((code) == ATIO2_TC_ATTR_HEADOFQ)? 0x21 : \
	(((code) == ATIO2_TC_ATTR_ORDERED)? 0x22 : \
	(((code) == ATIO2_TC_ATTR_ACAQ)? 0x24 : 0))))
	     

/*
 * Continue Target I/O Entry structure
 * Request from driver. The response from the
 * ISP firmware is the same except that the last 18
 * bytes are overwritten by suggested sense data if
 * the 'autosense valid' bit is set in the status byte.
 */
typedef struct {
	isphdr_t	ct_header;
	u_int32_t	ct_reserved;
	u_int8_t	ct_lun;		/* lun */
	u_int8_t	ct_iid;		/* initiator id */
	u_int8_t	ct_rsvd;
	u_int8_t	ct_tgt;		/* our target id */
	u_int32_t	ct_flags;
	u_int8_t 	ct_status;	/* isp status */
	u_int8_t 	ct_scsi_status;	/* scsi status */
	u_int8_t 	ct_tag_val;	/* tag value */
	u_int8_t 	ct_tag_type;	/* tag type */
	u_int32_t	ct_xfrlen;	/* transfer length */
	u_int32_t	ct_resid;	/* residual length */
	u_int16_t	ct_timeout;
	u_int16_t	ct_seg_count;
	ispds_t		ct_dataseg[ISP_RQDSEG];
} ct_entry_t;

/*
 * ct_flags values
 */
#define CT_TQAE		0x00000001	/* Tagged Queue Action enable */
#define CT_DATA_IN	0x00000040	/* Data direction */
#define CT_DATA_OUT	0x00000080	/* Data direction */
#define CT_NO_DATA	0x000000C0	/* Data direction */
#define CT_DATAMASK	0x000000C0	/* Data direction */
#define CT_NODISC	0x00008000	/* Disconnects disabled */
#define CT_DSDP		0x01000000	/* Disable Save Data Pointers */
#define CT_SENDRDP	0x04000000	/* Send Restore Pointers msg */
#define CT_SENDSTATUS	0x80000000	/* Send SCSI status byte */

/*
 * ct_status values
 * - set by the firmware when it returns the CTIO
 */
#define CT_OK		0x01	/* completed without error */
#define CT_ABORTED	0x02	/* aborted by host */
#define CT_ERR		0x04	/* see sense data for error */
#define CT_INVAL	0x06	/* request for disabled lun */
#define CT_NOPATH	0x07	/* invalid ITL nexus */
#define	CT_INVRXID	0x08	/* (FC only) Invalid RX_ID */
#define CT_RSELTMO	0x0A	/* reselection timeout after 2 tries */
#define CT_TIMEOUT	0x0B	/* timed out */
#define CT_RESET	0x0E	/* SCSI Bus Reset occurred */
#define CT_PHASE_ERROR	0x14	/* Bus phase sequence error */
#define CT_BDR_MSG	0x17	/* Bus Device Reset msg received */
#define CT_TERMINATED	0x19	/* due to Terminate Transfer mbox cmd */
#define	CT_LOGOUT	0x29	/* port logout not acknowledged yet */
#define CT_NOACK	0x35	/* Outstanding Immed. Notify. entry */

/*
 * When the firmware returns a CTIO entry, it may overwrite the last
 * part of the structure with sense data. This starts at offset 0x2E
 * into the entry, which is in the middle of ct_dataseg[1]. Rather
 * than define a new struct for this, I'm just using the sense data
 * offset.
 */
#define CTIO_SENSE_OFFSET	0x2E

/*
 * Entry length in u_longs. All entries are the same size so
 * any one will do as the numerator.
 */
#define UINT32_ENTRY_SIZE	(sizeof(at_entry_t)/sizeof(u_int32_t))

/*
 * QLA2100 CTIO (type 2) entry
 */
#define	MAXRESPLEN	26
typedef struct {
	isphdr_t	ct_header;
	u_int32_t	ct_reserved;
	u_int8_t	ct_lun;		/* lun */
	u_int8_t	ct_iid;		/* initiator id */
	u_int16_t	ct_rxid;	 /* response ID */
	u_int16_t	ct_flags;
	u_int16_t 	ct_status;	/* isp status */
	u_int16_t	ct_timeout;
	u_int16_t	ct_seg_count;
	u_int32_t	ct_reloff;	/* relative offset */
	u_int32_t	ct_resid;	/* residual length */
	union {
		/*
		 * The three different modes that the target driver
		 * can set the CTIO2 up as.
		 *
		 * The first is for sending FCP_DATA_IUs as well as
		 * (optionally) sending a terminal SCSI status FCP_RSP_IU.
		 *
		 * The second is for sending SCSI sense data in an FCP_RSP_IU.
		 * Note that no FCP_DATA_IUs will be sent.
		 *
		 * The third is for sending FCP_RSP_IUs as built specifically
		 * in system memory as located by the isp_dataseg.
		 */
		struct {
			u_int32_t _reserved;
			u_int16_t _reserved2;
			u_int16_t ct_scsi_status;
			u_int32_t ct_xfrlen;
			ispds_t   ct_dataseg[ISP_RQDSEG_T2];
		} m0;
		struct {
			u_int16_t _reserved;
			u_int16_t _reserved2;
			u_int16_t ct_senselen;
			u_int16_t ct_scsi_status;
			u_int16_t ct_resplen;
			u_int8_t  ct_resp[MAXRESPLEN];
		} m1;
		struct {
			u_int32_t _reserved;
			u_int16_t _reserved2;
			u_int16_t _reserved3;
			u_int32_t ct_datalen;
			ispds_t ct_fcp_rsp_iudata;
		} m2;
		/*
		 * CTIO2 returned from F/W...
		 */
		struct {
			u_int32_t _reserved[4];
			u_int16_t ct_scsi_status;
			u_int8_t  ct_sense[SUGGSENSELEN];
		} fw;
	} rsp;
} ct2_entry_t;
/*
 * ct_flags values for CTIO2
 */
#define	CT2_FLAG_MMASK	0x0003
#define	CT2_FLAG_MODE0	0x0000
#define	CT2_FLAG_MODE1	0x0001
#define	CT2_FLAG_MODE2	0x0002
#define CT2_DATA_IN	CT_DATA_IN
#define CT2_DATA_OUT	CT_DATA_OUT
#define CT2_NO_DATA	CT_NO_DATA
#define CT2_DATAMASK	CT_DATA_MASK
#define	CT2_CCINCR	0x0100
#define	CT2_FASTPOST	0x0200
#define CT2_SENDSTATUS	0x8000

/*
 * ct_status values are (mostly) the same as that for ct_entry.
 */

/*
 * ct_scsi_status values- the low 8 bits are the normal SCSI status
 * we know and love. The upper 8 bits are validity markers for FCP_RSP_IU
 * fields.
 */
#define	CT2_RSPLEN_VALID	0x0100
#define	CT2_SNSLEN_VALID	0x0200
#define	CT2_DATA_OVER		0x0400
#define	CT2_DATA_UNDER		0x0800

#endif	/* _ISPMBOX_H */
