/* $FreeBSD: src/sys/dev/isp/isp_target.h,v 1.30 2007/03/10 02:39:54 mjacob Exp $ */
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
 * Qlogic Target Mode Structure and Flag Definitions
 */
#ifndef	_ISP_TARGET_H
#define	_ISP_TARGET_H

#define	QLTM_SENSELEN	18	/* non-FC cards only */
#define QLTM_SVALID	0x80

/*
 * Structure for Enable Lun and Modify Lun queue entries
 */
typedef struct {
	isphdr_t	le_header;
	uint32_t	le_reserved;
	uint8_t		le_lun;
	uint8_t		le_rsvd;
	uint8_t		le_ops;		/* Modify LUN only */
	uint8_t		le_tgt;		/* Not for FC */
	uint32_t	le_flags;	/* Not for FC */
	uint8_t		le_status;
	uint8_t		le_reserved2;
	uint8_t		le_cmd_count;
	uint8_t		le_in_count;
	uint8_t		le_cdb6len;	/* Not for FC */
	uint8_t		le_cdb7len;	/* Not for FC */
	uint16_t	le_timeout;
	uint16_t	le_reserved3[20];
} lun_entry_t;

/*
 * le_flags values
 */
#define LUN_TQAE	0x00000002	/* bit1  Tagged Queue Action Enable */
#define LUN_DSSM	0x01000000	/* bit24 Disable Sending SDP Message */
#define	LUN_DISAD	0x02000000	/* bit25 Disable autodisconnect */
#define LUN_DM		0x40000000	/* bit30 Disconnects Mandatory */

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
#define	LUN_OK		0x01	/* we be rockin' */
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
	uint32_t	in_reserved;
	uint8_t		in_lun;		/* lun */
	uint8_t		in_iid;		/* initiator */
	uint8_t		in_reserved2;
	uint8_t		in_tgt;		/* target */
	uint32_t	in_flags;
	uint8_t		in_status;
	uint8_t		in_rsvd2;
	uint8_t		in_tag_val;	/* tag value */
	uint8_t		in_tag_type;	/* tag type */
	uint16_t	in_seqid;	/* sequence id */
	uint8_t		in_msg[IN_MSGLEN];	/* SCSI message bytes */
	uint16_t	in_reserved3[IN_RSVDLEN];
	uint8_t		in_sense[QLTM_SENSELEN];/* suggested sense data */
} in_entry_t;

typedef struct {
	isphdr_t	in_header;
	uint32_t	in_reserved;
	uint8_t		in_lun;		/* lun */
	uint8_t		in_iid;		/* initiator */
	uint16_t	in_scclun;
	uint32_t	in_reserved2;
	uint16_t	in_status;
	uint16_t	in_task_flags;
	uint16_t	in_seqid;	/* sequence id */
} in_fcentry_t;

typedef struct {
	isphdr_t	in_header;
	uint32_t	in_reserved;
	uint16_t	in_iid;		/* initiator */
	uint16_t	in_scclun;
	uint32_t	in_reserved2;
	uint16_t	in_status;
	uint16_t	in_task_flags;
	uint16_t	in_seqid;	/* sequence id */
} in_fcentry_e_t;

/*
 * Values for the in_status field
 */
#define	IN_REJECT	0x0D	/* Message Reject message received */
#define IN_RESET	0x0E	/* Bus Reset occurred */
#define IN_NO_RCAP	0x16	/* requested capability not available */
#define IN_IDE_RECEIVED	0x33	/* Initiator Detected Error msg received */
#define IN_RSRC_UNAVAIL	0x34	/* resource unavailable */
#define IN_MSG_RECEIVED	0x36	/* SCSI message received */
#define	IN_ABORT_TASK	0x20	/* task named in RX_ID is being aborted (FC) */
#define	IN_PORT_LOGOUT	0x29	/* port has logged out (FC) */
#define	IN_PORT_CHANGED	0x2A	/* port changed */
#define	IN_GLOBAL_LOGO	0x2E	/* all ports logged out */
#define	IN_NO_NEXUS	0x3B	/* Nexus not established */

/*
 * Values for the in_task_flags field- should only get one at a time!
 */
#define	TASK_FLAGS_RESERVED_MASK	(0xe700)
#define	TASK_FLAGS_CLEAR_ACA		(1<<14)
#define	TASK_FLAGS_TARGET_RESET		(1<<13)
#define	TASK_FLAGS_LUN_RESET		(1<<12)
#define	TASK_FLAGS_CLEAR_TASK_SET	(1<<10)
#define	TASK_FLAGS_ABORT_TASK_SET	(1<<9)

/*
 * ISP24XX Immediate Notify
 */
typedef struct {
	isphdr_t	in_header;
	uint32_t	in_reserved;
	uint16_t	in_nphdl;
	uint16_t	in_reserved1;
	uint16_t	in_flags;
	uint16_t	in_srr_rxid;
	uint16_t	in_status;
	uint8_t		in_status_subcode;
	uint8_t		in_reserved2;
	uint32_t	in_rxid;
	uint16_t	in_srr_reloff_lo;
	uint16_t	in_srr_reloff_hi;
	uint16_t	in_srr_iu;
	uint16_t	in_srr_oxid;
	uint8_t		in_reserved3[18];
	uint8_t		in_reserved4;
	uint8_t		in_vpindex;
	uint32_t	in_reserved5;
	uint16_t	in_portid_lo;
	uint8_t		in_portid_hi;
	uint8_t		in_reserved6;
	uint16_t	in_reserved7;
	uint16_t	in_oxid;
} in_fcentry_24xx_t;

#define	IN24XX_FLAG_PUREX_IOCB		0x1
#define	IN24XX_FLAG_GLOBAL_LOGOUT	0x2

#define	IN24XX_LIP_RESET	0x0E
#define	IN24XX_LINK_RESET	0x0F
#define	IN24XX_PORT_LOGOUT	0x29
#define	IN24XX_PORT_CHANGED	0x2A
#define	IN24XX_LINK_FAILED	0x2E
#define	IN24XX_SRR_RCVD		0x45
#define	IN24XX_ELS_RCVD		0x46	/*
					 * login-affectin ELS received- check
					 * subcode for specific opcode
					 */
/*
 * Notify Acknowledge Entry structure
 */
#define NA_RSVDLEN	22
typedef struct {
	isphdr_t	na_header;
	uint32_t	na_reserved;
	uint8_t		na_lun;		/* lun */
	uint8_t		na_iid;		/* initiator */
	uint8_t		na_reserved2;
	uint8_t		na_tgt;		/* target */
	uint32_t	na_flags;
	uint8_t		na_status;
	uint8_t		na_event;
	uint16_t	na_seqid;	/* sequence id */
	uint16_t	na_reserved3[NA_RSVDLEN];
} na_entry_t;

/*
 * Value for the na_event field
 */
#define NA_RST_CLRD	0x80	/* Clear an async event notification */
#define	NA_OK		0x01	/* Notify Acknowledge Succeeded */
#define	NA_INVALID	0x06	/* Invalid Notify Acknowledge */

#define	NA2_RSVDLEN	21
typedef struct {
	isphdr_t	na_header;
	uint32_t	na_reserved;
	uint8_t		na_reserved1;
	uint8_t		na_iid;		/* initiator loop id */
	uint16_t	na_response;
	uint16_t	na_flags;
	uint16_t	na_reserved2;
	uint16_t	na_status;
	uint16_t	na_task_flags;
	uint16_t	na_seqid;	/* sequence id */
	uint16_t	na_reserved3[NA2_RSVDLEN];
} na_fcentry_t;

typedef struct {
	isphdr_t	na_header;
	uint32_t	na_reserved;
	uint16_t	na_iid;		/* initiator loop id */
	uint16_t	na_response;	/* response code */
	uint16_t	na_flags;
	uint16_t	na_reserved2;
	uint16_t	na_status;
	uint16_t	na_task_flags;
	uint16_t	na_seqid;	/* sequence id */
	uint16_t	na_reserved3[NA2_RSVDLEN];
} na_fcentry_e_t;

#define	NAFC_RCOUNT	0x80	/* increment resource count */
#define NAFC_RST_CLRD	0x20	/* Clear LIP Reset */
#define	NAFC_TVALID	0x10	/* task mangement response code is valid */

/*
 * ISP24XX Notify Acknowledge
 */

typedef struct {
	isphdr_t	na_header;
	uint32_t	na_handle;
	uint16_t	na_nphdl;
	uint16_t	na_reserved1;
	uint16_t	na_flags;
	uint16_t	na_srr_rxid;
	uint16_t	na_status;
	uint8_t		na_status_subcode;
	uint8_t		na_reserved2;
	uint32_t	na_rxid;
	uint16_t	na_srr_reloff_lo;
	uint16_t	na_srr_reloff_hi;
	uint16_t	na_srr_iu;
	uint16_t	na_srr_flags;
	uint8_t		na_reserved3[18];
	uint8_t		na_reserved4;
	uint8_t		na_vpindex;
	uint8_t		na_srr_reject_vunique;
	uint8_t		na_srr_reject_explanation;
	uint8_t		na_srr_reject_code;
	uint8_t		na_reserved5;
	uint8_t		na_reserved6[6];
	uint16_t	na_oxid;
} na_fcentry_24xx_t;

/*
 * Accept Target I/O Entry structure
 */
#define ATIO_CDBLEN	26

typedef struct {
	isphdr_t	at_header;
	uint16_t	at_reserved;
	uint16_t	at_handle;
	uint8_t		at_lun;		/* lun */
	uint8_t		at_iid;		/* initiator */
	uint8_t		at_cdblen; 	/* cdb length */
	uint8_t		at_tgt;		/* target */
	uint32_t	at_flags;
	uint8_t		at_status;	/* firmware status */
	uint8_t		at_scsi_status;	/* scsi status */
	uint8_t		at_tag_val;	/* tag value */
	uint8_t		at_tag_type;	/* tag type */
	uint8_t		at_cdb[ATIO_CDBLEN];	/* received CDB */
	uint8_t		at_sense[QLTM_SENSELEN];/* suggested sense data */
} at_entry_t;

/*
 * at_flags values
 */
#define AT_NODISC	0x00008000	/* disconnect disabled */
#define AT_TQAE		0x00000002	/* Tagged Queue Action enabled */

/*
 * at_status values
 */
#define AT_PATH_INVALID	0x07	/* ATIO sent to firmware for disabled lun */
#define	AT_RESET	0x0E	/* SCSI Bus Reset Occurred */
#define AT_PHASE_ERROR	0x14	/* Bus phase sequence error */
#define AT_NOCAP	0x16	/* Requested capability not available */
#define AT_BDR_MSG	0x17	/* Bus Device Reset msg received */
#define AT_CDB		0x3D	/* CDB received */
/*
 * Macros to create and fetch and test concatenated handle and tag value macros
 */

#define	AT_MAKE_TAGID(tid, bus, inst, aep)				\
	tid = aep->at_handle;						\
	if (aep->at_flags & AT_TQAE) {					\
		tid |= (aep->at_tag_val << 16);				\
		tid |= (1 << 24);					\
	}								\
	tid |= (bus << 25);						\
	tid |= (inst << 26)

#define	CT_MAKE_TAGID(tid, bus, inst, ct)				\
	tid = ct->ct_fwhandle;						\
	if (ct->ct_flags & CT_TQAE) {					\
		tid |= (ct->ct_tag_val << 16);				\
		tid |= (1 << 24);					\
	}								\
	tid |= ((bus & 0x1) << 25);					\
	tid |= (inst << 26)

#define	AT_HAS_TAG(val)		((val) & (1 << 24))
#define	AT_GET_TAG(val)		(((val) >> 16) & 0xff)
#define	AT_GET_INST(val)	(((val) >> 26) & 0x3f)
#define	AT_GET_BUS(val)		(((val) >> 25) & 0x1)
#define	AT_GET_HANDLE(val)	((val) & 0xffff)

#define	IN_MAKE_TAGID(tid, bus, inst, inp)				\
	tid = inp->in_seqid;						\
	tid |= (inp->in_tag_val << 16);					\
	tid |= (1 << 24);						\
	tid |= (bus << 25);						\
	tid |= (inst << 26)

#define	TAG_INSERT_INST(tid, inst)					\
	tid &= ~(0x3ffffff);						\
	tid |= (inst << 26)

#define	TAG_INSERT_BUS(tid, bus)					\
	tid &= ~(1 << 25);						\
	tid |= (bus << 25)

/*
 * Accept Target I/O Entry structure, Type 2
 */
#define ATIO2_CDBLEN	16

typedef struct {
	isphdr_t	at_header;
	uint32_t	at_reserved;
	uint8_t		at_lun;		/* lun or reserved */
	uint8_t		at_iid;		/* initiator */
	uint16_t	at_rxid; 	/* response ID */
	uint16_t	at_flags;
	uint16_t	at_status;	/* firmware status */
	uint8_t		at_crn;		/* command reference number */
	uint8_t		at_taskcodes;
	uint8_t		at_taskflags;
	uint8_t		at_execodes;
	uint8_t		at_cdb[ATIO2_CDBLEN];	/* received CDB */
	uint32_t	at_datalen;		/* allocated data len */
	uint16_t	at_scclun;		/* SCC Lun or reserved */
	uint16_t	at_wwpn[4];		/* WWPN of initiator */
	uint16_t	at_reserved2[6];
	uint16_t	at_oxid;
} at2_entry_t;

typedef struct {
	isphdr_t	at_header;
	uint32_t	at_reserved;
	uint16_t	at_iid;		/* initiator */
	uint16_t	at_rxid; 	/* response ID */
	uint16_t	at_flags;
	uint16_t	at_status;	/* firmware status */
	uint8_t		at_crn;		/* command reference number */
	uint8_t		at_taskcodes;
	uint8_t		at_taskflags;
	uint8_t		at_execodes;
	uint8_t		at_cdb[ATIO2_CDBLEN];	/* received CDB */
	uint32_t	at_datalen;		/* allocated data len */
	uint16_t	at_scclun;		/* SCC Lun or reserved */
	uint16_t	at_wwpn[4];		/* WWPN of initiator */
	uint16_t	at_reserved2[6];
	uint16_t	at_oxid;
} at2e_entry_t;

#define	ATIO2_WWPN_OFFSET	0x2A
#define	ATIO2_OXID_OFFSET	0x3E

#define	ATIO2_TC_ATTR_MASK	0x7
#define	ATIO2_TC_ATTR_SIMPLEQ	0
#define	ATIO2_TC_ATTR_HEADOFQ	1
#define	ATIO2_TC_ATTR_ORDERED	2
#define	ATIO2_TC_ATTR_ACAQ	4
#define	ATIO2_TC_ATTR_UNTAGGED	5

#define	ATIO2_EX_WRITE		0x1
#define	ATIO2_EX_READ		0x2
/*
 * Macros to create and fetch and test concatenated handle and tag value macros
 */
#define	AT2_MAKE_TAGID(tid, bus, inst, aep)				\
	tid = aep->at_rxid;						\
	tid |= (((uint64_t)inst) << 32);				\
	tid |= (((uint64_t)bus) << 48)

#define	CT2_MAKE_TAGID(tid, bus, inst, ct)				\
	tid = ct->ct_rxid;						\
	tid |= (((uint64_t)inst) << 32);				\
	tid |= (((uint64_t)(bus & 0xff)) << 48)

#define	AT2_HAS_TAG(val)	1
#define	AT2_GET_TAG(val)	((val) & 0xffffffff)
#define	AT2_GET_INST(val)	((val) >> 32)
#define	AT2_GET_HANDLE		AT2_GET_TAG
#define	AT2_GET_BUS(val)	(((val) >> 48) & 0xff)

#define	FC_HAS_TAG	AT2_HAS_TAG
#define	FC_GET_TAG	AT2_GET_TAG
#define	FC_GET_INST	AT2_GET_INST
#define	FC_GET_HANDLE	AT2_GET_HANDLE

#define	IN_FC_MAKE_TAGID(tid, bus, inst, seqid)				\
	tid = seqid;							\
	tid |= (((uint64_t)inst) << 32);				\
	tid |= (((uint64_t)(bus & 0xff)) << 48)

#define	FC_TAG_INSERT_INST(tid, inst)					\
	tid &= ~0xffff00000000ull;					\
	tid |= (((uint64_t)inst) << 32)

/*
 * 24XX ATIO Definition
 *
 * This is *quite* different from other entry types.
 * First of all, it has its own queue it comes in on.
 *
 * Secondly, it doesn't have a normal header.
 *
 * Thirdly, it's just a passthru of the FCP CMND IU
 * which is recorded in big endian mode.
 */
typedef struct {
	uint8_t		at_type;
	uint8_t		at_count;
	/*
	 * Task attribute in high four bits,
	 * the rest is the FCP CMND IU Length.
	 * NB: the command can extend past the
	 * length for a single queue entry.
	 */
	uint16_t	at_ta_len;
	uint32_t	at_rxid;
	fc_hdr_t	at_hdr;
	fcp_cmnd_iu_t	at_cmnd;
} at7_entry_t;


/*
 * Continue Target I/O Entry structure
 * Request from driver. The response from the
 * ISP firmware is the same except that the last 18
 * bytes are overwritten by suggested sense data if
 * the 'autosense valid' bit is set in the status byte.
 */
typedef struct {
	isphdr_t	ct_header;
	uint16_t	ct_syshandle;
	uint16_t	ct_fwhandle;	/* required by f/w */
	uint8_t		ct_lun;	/* lun */
	uint8_t		ct_iid;	/* initiator id */
	uint8_t		ct_reserved2;
	uint8_t		ct_tgt;	/* our target id */
	uint32_t	ct_flags;
	uint8_t 	ct_status;	/* isp status */
	uint8_t 	ct_scsi_status;	/* scsi status */
	uint8_t 	ct_tag_val;	/* tag value */
	uint8_t 	ct_tag_type;	/* tag type */
	uint32_t	ct_xfrlen;	/* transfer length */
	uint32_t	ct_resid;	/* residual length */
	uint16_t	ct_timeout;
	uint16_t	ct_seg_count;
	ispds_t		ct_dataseg[ISP_RQDSEG];
} ct_entry_t;

/*
 * For some of the dual port SCSI adapters, port (bus #) is reported
 * in the MSbit of ct_iid. Bit fields are a bit too awkward here.
 *
 * Note that this does not apply to FC adapters at all which can and
 * do report IIDs between 0x81 && 0xfe (or 0x7ff) which represent devices
 * that have logged in across a SCSI fabric.
 */
#define	GET_IID_VAL(x)		(x & 0x3f)
#define	GET_BUS_VAL(x)		((x >> 7) & 0x1)
#define	SET_IID_VAL(y, x)	y = ((y & ~0x3f) | (x & 0x3f))
#define	SET_BUS_VAL(y, x)	y = ((y & 0x3f) | ((x & 0x1) << 7))

/*
 * ct_flags values
 */
#define CT_TQAE		0x00000002	/* bit  1, Tagged Queue Action enable */
#define CT_DATA_IN	0x00000040	/* bits 6&7, Data direction */
#define CT_DATA_OUT	0x00000080	/* bits 6&7, Data direction */
#define CT_NO_DATA	0x000000C0	/* bits 6&7, Data direction */
#define	CT_CCINCR	0x00000100	/* bit 8, autoincrement atio count */
#define CT_DATAMASK	0x000000C0	/* bits 6&7, Data direction */
#define	CT_INISYNCWIDE	0x00004000	/* bit 14, Do Sync/Wide Negotiation */
#define CT_NODISC	0x00008000	/* bit 15, Disconnects disabled */
#define CT_DSDP		0x01000000	/* bit 24, Disable Save Data Pointers */
#define CT_SENDRDP	0x04000000	/* bit 26, Send Restore Pointers msg */
#define CT_SENDSTATUS	0x80000000	/* bit 31, Send SCSI status byte */

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
#define	CT_DATA_OVER	0x09	/* (FC only) Data Overrun */
#define CT_RSELTMO	0x0A	/* reselection timeout after 2 tries */
#define CT_TIMEOUT	0x0B	/* timed out */
#define CT_RESET	0x0E	/* SCSI Bus Reset occurred */
#define	CT_PARITY	0x0F	/* Uncorrectable Parity Error */
#define	CT_BUS_ERROR	0x10	/* (FC Only) DMA PCI Error */
#define	CT_PANIC	0x13	/* Unrecoverable Error */
#define CT_PHASE_ERROR	0x14	/* Bus phase sequence error */
#define	CT_DATA_UNDER	0x15	/* (FC only) Data Underrun */
#define CT_BDR_MSG	0x17	/* Bus Device Reset msg received */
#define CT_TERMINATED	0x19	/* due to Terminate Transfer mbox cmd */
#define	CT_PORTUNAVAIL	0x28	/* port not available */
#define	CT_LOGOUT	0x29	/* port logout */
#define	CT_PORTCHANGED	0x2A	/* port changed */
#define	CT_IDE		0x33	/* Initiator Detected Error */
#define CT_NOACK	0x35	/* Outstanding Immed. Notify. entry */
#define	CT_SRR		0x45	/* SRR Received */
#define	CT_LUN_RESET	0x48	/* Lun Reset Received */

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
#define UINT32_ENTRY_SIZE	(sizeof(at_entry_t)/sizeof(uint32_t))

/*
 * QLA2100 CTIO (type 2) entry
 */
#define	MAXRESPLEN	26
typedef struct {
	isphdr_t	ct_header;
	uint32_t	ct_syshandle;
	uint8_t		ct_lun;		/* lun */
	uint8_t		ct_iid;		/* initiator id */
	uint16_t	ct_rxid;	/* response ID */
	uint16_t	ct_flags;
	uint16_t 	ct_status;	/* isp status */
	uint16_t	ct_timeout;
	uint16_t	ct_seg_count;
	uint32_t	ct_reloff;	/* relative offset */
	int32_t		ct_resid;	/* residual length */
	union {
		/*
		 * The three different modes that the target driver
		 * can set the CTIO{2,3,4} up as.
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
			uint32_t _reserved;
			uint16_t _reserved2;
			uint16_t ct_scsi_status;
			uint32_t ct_xfrlen;
			union {
				ispds_t ct_dataseg[ISP_RQDSEG_T2];
				ispds64_t ct_dataseg64[ISP_RQDSEG_T3];
				ispdslist_t ct_dslist;
			} u;
		} m0;
		struct {
			uint16_t _reserved;
			uint16_t _reserved2;
			uint16_t ct_senselen;
			uint16_t ct_scsi_status;
			uint16_t ct_resplen;
			uint8_t  ct_resp[MAXRESPLEN];
		} m1;
		struct {
			uint32_t _reserved;
			uint16_t _reserved2;
			uint16_t _reserved3;
			uint32_t ct_datalen;
			ispds_t ct_fcp_rsp_iudata;
		} m2;
	} rsp;
} ct2_entry_t;

typedef struct {
	isphdr_t	ct_header;
	uint32_t	ct_syshandle;
	uint16_t	ct_iid;		/* initiator id */
	uint16_t	ct_rxid;	/* response ID */
	uint16_t	ct_flags;
	uint16_t 	ct_status;	/* isp status */
	uint16_t	ct_timeout;
	uint16_t	ct_seg_count;
	uint32_t	ct_reloff;	/* relative offset */
	int32_t		ct_resid;	/* residual length */
	union {
		struct {
			uint32_t _reserved;
			uint16_t _reserved2;
			uint16_t ct_scsi_status;
			uint32_t ct_xfrlen;
			union {
				ispds_t ct_dataseg[ISP_RQDSEG_T2];
				ispds64_t ct_dataseg64[ISP_RQDSEG_T3];
				ispdslist_t ct_dslist;
			} u;
		} m0;
		struct {
			uint16_t _reserved;
			uint16_t _reserved2;
			uint16_t ct_senselen;
			uint16_t ct_scsi_status;
			uint16_t ct_resplen;
			uint8_t  ct_resp[MAXRESPLEN];
		} m1;
		struct {
			uint32_t _reserved;
			uint16_t _reserved2;
			uint16_t _reserved3;
			uint32_t ct_datalen;
			ispds_t ct_fcp_rsp_iudata;
		} m2;
	} rsp;
} ct2e_entry_t;

/*
 * ct_flags values for CTIO2
 */
#define	CT2_FLAG_MODE0	0x0000
#define	CT2_FLAG_MODE1	0x0001
#define	CT2_FLAG_MODE2	0x0002
#define		CT2_FLAG_MMASK	0x0003
#define CT2_DATA_IN	0x0040
#define CT2_DATA_OUT	0x0080
#define CT2_NO_DATA	0x00C0
#define 	CT2_DATAMASK	0x00C0
#define	CT2_CCINCR	0x0100
#define	CT2_FASTPOST	0x0200
#define	CT2_CONFIRM	0x2000
#define	CT2_TERMINATE	0x4000
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

/*
 * ISP24XX CTIO
 */
#define	MAXRESPLEN_24XX	24
typedef struct {
	isphdr_t	ct_header;
	uint32_t	ct_syshandle;
	uint16_t	ct_nphdl;	/* status on returned CTIOs */
	uint16_t	ct_timeout;
	uint16_t	ct_seg_count;
	uint8_t		ct_vpindex;
	uint8_t		ct_xflags;
	uint16_t	ct_iid_lo;	/* low 16 bits of portid */
	uint8_t		ct_iid_hi;	/* hi 8 bits of portid */
	uint8_t		ct_reserved;
	uint32_t	ct_rxid;
	uint16_t	ct_senselen;	/* mode 0 only */
	uint16_t	ct_flags;
	int32_t		ct_resid;	/* residual length */
	uint16_t	ct_oxid;
	uint16_t	ct_scsi_status;	/* modes 0 && 1 only */
	union {
		struct {
			uint32_t	reloff;
			uint32_t	reserved0;
			uint32_t	ct_xfrlen;
			uint32_t	reserved1;
			ispds64_t	ds;
		} m0;
		struct {
			uint16_t ct_resplen;
			uint16_t reserved;
			uint8_t  ct_resp[MAXRESPLEN_24XX];
		} m1;
		struct {
			uint32_t reserved0;
			uint32_t ct_datalen;
			uint32_t reserved1;
			ispds64_t ct_fcp_rsp_iudata;
		} m2;
	} rsp;
} ct7_entry_t;

/*
 * ct_flags values for CTIO7
 */
#define CT7_DATA_IN	0x0002
#define CT7_DATA_OUT	0x0001
#define CT7_NO_DATA	0x0000
#define 	CT7_DATAMASK	0x003
#define	CT7_DSD_ENABLE	0x0004
#define	CT7_CONF_STSFD	0x0010
#define	CT7_EXPLCT_CONF	0x0020
#define	CT7_FLAG_MODE0	0x0000
#define	CT7_FLAG_MODE1	0x0040
#define	CT7_FLAG_MODE7	0x0080
#define		CT7_FLAG_MMASK	0x00C0
#define	CT7_FASTPOST	0x0100
#define	CT7_ATTR_MASK	0x1e00	/* task attributes from atio7 */
#define	CT7_CONFIRM	0x2000
#define	CT7_TERMINATE	0x4000
#define CT7_SENDSTATUS	0x8000

/*
 * Type 7 CTIO status codes
 */
#define CT7_OK		0x01	/* completed without error */
#define CT7_ABORTED	0x02	/* aborted by host */
#define CT7_ERR		0x04	/* see sense data for error */
#define CT7_INVAL	0x06	/* request for disabled lun */
#define	CT7_INVRXID	0x08	/* (FC only) Invalid RX_ID */
#define	CT7_DATA_OVER	0x09	/* (FC only) Data Overrun */
#define CT7_TIMEOUT	0x0B	/* timed out */
#define CT7_RESET	0x0E	/* LIP Rset Received */
#define	CT7_BUS_ERROR	0x10	/* DMA PCI Error */
#define	CT7_REASSY_ERR	0x11	/* DMA reassembly error */
#define	CT7_DATA_UNDER	0x15	/* (FC only) Data Underrun */
#define	CT7_PORTUNAVAIL	0x28	/* port not available */
#define	CT7_LOGOUT	0x29	/* port logout */
#define	CT7_PORTCHANGED	0x2A	/* port changed */
#define	CT7_SRR		0x45	/* SRR Received */

/*
 * Other 24XX related target IOCBs
 */

/*
 * ABTS Received
 */
typedef struct {
	isphdr_t	abts_header;
	uint8_t		abts_reserved0[6];
	uint16_t	abts_nphdl;
	uint16_t	abts_reserved1;
	uint16_t	abts_sof;
	uint32_t	abts_rxid_abts;
	uint16_t	abts_did_lo;
	uint8_t		abts_did_hi;
	uint8_t		abts_r_ctl;
	uint16_t	abts_sid_lo;
	uint8_t		abts_sid_hi;
	uint8_t		abts_cs_ctl;
	uint16_t	abts_fs_ctl;
	uint8_t		abts_f_ctl;
	uint8_t		abts_type;
	uint16_t	abts_seq_cnt;
	uint8_t		abts_df_ctl;
	uint8_t		abts_seq_id;
	uint16_t	abts_rx_id;
	uint16_t	abts_ox_id;
	uint32_t	abts_param;
	uint8_t		abts_reserved2[16];
	uint32_t	abts_rxid_task;
} abts_t;

typedef struct {
	isphdr_t	abts_rsp_header;
	uint32_t	abts_rsp_handle;
	uint16_t	abts_rsp_status;
	uint16_t	abts_rsp_nphdl;
	uint16_t	abts_rsp_ctl_flags;
	uint16_t	abts_rsp_sof;
	uint32_t	abts_rsp_rxid_abts;
	uint16_t	abts_rsp_did_lo;
	uint8_t		abts_rsp_did_hi;
	uint8_t		abts_rsp_r_ctl;
	uint16_t	abts_rsp_sid_lo;
	uint8_t		abts_rsp_sid_hi;
	uint8_t		abts_rsp_cs_ctl;
	uint16_t	abts_rsp_f_ctl_lo;
	uint8_t		abts_rsp_f_ctl_hi;
	uint8_t		abts_rsp_type;
	uint16_t	abts_rsp_seq_cnt;
	uint8_t		abts_rsp_df_ctl;
	uint8_t		abts_rsp_seq_id;
	uint16_t	abts_rsp_rx_id;
	uint16_t	abts_rsp_ox_id;
	uint32_t	abts_rsp_param;
	union {
		struct {
			uint16_t reserved;
			uint8_t	last_seq_id;
			uint8_t seq_id_valid;
			uint16_t aborted_rx_id;
			uint16_t aborted_ox_id;
			uint16_t high_seq_cnt;
			uint16_t low_seq_cnt;
			uint8_t reserved2[4];
		} ba_acc;
		struct {
			uint8_t vendor_unique;
			uint8_t	explanation;
			uint8_t reason;
			uint8_t reserved;
			uint8_t reserved2[12];
		} ba_rjt;
		struct {
			uint8_t reserved[8];
			uint32_t subcode1;
			uint32_t subcode2;
		} rsp;
		uint8_t reserved[16];
	} abts_rsp_payload;
	uint32_t	abts_rsp_rxid_task;
} abts_rsp_t;

/* terminate this ABTS exchange */
#define	ISP24XX_ABTS_RSP_TERMINATE	0x01

#define	ISP24XX_ABTS_RSP_COMPLETE	0x00
#define	ISP24XX_ABTS_RSP_RESET		0x04
#define	ISP24XX_ABTS_RSP_ABORTED	0x05
#define	ISP24XX_ABTS_RSP_TIMEOUT	0x06
#define	ISP24XX_ABTS_RSP_INVXID		0x08
#define	ISP24XX_ABTS_RSP_LOGOUT		0x29
#define	ISP24XX_ABTS_RSP_SUBCODE	0x31

/*
 * Debug macros
 */

#define	ISP_TDQE(isp, msg, idx, arg)	\
    if (isp->isp_dblev & ISP_LOGTDEBUG2) isp_print_qentry(isp, msg, idx, arg)

#ifndef	ISP_TOOLS
/*
 * The functions below are for the publicly available
 * target mode functions that are internal to the Qlogic driver.
 */

/*
 * This function handles new response queue entry appropriate for target mode.
 */
int isp_target_notify(ispsoftc_t *, void *, uint32_t *);

/*
 * This function externalizes the ability to acknowledge an Immediate Notify
 * request.
 */
void isp_notify_ack(ispsoftc_t *, void *);

/*
 * Enable/Disable/Modify a logical unit.
 * (softc, cmd, bus, tgt, lun, cmd_cnt, inotify_cnt, opaque)
 */
#define	DFLT_CMND_CNT	0xfe	/* unmonitored */
#define	DFLT_INOT_CNT	0xfe	/* unmonitored */
int isp_lun_cmd(ispsoftc_t *, int, int, int, int, int, int, uint32_t);

/*
 * General request queue 'put' routine for target mode entries.
 */
int isp_target_put_entry(ispsoftc_t *isp, void *);

/*
 * General routine to put back an ATIO entry-
 * used for replenishing f/w resource counts.
 * The argument is a pointer to a source ATIO
 * or ATIO2.
 */
int isp_target_put_atio(ispsoftc_t *, void *);

/*
 * General routine to send a final CTIO for a command- used mostly for
 * local responses.
 */
int isp_endcmd(ispsoftc_t *, void *, uint32_t, uint32_t);
#define	ECMD_SVALID	0x100

/*
 * Handle an asynchronous event
 *
 * Return nonzero if the interrupt that generated this event has been dismissed.
 */
int isp_target_async(ispsoftc_t *, int, int);
#endif
#endif	/* _ISP_TARGET_H */
