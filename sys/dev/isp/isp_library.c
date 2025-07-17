/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 *  Copyright (c) 2009-2020 Alexander Motin <mav@FreeBSD.org>
 *  Copyright (c) 1997-2009 by Matthew Jacob
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
 *
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

const char *isp_class3_roles[4] = {
    "None", "Target", "Initiator", "Target/Initiator"
};

/*
 * Command shipping- finish off first queue entry and do dma mapping and additional segments as needed.
 *
 * Called with the first queue entry at least partially filled out.
 */
int
isp_send_cmd(ispsoftc_t *isp, void *fqe, void *segp, uint32_t nsegs)
{
	ispcontreq64_t crq;
	uint8_t type, nqe = 1;
	uint32_t seg, seglim, nxt;
	ispds64_t *dsp64 = NULL;
	void *qe0, *qe1;

	qe0 = isp_getrqentry(isp);
	if (qe0 == NULL)
		return (CMD_EAGAIN);
	nxt = ISP_NXT_QENTRY(isp->isp_reqidx, RQUEST_QUEUE_LEN(isp));
	type = ((isphdr_t *)fqe)->rqs_entry_type;

	/*
	 * If we have no data to transmit, just copy the first IOCB and start it up.
	 */
	if (nsegs == 0)
		goto copy_and_sync;

	/*
	 * First figure out how many pieces of data to transfer, what
	 * kind and how many we can put into the first queue entry.
	 */
	switch (type) {
	case RQSTYPE_T7RQS:
		dsp64 = &((ispreqt7_t *)fqe)->req_dataseg;
		seglim = 1;
		break;
#ifdef	ISP_TARGET_MODE
	case RQSTYPE_CTIO7:
		dsp64 = &((ct7_entry_t *)fqe)->rsp.m0.ds;
		seglim = 1;
		break;
#endif
	default:
		panic("%s: unsupported IOCB type 0x%x\n", __func__, type);
	}
	if (seglim > nsegs)
		seglim = nsegs;
	seg = 0;
	while (seg < seglim)
		XS_GET_DMA64_SEG(dsp64++, segp, seg++);

	/*
	 * Second, start building additional continuation segments as needed.
	 */
	while (seg < nsegs) {
		if (!isp_rqentry_avail(isp, ++nqe))
			return (CMD_EAGAIN);

		ISP_MEMZERO(&crq, QENTRY_LEN);
		crq.req_header.rqs_entry_type = RQSTYPE_A64_CONT;
		crq.req_header.rqs_entry_count = 1;
		dsp64 = crq.req_dataseg;

		seglim = seg + ISP_CDSEG64;
		if (seglim > nsegs)
			seglim = nsegs;
		while (seg < seglim)
			XS_GET_DMA64_SEG(dsp64++, segp, seg++);

		qe1 = ISP_QUEUE_ENTRY(isp->isp_rquest, nxt);
		isp_put_cont64_req(isp, &crq, qe1);
		if (isp->isp_dblev & ISP_LOGDEBUG1) {
			isp_print_bytes(isp, "additional queue entry",
			    QENTRY_LEN, qe1);
		}

		nxt = ISP_NXT_QENTRY(nxt, RQUEST_QUEUE_LEN(isp));
	}

copy_and_sync:
	((isphdr_t *)fqe)->rqs_entry_count = nqe;
	switch (type) {
	case RQSTYPE_T7RQS:
		((ispreqt7_t *)fqe)->req_seg_count = nsegs;
		isp_put_request_t7(isp, fqe, qe0);
		break;
#ifdef	ISP_TARGET_MODE
	case RQSTYPE_CTIO7:
		if (((ct7_entry_t *)fqe)->ct_flags & CT7_FLAG_MODE2) {
			((ct7_entry_t *)fqe)->ct_seg_count = 1;
		} else {
			((ct7_entry_t *)fqe)->ct_seg_count = nsegs;
		}
		isp_put_ctio7(isp, fqe, qe0);
		break;
#endif
	default:
		panic("%s: unsupported IOCB type 0x%x\n", __func__, type);
	}
	if (isp->isp_dblev & ISP_LOGDEBUG1) {
		isp_print_bytes(isp, "first queue entry", QENTRY_LEN, qe0);
	}
	ISP_ADD_REQUEST(isp, nxt);
	return (0);
}

uint32_t
isp_allocate_handle(ispsoftc_t *isp, void *xs, int type)
{
	isp_hdl_t *hdp;

	hdp = isp->isp_xffree;
	if (hdp == NULL)
		return (ISP_HANDLE_FREE);
	isp->isp_xffree = hdp->cmd;
	hdp->cmd = xs;
	hdp->handle = (hdp - isp->isp_xflist);
	hdp->handle |= (type << ISP_HANDLE_USAGE_SHIFT);
	hdp->handle |= (isp->isp_seqno++ << ISP_HANDLE_SEQ_SHIFT);
	return (hdp->handle);
}

void *
isp_find_xs(ispsoftc_t *isp, uint32_t handle)
{
	if (!ISP_VALID_HANDLE(isp, handle)) {
		isp_prt(isp, ISP_LOGERR, "%s: bad handle 0x%x", __func__, handle);
		return (NULL);
	}
	return (isp->isp_xflist[(handle & ISP_HANDLE_CMD_MASK)].cmd);
}

uint32_t
isp_find_handle(ispsoftc_t *isp, void *xs)
{
	uint32_t i, foundhdl = ISP_HANDLE_FREE;

	if (xs != NULL) {
		for (i = 0; i < ISP_HANDLE_NUM(isp); i++) {
			if (isp->isp_xflist[i].cmd != xs) {
				continue;
			}
			foundhdl = isp->isp_xflist[i].handle;
			break;
		}
	}
	return (foundhdl);
}

void
isp_destroy_handle(ispsoftc_t *isp, uint32_t handle)
{
	if (!ISP_VALID_HANDLE(isp, handle)) {
		isp_prt(isp, ISP_LOGERR, "%s: bad handle 0x%x", __func__, handle);
	} else {
		isp->isp_xflist[(handle & ISP_HANDLE_CMD_MASK)].handle = ISP_HANDLE_FREE;
		isp->isp_xflist[(handle & ISP_HANDLE_CMD_MASK)].cmd = isp->isp_xffree;
		isp->isp_xffree = &isp->isp_xflist[(handle & ISP_HANDLE_CMD_MASK)];
	}
}

#define	TBA	(4 * (((QENTRY_LEN >> 2) * 3) + 1) + 1)
void
isp_print_qentry(ispsoftc_t *isp, const char *msg, int idx, void *arg)
{
	char buf[TBA];
	int amt, i, j;
	uint8_t *ptr = arg;

	isp_prt(isp, ISP_LOGALL, "%s index %d=>", msg, idx);
	for (buf[0] = 0, amt = i = 0; i < 4; i++) {
		buf[0] = 0;
		ISP_SNPRINTF(buf, TBA, "  ");
		for (j = 0; j < (QENTRY_LEN >> 2); j++) {
			ISP_SNPRINTF(buf, TBA, "%s %02x", buf, ptr[amt++] & 0xff);
		}
		isp_prt(isp, ISP_LOGALL, "%s", buf);
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
			ISP_SNPRINTF(buf, 128, "%s %02x", buf, ptr[off++] & 0xff);
			if (off == amt) {
				break;
			}
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
isp_fc_runstate(ispsoftc_t *isp, int chan, int tval)
{
	fcparam *fcp = FCPARAM(isp, chan);
	int res;

again:
	if (fcp->role == ISP_ROLE_NONE)
		return (-1);
	res = isp_control(isp, ISPCTL_FCLINK_TEST, chan, tval);
	if (res > 0)
		goto again;
	if (res < 0)
		return (fcp->isp_loopstate);
	res = isp_control(isp, ISPCTL_SCAN_LOOP, chan);
	if (res > 0)
		goto again;
	if (res < 0)
		return (fcp->isp_loopstate);
	res = isp_control(isp, ISPCTL_SCAN_FABRIC, chan);
	if (res > 0)
		goto again;
	if (res < 0)
		return (fcp->isp_loopstate);
	res = isp_control(isp, ISPCTL_PDB_SYNC, chan);
	if (res > 0)
		goto again;
	return (fcp->isp_loopstate);
}

/*
 * Fibre Channel Support routines
 */
void
isp_dump_portdb(ispsoftc_t *isp, int chan)
{
	fcparam *fcp = FCPARAM(isp, chan);
	int i;

	for (i = 0; i < MAX_FC_TARG; i++) {
		char buf1[64], buf2[64];
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
		fcportdb_t *lp = &fcp->portdb[i];

		if (lp->state == FC_PORTDB_STATE_NIL) {
			continue;
		}
		isp_gen_role_str(buf1, sizeof (buf1), lp->prli_word3);
		isp_gen_role_str(buf2, sizeof (buf2), lp->new_prli_word3);
		isp_prt(isp, ISP_LOGALL, "Chan %d [%d]: hdl 0x%x %s %s 0x%06x =>%s 0x%06x; WWNN 0x%08x%08x WWPN 0x%08x%08x",
		    chan, i, lp->handle, dbs[lp->state], buf1, lp->portid, buf2, lp->new_portid,
		    (uint32_t) (lp->node_wwn >> 32), (uint32_t) (lp->node_wwn), (uint32_t) (lp->port_wwn >> 32), (uint32_t) (lp->port_wwn));
	}
}

void
isp_gen_role_str(char *buf, size_t len, uint16_t p3)
{
	int nd = 0;
	buf[0] = '(';
	buf[1] = 0;
	if (p3 & PRLI_WD3_ENHANCED_DISCOVERY) {
		nd++;
		strlcat(buf, "EDisc", len);
	}
	if (p3 & PRLI_WD3_REC_SUPPORT) {
		if (nd++) {
			strlcat(buf, ",", len);
		}
		strlcat(buf, "REC", len);
	}
	if (p3 & PRLI_WD3_TASK_RETRY_IDENTIFICATION_REQUESTED) {
		if (nd++) {
			strlcat(buf, ",", len);
		}
		strlcat(buf, "RetryID", len);
	}
	if (p3 & PRLI_WD3_RETRY) {
		if (nd++) {
			strlcat(buf, ",", len);
		}
		strlcat(buf, "Retry", len);
	}
	if (p3 & PRLI_WD3_CONFIRMED_COMPLETION_ALLOWED) {
		if (nd++) {
			strlcat(buf, ",", len);
		}
		strlcat(buf, "CNFRM", len);
	}
	if (p3 & PRLI_WD3_DATA_OVERLAY_ALLOWED) {
		if (nd++) {
			strlcat(buf, ",", len);
		}
		strlcat(buf, "DOver", len);
	}
	if (p3 & PRLI_WD3_INITIATOR_FUNCTION) {
		if (nd++) {
			strlcat(buf, ",", len);
		}
		strlcat(buf, "INI", len);
	}
	if (p3 & PRLI_WD3_TARGET_FUNCTION) {
		if (nd++) {
			strlcat(buf, ",", len);
		}
		strlcat(buf, "TGT", len);
	}
	if (p3 & PRLI_WD3_READ_FCP_XFER_RDY_DISABLED) {
		if (nd++) {
			strlcat(buf, ",", len);
		}
		strlcat(buf, "RdXfrDis", len);
	}
	if (p3 & PRLI_WD3_WRITE_FCP_XFER_RDY_DISABLED) {
		if (nd++) {
			strlcat(buf, ",", len);
		}
		strlcat(buf, "XfrDis", len);
	}
	strlcat(buf, ")", len);
}

const char *
isp_fc_fw_statename(int state)
{
	switch (state) {
	case FW_CONFIG_WAIT:	return "Config Wait";
	case FW_WAIT_LINK:	return "Wait Link";
	case FW_WAIT_LOGIN:	return "Wait Login";
	case FW_READY:		return "Ready";
	case FW_LOSS_OF_SYNC:	return "Loss Of Sync";
	case FW_ERROR:		return "Error";
	case FW_REINIT:		return "Re-Init";
	case FW_NON_PART:	return "Nonparticipating";
	default:		return "?????";
	}
}

const char *
isp_fc_loop_statename(int state)
{
	switch (state) {
	case LOOP_NIL:                  return "NIL";
	case LOOP_HAVE_LINK:            return "Have Link";
	case LOOP_HAVE_ADDR:            return "Have Address";
	case LOOP_TESTING_LINK:         return "Testing Link";
	case LOOP_LTEST_DONE:           return "Link Test Done";
	case LOOP_SCANNING_LOOP:        return "Scanning Loop";
	case LOOP_LSCAN_DONE:           return "Loop Scan Done";
	case LOOP_SCANNING_FABRIC:      return "Scanning Fabric";
	case LOOP_FSCAN_DONE:           return "Fabric Scan Done";
	case LOOP_SYNCING_PDB:          return "Syncing PDB";
	case LOOP_READY:                return "Ready"; 
	default:                        return "?????";
	}
}

const char *
isp_fc_toponame(fcparam *fcp)
{

	if (fcp->isp_loopstate < LOOP_HAVE_ADDR) {
		return "Unavailable";
	}
	switch (fcp->isp_topo) {
	case TOPO_NL_PORT:      return "Private Loop (NL_Port)";
	case TOPO_FL_PORT:      return "Public Loop (FL_Port)";
	case TOPO_N_PORT:       return "Point-to-Point (N_Port)";
	case TOPO_F_PORT:       return "Fabric (F_Port)";
	case TOPO_PTP_STUB:     return "Point-to-Point (no response)";
	default:                return "?????";
	}
}

void
isp_clear_commands(ispsoftc_t *isp)
{
	uint32_t tmp;
	isp_hdl_t *hdp;
#ifdef	ISP_TARGET_MODE
	isp_notify_t notify;
#endif

	for (tmp = 0; isp->isp_xflist && tmp < ISP_HANDLE_NUM(isp); tmp++) {

		hdp = &isp->isp_xflist[tmp];
		switch (ISP_H2HT(hdp->handle)) {
		case ISP_HANDLE_INITIATOR: {
			XS_T *xs = hdp->cmd;
			ISP_DMAFREE(isp, xs);
			isp_destroy_handle(isp, hdp->handle);
			XS_SET_RESID(xs, XS_XFRLEN(xs));
			XS_SETERR(xs, HBA_BUSRESET);
			isp_done(xs);
			break;
		}
#ifdef	ISP_TARGET_MODE
		case ISP_HANDLE_TARGET: {
			ct7_entry_t ctio;

			ISP_DMAFREE(isp, hdp->cmd);
			ISP_MEMZERO(&ctio, sizeof(ct7_entry_t));
			ctio.ct_syshandle = hdp->handle;
			ctio.ct_nphdl = CT_HBA_RESET;
			ctio.ct_header.rqs_entry_type = RQSTYPE_CTIO7;
			isp_async(isp, ISPASYNC_TARGET_ACTION, &ctio);
			break;
		}
#endif
		case ISP_HANDLE_CTRL:
			ISP_MEMZERO(hdp->cmd, QENTRY_LEN);
			wakeup(hdp->cmd);
			isp_destroy_handle(isp, hdp->handle);
			break;
		}
	}
#ifdef	ISP_TARGET_MODE
	for (tmp = 0; tmp < isp->isp_nchan; tmp++) {
		ISP_MEMZERO(&notify, sizeof (isp_notify_t));
		notify.nt_ncode = NT_HBA_RESET;
		notify.nt_hba = isp;
		notify.nt_wwn = INI_ANY;
		notify.nt_nphdl = NIL_HANDLE;
		notify.nt_sid = PORT_ANY;
		notify.nt_did = PORT_ANY;
		notify.nt_tgt = TGT_ANY;
		notify.nt_channel = tmp;
		notify.nt_lun = LUN_ANY;
		notify.nt_tagval = TAG_ANY;
		isp_async(isp, ISPASYNC_TARGET_NOTIFY, &notify);
	}
#endif
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

/*
 * Swizzle/Copy Functions
 */
void
isp_put_hdr(ispsoftc_t *isp, isphdr_t *hpsrc, isphdr_t *hpdst)
{
	ISP_IOXPUT_8(isp, hpsrc->rqs_entry_type, &hpdst->rqs_entry_type);
	ISP_IOXPUT_8(isp, hpsrc->rqs_entry_count, &hpdst->rqs_entry_count);
	ISP_IOXPUT_8(isp, hpsrc->rqs_seqno, &hpdst->rqs_seqno);
	ISP_IOXPUT_8(isp, hpsrc->rqs_flags, &hpdst->rqs_flags);
}

void
isp_get_hdr(ispsoftc_t *isp, isphdr_t *hpsrc, isphdr_t *hpdst)
{
	ISP_IOXGET_8(isp, &hpsrc->rqs_entry_type, hpdst->rqs_entry_type);
	ISP_IOXGET_8(isp, &hpsrc->rqs_entry_count, hpdst->rqs_entry_count);
	ISP_IOXGET_8(isp, &hpsrc->rqs_seqno, hpdst->rqs_seqno);
	ISP_IOXGET_8(isp, &hpsrc->rqs_flags, hpdst->rqs_flags);
}

int
isp_get_response_type(ispsoftc_t *isp, isphdr_t *hp)
{
	uint8_t type;
	ISP_IOXGET_8(isp, &hp->rqs_entry_type, type);
	return ((int)type);
}

void
isp_put_marker_24xx(ispsoftc_t *isp, isp_marker_24xx_t *src, isp_marker_24xx_t *dst)
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
	for (i = 0; i < nitems(src->mrk_lun); i++) {
		ISP_IOXPUT_8(isp, src->mrk_lun[i], &dst->mrk_lun[i]);
	}
	for (i = 0; i < nitems(src->mrk_reserved3); i++) {
		ISP_IOXPUT_8(isp, src->mrk_reserved3[i], &dst->mrk_reserved3[i]);
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
	for (i = 0; i < (nitems(src->req_lun) >> 2); i++ ) {
		*b++ = ISP_SWAP32(isp, *a++);
	}
	ISP_IOXPUT_8(isp, src->req_alen_datadir, &dst->req_alen_datadir);
	ISP_IOXPUT_8(isp, src->req_task_management, &dst->req_task_management);
	ISP_IOXPUT_8(isp, src->req_task_attribute, &dst->req_task_attribute);
	ISP_IOXPUT_8(isp, src->req_crn, &dst->req_crn);
	a = (uint32_t *) src->req_cdb;
	b = (uint32_t *) dst->req_cdb;
	for (i = 0; i < (nitems(src->req_cdb) >> 2); i++) {
		*b++ = ISP_SWAP32(isp, *a++);
	}
	ISP_IOXPUT_32(isp, src->req_dl, &dst->req_dl);
	ISP_IOXPUT_16(isp, src->req_tidlo, &dst->req_tidlo);
	ISP_IOXPUT_8(isp, src->req_tidhi, &dst->req_tidhi);
	ISP_IOXPUT_8(isp, src->req_vpidx, &dst->req_vpidx);
	ISP_IOXPUT_32(isp, src->req_dataseg.ds_base, &dst->req_dataseg.ds_base);
	ISP_IOXPUT_32(isp, src->req_dataseg.ds_basehi, &dst->req_dataseg.ds_basehi);
	ISP_IOXPUT_32(isp, src->req_dataseg.ds_count, &dst->req_dataseg.ds_count);
}

void
isp_put_24xx_tmf(ispsoftc_t *isp, isp24xx_tmf_t *src, isp24xx_tmf_t *dst)
{
	int i;
	uint32_t *a, *b;

	isp_put_hdr(isp, &src->tmf_header, &dst->tmf_header);
	ISP_IOXPUT_32(isp, src->tmf_handle, &dst->tmf_handle);
	ISP_IOXPUT_16(isp, src->tmf_nphdl, &dst->tmf_nphdl);
	ISP_IOXPUT_16(isp, src->tmf_delay, &dst->tmf_delay);
	ISP_IOXPUT_16(isp, src->tmf_timeout, &dst->tmf_timeout);
	for (i = 0; i < nitems(src->tmf_reserved0); i++) {
		ISP_IOXPUT_8(isp, src->tmf_reserved0[i], &dst->tmf_reserved0[i]);
	}
	a = (uint32_t *) src->tmf_lun;
	b = (uint32_t *) dst->tmf_lun;
	for (i = 0; i < (nitems(src->tmf_lun) >> 2); i++ ) {
		*b++ = ISP_SWAP32(isp, *a++);
	}
	ISP_IOXPUT_32(isp, src->tmf_flags, &dst->tmf_flags);
	for (i = 0; i < nitems(src->tmf_reserved1); i++) {
		ISP_IOXPUT_8(isp, src->tmf_reserved1[i], &dst->tmf_reserved1[i]);
	}
	ISP_IOXPUT_16(isp, src->tmf_tidlo, &dst->tmf_tidlo);
	ISP_IOXPUT_8(isp, src->tmf_tidhi, &dst->tmf_tidhi);
	ISP_IOXPUT_8(isp, src->tmf_vpidx, &dst->tmf_vpidx);
	for (i = 0; i < nitems(src->tmf_reserved2); i++) {
		ISP_IOXPUT_8(isp, src->tmf_reserved2[i], &dst->tmf_reserved2[i]);
	}
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
	ISP_IOXPUT_16(isp, src->abrt_queue_number, &dst->abrt_queue_number);
	for (i = 0; i < nitems(src->abrt_reserved); i++) {
		ISP_IOXPUT_8(isp, src->abrt_reserved[i], &dst->abrt_reserved[i]);
	}
	ISP_IOXPUT_16(isp, src->abrt_tidlo, &dst->abrt_tidlo);
	ISP_IOXPUT_8(isp, src->abrt_tidhi, &dst->abrt_tidhi);
	ISP_IOXPUT_8(isp, src->abrt_vpidx, &dst->abrt_vpidx);
	for (i = 0; i < nitems(src->abrt_reserved1); i++) {
		ISP_IOXPUT_8(isp, src->abrt_reserved1[i], &dst->abrt_reserved1[i]);
	}
}

void
isp_put_cont64_req(ispsoftc_t *isp, ispcontreq64_t *src, ispcontreq64_t *dst)
{
	int i;
	isp_put_hdr(isp, &src->req_header, &dst->req_header);
	for (i = 0; i < ISP_CDSEG64; i++) {
		ISP_IOXPUT_32(isp, src->req_dataseg[i].ds_base, &dst->req_dataseg[i].ds_base);
		ISP_IOXPUT_32(isp, src->req_dataseg[i].ds_basehi, &dst->req_dataseg[i].ds_basehi);
		ISP_IOXPUT_32(isp, src->req_dataseg[i].ds_count, &dst->req_dataseg[i].ds_count);
	}
}

void
isp_get_cont_response(ispsoftc_t *isp, ispstatus_cont_t *src, ispstatus_cont_t *dst)
{
	int i;
	uint32_t *a, *b;

	isp_get_hdr(isp, &src->req_header, &dst->req_header);
	a = (uint32_t *) src->req_sense_data;
	b = (uint32_t *) dst->req_sense_data;
	for (i = 0; i < (sizeof (src->req_sense_data) / sizeof (uint32_t)); i++)
		ISP_IOZGET_32(isp, a++, *b++);
}

void
isp_get_24xx_response(ispsoftc_t *isp, isp24xx_statusreq_t *src, isp24xx_statusreq_t *dst)
{
	int i;
	uint32_t *s, *d;

	isp_get_hdr(isp, &src->req_header, &dst->req_header);
	ISP_IOXGET_32(isp, &src->req_handle, dst->req_handle);
	ISP_IOXGET_16(isp, &src->req_completion_status, dst->req_completion_status);
	ISP_IOXGET_16(isp, &src->req_oxid, dst->req_oxid);
	ISP_IOXGET_32(isp, &src->req_resid, dst->req_resid);
	ISP_IOXGET_16(isp, &src->req_reserved0, dst->req_reserved0);
	ISP_IOXGET_16(isp, &src->req_state_flags, dst->req_state_flags);
	ISP_IOXGET_16(isp, &src->req_retry_delay, dst->req_retry_delay);
	ISP_IOXGET_16(isp, &src->req_scsi_status, dst->req_scsi_status);
	ISP_IOXGET_32(isp, &src->req_fcp_residual, dst->req_fcp_residual);
	ISP_IOXGET_32(isp, &src->req_sense_len, dst->req_sense_len);
	ISP_IOXGET_32(isp, &src->req_response_len, dst->req_response_len);
	s = (uint32_t *)src->req_rsp_sense;
	d = (uint32_t *)dst->req_rsp_sense;
	for (i = 0; i < (nitems(src->req_rsp_sense) >> 2); i++) {
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
	ISP_IOXGET_16(isp, &src->abrt_queue_number, dst->abrt_queue_number);
	for (i = 0; i < nitems(src->abrt_reserved); i++) {
		ISP_IOXGET_8(isp, &src->abrt_reserved[i], dst->abrt_reserved[i]);
	}
	ISP_IOXGET_16(isp, &src->abrt_tidlo, dst->abrt_tidlo);
	ISP_IOXGET_8(isp, &src->abrt_tidhi, dst->abrt_tidhi);
	ISP_IOXGET_8(isp, &src->abrt_vpidx, dst->abrt_vpidx);
	for (i = 0; i < nitems(src->abrt_reserved1); i++) {
		ISP_IOXGET_8(isp, &src->abrt_reserved1[i], dst->abrt_reserved1[i]);
	}
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
	ISP_IOXPUT_16(isp, src->icb_msixresp, &dst->icb_msixresp);
	ISP_IOXPUT_16(isp, src->icb_msixatio, &dst->icb_msixatio);
	for (i = 0; i < 2; i++) {
		ISP_IOXPUT_16(isp, src->icb_reserved1[i], &dst->icb_reserved1[i]);
	}
	ISP_IOXPUT_16(isp, src->icb_atio_in, &dst->icb_atio_in);
	ISP_IOXPUT_16(isp, src->icb_atioqlen, &dst->icb_atioqlen);
	for (i = 0; i < 4; i++) {
		ISP_IOXPUT_16(isp, src->icb_atioqaddr[i], &dst->icb_atioqaddr[i]);
	}
	ISP_IOXPUT_16(isp, src->icb_idelaytimer, &dst->icb_idelaytimer);
	ISP_IOXPUT_16(isp, src->icb_logintime, &dst->icb_logintime);
	ISP_IOXPUT_32(isp, src->icb_fwoptions1, &dst->icb_fwoptions1);
	ISP_IOXPUT_32(isp, src->icb_fwoptions2, &dst->icb_fwoptions2);
	ISP_IOXPUT_32(isp, src->icb_fwoptions3, &dst->icb_fwoptions3);
	ISP_IOXPUT_16(isp, src->icb_qos, &dst->icb_qos);
	for (i = 0; i < 3; i++)
		ISP_IOXPUT_16(isp, src->icb_reserved2[i], &dst->icb_reserved2[i]);
	for (i = 0; i < 6; i++)
		ISP_IOXPUT_8(isp, src->icb_enodemac[i], &dst->icb_enodemac[i]);
	ISP_IOXPUT_16(isp, src->icb_disctime, &dst->icb_disctime);
	for (i = 0; i < 4; i++)
		ISP_IOXPUT_16(isp, src->icb_reserved3[i], &dst->icb_reserved3[i]);
}

void
isp_put_icb_2400_vpinfo(ispsoftc_t *isp, isp_icb_2400_vpinfo_t *src, isp_icb_2400_vpinfo_t *dst)
{
	ISP_IOXPUT_16(isp, src->vp_count, &dst->vp_count);
	ISP_IOXPUT_16(isp, src->vp_global_options, &dst->vp_global_options);
}

void
isp_put_vp_port_info(ispsoftc_t *isp, vp_port_info_t *src, vp_port_info_t *dst)
{
	int i;
	ISP_IOXPUT_16(isp, src->vp_port_status, &dst->vp_port_status);
	ISP_IOXPUT_8(isp, src->vp_port_options, &dst->vp_port_options);
	ISP_IOXPUT_8(isp, src->vp_port_loopid, &dst->vp_port_loopid);
	for (i = 0; i < 8; i++) {
		ISP_IOXPUT_8(isp, src->vp_port_portname[i], &dst->vp_port_portname[i]);
	}
	for (i = 0; i < 8; i++) {
		ISP_IOXPUT_8(isp, src->vp_port_nodename[i], &dst->vp_port_nodename[i]);
	}
	/* we never *put* portid_lo/portid_hi */
}

void
isp_get_vp_port_info(ispsoftc_t *isp, vp_port_info_t *src, vp_port_info_t *dst)
{
	int i;
	ISP_IOXGET_16(isp, &src->vp_port_status, dst->vp_port_status);
	ISP_IOXGET_8(isp, &src->vp_port_options, dst->vp_port_options);
	ISP_IOXGET_8(isp, &src->vp_port_loopid, dst->vp_port_loopid);
	for (i = 0; i < nitems(src->vp_port_portname); i++) {
		ISP_IOXGET_8(isp, &src->vp_port_portname[i], dst->vp_port_portname[i]);
	}
	for (i = 0; i < nitems(src->vp_port_nodename); i++) {
		ISP_IOXGET_8(isp, &src->vp_port_nodename[i], dst->vp_port_nodename[i]);
	}
	ISP_IOXGET_16(isp, &src->vp_port_portid_lo, dst->vp_port_portid_lo);
	ISP_IOXGET_16(isp, &src->vp_port_portid_hi, dst->vp_port_portid_hi);
}

void
isp_put_vp_ctrl_info(ispsoftc_t *isp, vp_ctrl_info_t *src, vp_ctrl_info_t *dst)
{
	int i;
	isp_put_hdr(isp, &src->vp_ctrl_hdr, &dst->vp_ctrl_hdr);
	ISP_IOXPUT_32(isp, src->vp_ctrl_handle, &dst->vp_ctrl_handle);
	ISP_IOXPUT_16(isp, src->vp_ctrl_index_fail, &dst->vp_ctrl_index_fail);
	ISP_IOXPUT_16(isp, src->vp_ctrl_status, &dst->vp_ctrl_status);
	ISP_IOXPUT_16(isp, src->vp_ctrl_command, &dst->vp_ctrl_command);
	ISP_IOXPUT_16(isp, src->vp_ctrl_vp_count, &dst->vp_ctrl_vp_count);
	for (i = 0; i < nitems(src->vp_ctrl_idmap); i++) {
		ISP_IOXPUT_16(isp, src->vp_ctrl_idmap[i], &dst->vp_ctrl_idmap[i]);
	}
	for (i = 0; i < nitems(src->vp_ctrl_reserved); i++) {
		ISP_IOXPUT_16(isp, src->vp_ctrl_reserved[i], &dst->vp_ctrl_reserved[i]);
	}
	ISP_IOXPUT_16(isp, src->vp_ctrl_fcf_index, &dst->vp_ctrl_fcf_index);
}

void
isp_get_vp_ctrl_info(ispsoftc_t *isp, vp_ctrl_info_t *src, vp_ctrl_info_t *dst)
{
	int i;
	isp_get_hdr(isp, &src->vp_ctrl_hdr, &dst->vp_ctrl_hdr);
	ISP_IOXGET_32(isp, &src->vp_ctrl_handle, dst->vp_ctrl_handle);
	ISP_IOXGET_16(isp, &src->vp_ctrl_index_fail, dst->vp_ctrl_index_fail);
	ISP_IOXGET_16(isp, &src->vp_ctrl_status, dst->vp_ctrl_status);
	ISP_IOXGET_16(isp, &src->vp_ctrl_command, dst->vp_ctrl_command);
	ISP_IOXGET_16(isp, &src->vp_ctrl_vp_count, dst->vp_ctrl_vp_count);
	for (i = 0; i < nitems(src->vp_ctrl_idmap); i++) {
		ISP_IOXGET_16(isp, &src->vp_ctrl_idmap[i], dst->vp_ctrl_idmap[i]);
	}
	for (i = 0; i < nitems(src->vp_ctrl_reserved); i++) {
		ISP_IOXGET_16(isp, &src->vp_ctrl_reserved[i], dst->vp_ctrl_reserved[i]);
	}
	ISP_IOXGET_16(isp, &src->vp_ctrl_fcf_index, dst->vp_ctrl_fcf_index);
}

void
isp_put_vp_modify(ispsoftc_t *isp, vp_modify_t *src, vp_modify_t *dst)
{
	int i, j;
	isp_put_hdr(isp, &src->vp_mod_hdr, &dst->vp_mod_hdr);
	ISP_IOXPUT_32(isp, src->vp_mod_hdl, &dst->vp_mod_hdl);
	ISP_IOXPUT_16(isp, src->vp_mod_reserved0, &dst->vp_mod_reserved0);
	ISP_IOXPUT_16(isp, src->vp_mod_status, &dst->vp_mod_status);
	ISP_IOXPUT_8(isp, src->vp_mod_cmd, &dst->vp_mod_cmd);
	ISP_IOXPUT_8(isp, src->vp_mod_cnt, &dst->vp_mod_cnt);
	ISP_IOXPUT_8(isp, src->vp_mod_idx0, &dst->vp_mod_idx0);
	ISP_IOXPUT_8(isp, src->vp_mod_idx1, &dst->vp_mod_idx1);
	for (i = 0; i < nitems(src->vp_mod_ports); i++) {
		ISP_IOXPUT_8(isp, src->vp_mod_ports[i].options, &dst->vp_mod_ports[i].options);
		ISP_IOXPUT_8(isp, src->vp_mod_ports[i].loopid, &dst->vp_mod_ports[i].loopid);
		ISP_IOXPUT_16(isp, src->vp_mod_ports[i].reserved1, &dst->vp_mod_ports[i].reserved1);
		for (j = 0; j < nitems(src->vp_mod_ports[i].wwpn); j++) {
			ISP_IOXPUT_8(isp, src->vp_mod_ports[i].wwpn[j], &dst->vp_mod_ports[i].wwpn[j]);
		}
		for (j = 0; j < nitems(src->vp_mod_ports[i].wwnn); j++) {
			ISP_IOXPUT_8(isp, src->vp_mod_ports[i].wwnn[j], &dst->vp_mod_ports[i].wwnn[j]);
		}
	}
	for (i = 0; i < nitems(src->vp_mod_reserved2); i++) {
		ISP_IOXPUT_8(isp, src->vp_mod_reserved2[i], &dst->vp_mod_reserved2[i]);
	}
}

void
isp_get_vp_modify(ispsoftc_t *isp, vp_modify_t *src, vp_modify_t *dst)
{
	int i, j;
	isp_get_hdr(isp, &src->vp_mod_hdr, &dst->vp_mod_hdr);
	ISP_IOXGET_32(isp, &src->vp_mod_hdl, dst->vp_mod_hdl);
	ISP_IOXGET_16(isp, &src->vp_mod_reserved0, dst->vp_mod_reserved0);
	ISP_IOXGET_16(isp, &src->vp_mod_status, dst->vp_mod_status);
	ISP_IOXGET_8(isp, &src->vp_mod_cmd, dst->vp_mod_cmd);
	ISP_IOXGET_8(isp, &src->vp_mod_cnt, dst->vp_mod_cnt);
	ISP_IOXGET_8(isp, &src->vp_mod_idx0, dst->vp_mod_idx0);
	ISP_IOXGET_8(isp, &src->vp_mod_idx1, dst->vp_mod_idx1);
	for (i = 0; i < nitems(src->vp_mod_ports); i++) {
		ISP_IOXGET_8(isp, &src->vp_mod_ports[i].options, dst->vp_mod_ports[i].options);
		ISP_IOXGET_8(isp, &src->vp_mod_ports[i].loopid, dst->vp_mod_ports[i].loopid);
		ISP_IOXGET_16(isp, &src->vp_mod_ports[i].reserved1, dst->vp_mod_ports[i].reserved1);
		for (j = 0; j < nitems(src->vp_mod_ports[i].wwpn); j++) {
			ISP_IOXGET_8(isp, &src->vp_mod_ports[i].wwpn[j], dst->vp_mod_ports[i].wwpn[j]);
		}
		for (j = 0; j < nitems(src->vp_mod_ports[i].wwnn); j++) {
			ISP_IOXGET_8(isp, &src->vp_mod_ports[i].wwnn[j], dst->vp_mod_ports[i].wwnn[j]);
		}
	}
	for (i = 0; i < nitems(src->vp_mod_reserved2); i++) {
		ISP_IOXGET_8(isp, &src->vp_mod_reserved2[i], dst->vp_mod_reserved2[i]);
	}
}

void
isp_get_pdb_24xx(ispsoftc_t *isp, isp_pdb_24xx_t *src, isp_pdb_24xx_t *dst)
{
	int i;
	ISP_IOXGET_16(isp, &src->pdb_flags, dst->pdb_flags);
        ISP_IOXGET_8(isp, &src->pdb_curstate, dst->pdb_curstate);
        ISP_IOXGET_8(isp, &src->pdb_laststate, dst->pdb_laststate);
	for (i = 0; i < 4; i++) {
		ISP_IOXGET_8(isp, &src->pdb_hardaddr_bits[i], dst->pdb_hardaddr_bits[i]);
	}
	for (i = 0; i < 4; i++) {
		ISP_IOXGET_8(isp, &src->pdb_portid_bits[i], dst->pdb_portid_bits[i]);
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
		ISP_IOXGET_8(isp, &src->pdb_reserved1[i], dst->pdb_reserved1[i]);
	}
}

void
isp_get_pnhle_24xx(ispsoftc_t *isp, isp_pnhle_24xx_t *src, isp_pnhle_24xx_t *dst)
{

	ISP_IOXGET_16(isp, &src->pnhle_port_id_lo, dst->pnhle_port_id_lo);
	ISP_IOXGET_16(isp, &src->pnhle_port_id_hi, dst->pnhle_port_id_hi);
	ISP_IOXGET_16(isp, &src->pnhle_handle, dst->pnhle_handle);
	ISP_IOXGET_16(isp, &src->pnhle_reserved, dst->pnhle_reserved);
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
		ISP_IOXGET_16(isp, &src->plogx_ioparm[i].lo16, dst->plogx_ioparm[i].lo16);
		ISP_IOXGET_16(isp, &src->plogx_ioparm[i].hi16, dst->plogx_ioparm[i].hi16);
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
		ISP_IOXPUT_16(isp, src->plogx_ioparm[i].lo16, &dst->plogx_ioparm[i].lo16);
		ISP_IOXPUT_16(isp, src->plogx_ioparm[i].hi16, &dst->plogx_ioparm[i].hi16);
	}
}

/*
 * Report ID canonicalization
 */
void
isp_get_ridacq(ispsoftc_t *isp, isp_ridacq_t *src, isp_ridacq_t *dst)
{
	int i;
	isp_get_hdr(isp, &src->ridacq_hdr, &dst->ridacq_hdr);
	ISP_IOXGET_32(isp, &src->ridacq_handle, dst->ridacq_handle);
	ISP_IOXGET_8(isp, &src->ridacq_vp_acquired, dst->ridacq_vp_acquired);
	ISP_IOXGET_8(isp, &src->ridacq_vp_setup, dst->ridacq_vp_setup);
	ISP_IOXGET_8(isp, &src->ridacq_vp_index, dst->ridacq_vp_index);
	ISP_IOXGET_8(isp, &src->ridacq_vp_status, dst->ridacq_vp_status);
	ISP_IOXGET_16(isp, &src->ridacq_vp_port_lo, dst->ridacq_vp_port_lo);
	ISP_IOXGET_8(isp, &src->ridacq_vp_port_hi, dst->ridacq_vp_port_hi);
	ISP_IOXGET_8(isp, &src->ridacq_format, dst->ridacq_format);
	for (i = 0; i < sizeof (src->ridacq_map) / sizeof (src->ridacq_map[0]); i++) {
		ISP_IOXGET_16(isp, &src->ridacq_map[i], dst->ridacq_map[i]);
	}
	for (i = 0; i < sizeof (src->ridacq_reserved1) / sizeof (src->ridacq_reserved1[0]); i++) {
		ISP_IOXGET_16(isp, &src->ridacq_reserved1[i], dst->ridacq_reserved1[i]);
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
	ISP_IOXGET_8(isp, &src->ctp_vpidx, dst->ctp_vpidx);
	ISP_IOXGET_8(isp, &src->ctp_reserved0, dst->ctp_reserved0);
	ISP_IOXGET_16(isp, &src->ctp_time, dst->ctp_time);
	ISP_IOXGET_16(isp, &src->ctp_reserved1, dst->ctp_reserved1);
	ISP_IOXGET_16(isp, &src->ctp_rsp_cnt, dst->ctp_rsp_cnt);
	for (i = 0; i < 5; i++) {
		ISP_IOXGET_16(isp, &src->ctp_reserved2[i], dst->ctp_reserved2[i]);
	}
	ISP_IOXGET_32(isp, &src->ctp_rsp_bcnt, dst->ctp_rsp_bcnt);
	ISP_IOXGET_32(isp, &src->ctp_cmd_bcnt, dst->ctp_cmd_bcnt);
	for (i = 0; i < 2; i++) {
		ISP_IOXGET_32(isp, &src->ctp_dataseg[i].ds_base, dst->ctp_dataseg[i].ds_base);
		ISP_IOXGET_32(isp, &src->ctp_dataseg[i].ds_basehi, dst->ctp_dataseg[i].ds_basehi);
		ISP_IOXGET_32(isp, &src->ctp_dataseg[i].ds_count, dst->ctp_dataseg[i].ds_count);
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
	ISP_IOXPUT_8(isp, src->ctp_vpidx, &dst->ctp_vpidx);
	ISP_IOXPUT_8(isp, src->ctp_reserved0, &dst->ctp_reserved0);
	ISP_IOXPUT_16(isp, src->ctp_time, &dst->ctp_time);
	ISP_IOXPUT_16(isp, src->ctp_reserved1, &dst->ctp_reserved1);
	ISP_IOXPUT_16(isp, src->ctp_rsp_cnt, &dst->ctp_rsp_cnt);
	for (i = 0; i < 5; i++) {
		ISP_IOXPUT_16(isp, src->ctp_reserved2[i], &dst->ctp_reserved2[i]);
	}
	ISP_IOXPUT_32(isp, src->ctp_rsp_bcnt, &dst->ctp_rsp_bcnt);
	ISP_IOXPUT_32(isp, src->ctp_cmd_bcnt, &dst->ctp_cmd_bcnt);
	for (i = 0; i < 2; i++) {
		ISP_IOXPUT_32(isp, src->ctp_dataseg[i].ds_base, &dst->ctp_dataseg[i].ds_base);
		ISP_IOXPUT_32(isp, src->ctp_dataseg[i].ds_basehi, &dst->ctp_dataseg[i].ds_basehi);
		ISP_IOXPUT_32(isp, src->ctp_dataseg[i].ds_count, &dst->ctp_dataseg[i].ds_count);
	}
}

void
isp_put_gid_ft_request(ispsoftc_t *isp, sns_gid_ft_req_t *src, sns_gid_ft_req_t *dst)
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
isp_get_gid_xx_response(ispsoftc_t *isp, sns_gid_xx_rsp_t *src, sns_gid_xx_rsp_t *dst, int nwords)
{
	int i, j;

	isp_get_ct_hdr(isp, &src->snscb_cthdr, &dst->snscb_cthdr);
	for (i = 0; i < nwords; i++) {
		ISP_IOZGET_8(isp, &src->snscb_ports[i].control,
		    dst->snscb_ports[i].control);
		for (j = 0; j < 3; j++) {
			ISP_IOZGET_8(isp, &src->snscb_ports[i].portid[j],
			    dst->snscb_ports[i].portid[j]);
		}
		if (dst->snscb_ports[i].control & 0x80)
			break;
	}
}

void
isp_get_gxn_id_response(ispsoftc_t *isp, sns_gxn_id_rsp_t *src, sns_gxn_id_rsp_t *dst)
{
	int i;

	isp_get_ct_hdr(isp, &src->snscb_cthdr, &dst->snscb_cthdr);
	for (i = 0; i < 8; i++)
		ISP_IOZGET_8(isp, &src->snscb_wwn[i], dst->snscb_wwn[i]);
}

void
isp_get_gft_id_response(ispsoftc_t *isp, sns_gft_id_rsp_t *src, sns_gft_id_rsp_t *dst)
{
	int i;

	isp_get_ct_hdr(isp, &src->snscb_cthdr, &dst->snscb_cthdr);
	for (i = 0; i < 8; i++) {
		ISP_IOZGET_32(isp, &src->snscb_fc4_types[i],
		    dst->snscb_fc4_types[i]);
	}
}

void
isp_get_gff_id_response(ispsoftc_t *isp, sns_gff_id_rsp_t *src, sns_gff_id_rsp_t *dst)
{
	int i;

	isp_get_ct_hdr(isp, &src->snscb_cthdr, &dst->snscb_cthdr);
	for (i = 0; i < 32; i++) {
		ISP_IOZGET_32(isp, &src->snscb_fc4_features[i],
		    dst->snscb_fc4_features[i]);
	}
}

void
isp_get_ga_nxt_response(ispsoftc_t *isp, sns_ga_nxt_rsp_t *src, sns_ga_nxt_rsp_t *dst)
{
	int i;
	isp_get_ct_hdr(isp, &src->snscb_cthdr, &dst->snscb_cthdr);
	ISP_IOZGET_8(isp, &src->snscb_port_type, dst->snscb_port_type);
	for (i = 0; i < 3; i++) {
		ISP_IOZGET_8(isp, &src->snscb_port_id[i], dst->snscb_port_id[i]);
	}
	for (i = 0; i < 8; i++) {
		ISP_IOZGET_8(isp, &src->snscb_portname[i], dst->snscb_portname[i]);
	}
	ISP_IOZGET_8(isp, &src->snscb_pnlen, dst->snscb_pnlen);
	for (i = 0; i < 255; i++) {
		ISP_IOZGET_8(isp, &src->snscb_pname[i], dst->snscb_pname[i]);
	}
	for (i = 0; i < 8; i++) {
		ISP_IOZGET_8(isp, &src->snscb_nodename[i], dst->snscb_nodename[i]);
	}
	ISP_IOZGET_8(isp, &src->snscb_nnlen, dst->snscb_nnlen);
	for (i = 0; i < 255; i++) {
		ISP_IOZGET_8(isp, &src->snscb_nname[i], dst->snscb_nname[i]);
	}
	for (i = 0; i < 8; i++) {
		ISP_IOZGET_8(isp, &src->snscb_ipassoc[i], dst->snscb_ipassoc[i]);
	}
	for (i = 0; i < 16; i++) {
		ISP_IOZGET_8(isp, &src->snscb_ipaddr[i], dst->snscb_ipaddr[i]);
	}
	for (i = 0; i < 4; i++) {
		ISP_IOZGET_8(isp, &src->snscb_svc_class[i], dst->snscb_svc_class[i]);
	}
	for (i = 0; i < 32; i++) {
		ISP_IOZGET_8(isp, &src->snscb_fc4_types[i], dst->snscb_fc4_types[i]);
	}
	for (i = 0; i < 8; i++) {
		ISP_IOZGET_8(isp, &src->snscb_fpname[i], dst->snscb_fpname[i]);
	}
	ISP_IOZGET_8(isp, &src->snscb_reserved, dst->snscb_reserved);
	for (i = 0; i < 3; i++) {
		ISP_IOZGET_8(isp, &src->snscb_hardaddr[i], dst->snscb_hardaddr[i]);
	}
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
        ISP_IOZGET_8(isp, &src->f_ctl[0], dst->f_ctl[0]);
        ISP_IOZGET_8(isp, &src->f_ctl[1], dst->f_ctl[1]);
        ISP_IOZGET_8(isp, &src->f_ctl[2], dst->f_ctl[2]);
        ISP_IOZGET_8(isp, &src->seq_id, dst->seq_id);
        ISP_IOZGET_8(isp, &src->df_ctl, dst->df_ctl);
        ISP_IOZGET_16(isp, &src->seq_cnt, dst->seq_cnt);
        ISP_IOZGET_16(isp, &src->ox_id, dst->ox_id);
        ISP_IOZGET_16(isp, &src->rx_id, dst->rx_id);
        ISP_IOZGET_32(isp, &src->parameter, dst->parameter);
}

void
isp_put_fc_hdr(ispsoftc_t *isp, fc_hdr_t *src, fc_hdr_t *dst)
{
        ISP_IOZPUT_8(isp, src->r_ctl, &dst->r_ctl);
        ISP_IOZPUT_8(isp, src->d_id[0], &dst->d_id[0]);
        ISP_IOZPUT_8(isp, src->d_id[1], &dst->d_id[1]);
        ISP_IOZPUT_8(isp, src->d_id[2], &dst->d_id[2]);
        ISP_IOZPUT_8(isp, src->cs_ctl, &dst->cs_ctl);
        ISP_IOZPUT_8(isp, src->s_id[0], &dst->s_id[0]);
        ISP_IOZPUT_8(isp, src->s_id[1], &dst->s_id[1]);
        ISP_IOZPUT_8(isp, src->s_id[2], &dst->s_id[2]);
        ISP_IOZPUT_8(isp, src->type, &dst->type);
        ISP_IOZPUT_8(isp, src->f_ctl[0], &dst->f_ctl[0]);
        ISP_IOZPUT_8(isp, src->f_ctl[1], &dst->f_ctl[1]);
        ISP_IOZPUT_8(isp, src->f_ctl[2], &dst->f_ctl[2]);
        ISP_IOZPUT_8(isp, src->seq_id, &dst->seq_id);
        ISP_IOZPUT_8(isp, src->df_ctl, &dst->df_ctl);
        ISP_IOZPUT_16(isp, src->seq_cnt, &dst->seq_cnt);
        ISP_IOZPUT_16(isp, src->ox_id, &dst->ox_id);
        ISP_IOZPUT_16(isp, src->rx_id, &dst->rx_id);
        ISP_IOZPUT_32(isp, src->parameter, &dst->parameter);
}

void
isp_get_fcp_cmnd_iu(ispsoftc_t *isp, fcp_cmnd_iu_t *src, fcp_cmnd_iu_t *dst)
{
	int i;

	for (i = 0; i < 8; i++) {
		ISP_IOZGET_8(isp, &src->fcp_cmnd_lun[i], dst->fcp_cmnd_lun[i]);
	}
        ISP_IOZGET_8(isp, &src->fcp_cmnd_crn, dst->fcp_cmnd_crn);
        ISP_IOZGET_8(isp, &src->fcp_cmnd_task_attribute, dst->fcp_cmnd_task_attribute);
        ISP_IOZGET_8(isp, &src->fcp_cmnd_task_management, dst->fcp_cmnd_task_management);
        ISP_IOZGET_8(isp, &src->fcp_cmnd_alen_datadir, dst->fcp_cmnd_alen_datadir);
	for (i = 0; i < 16; i++) {
		ISP_IOZGET_8(isp, &src->cdb_dl.sf.fcp_cmnd_cdb[i], dst->cdb_dl.sf.fcp_cmnd_cdb[i]);
	}
	ISP_IOZGET_32(isp, &src->cdb_dl.sf.fcp_cmnd_dl, dst->cdb_dl.sf.fcp_cmnd_dl);
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
		ISP_IOZPUT_32(isp, src->rftid_fc4types[i], &dst->rftid_fc4types[i]);
	}
}

void
isp_put_rspn_id(ispsoftc_t *isp, rspn_id_t *src, rspn_id_t *dst)
{
/*	int i;*/
	isp_put_ct_hdr(isp, &src->rspnid_hdr, &dst->rspnid_hdr);
	ISP_IOZPUT_8(isp, src->rspnid_reserved, &dst->rspnid_reserved);
	ISP_IOZPUT_8(isp, src->rspnid_length, &dst->rspnid_length);
/*	for (i = 0; i < src->rspnid_length; i++)
		ISP_IOZPUT_8(isp, src->rspnid_name[i], &dst->rspnid_name[i]);*/
}

void
isp_put_rff_id(ispsoftc_t *isp, rff_id_t *src, rff_id_t *dst)
{
	int i;

	isp_put_ct_hdr(isp, &src->rffid_hdr, &dst->rffid_hdr);
	ISP_IOZPUT_8(isp, src->rffid_reserved, &dst->rffid_reserved);
	for (i = 0; i < 3; i++)
		ISP_IOZPUT_8(isp, src->rffid_portid[i], &dst->rffid_portid[i]);
	ISP_IOZPUT_16(isp, src->rffid_reserved2, &dst->rffid_reserved2);
	ISP_IOZPUT_8(isp, src->rffid_fc4features, &dst->rffid_fc4features);
	ISP_IOZPUT_8(isp, src->rffid_fc4type, &dst->rffid_fc4type);
}

void
isp_put_rsnn_nn(ispsoftc_t *isp, rsnn_nn_t *src, rsnn_nn_t *dst)
{
	int i;
	isp_put_ct_hdr(isp, &src->rsnnnn_hdr, &dst->rsnnnn_hdr);
	for (i = 0; i < 8; i++)
		ISP_IOZPUT_8(isp, src->rsnnnn_nodename[i], &dst->rsnnnn_nodename[i]);
	ISP_IOZPUT_8(isp, src->rsnnnn_length, &dst->rsnnnn_length);
/*	for (i = 0; i < src->rsnnnn_length; i++)
		ISP_IOZPUT_8(isp, src->rsnnnn_name[i], &dst->rsnnnn_name[i]);*/
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

void
isp_put_fcp_rsp_iu(ispsoftc_t *isp, fcp_rsp_iu_t *src, fcp_rsp_iu_t *dst)
{
	int i;
	for (i = 0; i < ((sizeof (src->fcp_rsp_reserved))/(sizeof (src->fcp_rsp_reserved[0]))); i++) {
		ISP_IOZPUT_8(isp, src->fcp_rsp_reserved[i], &dst->fcp_rsp_reserved[i]);
	}
	ISP_IOZPUT_16(isp, src->fcp_rsp_status_qualifier, &dst->fcp_rsp_status_qualifier);
	ISP_IOZPUT_8(isp, src->fcp_rsp_bits, &dst->fcp_rsp_bits);
	ISP_IOZPUT_8(isp, src->fcp_rsp_scsi_status, &dst->fcp_rsp_scsi_status);
	ISP_IOZPUT_32(isp, src->fcp_rsp_resid, &dst->fcp_rsp_resid);
	ISP_IOZPUT_32(isp, src->fcp_rsp_snslen, &dst->fcp_rsp_snslen);
	ISP_IOZPUT_32(isp, src->fcp_rsp_rsplen, &dst->fcp_rsp_rsplen);
}

/*
 * Find port database entries
 */
int
isp_find_pdb_empty(ispsoftc_t *isp, int chan, fcportdb_t **lptr)
{
	fcparam *fcp = FCPARAM(isp, chan);
	int i;

	for (i = 0; i < MAX_FC_TARG; i++) {
		fcportdb_t *lp = &fcp->portdb[i];

		if (lp->state == FC_PORTDB_STATE_NIL) {
			*lptr = lp;
			return (1);
		}
	}
	return (0);
}

int
isp_find_pdb_by_wwpn(ispsoftc_t *isp, int chan, uint64_t wwpn, fcportdb_t **lptr)
{
	fcparam *fcp = FCPARAM(isp, chan);
	int i;

	for (i = 0; i < MAX_FC_TARG; i++) {
		fcportdb_t *lp = &fcp->portdb[i];

		if (lp->state == FC_PORTDB_STATE_NIL)
			continue;
		if (lp->port_wwn == wwpn) {
			*lptr = lp;
			return (1);
		}
	}
	return (0);
}

int
isp_find_pdb_by_handle(ispsoftc_t *isp, int chan, uint16_t handle,
    fcportdb_t **lptr)
{
	fcparam *fcp = FCPARAM(isp, chan);
	int i;

	for (i = 0; i < MAX_FC_TARG; i++) {
		fcportdb_t *lp = &fcp->portdb[i];

		if (lp->state == FC_PORTDB_STATE_NIL)
			continue;
		if (lp->handle == handle) {
			*lptr = lp;
			return (1);
		}
	}
	return (0);
}

int
isp_find_pdb_by_portid(ispsoftc_t *isp, int chan, uint32_t portid,
    fcportdb_t **lptr)
{
	fcparam *fcp = FCPARAM(isp, chan);
	int i;

	for (i = 0; i < MAX_FC_TARG; i++) {
		fcportdb_t *lp = &fcp->portdb[i];

		if (lp->state == FC_PORTDB_STATE_NIL)
			continue;
		if (lp->portid == portid) {
			*lptr = lp;
			return (1);
		}
	}
	return (0);
}

#ifdef	ISP_TARGET_MODE
void
isp_find_chan_by_did(ispsoftc_t *isp, uint32_t did, uint16_t *cp)
{
	uint16_t chan;

	*cp = ISP_NOCHAN;
	for (chan = 0; chan < isp->isp_nchan; chan++) {
		fcparam *fcp = FCPARAM(isp, chan);
		if ((fcp->role & ISP_ROLE_TARGET) == 0 ||
		    fcp->isp_loopstate < LOOP_HAVE_ADDR) {
			continue;
		}
		if (fcp->isp_portid == did) {
			*cp = chan;
			break;
		}
	}
}

/*
 * Add an initiator device to the port database
 */
void
isp_add_wwn_entry(ispsoftc_t *isp, int chan, uint64_t wwpn, uint64_t wwnn,
    uint16_t nphdl, uint32_t s_id, uint16_t prli_params)
{
	char buf[64];
	fcparam *fcp;
	fcportdb_t *lp;
	int i, change;

	fcp = FCPARAM(isp, chan);
	if (nphdl >= MAX_NPORT_HANDLE) {
		isp_prt(isp, ISP_LOGTINFO|ISP_LOGWARN, "Chan %d WWPN 0x%016llx "
		    "PortID 0x%06x handle 0x%x -- bad handle",
		    chan, (unsigned long long) wwpn, s_id, nphdl);
		return;
	}

	/*
	 * If valid record for requested handle already exists, update it
	 * with new parameters.  Some cases of update can be suspicious,
	 * so log them verbosely and dump the whole port database.
	 */
	if ((VALID_INI(wwpn) && isp_find_pdb_by_wwpn(isp, chan, wwpn, &lp)) ||
	    (VALID_PORT(s_id) && isp_find_pdb_by_portid(isp, chan, s_id, &lp))) {
		change = 0;
		lp->new_portid = lp->portid;
		lp->new_prli_word3 = lp->prli_word3;
		if (VALID_PORT(s_id) && lp->portid != s_id) {
			if (!VALID_PORT(lp->portid)) {
				isp_prt(isp, ISP_LOGTINFO,
				    "Chan %d WWPN 0x%016llx handle 0x%x "
				    "gets PortID 0x%06x",
				    chan, (unsigned long long) lp->port_wwn,
				    nphdl, s_id);
			} else {
				isp_prt(isp, ISP_LOGTINFO|ISP_LOGWARN,
				    "Chan %d WWPN 0x%016llx handle 0x%x "
				    "changes PortID 0x%06x to 0x%06x",
				    chan, (unsigned long long) lp->port_wwn,
				    nphdl, lp->portid, s_id);
				if (isp->isp_dblev & (ISP_LOGTINFO|ISP_LOGWARN))
					isp_dump_portdb(isp, chan);
			}
			lp->new_portid = s_id;
			change++;
		}
		if (VALID_INI(wwpn) && lp->port_wwn != wwpn) {
			if (!VALID_INI(lp->port_wwn)) {
				isp_prt(isp, ISP_LOGTINFO,
				    "Chan %d PortID 0x%06x handle 0x%x "
				    "gets WWPN 0x%016llxx",
				    chan, lp->portid, nphdl,
				    (unsigned long long) wwpn);
			} else if (lp->port_wwn != wwpn) {
				isp_prt(isp, ISP_LOGTINFO|ISP_LOGWARN,
				    "Chan %d PortID 0x%06x handle 0x%x "
				    "changes WWPN 0x%016llx to 0x%016llx",
				    chan, lp->portid, nphdl,
				    (unsigned long long) lp->port_wwn,
				    (unsigned long long) wwpn);
				if (isp->isp_dblev & (ISP_LOGTINFO|ISP_LOGWARN))
					isp_dump_portdb(isp, chan);
			}
			lp->port_wwn = wwpn;
			change++;
		}
		if (VALID_INI(wwnn) && lp->node_wwn != wwnn) {
			if (!VALID_INI(lp->node_wwn)) {
				isp_prt(isp, ISP_LOGTINFO,
				    "Chan %d PortID 0x%06x handle 0x%x "
				    "gets WWNN 0x%016llxx",
				    chan, lp->portid, nphdl,
				    (unsigned long long) wwnn);
			} else if (lp->port_wwn != wwnn) {
				isp_prt(isp, ISP_LOGTINFO,
				    "Chan %d PortID 0x%06x handle 0x%x "
				    "changes WWNN 0x%016llx to 0x%016llx",
				    chan, lp->portid, nphdl,
				    (unsigned long long) lp->node_wwn,
				    (unsigned long long) wwnn);
			}
			lp->node_wwn = wwnn;
			change++;
		}
		if (prli_params != 0 && lp->prli_word3 != prli_params) {
			isp_gen_role_str(buf, sizeof (buf), prli_params);
			isp_prt(isp, ISP_LOGTINFO|ISP_LOGCONFIG,
			    "Chan %d WWPN 0x%016llx PortID 0x%06x "
			    "handle 0x%x changes PRLI Word 3 %s",
			    chan, (unsigned long long) lp->port_wwn,
			    lp->portid, lp->handle, buf);
			lp->new_prli_word3 = prli_params;
			change++;
		}
		if (lp->handle != nphdl) {
			isp_prt(isp, ISP_LOGTINFO|ISP_LOGCONFIG,
			    "Chan %d WWPN 0x%016llx PortID 0x%06x "
			    "changes handle 0x%x to 0x%x",
			    chan, (unsigned long long) lp->port_wwn,
			    lp->portid, lp->handle, nphdl);
			lp->handle = nphdl;
			change++;
		}
		lp->state = FC_PORTDB_STATE_VALID;
		if (change) {
			isp_async(isp, ISPASYNC_DEV_CHANGED, chan, lp);
			lp->portid = lp->new_portid;
			lp->prli_word3 = lp->new_prli_word3;
		} else {
			isp_prt(isp, ISP_LOGTINFO,
			    "Chan %d WWPN 0x%016llx PortID 0x%06x "
			    "handle 0x%x reentered",
			    chan, (unsigned long long) lp->port_wwn,
			    lp->portid, lp->handle);
			isp_async(isp, ISPASYNC_DEV_STAYED, chan, lp);
		}
		return;
	}

	/* Search for room to insert new record. */
	for (i = 0; i < MAX_FC_TARG; i++) {
		if (fcp->portdb[i].state == FC_PORTDB_STATE_NIL)
			break;
	}
	if (i >= MAX_FC_TARG) {
		isp_prt(isp, ISP_LOGTINFO|ISP_LOGWARN,
		    "Chan %d WWPN 0x%016llx PortID 0x%06x handle 0x%x "
		    "-- no room in port database",
		    chan, (unsigned long long) wwpn, s_id, nphdl);
		if (isp->isp_dblev & (ISP_LOGTINFO|ISP_LOGWARN))
			isp_dump_portdb(isp, chan);
		return;
	}

	/* Insert new record and mark it valid. */
	lp = &fcp->portdb[i];
	ISP_MEMZERO(lp, sizeof (fcportdb_t));
	lp->handle = nphdl;
	lp->portid = s_id;
	lp->port_wwn = wwpn;
	lp->node_wwn = wwnn;
	lp->prli_word3 = (prli_params != 0) ? prli_params : PRLI_WD3_INITIATOR_FUNCTION;
	lp->state = FC_PORTDB_STATE_VALID;

	isp_gen_role_str(buf, sizeof (buf), lp->prli_word3);
	isp_prt(isp, ISP_LOGTINFO, "Chan %d WWPN 0x%016llx "
	    "PortID 0x%06x handle 0x%x vtgt %d %s added", chan,
	    (unsigned long long) wwpn, s_id, nphdl, i, buf);

	/* Notify above levels about new port arrival. */
	isp_async(isp, ISPASYNC_DEV_ARRIVED, chan, lp);
}

/*
 * Remove a target device to the port database
 */
void
isp_del_wwn_entry(ispsoftc_t *isp, int chan, uint64_t wwpn, uint16_t nphdl, uint32_t s_id)
{
	fcportdb_t *lp;

	if (nphdl >= MAX_NPORT_HANDLE) {
		isp_prt(isp, ISP_LOGWARN, "Chan %d WWPN 0x%016llx PortID 0x%06x bad handle 0x%x",
		    chan, (unsigned long long) wwpn, s_id, nphdl);
		return;
	}

	if (isp_find_pdb_by_handle(isp, chan, nphdl, &lp) == 0) {
		isp_prt(isp, ISP_LOGWARN, "Chan %d WWPN 0x%016llx PortID 0x%06x handle 0x%x cannot be found to be deleted",
		    chan, (unsigned long long) wwpn, s_id, nphdl);
		isp_dump_portdb(isp, chan);
		return;
	}
	isp_prt(isp, ISP_LOGTINFO, "Chan %d WWPN 0x%016llx PortID 0x%06x handle 0x%x vtgt %d deleted",
	    chan, (unsigned long long) lp->port_wwn, lp->portid, nphdl, FC_PORTDB_TGT(isp, chan, lp));
	lp->state = FC_PORTDB_STATE_NIL;

	/* Notify above levels about gone port. */
	isp_async(isp, ISPASYNC_DEV_GONE, chan, lp);
}
#endif	/* ISP_TARGET_MODE */

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
isp_put_ctio7(ispsoftc_t *isp, ct7_entry_t *src, ct7_entry_t *dst)
{
	int i;

	isp_put_hdr(isp, &src->ct_header, &dst->ct_header);
	ISP_IOXPUT_32(isp, src->ct_syshandle, &dst->ct_syshandle);
	ISP_IOXPUT_16(isp, src->ct_nphdl, &dst->ct_nphdl);
	ISP_IOXPUT_16(isp, src->ct_timeout, &dst->ct_timeout);
	ISP_IOXPUT_16(isp, src->ct_seg_count, &dst->ct_seg_count);
	ISP_IOXPUT_8(isp, src->ct_vpidx, &dst->ct_vpidx);
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
		ISP_IOXPUT_32(isp, src->rsp.m0.reserved0, &dst->rsp.m0.reserved0);
		ISP_IOXPUT_32(isp, src->rsp.m0.ct_xfrlen, &dst->rsp.m0.ct_xfrlen);
		ISP_IOXPUT_32(isp, src->rsp.m0.reserved1, &dst->rsp.m0.reserved1);
		ISP_IOXPUT_32(isp, src->rsp.m0.ds.ds_base, &dst->rsp.m0.ds.ds_base);
		ISP_IOXPUT_32(isp, src->rsp.m0.ds.ds_basehi, &dst->rsp.m0.ds.ds_basehi);
		ISP_IOXPUT_32(isp, src->rsp.m0.ds.ds_count, &dst->rsp.m0.ds.ds_count);
	} else if ((dst->ct_flags & CT7_FLAG_MMASK) == CT7_FLAG_MODE1) {
		uint32_t *a, *b;

		ISP_IOXPUT_16(isp, src->rsp.m1.ct_resplen, &dst->rsp.m1.ct_resplen);
		ISP_IOXPUT_16(isp, src->rsp.m1.reserved, &dst->rsp.m1.reserved);
		a = (uint32_t *) src->rsp.m1.ct_resp;
		b = (uint32_t *) dst->rsp.m1.ct_resp;
		for (i = 0; i < (nitems(src->rsp.m1.ct_resp) >> 2); i++) {
			*b++ = ISP_SWAP32(isp, *a++);
		}
	} else {
		ISP_IOXPUT_32(isp, src->rsp.m2.reserved0, &dst->rsp.m2.reserved0);
		ISP_IOXPUT_32(isp, src->rsp.m2.reserved1, &dst->rsp.m2.reserved1);
		ISP_IOXPUT_32(isp, src->rsp.m2.ct_datalen, &dst->rsp.m2.ct_datalen);
		ISP_IOXPUT_32(isp, src->rsp.m2.reserved2, &dst->rsp.m2.reserved2);
		ISP_IOXPUT_32(isp, src->rsp.m2.ct_fcp_rsp_iudata.ds_base, &dst->rsp.m2.ct_fcp_rsp_iudata.ds_base);
		ISP_IOXPUT_32(isp, src->rsp.m2.ct_fcp_rsp_iudata.ds_basehi, &dst->rsp.m2.ct_fcp_rsp_iudata.ds_basehi);
		ISP_IOXPUT_32(isp, src->rsp.m2.ct_fcp_rsp_iudata.ds_count, &dst->rsp.m2.ct_fcp_rsp_iudata.ds_count);
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
	ISP_IOXGET_8(isp, &src->ct_vpidx, dst->ct_vpidx);
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
		ISP_IOXGET_32(isp, &src->rsp.m0.reserved0, dst->rsp.m0.reserved0);
		ISP_IOXGET_32(isp, &src->rsp.m0.ct_xfrlen, dst->rsp.m0.ct_xfrlen);
		ISP_IOXGET_32(isp, &src->rsp.m0.reserved1, dst->rsp.m0.reserved1);
		ISP_IOXGET_32(isp, &src->rsp.m0.ds.ds_base, dst->rsp.m0.ds.ds_base);
		ISP_IOXGET_32(isp, &src->rsp.m0.ds.ds_basehi, dst->rsp.m0.ds.ds_basehi);
		ISP_IOXGET_32(isp, &src->rsp.m0.ds.ds_count, dst->rsp.m0.ds.ds_count);
	} else if ((dst->ct_flags & CT7_FLAG_MMASK) == CT7_FLAG_MODE1) {
		uint32_t *a, *b;

		ISP_IOXGET_16(isp, &src->rsp.m1.ct_resplen, dst->rsp.m1.ct_resplen);
		ISP_IOXGET_16(isp, &src->rsp.m1.reserved, dst->rsp.m1.reserved);
		a = (uint32_t *) src->rsp.m1.ct_resp;
		b = (uint32_t *) dst->rsp.m1.ct_resp;
		for (i = 0; i < MAXRESPLEN_24XX; i++) {
			ISP_IOXGET_8(isp, &src->rsp.m1.ct_resp[i], dst->rsp.m1.ct_resp[i]);
		}
		for (i = 0; i < (nitems(src->rsp.m1.ct_resp) >> 2); i++) {
			*b++ = ISP_SWAP32(isp, *a++);
		}
	} else {
		ISP_IOXGET_32(isp, &src->rsp.m2.reserved0, dst->rsp.m2.reserved0);
		ISP_IOXGET_32(isp, &src->rsp.m2.ct_datalen, dst->rsp.m2.ct_datalen);
		ISP_IOXGET_32(isp, &src->rsp.m2.reserved1, dst->rsp.m2.reserved1);
		ISP_IOXGET_32(isp, &src->rsp.m2.ct_fcp_rsp_iudata.ds_base, dst->rsp.m2.ct_fcp_rsp_iudata.ds_base);
		ISP_IOXGET_32(isp, &src->rsp.m2.ct_fcp_rsp_iudata.ds_basehi, dst->rsp.m2.ct_fcp_rsp_iudata.ds_basehi);
		ISP_IOXGET_32(isp, &src->rsp.m2.ct_fcp_rsp_iudata.ds_count, dst->rsp.m2.ct_fcp_rsp_iudata.ds_count);
	}
}

void
isp_put_notify_24xx(ispsoftc_t *isp, in_fcentry_24xx_t *src, in_fcentry_24xx_t *dst)
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
	ISP_IOXPUT_8(isp, src->in_fwhandle, &dst->in_fwhandle);
	ISP_IOXPUT_32(isp, src->in_rxid, &dst->in_rxid);
	ISP_IOXPUT_16(isp, src->in_srr_reloff_hi, &dst->in_srr_reloff_hi);
	ISP_IOXPUT_16(isp, src->in_srr_reloff_lo, &dst->in_srr_reloff_lo);
	ISP_IOXPUT_16(isp, src->in_srr_iu, &dst->in_srr_iu);
	ISP_IOXPUT_16(isp, src->in_srr_oxid, &dst->in_srr_oxid);
	ISP_IOXPUT_16(isp, src->in_nport_id_hi, &dst->in_nport_id_hi);
	ISP_IOXPUT_8(isp, src->in_nport_id_lo, &dst->in_nport_id_lo);
	ISP_IOXPUT_8(isp, src->in_reserved3, &dst->in_reserved3);
	ISP_IOXPUT_16(isp, src->in_np_handle, &dst->in_np_handle);
	for (i = 0; i < nitems(src->in_reserved4); i++) {
		ISP_IOXPUT_8(isp, src->in_reserved4[i], &dst->in_reserved4[i]);
	}
	ISP_IOXPUT_8(isp, src->in_reserved5, &dst->in_reserved5);
	ISP_IOXPUT_8(isp, src->in_vpidx, &dst->in_vpidx);
	ISP_IOXPUT_32(isp, src->in_reserved6, &dst->in_reserved6);
	ISP_IOXPUT_16(isp, src->in_portid_lo, &dst->in_portid_lo);
	ISP_IOXPUT_8(isp, src->in_portid_hi, &dst->in_portid_hi);
	ISP_IOXPUT_8(isp, src->in_reserved7, &dst->in_reserved7);
	ISP_IOXPUT_16(isp, src->in_reserved8, &dst->in_reserved8);
	ISP_IOXPUT_16(isp, src->in_oxid, &dst->in_oxid);
}

void
isp_get_notify_24xx(ispsoftc_t *isp, in_fcentry_24xx_t *src, in_fcentry_24xx_t *dst)
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
	ISP_IOXGET_8(isp, &src->in_fwhandle, dst->in_fwhandle);
	ISP_IOXGET_32(isp, &src->in_rxid, dst->in_rxid);
	ISP_IOXGET_16(isp, &src->in_srr_reloff_hi, dst->in_srr_reloff_hi);
	ISP_IOXGET_16(isp, &src->in_srr_reloff_lo, dst->in_srr_reloff_lo);
	ISP_IOXGET_16(isp, &src->in_srr_iu, dst->in_srr_iu);
	ISP_IOXGET_16(isp, &src->in_srr_oxid, dst->in_srr_oxid);
	ISP_IOXGET_16(isp, &src->in_nport_id_hi, dst->in_nport_id_hi);
	ISP_IOXGET_8(isp, &src->in_nport_id_lo, dst->in_nport_id_lo);
	ISP_IOXGET_8(isp, &src->in_reserved3, dst->in_reserved3);
	ISP_IOXGET_16(isp, &src->in_np_handle, dst->in_np_handle);
	for (i = 0; i < nitems(src->in_reserved4); i++) {
		ISP_IOXGET_8(isp, &src->in_reserved4[i], dst->in_reserved4[i]);
	}
	ISP_IOXGET_8(isp, &src->in_reserved5, dst->in_reserved5);
	ISP_IOXGET_8(isp, &src->in_vpidx, dst->in_vpidx);
	ISP_IOXGET_32(isp, &src->in_reserved6, dst->in_reserved6);
	ISP_IOXGET_16(isp, &src->in_portid_lo, dst->in_portid_lo);
	ISP_IOXGET_8(isp, &src->in_portid_hi, dst->in_portid_hi);
	ISP_IOXGET_8(isp, &src->in_reserved7, dst->in_reserved7);
	ISP_IOXGET_16(isp, &src->in_reserved8, dst->in_reserved8);
	ISP_IOXGET_16(isp, &src->in_oxid, dst->in_oxid);
}

void
isp_put_notify_ack_24xx(ispsoftc_t *isp, na_fcentry_24xx_t *src, na_fcentry_24xx_t *dst)
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
	ISP_IOXPUT_8(isp, src->na_fwhandle, &dst->na_fwhandle);
	ISP_IOXPUT_32(isp, src->na_rxid, &dst->na_rxid);
	ISP_IOXPUT_16(isp, src->na_srr_reloff_hi, &dst->na_srr_reloff_hi);
	ISP_IOXPUT_16(isp, src->na_srr_reloff_lo, &dst->na_srr_reloff_lo);
	ISP_IOXPUT_16(isp, src->na_srr_iu, &dst->na_srr_iu);
	ISP_IOXPUT_16(isp, src->na_srr_flags, &dst->na_srr_flags);
	for (i = 0; i < 18; i++) {
		ISP_IOXPUT_8(isp, src->na_reserved3[i], &dst->na_reserved3[i]);
	}
	ISP_IOXPUT_8(isp, src->na_reserved4, &dst->na_reserved4);
	ISP_IOXPUT_8(isp, src->na_vpidx, &dst->na_vpidx);
	ISP_IOXPUT_8(isp, src->na_srr_reject_vunique, &dst->na_srr_reject_vunique);
	ISP_IOXPUT_8(isp, src->na_srr_reject_explanation, &dst->na_srr_reject_explanation);
	ISP_IOXPUT_8(isp, src->na_srr_reject_code, &dst->na_srr_reject_code);
	ISP_IOXPUT_8(isp, src->na_reserved5, &dst->na_reserved5);
	for (i = 0; i < 6; i++) {
		ISP_IOXPUT_8(isp, src->na_reserved6[i], &dst->na_reserved6[i]);
	}
	ISP_IOXPUT_16(isp, src->na_oxid, &dst->na_oxid);
}

void
isp_get_notify_ack_24xx(ispsoftc_t *isp, na_fcentry_24xx_t *src, na_fcentry_24xx_t *dst)
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
	ISP_IOXGET_8(isp, &src->na_fwhandle, dst->na_fwhandle);
	ISP_IOXGET_32(isp, &src->na_rxid, dst->na_rxid);
	ISP_IOXGET_16(isp, &src->na_srr_reloff_hi, dst->na_srr_reloff_hi);
	ISP_IOXGET_16(isp, &src->na_srr_reloff_lo, dst->na_srr_reloff_lo);
	ISP_IOXGET_16(isp, &src->na_srr_iu, dst->na_srr_iu);
	ISP_IOXGET_16(isp, &src->na_srr_flags, dst->na_srr_flags);
	for (i = 0; i < 18; i++) {
		ISP_IOXGET_8(isp, &src->na_reserved3[i], dst->na_reserved3[i]);
	}
	ISP_IOXGET_8(isp, &src->na_reserved4, dst->na_reserved4);
	ISP_IOXGET_8(isp, &src->na_vpidx, dst->na_vpidx);
	ISP_IOXGET_8(isp, &src->na_srr_reject_vunique, dst->na_srr_reject_vunique);
	ISP_IOXGET_8(isp, &src->na_srr_reject_explanation, dst->na_srr_reject_explanation);
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
		ISP_IOXGET_8(isp, &src->abts_reserved0[i], dst->abts_reserved0[i]);
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
		ISP_IOXGET_8(isp, &src->abts_reserved2[i], dst->abts_reserved2[i]);
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
		ISP_IOXPUT_16(isp, src->abts_rsp_payload.ba_acc.reserved, &dst->abts_rsp_payload.ba_acc.reserved);
		ISP_IOXPUT_8(isp, src->abts_rsp_payload.ba_acc.last_seq_id, &dst->abts_rsp_payload.ba_acc.last_seq_id);
		ISP_IOXPUT_8(isp, src->abts_rsp_payload.ba_acc.seq_id_valid, &dst->abts_rsp_payload.ba_acc.seq_id_valid);
		ISP_IOXPUT_16(isp, src->abts_rsp_payload.ba_acc.aborted_rx_id, &dst->abts_rsp_payload.ba_acc.aborted_rx_id);
		ISP_IOXPUT_16(isp, src->abts_rsp_payload.ba_acc.aborted_ox_id, &dst->abts_rsp_payload.ba_acc.aborted_ox_id);
		ISP_IOXPUT_16(isp, src->abts_rsp_payload.ba_acc.high_seq_cnt, &dst->abts_rsp_payload.ba_acc.high_seq_cnt);
		ISP_IOXPUT_16(isp, src->abts_rsp_payload.ba_acc.low_seq_cnt, &dst->abts_rsp_payload.ba_acc.low_seq_cnt);
		for (i = 0; i < 4; i++) {
			ISP_IOXPUT_16(isp, src->abts_rsp_payload.ba_acc.reserved2[i], &dst->abts_rsp_payload.ba_acc.reserved2[i]);
		}
	} else if (src->abts_rsp_r_ctl == BA_RJT) {
		ISP_IOXPUT_8(isp, src->abts_rsp_payload.ba_rjt.vendor_unique, &dst->abts_rsp_payload.ba_rjt.vendor_unique);
		ISP_IOXPUT_8(isp, src->abts_rsp_payload.ba_rjt.explanation, &dst->abts_rsp_payload.ba_rjt.explanation);
		ISP_IOXPUT_8(isp, src->abts_rsp_payload.ba_rjt.reason, &dst->abts_rsp_payload.ba_rjt.reason);
		ISP_IOXPUT_8(isp, src->abts_rsp_payload.ba_rjt.reserved, &dst->abts_rsp_payload.ba_rjt.reserved);
		for (i = 0; i < 12; i++) {
			ISP_IOXPUT_16(isp, src->abts_rsp_payload.ba_rjt.reserved2[i], &dst->abts_rsp_payload.ba_rjt.reserved2[i]);
		}
	} else {
		for (i = 0; i < 16; i++) {
			ISP_IOXPUT_8(isp, src->abts_rsp_payload.reserved[i], &dst->abts_rsp_payload.reserved[i]);
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
		ISP_IOXGET_8(isp, &src->abts_rsp_payload.rsp.reserved[i], dst->abts_rsp_payload.rsp.reserved[i]);
	}
	ISP_IOXGET_32(isp, &src->abts_rsp_payload.rsp.subcode1, dst->abts_rsp_payload.rsp.subcode1);
	ISP_IOXGET_32(isp, &src->abts_rsp_payload.rsp.subcode2, dst->abts_rsp_payload.rsp.subcode2);
	ISP_IOXGET_32(isp, &src->abts_rsp_rxid_task, dst->abts_rsp_rxid_task);
}

void
isp_put_entry(ispsoftc_t *isp, void *src, void *dst)
{
	uint8_t etype = ((isphdr_t *)src)->rqs_entry_type;

	switch (etype) {
	case RQSTYPE_NOTIFY_ACK:
		isp_put_notify_ack_24xx(isp, (na_fcentry_24xx_t *)src,
		    (na_fcentry_24xx_t *)dst);
		break;
	case RQSTYPE_CTIO7:
		isp_put_ctio7(isp, (ct7_entry_t *)src, (ct7_entry_t *)dst);
		break;
	case RQSTYPE_TSK_MGMT:
		isp_put_24xx_tmf(isp, (isp24xx_tmf_t *)src, (isp24xx_tmf_t *)dst);
		break;
	case RQSTYPE_CT_PASSTHRU:
		isp_put_ct_pt(isp, (isp_ct_pt_t *)src, (isp_ct_pt_t *)dst);
		break;
	case RQSTYPE_VP_CTRL:
		isp_put_vp_ctrl_info(isp, (vp_ctrl_info_t *)src, (vp_ctrl_info_t *)dst);
		break;
	case RQSTYPE_VP_MODIFY:
		isp_put_vp_modify(isp, (vp_modify_t *)src, (vp_modify_t *)dst);
		break;
	case RQSTYPE_ABORT_IO:
		isp_put_24xx_abrt(isp, (isp24xx_abrt_t *)src, (isp24xx_abrt_t *)dst);
		break;
	case RQSTYPE_LOGIN:
		isp_put_plogx(isp, (isp_plogx_t *)src, (isp_plogx_t *)dst);
		break;
	case RQSTYPE_ABTS_RSP:
		isp_put_abts_rsp(isp, (abts_rsp_t *)src, (abts_rsp_t *)dst);
		break;
	default:
		panic("%s: Unknown type 0x%x\n", __func__, etype);
	}
}

void
isp_get_entry(ispsoftc_t *isp, void *src, void *dst)
{
	uint8_t etype;

	isp_get_hdr(isp, src, (isphdr_t *)dst);
	etype = ((isphdr_t *)dst)->rqs_entry_type;
	switch (etype) {
	case 0:	/* After isp_clear_commands() */
		ISP_MEMZERO(dst, QENTRY_LEN);
		break;
	case RQSTYPE_RESPONSE:
		isp_get_24xx_response(isp, (isp24xx_statusreq_t *)src, (isp24xx_statusreq_t *)dst);
		break;
	case RQSTYPE_CT_PASSTHRU:
		isp_get_ct_pt(isp, (isp_ct_pt_t *)src, (isp_ct_pt_t *)dst);
		break;
	case RQSTYPE_VP_CTRL:
		isp_get_vp_ctrl_info(isp, (vp_ctrl_info_t *)src, (vp_ctrl_info_t *)dst);
		break;
	case RQSTYPE_VP_MODIFY:
		isp_get_vp_modify(isp, (vp_modify_t *)src, (vp_modify_t *)dst);
		break;
	case RQSTYPE_ABORT_IO:
		isp_get_24xx_abrt(isp, (isp24xx_abrt_t *)src, (isp24xx_abrt_t *)dst);
		break;
	case RQSTYPE_LOGIN:
		isp_get_plogx(isp, (isp_plogx_t *)src, (isp_plogx_t *)dst);
		break;
	default:
		panic("%s: Unknown type 0x%x\n", __func__, etype);
	}
}

int
isp_send_entry(ispsoftc_t *isp, void *src)
{
	void *outp;

	outp = isp_getrqentry(isp);
	if (outp == NULL) {
		isp_prt(isp, ISP_LOGWARN, "%s: Request Queue Overflow", __func__);
		return (-1);
	}
	isp_put_entry(isp, src, outp);
	if (isp->isp_dblev & ISP_LOGTDEBUG2)
	    isp_print_qentry(isp, __func__, isp->isp_reqidx, outp);
	ISP_SYNC_REQUEST(isp);
	return (0);
}

int
isp_exec_entry_mbox(ispsoftc_t *isp, void *src, void *dst, int timeout)
{
	mbreg_t mbs;

	isp_put_entry(isp, src, isp->isp_iocb);
	MEMORYBARRIER(isp, SYNC_IFORDEV, 0, 2 * QENTRY_LEN, 0);

	MBSINIT(&mbs, MBOX_EXEC_COMMAND_IOCB_A64, MBLOGALL,
	    MBCMD_DEFAULT_TIMEOUT + timeout * 1000000);
	mbs.param[1] = QENTRY_LEN;
	mbs.param[2] = DMA_WD1(isp->isp_iocb_dma);
	mbs.param[3] = DMA_WD0(isp->isp_iocb_dma);
	mbs.param[6] = DMA_WD3(isp->isp_iocb_dma);
	mbs.param[7] = DMA_WD2(isp->isp_iocb_dma);
	isp_control(isp, ISPCTL_RUN_MBOXCMD, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE)
		return (EIO);

	MEMORYBARRIER(isp, SYNC_IFORCPU, QENTRY_LEN, QENTRY_LEN, 0);
	isp_get_entry(isp, &((isp24xx_statusreq_t *)isp->isp_iocb)[1], dst);
	return (0);
}

int
isp_exec_entry_queue(ispsoftc_t *isp, void *src, void *dst, int timeout)
{
	uint8_t resp[QENTRY_LEN];
	uint32_t hdl;

	timeout *= hz;
again:
	/* Wait for empty request queue slot. */
	while (!isp_rqentry_avail(isp, QENTRY_MAX) && timeout-- > 0)
		msleep(resp, &isp->isp_lock, 0, "isprqa", 1);
	if (timeout <= 0)
		return (ENOMEM);

	/* Allocate handle.  Should always be available, but stay safe. */
	hdl = isp_allocate_handle(isp, resp, ISP_HANDLE_CTRL);
	if (hdl == 0) {
		if (timeout-- > 0) {
			msleep(resp, &isp->isp_lock, 0, "ispha", 1);
			goto again;
		}
		return (ENOMEM);
	}
	((uint32_t *)src)[1] = hdl;

	/* Execute the request and wait for response. */
	isp_send_entry(isp, src);
	if (msleep(resp, &isp->isp_lock, 0, "ispeeq", timeout) != 0) {
		isp_destroy_handle(isp, hdl);
		return (EWOULDBLOCK);
	}
	isp_get_entry(isp, resp, dst);
	return (0);
}

/*
 * vim:ts=8:sw=8
 */
