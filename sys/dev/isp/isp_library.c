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
 * Qlogic Host Adapter Internal Library Functions
 */
#ifdef	__NetBSD__
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD$");
#include <dev/ic/isp_netbsd.h>
#endif
#ifdef	__FreeBSD__
#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/isp/isp_library.c,v 1.14.6.1 2008/11/25 02:59:29 kensmith Exp $");
#include <dev/isp/isp_freebsd.h>
#endif
#ifdef	__OpenBSD__
#include <dev/ic/isp_openbsd.h>
#endif
#ifdef	__linux__
#include "isp_linux.h"
#endif
#ifdef	__svr4__
#include "isp_solaris.h"
#endif

int
isp_save_xs(ispsoftc_t *isp, XS_T *xs, uint32_t *handlep)
{
	uint16_t i, j;

	for (j = isp->isp_lasthdls, i = 0; i < isp->isp_maxcmds; i++) {
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
	if (++j == isp->isp_maxcmds) {
		j = 0;
	}
	isp->isp_lasthdls = (uint32_t)j;
	return (0);
}

XS_T *
isp_find_xs(ispsoftc_t *isp, uint32_t handle)
{
	if (handle < 1 || handle > (uint32_t) isp->isp_maxcmds) {
		return (NULL);
	} else {
		return (isp->isp_xflist[handle - 1]);
	}
}

uint32_t
isp_find_handle(ispsoftc_t *isp, XS_T *xs)
{
	uint16_t i;
	if (xs != NULL) {
		for (i = 0; i < isp->isp_maxcmds; i++) {
			if (isp->isp_xflist[i] == xs) {
				return ((uint32_t) (i+1));
			}
		}
	}
	return (0);
}

uint32_t
isp_handle_index(uint32_t handle)
{
	return (handle - 1);
}

void
isp_destroy_handle(ispsoftc_t *isp, uint32_t handle)
{
	if (handle > 0 && handle <= (uint32_t) isp->isp_maxcmds) {
		isp->isp_xflist[handle - 1] = NULL;
	}
}

int
isp_getrqentry(ispsoftc_t *isp, uint32_t *iptrp,
    uint32_t *optrp, void **resultp)
{
	volatile uint32_t iptr, optr;

	optr = isp->isp_reqodx = ISP_READ(isp, isp->isp_rqstoutrp);
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

#define	TBA	(4 * (((QENTRY_LEN >> 2) * 3) + 1) + 1)
void
isp_print_qentry(ispsoftc_t *isp, char *msg, int idx, void *arg)
{
	char buf[TBA];
	int amt, i, j;
	uint8_t *ptr = arg;

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

void
isp_print_bytes(ispsoftc_t *isp, const char *msg, int amt, void *arg)
{
	char buf[128];
	uint8_t *ptr = arg;
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
 * We assume we enter here with any locks held.
 */

int
isp_fc_runstate(ispsoftc_t *isp, int tval)
{
	fcparam *fcp;
	int *tptr;

        if (isp->isp_role == ISP_ROLE_NONE) {
		return (0);
	}
	fcp = FCPARAM(isp);
	tptr = &tval;
	if (fcp->isp_fwstate < FW_READY ||
	    fcp->isp_loopstate < LOOP_PDB_RCVD) {
		if (isp_control(isp, ISPCTL_FCLINK_TEST, tptr) != 0) {
			isp_prt(isp, ISP_LOGSANCFG,
			    "isp_fc_runstate: linktest failed");
			return (-1);
		}
		if (fcp->isp_fwstate != FW_READY ||
		    fcp->isp_loopstate < LOOP_PDB_RCVD) {
			isp_prt(isp, ISP_LOGSANCFG,
				"isp_fc_runstate: f/w not ready");
			return (-1);
		}
	}
	if ((isp->isp_role & ISP_ROLE_INITIATOR) == 0) {
		return (0);
	}
	if (isp_control(isp, ISPCTL_SCAN_LOOP, NULL) != 0) {
		isp_prt(isp, ISP_LOGSANCFG,
		    "isp_fc_runstate: scan loop fails");
		return (LOOP_PDB_RCVD);
	}
	if (isp_control(isp, ISPCTL_SCAN_FABRIC, NULL) != 0) {
		isp_prt(isp, ISP_LOGSANCFG,
		    "isp_fc_runstate: scan fabric fails");
		return (LOOP_LSCAN_DONE);
	}
	if (isp_control(isp, ISPCTL_PDB_SYNC, NULL) != 0) {
		isp_prt(isp, ISP_LOGSANCFG, "isp_fc_runstate: pdb_sync fails");
		return (LOOP_FSCAN_DONE);
	}
	if (fcp->isp_fwstate != FW_READY || fcp->isp_loopstate != LOOP_READY) {
		isp_prt(isp, ISP_LOGSANCFG,
		    "isp_fc_runstate: f/w not ready again");
		return (-1);
	}
	return (0);
}

/*
 * Fibre Channel Support- get the port database for the id.
 */
void
isp_dump_portdb(ispsoftc_t *isp)
{
	fcparam *fcp = (fcparam *) isp->isp_param;
	int i;

	for (i = 0; i < MAX_FC_TARG; i++) {
		char mb[4];
		const char *dbs[8] = {
			"NIL ",
			"PROB",
			"DEAD",
			"CHGD",
			"NEW ",
			"PVLD",
			"ZOMB",
			"VLD "
		};
		const char *roles[4] = {
			" UNK", " TGT", " INI", "TINI"
		};
		fcportdb_t *lp = &fcp->portdb[i];

		if (lp->state == FC_PORTDB_STATE_NIL) {
			continue;
		}
		if (lp->ini_map_idx) {
			SNPRINTF(mb, sizeof (mb), "%3d",
			    ((int) lp->ini_map_idx) - 1);
		} else {
			SNPRINTF(mb, sizeof (mb), "---");
		}
		isp_prt(isp, ISP_LOGALL, "%d: hdl 0x%x %s al%d tgt %s %s "
		    "0x%06x =>%s 0x%06x; WWNN 0x%08x%08x WWPN 0x%08x%08x", i,
		    lp->handle, dbs[lp->state], lp->autologin, mb,
		    roles[lp->roles], lp->portid,
		    roles[lp->new_roles], lp->new_portid,
		    (uint32_t) (lp->node_wwn >> 32),
		    (uint32_t) (lp->node_wwn),
		    (uint32_t) (lp->port_wwn >> 32),
		    (uint32_t) (lp->port_wwn));
	}
}

void
isp_shutdown(ispsoftc_t *isp)
{
	if (IS_FC(isp)) {
		if (IS_24XX(isp)) {
			ISP_WRITE(isp, BIU2400_ICR, 0);
			ISP_WRITE(isp, BIU2400_HCCR, HCCR_2400_CMD_PAUSE);
		} else {
			ISP_WRITE(isp, BIU_ICR, 0);
			ISP_WRITE(isp, HCCR, HCCR_CMD_PAUSE);
			ISP_WRITE(isp, BIU2100_CSR, BIU2100_FPM0_REGS);
			ISP_WRITE(isp, FPM_DIAG_CONFIG, FPM_SOFT_RESET);
			ISP_WRITE(isp, BIU2100_CSR, BIU2100_FB_REGS);
			ISP_WRITE(isp, FBM_CMD, FBMCMD_FIFO_RESET_ALL);
			ISP_WRITE(isp, BIU2100_CSR, BIU2100_RISC_REGS);
		}
	} else {
		ISP_WRITE(isp, BIU_ICR, 0);
		ISP_WRITE(isp, HCCR, HCCR_CMD_PAUSE);
	}
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

#define	ISP_IS_SBUS(isp)	\
	(ISP_SBUS_SUPPORTED && (isp)->isp_bustype == ISP_BT_SBUS)

#define	ASIZE(x)	(sizeof (x) / sizeof (x[0]))
/*
 * Swizzle/Copy Functions
 */
void
isp_put_hdr(ispsoftc_t *isp, isphdr_t *hpsrc, isphdr_t *hpdst)
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

void
isp_get_hdr(ispsoftc_t *isp, isphdr_t *hpsrc, isphdr_t *hpdst)
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

int
isp_get_response_type(ispsoftc_t *isp, isphdr_t *hp)
{
	uint8_t type;
	if (ISP_IS_SBUS(isp)) {
		ISP_IOXGET_8(isp, &hp->rqs_entry_count, type);
	} else {
		ISP_IOXGET_8(isp, &hp->rqs_entry_type, type);
	}
	return ((int)type);
}

void
isp_put_request(ispsoftc_t *isp, ispreq_t *rqsrc, ispreq_t *rqdst)
{
	int i;
	isp_put_hdr(isp, &rqsrc->req_header, &rqdst->req_header);
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
	for (i = 0; i < ASIZE(rqsrc->req_cdb); i++) {
		ISP_IOXPUT_8(isp, rqsrc->req_cdb[i], &rqdst->req_cdb[i]);
	}
	for (i = 0; i < ISP_RQDSEG; i++) {
		ISP_IOXPUT_32(isp, rqsrc->req_dataseg[i].ds_base,
		    &rqdst->req_dataseg[i].ds_base);
		ISP_IOXPUT_32(isp, rqsrc->req_dataseg[i].ds_count,
		    &rqdst->req_dataseg[i].ds_count);
	}
}

void
isp_put_marker(ispsoftc_t *isp, isp_marker_t *src, isp_marker_t *dst)
{
	int i;
	isp_put_hdr(isp, &src->mrk_header, &dst->mrk_header);
	ISP_IOXPUT_32(isp, src->mrk_handle, &dst->mrk_handle);
	if (ISP_IS_SBUS(isp)) {
		ISP_IOXPUT_8(isp, src->mrk_reserved0, &dst->mrk_target);
		ISP_IOXPUT_8(isp, src->mrk_target, &dst->mrk_reserved0);
	} else {
		ISP_IOXPUT_8(isp, src->mrk_reserved0, &dst->mrk_reserved0);
		ISP_IOXPUT_8(isp, src->mrk_target, &dst->mrk_target);
	}
	ISP_IOXPUT_16(isp, src->mrk_modifier, &dst->mrk_modifier);
	ISP_IOXPUT_16(isp, src->mrk_flags, &dst->mrk_flags);
	ISP_IOXPUT_16(isp, src->mrk_lun, &dst->mrk_lun);
	for (i = 0; i < ASIZE(src->mrk_reserved1); i++) {
		ISP_IOXPUT_8(isp, src->mrk_reserved1[i],
		    &dst->mrk_reserved1[i]);
	}
}

void
isp_put_marker_24xx(ispsoftc_t *isp,
    isp_marker_24xx_t *src, isp_marker_24xx_t *dst)
{
	int i;
	isp_put_hdr(isp, &src->mrk_header, &dst->mrk_header);
	ISP_IOXPUT_32(isp, src->mrk_handle, &dst->mrk_handle);
	ISP_IOXPUT_16(isp, src->mrk_nphdl, &dst->mrk_nphdl);
	ISP_IOXPUT_8(isp, src->mrk_modifier, &dst->mrk_modifier);
	ISP_IOXPUT_8(isp, src->mrk_reserved0, &dst->mrk_reserved0);
	ISP_IOXPUT_8(isp, src->mrk_reserved1, &dst->mrk_reserved1);
	ISP_IOXPUT_8(isp, src->mrk_vphdl, &dst->mrk_vphdl);
	ISP_IOXPUT_8(isp, src->mrk_reserved2, &dst->mrk_reserved2);
	for (i = 0; i < ASIZE(src->mrk_lun); i++) {
		ISP_IOXPUT_8(isp, src->mrk_lun[i], &dst->mrk_lun[i]);
	}
	for (i = 0; i < ASIZE(src->mrk_reserved3); i++) {
		ISP_IOXPUT_8(isp, src->mrk_reserved3[i],
		    &dst->mrk_reserved3[i]);
	}
}

void
isp_put_request_t2(ispsoftc_t *isp, ispreqt2_t *src, ispreqt2_t *dst)
{
	int i;
	isp_put_hdr(isp, &src->req_header, &dst->req_header);
	ISP_IOXPUT_32(isp, src->req_handle, &dst->req_handle);
	ISP_IOXPUT_8(isp, src->req_lun_trn, &dst->req_lun_trn);
	ISP_IOXPUT_8(isp, src->req_target, &dst->req_target);
	ISP_IOXPUT_16(isp, src->req_scclun, &dst->req_scclun);
	ISP_IOXPUT_16(isp, src->req_flags,  &dst->req_flags);
	ISP_IOXPUT_16(isp, src->req_reserved, &dst->req_reserved);
	ISP_IOXPUT_16(isp, src->req_time, &dst->req_time);
	ISP_IOXPUT_16(isp, src->req_seg_count, &dst->req_seg_count);
	for (i = 0; i < ASIZE(src->req_cdb); i++) {
		ISP_IOXPUT_8(isp, src->req_cdb[i], &dst->req_cdb[i]);
	}
	ISP_IOXPUT_32(isp, src->req_totalcnt, &dst->req_totalcnt);
	for (i = 0; i < ISP_RQDSEG_T2; i++) {
		ISP_IOXPUT_32(isp, src->req_dataseg[i].ds_base,
		    &dst->req_dataseg[i].ds_base);
		ISP_IOXPUT_32(isp, src->req_dataseg[i].ds_count,
		    &dst->req_dataseg[i].ds_count);
	}
}

void
isp_put_request_t2e(ispsoftc_t *isp, ispreqt2e_t *src, ispreqt2e_t *dst)
{
	int i;
	isp_put_hdr(isp, &src->req_header, &dst->req_header);
	ISP_IOXPUT_32(isp, src->req_handle, &dst->req_handle);
	ISP_IOXPUT_16(isp, src->req_target, &dst->req_target);
	ISP_IOXPUT_16(isp, src->req_scclun, &dst->req_scclun);
	ISP_IOXPUT_16(isp, src->req_flags,  &dst->req_flags);
	ISP_IOXPUT_16(isp, src->req_reserved, &dst->req_reserved);
	ISP_IOXPUT_16(isp, src->req_time, &dst->req_time);
	ISP_IOXPUT_16(isp, src->req_seg_count, &dst->req_seg_count);
	for (i = 0; i < ASIZE(src->req_cdb); i++) {
		ISP_IOXPUT_8(isp, src->req_cdb[i], &dst->req_cdb[i]);
	}
	ISP_IOXPUT_32(isp, src->req_totalcnt, &dst->req_totalcnt);
	for (i = 0; i < ISP_RQDSEG_T2; i++) {
		ISP_IOXPUT_32(isp, src->req_dataseg[i].ds_base,
		    &dst->req_dataseg[i].ds_base);
		ISP_IOXPUT_32(isp, src->req_dataseg[i].ds_count,
		    &dst->req_dataseg[i].ds_count);
	}
}

void
isp_put_request_t3(ispsoftc_t *isp, ispreqt3_t *src, ispreqt3_t *dst)
{
	int i;
	isp_put_hdr(isp, &src->req_header, &dst->req_header);
	ISP_IOXPUT_32(isp, src->req_handle, &dst->req_handle);
	ISP_IOXPUT_8(isp, src->req_lun_trn, &dst->req_lun_trn);
	ISP_IOXPUT_8(isp, src->req_target, &dst->req_target);
	ISP_IOXPUT_16(isp, src->req_scclun, &dst->req_scclun);
	ISP_IOXPUT_16(isp, src->req_flags,  &dst->req_flags);
	ISP_IOXPUT_16(isp, src->req_reserved, &dst->req_reserved);
	ISP_IOXPUT_16(isp, src->req_time, &dst->req_time);
	ISP_IOXPUT_16(isp, src->req_seg_count, &dst->req_seg_count);
	for (i = 0; i < ASIZE(src->req_cdb); i++) {
		ISP_IOXPUT_8(isp, src->req_cdb[i], &dst->req_cdb[i]);
	}
	ISP_IOXPUT_32(isp, src->req_totalcnt, &dst->req_totalcnt);
	for (i = 0; i < ISP_RQDSEG_T3; i++) {
		ISP_IOXPUT_32(isp, src->req_dataseg[i].ds_base,
		    &dst->req_dataseg[i].ds_base);
		ISP_IOXPUT_32(isp, src->req_dataseg[i].ds_basehi,
		    &dst->req_dataseg[i].ds_basehi);
		ISP_IOXPUT_32(isp, src->req_dataseg[i].ds_count,
		    &dst->req_dataseg[i].ds_count);
	}
}

void
isp_put_request_t3e(ispsoftc_t *isp, ispreqt3e_t *src, ispreqt3e_t *dst)
{
	int i;
	isp_put_hdr(isp, &src->req_header, &dst->req_header);
	ISP_IOXPUT_32(isp, src->req_handle, &dst->req_handle);
	ISP_IOXPUT_16(isp, src->req_target, &dst->req_target);
	ISP_IOXPUT_16(isp, src->req_scclun, &dst->req_scclun);
	ISP_IOXPUT_16(isp, src->req_flags,  &dst->req_flags);
	ISP_IOXPUT_16(isp, src->req_reserved, &dst->req_reserved);
	ISP_IOXPUT_16(isp, src->req_time, &dst->req_time);
	ISP_IOXPUT_16(isp, src->req_seg_count, &dst->req_seg_count);
	for (i = 0; i < ASIZE(src->req_cdb); i++) {
		ISP_IOXPUT_8(isp, src->req_cdb[i], &dst->req_cdb[i]);
	}
	ISP_IOXPUT_32(isp, src->req_totalcnt, &dst->req_totalcnt);
	for (i = 0; i < ISP_RQDSEG_T3; i++) {
		ISP_IOXPUT_32(isp, src->req_dataseg[i].ds_base,
		    &dst->req_dataseg[i].ds_base);
		ISP_IOXPUT_32(isp, src->req_dataseg[i].ds_basehi,
		    &dst->req_dataseg[i].ds_basehi);
		ISP_IOXPUT_32(isp, src->req_dataseg[i].ds_count,
		    &dst->req_dataseg[i].ds_count);
	}
}

void
isp_put_extended_request(ispsoftc_t *isp, ispextreq_t *src, ispextreq_t *dst)
{
	int i;
	isp_put_hdr(isp, &src->req_header, &dst->req_header);
	ISP_IOXPUT_32(isp, src->req_handle, &dst->req_handle);
	if (ISP_IS_SBUS(isp)) {
		ISP_IOXPUT_8(isp, src->req_lun_trn, &dst->req_target);
		ISP_IOXPUT_8(isp, src->req_target, &dst->req_lun_trn);
	} else {
		ISP_IOXPUT_8(isp, src->req_lun_trn, &dst->req_lun_trn);
		ISP_IOXPUT_8(isp, src->req_target, &dst->req_target);
	}
	ISP_IOXPUT_16(isp, src->req_cdblen, &dst->req_cdblen);
	ISP_IOXPUT_16(isp, src->req_flags, &dst->req_flags);
	ISP_IOXPUT_16(isp, src->req_time, &dst->req_time);
	ISP_IOXPUT_16(isp, src->req_seg_count, &dst->req_seg_count);
	for (i = 0; i < ASIZE(src->req_cdb); i++) {
		ISP_IOXPUT_8(isp, src->req_cdb[i], &dst->req_cdb[i]);
	}
}

void
isp_put_request_t7(ispsoftc_t *isp, ispreqt7_t *src, ispreqt7_t *dst)
{
	int i;
	uint32_t *a, *b;

	isp_put_hdr(isp, &src->req_header, &dst->req_header);
	ISP_IOXPUT_32(isp, src->req_handle, &dst->req_handle);
	ISP_IOXPUT_16(isp, src->req_nphdl, &dst->req_nphdl);
	ISP_IOXPUT_16(isp, src->req_time, &dst->req_time);
	ISP_IOXPUT_16(isp, src->req_seg_count, &dst->req_seg_count);
	ISP_IOXPUT_16(isp, src->req_reserved, &dst->req_reserved);
	a = (uint32_t *) src->req_lun;
	b = (uint32_t *) dst->req_lun;
	for (i = 0; i < (ASIZE(src->req_lun) >> 2); i++ ) {
		*b++ = ISP_SWAP32(isp, *a++);
	}
	ISP_IOXPUT_8(isp, src->req_alen_datadir, &dst->req_alen_datadir);
	ISP_IOXPUT_8(isp, src->req_task_management, &dst->req_task_management);
	ISP_IOXPUT_8(isp, src->req_task_attribute, &dst->req_task_attribute);
	ISP_IOXPUT_8(isp, src->req_crn, &dst->req_crn);
	a = (uint32_t *) src->req_cdb;
	b = (uint32_t *) dst->req_cdb;
	for (i = 0; i < (ASIZE(src->req_cdb) >> 2); i++) {
		*b++ = ISP_SWAP32(isp, *a++);
	}
	ISP_IOXPUT_32(isp, src->req_dl, &dst->req_dl);
	ISP_IOXPUT_16(isp, src->req_tidlo, &dst->req_tidlo);
	ISP_IOXPUT_8(isp, src->req_tidhi, &dst->req_tidhi);
	ISP_IOXPUT_8(isp, src->req_vpidx, &dst->req_vpidx);
	ISP_IOXPUT_32(isp, src->req_dataseg.ds_base,
	    &dst->req_dataseg.ds_base);
	ISP_IOXPUT_32(isp, src->req_dataseg.ds_basehi,
	    &dst->req_dataseg.ds_basehi);
	ISP_IOXPUT_32(isp, src->req_dataseg.ds_count,
	    &dst->req_dataseg.ds_count);
}

void
isp_put_24xx_abrt(ispsoftc_t *isp, isp24xx_abrt_t *src, isp24xx_abrt_t *dst)
{
	int i;
	isp_put_hdr(isp, &src->abrt_header, &dst->abrt_header);
	ISP_IOXPUT_32(isp, src->abrt_handle, &dst->abrt_handle);
	ISP_IOXPUT_16(isp, src->abrt_nphdl, &dst->abrt_nphdl);
	ISP_IOXPUT_16(isp, src->abrt_options, &dst->abrt_options);
	ISP_IOXPUT_32(isp, src->abrt_cmd_handle, &dst->abrt_cmd_handle);
	for (i = 0; i < ASIZE(src->abrt_reserved); i++) {
		ISP_IOXPUT_8(isp, src->abrt_reserved[i],
		    &dst->abrt_reserved[i]);
	}
	ISP_IOXPUT_16(isp, src->abrt_tidlo, &dst->abrt_tidlo);
	ISP_IOXPUT_8(isp, src->abrt_tidhi, &dst->abrt_tidhi);
	ISP_IOXPUT_8(isp, src->abrt_vpidx, &dst->abrt_vpidx);
	for (i = 0; i < ASIZE(src->abrt_reserved1); i++) {
		ISP_IOXPUT_8(isp, src->abrt_reserved1[i],
		    &dst->abrt_reserved1[i]);
	}
}

void
isp_put_cont_req(ispsoftc_t *isp, ispcontreq_t *src, ispcontreq_t *dst)
{
	int i;
	isp_put_hdr(isp, &src->req_header, &dst->req_header);
	for (i = 0; i < ISP_CDSEG; i++) {
		ISP_IOXPUT_32(isp, src->req_dataseg[i].ds_base,
		    &dst->req_dataseg[i].ds_base);
		ISP_IOXPUT_32(isp, src->req_dataseg[i].ds_count,
		    &dst->req_dataseg[i].ds_count);
	}
}

void
isp_put_cont64_req(ispsoftc_t *isp, ispcontreq64_t *src, ispcontreq64_t *dst)
{
	int i;
	isp_put_hdr(isp, &src->req_header, &dst->req_header);
	for (i = 0; i < ISP_CDSEG64; i++) {
		ISP_IOXPUT_32(isp, src->req_dataseg[i].ds_base,
		    &dst->req_dataseg[i].ds_base);
		ISP_IOXPUT_32(isp, src->req_dataseg[i].ds_basehi,
		    &dst->req_dataseg[i].ds_basehi);
		ISP_IOXPUT_32(isp, src->req_dataseg[i].ds_count,
		    &dst->req_dataseg[i].ds_count);
	}
}

void
isp_get_response(ispsoftc_t *isp, ispstatusreq_t *src, ispstatusreq_t *dst)
{
	int i;
	isp_get_hdr(isp, &src->req_header, &dst->req_header);
	ISP_IOXGET_32(isp, &src->req_handle, dst->req_handle);
	ISP_IOXGET_16(isp, &src->req_scsi_status, dst->req_scsi_status);
	ISP_IOXGET_16(isp, &src->req_completion_status,
	    dst->req_completion_status);
	ISP_IOXGET_16(isp, &src->req_state_flags, dst->req_state_flags);
	ISP_IOXGET_16(isp, &src->req_status_flags, dst->req_status_flags);
	ISP_IOXGET_16(isp, &src->req_time, dst->req_time);
	ISP_IOXGET_16(isp, &src->req_sense_len, dst->req_sense_len);
	ISP_IOXGET_32(isp, &src->req_resid, dst->req_resid);
	for (i = 0; i < 8; i++) {
		ISP_IOXGET_8(isp, &src->req_response[i],
		    dst->req_response[i]);
	}
	for (i = 0; i < 32; i++) {
		ISP_IOXGET_8(isp, &src->req_sense_data[i],
		    dst->req_sense_data[i]);
	}
}

void
isp_get_24xx_response(ispsoftc_t *isp, isp24xx_statusreq_t *src,
    isp24xx_statusreq_t *dst)
{
	int i;
	uint32_t *s, *d;

	isp_get_hdr(isp, &src->req_header, &dst->req_header);
	ISP_IOXGET_32(isp, &src->req_handle, dst->req_handle);
	ISP_IOXGET_16(isp, &src->req_completion_status,
	    dst->req_completion_status);
	ISP_IOXGET_16(isp, &src->req_oxid, dst->req_oxid);
	ISP_IOXGET_32(isp, &src->req_resid, dst->req_resid);
	ISP_IOXGET_16(isp, &src->req_reserved0, dst->req_reserved0);
	ISP_IOXGET_16(isp, &src->req_state_flags, dst->req_state_flags);
	ISP_IOXGET_16(isp, &src->req_reserved1, dst->req_reserved1);
	ISP_IOXGET_16(isp, &src->req_scsi_status, dst->req_scsi_status);
	ISP_IOXGET_32(isp, &src->req_fcp_residual, dst->req_fcp_residual);
	ISP_IOXGET_32(isp, &src->req_sense_len, dst->req_sense_len);
	ISP_IOXGET_32(isp, &src->req_response_len, dst->req_response_len);
	s = (uint32_t *)src->req_rsp_sense;
	d = (uint32_t *)dst->req_rsp_sense;
	for (i = 0; i < (ASIZE(src->req_rsp_sense) >> 2); i++) {
		d[i] = ISP_SWAP32(isp, s[i]);
	}
}

void
isp_get_24xx_abrt(ispsoftc_t *isp, isp24xx_abrt_t *src, isp24xx_abrt_t *dst)
{
	int i;
	isp_get_hdr(isp, &src->abrt_header, &dst->abrt_header);
	ISP_IOXGET_32(isp, &src->abrt_handle, dst->abrt_handle);
	ISP_IOXGET_16(isp, &src->abrt_nphdl, dst->abrt_nphdl);
	ISP_IOXGET_16(isp, &src->abrt_options, dst->abrt_options);
	ISP_IOXGET_32(isp, &src->abrt_cmd_handle, dst->abrt_cmd_handle);
	for (i = 0; i < ASIZE(src->abrt_reserved); i++) {
		ISP_IOXGET_8(isp, &src->abrt_reserved[i],
		    dst->abrt_reserved[i]);
	}
	ISP_IOXGET_16(isp, &src->abrt_tidlo, dst->abrt_tidlo);
	ISP_IOXGET_8(isp, &src->abrt_tidhi, dst->abrt_tidhi);
	ISP_IOXGET_8(isp, &src->abrt_vpidx, dst->abrt_vpidx);
	for (i = 0; i < ASIZE(src->abrt_reserved1); i++) {
		ISP_IOXGET_8(isp, &src->abrt_reserved1[i],
		    dst->abrt_reserved1[i]);
	}
}


void
isp_get_rio2(ispsoftc_t *isp, isp_rio2_t *r2src, isp_rio2_t *r2dst)
{
	int i;
	isp_get_hdr(isp, &r2src->req_header, &r2dst->req_header);
	if (r2dst->req_header.rqs_seqno > 30) {
		r2dst->req_header.rqs_seqno = 30;
	}
	for (i = 0; i < r2dst->req_header.rqs_seqno; i++) {
		ISP_IOXGET_16(isp, &r2src->req_handles[i],
		    r2dst->req_handles[i]);
	}
	while (i < 30) {
		r2dst->req_handles[i++] = 0;
	}
}

void
isp_put_icb(ispsoftc_t *isp, isp_icb_t *src, isp_icb_t *dst)
{
	int i;
	if (ISP_IS_SBUS(isp)) {
		ISP_IOXPUT_8(isp, src->icb_version, &dst->icb_reserved0);
		ISP_IOXPUT_8(isp, src->icb_reserved0, &dst->icb_version);
	} else {
		ISP_IOXPUT_8(isp, src->icb_version, &dst->icb_version);
		ISP_IOXPUT_8(isp, src->icb_reserved0, &dst->icb_reserved0);
	}
	ISP_IOXPUT_16(isp, src->icb_fwoptions, &dst->icb_fwoptions);
	ISP_IOXPUT_16(isp, src->icb_maxfrmlen, &dst->icb_maxfrmlen);
	ISP_IOXPUT_16(isp, src->icb_maxalloc, &dst->icb_maxalloc);
	ISP_IOXPUT_16(isp, src->icb_execthrottle, &dst->icb_execthrottle);
	if (ISP_IS_SBUS(isp)) {
		ISP_IOXPUT_8(isp, src->icb_retry_count, &dst->icb_retry_delay);
		ISP_IOXPUT_8(isp, src->icb_retry_delay, &dst->icb_retry_count);
	} else {
		ISP_IOXPUT_8(isp, src->icb_retry_count, &dst->icb_retry_count);
		ISP_IOXPUT_8(isp, src->icb_retry_delay, &dst->icb_retry_delay);
	}
	for (i = 0; i < 8; i++) {
		ISP_IOXPUT_8(isp, src->icb_portname[i], &dst->icb_portname[i]);
	}
	ISP_IOXPUT_16(isp, src->icb_hardaddr, &dst->icb_hardaddr);
	if (ISP_IS_SBUS(isp)) {
		ISP_IOXPUT_8(isp, src->icb_iqdevtype, &dst->icb_logintime);
		ISP_IOXPUT_8(isp, src->icb_logintime, &dst->icb_iqdevtype);
	} else {
		ISP_IOXPUT_8(isp, src->icb_iqdevtype, &dst->icb_iqdevtype);
		ISP_IOXPUT_8(isp, src->icb_logintime, &dst->icb_logintime);
	}
	for (i = 0; i < 8; i++) {
		ISP_IOXPUT_8(isp, src->icb_nodename[i], &dst->icb_nodename[i]);
	}
	ISP_IOXPUT_16(isp, src->icb_rqstout, &dst->icb_rqstout);
	ISP_IOXPUT_16(isp, src->icb_rspnsin, &dst->icb_rspnsin);
	ISP_IOXPUT_16(isp, src->icb_rqstqlen, &dst->icb_rqstqlen);
	ISP_IOXPUT_16(isp, src->icb_rsltqlen, &dst->icb_rsltqlen);
	for (i = 0; i < 4; i++) {
		ISP_IOXPUT_16(isp, src->icb_rqstaddr[i], &dst->icb_rqstaddr[i]);
	}
	for (i = 0; i < 4; i++) {
		ISP_IOXPUT_16(isp, src->icb_respaddr[i], &dst->icb_respaddr[i]);
	}
	ISP_IOXPUT_16(isp, src->icb_lunenables, &dst->icb_lunenables);
	if (ISP_IS_SBUS(isp)) {
		ISP_IOXPUT_8(isp, src->icb_ccnt, &dst->icb_icnt);
		ISP_IOXPUT_8(isp, src->icb_icnt, &dst->icb_ccnt);
	} else {
		ISP_IOXPUT_8(isp, src->icb_ccnt, &dst->icb_ccnt);
		ISP_IOXPUT_8(isp, src->icb_icnt, &dst->icb_icnt);
	}
	ISP_IOXPUT_16(isp, src->icb_lunetimeout, &dst->icb_lunetimeout);
	ISP_IOXPUT_16(isp, src->icb_reserved1, &dst->icb_reserved1);
	ISP_IOXPUT_16(isp, src->icb_xfwoptions, &dst->icb_xfwoptions);
	if (ISP_IS_SBUS(isp)) {
		ISP_IOXPUT_8(isp, src->icb_racctimer, &dst->icb_idelaytimer);
		ISP_IOXPUT_8(isp, src->icb_idelaytimer, &dst->icb_racctimer);
	} else {
		ISP_IOXPUT_8(isp, src->icb_racctimer, &dst->icb_racctimer);
		ISP_IOXPUT_8(isp, src->icb_idelaytimer, &dst->icb_idelaytimer);
	}
	ISP_IOXPUT_16(isp, src->icb_zfwoptions, &dst->icb_zfwoptions);
}

void
isp_put_icb_2400(ispsoftc_t *isp, isp_icb_2400_t *src, isp_icb_2400_t *dst)
{
	int i;
	ISP_IOXPUT_16(isp, src->icb_version, &dst->icb_version);
	ISP_IOXPUT_16(isp, src->icb_reserved0, &dst->icb_reserved0);
	ISP_IOXPUT_16(isp, src->icb_maxfrmlen, &dst->icb_maxfrmlen);
	ISP_IOXPUT_16(isp, src->icb_execthrottle, &dst->icb_execthrottle);
	ISP_IOXPUT_16(isp, src->icb_xchgcnt, &dst->icb_xchgcnt);
	ISP_IOXPUT_16(isp, src->icb_hardaddr, &dst->icb_hardaddr);
	for (i = 0; i < 8; i++) {
		ISP_IOXPUT_8(isp, src->icb_portname[i], &dst->icb_portname[i]);
	}
	for (i = 0; i < 8; i++) {
		ISP_IOXPUT_8(isp, src->icb_nodename[i], &dst->icb_nodename[i]);
	}
	ISP_IOXPUT_16(isp, src->icb_rspnsin, &dst->icb_rspnsin);
	ISP_IOXPUT_16(isp, src->icb_rqstout, &dst->icb_rqstout);
	ISP_IOXPUT_16(isp, src->icb_retry_count, &dst->icb_retry_count);
	ISP_IOXPUT_16(isp, src->icb_priout, &dst->icb_priout);
	ISP_IOXPUT_16(isp, src->icb_rsltqlen, &dst->icb_rsltqlen);
	ISP_IOXPUT_16(isp, src->icb_rqstqlen, &dst->icb_rqstqlen);
	ISP_IOXPUT_16(isp, src->icb_ldn_nols, &dst->icb_ldn_nols);
	ISP_IOXPUT_16(isp, src->icb_prqstqlen, &dst->icb_prqstqlen);
	for (i = 0; i < 4; i++) {
		ISP_IOXPUT_16(isp, src->icb_rqstaddr[i], &dst->icb_rqstaddr[i]);
	}
	for (i = 0; i < 4; i++) {
		ISP_IOXPUT_16(isp, src->icb_respaddr[i], &dst->icb_respaddr[i]);
	}
	for (i = 0; i < 4; i++) {
		ISP_IOXPUT_16(isp, src->icb_priaddr[i], &dst->icb_priaddr[i]);
	}
	for (i = 0; i < 4; i++) {
		ISP_IOXPUT_16(isp, src->icb_reserved1[i],
		    &dst->icb_reserved1[i]);
	}
	ISP_IOXPUT_16(isp, src->icb_atio_in, &dst->icb_atio_in);
	ISP_IOXPUT_16(isp, src->icb_atioqlen, &dst->icb_atioqlen);
	for (i = 0; i < 4; i++) {
		ISP_IOXPUT_16(isp, src->icb_atioqaddr[i],
		    &dst->icb_atioqaddr[i]);
	}
	ISP_IOXPUT_16(isp, src->icb_idelaytimer, &dst->icb_idelaytimer);
	ISP_IOXPUT_16(isp, src->icb_logintime, &dst->icb_logintime);
	ISP_IOXPUT_32(isp, src->icb_fwoptions1, &dst->icb_fwoptions1);
	ISP_IOXPUT_32(isp, src->icb_fwoptions2, &dst->icb_fwoptions2);
	ISP_IOXPUT_32(isp, src->icb_fwoptions3, &dst->icb_fwoptions3);
	for (i = 0; i < 12; i++) {
		ISP_IOXPUT_16(isp, src->icb_reserved2[i],
		    &dst->icb_reserved2[i]);
	}
}

void
isp_get_pdb_21xx(ispsoftc_t *isp, isp_pdb_21xx_t *src, isp_pdb_21xx_t *dst)
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

void
isp_get_pdb_24xx(ispsoftc_t *isp, isp_pdb_24xx_t *src, isp_pdb_24xx_t *dst)
{
	int i;
	ISP_IOXGET_16(isp, &src->pdb_flags, dst->pdb_flags);
        ISP_IOXGET_8(isp, &src->pdb_curstate, dst->pdb_curstate);
        ISP_IOXGET_8(isp, &src->pdb_laststate, dst->pdb_laststate);
	for (i = 0; i < 4; i++) {
		ISP_IOXGET_8(isp, &src->pdb_hardaddr_bits[i],
		    dst->pdb_hardaddr_bits[i]);
	}
	for (i = 0; i < 4; i++) {
		ISP_IOXGET_8(isp, &src->pdb_portid_bits[i],
		    dst->pdb_portid_bits[i]);
	}
	ISP_IOXGET_16(isp, &src->pdb_retry_timer, dst->pdb_retry_timer);
	ISP_IOXGET_16(isp, &src->pdb_handle, dst->pdb_handle);
	ISP_IOXGET_16(isp, &src->pdb_rcv_dsize, dst->pdb_rcv_dsize);
	ISP_IOXGET_16(isp, &src->pdb_reserved0, dst->pdb_reserved0);
	ISP_IOXGET_16(isp, &src->pdb_prli_svc0, dst->pdb_prli_svc0);
	ISP_IOXGET_16(isp, &src->pdb_prli_svc3, dst->pdb_prli_svc3);
	for (i = 0; i < 8; i++) {
		ISP_IOXGET_8(isp, &src->pdb_nodename[i], dst->pdb_nodename[i]);
	}
	for (i = 0; i < 8; i++) {
		ISP_IOXGET_8(isp, &src->pdb_portname[i], dst->pdb_portname[i]);
	}
	for (i = 0; i < 24; i++) {
		ISP_IOXGET_8(isp, &src->pdb_reserved1[i],
		    dst->pdb_reserved1[i]);
	}
}

/*
 * PLOGI/LOGO IOCB canonicalization
 */

void
isp_get_plogx(ispsoftc_t *isp, isp_plogx_t *src, isp_plogx_t *dst)
{
	int i;
	isp_get_hdr(isp, &src->plogx_header, &dst->plogx_header);
	ISP_IOXGET_32(isp, &src->plogx_handle, dst->plogx_handle);
	ISP_IOXGET_16(isp, &src->plogx_status, dst->plogx_status);
	ISP_IOXGET_16(isp, &src->plogx_nphdl, dst->plogx_nphdl);
	ISP_IOXGET_16(isp, &src->plogx_flags, dst->plogx_flags);
	ISP_IOXGET_16(isp, &src->plogx_vphdl, dst->plogx_vphdl);
	ISP_IOXGET_16(isp, &src->plogx_portlo, dst->plogx_portlo);
	ISP_IOXGET_16(isp, &src->plogx_rspsz_porthi, dst->plogx_rspsz_porthi);
	for (i = 0; i < 11; i++) {
		ISP_IOXGET_16(isp, &src->plogx_ioparm[i].lo16,
		    dst->plogx_ioparm[i].lo16);
		ISP_IOXGET_16(isp, &src->plogx_ioparm[i].hi16,
		    dst->plogx_ioparm[i].hi16);
	}
}

void
isp_put_plogx(ispsoftc_t *isp, isp_plogx_t *src, isp_plogx_t *dst)
{
	int i;
	isp_put_hdr(isp, &src->plogx_header, &dst->plogx_header);
	ISP_IOXPUT_32(isp, src->plogx_handle, &dst->plogx_handle);
	ISP_IOXPUT_16(isp, src->plogx_status, &dst->plogx_status);
	ISP_IOXPUT_16(isp, src->plogx_nphdl, &dst->plogx_nphdl);
	ISP_IOXPUT_16(isp, src->plogx_flags, &dst->plogx_flags);
	ISP_IOXPUT_16(isp, src->plogx_vphdl, &dst->plogx_vphdl);
	ISP_IOXPUT_16(isp, src->plogx_portlo, &dst->plogx_portlo);
	ISP_IOXPUT_16(isp, src->plogx_rspsz_porthi, &dst->plogx_rspsz_porthi);
	for (i = 0; i < 11; i++) {
		ISP_IOXPUT_16(isp, src->plogx_ioparm[i].lo16,
		    &dst->plogx_ioparm[i].lo16);
		ISP_IOXPUT_16(isp, src->plogx_ioparm[i].hi16,
		    &dst->plogx_ioparm[i].hi16);
	}
}

/*
 * CT Passthru canonicalization
 */
void
isp_get_ct_pt(ispsoftc_t *isp, isp_ct_pt_t *src, isp_ct_pt_t *dst)
{
	int i;

	isp_get_hdr(isp, &src->ctp_header, &dst->ctp_header);
	ISP_IOXGET_32(isp, &src->ctp_handle, dst->ctp_handle);
	ISP_IOXGET_16(isp, &src->ctp_status, dst->ctp_status);
	ISP_IOXGET_16(isp, &src->ctp_nphdl, dst->ctp_nphdl);
	ISP_IOXGET_16(isp, &src->ctp_cmd_cnt, dst->ctp_cmd_cnt);
	ISP_IOXGET_16(isp, &src->ctp_vpidx, dst->ctp_vpidx);
	ISP_IOXGET_16(isp, &src->ctp_time, dst->ctp_time);
	ISP_IOXGET_16(isp, &src->ctp_reserved0, dst->ctp_reserved0);
	ISP_IOXGET_16(isp, &src->ctp_rsp_cnt, dst->ctp_rsp_cnt);
	for (i = 0; i < 5; i++) {
		ISP_IOXGET_16(isp, &src->ctp_reserved1[i],
		    dst->ctp_reserved1[i]);
	}
	ISP_IOXGET_32(isp, &src->ctp_rsp_bcnt, dst->ctp_rsp_bcnt);
	ISP_IOXGET_32(isp, &src->ctp_cmd_bcnt, dst->ctp_cmd_bcnt);
	for (i = 0; i < 2; i++) {
		ISP_IOXGET_32(isp, &src->ctp_dataseg[i].ds_base,
		    dst->ctp_dataseg[i].ds_base);
		ISP_IOXGET_32(isp, &src->ctp_dataseg[i].ds_basehi,
		    dst->ctp_dataseg[i].ds_basehi);
		ISP_IOXGET_32(isp, &src->ctp_dataseg[i].ds_count,
		    dst->ctp_dataseg[i].ds_count);
	}
}

void
isp_get_ms(ispsoftc_t *isp, isp_ms_t *src, isp_ms_t *dst)
{
	int i;

	isp_get_hdr(isp, &src->ms_header, &dst->ms_header);
	ISP_IOXGET_32(isp, &src->ms_handle, dst->ms_handle);
	ISP_IOXGET_16(isp, &src->ms_nphdl, dst->ms_nphdl);
	ISP_IOXGET_16(isp, &src->ms_status, dst->ms_status);
	ISP_IOXGET_16(isp, &src->ms_flags, dst->ms_flags);
	ISP_IOXGET_16(isp, &src->ms_reserved1, dst->ms_reserved1);
	ISP_IOXGET_16(isp, &src->ms_time, dst->ms_time);
	ISP_IOXGET_16(isp, &src->ms_cmd_cnt, dst->ms_cmd_cnt);
	ISP_IOXGET_16(isp, &src->ms_tot_cnt, dst->ms_tot_cnt);
	ISP_IOXGET_8(isp, &src->ms_type, dst->ms_type);
	ISP_IOXGET_8(isp, &src->ms_r_ctl, dst->ms_r_ctl);
	ISP_IOXGET_16(isp, &src->ms_rxid, dst->ms_rxid);
	ISP_IOXGET_16(isp, &src->ms_reserved2, dst->ms_reserved2);
	ISP_IOXGET_32(isp, &src->ms_rsp_bcnt, dst->ms_rsp_bcnt);
	ISP_IOXGET_32(isp, &src->ms_cmd_bcnt, dst->ms_cmd_bcnt);
	for (i = 0; i < 2; i++) {
		ISP_IOXGET_32(isp, &src->ms_dataseg[i].ds_base,
		    dst->ms_dataseg[i].ds_base);
		ISP_IOXGET_32(isp, &src->ms_dataseg[i].ds_basehi,
		    dst->ms_dataseg[i].ds_basehi);
		ISP_IOXGET_32(isp, &src->ms_dataseg[i].ds_count,
		    dst->ms_dataseg[i].ds_count);
	}
}

void
isp_put_ct_pt(ispsoftc_t *isp, isp_ct_pt_t *src, isp_ct_pt_t *dst)
{
	int i;

	isp_put_hdr(isp, &src->ctp_header, &dst->ctp_header);
	ISP_IOXPUT_32(isp, src->ctp_handle, &dst->ctp_handle);
	ISP_IOXPUT_16(isp, src->ctp_status, &dst->ctp_status);
	ISP_IOXPUT_16(isp, src->ctp_nphdl, &dst->ctp_nphdl);
	ISP_IOXPUT_16(isp, src->ctp_cmd_cnt, &dst->ctp_cmd_cnt);
	ISP_IOXPUT_16(isp, src->ctp_vpidx, &dst->ctp_vpidx);
	ISP_IOXPUT_16(isp, src->ctp_time, &dst->ctp_time);
	ISP_IOXPUT_16(isp, src->ctp_reserved0, &dst->ctp_reserved0);
	ISP_IOXPUT_16(isp, src->ctp_rsp_cnt, &dst->ctp_rsp_cnt);
	for (i = 0; i < 5; i++) {
		ISP_IOXPUT_16(isp, src->ctp_reserved1[i],
		    &dst->ctp_reserved1[i]);
	}
	ISP_IOXPUT_32(isp, src->ctp_rsp_bcnt, &dst->ctp_rsp_bcnt);
	ISP_IOXPUT_32(isp, src->ctp_cmd_bcnt, &dst->ctp_cmd_bcnt);
	for (i = 0; i < 2; i++) {
		ISP_IOXPUT_32(isp, src->ctp_dataseg[i].ds_base,
		    &dst->ctp_dataseg[i].ds_base);
		ISP_IOXPUT_32(isp, src->ctp_dataseg[i].ds_basehi,
		    &dst->ctp_dataseg[i].ds_basehi);
		ISP_IOXPUT_32(isp, src->ctp_dataseg[i].ds_count,
		    &dst->ctp_dataseg[i].ds_count);
	}
}

void
isp_put_ms(ispsoftc_t *isp, isp_ms_t *src, isp_ms_t *dst)
{
	int i;

	isp_put_hdr(isp, &src->ms_header, &dst->ms_header);
	ISP_IOXPUT_32(isp, src->ms_handle, &dst->ms_handle);
	ISP_IOXPUT_16(isp, src->ms_nphdl, &dst->ms_nphdl);
	ISP_IOXPUT_16(isp, src->ms_status, &dst->ms_status);
	ISP_IOXPUT_16(isp, src->ms_flags, &dst->ms_flags);
	ISP_IOXPUT_16(isp, src->ms_reserved1, &dst->ms_reserved1);
	ISP_IOXPUT_16(isp, src->ms_time, &dst->ms_time);
	ISP_IOXPUT_16(isp, src->ms_cmd_cnt, &dst->ms_cmd_cnt);
	ISP_IOXPUT_16(isp, src->ms_tot_cnt, &dst->ms_tot_cnt);
	ISP_IOXPUT_8(isp, src->ms_type, &dst->ms_type);
	ISP_IOXPUT_8(isp, src->ms_r_ctl, &dst->ms_r_ctl);
	ISP_IOXPUT_16(isp, src->ms_rxid, &dst->ms_rxid);
	ISP_IOXPUT_16(isp, src->ms_reserved2, &dst->ms_reserved2);
	ISP_IOXPUT_32(isp, src->ms_rsp_bcnt, &dst->ms_rsp_bcnt);
	ISP_IOXPUT_32(isp, src->ms_cmd_bcnt, &dst->ms_cmd_bcnt);
	for (i = 0; i < 2; i++) {
		ISP_IOXPUT_32(isp, src->ms_dataseg[i].ds_base,
		    &dst->ms_dataseg[i].ds_base);
		ISP_IOXPUT_32(isp, src->ms_dataseg[i].ds_basehi,
		    &dst->ms_dataseg[i].ds_basehi);
		ISP_IOXPUT_32(isp, src->ms_dataseg[i].ds_count,
		    &dst->ms_dataseg[i].ds_count);
	}
}

/*
 * Generic SNS request - not particularly useful since the per-command data
 * isn't always 16 bit words.
 */
void
isp_put_sns_request(ispsoftc_t *isp, sns_screq_t *src, sns_screq_t *dst)
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

void
isp_put_gid_ft_request(ispsoftc_t *isp, sns_gid_ft_req_t *src,
    sns_gid_ft_req_t *dst)
{
	ISP_IOXPUT_16(isp, src->snscb_rblen, &dst->snscb_rblen);
	ISP_IOXPUT_16(isp, src->snscb_reserved0, &dst->snscb_reserved0);
	ISP_IOXPUT_16(isp, src->snscb_addr[0], &dst->snscb_addr[0]);
	ISP_IOXPUT_16(isp, src->snscb_addr[1], &dst->snscb_addr[1]);
	ISP_IOXPUT_16(isp, src->snscb_addr[2], &dst->snscb_addr[2]);
	ISP_IOXPUT_16(isp, src->snscb_addr[3], &dst->snscb_addr[3]);
	ISP_IOXPUT_16(isp, src->snscb_sblen, &dst->snscb_sblen);
	ISP_IOXPUT_16(isp, src->snscb_reserved1, &dst->snscb_reserved1);
	ISP_IOXPUT_16(isp, src->snscb_cmd, &dst->snscb_cmd);
	ISP_IOXPUT_16(isp, src->snscb_mword_div_2, &dst->snscb_mword_div_2);
	ISP_IOXPUT_32(isp, src->snscb_reserved3, &dst->snscb_reserved3);
	ISP_IOXPUT_32(isp, src->snscb_fc4_type, &dst->snscb_fc4_type);
}

void
isp_put_gxn_id_request(ispsoftc_t *isp, sns_gxn_id_req_t *src,
    sns_gxn_id_req_t *dst)
{
	ISP_IOXPUT_16(isp, src->snscb_rblen, &dst->snscb_rblen);
	ISP_IOXPUT_16(isp, src->snscb_reserved0, &dst->snscb_reserved0);
	ISP_IOXPUT_16(isp, src->snscb_addr[0], &dst->snscb_addr[0]);
	ISP_IOXPUT_16(isp, src->snscb_addr[1], &dst->snscb_addr[1]);
	ISP_IOXPUT_16(isp, src->snscb_addr[2], &dst->snscb_addr[2]);
	ISP_IOXPUT_16(isp, src->snscb_addr[3], &dst->snscb_addr[3]);
	ISP_IOXPUT_16(isp, src->snscb_sblen, &dst->snscb_sblen);
	ISP_IOXPUT_16(isp, src->snscb_reserved1, &dst->snscb_reserved1);
	ISP_IOXPUT_16(isp, src->snscb_cmd, &dst->snscb_cmd);
	ISP_IOXPUT_16(isp, src->snscb_reserved2, &dst->snscb_reserved2);
	ISP_IOXPUT_32(isp, src->snscb_reserved3, &dst->snscb_reserved3);
	ISP_IOXPUT_32(isp, src->snscb_portid, &dst->snscb_portid);
}

/*
 * Generic SNS response - not particularly useful since the per-command data
 * isn't always 16 bit words.
 */
void
isp_get_sns_response(ispsoftc_t *isp, sns_scrsp_t *src,
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

void
isp_get_gid_ft_response(ispsoftc_t *isp, sns_gid_ft_rsp_t *src,
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

void
isp_get_gxn_id_response(ispsoftc_t *isp, sns_gxn_id_rsp_t *src,
    sns_gxn_id_rsp_t *dst)
{
	int i;
	isp_get_ct_hdr(isp, &src->snscb_cthdr, &dst->snscb_cthdr);
	for (i = 0; i < 8; i++)
		ISP_IOXGET_8(isp, &src->snscb_wwn[i], dst->snscb_wwn[i]);
}

void
isp_get_gff_id_response(ispsoftc_t *isp, sns_gff_id_rsp_t *src,
    sns_gff_id_rsp_t *dst)
{
	int i;
	isp_get_ct_hdr(isp, &src->snscb_cthdr, &dst->snscb_cthdr);
	for (i = 0; i < 32; i++) {
		ISP_IOXGET_32(isp, &src->snscb_fc4_features[i],
		    dst->snscb_fc4_features[i]);
	}
}

void
isp_get_ga_nxt_response(ispsoftc_t *isp, sns_ga_nxt_rsp_t *src,
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

void
isp_get_els(ispsoftc_t *isp, els_t *src, els_t *dst)
{
	int i;

	isp_get_hdr(isp, &src->els_hdr, &dst->els_hdr);
	ISP_IOXGET_32(isp, &src->els_handle, dst->els_handle);
	ISP_IOXGET_16(isp, &src->els_status, dst->els_status);
	ISP_IOXGET_16(isp, &src->els_nphdl, dst->els_nphdl);
	ISP_IOXGET_16(isp, &src->els_xmit_dsd_count, dst->els_xmit_dsd_count);
	ISP_IOXGET_8(isp, &src->els_vphdl, dst->els_vphdl);
	ISP_IOXGET_8(isp, &src->els_sof, dst->els_sof);
	ISP_IOXGET_32(isp, &src->els_rxid, dst->els_rxid);
	ISP_IOXGET_16(isp, &src->els_recv_dsd_count, dst->els_recv_dsd_count);
	ISP_IOXGET_8(isp, &src->els_opcode, dst->els_opcode);
	ISP_IOXGET_8(isp, &src->els_reserved2, dst->els_reserved1);
	ISP_IOXGET_8(isp, &src->els_did_lo, dst->els_did_lo);
	ISP_IOXGET_8(isp, &src->els_did_mid, dst->els_did_mid);
	ISP_IOXGET_8(isp, &src->els_did_hi, dst->els_did_hi);
	ISP_IOXGET_8(isp, &src->els_reserved2, dst->els_reserved2);
	ISP_IOXGET_16(isp, &src->els_reserved3, dst->els_reserved3);
	ISP_IOXGET_16(isp, &src->els_ctl_flags, dst->els_ctl_flags);
	ISP_IOXGET_32(isp, &src->els_bytecnt, dst->els_bytecnt);
	ISP_IOXGET_32(isp, &src->els_subcode1, dst->els_subcode1);
	ISP_IOXGET_32(isp, &src->els_subcode2, dst->els_subcode2);
	for (i = 0; i < 20; i++) {
		ISP_IOXGET_8(isp, &src->els_reserved4[i],
		    dst->els_reserved4[i]);
	}
}

void
isp_put_els(ispsoftc_t *isp, els_t *src, els_t *dst)
{
	isp_put_hdr(isp, &src->els_hdr, &dst->els_hdr);
	ISP_IOXPUT_32(isp, src->els_handle, &dst->els_handle);
	ISP_IOXPUT_16(isp, src->els_status, &dst->els_status);
	ISP_IOXPUT_16(isp, src->els_nphdl, &dst->els_nphdl);
	ISP_IOXPUT_16(isp, src->els_xmit_dsd_count, &dst->els_xmit_dsd_count);
	ISP_IOXPUT_8(isp, src->els_vphdl, &dst->els_vphdl);
	ISP_IOXPUT_8(isp, src->els_sof, &dst->els_sof);
	ISP_IOXPUT_32(isp, src->els_rxid, &dst->els_rxid);
	ISP_IOXPUT_16(isp, src->els_recv_dsd_count, &dst->els_recv_dsd_count);
	ISP_IOXPUT_8(isp, src->els_opcode, &dst->els_opcode);
	ISP_IOXPUT_8(isp, src->els_reserved2, &dst->els_reserved1);
	ISP_IOXPUT_8(isp, src->els_did_lo, &dst->els_did_lo);
	ISP_IOXPUT_8(isp, src->els_did_mid, &dst->els_did_mid);
	ISP_IOXPUT_8(isp, src->els_did_hi, &dst->els_did_hi);
	ISP_IOXPUT_8(isp, src->els_reserved2, &dst->els_reserved2);
	ISP_IOXPUT_16(isp, src->els_reserved3, &dst->els_reserved3);
	ISP_IOXPUT_16(isp, src->els_ctl_flags, &dst->els_ctl_flags);
	ISP_IOXPUT_32(isp, src->els_recv_bytecnt, &dst->els_recv_bytecnt);
	ISP_IOXPUT_32(isp, src->els_xmit_bytecnt, &dst->els_xmit_bytecnt);
	ISP_IOXPUT_32(isp, src->els_xmit_dsd_length, &dst->els_xmit_dsd_length);
	ISP_IOXPUT_16(isp, src->els_xmit_dsd_a1500, &dst->els_xmit_dsd_a1500);
	ISP_IOXPUT_16(isp, src->els_xmit_dsd_a3116, &dst->els_xmit_dsd_a3116);
	ISP_IOXPUT_16(isp, src->els_xmit_dsd_a4732, &dst->els_xmit_dsd_a4732);
	ISP_IOXPUT_16(isp, src->els_xmit_dsd_a6348, &dst->els_xmit_dsd_a6348);
	ISP_IOXPUT_32(isp, src->els_recv_dsd_length, &dst->els_recv_dsd_length);
	ISP_IOXPUT_16(isp, src->els_recv_dsd_a1500, &dst->els_recv_dsd_a1500);
	ISP_IOXPUT_16(isp, src->els_recv_dsd_a3116, &dst->els_recv_dsd_a3116);
	ISP_IOXPUT_16(isp, src->els_recv_dsd_a4732, &dst->els_recv_dsd_a4732);
	ISP_IOXPUT_16(isp, src->els_recv_dsd_a6348, &dst->els_recv_dsd_a6348);
}

/*
 * FC Structure Canonicalization
 */

void
isp_get_fc_hdr(ispsoftc_t *isp, fc_hdr_t *src, fc_hdr_t *dst)
{
        ISP_IOZGET_8(isp, &src->r_ctl, dst->r_ctl);
        ISP_IOZGET_8(isp, &src->d_id[0], dst->d_id[0]);
        ISP_IOZGET_8(isp, &src->d_id[1], dst->d_id[1]);
        ISP_IOZGET_8(isp, &src->d_id[2], dst->d_id[2]);
        ISP_IOZGET_8(isp, &src->cs_ctl, dst->cs_ctl);
        ISP_IOZGET_8(isp, &src->s_id[0], dst->s_id[0]);
        ISP_IOZGET_8(isp, &src->s_id[1], dst->s_id[1]);
        ISP_IOZGET_8(isp, &src->s_id[2], dst->s_id[2]);
        ISP_IOZGET_8(isp, &src->type, dst->type);
        ISP_IOZGET_8(isp, &src->f_ctl, dst->f_ctl);
        ISP_IOZGET_8(isp, &src->seq_id, dst->seq_id);
        ISP_IOZGET_8(isp, &src->df_ctl, dst->df_ctl);
        ISP_IOZGET_16(isp, &src->seq_cnt, dst->seq_cnt);
	/* XXX SOMETHING WAS AND STILL CONTINUES WRONG HERE XXX */
#if	0
        ISP_IOZGET_16(isp, &src->ox_id, dst->ox_id);
        ISP_IOZGET_16(isp, &src->rx_id, dst->rx_id);
#else
        ISP_IOZGET_32(isp, &src->ox_id, dst->parameter);
        dst->ox_id = dst->parameter;
        dst->rx_id = dst->parameter >> 16;
#endif
        ISP_IOZGET_32(isp, &src->parameter, dst->parameter);
}

void
isp_get_fcp_cmnd_iu(ispsoftc_t *isp, fcp_cmnd_iu_t *src, fcp_cmnd_iu_t *dst)
{
	int i;

	for (i = 0; i < 8; i++) {
		ISP_IOZGET_8(isp, &src->fcp_cmnd_lun[i], dst->fcp_cmnd_lun[i]);
	}
        ISP_IOZGET_8(isp, &src->fcp_cmnd_crn, dst->fcp_cmnd_crn);
        ISP_IOZGET_8(isp, &src->fcp_cmnd_task_attribute,
	    dst->fcp_cmnd_task_attribute);
        ISP_IOZGET_8(isp, &src->fcp_cmnd_task_management,
	    dst->fcp_cmnd_task_management);
        ISP_IOZGET_8(isp, &src->fcp_cmnd_alen_datadir,
	    dst->fcp_cmnd_alen_datadir);
	for (i = 0; i < 16; i++) {
		ISP_IOZGET_8(isp, &src->cdb_dl.sf.fcp_cmnd_cdb[i],
		    dst->cdb_dl.sf.fcp_cmnd_cdb[i]);
	}
	ISP_IOZGET_32(isp, &src->cdb_dl.sf.fcp_cmnd_dl,
	    dst->cdb_dl.sf.fcp_cmnd_dl);
}

void
isp_put_rft_id(ispsoftc_t *isp, rft_id_t *src, rft_id_t *dst)
{
	int i;
	isp_put_ct_hdr(isp, &src->rftid_hdr, &dst->rftid_hdr);
	ISP_IOZPUT_8(isp, src->rftid_reserved, &dst->rftid_reserved);
	for (i = 0; i < 3; i++) {
		ISP_IOZPUT_8(isp, src->rftid_portid[i], &dst->rftid_portid[i]);
	}
	for (i = 0; i < 8; i++) {
		ISP_IOZPUT_32(isp, src->rftid_fc4types[i],
		    &dst->rftid_fc4types[i]);
	}
}

void
isp_get_ct_hdr(ispsoftc_t *isp, ct_hdr_t *src, ct_hdr_t *dst)
{
	ISP_IOZGET_8(isp, &src->ct_revision, dst->ct_revision);
	ISP_IOZGET_8(isp, &src->ct_in_id[0], dst->ct_in_id[0]);
	ISP_IOZGET_8(isp, &src->ct_in_id[1], dst->ct_in_id[1]);
	ISP_IOZGET_8(isp, &src->ct_in_id[2], dst->ct_in_id[2]);
	ISP_IOZGET_8(isp, &src->ct_fcs_type, dst->ct_fcs_type);
	ISP_IOZGET_8(isp, &src->ct_fcs_subtype, dst->ct_fcs_subtype);
	ISP_IOZGET_8(isp, &src->ct_options, dst->ct_options);
	ISP_IOZGET_8(isp, &src->ct_reserved0, dst->ct_reserved0);
	ISP_IOZGET_16(isp, &src->ct_cmd_resp, dst->ct_cmd_resp);
	ISP_IOZGET_16(isp, &src->ct_bcnt_resid, dst->ct_bcnt_resid);
	ISP_IOZGET_8(isp, &src->ct_reserved1, dst->ct_reserved1);
	ISP_IOZGET_8(isp, &src->ct_reason, dst->ct_reason);
	ISP_IOZGET_8(isp, &src->ct_explanation, dst->ct_explanation);
	ISP_IOZGET_8(isp, &src->ct_vunique, dst->ct_vunique);
}

void
isp_put_ct_hdr(ispsoftc_t *isp, ct_hdr_t *src, ct_hdr_t *dst)
{
	ISP_IOZPUT_8(isp, src->ct_revision, &dst->ct_revision);
	ISP_IOZPUT_8(isp, src->ct_in_id[0], &dst->ct_in_id[0]);
	ISP_IOZPUT_8(isp, src->ct_in_id[1], &dst->ct_in_id[1]);
	ISP_IOZPUT_8(isp, src->ct_in_id[2], &dst->ct_in_id[2]);
	ISP_IOZPUT_8(isp, src->ct_fcs_type, &dst->ct_fcs_type);
	ISP_IOZPUT_8(isp, src->ct_fcs_subtype, &dst->ct_fcs_subtype);
	ISP_IOZPUT_8(isp, src->ct_options, &dst->ct_options);
	ISP_IOZPUT_8(isp, src->ct_reserved0, &dst->ct_reserved0);
	ISP_IOZPUT_16(isp, src->ct_cmd_resp, &dst->ct_cmd_resp);
	ISP_IOZPUT_16(isp, src->ct_bcnt_resid, &dst->ct_bcnt_resid);
	ISP_IOZPUT_8(isp, src->ct_reserved1, &dst->ct_reserved1);
	ISP_IOZPUT_8(isp, src->ct_reason, &dst->ct_reason);
	ISP_IOZPUT_8(isp, src->ct_explanation, &dst->ct_explanation);
	ISP_IOZPUT_8(isp, src->ct_vunique, &dst->ct_vunique);
}

#ifdef	ISP_TARGET_MODE
int
isp_save_xs_tgt(ispsoftc_t *isp, void *xs, uint32_t *handlep)
{
	int i;

	for (i = 0; i < (int) isp->isp_maxcmds; i++) {
		if (isp->isp_tgtlist[i] == NULL) {
			break;
		}
	}
	if (i == isp->isp_maxcmds) {
		return (-1);
	}
	isp->isp_tgtlist[i] = xs;
	*handlep = (i+1) | 0x8000;
	return (0);
}

void *
isp_find_xs_tgt(ispsoftc_t *isp, uint32_t handle)
{
	if (handle == 0 || IS_TARGET_HANDLE(handle) == 0 ||
	    (handle & ISP_HANDLE_MASK) > isp->isp_maxcmds) {
		isp_prt(isp, ISP_LOGERR, "bad handle in isp_find_xs_tgt");
		return (NULL);
	} else {
		return (isp->isp_tgtlist[(handle & ISP_HANDLE_MASK) - 1]);
	}
}

uint32_t
isp_find_tgt_handle(ispsoftc_t *isp, void *xs)
{
	int i;
	if (xs != NULL) {
		for (i = 0; i < isp->isp_maxcmds; i++) {
			if (isp->isp_tgtlist[i] == xs) {
				return ((i+1) & ISP_HANDLE_MASK);
			}
		}
	}
	return (0);
}

void
isp_destroy_tgt_handle(ispsoftc_t *isp, uint32_t handle)
{
	if (handle == 0 || IS_TARGET_HANDLE(handle) == 0 ||
	    (handle & ISP_HANDLE_MASK) > isp->isp_maxcmds) {
		isp_prt(isp, ISP_LOGERR,
		    "bad handle in isp_destroy_tgt_handle");
	} else {
		isp->isp_tgtlist[(handle & ISP_HANDLE_MASK) - 1] = NULL;
	}
}

void
isp_put_atio(ispsoftc_t *isp, at_entry_t *src, at_entry_t *dst)
{
	int i;
	isp_put_hdr(isp, &src->at_header, &dst->at_header);
	ISP_IOXPUT_16(isp, src->at_reserved, &dst->at_reserved);
	ISP_IOXPUT_16(isp, src->at_handle, &dst->at_handle);
	if (ISP_IS_SBUS(isp)) {
		ISP_IOXPUT_8(isp, src->at_lun, &dst->at_iid);
		ISP_IOXPUT_8(isp, src->at_iid, &dst->at_lun);
		ISP_IOXPUT_8(isp, src->at_cdblen, &dst->at_tgt);
		ISP_IOXPUT_8(isp, src->at_tgt, &dst->at_cdblen);
		ISP_IOXPUT_8(isp, src->at_status, &dst->at_scsi_status);
		ISP_IOXPUT_8(isp, src->at_scsi_status, &dst->at_status);
		ISP_IOXPUT_8(isp, src->at_tag_val, &dst->at_tag_type);
		ISP_IOXPUT_8(isp, src->at_tag_type, &dst->at_tag_val);
	} else {
		ISP_IOXPUT_8(isp, src->at_lun, &dst->at_lun);
		ISP_IOXPUT_8(isp, src->at_iid, &dst->at_iid);
		ISP_IOXPUT_8(isp, src->at_cdblen, &dst->at_cdblen);
		ISP_IOXPUT_8(isp, src->at_tgt, &dst->at_tgt);
		ISP_IOXPUT_8(isp, src->at_status, &dst->at_status);
		ISP_IOXPUT_8(isp, src->at_scsi_status,
		    &dst->at_scsi_status);
		ISP_IOXPUT_8(isp, src->at_tag_val, &dst->at_tag_val);
		ISP_IOXPUT_8(isp, src->at_tag_type, &dst->at_tag_type);
	}
	ISP_IOXPUT_32(isp, src->at_flags, &dst->at_flags);
	for (i = 0; i < ATIO_CDBLEN; i++) {
		ISP_IOXPUT_8(isp, src->at_cdb[i], &dst->at_cdb[i]);
	}
	for (i = 0; i < QLTM_SENSELEN; i++) {
		ISP_IOXPUT_8(isp, src->at_sense[i], &dst->at_sense[i]);
	}
}

void
isp_get_atio(ispsoftc_t *isp, at_entry_t *src, at_entry_t *dst)
{
	int i;
	isp_get_hdr(isp, &src->at_header, &dst->at_header);
	ISP_IOXGET_16(isp, &src->at_reserved, dst->at_reserved);
	ISP_IOXGET_16(isp, &src->at_handle, dst->at_handle);
	if (ISP_IS_SBUS(isp)) {
		ISP_IOXGET_8(isp, &src->at_lun, dst->at_iid);
		ISP_IOXGET_8(isp, &src->at_iid, dst->at_lun);
		ISP_IOXGET_8(isp, &src->at_cdblen, dst->at_tgt);
		ISP_IOXGET_8(isp, &src->at_tgt, dst->at_cdblen);
		ISP_IOXGET_8(isp, &src->at_status, dst->at_scsi_status);
		ISP_IOXGET_8(isp, &src->at_scsi_status, dst->at_status);
		ISP_IOXGET_8(isp, &src->at_tag_val, dst->at_tag_type);
		ISP_IOXGET_8(isp, &src->at_tag_type, dst->at_tag_val);
	} else {
		ISP_IOXGET_8(isp, &src->at_lun, dst->at_lun);
		ISP_IOXGET_8(isp, &src->at_iid, dst->at_iid);
		ISP_IOXGET_8(isp, &src->at_cdblen, dst->at_cdblen);
		ISP_IOXGET_8(isp, &src->at_tgt, dst->at_tgt);
		ISP_IOXGET_8(isp, &src->at_status, dst->at_status);
		ISP_IOXGET_8(isp, &src->at_scsi_status,
		    dst->at_scsi_status);
		ISP_IOXGET_8(isp, &src->at_tag_val, dst->at_tag_val);
		ISP_IOXGET_8(isp, &src->at_tag_type, dst->at_tag_type);
	}
	ISP_IOXGET_32(isp, &src->at_flags, dst->at_flags);
	for (i = 0; i < ATIO_CDBLEN; i++) {
		ISP_IOXGET_8(isp, &src->at_cdb[i], dst->at_cdb[i]);
	}
	for (i = 0; i < QLTM_SENSELEN; i++) {
		ISP_IOXGET_8(isp, &src->at_sense[i], dst->at_sense[i]);
	}
}

void
isp_put_atio2(ispsoftc_t *isp, at2_entry_t *src, at2_entry_t *dst)
{
	int i;
	isp_put_hdr(isp, &src->at_header, &dst->at_header);
	ISP_IOXPUT_32(isp, src->at_reserved, &dst->at_reserved);
	ISP_IOXPUT_8(isp, src->at_lun, &dst->at_lun);
	ISP_IOXPUT_8(isp, src->at_iid, &dst->at_iid);
	ISP_IOXPUT_16(isp, src->at_rxid, &dst->at_rxid);
	ISP_IOXPUT_16(isp, src->at_flags, &dst->at_flags);
	ISP_IOXPUT_16(isp, src->at_status, &dst->at_status);
	ISP_IOXPUT_8(isp, src->at_crn, &dst->at_crn);
	ISP_IOXPUT_8(isp, src->at_taskcodes, &dst->at_taskcodes);
	ISP_IOXPUT_8(isp, src->at_taskflags, &dst->at_taskflags);
	ISP_IOXPUT_8(isp, src->at_execodes, &dst->at_execodes);
	for (i = 0; i < ATIO2_CDBLEN; i++) {
		ISP_IOXPUT_8(isp, src->at_cdb[i], &dst->at_cdb[i]);
	}
	ISP_IOXPUT_32(isp, src->at_datalen, &dst->at_datalen);
	ISP_IOXPUT_16(isp, src->at_scclun, &dst->at_scclun);
	for (i = 0; i < 4; i++) {
		ISP_IOXPUT_16(isp, src->at_wwpn[i], &dst->at_wwpn[i]);
	}
	for (i = 0; i < 6; i++) {
		ISP_IOXPUT_16(isp, src->at_reserved2[i],
		    &dst->at_reserved2[i]);
	}
	ISP_IOXPUT_16(isp, src->at_oxid, &dst->at_oxid);
}

void
isp_put_atio2e(ispsoftc_t *isp, at2e_entry_t *src, at2e_entry_t *dst)
{
	int i;
	isp_put_hdr(isp, &src->at_header, &dst->at_header);
	ISP_IOXPUT_32(isp, src->at_reserved, &dst->at_reserved);
	ISP_IOXPUT_16(isp, src->at_iid, &dst->at_iid);
	ISP_IOXPUT_16(isp, src->at_rxid, &dst->at_rxid);
	ISP_IOXPUT_16(isp, src->at_flags, &dst->at_flags);
	ISP_IOXPUT_16(isp, src->at_status, &dst->at_status);
	ISP_IOXPUT_8(isp, src->at_crn, &dst->at_crn);
	ISP_IOXPUT_8(isp, src->at_taskcodes, &dst->at_taskcodes);
	ISP_IOXPUT_8(isp, src->at_taskflags, &dst->at_taskflags);
	ISP_IOXPUT_8(isp, src->at_execodes, &dst->at_execodes);
	for (i = 0; i < ATIO2_CDBLEN; i++) {
		ISP_IOXPUT_8(isp, src->at_cdb[i], &dst->at_cdb[i]);
	}
	ISP_IOXPUT_32(isp, src->at_datalen, &dst->at_datalen);
	ISP_IOXPUT_16(isp, src->at_scclun, &dst->at_scclun);
	for (i = 0; i < 4; i++) {
		ISP_IOXPUT_16(isp, src->at_wwpn[i], &dst->at_wwpn[i]);
	}
	for (i = 0; i < 6; i++) {
		ISP_IOXPUT_16(isp, src->at_reserved2[i],
		    &dst->at_reserved2[i]);
	}
	ISP_IOXPUT_16(isp, src->at_oxid, &dst->at_oxid);
}

void
isp_get_atio2(ispsoftc_t *isp, at2_entry_t *src, at2_entry_t *dst)
{
	int i;
	isp_get_hdr(isp, &src->at_header, &dst->at_header);
	ISP_IOXGET_32(isp, &src->at_reserved, dst->at_reserved);
	ISP_IOXGET_8(isp, &src->at_lun, dst->at_lun);
	ISP_IOXGET_8(isp, &src->at_iid, dst->at_iid);
	ISP_IOXGET_16(isp, &src->at_rxid, dst->at_rxid);
	ISP_IOXGET_16(isp, &src->at_flags, dst->at_flags);
	ISP_IOXGET_16(isp, &src->at_status, dst->at_status);
	ISP_IOXGET_8(isp, &src->at_crn, dst->at_crn);
	ISP_IOXGET_8(isp, &src->at_taskcodes, dst->at_taskcodes);
	ISP_IOXGET_8(isp, &src->at_taskflags, dst->at_taskflags);
	ISP_IOXGET_8(isp, &src->at_execodes, dst->at_execodes);
	for (i = 0; i < ATIO2_CDBLEN; i++) {
		ISP_IOXGET_8(isp, &src->at_cdb[i], dst->at_cdb[i]);
	}
	ISP_IOXGET_32(isp, &src->at_datalen, dst->at_datalen);
	ISP_IOXGET_16(isp, &src->at_scclun, dst->at_scclun);
	for (i = 0; i < 4; i++) {
		ISP_IOXGET_16(isp, &src->at_wwpn[i], dst->at_wwpn[i]);
	}
	for (i = 0; i < 6; i++) {
		ISP_IOXGET_16(isp, &src->at_reserved2[i],
		    dst->at_reserved2[i]);
	}
	ISP_IOXGET_16(isp, &src->at_oxid, dst->at_oxid);
}

void
isp_get_atio2e(ispsoftc_t *isp, at2e_entry_t *src, at2e_entry_t *dst)
{
	int i;
	isp_get_hdr(isp, &src->at_header, &dst->at_header);
	ISP_IOXGET_32(isp, &src->at_reserved, dst->at_reserved);
	ISP_IOXGET_16(isp, &src->at_iid, dst->at_iid);
	ISP_IOXGET_16(isp, &src->at_rxid, dst->at_rxid);
	ISP_IOXGET_16(isp, &src->at_flags, dst->at_flags);
	ISP_IOXGET_16(isp, &src->at_status, dst->at_status);
	ISP_IOXGET_8(isp, &src->at_crn, dst->at_crn);
	ISP_IOXGET_8(isp, &src->at_taskcodes, dst->at_taskcodes);
	ISP_IOXGET_8(isp, &src->at_taskflags, dst->at_taskflags);
	ISP_IOXGET_8(isp, &src->at_execodes, dst->at_execodes);
	for (i = 0; i < ATIO2_CDBLEN; i++) {
		ISP_IOXGET_8(isp, &src->at_cdb[i], dst->at_cdb[i]);
	}
	ISP_IOXGET_32(isp, &src->at_datalen, dst->at_datalen);
	ISP_IOXGET_16(isp, &src->at_scclun, dst->at_scclun);
	for (i = 0; i < 4; i++) {
		ISP_IOXGET_16(isp, &src->at_wwpn[i], dst->at_wwpn[i]);
	}
	for (i = 0; i < 6; i++) {
		ISP_IOXGET_16(isp, &src->at_reserved2[i],
		    dst->at_reserved2[i]);
	}
	ISP_IOXGET_16(isp, &src->at_oxid, dst->at_oxid);
}

void
isp_get_atio7(ispsoftc_t *isp, at7_entry_t *src, at7_entry_t *dst)
{
	ISP_IOXGET_8(isp, &src->at_type, dst->at_type);
	ISP_IOXGET_8(isp, &src->at_count, dst->at_count);
	ISP_IOXGET_16(isp, &src->at_ta_len, dst->at_ta_len);
	ISP_IOXGET_32(isp, &src->at_rxid, dst->at_rxid);
	isp_get_fc_hdr(isp, &src->at_hdr, &dst->at_hdr);
	isp_get_fcp_cmnd_iu(isp, &src->at_cmnd, &dst->at_cmnd);
}

void
isp_put_ctio(ispsoftc_t *isp, ct_entry_t *src, ct_entry_t *dst)
{
	int i;
	isp_put_hdr(isp, &src->ct_header, &dst->ct_header);
	ISP_IOXPUT_16(isp, src->ct_syshandle, &dst->ct_syshandle);
	ISP_IOXPUT_16(isp, src->ct_fwhandle, &dst->ct_fwhandle);
	if (ISP_IS_SBUS(isp)) {
		ISP_IOXPUT_8(isp, src->ct_iid, &dst->ct_lun);
		ISP_IOXPUT_8(isp, src->ct_lun, &dst->ct_iid);
		ISP_IOXPUT_8(isp, src->ct_tgt, &dst->ct_reserved2);
		ISP_IOXPUT_8(isp, src->ct_reserved2, &dst->ct_tgt);
		ISP_IOXPUT_8(isp, src->ct_status, &dst->ct_scsi_status);
		ISP_IOXPUT_8(isp, src->ct_scsi_status, &dst->ct_status);
		ISP_IOXPUT_8(isp, src->ct_tag_type, &dst->ct_tag_val);
		ISP_IOXPUT_8(isp, src->ct_tag_val, &dst->ct_tag_type);
	} else {
		ISP_IOXPUT_8(isp, src->ct_iid, &dst->ct_iid);
		ISP_IOXPUT_8(isp, src->ct_lun, &dst->ct_lun);
		ISP_IOXPUT_8(isp, src->ct_tgt, &dst->ct_tgt);
		ISP_IOXPUT_8(isp, src->ct_reserved2, &dst->ct_reserved2);
		ISP_IOXPUT_8(isp, src->ct_scsi_status,
		    &dst->ct_scsi_status);
		ISP_IOXPUT_8(isp, src->ct_status, &dst->ct_status);
		ISP_IOXPUT_8(isp, src->ct_tag_type, &dst->ct_tag_type);
		ISP_IOXPUT_8(isp, src->ct_tag_val, &dst->ct_tag_val);
	}
	ISP_IOXPUT_32(isp, src->ct_flags, &dst->ct_flags);
	ISP_IOXPUT_32(isp, src->ct_xfrlen, &dst->ct_xfrlen);
	ISP_IOXPUT_32(isp, src->ct_resid, &dst->ct_resid);
	ISP_IOXPUT_16(isp, src->ct_timeout, &dst->ct_timeout);
	ISP_IOXPUT_16(isp, src->ct_seg_count, &dst->ct_seg_count);
	for (i = 0; i < ISP_RQDSEG; i++) {
		ISP_IOXPUT_32(isp, src->ct_dataseg[i].ds_base,
		    &dst->ct_dataseg[i].ds_base);
		ISP_IOXPUT_32(isp, src->ct_dataseg[i].ds_count,
		    &dst->ct_dataseg[i].ds_count);
	}
}

void
isp_get_ctio(ispsoftc_t *isp, ct_entry_t *src, ct_entry_t *dst)
{
	int i;
	isp_get_hdr(isp, &src->ct_header, &dst->ct_header);
	ISP_IOXGET_16(isp, &src->ct_syshandle, dst->ct_syshandle);
	ISP_IOXGET_16(isp, &src->ct_fwhandle, dst->ct_fwhandle);
	if (ISP_IS_SBUS(isp)) {
		ISP_IOXGET_8(isp, &src->ct_lun, dst->ct_iid);
		ISP_IOXGET_8(isp, &src->ct_iid, dst->ct_lun);
		ISP_IOXGET_8(isp, &src->ct_reserved2, dst->ct_tgt);
		ISP_IOXGET_8(isp, &src->ct_tgt, dst->ct_reserved2);
		ISP_IOXGET_8(isp, &src->ct_status, dst->ct_scsi_status);
		ISP_IOXGET_8(isp, &src->ct_scsi_status, dst->ct_status);
		ISP_IOXGET_8(isp, &src->ct_tag_val, dst->ct_tag_type);
		ISP_IOXGET_8(isp, &src->ct_tag_type, dst->ct_tag_val);
	} else {
		ISP_IOXGET_8(isp, &src->ct_lun, dst->ct_lun);
		ISP_IOXGET_8(isp, &src->ct_iid, dst->ct_iid);
		ISP_IOXGET_8(isp, &src->ct_reserved2, dst->ct_reserved2);
		ISP_IOXGET_8(isp, &src->ct_tgt, dst->ct_tgt);
		ISP_IOXGET_8(isp, &src->ct_status, dst->ct_status);
		ISP_IOXGET_8(isp, &src->ct_scsi_status,
		    dst->ct_scsi_status);
		ISP_IOXGET_8(isp, &src->ct_tag_val, dst->ct_tag_val);
		ISP_IOXGET_8(isp, &src->ct_tag_type, dst->ct_tag_type);
	}
	ISP_IOXGET_32(isp, &src->ct_flags, dst->ct_flags);
	ISP_IOXGET_32(isp, &src->ct_xfrlen, dst->ct_xfrlen);
	ISP_IOXGET_32(isp, &src->ct_resid, dst->ct_resid);
	ISP_IOXGET_16(isp, &src->ct_timeout, dst->ct_timeout);
	ISP_IOXGET_16(isp, &src->ct_seg_count, dst->ct_seg_count);
	for (i = 0; i < ISP_RQDSEG; i++) {
		ISP_IOXGET_32(isp,
		    &src->ct_dataseg[i].ds_base,
		    dst->ct_dataseg[i].ds_base);
		ISP_IOXGET_32(isp,
		    &src->ct_dataseg[i].ds_count,
		    dst->ct_dataseg[i].ds_count);
	}
}

void
isp_put_ctio2(ispsoftc_t *isp, ct2_entry_t *src, ct2_entry_t *dst)
{
	int i;
	isp_put_hdr(isp, &src->ct_header, &dst->ct_header);
	ISP_IOXPUT_32(isp, src->ct_syshandle, &dst->ct_syshandle);
	ISP_IOXPUT_8(isp, src->ct_lun, &dst->ct_lun);
	ISP_IOXPUT_8(isp, src->ct_iid, &dst->ct_iid);
	ISP_IOXPUT_16(isp, src->ct_rxid, &dst->ct_rxid);
	ISP_IOXPUT_16(isp, src->ct_flags, &dst->ct_flags);
	ISP_IOXPUT_16(isp, src->ct_timeout, &dst->ct_timeout);
	ISP_IOXPUT_16(isp, src->ct_seg_count, &dst->ct_seg_count);
	ISP_IOXPUT_32(isp, src->ct_resid, &dst->ct_resid);
	ISP_IOXPUT_32(isp, src->ct_reloff, &dst->ct_reloff);
	if ((src->ct_flags & CT2_FLAG_MMASK) == CT2_FLAG_MODE0) {
		ISP_IOXPUT_32(isp, src->rsp.m0._reserved,
		    &dst->rsp.m0._reserved);
		ISP_IOXPUT_16(isp, src->rsp.m0._reserved2,
		    &dst->rsp.m0._reserved2);
		ISP_IOXPUT_16(isp, src->rsp.m0.ct_scsi_status,
		    &dst->rsp.m0.ct_scsi_status);
		ISP_IOXPUT_32(isp, src->rsp.m0.ct_xfrlen,
		    &dst->rsp.m0.ct_xfrlen);
		if (src->ct_header.rqs_entry_type == RQSTYPE_CTIO2) {
			for (i = 0; i < ISP_RQDSEG_T2; i++) {
				ISP_IOXPUT_32(isp,
				    src->rsp.m0.u.ct_dataseg[i].ds_base,
				    &dst->rsp.m0.u.ct_dataseg[i].ds_base);
				ISP_IOXPUT_32(isp,
				    src->rsp.m0.u.ct_dataseg[i].ds_count,
				    &dst->rsp.m0.u.ct_dataseg[i].ds_count);
			}
		} else if (src->ct_header.rqs_entry_type == RQSTYPE_CTIO3) {
			for (i = 0; i < ISP_RQDSEG_T3; i++) {
				ISP_IOXPUT_32(isp,
				    src->rsp.m0.u.ct_dataseg64[i].ds_base,
				    &dst->rsp.m0.u.ct_dataseg64[i].ds_base);
				ISP_IOXPUT_32(isp,
				    src->rsp.m0.u.ct_dataseg64[i].ds_basehi,
				    &dst->rsp.m0.u.ct_dataseg64[i].ds_basehi);
				ISP_IOXPUT_32(isp,
				    src->rsp.m0.u.ct_dataseg64[i].ds_count,
				    &dst->rsp.m0.u.ct_dataseg64[i].ds_count);
			}
		} else if (src->ct_header.rqs_entry_type == RQSTYPE_CTIO4) {
			ISP_IOXPUT_16(isp, src->rsp.m0.u.ct_dslist.ds_type,
			    &dst->rsp.m0.u.ct_dslist.ds_type);
			ISP_IOXPUT_32(isp, src->rsp.m0.u.ct_dslist.ds_segment,
			    &dst->rsp.m0.u.ct_dslist.ds_segment);
			ISP_IOXPUT_32(isp, src->rsp.m0.u.ct_dslist.ds_base,
			    &dst->rsp.m0.u.ct_dslist.ds_base);
		}
	} else if ((src->ct_flags & CT2_FLAG_MMASK) == CT2_FLAG_MODE1) {
		ISP_IOXPUT_16(isp, src->rsp.m1._reserved,
		    &dst->rsp.m1._reserved);
		ISP_IOXPUT_16(isp, src->rsp.m1._reserved2,
		    &dst->rsp.m1._reserved2);
		ISP_IOXPUT_16(isp, src->rsp.m1.ct_senselen,
		    &dst->rsp.m1.ct_senselen);
		ISP_IOXPUT_16(isp, src->rsp.m1.ct_scsi_status,
		    &dst->rsp.m1.ct_scsi_status);
		ISP_IOXPUT_16(isp, src->rsp.m1.ct_resplen,
		    &dst->rsp.m1.ct_resplen);
		for (i = 0; i < MAXRESPLEN; i++) {
			ISP_IOXPUT_8(isp, src->rsp.m1.ct_resp[i],
			    &dst->rsp.m1.ct_resp[i]);
		}
	} else {
		ISP_IOXPUT_32(isp, src->rsp.m2._reserved,
		    &dst->rsp.m2._reserved);
		ISP_IOXPUT_16(isp, src->rsp.m2._reserved2,
		    &dst->rsp.m2._reserved2);
		ISP_IOXPUT_16(isp, src->rsp.m2._reserved3,
		    &dst->rsp.m2._reserved3);
		ISP_IOXPUT_32(isp, src->rsp.m2.ct_datalen,
		    &dst->rsp.m2.ct_datalen);
		ISP_IOXPUT_32(isp, src->rsp.m2.ct_fcp_rsp_iudata.ds_base,
		    &dst->rsp.m2.ct_fcp_rsp_iudata.ds_base);
		ISP_IOXPUT_32(isp, src->rsp.m2.ct_fcp_rsp_iudata.ds_count,
		    &dst->rsp.m2.ct_fcp_rsp_iudata.ds_count);
	}
}

void
isp_put_ctio2e(ispsoftc_t *isp, ct2e_entry_t *src, ct2e_entry_t *dst)
{
	int i;
	isp_put_hdr(isp, &src->ct_header, &dst->ct_header);
	ISP_IOXPUT_32(isp, src->ct_syshandle, &dst->ct_syshandle);
	ISP_IOXPUT_16(isp, src->ct_iid, &dst->ct_iid);
	ISP_IOXPUT_16(isp, src->ct_rxid, &dst->ct_rxid);
	ISP_IOXPUT_16(isp, src->ct_flags, &dst->ct_flags);
	ISP_IOXPUT_16(isp, src->ct_timeout, &dst->ct_timeout);
	ISP_IOXPUT_16(isp, src->ct_seg_count, &dst->ct_seg_count);
	ISP_IOXPUT_32(isp, src->ct_resid, &dst->ct_resid);
	ISP_IOXPUT_32(isp, src->ct_reloff, &dst->ct_reloff);
	if ((src->ct_flags & CT2_FLAG_MMASK) == CT2_FLAG_MODE0) {
		ISP_IOXPUT_32(isp, src->rsp.m0._reserved,
		    &dst->rsp.m0._reserved);
		ISP_IOXPUT_16(isp, src->rsp.m0._reserved2,
		    &dst->rsp.m0._reserved2);
		ISP_IOXPUT_16(isp, src->rsp.m0.ct_scsi_status,
		    &dst->rsp.m0.ct_scsi_status);
		ISP_IOXPUT_32(isp, src->rsp.m0.ct_xfrlen,
		    &dst->rsp.m0.ct_xfrlen);
		if (src->ct_header.rqs_entry_type == RQSTYPE_CTIO2) {
			for (i = 0; i < ISP_RQDSEG_T2; i++) {
				ISP_IOXPUT_32(isp,
				    src->rsp.m0.u.ct_dataseg[i].ds_base,
				    &dst->rsp.m0.u.ct_dataseg[i].ds_base);
				ISP_IOXPUT_32(isp,
				    src->rsp.m0.u.ct_dataseg[i].ds_count,
				    &dst->rsp.m0.u.ct_dataseg[i].ds_count);
			}
		} else if (src->ct_header.rqs_entry_type == RQSTYPE_CTIO3) {
			for (i = 0; i < ISP_RQDSEG_T3; i++) {
				ISP_IOXPUT_32(isp,
				    src->rsp.m0.u.ct_dataseg64[i].ds_base,
				    &dst->rsp.m0.u.ct_dataseg64[i].ds_base);
				ISP_IOXPUT_32(isp,
				    src->rsp.m0.u.ct_dataseg64[i].ds_basehi,
				    &dst->rsp.m0.u.ct_dataseg64[i].ds_basehi);
				ISP_IOXPUT_32(isp,
				    src->rsp.m0.u.ct_dataseg64[i].ds_count,
				    &dst->rsp.m0.u.ct_dataseg64[i].ds_count);
			}
		} else if (src->ct_header.rqs_entry_type == RQSTYPE_CTIO4) {
			ISP_IOXPUT_16(isp, src->rsp.m0.u.ct_dslist.ds_type,
			    &dst->rsp.m0.u.ct_dslist.ds_type);
			ISP_IOXPUT_32(isp, src->rsp.m0.u.ct_dslist.ds_segment,
			    &dst->rsp.m0.u.ct_dslist.ds_segment);
			ISP_IOXPUT_32(isp, src->rsp.m0.u.ct_dslist.ds_base,
			    &dst->rsp.m0.u.ct_dslist.ds_base);
		}
	} else if ((src->ct_flags & CT2_FLAG_MMASK) == CT2_FLAG_MODE1) {
		ISP_IOXPUT_16(isp, src->rsp.m1._reserved,
		    &dst->rsp.m1._reserved);
		ISP_IOXPUT_16(isp, src->rsp.m1._reserved2,
		    &dst->rsp.m1._reserved2);
		ISP_IOXPUT_16(isp, src->rsp.m1.ct_senselen,
		    &dst->rsp.m1.ct_senselen);
		ISP_IOXPUT_16(isp, src->rsp.m1.ct_scsi_status,
		    &dst->rsp.m1.ct_scsi_status);
		ISP_IOXPUT_16(isp, src->rsp.m1.ct_resplen,
		    &dst->rsp.m1.ct_resplen);
		for (i = 0; i < MAXRESPLEN; i++) {
			ISP_IOXPUT_8(isp, src->rsp.m1.ct_resp[i],
			    &dst->rsp.m1.ct_resp[i]);
		}
	} else {
		ISP_IOXPUT_32(isp, src->rsp.m2._reserved,
		    &dst->rsp.m2._reserved);
		ISP_IOXPUT_16(isp, src->rsp.m2._reserved2,
		    &dst->rsp.m2._reserved2);
		ISP_IOXPUT_16(isp, src->rsp.m2._reserved3,
		    &dst->rsp.m2._reserved3);
		ISP_IOXPUT_32(isp, src->rsp.m2.ct_datalen,
		    &dst->rsp.m2.ct_datalen);
		ISP_IOXPUT_32(isp, src->rsp.m2.ct_fcp_rsp_iudata.ds_base,
		    &dst->rsp.m2.ct_fcp_rsp_iudata.ds_base);
		ISP_IOXPUT_32(isp, src->rsp.m2.ct_fcp_rsp_iudata.ds_count,
		    &dst->rsp.m2.ct_fcp_rsp_iudata.ds_count);
	}
}

void
isp_put_ctio7(ispsoftc_t *isp, ct7_entry_t *src, ct7_entry_t *dst)
{
	int i;

	isp_put_hdr(isp, &src->ct_header, &dst->ct_header);
	ISP_IOXPUT_32(isp, src->ct_syshandle, &dst->ct_syshandle);
	ISP_IOXPUT_16(isp, src->ct_nphdl, &dst->ct_nphdl);
	ISP_IOXPUT_16(isp, src->ct_timeout, &dst->ct_timeout);
	ISP_IOXPUT_16(isp, src->ct_seg_count, &dst->ct_seg_count);
	ISP_IOXPUT_8(isp, src->ct_vpindex, &dst->ct_vpindex);
	ISP_IOXPUT_8(isp, src->ct_xflags, &dst->ct_xflags);
	ISP_IOXPUT_16(isp, src->ct_iid_lo, &dst->ct_iid_lo);
	ISP_IOXPUT_8(isp, src->ct_iid_hi, &dst->ct_iid_hi);
	ISP_IOXPUT_8(isp, src->ct_reserved, &dst->ct_reserved);
	ISP_IOXPUT_32(isp, src->ct_rxid, &dst->ct_rxid);
	ISP_IOXPUT_16(isp, src->ct_senselen, &dst->ct_senselen);
	ISP_IOXPUT_16(isp, src->ct_flags, &dst->ct_flags);
	ISP_IOXPUT_32(isp, src->ct_resid, &dst->ct_resid);
	ISP_IOXPUT_16(isp, src->ct_oxid, &dst->ct_oxid);
	ISP_IOXPUT_16(isp, src->ct_scsi_status, &dst->ct_scsi_status);
	if ((dst->ct_flags & CT7_FLAG_MMASK) == CT7_FLAG_MODE0) {
		ISP_IOXPUT_32(isp, src->rsp.m0.reloff, &dst->rsp.m0.reloff);
		ISP_IOXPUT_32(isp, src->rsp.m0.reserved0,
		    &dst->rsp.m0.reserved0);
		ISP_IOXPUT_32(isp, src->rsp.m0.ct_xfrlen,
		    &dst->rsp.m0.ct_xfrlen);
		ISP_IOXPUT_32(isp, src->rsp.m0.reserved1,
		    &dst->rsp.m0.reserved1);
		ISP_IOXPUT_32(isp, src->rsp.m0.ds.ds_base,
		    &dst->rsp.m0.ds.ds_base);
		ISP_IOXPUT_32(isp, src->rsp.m0.ds.ds_basehi,
		    &dst->rsp.m0.ds.ds_basehi);
		ISP_IOXPUT_32(isp, src->rsp.m0.ds.ds_count,
		    &dst->rsp.m0.ds.ds_count);
	} else if ((dst->ct_flags & CT7_FLAG_MMASK) == CT7_FLAG_MODE1) {
		ISP_IOXPUT_16(isp, src->rsp.m1.ct_resplen,
		    &dst->rsp.m1.ct_resplen);
		ISP_IOXPUT_16(isp, src->rsp.m1.reserved, &dst->rsp.m1.reserved);
		for (i = 0; i < MAXRESPLEN_24XX; i++) {
			ISP_IOXPUT_8(isp, src->rsp.m1.ct_resp[i],
			    &dst->rsp.m1.ct_resp[i]);
		}
	} else {
		ISP_IOXPUT_32(isp, src->rsp.m2.reserved0,
		    &dst->rsp.m2.reserved0);
		ISP_IOXPUT_32(isp, src->rsp.m2.ct_datalen,
		    &dst->rsp.m2.ct_datalen);
		ISP_IOXPUT_32(isp, src->rsp.m2.reserved1,
		    &dst->rsp.m2.reserved1);
		ISP_IOXPUT_32(isp, src->rsp.m2.ct_fcp_rsp_iudata.ds_base,
		    &dst->rsp.m2.ct_fcp_rsp_iudata.ds_base);
		ISP_IOXPUT_32(isp, src->rsp.m2.ct_fcp_rsp_iudata.ds_basehi,
		    &dst->rsp.m2.ct_fcp_rsp_iudata.ds_basehi);
		ISP_IOXPUT_32(isp, src->rsp.m2.ct_fcp_rsp_iudata.ds_count,
		    &dst->rsp.m2.ct_fcp_rsp_iudata.ds_count);
	}
}


void
isp_get_ctio2(ispsoftc_t *isp, ct2_entry_t *src, ct2_entry_t *dst)
{
	int i;

	isp_get_hdr(isp, &src->ct_header, &dst->ct_header);
	ISP_IOXGET_32(isp, &src->ct_syshandle, dst->ct_syshandle);
	ISP_IOXGET_8(isp, &src->ct_lun, dst->ct_lun);
	ISP_IOXGET_8(isp, &src->ct_iid, dst->ct_iid);
	ISP_IOXGET_16(isp, &src->ct_rxid, dst->ct_rxid);
	ISP_IOXGET_16(isp, &src->ct_flags, dst->ct_flags);
	ISP_IOXGET_16(isp, &src->ct_status, dst->ct_status);
	ISP_IOXGET_16(isp, &src->ct_timeout, dst->ct_timeout);
	ISP_IOXGET_16(isp, &src->ct_seg_count, dst->ct_seg_count);
	ISP_IOXGET_32(isp, &src->ct_reloff, dst->ct_reloff);
	ISP_IOXGET_32(isp, &src->ct_resid, dst->ct_resid);
	if ((dst->ct_flags & CT2_FLAG_MMASK) == CT2_FLAG_MODE0) {
		ISP_IOXGET_32(isp, &src->rsp.m0._reserved,
		    dst->rsp.m0._reserved);
		ISP_IOXGET_16(isp, &src->rsp.m0._reserved2,
		    dst->rsp.m0._reserved2);
		ISP_IOXGET_16(isp, &src->rsp.m0.ct_scsi_status,
		    dst->rsp.m0.ct_scsi_status);
		ISP_IOXGET_32(isp, &src->rsp.m0.ct_xfrlen,
		    dst->rsp.m0.ct_xfrlen);
		if (dst->ct_header.rqs_entry_type == RQSTYPE_CTIO2) {
			for (i = 0; i < ISP_RQDSEG_T2; i++) {
				ISP_IOXGET_32(isp,
				    &src->rsp.m0.u.ct_dataseg[i].ds_base,
				    dst->rsp.m0.u.ct_dataseg[i].ds_base);
				ISP_IOXGET_32(isp,
				    &src->rsp.m0.u.ct_dataseg[i].ds_count,
				    dst->rsp.m0.u.ct_dataseg[i].ds_count);
			}
		} else if (dst->ct_header.rqs_entry_type == RQSTYPE_CTIO3) {
			for (i = 0; i < ISP_RQDSEG_T3; i++) {
				ISP_IOXGET_32(isp,
				    &src->rsp.m0.u.ct_dataseg64[i].ds_base,
				    dst->rsp.m0.u.ct_dataseg64[i].ds_base);
				ISP_IOXGET_32(isp,
				    &src->rsp.m0.u.ct_dataseg64[i].ds_basehi,
				    dst->rsp.m0.u.ct_dataseg64[i].ds_basehi);
				ISP_IOXGET_32(isp,
				    &src->rsp.m0.u.ct_dataseg64[i].ds_count,
				    dst->rsp.m0.u.ct_dataseg64[i].ds_count);
			}
		} else if (dst->ct_header.rqs_entry_type == RQSTYPE_CTIO4) {
			ISP_IOXGET_16(isp, &src->rsp.m0.u.ct_dslist.ds_type,
			    dst->rsp.m0.u.ct_dslist.ds_type);
			ISP_IOXGET_32(isp, &src->rsp.m0.u.ct_dslist.ds_segment,
			    dst->rsp.m0.u.ct_dslist.ds_segment);
			ISP_IOXGET_32(isp, &src->rsp.m0.u.ct_dslist.ds_base,
			    dst->rsp.m0.u.ct_dslist.ds_base);
		}
	} else if ((dst->ct_flags & CT2_FLAG_MMASK) == CT2_FLAG_MODE1) {
		ISP_IOXGET_16(isp, &src->rsp.m1._reserved,
		    dst->rsp.m1._reserved);
		ISP_IOXGET_16(isp, &src->rsp.m1._reserved2,
		    dst->rsp.m1._reserved2);
		ISP_IOXGET_16(isp, &src->rsp.m1.ct_senselen,
		    dst->rsp.m1.ct_senselen);
		ISP_IOXGET_16(isp, &src->rsp.m1.ct_scsi_status,
		    dst->rsp.m1.ct_scsi_status);
		ISP_IOXGET_16(isp, &src->rsp.m1.ct_resplen,
		    dst->rsp.m1.ct_resplen);
		for (i = 0; i < MAXRESPLEN; i++) {
			ISP_IOXGET_8(isp, &src->rsp.m1.ct_resp[i],
			    dst->rsp.m1.ct_resp[i]);
		}
	} else {
		ISP_IOXGET_32(isp, &src->rsp.m2._reserved,
		    dst->rsp.m2._reserved);
		ISP_IOXGET_16(isp, &src->rsp.m2._reserved2,
		    dst->rsp.m2._reserved2);
		ISP_IOXGET_16(isp, &src->rsp.m2._reserved3,
		    dst->rsp.m2._reserved3);
		ISP_IOXGET_32(isp, &src->rsp.m2.ct_datalen,
		    dst->rsp.m2.ct_datalen);
		ISP_IOXGET_32(isp, &src->rsp.m2.ct_fcp_rsp_iudata.ds_base,
		    dst->rsp.m2.ct_fcp_rsp_iudata.ds_base);
		ISP_IOXGET_32(isp, &src->rsp.m2.ct_fcp_rsp_iudata.ds_count,
		    dst->rsp.m2.ct_fcp_rsp_iudata.ds_count);
	}
}

void
isp_get_ctio2e(ispsoftc_t *isp, ct2e_entry_t *src, ct2e_entry_t *dst)
{
	int i;

	isp_get_hdr(isp, &src->ct_header, &dst->ct_header);
	ISP_IOXGET_32(isp, &src->ct_syshandle, dst->ct_syshandle);
	ISP_IOXGET_16(isp, &src->ct_iid, dst->ct_iid);
	ISP_IOXGET_16(isp, &src->ct_rxid, dst->ct_rxid);
	ISP_IOXGET_16(isp, &src->ct_flags, dst->ct_flags);
	ISP_IOXGET_16(isp, &src->ct_status, dst->ct_status);
	ISP_IOXGET_16(isp, &src->ct_timeout, dst->ct_timeout);
	ISP_IOXGET_16(isp, &src->ct_seg_count, dst->ct_seg_count);
	ISP_IOXGET_32(isp, &src->ct_reloff, dst->ct_reloff);
	ISP_IOXGET_32(isp, &src->ct_resid, dst->ct_resid);
	if ((dst->ct_flags & CT2_FLAG_MMASK) == CT2_FLAG_MODE0) {
		ISP_IOXGET_32(isp, &src->rsp.m0._reserved,
		    dst->rsp.m0._reserved);
		ISP_IOXGET_16(isp, &src->rsp.m0._reserved2,
		    dst->rsp.m0._reserved2);
		ISP_IOXGET_16(isp, &src->rsp.m0.ct_scsi_status,
		    dst->rsp.m0.ct_scsi_status);
		ISP_IOXGET_32(isp, &src->rsp.m0.ct_xfrlen,
		    dst->rsp.m0.ct_xfrlen);
		if (src->ct_header.rqs_entry_type == RQSTYPE_CTIO2) {
			for (i = 0; i < ISP_RQDSEG_T2; i++) {
				ISP_IOXGET_32(isp,
				    &src->rsp.m0.u.ct_dataseg[i].ds_base,
				    dst->rsp.m0.u.ct_dataseg[i].ds_base);
				ISP_IOXGET_32(isp,
				    &src->rsp.m0.u.ct_dataseg[i].ds_count,
				    dst->rsp.m0.u.ct_dataseg[i].ds_count);
			}
		} else if (dst->ct_header.rqs_entry_type == RQSTYPE_CTIO3) {
			for (i = 0; i < ISP_RQDSEG_T3; i++) {
				ISP_IOXGET_32(isp,
				    &src->rsp.m0.u.ct_dataseg64[i].ds_base,
				    dst->rsp.m0.u.ct_dataseg64[i].ds_base);
				ISP_IOXGET_32(isp,
				    &src->rsp.m0.u.ct_dataseg64[i].ds_basehi,
				    dst->rsp.m0.u.ct_dataseg64[i].ds_basehi);
				ISP_IOXGET_32(isp,
				    &src->rsp.m0.u.ct_dataseg64[i].ds_count,
				    dst->rsp.m0.u.ct_dataseg64[i].ds_count);
			}
		} else if (dst->ct_header.rqs_entry_type == RQSTYPE_CTIO4) {
			ISP_IOXGET_16(isp, &src->rsp.m0.u.ct_dslist.ds_type,
			    dst->rsp.m0.u.ct_dslist.ds_type);
			ISP_IOXGET_32(isp, &src->rsp.m0.u.ct_dslist.ds_segment,
			    dst->rsp.m0.u.ct_dslist.ds_segment);
			ISP_IOXGET_32(isp, &src->rsp.m0.u.ct_dslist.ds_base,
			    dst->rsp.m0.u.ct_dslist.ds_base);
		}
	} else if ((dst->ct_flags & CT2_FLAG_MMASK) == CT2_FLAG_MODE1) {
		ISP_IOXGET_16(isp, &src->rsp.m1._reserved,
		    dst->rsp.m1._reserved);
		ISP_IOXGET_16(isp, &src->rsp.m1._reserved2,
		    dst->rsp.m1._reserved2);
		ISP_IOXGET_16(isp, &src->rsp.m1.ct_senselen,
		    dst->rsp.m1.ct_senselen);
		ISP_IOXGET_16(isp, &src->rsp.m1.ct_scsi_status,
		    dst->rsp.m1.ct_scsi_status);
		ISP_IOXGET_16(isp, &src->rsp.m1.ct_resplen,
		    dst->rsp.m1.ct_resplen);
		for (i = 0; i < MAXRESPLEN; i++) {
			ISP_IOXGET_8(isp, &src->rsp.m1.ct_resp[i],
			    dst->rsp.m1.ct_resp[i]);
		}
	} else {
		ISP_IOXGET_32(isp, &src->rsp.m2._reserved,
		    dst->rsp.m2._reserved);
		ISP_IOXGET_16(isp, &src->rsp.m2._reserved2,
		    dst->rsp.m2._reserved2);
		ISP_IOXGET_16(isp, &src->rsp.m2._reserved3,
		    dst->rsp.m2._reserved3);
		ISP_IOXGET_32(isp, &src->rsp.m2.ct_datalen,
		    dst->rsp.m2.ct_datalen);
		ISP_IOXGET_32(isp, &src->rsp.m2.ct_fcp_rsp_iudata.ds_base,
		    dst->rsp.m2.ct_fcp_rsp_iudata.ds_base);
		ISP_IOXGET_32(isp, &src->rsp.m2.ct_fcp_rsp_iudata.ds_count,
		    dst->rsp.m2.ct_fcp_rsp_iudata.ds_count);
	}
}

void
isp_get_ctio7(ispsoftc_t *isp, ct7_entry_t *src, ct7_entry_t *dst)
{
	int i;

	isp_get_hdr(isp, &src->ct_header, &dst->ct_header);
	ISP_IOXGET_32(isp, &src->ct_syshandle, dst->ct_syshandle);
	ISP_IOXGET_16(isp, &src->ct_nphdl, dst->ct_nphdl);
	ISP_IOXGET_16(isp, &src->ct_timeout, dst->ct_timeout);
	ISP_IOXGET_16(isp, &src->ct_seg_count, dst->ct_seg_count);
	ISP_IOXGET_8(isp, &src->ct_vpindex, dst->ct_vpindex);
	ISP_IOXGET_8(isp, &src->ct_xflags, dst->ct_xflags);
	ISP_IOXGET_16(isp, &src->ct_iid_lo, dst->ct_iid_lo);
	ISP_IOXGET_8(isp, &src->ct_iid_hi, dst->ct_iid_hi);
	ISP_IOXGET_8(isp, &src->ct_reserved, dst->ct_reserved);
	ISP_IOXGET_32(isp, &src->ct_rxid, dst->ct_rxid);
	ISP_IOXGET_16(isp, &src->ct_senselen, dst->ct_senselen);
	ISP_IOXGET_16(isp, &src->ct_flags, dst->ct_flags);
	ISP_IOXGET_32(isp, &src->ct_resid, dst->ct_resid);
	ISP_IOXGET_16(isp, &src->ct_oxid, dst->ct_oxid);
	ISP_IOXGET_16(isp, &src->ct_scsi_status, dst->ct_scsi_status);
	if ((dst->ct_flags & CT7_FLAG_MMASK) == CT7_FLAG_MODE0) {
		ISP_IOXGET_32(isp, &src->rsp.m0.reloff, dst->rsp.m0.reloff);
		ISP_IOXGET_32(isp, &src->rsp.m0.reserved0,
		    dst->rsp.m0.reserved0);
		ISP_IOXGET_32(isp, &src->rsp.m0.ct_xfrlen,
		    dst->rsp.m0.ct_xfrlen);
		ISP_IOXGET_32(isp, &src->rsp.m0.reserved1,
		    dst->rsp.m0.reserved1);
		ISP_IOXGET_32(isp, &src->rsp.m0.ds.ds_base,
		    dst->rsp.m0.ds.ds_base);
		ISP_IOXGET_32(isp, &src->rsp.m0.ds.ds_basehi,
		    dst->rsp.m0.ds.ds_basehi);
		ISP_IOXGET_32(isp, &src->rsp.m0.ds.ds_count,
		    dst->rsp.m0.ds.ds_count);
	} else if ((dst->ct_flags & CT7_FLAG_MMASK) == CT7_FLAG_MODE1) {
		ISP_IOXGET_16(isp, &src->rsp.m1.ct_resplen,
		    dst->rsp.m1.ct_resplen);
		ISP_IOXGET_16(isp, &src->rsp.m1.reserved, dst->rsp.m1.reserved);
		for (i = 0; i < MAXRESPLEN_24XX; i++) {
			ISP_IOXGET_8(isp, &src->rsp.m1.ct_resp[i],
			    dst->rsp.m1.ct_resp[i]);
		}
	} else {
		ISP_IOXGET_32(isp, &src->rsp.m2.reserved0,
		    dst->rsp.m2.reserved0);
		ISP_IOXGET_32(isp, &src->rsp.m2.ct_datalen,
		    dst->rsp.m2.ct_datalen);
		ISP_IOXGET_32(isp, &src->rsp.m2.reserved1,
		    dst->rsp.m2.reserved1);
		ISP_IOXGET_32(isp, &src->rsp.m2.ct_fcp_rsp_iudata.ds_base,
		    dst->rsp.m2.ct_fcp_rsp_iudata.ds_base);
		ISP_IOXGET_32(isp, &src->rsp.m2.ct_fcp_rsp_iudata.ds_basehi,
		    dst->rsp.m2.ct_fcp_rsp_iudata.ds_basehi);
		ISP_IOXGET_32(isp, &src->rsp.m2.ct_fcp_rsp_iudata.ds_count,
		    dst->rsp.m2.ct_fcp_rsp_iudata.ds_count);
	}
}

void
isp_put_enable_lun(ispsoftc_t *isp, lun_entry_t *lesrc, lun_entry_t *ledst)
{
	int i;
	isp_put_hdr(isp, &lesrc->le_header, &ledst->le_header);
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

void
isp_get_enable_lun(ispsoftc_t *isp, lun_entry_t *lesrc, lun_entry_t *ledst)
{
	int i;
	isp_get_hdr(isp, &lesrc->le_header, &ledst->le_header);
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

void
isp_put_notify(ispsoftc_t *isp, in_entry_t *src, in_entry_t *dst)
{
	int i;
	isp_put_hdr(isp, &src->in_header, &dst->in_header);
	ISP_IOXPUT_32(isp, src->in_reserved, &dst->in_reserved);
	if (ISP_IS_SBUS(isp)) {
		ISP_IOXPUT_8(isp, src->in_lun, &dst->in_iid);
		ISP_IOXPUT_8(isp, src->in_iid, &dst->in_lun);
		ISP_IOXPUT_8(isp, src->in_reserved2, &dst->in_tgt);
		ISP_IOXPUT_8(isp, src->in_tgt, &dst->in_reserved2);
		ISP_IOXPUT_8(isp, src->in_status, &dst->in_rsvd2);
		ISP_IOXPUT_8(isp, src->in_rsvd2, &dst->in_status);
		ISP_IOXPUT_8(isp, src->in_tag_val, &dst->in_tag_type);
		ISP_IOXPUT_8(isp, src->in_tag_type, &dst->in_tag_val);
	} else {
		ISP_IOXPUT_8(isp, src->in_lun, &dst->in_lun);
		ISP_IOXPUT_8(isp, src->in_iid, &dst->in_iid);
		ISP_IOXPUT_8(isp, src->in_reserved2, &dst->in_reserved2);
		ISP_IOXPUT_8(isp, src->in_tgt, &dst->in_tgt);
		ISP_IOXPUT_8(isp, src->in_status, &dst->in_status);
		ISP_IOXPUT_8(isp, src->in_rsvd2, &dst->in_rsvd2);
		ISP_IOXPUT_8(isp, src->in_tag_val, &dst->in_tag_val);
		ISP_IOXPUT_8(isp, src->in_tag_type, &dst->in_tag_type);
	}
	ISP_IOXPUT_32(isp, src->in_flags, &dst->in_flags);
	ISP_IOXPUT_16(isp, src->in_seqid, &dst->in_seqid);
	for (i = 0; i < IN_MSGLEN; i++) {
		ISP_IOXPUT_8(isp, src->in_msg[i], &dst->in_msg[i]);
	}
	for (i = 0; i < IN_RSVDLEN; i++) {
		ISP_IOXPUT_8(isp, src->in_reserved3[i],
		    &dst->in_reserved3[i]);
	}
	for (i = 0; i < QLTM_SENSELEN; i++) {
		ISP_IOXPUT_8(isp, src->in_sense[i],
		    &dst->in_sense[i]);
	}
}

void
isp_get_notify(ispsoftc_t *isp, in_entry_t *src, in_entry_t *dst)
{
	int i;
	isp_get_hdr(isp, &src->in_header, &dst->in_header);
	ISP_IOXGET_32(isp, &src->in_reserved, dst->in_reserved);
	if (ISP_IS_SBUS(isp)) {
		ISP_IOXGET_8(isp, &src->in_lun, dst->in_iid);
		ISP_IOXGET_8(isp, &src->in_iid, dst->in_lun);
		ISP_IOXGET_8(isp, &src->in_reserved2, dst->in_tgt);
		ISP_IOXGET_8(isp, &src->in_tgt, dst->in_reserved2);
		ISP_IOXGET_8(isp, &src->in_status, dst->in_rsvd2);
		ISP_IOXGET_8(isp, &src->in_rsvd2, dst->in_status);
		ISP_IOXGET_8(isp, &src->in_tag_val, dst->in_tag_type);
		ISP_IOXGET_8(isp, &src->in_tag_type, dst->in_tag_val);
	} else {
		ISP_IOXGET_8(isp, &src->in_lun, dst->in_lun);
		ISP_IOXGET_8(isp, &src->in_iid, dst->in_iid);
		ISP_IOXGET_8(isp, &src->in_reserved2, dst->in_reserved2);
		ISP_IOXGET_8(isp, &src->in_tgt, dst->in_tgt);
		ISP_IOXGET_8(isp, &src->in_status, dst->in_status);
		ISP_IOXGET_8(isp, &src->in_rsvd2, dst->in_rsvd2);
		ISP_IOXGET_8(isp, &src->in_tag_val, dst->in_tag_val);
		ISP_IOXGET_8(isp, &src->in_tag_type, dst->in_tag_type);
	}
	ISP_IOXGET_32(isp, &src->in_flags, dst->in_flags);
	ISP_IOXGET_16(isp, &src->in_seqid, dst->in_seqid);
	for (i = 0; i < IN_MSGLEN; i++) {
		ISP_IOXGET_8(isp, &src->in_msg[i], dst->in_msg[i]);
	}
	for (i = 0; i < IN_RSVDLEN; i++) {
		ISP_IOXGET_8(isp, &src->in_reserved3[i],
		    dst->in_reserved3[i]);
	}
	for (i = 0; i < QLTM_SENSELEN; i++) {
		ISP_IOXGET_8(isp, &src->in_sense[i],
		    dst->in_sense[i]);
	}
}

void
isp_put_notify_fc(ispsoftc_t *isp, in_fcentry_t *src,
    in_fcentry_t *dst)
{
	isp_put_hdr(isp, &src->in_header, &dst->in_header);
	ISP_IOXPUT_32(isp, src->in_reserved, &dst->in_reserved);
	ISP_IOXPUT_8(isp, src->in_lun, &dst->in_lun);
	ISP_IOXPUT_8(isp, src->in_iid, &dst->in_iid);
	ISP_IOXPUT_16(isp, src->in_scclun, &dst->in_scclun);
	ISP_IOXPUT_32(isp, src->in_reserved2, &dst->in_reserved2);
	ISP_IOXPUT_16(isp, src->in_status, &dst->in_status);
	ISP_IOXPUT_16(isp, src->in_task_flags, &dst->in_task_flags);
	ISP_IOXPUT_16(isp, src->in_seqid, &dst->in_seqid);
}

void
isp_put_notify_fc_e(ispsoftc_t *isp, in_fcentry_e_t *src,
    in_fcentry_e_t *dst)
{
	isp_put_hdr(isp, &src->in_header, &dst->in_header);
	ISP_IOXPUT_32(isp, src->in_reserved, &dst->in_reserved);
	ISP_IOXPUT_16(isp, src->in_iid, &dst->in_iid);
	ISP_IOXPUT_16(isp, src->in_scclun, &dst->in_scclun);
	ISP_IOXPUT_32(isp, src->in_reserved2, &dst->in_reserved2);
	ISP_IOXPUT_16(isp, src->in_status, &dst->in_status);
	ISP_IOXPUT_16(isp, src->in_task_flags, &dst->in_task_flags);
	ISP_IOXPUT_16(isp, src->in_seqid, &dst->in_seqid);
}

void
isp_put_notify_24xx(ispsoftc_t *isp, in_fcentry_24xx_t *src,
    in_fcentry_24xx_t *dst)
{
	int i;

	isp_put_hdr(isp, &src->in_header, &dst->in_header);
	ISP_IOXPUT_32(isp, src->in_reserved, &dst->in_reserved);
	ISP_IOXPUT_16(isp, src->in_nphdl, &dst->in_nphdl);
	ISP_IOXPUT_16(isp, src->in_reserved1, &dst->in_reserved1);
	ISP_IOXPUT_16(isp, src->in_flags, &dst->in_flags);
	ISP_IOXPUT_16(isp, src->in_srr_rxid, &dst->in_srr_rxid);
	ISP_IOXPUT_16(isp, src->in_status, &dst->in_status);
	ISP_IOXPUT_8(isp, src->in_status_subcode, &dst->in_status_subcode);
	ISP_IOXPUT_16(isp, src->in_reserved2, &dst->in_reserved2);
	ISP_IOXPUT_32(isp, src->in_rxid, &dst->in_rxid);
	ISP_IOXPUT_16(isp, src->in_srr_reloff_hi, &dst->in_srr_reloff_hi);
	ISP_IOXPUT_16(isp, src->in_srr_reloff_lo, &dst->in_srr_reloff_lo);
	ISP_IOXPUT_16(isp, src->in_srr_iu, &dst->in_srr_iu);
	ISP_IOXPUT_16(isp, src->in_srr_oxid, &dst->in_srr_oxid);
	for (i = 0; i < 18; i++) {
		ISP_IOXPUT_8(isp, src->in_reserved3[i], &dst->in_reserved3[i]);
	}
	ISP_IOXPUT_8(isp, src->in_reserved4, &dst->in_reserved4);
	ISP_IOXPUT_8(isp, src->in_vpindex, &dst->in_vpindex);
	ISP_IOXPUT_32(isp, src->in_reserved5, &dst->in_reserved5);
	ISP_IOXPUT_16(isp, src->in_portid_lo, &dst->in_portid_lo);
	ISP_IOXPUT_8(isp, src->in_portid_hi, &dst->in_portid_hi);
	ISP_IOXPUT_8(isp, src->in_reserved6, &dst->in_reserved6);
	ISP_IOXPUT_16(isp, src->in_reserved7, &dst->in_reserved7);
	ISP_IOXPUT_16(isp, src->in_oxid, &dst->in_oxid);
}

void
isp_get_notify_fc(ispsoftc_t *isp, in_fcentry_t *src,
    in_fcentry_t *dst)
{
	isp_get_hdr(isp, &src->in_header, &dst->in_header);
	ISP_IOXGET_32(isp, &src->in_reserved, dst->in_reserved);
	ISP_IOXGET_8(isp, &src->in_lun, dst->in_lun);
	ISP_IOXGET_8(isp, &src->in_iid, dst->in_iid);
	ISP_IOXGET_16(isp, &src->in_scclun, dst->in_scclun);
	ISP_IOXGET_32(isp, &src->in_reserved2, dst->in_reserved2);
	ISP_IOXGET_16(isp, &src->in_status, dst->in_status);
	ISP_IOXGET_16(isp, &src->in_task_flags, dst->in_task_flags);
	ISP_IOXGET_16(isp, &src->in_seqid, dst->in_seqid);
}

void
isp_get_notify_fc_e(ispsoftc_t *isp, in_fcentry_e_t *src,
    in_fcentry_e_t *dst)
{
	isp_get_hdr(isp, &src->in_header, &dst->in_header);
	ISP_IOXGET_32(isp, &src->in_reserved, dst->in_reserved);
	ISP_IOXGET_16(isp, &src->in_iid, dst->in_iid);
	ISP_IOXGET_16(isp, &src->in_scclun, dst->in_scclun);
	ISP_IOXGET_32(isp, &src->in_reserved2, dst->in_reserved2);
	ISP_IOXGET_16(isp, &src->in_status, dst->in_status);
	ISP_IOXGET_16(isp, &src->in_task_flags, dst->in_task_flags);
	ISP_IOXGET_16(isp, &src->in_seqid, dst->in_seqid);
}

void
isp_get_notify_24xx(ispsoftc_t *isp, in_fcentry_24xx_t *src,
    in_fcentry_24xx_t *dst)
{
	int i;

	isp_get_hdr(isp, &src->in_header, &dst->in_header);
	ISP_IOXGET_32(isp, &src->in_reserved, dst->in_reserved);
	ISP_IOXGET_16(isp, &src->in_nphdl, dst->in_nphdl);
	ISP_IOXGET_16(isp, &src->in_reserved1, dst->in_reserved1);
	ISP_IOXGET_16(isp, &src->in_flags, dst->in_flags);
	ISP_IOXGET_16(isp, &src->in_srr_rxid, dst->in_srr_rxid);
	ISP_IOXGET_16(isp, &src->in_status, dst->in_status);
	ISP_IOXGET_8(isp, &src->in_status_subcode, dst->in_status_subcode);
	ISP_IOXGET_16(isp, &src->in_reserved2, dst->in_reserved2);
	ISP_IOXGET_32(isp, &src->in_rxid, dst->in_rxid);
	ISP_IOXGET_16(isp, &src->in_srr_reloff_hi, dst->in_srr_reloff_hi);
	ISP_IOXGET_16(isp, &src->in_srr_reloff_lo, dst->in_srr_reloff_lo);
	ISP_IOXGET_16(isp, &src->in_srr_iu, dst->in_srr_iu);
	ISP_IOXGET_16(isp, &src->in_srr_oxid, dst->in_srr_oxid);
	for (i = 0; i < 18; i++) {
		ISP_IOXGET_8(isp, &src->in_reserved3[i], dst->in_reserved3[i]);
	}
	ISP_IOXGET_8(isp, &src->in_reserved4, dst->in_reserved4);
	ISP_IOXGET_8(isp, &src->in_vpindex, dst->in_vpindex);
	ISP_IOXGET_32(isp, &src->in_reserved5, dst->in_reserved5);
	ISP_IOXGET_16(isp, &src->in_portid_lo, dst->in_portid_lo);
	ISP_IOXGET_8(isp, &src->in_portid_hi, dst->in_portid_hi);
	ISP_IOXGET_8(isp, &src->in_reserved6, dst->in_reserved6);
	ISP_IOXGET_16(isp, &src->in_reserved7, dst->in_reserved7);
	ISP_IOXGET_16(isp, &src->in_oxid, dst->in_oxid);
}

void
isp_put_notify_ack(ispsoftc_t *isp, na_entry_t *src,  na_entry_t *dst)
{
	int i;
	isp_put_hdr(isp, &src->na_header, &dst->na_header);
	ISP_IOXPUT_32(isp, src->na_reserved, &dst->na_reserved);
	if (ISP_IS_SBUS(isp)) {
		ISP_IOXPUT_8(isp, src->na_lun, &dst->na_iid);
		ISP_IOXPUT_8(isp, src->na_iid, &dst->na_lun);
		ISP_IOXPUT_8(isp, src->na_status, &dst->na_event);
		ISP_IOXPUT_8(isp, src->na_event, &dst->na_status);
	} else {
		ISP_IOXPUT_8(isp, src->na_lun, &dst->na_lun);
		ISP_IOXPUT_8(isp, src->na_iid, &dst->na_iid);
		ISP_IOXPUT_8(isp, src->na_status, &dst->na_status);
		ISP_IOXPUT_8(isp, src->na_event, &dst->na_event);
	}
	ISP_IOXPUT_32(isp, src->na_flags, &dst->na_flags);
	for (i = 0; i < NA_RSVDLEN; i++) {
		ISP_IOXPUT_16(isp, src->na_reserved3[i],
		    &dst->na_reserved3[i]);
	}
}

void
isp_get_notify_ack(ispsoftc_t *isp, na_entry_t *src, na_entry_t *dst)
{
	int i;
	isp_get_hdr(isp, &src->na_header, &dst->na_header);
	ISP_IOXGET_32(isp, &src->na_reserved, dst->na_reserved);
	if (ISP_IS_SBUS(isp)) {
		ISP_IOXGET_8(isp, &src->na_lun, dst->na_iid);
		ISP_IOXGET_8(isp, &src->na_iid, dst->na_lun);
		ISP_IOXGET_8(isp, &src->na_status, dst->na_event);
		ISP_IOXGET_8(isp, &src->na_event, dst->na_status);
	} else {
		ISP_IOXGET_8(isp, &src->na_lun, dst->na_lun);
		ISP_IOXGET_8(isp, &src->na_iid, dst->na_iid);
		ISP_IOXGET_8(isp, &src->na_status, dst->na_status);
		ISP_IOXGET_8(isp, &src->na_event, dst->na_event);
	}
	ISP_IOXGET_32(isp, &src->na_flags, dst->na_flags);
	for (i = 0; i < NA_RSVDLEN; i++) {
		ISP_IOXGET_16(isp, &src->na_reserved3[i],
		    dst->na_reserved3[i]);
	}
}

void
isp_put_notify_ack_fc(ispsoftc_t *isp, na_fcentry_t *src,
    na_fcentry_t *dst)
{
	int i;
	isp_put_hdr(isp, &src->na_header, &dst->na_header);
	ISP_IOXPUT_32(isp, src->na_reserved, &dst->na_reserved);
	ISP_IOXPUT_8(isp, src->na_reserved1, &dst->na_reserved1);
	ISP_IOXPUT_8(isp, src->na_iid, &dst->na_iid);
	ISP_IOXPUT_16(isp, src->na_response, &dst->na_response);
	ISP_IOXPUT_16(isp, src->na_flags, &dst->na_flags);
	ISP_IOXPUT_16(isp, src->na_reserved2, &dst->na_reserved2);
	ISP_IOXPUT_16(isp, src->na_status, &dst->na_status);
	ISP_IOXPUT_16(isp, src->na_task_flags, &dst->na_task_flags);
	ISP_IOXPUT_16(isp, src->na_seqid, &dst->na_seqid);
	for (i = 0; i < NA2_RSVDLEN; i++) {
		ISP_IOXPUT_16(isp, src->na_reserved3[i],
		    &dst->na_reserved3[i]);
	}
}

void
isp_put_notify_ack_fc_e(ispsoftc_t *isp, na_fcentry_e_t *src,
    na_fcentry_e_t *dst)
{
	int i;
	isp_put_hdr(isp, &src->na_header, &dst->na_header);
	ISP_IOXPUT_32(isp, src->na_reserved, &dst->na_reserved);
	ISP_IOXPUT_16(isp, src->na_iid, &dst->na_iid);
	ISP_IOXPUT_16(isp, src->na_response, &dst->na_response);
	ISP_IOXPUT_16(isp, src->na_flags, &dst->na_flags);
	ISP_IOXPUT_16(isp, src->na_reserved2, &dst->na_reserved2);
	ISP_IOXPUT_16(isp, src->na_status, &dst->na_status);
	ISP_IOXPUT_16(isp, src->na_task_flags, &dst->na_task_flags);
	ISP_IOXPUT_16(isp, src->na_seqid, &dst->na_seqid);
	for (i = 0; i < NA2_RSVDLEN; i++) {
		ISP_IOXPUT_16(isp, src->na_reserved3[i],
		    &dst->na_reserved3[i]);
	}
}

void
isp_put_notify_24xx_ack(ispsoftc_t *isp, na_fcentry_24xx_t *src,
    na_fcentry_24xx_t *dst)
{
	int i;

	isp_put_hdr(isp, &src->na_header, &dst->na_header);
	ISP_IOXPUT_32(isp, src->na_handle, &dst->na_handle);
	ISP_IOXPUT_16(isp, src->na_nphdl, &dst->na_nphdl);
	ISP_IOXPUT_16(isp, src->na_reserved1, &dst->na_reserved1);
	ISP_IOXPUT_16(isp, src->na_flags, &dst->na_flags);
	ISP_IOXPUT_16(isp, src->na_srr_rxid, &dst->na_srr_rxid);
	ISP_IOXPUT_16(isp, src->na_status, &dst->na_status);
	ISP_IOXPUT_8(isp, src->na_status_subcode, &dst->na_status_subcode);
	ISP_IOXPUT_16(isp, src->na_reserved2, &dst->na_reserved2);
	ISP_IOXPUT_32(isp, src->na_rxid, &dst->na_rxid);
	ISP_IOXPUT_16(isp, src->na_srr_reloff_hi, &dst->na_srr_reloff_hi);
	ISP_IOXPUT_16(isp, src->na_srr_reloff_lo, &dst->na_srr_reloff_lo);
	ISP_IOXPUT_16(isp, src->na_srr_iu, &dst->na_srr_iu);
	ISP_IOXPUT_16(isp, src->na_srr_flags, &dst->na_srr_flags);
	for (i = 0; i < 18; i++) {
		ISP_IOXPUT_8(isp, src->na_reserved3[i], &dst->na_reserved3[i]);
	}
	ISP_IOXPUT_8(isp, src->na_reserved4, &dst->na_reserved4);
	ISP_IOXPUT_8(isp, src->na_vpindex, &dst->na_vpindex);
	ISP_IOXPUT_8(isp, src->na_srr_reject_vunique,
	    &dst->na_srr_reject_vunique);
	ISP_IOXPUT_8(isp, src->na_srr_reject_explanation,
	    &dst->na_srr_reject_explanation);
	ISP_IOXPUT_8(isp, src->na_srr_reject_code, &dst->na_srr_reject_code);
	ISP_IOXPUT_8(isp, src->na_reserved5, &dst->na_reserved5);
	for (i = 0; i < 6; i++) {
		ISP_IOXPUT_8(isp, src->na_reserved6[i], &dst->na_reserved6[i]);
	}
	ISP_IOXPUT_16(isp, src->na_oxid, &dst->na_oxid);
}

void
isp_get_notify_ack_fc(ispsoftc_t *isp, na_fcentry_t *src,
    na_fcentry_t *dst)
{
	int i;
	isp_get_hdr(isp, &src->na_header, &dst->na_header);
	ISP_IOXGET_32(isp, &src->na_reserved, dst->na_reserved);
	ISP_IOXGET_8(isp, &src->na_reserved1, dst->na_reserved1);
	ISP_IOXGET_8(isp, &src->na_iid, dst->na_iid);
	ISP_IOXGET_16(isp, &src->na_response, dst->na_response);
	ISP_IOXGET_16(isp, &src->na_flags, dst->na_flags);
	ISP_IOXGET_16(isp, &src->na_reserved2, dst->na_reserved2);
	ISP_IOXGET_16(isp, &src->na_status, dst->na_status);
	ISP_IOXGET_16(isp, &src->na_task_flags, dst->na_task_flags);
	ISP_IOXGET_16(isp, &src->na_seqid, dst->na_seqid);
	for (i = 0; i < NA2_RSVDLEN; i++) {
		ISP_IOXGET_16(isp, &src->na_reserved3[i],
		    dst->na_reserved3[i]);
	}
}

void
isp_get_notify_ack_fc_e(ispsoftc_t *isp, na_fcentry_e_t *src,
    na_fcentry_e_t *dst)
{
	int i;
	isp_get_hdr(isp, &src->na_header, &dst->na_header);
	ISP_IOXGET_32(isp, &src->na_reserved, dst->na_reserved);
	ISP_IOXGET_16(isp, &src->na_iid, dst->na_iid);
	ISP_IOXGET_16(isp, &src->na_response, dst->na_response);
	ISP_IOXGET_16(isp, &src->na_flags, dst->na_flags);
	ISP_IOXGET_16(isp, &src->na_reserved2, dst->na_reserved2);
	ISP_IOXGET_16(isp, &src->na_status, dst->na_status);
	ISP_IOXGET_16(isp, &src->na_task_flags, dst->na_task_flags);
	ISP_IOXGET_16(isp, &src->na_seqid, dst->na_seqid);
	for (i = 0; i < NA2_RSVDLEN; i++) {
		ISP_IOXGET_16(isp, &src->na_reserved3[i],
		    dst->na_reserved3[i]);
	}
}

void
isp_get_notify_ack_24xx(ispsoftc_t *isp, na_fcentry_24xx_t *src,
    na_fcentry_24xx_t *dst)
{
	int i;

	isp_get_hdr(isp, &src->na_header, &dst->na_header);
	ISP_IOXGET_32(isp, &src->na_handle, dst->na_handle);
	ISP_IOXGET_16(isp, &src->na_nphdl, dst->na_nphdl);
	ISP_IOXGET_16(isp, &src->na_reserved1, dst->na_reserved1);
	ISP_IOXGET_16(isp, &src->na_flags, dst->na_flags);
	ISP_IOXGET_16(isp, &src->na_srr_rxid, dst->na_srr_rxid);
	ISP_IOXGET_16(isp, &src->na_status, dst->na_status);
	ISP_IOXGET_8(isp, &src->na_status_subcode, dst->na_status_subcode);
	ISP_IOXGET_16(isp, &src->na_reserved2, dst->na_reserved2);
	ISP_IOXGET_32(isp, &src->na_rxid, dst->na_rxid);
	ISP_IOXGET_16(isp, &src->na_srr_reloff_hi, dst->na_srr_reloff_hi);
	ISP_IOXGET_16(isp, &src->na_srr_reloff_lo, dst->na_srr_reloff_lo);
	ISP_IOXGET_16(isp, &src->na_srr_iu, dst->na_srr_iu);
	ISP_IOXGET_16(isp, &src->na_srr_flags, dst->na_srr_flags);
	for (i = 0; i < 18; i++) {
		ISP_IOXGET_8(isp, &src->na_reserved3[i], dst->na_reserved3[i]);
	}
	ISP_IOXGET_8(isp, &src->na_reserved4, dst->na_reserved4);
	ISP_IOXGET_8(isp, &src->na_vpindex, dst->na_vpindex);
	ISP_IOXGET_8(isp, &src->na_srr_reject_vunique,
	    dst->na_srr_reject_vunique);
	ISP_IOXGET_8(isp, &src->na_srr_reject_explanation,
	    dst->na_srr_reject_explanation);
	ISP_IOXGET_8(isp, &src->na_srr_reject_code, dst->na_srr_reject_code);
	ISP_IOXGET_8(isp, &src->na_reserved5, dst->na_reserved5);
	for (i = 0; i < 6; i++) {
		ISP_IOXGET_8(isp, &src->na_reserved6[i], dst->na_reserved6[i]);
	}
	ISP_IOXGET_16(isp, &src->na_oxid, dst->na_oxid);
}

void
isp_get_abts(ispsoftc_t *isp, abts_t *src, abts_t *dst)
{
	int i;

	isp_get_hdr(isp, &src->abts_header, &dst->abts_header);
	for (i = 0; i < 6; i++) {
		ISP_IOXGET_8(isp, &src->abts_reserved0[i],
		    dst->abts_reserved0[i]);
	}
	ISP_IOXGET_16(isp, &src->abts_nphdl, dst->abts_nphdl);
	ISP_IOXGET_16(isp, &src->abts_reserved1, dst->abts_reserved1);
	ISP_IOXGET_16(isp, &src->abts_sof, dst->abts_sof);
	ISP_IOXGET_32(isp, &src->abts_rxid_abts, dst->abts_rxid_abts);
	ISP_IOXGET_16(isp, &src->abts_did_lo, dst->abts_did_lo);
	ISP_IOXGET_8(isp, &src->abts_did_hi, dst->abts_did_hi);
	ISP_IOXGET_8(isp, &src->abts_r_ctl, dst->abts_r_ctl);
	ISP_IOXGET_16(isp, &src->abts_sid_lo, dst->abts_sid_lo);
	ISP_IOXGET_8(isp, &src->abts_sid_hi, dst->abts_sid_hi);
	ISP_IOXGET_8(isp, &src->abts_cs_ctl, dst->abts_cs_ctl);
	ISP_IOXGET_16(isp, &src->abts_fs_ctl, dst->abts_fs_ctl);
	ISP_IOXGET_8(isp, &src->abts_f_ctl, dst->abts_f_ctl);
	ISP_IOXGET_8(isp, &src->abts_type, dst->abts_type);
	ISP_IOXGET_16(isp, &src->abts_seq_cnt, dst->abts_seq_cnt);
	ISP_IOXGET_8(isp, &src->abts_df_ctl, dst->abts_df_ctl);
	ISP_IOXGET_8(isp, &src->abts_seq_id, dst->abts_seq_id);
	ISP_IOXGET_16(isp, &src->abts_rx_id, dst->abts_rx_id);
	ISP_IOXGET_16(isp, &src->abts_ox_id, dst->abts_ox_id);
	ISP_IOXGET_32(isp, &src->abts_param, dst->abts_param);
	for (i = 0; i < 16; i++) {
	ISP_IOXGET_8(isp, &src->abts_reserved2[i],
		    dst->abts_reserved2[i]);
	}
	ISP_IOXGET_32(isp, &src->abts_rxid_task, dst->abts_rxid_task);
}

void
isp_put_abts_rsp(ispsoftc_t *isp, abts_rsp_t *src, abts_rsp_t *dst)
{
	int i;

	isp_put_hdr(isp, &src->abts_rsp_header, &dst->abts_rsp_header);
	ISP_IOXPUT_32(isp, src->abts_rsp_handle, &dst->abts_rsp_handle);
	ISP_IOXPUT_16(isp, src->abts_rsp_status, &dst->abts_rsp_status);
	ISP_IOXPUT_16(isp, src->abts_rsp_nphdl, &dst->abts_rsp_nphdl);
	ISP_IOXPUT_16(isp, src->abts_rsp_ctl_flags, &dst->abts_rsp_ctl_flags);
	ISP_IOXPUT_16(isp, src->abts_rsp_sof, &dst->abts_rsp_sof);
	ISP_IOXPUT_32(isp, src->abts_rsp_rxid_abts, &dst->abts_rsp_rxid_abts);
	ISP_IOXPUT_16(isp, src->abts_rsp_did_lo, &dst->abts_rsp_did_lo);
	ISP_IOXPUT_8(isp, src->abts_rsp_did_hi, &dst->abts_rsp_did_hi);
	ISP_IOXPUT_8(isp, src->abts_rsp_r_ctl, &dst->abts_rsp_r_ctl);
	ISP_IOXPUT_16(isp, src->abts_rsp_sid_lo, &dst->abts_rsp_sid_lo);
	ISP_IOXPUT_8(isp, src->abts_rsp_sid_hi, &dst->abts_rsp_sid_hi);
	ISP_IOXPUT_8(isp, src->abts_rsp_cs_ctl, &dst->abts_rsp_cs_ctl);
	ISP_IOXPUT_16(isp, src->abts_rsp_f_ctl_lo, &dst->abts_rsp_f_ctl_lo);
	ISP_IOXPUT_8(isp, src->abts_rsp_f_ctl_hi, &dst->abts_rsp_f_ctl_hi);
	ISP_IOXPUT_8(isp, src->abts_rsp_type, &dst->abts_rsp_type);
	ISP_IOXPUT_16(isp, src->abts_rsp_seq_cnt, &dst->abts_rsp_seq_cnt);
	ISP_IOXPUT_8(isp, src->abts_rsp_df_ctl, &dst->abts_rsp_df_ctl);
	ISP_IOXPUT_8(isp, src->abts_rsp_seq_id, &dst->abts_rsp_seq_id);
	ISP_IOXPUT_16(isp, src->abts_rsp_rx_id, &dst->abts_rsp_rx_id);
	ISP_IOXPUT_16(isp, src->abts_rsp_ox_id, &dst->abts_rsp_ox_id);
	ISP_IOXPUT_32(isp, src->abts_rsp_param, &dst->abts_rsp_param);
	if (src->abts_rsp_r_ctl == BA_ACC) {
		ISP_IOXPUT_16(isp, src->abts_rsp_payload.ba_acc.reserved,
		    &dst->abts_rsp_payload.ba_acc.reserved);
		ISP_IOXPUT_8(isp, src->abts_rsp_payload.ba_acc.last_seq_id,
		    &dst->abts_rsp_payload.ba_acc.last_seq_id);
		ISP_IOXPUT_8(isp, src->abts_rsp_payload.ba_acc.seq_id_valid,
		    &dst->abts_rsp_payload.ba_acc.seq_id_valid);
		ISP_IOXPUT_16(isp, src->abts_rsp_payload.ba_acc.aborted_rx_id,
		    &dst->abts_rsp_payload.ba_acc.aborted_rx_id);
		ISP_IOXPUT_16(isp, src->abts_rsp_payload.ba_acc.aborted_ox_id,
		    &dst->abts_rsp_payload.ba_acc.aborted_ox_id);
		ISP_IOXPUT_16(isp, src->abts_rsp_payload.ba_acc.high_seq_cnt,
		    &dst->abts_rsp_payload.ba_acc.high_seq_cnt);
		ISP_IOXPUT_16(isp, src->abts_rsp_payload.ba_acc.low_seq_cnt,
		    &dst->abts_rsp_payload.ba_acc.low_seq_cnt);
		for (i = 0; i < 4; i++) {
			ISP_IOXPUT_16(isp,
			    src->abts_rsp_payload.ba_acc.reserved2[i],
			    &dst->abts_rsp_payload.ba_acc.reserved2[i]);
		}
	} else if (src->abts_rsp_r_ctl == BA_RJT) {
		ISP_IOXPUT_8(isp, src->abts_rsp_payload.ba_rjt.vendor_unique,
		    &dst->abts_rsp_payload.ba_rjt.vendor_unique);
		ISP_IOXPUT_8(isp, src->abts_rsp_payload.ba_rjt.explanation,
		    &dst->abts_rsp_payload.ba_rjt.explanation);
		ISP_IOXPUT_8(isp, src->abts_rsp_payload.ba_rjt.reason,
		    &dst->abts_rsp_payload.ba_rjt.reason);
		ISP_IOXPUT_8(isp, src->abts_rsp_payload.ba_rjt.reserved,
		    &dst->abts_rsp_payload.ba_rjt.reserved);
		for (i = 0; i < 12; i++) {
			ISP_IOXPUT_16(isp,
			    src->abts_rsp_payload.ba_rjt.reserved2[i],
			    &dst->abts_rsp_payload.ba_rjt.reserved2[i]);
		}
	} else {
		for (i = 0; i < 16; i++) {
			ISP_IOXPUT_8(isp, src->abts_rsp_payload.reserved[i],
			    &dst->abts_rsp_payload.reserved[i]);
		}
	}
	ISP_IOXPUT_32(isp, src->abts_rsp_rxid_task, &dst->abts_rsp_rxid_task);
}

void
isp_get_abts_rsp(ispsoftc_t *isp, abts_rsp_t *src, abts_rsp_t *dst)
{
	int i;

	isp_get_hdr(isp, &src->abts_rsp_header, &dst->abts_rsp_header);
	ISP_IOXGET_32(isp, &src->abts_rsp_handle, dst->abts_rsp_handle);
	ISP_IOXGET_16(isp, &src->abts_rsp_status, dst->abts_rsp_status);
	ISP_IOXGET_16(isp, &src->abts_rsp_nphdl, dst->abts_rsp_nphdl);
	ISP_IOXGET_16(isp, &src->abts_rsp_ctl_flags, dst->abts_rsp_ctl_flags);
	ISP_IOXGET_16(isp, &src->abts_rsp_sof, dst->abts_rsp_sof);
	ISP_IOXGET_32(isp, &src->abts_rsp_rxid_abts, dst->abts_rsp_rxid_abts);
	ISP_IOXGET_16(isp, &src->abts_rsp_did_lo, dst->abts_rsp_did_lo);
	ISP_IOXGET_8(isp, &src->abts_rsp_did_hi, dst->abts_rsp_did_hi);
	ISP_IOXGET_8(isp, &src->abts_rsp_r_ctl, dst->abts_rsp_r_ctl);
	ISP_IOXGET_16(isp, &src->abts_rsp_sid_lo, dst->abts_rsp_sid_lo);
	ISP_IOXGET_8(isp, &src->abts_rsp_sid_hi, dst->abts_rsp_sid_hi);
	ISP_IOXGET_8(isp, &src->abts_rsp_cs_ctl, dst->abts_rsp_cs_ctl);
	ISP_IOXGET_16(isp, &src->abts_rsp_f_ctl_lo, dst->abts_rsp_f_ctl_lo);
	ISP_IOXGET_8(isp, &src->abts_rsp_f_ctl_hi, dst->abts_rsp_f_ctl_hi);
	ISP_IOXGET_8(isp, &src->abts_rsp_type, dst->abts_rsp_type);
	ISP_IOXGET_16(isp, &src->abts_rsp_seq_cnt, dst->abts_rsp_seq_cnt);
	ISP_IOXGET_8(isp, &src->abts_rsp_df_ctl, dst->abts_rsp_df_ctl);
	ISP_IOXGET_8(isp, &src->abts_rsp_seq_id, dst->abts_rsp_seq_id);
	ISP_IOXGET_16(isp, &src->abts_rsp_rx_id, dst->abts_rsp_rx_id);
	ISP_IOXGET_16(isp, &src->abts_rsp_ox_id, dst->abts_rsp_ox_id);
	ISP_IOXGET_32(isp, &src->abts_rsp_param, dst->abts_rsp_param);
	for (i = 0; i < 8; i++) {
		ISP_IOXGET_8(isp, &src->abts_rsp_payload.rsp.reserved[i],
		    dst->abts_rsp_payload.rsp.reserved[i]);
	}
	ISP_IOXGET_32(isp, &src->abts_rsp_payload.rsp.subcode1,
	    dst->abts_rsp_payload.rsp.subcode1);
	ISP_IOXGET_32(isp, &src->abts_rsp_payload.rsp.subcode2,
	    dst->abts_rsp_payload.rsp.subcode2);
	ISP_IOXGET_32(isp, &src->abts_rsp_rxid_task, dst->abts_rsp_rxid_task);
}
#endif	/* ISP_TARGET_MODE */
/*
 * vim:ts=8:sw=8
 */
