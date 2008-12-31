/* $FreeBSD: src/sys/dev/isp/isp_stds.h,v 1.4.10.1 2008/11/25 02:59:29 kensmith Exp $ */
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
 * Structures that derive directly from public standards.
 */
#ifndef	_ISP_STDS_H
#define	_ISP_STDS_H

/*
 * FC Frame Header
 *
 * Source: dpANS-X3.xxx-199x, section 18 (AKA FC-PH-2)
 *
 */
typedef struct {
	uint8_t		r_ctl;
	uint8_t		d_id[3];
	uint8_t		cs_ctl;
	uint8_t		s_id[3];
	uint8_t		type;
	uint8_t		f_ctl;
	uint8_t		seq_id;
	uint8_t		df_ctl;
	uint16_t	seq_cnt;
	uint16_t	ox_id;
	uint16_t	rx_id;
	uint32_t	parameter;
} fc_hdr_t;

/*
 * FCP_CMND_IU Payload
 *
 * Source: NICTS T10, Project 1144D, Revision 07a, Section 9 (AKA fcp2-r07a)
 *
 * Notes:
 *	When additional cdb length is defined in fcp_cmnd_alen_datadir,
 * 	bits 2..7, the actual cdb length is 16 + ((fcp_cmnd_alen_datadir>>2)*4),
 *	with the datalength following in MSB format just after.
 */
typedef struct {
	uint8_t		fcp_cmnd_lun[8];
	uint8_t		fcp_cmnd_crn;
	uint8_t		fcp_cmnd_task_attribute;
	uint8_t		fcp_cmnd_task_management;
	uint8_t		fcp_cmnd_alen_datadir;
	union {
		struct {
			uint8_t		fcp_cmnd_cdb[16];
			uint32_t	fcp_cmnd_dl;
		} sf;
		struct {
			uint8_t		fcp_cmnd_cdb[1];
		} lf;
	} cdb_dl;
} fcp_cmnd_iu_t;


#define	FCP_CMND_TASK_ATTR_SIMPLE	0x00
#define	FCP_CMND_TASK_ATTR_HEAD		0x01
#define	FCP_CMND_TASK_ATTR_ORDERED	0x02
#define	FCP_CMND_TASK_ATTR_ACA		0x04
#define	FCP_CMND_TASK_ATTR_UNTAGGED	0x05
#define	FCP_CMND_TASK_ATTR_MASK		0x07

#define	FCP_CMND_ADDTL_CDBLEN_SHIFT	2

#define	FCP_CMND_DATA_WRITE		0x01
#define	FCP_CMND_DATA_READ		0x02

#define	FCP_CMND_DATA_DIR_MASK		0x03

#define	FCP_CMND_TMF_CLEAR_ACA		0x40
#define	FCP_CMND_TMF_TGT_RESET		0x20
#define	FCP_CMND_TMF_LUN_RESET		0x10
#define	FCP_CMND_TMF_CLEAR_TASK_SET	0x04
#define	FCP_CMND_TMF_ABORT_TASK_SET	0x02

/*
 * Basic CT IU Header
 *
 * Source: X3.288-199x Generic Services 2 Rev 5.3 (FC-GS-2) Section 4.3.1
 */

typedef struct {
	uint8_t		ct_revision;
	uint8_t		ct_in_id[3];
	uint8_t		ct_fcs_type;
	uint8_t		ct_fcs_subtype;
	uint8_t		ct_options;
	uint8_t		ct_reserved0;
	uint16_t	ct_cmd_resp;
	uint16_t	ct_bcnt_resid;
	uint8_t		ct_reserved1;
	uint8_t		ct_reason;
	uint8_t		ct_explanation;
	uint8_t		ct_vunique;
} ct_hdr_t;
#define	CT_REVISION		1
#define	CT_FC_TYPE_FC		0xFC
#define CT_FC_SUBTYPE_NS	0x02

/*
 * RFT_ID Requet CT_IU
 *
 * Source: NCITS xxx-200x Generic Services- 5 Rev 8.5 Section 5.2.5.30
 */
typedef struct {
	ct_hdr_t	rftid_hdr;
	uint8_t		rftid_reserved;
	uint8_t		rftid_portid[3];
	uint32_t	rftid_fc4types[8];
} rft_id_t;

/*
 * FCP Response Code Definitions
 * Source: NCITS T10, Project 1144D, Revision 07a (aka FCP2r07a)
 */
#define	FCP_RSPNS_CODE_OFFSET		3

#define	FCP_RSPNS_TMF_DONE		0
#define	FCP_RSPNS_DLBRSTX		1
#define	FCP_RSPNS_BADCMND		2
#define	FCP_RSPNS_EROFS			3
#define	FCP_RSPNS_TMF_REJECT		4
#define	FCP_RSPNS_TMF_FAILED		5


/* unconverted miscellany */
/*
 * Basic FC Link Service defines
 */
/*
 * These are in the R_CTL field.
 */
#define	ABTS			0x81
#define	BA_ACC			0x84	/* of ABORT SEQUENCE */
#define	BA_RJT			0x85	/* of ABORT SEQUENCE */

/*
 * Link Service Accept/Reject
 */
#define	LS_ACC			0x8002
#define	LS_RJT			0x8001

/*
 * FC ELS Codes- bits 31-24 of the first payload word of an ELS frame.
 */
#define	PLOGI			0x03
#define	FLOGI			0x04
#define	LOGO			0x05
#define	ABTX			0x06
#define	PRLI			0x20
#define	PRLO			0x21
#define	TPRLO			0x24
#define	RNC			0x53

/*
 * FC4 defines
 */
#define	FC4_IP		5	/* ISO/EEC 8802-2 LLC/SNAP */
#define	FC4_SCSI	8	/* SCSI-3 via Fibre Channel Protocol (FCP) */
#define	FC4_FC_SVC	0x20	/* Fibre Channel Services */

#ifndef	MSG_ABORT
#define	MSG_ABORT		0x06
#endif
#ifndef	MSG_BUS_DEV_RESET
#define	MSG_BUS_DEV_RESET	0x0c
#endif
#ifndef	MSG_ABORT_TAG
#define	MSG_ABORT_TAG		0x0d
#endif
#ifndef	MSG_CLEAR_QUEUE
#define	MSG_CLEAR_QUEUE		0x0e
#endif
#ifndef	MSG_REL_RECOVERY
#define	MSG_REL_RECOVERY	0x10
#endif
#ifndef	MSG_TERM_IO_PROC
#define	MSG_TERM_IO_PROC	0x11
#endif
#ifndef	MSG_LUN_RESET
#define	MSG_LUN_RESET		0x17
#endif

#endif	/* _ISP_STDS_H */
