/* $FreeBSD$ */
/*
 * Qlogic Host Adapter Inline Functions
 *
 * Copyright (c) 1999, 2000, 2001 by Matthew Jacob
 * Feral Software
 * All rights reserved.
 * mjacob@feral.com
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
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
#ifndef	_ISP_INLINE_H
#define	_ISP_INLINE_H

/*
 * Handle Functions.
 * For each outstanding command there will be a non-zero handle.
 * There will be at most isp_maxcmds handles, and isp_lasthdls
 * will be a seed for the last handled allocated.
 */

static INLINE int isp_save_xs(struct ispsoftc *, XS_T *, u_int16_t *);
static INLINE XS_T *isp_find_xs(struct ispsoftc *, u_int16_t);
static INLINE u_int16_t isp_find_handle(struct ispsoftc *, XS_T *);
static INLINE int isp_handle_index(u_int16_t);
static INLINE void isp_destroy_handle(struct ispsoftc *, u_int16_t);
static INLINE void isp_remove_handle(struct ispsoftc *, XS_T *);

static INLINE int
isp_save_xs(struct ispsoftc *isp, XS_T *xs, u_int16_t *handlep)
{
	int i, j;

	for (j = isp->isp_lasthdls, i = 0; i < (int) isp->isp_maxcmds; i++) {
		if (isp->isp_xflist[j] == NULL) {
			break;
		}
		if (++j == isp->isp_maxcmds) {
			j = 0;
		}
	}
	if (i == isp->isp_maxcmds) {
		return (-1);
	}
	isp->isp_xflist[j] = xs;
	*handlep = j+1;
	if (++j == isp->isp_maxcmds)
		j = 0;
	isp->isp_lasthdls = (u_int16_t)j;
	return (0);
}

static INLINE XS_T *
isp_find_xs(struct ispsoftc *isp, u_int16_t handle)
{
	if (handle < 1 || handle > (u_int16_t) isp->isp_maxcmds) {
		return (NULL);
	} else {
		return (isp->isp_xflist[handle - 1]);
	}
}

static INLINE u_int16_t
isp_find_handle(struct ispsoftc *isp, XS_T *xs)
{
	int i;
	if (xs != NULL) {
		for (i = 0; i < isp->isp_maxcmds; i++) {
			if (isp->isp_xflist[i] == xs) {
				return ((u_int16_t) i+1);
			}
		}
	}
	return (0);
}

static INLINE int
isp_handle_index(u_int16_t handle)
{
	return (handle-1);
}

static INLINE void
isp_destroy_handle(struct ispsoftc *isp, u_int16_t handle)
{
	if (handle > 0 && handle <= (u_int16_t) isp->isp_maxcmds) {
		isp->isp_xflist[isp_handle_index(handle)] = NULL;
	}
}

static INLINE void
isp_remove_handle(struct ispsoftc *isp, XS_T *xs)
{
	isp_destroy_handle(isp, isp_find_handle(isp, xs));
}

static INLINE int
isp_getrqentry(struct ispsoftc *, u_int16_t *, u_int16_t *, void **);

static INLINE int
isp_getrqentry(struct ispsoftc *isp, u_int16_t *iptrp,
    u_int16_t *optrp, void **resultp)
{
	volatile u_int16_t iptr, optr;

	optr = isp->isp_reqodx = READ_REQUEST_QUEUE_OUT_POINTER(isp);
	iptr = isp->isp_reqidx;
	*resultp = ISP_QUEUE_ENTRY(isp->isp_rquest, iptr);
	iptr = ISP_NXT_QENTRY(iptr, RQUEST_QUEUE_LEN(isp));
	if (iptr == optr) {
		return (1);
	}
	if (optrp)
		*optrp = optr;
	if (iptrp)
		*iptrp = iptr;
	return (0);
}

static INLINE void isp_print_qentry (struct ispsoftc *, char *, int, void *);


#define	TBA	(4 * (((QENTRY_LEN >> 2) * 3) + 1) + 1)
static INLINE void
isp_print_qentry(struct ispsoftc *isp, char *msg, int idx, void *arg)
{
	char buf[TBA];
	int amt, i, j;
	u_int8_t *ptr = arg;

	isp_prt(isp, ISP_LOGALL, "%s index %d=>", msg, idx);
	for (buf[0] = 0, amt = i = 0; i < 4; i++) {
		buf[0] = 0;
		SNPRINTF(buf, TBA, "  ");
		for (j = 0; j < (QENTRY_LEN >> 2); j++) {
			SNPRINTF(buf, TBA, "%s %02x", buf, ptr[amt++] & 0xff);
		}
		isp_prt(isp, ISP_LOGALL, buf);
	}
}

static INLINE void isp_print_bytes(struct ispsoftc *, char *, int, void *);

static INLINE void
isp_print_bytes(struct ispsoftc *isp, char *msg, int amt, void *arg)
{
	char buf[128];
	u_int8_t *ptr = arg;
	int off;

	if (msg)
		isp_prt(isp, ISP_LOGALL, "%s:", msg);
	off = 0;
	buf[0] = 0;
	while (off < amt) {
		int j, to;
		to = off;
		for (j = 0; j < 16; j++) {
			SNPRINTF(buf, 128, "%s %02x", buf, ptr[off++] & 0xff);
			if (off == amt)
				break;
		}
		isp_prt(isp, ISP_LOGALL, "0x%08x:%s", to, buf);
		buf[0] = 0;
	}
}

/*
 * Do the common path to try and ensure that link is up, we've scanned
 * the fabric (if we're on a fabric), and that we've synchronized this
 * all with our own database and done the appropriate logins.
 *
 * We repeatedly check for firmware state and loop state after each
 * action because things may have changed while we were doing this.
 * Any failure or change of state causes us to return a nonzero value.
 *
 * We honor HBA roles in that if we're not in Initiator mode, we don't
 * attempt to sync up the database (that's for somebody else to do,
 * if ever).
 *
 * We assume we enter here with any locks held.
 */

static INLINE int isp_fc_runstate(struct ispsoftc *, int);

static INLINE int
isp_fc_runstate(struct ispsoftc *isp, int tval)
{
	fcparam *fcp;
	int *tptr;

	if (IS_SCSI(isp) || isp->isp_role == ISP_ROLE_NONE)
		return (0);

	tptr = tval? &tval : NULL;
	if (isp_control(isp, ISPCTL_FCLINK_TEST, tptr) != 0) {
		return (-1);
	}
	fcp = FCPARAM(isp);
	if (fcp->isp_fwstate != FW_READY || fcp->isp_loopstate < LOOP_PDB_RCVD)
		return (-1);
	if (isp_control(isp, ISPCTL_SCAN_FABRIC, NULL) != 0) {
		return (-1);
	}
	if (isp_control(isp, ISPCTL_SCAN_LOOP, NULL) != 0) {
		return (-1);
	}
	if ((isp->isp_role & ISP_ROLE_INITIATOR) == 0) {
		return (0);
	}
	if (isp_control(isp, ISPCTL_PDB_SYNC, NULL) != 0) {
		return (-1);
	}
	if (fcp->isp_fwstate != FW_READY || fcp->isp_loopstate != LOOP_READY) {
		return (-1);
	}
	return (0);
}

/*
 * Functions to move stuff to a form that the QLogic RISC engine understands
 * and functions to move stuff back to a form the processor understands.
 *
 * Each platform is required to provide the 8, 16 and 32 bit
 * swizzle and unswizzle macros (ISP_IOX{PUT|GET}_{8,16,32})
 *
 * The assumption is that swizzling and unswizzling is mostly done 'in place'
 * (with a few exceptions for efficiency).
 */

static INLINE void isp_copy_out_hdr(struct ispsoftc *, isphdr_t *, isphdr_t *);
static INLINE void isp_copy_in_hdr(struct ispsoftc *, isphdr_t *, isphdr_t *);
static INLINE int isp_get_response_type(struct ispsoftc *, isphdr_t *);

static INLINE void
isp_put_request(struct ispsoftc *, ispreq_t *, ispreq_t *);
static INLINE void
isp_put_request_t2(struct ispsoftc *, ispreqt2_t *, ispreqt2_t *);
static INLINE void
isp_put_request_t3(struct ispsoftc *, ispreqt3_t *, ispreqt3_t *);
static INLINE void
isp_put_extended_request(struct ispsoftc *, ispextreq_t *, ispextreq_t *);
static INLINE void
isp_put_cont_req(struct ispsoftc *, ispcontreq_t *, ispcontreq_t *);
static INLINE void
isp_put_cont64_req(struct ispsoftc *, ispcontreq64_t *, ispcontreq64_t *);
static INLINE void
isp_get_response(struct ispsoftc *, ispstatusreq_t *, ispstatusreq_t *);
static INLINE void
isp_get_response_x(struct ispsoftc *, ispstatus_cont_t *, ispstatus_cont_t *);
static INLINE void
isp_get_rio2(struct ispsoftc *, isp_rio2_t *, isp_rio2_t *);
static INLINE void
isp_put_icb(struct ispsoftc *, isp_icb_t *, isp_icb_t *);
static INLINE void
isp_get_pdb(struct ispsoftc *, isp_pdb_t *, isp_pdb_t *);
static INLINE void
isp_get_ct_hdr(struct ispsoftc *isp, ct_hdr_t *, ct_hdr_t *);
static INLINE void
isp_put_sns_request(struct ispsoftc *, sns_screq_t *, sns_screq_t *);
static INLINE void
isp_put_gid_ft_request(struct ispsoftc *, sns_gid_ft_req_t *,
    sns_gid_ft_req_t *);
static INLINE void
isp_put_gxn_id_request(struct ispsoftc *, sns_gxn_id_req_t *,
    sns_gxn_id_req_t *);
static INLINE void
isp_get_sns_response(struct ispsoftc *, sns_scrsp_t *, sns_scrsp_t *, int);
static INLINE void
isp_get_gid_ft_response(struct ispsoftc *, sns_gid_ft_rsp_t *,
    sns_gid_ft_rsp_t *, int);
static INLINE void
isp_get_gxn_id_response(struct ispsoftc *, sns_gxn_id_rsp_t *,
    sns_gxn_id_rsp_t *);
static INLINE void
isp_get_ga_nxt_response(struct ispsoftc *, sns_ga_nxt_rsp_t *,
    sns_ga_nxt_rsp_t *);
#ifdef	ISP_TARGET_MODE
#ifndef	_ISP_TARGET_H
#include "isp_target.h"
#endif
static INLINE void
isp_put_atio(struct ispsoftc *, at_entry_t *, at_entry_t *);
static INLINE void
isp_get_atio(struct ispsoftc *, at_entry_t *, at_entry_t *);
static INLINE void
isp_put_atio2(struct ispsoftc *, at2_entry_t *, at2_entry_t *);
static INLINE void
isp_get_atio2(struct ispsoftc *, at2_entry_t *, at2_entry_t *);
static INLINE void
isp_put_ctio(struct ispsoftc *, ct_entry_t *, ct_entry_t *);
static INLINE void
isp_get_ctio(struct ispsoftc *, ct_entry_t *, ct_entry_t *);
static INLINE void
isp_put_ctio2(struct ispsoftc *, ct2_entry_t *, ct2_entry_t *);
static INLINE void
isp_get_ctio2(struct ispsoftc *, ct2_entry_t *, ct2_entry_t *);
static INLINE void
isp_put_enable_lun(struct ispsoftc *, lun_entry_t *, lun_entry_t *);
static INLINE void
isp_get_enable_lun(struct ispsoftc *, lun_entry_t *, lun_entry_t *);
static INLINE void
isp_put_notify(struct ispsoftc *, in_entry_t *, in_entry_t *);
static INLINE void
isp_get_notify(struct ispsoftc *, in_entry_t *, in_entry_t *);
static INLINE void
isp_put_notify_fc(struct ispsoftc *, in_fcentry_t *, in_fcentry_t *);
static INLINE void
isp_get_notify_fc(struct ispsoftc *, in_fcentry_t *, in_fcentry_t *);
static INLINE void
isp_put_notify_ack(struct ispsoftc *, na_entry_t *, na_entry_t *);
static INLINE void
isp_get_notify_ack(struct ispsoftc *, na_entry_t *, na_entry_t *);
static INLINE void
isp_put_notify_ack_fc(struct ispsoftc *, na_fcentry_t *, na_fcentry_t *);
static INLINE void
isp_get_notify_ack_fc(struct ispsoftc *, na_fcentry_t *, na_fcentry_t *);
#endif

#define	ISP_IS_SBUS(isp)	\
	(ISP_SBUS_SUPPORTED && (isp)->isp_bustype == ISP_BT_SBUS)

/*
 * Swizzle/Copy Functions
 */
static INLINE void
isp_copy_out_hdr(struct ispsoftc *isp, isphdr_t *hpsrc, isphdr_t *hpdst)
{
	if (ISP_IS_SBUS(isp)) {
		ISP_IOXPUT_8(isp, hpsrc->rqs_entry_type,
		    &hpdst->rqs_entry_count);
		ISP_IOXPUT_8(isp, hpsrc->rqs_entry_count,
		    &hpdst->rqs_entry_type);
		ISP_IOXPUT_8(isp, hpsrc->rqs_seqno,
		    &hpdst->rqs_flags);
		ISP_IOXPUT_8(isp, hpsrc->rqs_flags,
		    &hpdst->rqs_seqno);
	} else {
		ISP_IOXPUT_8(isp, hpsrc->rqs_entry_type,
		    &hpdst->rqs_entry_type);
		ISP_IOXPUT_8(isp, hpsrc->rqs_entry_count,
		    &hpdst->rqs_entry_count);
		ISP_IOXPUT_8(isp, hpsrc->rqs_seqno,
		    &hpdst->rqs_seqno);
		ISP_IOXPUT_8(isp, hpsrc->rqs_flags,
		    &hpdst->rqs_flags);
	}
}

static INLINE void
isp_copy_in_hdr(struct ispsoftc *isp, isphdr_t *hpsrc, isphdr_t *hpdst)
{
	if (ISP_IS_SBUS(isp)) {
		ISP_IOXGET_8(isp, &hpsrc->rqs_entry_type,
		    hpdst->rqs_entry_count);
		ISP_IOXGET_8(isp, &hpsrc->rqs_entry_count,
		    hpdst->rqs_entry_type);
		ISP_IOXGET_8(isp, &hpsrc->rqs_seqno,
		    hpdst->rqs_flags);
		ISP_IOXGET_8(isp, &hpsrc->rqs_flags,
		    hpdst->rqs_seqno);
	} else {
		ISP_IOXGET_8(isp, &hpsrc->rqs_entry_type,
		    hpdst->rqs_entry_type);
		ISP_IOXGET_8(isp, &hpsrc->rqs_entry_count,
		    hpdst->rqs_entry_count);
		ISP_IOXGET_8(isp, &hpsrc->rqs_seqno,
		    hpdst->rqs_seqno);
		ISP_IOXGET_8(isp, &hpsrc->rqs_flags,
		    hpdst->rqs_flags);
	}
}

static INLINE int
isp_get_response_type(struct ispsoftc *isp, isphdr_t *hp)
{
	u_int8_t type;
	if (ISP_IS_SBUS(isp)) {
		ISP_IOXGET_8(isp, &hp->rqs_entry_count, type);
	} else {
		ISP_IOXGET_8(isp, &hp->rqs_entry_type, type);
	}
	return ((int)type);
}

static INLINE void
isp_put_request(struct ispsoftc *isp, ispreq_t *rqsrc, ispreq_t *rqdst)
{
	int i;
	isp_copy_out_hdr(isp, &rqsrc->req_header, &rqdst->req_header);
	ISP_IOXPUT_32(isp, rqsrc->req_handle, &rqdst->req_handle);
	if (ISP_IS_SBUS(isp)) {
		ISP_IOXPUT_8(isp, rqsrc->req_lun_trn, &rqdst->req_target);
		ISP_IOXPUT_8(isp, rqsrc->req_target, &rqdst->req_lun_trn);
	} else {
		ISP_IOXPUT_8(isp, rqsrc->req_lun_trn, &rqdst->req_lun_trn);
		ISP_IOXPUT_8(isp, rqsrc->req_target, &rqdst->req_target);
	}
	ISP_IOXPUT_16(isp, rqsrc->req_cdblen, &rqdst->req_cdblen);
	ISP_IOXPUT_16(isp, rqsrc->req_flags, &rqdst->req_flags);
	ISP_IOXPUT_16(isp, rqsrc->req_time, &rqdst->req_time);
	ISP_IOXPUT_16(isp, rqsrc->req_seg_count, &rqdst->req_seg_count);
	for (i = 0; i < 12; i++) {
		ISP_IOXPUT_8(isp, rqsrc->req_cdb[i], &rqdst->req_cdb[i]);
	}
	for (i = 0; i < ISP_RQDSEG; i++) {
		ISP_IOXPUT_32(isp, rqsrc->req_dataseg[i].ds_base,
		    &rqdst->req_dataseg[i].ds_base);
		ISP_IOXPUT_32(isp, rqsrc->req_dataseg[i].ds_count,
		    &rqdst->req_dataseg[i].ds_count);
	}
}

static INLINE void
isp_put_request_t2(struct ispsoftc *isp, ispreqt2_t *tqsrc, ispreqt2_t *tqdst)
{
	int i;
	isp_copy_out_hdr(isp, &tqsrc->req_header, &tqdst->req_header);
	ISP_IOXPUT_32(isp, tqsrc->req_handle, &tqdst->req_handle);
	ISP_IOXPUT_8(isp, tqsrc->req_lun_trn, &tqdst->req_lun_trn);
	ISP_IOXPUT_8(isp, tqsrc->req_target, &tqdst->req_target);
	ISP_IOXPUT_16(isp, tqsrc->req_scclun, &tqdst->req_scclun);
	ISP_IOXPUT_16(isp, tqsrc->req_flags,  &tqdst->req_flags);
	ISP_IOXPUT_16(isp, tqsrc->_res2, &tqdst->_res2);
	ISP_IOXPUT_16(isp, tqsrc->req_time, &tqdst->req_time);
	ISP_IOXPUT_16(isp, tqsrc->req_seg_count, &tqdst->req_seg_count);
	for (i = 0; i < 16; i++) {
		ISP_IOXPUT_8(isp, tqsrc->req_cdb[i], &tqdst->req_cdb[i]);
	}
	ISP_IOXPUT_32(isp, tqsrc->req_totalcnt, &tqdst->req_totalcnt);
	for (i = 0; i < ISP_RQDSEG_T2; i++) {
		ISP_IOXPUT_32(isp, tqsrc->req_dataseg[i].ds_base,
		    &tqdst->req_dataseg[i].ds_base);
		ISP_IOXPUT_32(isp, tqsrc->req_dataseg[i].ds_count,
		    &tqdst->req_dataseg[i].ds_count);
	}
}

static INLINE void
isp_put_request_t3(struct ispsoftc *isp, ispreqt3_t *tqsrc, ispreqt3_t *tqdst)
{
	int i;
	isp_copy_out_hdr(isp, &tqsrc->req_header, &tqdst->req_header);
	ISP_IOXPUT_32(isp, tqsrc->req_handle, &tqdst->req_handle);
	ISP_IOXPUT_8(isp, tqsrc->req_lun_trn, &tqdst->req_lun_trn);
	ISP_IOXPUT_8(isp, tqsrc->req_target, &tqdst->req_target);
	ISP_IOXPUT_16(isp, tqsrc->req_scclun, &tqdst->req_scclun);
	ISP_IOXPUT_16(isp, tqsrc->req_flags,  &tqdst->req_flags);
	ISP_IOXPUT_16(isp, tqsrc->_res2, &tqdst->_res2);
	ISP_IOXPUT_16(isp, tqsrc->req_time, &tqdst->req_time);
	ISP_IOXPUT_16(isp, tqsrc->req_seg_count, &tqdst->req_seg_count);
	for (i = 0; i < 16; i++) {
		ISP_IOXPUT_8(isp, tqsrc->req_cdb[i], &tqdst->req_cdb[i]);
	}
	ISP_IOXPUT_32(isp, tqsrc->req_totalcnt, &tqdst->req_totalcnt);
	for (i = 0; i < ISP_RQDSEG_T2; i++) {
		ISP_IOXPUT_32(isp, tqsrc->req_dataseg[i].ds_base,
		    &tqdst->req_dataseg[i].ds_base);
		ISP_IOXPUT_32(isp, tqsrc->req_dataseg[i].ds_basehi,
		    &tqdst->req_dataseg[i].ds_basehi);
		ISP_IOXPUT_32(isp, tqsrc->req_dataseg[i].ds_count,
		    &tqdst->req_dataseg[i].ds_count);
	}
}

static INLINE void
isp_put_extended_request(struct ispsoftc *isp, ispextreq_t *xqsrc,
    ispextreq_t *xqdst)
{
	int i;
	isp_copy_out_hdr(isp, &xqsrc->req_header, &xqdst->req_header);
	ISP_IOXPUT_32(isp, xqsrc->req_handle, &xqdst->req_handle);
	if (ISP_IS_SBUS(isp)) {
		ISP_IOXPUT_8(isp, xqsrc->req_lun_trn, &xqdst->req_target);
		ISP_IOXPUT_8(isp, xqsrc->req_target, &xqdst->req_lun_trn);
	} else {
		ISP_IOXPUT_8(isp, xqsrc->req_lun_trn, &xqdst->req_lun_trn);
		ISP_IOXPUT_8(isp, xqsrc->req_target, &xqdst->req_target);
	}
	ISP_IOXPUT_16(isp, xqsrc->req_cdblen, &xqdst->req_cdblen);
	ISP_IOXPUT_16(isp, xqsrc->req_flags, &xqdst->req_flags);
	ISP_IOXPUT_16(isp, xqsrc->req_time, &xqdst->req_time);
	ISP_IOXPUT_16(isp, xqsrc->req_seg_count, &xqdst->req_seg_count);
	for (i = 0; i < 44; i++) {
		ISP_IOXPUT_8(isp, xqsrc->req_cdb[i], &xqdst->req_cdb[i]);
	}
}

static INLINE void
isp_put_cont_req(struct ispsoftc *isp, ispcontreq_t *cqsrc, ispcontreq_t *cqdst)
{
	int i;
	isp_copy_out_hdr(isp, &cqsrc->req_header, &cqdst->req_header);
	for (i = 0; i < ISP_CDSEG; i++) {
		ISP_IOXPUT_32(isp, cqsrc->req_dataseg[i].ds_base,
		    &cqdst->req_dataseg[i].ds_base);
		ISP_IOXPUT_32(isp, cqsrc->req_dataseg[i].ds_count,
		    &cqdst->req_dataseg[i].ds_count);
	}
}

static INLINE void
isp_put_cont64_req(struct ispsoftc *isp, ispcontreq64_t *cqsrc,
    ispcontreq64_t *cqdst)
{
	int i;
	isp_copy_out_hdr(isp, &cqsrc->req_header, &cqdst->req_header);
	for (i = 0; i < ISP_CDSEG64; i++) {
		ISP_IOXPUT_32(isp, cqsrc->req_dataseg[i].ds_base,
		    &cqdst->req_dataseg[i].ds_base);
		ISP_IOXPUT_32(isp, cqsrc->req_dataseg[i].ds_basehi,
		    &cqdst->req_dataseg[i].ds_basehi);
		ISP_IOXPUT_32(isp, cqsrc->req_dataseg[i].ds_count,
		    &cqdst->req_dataseg[i].ds_count);
	}
}

static INLINE void
isp_get_response(struct ispsoftc *isp, ispstatusreq_t *spsrc,
    ispstatusreq_t *spdst)
{
	int i;
	isp_copy_in_hdr(isp, &spsrc->req_header, &spdst->req_header);
	ISP_IOXGET_32(isp, &spsrc->req_handle, spdst->req_handle);
	ISP_IOXGET_16(isp, &spsrc->req_scsi_status, spdst->req_scsi_status);
	ISP_IOXGET_16(isp, &spsrc->req_completion_status,
	    spdst->req_completion_status);
	ISP_IOXGET_16(isp, &spsrc->req_state_flags, spdst->req_state_flags);
	ISP_IOXGET_16(isp, &spsrc->req_status_flags, spdst->req_status_flags);
	ISP_IOXGET_16(isp, &spsrc->req_time, spdst->req_time);
	ISP_IOXGET_16(isp, &spsrc->req_sense_len, spdst->req_sense_len);
	ISP_IOXGET_32(isp, &spsrc->req_resid, spdst->req_resid);
	for (i = 0; i < 8; i++) {
		ISP_IOXGET_8(isp, &spsrc->req_response[i],
		    spdst->req_response[i]);
	}
	for (i = 0; i < 32; i++) {
		ISP_IOXGET_8(isp, &spsrc->req_sense_data[i],
		    spdst->req_sense_data[i]);
	}
}

static INLINE void
isp_get_response_x(struct ispsoftc *isp, ispstatus_cont_t *cpsrc,
    ispstatus_cont_t *cpdst)
{
	int i;
	isp_copy_in_hdr(isp, &cpsrc->req_header, &cpdst->req_header);
	for (i = 0; i < 60; i++) {
		ISP_IOXGET_8(isp, &cpsrc->req_sense_data[i],
		    cpdst->req_sense_data[i]);
	}
}

static INLINE void
isp_get_rio2(struct ispsoftc *isp, isp_rio2_t *r2src, isp_rio2_t *r2dst)
{
	int i;
	isp_copy_in_hdr(isp, &r2src->req_header, &r2dst->req_header);
	if (r2dst->req_header.rqs_seqno > 30)
		r2dst->req_header.rqs_seqno = 30;
	for (i = 0; i < r2dst->req_header.rqs_seqno; i++) {
		ISP_IOXGET_16(isp, &r2src->req_handles[i],
		    r2dst->req_handles[i]);
	}
	while (i < 30) {
		r2dst->req_handles[i++] = 0;
	}
}

static INLINE void
isp_put_icb(struct ispsoftc *isp, isp_icb_t *Is, isp_icb_t *Id)
{
	int i;
	ISP_SWAP8(Is->icb_version, Is->_reserved0);
	ISP_IOXPUT_8(isp, Is->icb_version, &Id->icb_version);
	ISP_IOXPUT_8(isp, Is->_reserved0, &Id->_reserved0);
	ISP_IOXPUT_16(isp, Is->icb_fwoptions, &Id->icb_fwoptions);
	ISP_IOXPUT_16(isp, Is->icb_maxfrmlen, &Id->icb_maxfrmlen);
	ISP_IOXPUT_16(isp, Is->icb_maxalloc, &Id->icb_maxalloc);
	ISP_IOXPUT_16(isp, Is->icb_execthrottle, &Id->icb_execthrottle);
	ISP_SWAP8(Is->icb_retry_count, Is->icb_retry_delay);
	ISP_IOXPUT_8(isp, Is->icb_retry_count, &Id->icb_retry_count);
	ISP_IOXPUT_8(isp, Is->icb_retry_delay, &Id->icb_retry_delay);
	for (i = 0; i < 8; i++) {
		ISP_IOXPUT_8(isp, Is->icb_portname[i], &Id->icb_portname[i]);
	}
	ISP_IOXPUT_16(isp, Is->icb_hardaddr, &Id->icb_hardaddr);
	ISP_SWAP8(Is->icb_iqdevtype, Is->icb_logintime);
	ISP_IOXPUT_8(isp, Is->icb_iqdevtype, &Id->icb_iqdevtype);
	ISP_IOXPUT_8(isp, Is->icb_logintime, &Id->icb_logintime);
	for (i = 0; i < 8; i++) {
		ISP_IOXPUT_8(isp, Is->icb_nodename[i], &Id->icb_nodename[i]);
	}
	ISP_IOXPUT_16(isp, Is->icb_rqstout, &Id->icb_rqstout);
	ISP_IOXPUT_16(isp, Is->icb_rspnsin, &Id->icb_rspnsin);
	ISP_IOXPUT_16(isp, Is->icb_rqstqlen, &Id->icb_rqstqlen);
	ISP_IOXPUT_16(isp, Is->icb_rsltqlen, &Id->icb_rsltqlen);
	for (i = 0; i < 4; i++) {
		ISP_IOXPUT_16(isp, Is->icb_rqstaddr[i], &Id->icb_rqstaddr[i]);
	}
	for (i = 0; i < 4; i++) {
		ISP_IOXPUT_16(isp, Is->icb_respaddr[i], &Id->icb_respaddr[i]);
	}
	ISP_IOXPUT_16(isp, Is->icb_lunenables, &Id->icb_lunenables);
	ISP_SWAP8(Is->icb_ccnt, Is->icb_icnt);
	ISP_IOXPUT_8(isp, Is->icb_ccnt, &Id->icb_ccnt);
	ISP_IOXPUT_8(isp, Is->icb_icnt, &Id->icb_icnt);
	ISP_IOXPUT_16(isp, Is->icb_lunetimeout, &Id->icb_lunetimeout);
	ISP_IOXPUT_16(isp, Is->icb_xfwoptions, &Id->icb_xfwoptions);
	ISP_SWAP8(Is->icb_racctimer, Is->icb_idelaytimer);
	ISP_IOXPUT_8(isp, Is->icb_racctimer, &Id->icb_racctimer);
	ISP_IOXPUT_8(isp, Is->icb_idelaytimer, &Id->icb_idelaytimer);
	ISP_IOXPUT_16(isp, Is->icb_zfwoptions, &Id->icb_zfwoptions);
}

static INLINE void
isp_get_pdb(struct ispsoftc *isp, isp_pdb_t *src, isp_pdb_t *dst)
{
	int i;
	ISP_IOXGET_16(isp, &src->pdb_options, dst->pdb_options);
        ISP_IOXGET_8(isp, &src->pdb_mstate, dst->pdb_mstate);
        ISP_IOXGET_8(isp, &src->pdb_sstate, dst->pdb_sstate);
	for (i = 0; i < 4; i++) {
		ISP_IOXGET_8(isp, &src->pdb_hardaddr_bits[i],
		    dst->pdb_hardaddr_bits[i]);
	}
	for (i = 0; i < 4; i++) {
		ISP_IOXGET_8(isp, &src->pdb_portid_bits[i],
		    dst->pdb_portid_bits[i]);
	}
	for (i = 0; i < 8; i++) {
		ISP_IOXGET_8(isp, &src->pdb_nodename[i], dst->pdb_nodename[i]);
	}
	for (i = 0; i < 8; i++) {
		ISP_IOXGET_8(isp, &src->pdb_portname[i], dst->pdb_portname[i]);
	}
	ISP_IOXGET_16(isp, &src->pdb_execthrottle, dst->pdb_execthrottle);
	ISP_IOXGET_16(isp, &src->pdb_exec_count, dst->pdb_exec_count);
	ISP_IOXGET_8(isp, &src->pdb_retry_count, dst->pdb_retry_count);
	ISP_IOXGET_8(isp, &src->pdb_retry_delay, dst->pdb_retry_delay);
	ISP_IOXGET_16(isp, &src->pdb_resalloc, dst->pdb_resalloc);
	ISP_IOXGET_16(isp, &src->pdb_curalloc, dst->pdb_curalloc);
	ISP_IOXGET_16(isp, &src->pdb_qhead, dst->pdb_qhead);
	ISP_IOXGET_16(isp, &src->pdb_qtail, dst->pdb_qtail);
	ISP_IOXGET_16(isp, &src->pdb_tl_next, dst->pdb_tl_next);
	ISP_IOXGET_16(isp, &src->pdb_tl_last, dst->pdb_tl_last);
	ISP_IOXGET_16(isp, &src->pdb_features, dst->pdb_features);
	ISP_IOXGET_16(isp, &src->pdb_pconcurrnt, dst->pdb_pconcurrnt);
	ISP_IOXGET_16(isp, &src->pdb_roi, dst->pdb_roi);
	ISP_IOXGET_8(isp, &src->pdb_target, dst->pdb_target);
	ISP_IOXGET_8(isp, &src->pdb_initiator, dst->pdb_initiator);
	ISP_IOXGET_16(isp, &src->pdb_rdsiz, dst->pdb_rdsiz);
	ISP_IOXGET_16(isp, &src->pdb_ncseq, dst->pdb_ncseq);
	ISP_IOXGET_16(isp, &src->pdb_noseq, dst->pdb_noseq);
	ISP_IOXGET_16(isp, &src->pdb_labrtflg, dst->pdb_labrtflg);
	ISP_IOXGET_16(isp, &src->pdb_lstopflg, dst->pdb_lstopflg);
	ISP_IOXGET_16(isp, &src->pdb_sqhead, dst->pdb_sqhead);
	ISP_IOXGET_16(isp, &src->pdb_sqtail, dst->pdb_sqtail);
	ISP_IOXGET_16(isp, &src->pdb_ptimer, dst->pdb_ptimer);
	ISP_IOXGET_16(isp, &src->pdb_nxt_seqid, dst->pdb_nxt_seqid);
	ISP_IOXGET_16(isp, &src->pdb_fcount, dst->pdb_fcount);
	ISP_IOXGET_16(isp, &src->pdb_prli_len, dst->pdb_prli_len);
	ISP_IOXGET_16(isp, &src->pdb_prli_svc0, dst->pdb_prli_svc0);
	ISP_IOXGET_16(isp, &src->pdb_prli_svc3, dst->pdb_prli_svc3);
	ISP_IOXGET_16(isp, &src->pdb_loopid, dst->pdb_loopid);
	ISP_IOXGET_16(isp, &src->pdb_il_ptr, dst->pdb_il_ptr);
	ISP_IOXGET_16(isp, &src->pdb_sl_ptr, dst->pdb_sl_ptr);
}


/*
 * CT_HDR canonicalization- only needed for SNS responses
 */
static INLINE void
isp_get_ct_hdr(struct ispsoftc *isp, ct_hdr_t *src, ct_hdr_t *dst)
{
	ISP_IOXGET_8(isp, &src->ct_revision, dst->ct_revision);
	ISP_IOXGET_8(isp, &src->ct_portid[0], dst->ct_portid[0]);
	ISP_IOXGET_8(isp, &src->ct_portid[1], dst->ct_portid[1]);
	ISP_IOXGET_8(isp, &src->ct_portid[2], dst->ct_portid[2]);
	ISP_IOXGET_8(isp, &src->ct_fcs_type, dst->ct_fcs_type);
	ISP_IOXGET_8(isp, &src->ct_fcs_subtype, dst->ct_fcs_subtype);
	ISP_IOXGET_8(isp, &src->ct_options, dst->ct_options);
	ISP_IOXGET_8(isp, &src->ct_res0, dst->ct_res0);
	ISP_IOXGET_16(isp, &src->ct_response, dst->ct_response);
	dst->ct_response = (dst->ct_response << 8) | (dst->ct_response >> 8);
	ISP_IOXGET_16(isp, &src->ct_resid, dst->ct_resid);
	dst->ct_resid = (dst->ct_resid << 8) | (dst->ct_resid >> 8);
	ISP_IOXGET_8(isp, &src->ct_res1, dst->ct_res1);
	ISP_IOXGET_8(isp, &src->ct_reason, dst->ct_reason);
	ISP_IOXGET_8(isp, &src->ct_explanation, dst->ct_explanation);
	ISP_IOXGET_8(isp, &src->ct_vunique, dst->ct_vunique);
}

/*
 * Generic SNS request - not particularly useful since the per-command data
 * isn't always 16 bit words.
 */
static INLINE void
isp_put_sns_request(struct ispsoftc *isp, sns_screq_t *src, sns_screq_t *dst)
{
	int i, nw = (int) src->snscb_sblen;
	ISP_IOXPUT_16(isp, src->snscb_rblen, &dst->snscb_rblen);
	for (i = 0; i < 4; i++) {
		ISP_IOXPUT_16(isp, src->snscb_addr[i], &dst->snscb_addr[i]);
	}
	ISP_IOXPUT_16(isp, src->snscb_sblen, &dst->snscb_sblen);
	for (i = 0; i < nw; i++) {
		ISP_IOXPUT_16(isp, src->snscb_data[i], &dst->snscb_data[i]);
	}
	
}

static INLINE void
isp_put_gid_ft_request(struct ispsoftc *isp, sns_gid_ft_req_t *src,
    sns_gid_ft_req_t *dst)
{
	ISP_IOXPUT_16(isp, src->snscb_rblen, &dst->snscb_rblen);
	ISP_IOXPUT_16(isp, src->snscb_res0, &dst->snscb_res0);
	ISP_IOXPUT_16(isp, src->snscb_addr[0], &dst->snscb_addr[0]);
	ISP_IOXPUT_16(isp, src->snscb_addr[1], &dst->snscb_addr[1]);
	ISP_IOXPUT_16(isp, src->snscb_addr[2], &dst->snscb_addr[2]);
	ISP_IOXPUT_16(isp, src->snscb_addr[3], &dst->snscb_addr[3]);
	ISP_IOXPUT_16(isp, src->snscb_sblen, &dst->snscb_sblen);
	ISP_IOXPUT_16(isp, src->snscb_res1, &dst->snscb_res1);
	ISP_IOXPUT_16(isp, src->snscb_cmd, &dst->snscb_cmd);
	ISP_IOXPUT_16(isp, src->snscb_mword_div_2, &dst->snscb_mword_div_2);
	ISP_IOXPUT_32(isp, src->snscb_res3, &dst->snscb_res3);
	ISP_IOXPUT_16(isp, src->snscb_fc4_type, &dst->snscb_fc4_type);
}

static INLINE void
isp_put_gxn_id_request(struct ispsoftc *isp, sns_gxn_id_req_t *src,
    sns_gxn_id_req_t *dst)
{
	ISP_IOXPUT_16(isp, src->snscb_rblen, &dst->snscb_rblen);
	ISP_IOXPUT_16(isp, src->snscb_res0, &dst->snscb_res0);
	ISP_IOXPUT_16(isp, src->snscb_addr[0], &dst->snscb_addr[0]);
	ISP_IOXPUT_16(isp, src->snscb_addr[1], &dst->snscb_addr[1]);
	ISP_IOXPUT_16(isp, src->snscb_addr[2], &dst->snscb_addr[2]);
	ISP_IOXPUT_16(isp, src->snscb_addr[3], &dst->snscb_addr[3]);
	ISP_IOXPUT_16(isp, src->snscb_sblen, &dst->snscb_sblen);
	ISP_IOXPUT_16(isp, src->snscb_res1, &dst->snscb_res1);
	ISP_IOXPUT_16(isp, src->snscb_cmd, &dst->snscb_cmd);
	ISP_IOXPUT_16(isp, src->snscb_res2, &dst->snscb_res2);
	ISP_IOXPUT_32(isp, src->snscb_res3, &dst->snscb_res3);
	ISP_IOXPUT_32(isp, src->snscb_portid, &dst->snscb_portid);
}

/*
 * Generic SNS response - not particularly useful since the per-command data
 * isn't always 16 bit words.
 */
static INLINE void
isp_get_sns_response(struct ispsoftc *isp, sns_scrsp_t *src,
    sns_scrsp_t *dst, int nwords)
{
	int i;
	isp_get_ct_hdr(isp, &src->snscb_cthdr, &dst->snscb_cthdr);
	ISP_IOXGET_8(isp, &src->snscb_port_type, dst->snscb_port_type);
	for (i = 0; i < 3; i++) {
		ISP_IOXGET_8(isp, &src->snscb_port_id[i],
		    dst->snscb_port_id[i]);
	}
	for (i = 0; i < 8; i++) {
		ISP_IOXGET_8(isp, &src->snscb_portname[i],
		    dst->snscb_portname[i]);
	}
	for (i = 0; i < nwords; i++) {
		ISP_IOXGET_16(isp, &src->snscb_data[i], dst->snscb_data[i]);
	}
}

static INLINE void
isp_get_gid_ft_response(struct ispsoftc *isp, sns_gid_ft_rsp_t *src,
    sns_gid_ft_rsp_t *dst, int nwords)
{
	int i;
	isp_get_ct_hdr(isp, &src->snscb_cthdr, &dst->snscb_cthdr);
	for (i = 0; i < nwords; i++) {
		int j;
		ISP_IOXGET_8(isp,
		    &src->snscb_ports[i].control,
		    dst->snscb_ports[i].control);
		for (j = 0; j < 3; j++) {
			ISP_IOXGET_8(isp,
			    &src->snscb_ports[i].portid[j],
			    dst->snscb_ports[i].portid[j]);
		}
		if (dst->snscb_ports[i].control & 0x80) {
			break;
		}
	}
}

static INLINE void
isp_get_gxn_id_response(struct ispsoftc *isp, sns_gxn_id_rsp_t *src,
    sns_gxn_id_rsp_t *dst)
{
	int i;
	isp_get_ct_hdr(isp, &src->snscb_cthdr, &dst->snscb_cthdr);
	for (i = 0; i < 8; i++)
		ISP_IOXGET_8(isp, &src->snscb_wwn[i], dst->snscb_wwn[i]);
}

static INLINE void
isp_get_ga_nxt_response(struct ispsoftc *isp, sns_ga_nxt_rsp_t *src,
    sns_ga_nxt_rsp_t *dst)
{
	int i;
	isp_get_ct_hdr(isp, &src->snscb_cthdr, &dst->snscb_cthdr);
	ISP_IOXGET_8(isp, &src->snscb_port_type, dst->snscb_port_type);
	for (i = 0; i < 3; i++) {
		ISP_IOXGET_8(isp, &src->snscb_port_id[i],
		    dst->snscb_port_id[i]);
	}
	for (i = 0; i < 8; i++) {
		ISP_IOXGET_8(isp, &src->snscb_portname[i],
		    dst->snscb_portname[i]);
	}
	ISP_IOXGET_8(isp, &src->snscb_pnlen, dst->snscb_pnlen);
	for (i = 0; i < 255; i++) {
		ISP_IOXGET_8(isp, &src->snscb_pname[i], dst->snscb_pname[i]);
	}
	for (i = 0; i < 8; i++) {
		ISP_IOXGET_8(isp, &src->snscb_nodename[i],
		    dst->snscb_nodename[i]);
	}
	ISP_IOXGET_8(isp, &src->snscb_nnlen, dst->snscb_nnlen);
	for (i = 0; i < 255; i++) {
		ISP_IOXGET_8(isp, &src->snscb_nname[i], dst->snscb_nname[i]);
	}
	for (i = 0; i < 8; i++) {
		ISP_IOXGET_8(isp, &src->snscb_ipassoc[i],
		    dst->snscb_ipassoc[i]);
	}
	for (i = 0; i < 16; i++) {
		ISP_IOXGET_8(isp, &src->snscb_ipaddr[i], dst->snscb_ipaddr[i]);
	}
	for (i = 0; i < 4; i++) {
		ISP_IOXGET_8(isp, &src->snscb_svc_class[i],
		    dst->snscb_svc_class[i]);
	}
	for (i = 0; i < 32; i++) {
		ISP_IOXGET_8(isp, &src->snscb_fc4_types[i],
		    dst->snscb_fc4_types[i]);
	}
	for (i = 0; i < 8; i++) {
		ISP_IOXGET_8(isp, &src->snscb_fpname[i], dst->snscb_fpname[i]);
	}
	ISP_IOXGET_8(isp, &src->snscb_reserved, dst->snscb_reserved);
	for (i = 0; i < 3; i++) {
		ISP_IOXGET_8(isp, &src->snscb_hardaddr[i],
		    dst->snscb_hardaddr[i]);
	}
}

#ifdef	ISP_TARGET_MODE
static INLINE void
isp_put_atio(struct ispsoftc *isp, at_entry_t *atsrc, at_entry_t *atdst)
{
	int i;
	isp_copy_out_hdr(isp, &atsrc->at_header, &atdst->at_header);
	ISP_IOXPUT_16(isp, atsrc->at_reserved, &atdst->at_reserved);
	ISP_IOXPUT_16(isp, atsrc->at_handle, &atdst->at_handle);
	if (ISP_IS_SBUS(isp)) {
		ISP_IOXPUT_8(isp, atsrc->at_lun, &atdst->at_iid);
		ISP_IOXPUT_8(isp, atsrc->at_iid, &atdst->at_lun);
		ISP_IOXPUT_8(isp, atsrc->at_cdblen, &atdst->at_tgt);
		ISP_IOXPUT_8(isp, atsrc->at_tgt, &atdst->at_cdblen);
		ISP_IOXPUT_8(isp, atsrc->at_status, &atdst->at_scsi_status);
		ISP_IOXPUT_8(isp, atsrc->at_scsi_status, &atdst->at_status);
		ISP_IOXPUT_8(isp, atsrc->at_tag_val, &atdst->at_tag_type);
		ISP_IOXPUT_8(isp, atsrc->at_tag_type, &atdst->at_tag_val);
	} else {
		ISP_IOXPUT_8(isp, atsrc->at_lun, &atdst->at_lun);
		ISP_IOXPUT_8(isp, atsrc->at_iid, &atdst->at_iid);
		ISP_IOXPUT_8(isp, atsrc->at_cdblen, &atdst->at_cdblen);
		ISP_IOXPUT_8(isp, atsrc->at_tgt, &atdst->at_tgt);
		ISP_IOXPUT_8(isp, atsrc->at_status, &atdst->at_status);
		ISP_IOXPUT_8(isp, atsrc->at_scsi_status,
		    &atdst->at_scsi_status);
		ISP_IOXPUT_8(isp, atsrc->at_tag_val, &atdst->at_tag_val);
		ISP_IOXPUT_8(isp, atsrc->at_tag_type, &atdst->at_tag_type);
	}
	ISP_IOXPUT_32(isp, atsrc->at_flags, &atdst->at_flags);
	for (i = 0; i < ATIO_CDBLEN; i++) {
		ISP_IOXPUT_8(isp, atsrc->at_cdb[i], &atdst->at_cdb[i]);
	}
	for (i = 0; i < QLTM_SENSELEN; i++) {
		ISP_IOXPUT_8(isp, atsrc->at_sense[i], &atdst->at_sense[i]);
	}
}

static INLINE void
isp_get_atio(struct ispsoftc *isp, at_entry_t *atsrc, at_entry_t *atdst)
{
	int i;
	isp_copy_in_hdr(isp, &atsrc->at_header, &atdst->at_header);
	ISP_IOXGET_16(isp, &atsrc->at_reserved, atdst->at_reserved);
	ISP_IOXGET_16(isp, &atsrc->at_handle, atdst->at_handle);
	if (ISP_IS_SBUS(isp)) {
		ISP_IOXGET_8(isp, &atsrc->at_lun, atdst->at_iid);
		ISP_IOXGET_8(isp, &atsrc->at_iid, atdst->at_lun);
		ISP_IOXGET_8(isp, &atsrc->at_cdblen, atdst->at_tgt);
		ISP_IOXGET_8(isp, &atsrc->at_tgt, atdst->at_cdblen);
		ISP_IOXGET_8(isp, &atsrc->at_status, atdst->at_scsi_status);
		ISP_IOXGET_8(isp, &atsrc->at_scsi_status, atdst->at_status);
		ISP_IOXGET_8(isp, &atsrc->at_tag_val, atdst->at_tag_type);
		ISP_IOXGET_8(isp, &atsrc->at_tag_type, atdst->at_tag_val);
	} else {
		ISP_IOXGET_8(isp, &atsrc->at_lun, atdst->at_lun);
		ISP_IOXGET_8(isp, &atsrc->at_iid, atdst->at_iid);
		ISP_IOXGET_8(isp, &atsrc->at_cdblen, atdst->at_cdblen);
		ISP_IOXGET_8(isp, &atsrc->at_tgt, atdst->at_tgt);
		ISP_IOXGET_8(isp, &atsrc->at_status, atdst->at_status);
		ISP_IOXGET_8(isp, &atsrc->at_scsi_status,
		    atdst->at_scsi_status);
		ISP_IOXGET_8(isp, &atsrc->at_tag_val, atdst->at_tag_val);
		ISP_IOXGET_8(isp, &atsrc->at_tag_type, atdst->at_tag_type);
	}
	ISP_IOXGET_32(isp, &atsrc->at_flags, atdst->at_flags);
	for (i = 0; i < ATIO_CDBLEN; i++) {
		ISP_IOXGET_8(isp, &atsrc->at_cdb[i], atdst->at_cdb[i]);
	}
	for (i = 0; i < QLTM_SENSELEN; i++) {
		ISP_IOXGET_8(isp, &atsrc->at_sense[i], atdst->at_sense[i]);
	}
}

static INLINE void
isp_put_atio2(struct ispsoftc *isp, at2_entry_t *atsrc, at2_entry_t *atdst)
{
	int i;
	isp_copy_out_hdr(isp, &atsrc->at_header, &atdst->at_header);
	ISP_IOXPUT_32(isp, atsrc->at_reserved, &atdst->at_reserved);
	ISP_IOXPUT_8(isp, atsrc->at_lun, &atdst->at_lun);
	ISP_IOXPUT_8(isp, atsrc->at_iid, &atdst->at_iid);
	ISP_IOXPUT_16(isp, atsrc->at_rxid, &atdst->at_rxid);
	ISP_IOXPUT_16(isp, atsrc->at_flags, &atdst->at_flags);
	ISP_IOXPUT_16(isp, atsrc->at_status, &atdst->at_status);
	ISP_IOXPUT_8(isp, atsrc->at_reserved1, &atdst->at_reserved1);
	ISP_IOXPUT_8(isp, atsrc->at_taskcodes, &atdst->at_taskcodes);
	ISP_IOXPUT_8(isp, atsrc->at_taskflags, &atdst->at_taskflags);
	ISP_IOXPUT_8(isp, atsrc->at_execodes, &atdst->at_execodes);
	for (i = 0; i < ATIO2_CDBLEN; i++) {
		ISP_IOXPUT_8(isp, atsrc->at_cdb[i], &atdst->at_cdb[i]);
	}
	ISP_IOXPUT_32(isp, atsrc->at_datalen, &atdst->at_datalen);
	ISP_IOXPUT_16(isp, atsrc->at_scclun, &atdst->at_scclun);
	for (i = 0; i < 4; i++) {
		ISP_IOXPUT_16(isp, atsrc->at_wwpn[i], &atdst->at_wwpn[i]);
	}
	for (i = 0; i < 6; i++) {
		ISP_IOXPUT_16(isp, atsrc->at_reserved2[i],
		    &atdst->at_reserved2[i]);
	}
	ISP_IOXPUT_16(isp, atsrc->at_oxid, &atdst->at_oxid);
}

static INLINE void
isp_get_atio2(struct ispsoftc *isp, at2_entry_t *atsrc, at2_entry_t *atdst)
{
	int i;
	isp_copy_in_hdr(isp, &atsrc->at_header, &atdst->at_header);
	ISP_IOXGET_32(isp, &atsrc->at_reserved, atdst->at_reserved);
	ISP_IOXGET_8(isp, &atsrc->at_lun, atdst->at_lun);
	ISP_IOXGET_8(isp, &atsrc->at_iid, atdst->at_iid);
	ISP_IOXGET_16(isp, &atsrc->at_rxid, atdst->at_rxid);
	ISP_IOXGET_16(isp, &atsrc->at_flags, atdst->at_flags);
	ISP_IOXGET_16(isp, &atsrc->at_status, atdst->at_status);
	ISP_IOXGET_8(isp, &atsrc->at_reserved1, atdst->at_reserved1);
	ISP_IOXGET_8(isp, &atsrc->at_taskcodes, atdst->at_taskcodes);
	ISP_IOXGET_8(isp, &atsrc->at_taskflags, atdst->at_taskflags);
	ISP_IOXGET_8(isp, &atsrc->at_execodes, atdst->at_execodes);
	for (i = 0; i < ATIO2_CDBLEN; i++) {
		ISP_IOXGET_8(isp, &atsrc->at_cdb[i], atdst->at_cdb[i]);
	}
	ISP_IOXGET_32(isp, &atsrc->at_datalen, atdst->at_datalen);
	ISP_IOXGET_16(isp, &atsrc->at_scclun, atdst->at_scclun);
	for (i = 0; i < 4; i++) {
		ISP_IOXGET_16(isp, &atsrc->at_wwpn[i], atdst->at_wwpn[i]);
	}
	for (i = 0; i < 6; i++) {
		ISP_IOXGET_16(isp, &atsrc->at_reserved2[i],
		    atdst->at_reserved2[i]);
	}
	ISP_IOXGET_16(isp, &atsrc->at_oxid, atdst->at_oxid);
}

static INLINE void
isp_put_ctio(struct ispsoftc *isp, ct_entry_t *ctsrc, ct_entry_t *ctdst)
{
	int i;
	isp_copy_out_hdr(isp, &ctsrc->ct_header, &ctdst->ct_header);
	ISP_IOXPUT_16(isp, ctsrc->ct_reserved, &ctdst->ct_reserved);
	ISP_IOXPUT_16(isp, ctsrc->ct_fwhandle, &ctdst->ct_fwhandle);
	if (ISP_IS_SBUS(isp)) {
		ISP_IOXPUT_8(isp, ctsrc->ct_iid, &ctdst->ct_lun);
		ISP_IOXPUT_8(isp, ctsrc->ct_lun, &ctdst->ct_iid);
		ISP_IOXPUT_8(isp, ctsrc->ct_tgt, &ctdst->ct_reserved2);
		ISP_IOXPUT_8(isp, ctsrc->ct_reserved2, &ctdst->ct_tgt);
		ISP_IOXPUT_8(isp, ctsrc->ct_status, &ctdst->ct_scsi_status);
		ISP_IOXPUT_8(isp, ctsrc->ct_scsi_status, &ctdst->ct_status);
		ISP_IOXPUT_8(isp, ctsrc->ct_tag_type, &ctdst->ct_tag_val);
		ISP_IOXPUT_8(isp, ctsrc->ct_tag_val, &ctdst->ct_tag_type);
	} else {
		ISP_IOXPUT_8(isp, ctsrc->ct_iid, &ctdst->ct_iid);
		ISP_IOXPUT_8(isp, ctsrc->ct_lun, &ctdst->ct_lun);
		ISP_IOXPUT_8(isp, ctsrc->ct_tgt, &ctdst->ct_tgt);
		ISP_IOXPUT_8(isp, ctsrc->ct_reserved2, &ctdst->ct_reserved2);
		ISP_IOXPUT_8(isp, ctsrc->ct_scsi_status,
		    &ctdst->ct_scsi_status);
		ISP_IOXPUT_8(isp, ctsrc->ct_status, &ctdst->ct_status);
		ISP_IOXPUT_8(isp, ctsrc->ct_tag_type, &ctdst->ct_tag_type);
		ISP_IOXPUT_8(isp, ctsrc->ct_tag_val, &ctdst->ct_tag_val);
	}
	ISP_IOXPUT_32(isp, ctsrc->ct_flags, &ctdst->ct_flags);
	ISP_IOXPUT_32(isp, ctsrc->ct_xfrlen, &ctdst->ct_xfrlen);
	ISP_IOXPUT_32(isp, ctsrc->ct_resid, &ctdst->ct_resid);
	ISP_IOXPUT_16(isp, ctsrc->ct_timeout, &ctdst->ct_timeout);
	ISP_IOXPUT_16(isp, ctsrc->ct_seg_count, &ctdst->ct_seg_count);
	for (i = 0; i < ISP_RQDSEG; i++) {
		ISP_IOXPUT_32(isp, ctsrc->ct_dataseg[i].ds_base,
		    &ctdst->ct_dataseg[i].ds_base);
		ISP_IOXPUT_32(isp, ctsrc->ct_dataseg[i].ds_count,
		    &ctdst->ct_dataseg[i].ds_count);
	}
}

static INLINE void
isp_get_ctio(struct ispsoftc *isp, ct_entry_t *ctsrc, ct_entry_t *ctdst)
{
	int i;
	isp_copy_in_hdr(isp, &ctsrc->ct_header, &ctdst->ct_header);
	ISP_IOXGET_16(isp, &ctsrc->ct_reserved, ctdst->ct_reserved);
	ISP_IOXGET_16(isp, &ctsrc->ct_fwhandle, ctdst->ct_fwhandle);
	if (ISP_IS_SBUS(isp)) {
		ISP_IOXGET_8(isp, &ctsrc->ct_lun, ctdst->ct_iid);
		ISP_IOXGET_8(isp, &ctsrc->ct_iid, ctdst->ct_lun);
		ISP_IOXGET_8(isp, &ctsrc->ct_reserved2, ctdst->ct_tgt);
		ISP_IOXGET_8(isp, &ctsrc->ct_tgt, ctdst->ct_reserved2);
		ISP_IOXGET_8(isp, &ctsrc->ct_status, ctdst->ct_scsi_status);
		ISP_IOXGET_8(isp, &ctsrc->ct_scsi_status, ctdst->ct_status);
		ISP_IOXGET_8(isp, &ctsrc->ct_tag_val, ctdst->ct_tag_type);
		ISP_IOXGET_8(isp, &ctsrc->ct_tag_type, ctdst->ct_tag_val);
	} else {
		ISP_IOXGET_8(isp, &ctsrc->ct_lun, ctdst->ct_lun);
		ISP_IOXGET_8(isp, &ctsrc->ct_iid, ctdst->ct_iid);
		ISP_IOXGET_8(isp, &ctsrc->ct_reserved2, ctdst->ct_reserved2);
		ISP_IOXGET_8(isp, &ctsrc->ct_tgt, ctdst->ct_tgt);
		ISP_IOXGET_8(isp, &ctsrc->ct_status, ctdst->ct_status);
		ISP_IOXGET_8(isp, &ctsrc->ct_scsi_status,
		    ctdst->ct_scsi_status);
		ISP_IOXGET_8(isp, &ctsrc->ct_tag_val, ctdst->ct_tag_val);
		ISP_IOXGET_8(isp, &ctsrc->ct_tag_type, ctdst->ct_tag_type);
	}
	ISP_IOXGET_32(isp, &ctsrc->ct_flags, ctdst->ct_flags);
	ISP_IOXGET_32(isp, &ctsrc->ct_xfrlen, ctdst->ct_xfrlen);
	ISP_IOXGET_32(isp, &ctsrc->ct_resid, ctdst->ct_resid);
	ISP_IOXGET_16(isp, &ctsrc->ct_timeout, ctdst->ct_timeout);
	ISP_IOXGET_16(isp, &ctsrc->ct_seg_count, ctdst->ct_seg_count);
	for (i = 0; i < ISP_RQDSEG; i++) {
		ISP_IOXGET_32(isp,
		    &ctsrc->ct_dataseg[i].ds_base,
		    ctdst->ct_dataseg[i].ds_base);
		ISP_IOXGET_32(isp,
		    &ctsrc->ct_dataseg[i].ds_count,
		    ctdst->ct_dataseg[i].ds_count);
	}
}

static INLINE void
isp_put_ctio2(struct ispsoftc *isp, ct2_entry_t *ctsrc, ct2_entry_t *ctdst)
{
	int i;
	isp_copy_out_hdr(isp, &ctsrc->ct_header, &ctdst->ct_header);
	ISP_IOXPUT_16(isp, ctsrc->ct_reserved, &ctdst->ct_reserved);
	ISP_IOXPUT_16(isp, ctsrc->ct_fwhandle, &ctdst->ct_fwhandle);
	ISP_IOXPUT_8(isp, ctsrc->ct_lun, &ctdst->ct_lun);
	ISP_IOXPUT_8(isp, ctsrc->ct_iid, &ctdst->ct_iid);
	ISP_IOXPUT_16(isp, ctsrc->ct_rxid, &ctdst->ct_rxid);
	ISP_IOXPUT_16(isp, ctsrc->ct_flags, &ctdst->ct_flags);
	ISP_IOXPUT_16(isp, ctsrc->ct_timeout, &ctdst->ct_timeout);
	ISP_IOXPUT_16(isp, ctsrc->ct_seg_count, &ctdst->ct_seg_count);
	ISP_IOXPUT_32(isp, ctsrc->ct_resid, &ctdst->ct_resid);
	ISP_IOXPUT_32(isp, ctsrc->ct_reloff, &ctdst->ct_reloff);
	if ((ctsrc->ct_flags & CT2_FLAG_MMASK) == CT2_FLAG_MODE0) {
		ISP_IOXPUT_32(isp, ctsrc->rsp.m0._reserved,
		    &ctdst->rsp.m0._reserved);
		ISP_IOXPUT_16(isp, ctsrc->rsp.m0._reserved2,
		    &ctdst->rsp.m0._reserved2);
		ISP_IOXPUT_16(isp, ctsrc->rsp.m0.ct_scsi_status,
		    &ctdst->rsp.m0.ct_scsi_status);
		ISP_IOXPUT_32(isp, ctsrc->rsp.m0.ct_xfrlen,
		    &ctdst->rsp.m0.ct_xfrlen);
		if (ctsrc->ct_header.rqs_entry_type == RQSTYPE_CTIO2) {
			for (i = 0; i < ISP_RQDSEG_T2; i++) {
				ISP_IOXPUT_32(isp,
				    ctsrc->rsp.m0.ct_dataseg[i].ds_base,
				    &ctdst->rsp.m0.ct_dataseg[i].ds_base);
				ISP_IOXPUT_32(isp,
				    ctsrc->rsp.m0.ct_dataseg[i].ds_count,
				    &ctdst->rsp.m0.ct_dataseg[i].ds_count);
			}
		} else if (ctsrc->ct_header.rqs_entry_type == RQSTYPE_CTIO3) {
			for (i = 0; i < ISP_RQDSEG_T3; i++) {
				ISP_IOXPUT_32(isp,
				    ctsrc->rsp.m0.ct_dataseg64[i].ds_base,
				    &ctdst->rsp.m0.ct_dataseg64[i].ds_base);
				ISP_IOXPUT_32(isp,
				    ctsrc->rsp.m0.ct_dataseg64[i].ds_basehi,
				    &ctdst->rsp.m0.ct_dataseg64[i].ds_basehi);
				ISP_IOXPUT_32(isp,
				    ctsrc->rsp.m0.ct_dataseg64[i].ds_count,
				    &ctdst->rsp.m0.ct_dataseg64[i].ds_count);
			}
		} else if (ctsrc->ct_header.rqs_entry_type == RQSTYPE_CTIO4) {
			ISP_IOXPUT_16(isp, ctsrc->rsp.m0.ct_dslist.ds_type,
			    &ctdst->rsp.m0.ct_dslist.ds_type);
			ISP_IOXPUT_32(isp, ctsrc->rsp.m0.ct_dslist.ds_segment,
			    &ctdst->rsp.m0.ct_dslist.ds_segment);
			ISP_IOXPUT_32(isp, ctsrc->rsp.m0.ct_dslist.ds_base,
			    &ctdst->rsp.m0.ct_dslist.ds_base);
		}
	} else if ((ctsrc->ct_flags & CT2_FLAG_MMASK) == CT2_FLAG_MODE1) {
		ISP_IOXPUT_16(isp, ctsrc->rsp.m1._reserved,
		    &ctdst->rsp.m1._reserved);
		ISP_IOXPUT_16(isp, ctsrc->rsp.m1._reserved2,
		    &ctdst->rsp.m1._reserved2);
		ISP_IOXPUT_16(isp, ctsrc->rsp.m1.ct_senselen,
		    &ctdst->rsp.m1.ct_senselen);
		ISP_IOXPUT_16(isp, ctsrc->rsp.m1.ct_scsi_status,
		    &ctdst->rsp.m1.ct_scsi_status);
		ISP_IOXPUT_16(isp, ctsrc->rsp.m1.ct_resplen,
		    &ctdst->rsp.m1.ct_resplen);
		for (i = 0; i < MAXRESPLEN; i++) {
			ISP_IOXPUT_8(isp, ctsrc->rsp.m1.ct_resp[i],
			    &ctdst->rsp.m1.ct_resp[i]);
		}
	} else {
		ISP_IOXPUT_32(isp, ctsrc->rsp.m2._reserved,
		    &ctdst->rsp.m2._reserved);
		ISP_IOXPUT_16(isp, ctsrc->rsp.m2._reserved2,
		    &ctdst->rsp.m2._reserved2);
		ISP_IOXPUT_16(isp, ctsrc->rsp.m2._reserved3,
		    &ctdst->rsp.m2._reserved3);
		ISP_IOXPUT_32(isp, ctsrc->rsp.m2.ct_datalen,
		    &ctdst->rsp.m2.ct_datalen);
		ISP_IOXPUT_32(isp, ctsrc->rsp.m2.ct_fcp_rsp_iudata.ds_base,
		    &ctdst->rsp.m2.ct_fcp_rsp_iudata.ds_base);
		ISP_IOXPUT_32(isp, ctsrc->rsp.m2.ct_fcp_rsp_iudata.ds_count,
		    &ctdst->rsp.m2.ct_fcp_rsp_iudata.ds_count);
	}
}

static INLINE void
isp_get_ctio2(struct ispsoftc *isp, ct2_entry_t *ctsrc, ct2_entry_t *ctdst)
{
	int i;
	isp_copy_in_hdr(isp, &ctsrc->ct_header, &ctdst->ct_header);
	ISP_IOXGET_16(isp, &ctsrc->ct_reserved, ctdst->ct_reserved);
	ISP_IOXGET_16(isp, &ctsrc->ct_fwhandle, ctdst->ct_fwhandle);
	ISP_IOXGET_8(isp, &ctsrc->ct_lun, ctdst->ct_lun);
	ISP_IOXGET_8(isp, &ctsrc->ct_iid, ctdst->ct_iid);
	ISP_IOXGET_16(isp, &ctsrc->ct_rxid, ctdst->ct_rxid);
	ISP_IOXGET_16(isp, &ctsrc->ct_flags, ctdst->ct_flags);
	ISP_IOXGET_16(isp, &ctsrc->ct_status, ctdst->ct_status);
	ISP_IOXGET_16(isp, &ctsrc->ct_timeout, ctdst->ct_timeout);
	ISP_IOXGET_16(isp, &ctsrc->ct_seg_count, ctdst->ct_seg_count);
	ISP_IOXGET_32(isp, &ctsrc->ct_reloff, ctdst->ct_reloff);
	ISP_IOXGET_32(isp, &ctsrc->ct_resid, ctdst->ct_resid);
	for (i = 0; i < 4; i++) {
		ISP_IOXGET_32(isp, &ctsrc->rsp.fw._reserved[i],
		    ctdst->rsp.fw._reserved[i]);
	}
	ISP_IOXGET_16(isp, &ctsrc->rsp.fw.ct_scsi_status,
	    ctdst->rsp.fw.ct_scsi_status);
	for (i = 0; i < QLTM_SENSELEN; i++) {
		ISP_IOXGET_8(isp, &ctsrc->rsp.fw.ct_sense[i],
		    ctdst->rsp.fw.ct_sense[i]);
	}
}

static INLINE void
isp_put_enable_lun(struct ispsoftc *isp, lun_entry_t *lesrc, lun_entry_t *ledst)
{
	int i;
	isp_copy_out_hdr(isp, &lesrc->le_header, &ledst->le_header);
	ISP_IOXPUT_32(isp, lesrc->le_reserved, &ledst->le_reserved);
	if (ISP_IS_SBUS(isp)) {
		ISP_IOXPUT_8(isp, lesrc->le_lun, &ledst->le_rsvd);
		ISP_IOXPUT_8(isp, lesrc->le_rsvd, &ledst->le_lun);
		ISP_IOXPUT_8(isp, lesrc->le_ops, &ledst->le_tgt);
		ISP_IOXPUT_8(isp, lesrc->le_tgt, &ledst->le_ops);
		ISP_IOXPUT_8(isp, lesrc->le_status, &ledst->le_reserved2);
		ISP_IOXPUT_8(isp, lesrc->le_reserved2, &ledst->le_status);
		ISP_IOXPUT_8(isp, lesrc->le_cmd_count, &ledst->le_in_count);
		ISP_IOXPUT_8(isp, lesrc->le_in_count, &ledst->le_cmd_count);
		ISP_IOXPUT_8(isp, lesrc->le_cdb6len, &ledst->le_cdb7len);
		ISP_IOXPUT_8(isp, lesrc->le_cdb7len, &ledst->le_cdb6len);
	} else {
		ISP_IOXPUT_8(isp, lesrc->le_lun, &ledst->le_lun);
		ISP_IOXPUT_8(isp, lesrc->le_rsvd, &ledst->le_rsvd);
		ISP_IOXPUT_8(isp, lesrc->le_ops, &ledst->le_ops);
		ISP_IOXPUT_8(isp, lesrc->le_tgt, &ledst->le_tgt);
		ISP_IOXPUT_8(isp, lesrc->le_status, &ledst->le_status);
		ISP_IOXPUT_8(isp, lesrc->le_reserved2, &ledst->le_reserved2);
		ISP_IOXPUT_8(isp, lesrc->le_cmd_count, &ledst->le_cmd_count);
		ISP_IOXPUT_8(isp, lesrc->le_in_count, &ledst->le_in_count);
		ISP_IOXPUT_8(isp, lesrc->le_cdb6len, &ledst->le_cdb6len);
		ISP_IOXPUT_8(isp, lesrc->le_cdb7len, &ledst->le_cdb7len);
	}
	ISP_IOXPUT_32(isp, lesrc->le_flags, &ledst->le_flags);
	ISP_IOXPUT_16(isp, lesrc->le_timeout, &ledst->le_timeout);
	for (i = 0; i < 20; i++) {
		ISP_IOXPUT_8(isp, lesrc->le_reserved3[i],
		    &ledst->le_reserved3[i]);
	}
}

static INLINE void
isp_get_enable_lun(struct ispsoftc *isp, lun_entry_t *lesrc, lun_entry_t *ledst)
{
	int i;
	isp_copy_in_hdr(isp, &lesrc->le_header, &ledst->le_header);
	ISP_IOXGET_32(isp, &lesrc->le_reserved, ledst->le_reserved);
	if (ISP_IS_SBUS(isp)) {
		ISP_IOXGET_8(isp, &lesrc->le_lun, ledst->le_rsvd);
		ISP_IOXGET_8(isp, &lesrc->le_rsvd, ledst->le_lun);
		ISP_IOXGET_8(isp, &lesrc->le_ops, ledst->le_tgt);
		ISP_IOXGET_8(isp, &lesrc->le_tgt, ledst->le_ops);
		ISP_IOXGET_8(isp, &lesrc->le_status, ledst->le_reserved2);
		ISP_IOXGET_8(isp, &lesrc->le_reserved2, ledst->le_status);
		ISP_IOXGET_8(isp, &lesrc->le_cmd_count, ledst->le_in_count);
		ISP_IOXGET_8(isp, &lesrc->le_in_count, ledst->le_cmd_count);
		ISP_IOXGET_8(isp, &lesrc->le_cdb6len, ledst->le_cdb7len);
		ISP_IOXGET_8(isp, &lesrc->le_cdb7len, ledst->le_cdb6len);
	} else {
		ISP_IOXGET_8(isp, &lesrc->le_lun, ledst->le_lun);
		ISP_IOXGET_8(isp, &lesrc->le_rsvd, ledst->le_rsvd);
		ISP_IOXGET_8(isp, &lesrc->le_ops, ledst->le_ops);
		ISP_IOXGET_8(isp, &lesrc->le_tgt, ledst->le_tgt);
		ISP_IOXGET_8(isp, &lesrc->le_status, ledst->le_status);
		ISP_IOXGET_8(isp, &lesrc->le_reserved2, ledst->le_reserved2);
		ISP_IOXGET_8(isp, &lesrc->le_cmd_count, ledst->le_cmd_count);
		ISP_IOXGET_8(isp, &lesrc->le_in_count, ledst->le_in_count);
		ISP_IOXGET_8(isp, &lesrc->le_cdb6len, ledst->le_cdb6len);
		ISP_IOXGET_8(isp, &lesrc->le_cdb7len, ledst->le_cdb7len);
	}
	ISP_IOXGET_32(isp, &lesrc->le_flags, ledst->le_flags);
	ISP_IOXGET_16(isp, &lesrc->le_timeout, ledst->le_timeout);
	for (i = 0; i < 20; i++) {
		ISP_IOXGET_8(isp, &lesrc->le_reserved3[i],
		    ledst->le_reserved3[i]);
	}
}

static INLINE void
isp_put_notify(struct ispsoftc *isp, in_entry_t *insrc, in_entry_t *indst)
{
	int i;
	isp_copy_out_hdr(isp, &insrc->in_header, &indst->in_header);
	ISP_IOXPUT_32(isp, insrc->in_reserved, &indst->in_reserved);
	if (ISP_IS_SBUS(isp)) {
		ISP_IOXPUT_8(isp, insrc->in_lun, &indst->in_iid);
		ISP_IOXPUT_8(isp, insrc->in_iid, &indst->in_lun);
		ISP_IOXPUT_8(isp, insrc->in_reserved2, &indst->in_tgt);
		ISP_IOXPUT_8(isp, insrc->in_tgt, &indst->in_reserved2);
		ISP_IOXPUT_8(isp, insrc->in_status, &indst->in_rsvd2);
		ISP_IOXPUT_8(isp, insrc->in_rsvd2, &indst->in_status);
		ISP_IOXPUT_8(isp, insrc->in_tag_val, &indst->in_tag_type);
		ISP_IOXPUT_8(isp, insrc->in_tag_type, &indst->in_tag_val);
	} else {
		ISP_IOXPUT_8(isp, insrc->in_lun, &indst->in_lun);
		ISP_IOXPUT_8(isp, insrc->in_iid, &indst->in_iid);
		ISP_IOXPUT_8(isp, insrc->in_reserved2, &indst->in_reserved2);
		ISP_IOXPUT_8(isp, insrc->in_tgt, &indst->in_tgt);
		ISP_IOXPUT_8(isp, insrc->in_status, &indst->in_status);
		ISP_IOXPUT_8(isp, insrc->in_rsvd2, &indst->in_rsvd2);
		ISP_IOXPUT_8(isp, insrc->in_tag_val, &indst->in_tag_val);
		ISP_IOXPUT_8(isp, insrc->in_tag_type, &indst->in_tag_type);
	}
	ISP_IOXPUT_32(isp, insrc->in_flags, &indst->in_flags);
	ISP_IOXPUT_16(isp, insrc->in_seqid, &indst->in_seqid);
	for (i = 0; i < IN_MSGLEN; i++) {
		ISP_IOXPUT_8(isp, insrc->in_msg[i], &indst->in_msg[i]);
	}
	for (i = 0; i < IN_RSVDLEN; i++) {
		ISP_IOXPUT_8(isp, insrc->in_reserved3[i],
		    &indst->in_reserved3[i]);
	}
	for (i = 0; i < QLTM_SENSELEN; i++) {
		ISP_IOXPUT_8(isp, insrc->in_sense[i],
		    &indst->in_sense[i]);
	}
}

static INLINE void
isp_get_notify(struct ispsoftc *isp, in_entry_t *insrc, in_entry_t *indst)
{
	int i;
	isp_copy_in_hdr(isp, &insrc->in_header, &indst->in_header);
	ISP_IOXGET_32(isp, &insrc->in_reserved, indst->in_reserved);
	if (ISP_IS_SBUS(isp)) {
		ISP_IOXGET_8(isp, &insrc->in_lun, indst->in_iid);
		ISP_IOXGET_8(isp, &insrc->in_iid, indst->in_lun);
		ISP_IOXGET_8(isp, &insrc->in_reserved2, indst->in_tgt);
		ISP_IOXGET_8(isp, &insrc->in_tgt, indst->in_reserved2);
		ISP_IOXGET_8(isp, &insrc->in_status, indst->in_rsvd2);
		ISP_IOXGET_8(isp, &insrc->in_rsvd2, indst->in_status);
		ISP_IOXGET_8(isp, &insrc->in_tag_val, indst->in_tag_type);
		ISP_IOXGET_8(isp, &insrc->in_tag_type, indst->in_tag_val);
	} else {
		ISP_IOXGET_8(isp, &insrc->in_lun, indst->in_lun);
		ISP_IOXGET_8(isp, &insrc->in_iid, indst->in_iid);
		ISP_IOXGET_8(isp, &insrc->in_reserved2, indst->in_reserved2);
		ISP_IOXGET_8(isp, &insrc->in_tgt, indst->in_tgt);
		ISP_IOXGET_8(isp, &insrc->in_status, indst->in_status);
		ISP_IOXGET_8(isp, &insrc->in_rsvd2, indst->in_rsvd2);
		ISP_IOXGET_8(isp, &insrc->in_tag_val, indst->in_tag_val);
		ISP_IOXGET_8(isp, &insrc->in_tag_type, indst->in_tag_type);
	}
	ISP_IOXGET_32(isp, &insrc->in_flags, indst->in_flags);
	ISP_IOXGET_16(isp, &insrc->in_seqid, indst->in_seqid);
	for (i = 0; i < IN_MSGLEN; i++) {
		ISP_IOXGET_8(isp, &insrc->in_msg[i], indst->in_msg[i]);
	}
	for (i = 0; i < IN_RSVDLEN; i++) {
		ISP_IOXGET_8(isp, &insrc->in_reserved3[i],
		    indst->in_reserved3[i]);
	}
	for (i = 0; i < QLTM_SENSELEN; i++) {
		ISP_IOXGET_8(isp, &insrc->in_sense[i],
		    indst->in_sense[i]);
	}
}

static INLINE void
isp_put_notify_fc(struct ispsoftc *isp, in_fcentry_t *insrc,
    in_fcentry_t *indst)
{
	isp_copy_out_hdr(isp, &insrc->in_header, &indst->in_header);
	ISP_IOXPUT_32(isp, insrc->in_reserved, &indst->in_reserved);
	ISP_IOXPUT_8(isp, insrc->in_lun, &indst->in_lun);
	ISP_IOXPUT_8(isp, insrc->in_iid, &indst->in_iid);
	ISP_IOXPUT_16(isp, insrc->in_scclun, &indst->in_scclun);
	ISP_IOXPUT_32(isp, insrc->in_reserved2, &indst->in_reserved2);
	ISP_IOXPUT_16(isp, insrc->in_status, &indst->in_status);
	ISP_IOXPUT_16(isp, insrc->in_task_flags, &indst->in_task_flags);
	ISP_IOXPUT_16(isp, insrc->in_seqid, &indst->in_seqid);
}

static INLINE void
isp_get_notify_fc(struct ispsoftc *isp, in_fcentry_t *insrc,
    in_fcentry_t *indst)
{
	isp_copy_in_hdr(isp, &insrc->in_header, &indst->in_header);
	ISP_IOXGET_32(isp, &insrc->in_reserved, indst->in_reserved);
	ISP_IOXGET_8(isp, &insrc->in_lun, indst->in_lun);
	ISP_IOXGET_8(isp, &insrc->in_iid, indst->in_iid);
	ISP_IOXGET_16(isp, &insrc->in_scclun, indst->in_scclun);
	ISP_IOXGET_32(isp, &insrc->in_reserved2, indst->in_reserved2);
	ISP_IOXGET_16(isp, &insrc->in_status, indst->in_status);
	ISP_IOXGET_16(isp, &insrc->in_task_flags, indst->in_task_flags);
	ISP_IOXGET_16(isp, &insrc->in_seqid, indst->in_seqid);
}

static INLINE void
isp_put_notify_ack(struct ispsoftc *isp, na_entry_t *nasrc,  na_entry_t *nadst)
{
	int i;
	isp_copy_out_hdr(isp, &nasrc->na_header, &nadst->na_header);
	ISP_IOXPUT_32(isp, nasrc->na_reserved, &nadst->na_reserved);
	if (ISP_IS_SBUS(isp)) {
		ISP_IOXPUT_8(isp, nasrc->na_lun, &nadst->na_iid);
		ISP_IOXPUT_8(isp, nasrc->na_iid, &nadst->na_lun);
		ISP_IOXPUT_8(isp, nasrc->na_status, &nadst->na_event);
		ISP_IOXPUT_8(isp, nasrc->na_event, &nadst->na_status);
	} else {
		ISP_IOXPUT_8(isp, nasrc->na_lun, &nadst->na_lun);
		ISP_IOXPUT_8(isp, nasrc->na_iid, &nadst->na_iid);
		ISP_IOXPUT_8(isp, nasrc->na_status, &nadst->na_status);
		ISP_IOXPUT_8(isp, nasrc->na_event, &nadst->na_event);
	}
	ISP_IOXPUT_32(isp, nasrc->na_flags, &nadst->na_flags);
	for (i = 0; i < NA_RSVDLEN; i++) {
		ISP_IOXPUT_16(isp, nasrc->na_reserved3[i],
		    &nadst->na_reserved3[i]);
	}
}

static INLINE void
isp_get_notify_ack(struct ispsoftc *isp, na_entry_t *nasrc, na_entry_t *nadst)
{
	int i;
	isp_copy_in_hdr(isp, &nasrc->na_header, &nadst->na_header);
	ISP_IOXGET_32(isp, &nasrc->na_reserved, nadst->na_reserved);
	if (ISP_IS_SBUS(isp)) {
		ISP_IOXGET_8(isp, &nasrc->na_lun, nadst->na_iid);
		ISP_IOXGET_8(isp, &nasrc->na_iid, nadst->na_lun);
		ISP_IOXGET_8(isp, &nasrc->na_status, nadst->na_event);
		ISP_IOXGET_8(isp, &nasrc->na_event, nadst->na_status);
	} else {
		ISP_IOXGET_8(isp, &nasrc->na_lun, nadst->na_lun);
		ISP_IOXGET_8(isp, &nasrc->na_iid, nadst->na_iid);
		ISP_IOXGET_8(isp, &nasrc->na_status, nadst->na_status);
		ISP_IOXGET_8(isp, &nasrc->na_event, nadst->na_event);
	}
	ISP_IOXGET_32(isp, &nasrc->na_flags, nadst->na_flags);
	for (i = 0; i < NA_RSVDLEN; i++) {
		ISP_IOXGET_16(isp, &nasrc->na_reserved3[i],
		    nadst->na_reserved3[i]);
	}
}

static INLINE void
isp_put_notify_ack_fc(struct ispsoftc *isp, na_fcentry_t *nasrc,
    na_fcentry_t *nadst)
{
	int i;
	isp_copy_out_hdr(isp, &nasrc->na_header, &nadst->na_header);
	ISP_IOXPUT_32(isp, nasrc->na_reserved, &nadst->na_reserved);
	ISP_IOXPUT_8(isp, nasrc->na_lun, &nadst->na_lun);
	ISP_IOXPUT_8(isp, nasrc->na_iid, &nadst->na_iid);
	ISP_IOXPUT_16(isp, nasrc->na_scclun, &nadst->na_scclun);
	ISP_IOXPUT_16(isp, nasrc->na_flags, &nadst->na_flags);
	ISP_IOXPUT_16(isp, nasrc->na_reserved2, &nadst->na_reserved2);
	ISP_IOXPUT_16(isp, nasrc->na_status, &nadst->na_status);
	ISP_IOXPUT_16(isp, nasrc->na_task_flags, &nadst->na_task_flags);
	ISP_IOXPUT_16(isp, nasrc->na_seqid, &nadst->na_seqid);
	for (i = 0; i < NA2_RSVDLEN; i++) {
		ISP_IOXPUT_16(isp, nasrc->na_reserved3[i],
		    &nadst->na_reserved3[i]);
	}
}

static INLINE void
isp_get_notify_ack_fc(struct ispsoftc *isp, na_fcentry_t *nasrc,
    na_fcentry_t *nadst)
{
	int i;
	isp_copy_in_hdr(isp, &nasrc->na_header, &nadst->na_header);
	ISP_IOXGET_32(isp, &nasrc->na_reserved, nadst->na_reserved);
	ISP_IOXGET_8(isp, &nasrc->na_lun, nadst->na_lun);
	ISP_IOXGET_8(isp, &nasrc->na_iid, nadst->na_iid);
	ISP_IOXGET_16(isp, &nasrc->na_scclun, nadst->na_scclun);
	ISP_IOXGET_16(isp, &nasrc->na_flags, nadst->na_flags);
	ISP_IOXGET_16(isp, &nasrc->na_reserved2, nadst->na_reserved2);
	ISP_IOXGET_16(isp, &nasrc->na_status, nadst->na_status);
	ISP_IOXGET_16(isp, &nasrc->na_task_flags, nadst->na_task_flags);
	ISP_IOXGET_16(isp, &nasrc->na_seqid, nadst->na_seqid);
	for (i = 0; i < NA2_RSVDLEN; i++) {
		ISP_IOXGET_16(isp, &nasrc->na_reserved3[i],
		    nadst->na_reserved3[i]);
	}
}
#endif
#endif	/* _ISP_INLINE_H */
