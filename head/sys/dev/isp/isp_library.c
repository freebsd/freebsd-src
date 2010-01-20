/*-
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
__FBSDID("$FreeBSD$");
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
isp_send_cmd(ispsoftc_t *isp, void *fqe, void *segp, uint32_t nsegs, uint32_t totalcnt, isp_ddir_t ddir)
{
	uint8_t storage[QENTRY_LEN];
	uint8_t type, nqe;
	uint32_t seg, curseg, seglim, nxt, nxtnxt, ddf;
	ispds_t *dsp = NULL;
	ispds64_t *dsp64 = NULL;
	void *qe0, *qe1;

	qe0 = isp_getrqentry(isp);
	if (qe0 == NULL) {
		return (CMD_EAGAIN);
	}
	nxt = ISP_NXT_QENTRY(isp->isp_reqidx, RQUEST_QUEUE_LEN(isp));

	type = ((isphdr_t *)fqe)->rqs_entry_type;
	nqe = 1;

	/*
	 * If we have no data to transmit, just copy the first IOCB and start it up.
	 */
	if (ddir == ISP_NOXFR) {
		if (type == RQSTYPE_T2RQS || type == RQSTYPE_T3RQS) {
			ddf = CT2_NO_DATA;
		} else {
			ddf = 0;
		}
		goto copy_and_sync;
	}

	/*
	 * First figure out how many pieces of data to transfer and what kind and how many we can put into the first queue entry.
	 */
	switch (type) {
	case RQSTYPE_REQUEST:
		ddf = (ddir == ISP_TO_DEVICE)? REQFLAG_DATA_OUT : REQFLAG_DATA_IN;
		dsp = ((ispreq_t *)fqe)->req_dataseg;
		seglim = ISP_RQDSEG;
		break;
	case RQSTYPE_CMDONLY:
		ddf = (ddir == ISP_TO_DEVICE)? REQFLAG_DATA_OUT : REQFLAG_DATA_IN;
		seglim = 0;
		break;
	case RQSTYPE_T2RQS:
		ddf = (ddir == ISP_TO_DEVICE)? REQFLAG_DATA_OUT : REQFLAG_DATA_IN;
		dsp = ((ispreqt2_t *)fqe)->req_dataseg;
		seglim = ISP_RQDSEG_T2;
		break;
	case RQSTYPE_A64:
		ddf = (ddir == ISP_TO_DEVICE)? REQFLAG_DATA_OUT : REQFLAG_DATA_IN;
		dsp64 = ((ispreqt3_t *)fqe)->req_dataseg;
		seglim = ISP_RQDSEG_T3;
		break;
	case RQSTYPE_T3RQS:
		ddf = (ddir == ISP_TO_DEVICE)? REQFLAG_DATA_OUT : REQFLAG_DATA_IN;
		dsp64 = ((ispreqt3_t *)fqe)->req_dataseg;
		seglim = ISP_RQDSEG_T3;
		break;
	case RQSTYPE_T7RQS:
		ddf = (ddir == ISP_TO_DEVICE)? FCP_CMND_DATA_WRITE : FCP_CMND_DATA_READ;
		dsp64 = &((ispreqt7_t *)fqe)->req_dataseg;
		seglim = 1;
		break;
	default:
		return (CMD_COMPLETE);
	}

	if (seglim > nsegs) {
		seglim = nsegs;
	}

	for (seg = curseg = 0; curseg < seglim; curseg++) {
		if (dsp64) {
			XS_GET_DMA64_SEG(dsp64++, segp, seg++);
		} else {
			XS_GET_DMA_SEG(dsp++, segp, seg++);
		}
	}


	/*
	 * Second, start building additional continuation segments as needed.
	 */
	while (seg < nsegs) {
		nxtnxt = ISP_NXT_QENTRY(nxt, RQUEST_QUEUE_LEN(isp));
		if (nxtnxt == isp->isp_reqodx) {
			return (CMD_EAGAIN);
		}
		ISP_MEMZERO(storage, QENTRY_LEN);
		qe1 = ISP_QUEUE_ENTRY(isp->isp_rquest, nxt);
		nxt = nxtnxt;
		if (dsp64) {
			ispcontreq64_t *crq = (ispcontreq64_t *) storage;
			seglim = ISP_CDSEG64;
			crq->req_header.rqs_entry_type = RQSTYPE_A64_CONT;
			crq->req_header.rqs_entry_count = 1;
			dsp64 = crq->req_dataseg;
		} else {
			ispcontreq_t *crq = (ispcontreq_t *) storage;
			seglim = ISP_CDSEG;
			crq->req_header.rqs_entry_type = RQSTYPE_DATASEG;
			crq->req_header.rqs_entry_count = 1;
			dsp = crq->req_dataseg;
		}
		if (seg + seglim > nsegs) {
			seglim = nsegs - seg;
		}
		for (curseg = 0; curseg < seglim; curseg++) {
			if (dsp64) {
				XS_GET_DMA64_SEG(dsp64++, segp, seg++);
			} else {
				XS_GET_DMA_SEG(dsp++, segp, seg++);
			}
		}
		if (dsp64) {
			isp_put_cont64_req(isp, (ispcontreq64_t *)storage, qe1);
		} else {
			isp_put_cont_req(isp, (ispcontreq_t *)storage, qe1);
		}
		if (isp->isp_dblev & ISP_LOGDEBUG1) {
			isp_print_bytes(isp, "additional queue entry", QENTRY_LEN, storage);
		}
		nqe++;
        }

copy_and_sync:
	((isphdr_t *)fqe)->rqs_entry_count = nqe;
	switch (type) {
	case RQSTYPE_REQUEST:
		((ispreq_t *)fqe)->req_flags |= ddf;
		/*
		 * This is historical and not clear whether really needed.
		 */
		if (nsegs == 0) {
			nsegs = 1;
		}
		((ispreq_t *)fqe)->req_seg_count = nsegs;
		isp_put_request(isp, fqe, qe0);
		break;
	case RQSTYPE_CMDONLY:
		((ispreq_t *)fqe)->req_flags |= ddf;
		/*
		 * This is historical and not clear whether really needed.
		 */
		if (nsegs == 0) {
			nsegs = 1;
		}
		((ispextreq_t *)fqe)->req_seg_count = nsegs;
		isp_put_extended_request(isp, fqe, qe0);
		break;
	case RQSTYPE_T2RQS:
		((ispreqt2_t *)fqe)->req_flags |= ddf;
		((ispreqt2_t *)fqe)->req_seg_count = nsegs;
		((ispreqt2_t *)fqe)->req_totalcnt = totalcnt;
		if (ISP_CAP_2KLOGIN(isp)) {
			isp_put_request_t2e(isp, fqe, qe0);
		} else {
			isp_put_request_t2(isp, fqe, qe0);
		}
		break;
	case RQSTYPE_A64:
	case RQSTYPE_T3RQS:
		((ispreqt3_t *)fqe)->req_flags |= ddf;
		((ispreqt3_t *)fqe)->req_seg_count = nsegs;
		((ispreqt3_t *)fqe)->req_totalcnt = totalcnt;
		if (ISP_CAP_2KLOGIN(isp)) {
			isp_put_request_t3e(isp, fqe, qe0);
		} else {
			isp_put_request_t3(isp, fqe, qe0);
		}
		break;
	case RQSTYPE_T7RQS:
        	((ispreqt7_t *)fqe)->req_alen_datadir = ddf;
		((ispreqt7_t *)fqe)->req_seg_count = nsegs;
		((ispreqt7_t *)fqe)->req_dl = totalcnt;
		isp_put_request_t7(isp, fqe, qe0);
		break;
	default:
		return (CMD_COMPLETE);
	}
	if (isp->isp_dblev & ISP_LOGDEBUG1) {
		isp_print_bytes(isp, "first queue entry", QENTRY_LEN, fqe);
	}
	ISP_ADD_REQUEST(isp, nxt);
	return (CMD_QUEUED);
}

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

/*
 * Make sure we have space to put something on the request queue.
 * Return a pointer to that entry if we do. A side effect of this
 * function is to update the output index. The input index
 * stays the same.
 */
void *
isp_getrqentry(ispsoftc_t *isp)
{
	isp->isp_reqodx = ISP_READ(isp, isp->isp_rqstoutrp);
	if (ISP_NXT_QENTRY(isp->isp_reqidx, RQUEST_QUEUE_LEN(isp)) == isp->isp_reqodx) {
		return (NULL);
	}
	return (ISP_QUEUE_ENTRY(isp->isp_rquest, isp->isp_reqidx));
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
	fcparam *fcp;

	fcp = FCPARAM(isp, chan);
        if (fcp->role == ISP_ROLE_NONE) {
		return (0);
	}
	if (fcp->isp_fwstate < FW_READY || fcp->isp_loopstate < LOOP_PDB_RCVD) {
		if (isp_control(isp, ISPCTL_FCLINK_TEST, chan, tval) != 0) {
			isp_prt(isp, ISP_LOGSANCFG, "isp_fc_runstate: linktest failed for channel %d", chan);
			return (-1);
		}
		if (fcp->isp_fwstate != FW_READY || fcp->isp_loopstate < LOOP_PDB_RCVD) {
			isp_prt(isp, ISP_LOGSANCFG, "isp_fc_runstate: f/w not ready for channel %d", chan);
			return (-1);
		}
	}

	if ((fcp->role & ISP_ROLE_INITIATOR) == 0) {
		return (0);
	}

	if (isp_control(isp, ISPCTL_SCAN_LOOP, chan) != 0) {
		isp_prt(isp, ISP_LOGSANCFG, "isp_fc_runstate: scan loop fails on channel %d", chan);
		return (LOOP_PDB_RCVD);
	}
	if (isp_control(isp, ISPCTL_SCAN_FABRIC, chan) != 0) {
		isp_prt(isp, ISP_LOGSANCFG, "isp_fc_runstate: scan fabric fails on channel %d", chan);
		return (LOOP_LSCAN_DONE);
	}
	if (isp_control(isp, ISPCTL_PDB_SYNC, chan) != 0) {
		isp_prt(isp, ISP_LOGSANCFG, "isp_fc_runstate: pdb_sync fails on channel %d", chan);
		return (LOOP_FSCAN_DONE);
	}
	if (fcp->isp_fwstate != FW_READY || fcp->isp_loopstate != LOOP_READY) {
		isp_prt(isp, ISP_LOGSANCFG, "isp_fc_runstate: f/w not ready again on channel %d", chan);
		return (-1);
	}
	return (0);
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

		if (lp->state == FC_PORTDB_STATE_NIL && lp->target_mode == 0) {
			continue;
		}
		if (lp->dev_map_idx) {
			ISP_SNPRINTF(mb, sizeof (mb), "%3d", ((int) lp->dev_map_idx) - 1);
		} else {
			ISP_SNPRINTF(mb, sizeof (mb), "---");
		}
		isp_prt(isp, ISP_LOGALL, "Chan %d [%d]: hdl 0x%x %s al%d tgt %s %s 0x%06x =>%s 0x%06x; WWNN 0x%08x%08x WWPN 0x%08x%08x",
		    chan, i, lp->handle, dbs[lp->state], lp->autologin, mb, roles[lp->roles], lp->portid, roles[lp->new_roles], lp->new_portid,
		    (uint32_t) (lp->node_wwn >> 32), (uint32_t) (lp->node_wwn), (uint32_t) (lp->port_wwn >> 32), (uint32_t) (lp->port_wwn));
	}
}

const char *
isp_fc_fw_statename(int state)
{
	switch (state) {
	case FW_CONFIG_WAIT:	return "Config Wait";
	case FW_WAIT_AL_PA:	return "Waiting for AL_PA";
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
	case LOOP_LIP_RCVD:             return "LIP Received";
	case LOOP_PDB_RCVD:             return "PDB Received";
	case LOOP_SCANNING_LOOP:        return "Scanning";
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

	if (fcp->isp_fwstate != FW_READY) {
		return "Unavailable";
	}
	switch (fcp->isp_topo) {
	case TOPO_NL_PORT:      return "Private Loop";
	case TOPO_FL_PORT:      return "FL Port";
	case TOPO_N_PORT:       return "N-Port to N-Port";
	case TOPO_F_PORT:       return "F Port";
	case TOPO_PTP_STUB:     return "F Port (no FLOGI_ACC response)";
	default:                return "?????";
	}
}

/*
 * Change Roles
 */
int
isp_fc_change_role(ispsoftc_t *isp, int chan, int new_role)
{
	fcparam *fcp = FCPARAM(isp, chan);

	if (chan >= isp->isp_nchan) {
		isp_prt(isp, ISP_LOGWARN, "%s: bad channel %d", __func__, chan);
		return (ENXIO);
	}
	if (chan == 0) {
#ifdef	ISP_TARGET_MODE
		isp_del_all_wwn_entries(isp, chan);
#endif
		isp_clear_commands(isp);

		isp_reset(isp, 0);
		if (isp->isp_state != ISP_RESETSTATE) {
			isp_prt(isp, ISP_LOGERR, "%s: cannot reset card", __func__);
			return (EIO);
		}
		fcp->role = new_role;
		isp_init(isp);
		if (isp->isp_state != ISP_INITSTATE) {
			isp_prt(isp, ISP_LOGERR, "%s: cannot init card", __func__);
			return (EIO);
		}
		isp->isp_state = ISP_RUNSTATE;
		return (0);
	} else if (ISP_CAP_MULTI_ID(isp)) {
		mbreg_t mbs;
		vp_modify_t *vp;
		uint8_t qe[QENTRY_LEN], *scp;

		ISP_MEMZERO(qe, QENTRY_LEN);
		/* Acquire Scratch */

		if (FC_SCRATCH_ACQUIRE(isp, chan)) {
			return (EBUSY);
		}
		scp = fcp->isp_scratch;

		/*
		 * Build a VP MODIFY command in memory
		 */
		vp = (vp_modify_t *) qe;
		vp->vp_mod_hdr.rqs_entry_type = RQSTYPE_VP_MODIFY;
		vp->vp_mod_hdr.rqs_entry_count = 1;
		vp->vp_mod_cnt = 1;
		vp->vp_mod_idx0 = chan;
		vp->vp_mod_cmd = VP_MODIFY_ENA;
		vp->vp_mod_ports[0].options = ICB2400_VPOPT_ENABLED;
		if (new_role & ISP_ROLE_INITIATOR) {
			vp->vp_mod_ports[0].options |= ICB2400_VPOPT_INI_ENABLE;
		}
		if ((new_role & ISP_ROLE_TARGET) == 0) {
			vp->vp_mod_ports[0].options |= ICB2400_VPOPT_TGT_DISABLE;
		}
		MAKE_NODE_NAME_FROM_WWN(vp->vp_mod_ports[0].wwpn, fcp->isp_wwpn);
		MAKE_NODE_NAME_FROM_WWN(vp->vp_mod_ports[0].wwnn, fcp->isp_wwnn);
		isp_put_vp_modify(isp, vp, (vp_modify_t *) scp);

		/*
		 * Build a EXEC IOCB A64 command that points to the VP MODIFY command
		 */
		MBSINIT(&mbs, MBOX_EXEC_COMMAND_IOCB_A64, MBLOGALL, 0);
		mbs.param[1] = QENTRY_LEN;
		mbs.param[2] = DMA_WD1(fcp->isp_scdma);
		mbs.param[3] = DMA_WD0(fcp->isp_scdma);
		mbs.param[6] = DMA_WD3(fcp->isp_scdma);
		mbs.param[7] = DMA_WD2(fcp->isp_scdma);
		MEMORYBARRIER(isp, SYNC_SFORDEV, 0, 2 * QENTRY_LEN);
		isp_control(isp, ISPCTL_RUN_MBOXCMD, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			FC_SCRATCH_RELEASE(isp, chan);
			return (EIO);
		}
		MEMORYBARRIER(isp, SYNC_SFORCPU, QENTRY_LEN, QENTRY_LEN);
		isp_get_vp_modify(isp, (vp_modify_t *)&scp[QENTRY_LEN], vp);

#ifdef	ISP_TARGET_MODE
		isp_del_all_wwn_entries(isp, chan);
#endif
		/*
		 * Release Scratch
		 */
		FC_SCRATCH_RELEASE(isp, chan);

		if (vp->vp_mod_status != VP_STS_OK) {
			isp_prt(isp, ISP_LOGERR, "%s: VP_MODIFY of Chan %d failed with status %d", __func__, chan, vp->vp_mod_status);
			return (EIO);
		}
		fcp->role = new_role;
		return (0);
	} else {
		return (EINVAL);
	}
}

void
isp_clear_commands(ispsoftc_t *isp)
{
	XS_T *xs;
	uint32_t tmp, handle;
#ifdef	ISP_TARGET_MODE
	isp_notify_t notify;
#endif

	for (tmp = 0; isp->isp_xflist && tmp < isp->isp_maxcmds; tmp++) {
		xs = isp->isp_xflist[tmp];
		if (xs == NULL) {
			continue;
		}
		handle = isp_find_handle(isp, xs);
		if (handle == 0) {
			continue;
		}
		if (XS_XFRLEN(xs)) {
			ISP_DMAFREE(isp, xs, handle);
			XS_SET_RESID(xs, XS_XFRLEN(xs));
		} else {
			XS_SET_RESID(xs, 0);
		}
		isp_destroy_handle(isp, handle);
		XS_SETERR(xs, HBA_BUSRESET);
		isp_done(xs);
	}
#ifdef	ISP_TARGET_MODE
	for (tmp = 0; isp->isp_tgtlist && tmp < isp->isp_maxcmds; tmp++) {
		uint8_t local[QENTRY_LEN];

		xs = isp->isp_tgtlist[tmp];
		if (xs == NULL) {
			continue;
		}
		handle = isp_find_tgt_handle(isp, xs);
		if (handle == 0) {
			continue;
		}
		ISP_DMAFREE(isp, xs, handle);

		ISP_MEMZERO(local, QENTRY_LEN);
		if (IS_24XX(isp)) {
			ct7_entry_t *ctio = (ct7_entry_t *) local;
			ctio->ct_syshandle = handle;
			ctio->ct_nphdl = CT_HBA_RESET;
			ctio->ct_header.rqs_entry_type = RQSTYPE_CTIO7;
		} else if (IS_FC(isp)) {
			ct2_entry_t *ctio = (ct2_entry_t *) local;
			ctio->ct_syshandle = handle;
			ctio->ct_status = CT_HBA_RESET;
			ctio->ct_header.rqs_entry_type = RQSTYPE_CTIO2;
		} else {
			ct_entry_t *ctio = (ct_entry_t *) local;
			ctio->ct_syshandle = handle & 0xffff;
			ctio->ct_status = CT_HBA_RESET & 0xff;
			ctio->ct_header.rqs_entry_type = RQSTYPE_CTIO;
		}
		isp_async(isp, ISPASYNC_TARGET_ACTION, local);
	}
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

#define	ISP_IS_SBUS(isp)	(ISP_SBUS_SUPPORTED && (isp)->isp_bustype == ISP_BT_SBUS)

#define	ASIZE(x)	(sizeof (x) / sizeof (x[0]))
/*
 * Swizzle/Copy Functions
 */
void
isp_put_hdr(ispsoftc_t *isp, isphdr_t *hpsrc, isphdr_t *hpdst)
{
	if (ISP_IS_SBUS(isp)) {
		ISP_IOXPUT_8(isp, hpsrc->rqs_entry_type, &hpdst->rqs_entry_count);
		ISP_IOXPUT_8(isp, hpsrc->rqs_entry_count, &hpdst->rqs_entry_type);
		ISP_IOXPUT_8(isp, hpsrc->rqs_seqno, &hpdst->rqs_flags);
		ISP_IOXPUT_8(isp, hpsrc->rqs_flags, &hpdst->rqs_seqno);
	} else {
		ISP_IOXPUT_8(isp, hpsrc->rqs_entry_type, &hpdst->rqs_entry_type);
		ISP_IOXPUT_8(isp, hpsrc->rqs_entry_count, &hpdst->rqs_entry_count);
		ISP_IOXPUT_8(isp, hpsrc->rqs_seqno, &hpdst->rqs_seqno);
		ISP_IOXPUT_8(isp, hpsrc->rqs_flags, &hpdst->rqs_flags);
	}
}

void
isp_get_hdr(ispsoftc_t *isp, isphdr_t *hpsrc, isphdr_t *hpdst)
{
	if (ISP_IS_SBUS(isp)) {
		ISP_IOXGET_8(isp, &hpsrc->rqs_entry_type, hpdst->rqs_entry_count);
		ISP_IOXGET_8(isp, &hpsrc->rqs_entry_count, hpdst->rqs_entry_type);
		ISP_IOXGET_8(isp, &hpsrc->rqs_seqno, hpdst->rqs_flags);
		ISP_IOXGET_8(isp, &hpsrc->rqs_flags, hpdst->rqs_seqno);
	} else {
		ISP_IOXGET_8(isp, &hpsrc->rqs_entry_type, hpdst->rqs_entry_type);
		ISP_IOXGET_8(isp, &hpsrc->rqs_entry_count, hpdst->rqs_entry_count);
		ISP_IOXGET_8(isp, &hpsrc->rqs_seqno, hpdst->rqs_seqno);
		ISP_IOXGET_8(isp, &hpsrc->rqs_flags, hpdst->rqs_flags);
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
		ISP_IOXPUT_32(isp, rqsrc->req_dataseg[i].ds_base, &rqdst->req_dataseg[i].ds_base);
		ISP_IOXPUT_32(isp, rqsrc->req_dataseg[i].ds_count, &rqdst->req_dataseg[i].ds_count);
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
		ISP_IOXPUT_8(isp, src->mrk_reserved1[i], &dst->mrk_reserved1[i]);
	}
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
	for (i = 0; i < ASIZE(src->mrk_lun); i++) {
		ISP_IOXPUT_8(isp, src->mrk_lun[i], &dst->mrk_lun[i]);
	}
	for (i = 0; i < ASIZE(src->mrk_reserved3); i++) {
		ISP_IOXPUT_8(isp, src->mrk_reserved3[i], &dst->mrk_reserved3[i]);
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
		ISP_IOXPUT_32(isp, src->req_dataseg[i].ds_base, &dst->req_dataseg[i].ds_base);
		ISP_IOXPUT_32(isp, src->req_dataseg[i].ds_count, &dst->req_dataseg[i].ds_count);
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
		ISP_IOXPUT_32(isp, src->req_dataseg[i].ds_base, &dst->req_dataseg[i].ds_base);
		ISP_IOXPUT_32(isp, src->req_dataseg[i].ds_count, &dst->req_dataseg[i].ds_count);
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
		ISP_IOXPUT_32(isp, src->req_dataseg[i].ds_base, &dst->req_dataseg[i].ds_base);
		ISP_IOXPUT_32(isp, src->req_dataseg[i].ds_basehi, &dst->req_dataseg[i].ds_basehi);
		ISP_IOXPUT_32(isp, src->req_dataseg[i].ds_count, &dst->req_dataseg[i].ds_count);
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
		ISP_IOXPUT_32(isp, src->req_dataseg[i].ds_base, &dst->req_dataseg[i].ds_base);
		ISP_IOXPUT_32(isp, src->req_dataseg[i].ds_basehi, &dst->req_dataseg[i].ds_basehi);
		ISP_IOXPUT_32(isp, src->req_dataseg[i].ds_count, &dst->req_dataseg[i].ds_count);
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
	for (i = 0; i < ASIZE(src->tmf_reserved0); i++) {
		ISP_IOXPUT_8(isp, src->tmf_reserved0[i], &dst->tmf_reserved0[i]);
	}
	a = (uint32_t *) src->tmf_lun;
	b = (uint32_t *) dst->tmf_lun;
	for (i = 0; i < (ASIZE(src->tmf_lun) >> 2); i++ ) {
		*b++ = ISP_SWAP32(isp, *a++);
	}
	ISP_IOXPUT_32(isp, src->tmf_flags, &dst->tmf_flags);
	for (i = 0; i < ASIZE(src->tmf_reserved1); i++) {
		ISP_IOXPUT_8(isp, src->tmf_reserved1[i], &dst->tmf_reserved1[i]);
	}
	ISP_IOXPUT_16(isp, src->tmf_tidlo, &dst->tmf_tidlo);
	ISP_IOXPUT_8(isp, src->tmf_tidhi, &dst->tmf_tidhi);
	ISP_IOXPUT_8(isp, src->tmf_vpidx, &dst->tmf_vpidx);
	for (i = 0; i < ASIZE(src->tmf_reserved2); i++) {
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
	for (i = 0; i < ASIZE(src->abrt_reserved); i++) {
		ISP_IOXPUT_8(isp, src->abrt_reserved[i], &dst->abrt_reserved[i]);
	}
	ISP_IOXPUT_16(isp, src->abrt_tidlo, &dst->abrt_tidlo);
	ISP_IOXPUT_8(isp, src->abrt_tidhi, &dst->abrt_tidhi);
	ISP_IOXPUT_8(isp, src->abrt_vpidx, &dst->abrt_vpidx);
	for (i = 0; i < ASIZE(src->abrt_reserved1); i++) {
		ISP_IOXPUT_8(isp, src->abrt_reserved1[i], &dst->abrt_reserved1[i]);
	}
}

void
isp_put_cont_req(ispsoftc_t *isp, ispcontreq_t *src, ispcontreq_t *dst)
{
	int i;
	isp_put_hdr(isp, &src->req_header, &dst->req_header);
	for (i = 0; i < ISP_CDSEG; i++) {
		ISP_IOXPUT_32(isp, src->req_dataseg[i].ds_base, &dst->req_dataseg[i].ds_base);
		ISP_IOXPUT_32(isp, src->req_dataseg[i].ds_count, &dst->req_dataseg[i].ds_count);
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
isp_get_response(ispsoftc_t *isp, ispstatusreq_t *src, ispstatusreq_t *dst)
{
	int i;
	isp_get_hdr(isp, &src->req_header, &dst->req_header);
	ISP_IOXGET_32(isp, &src->req_handle, dst->req_handle);
	ISP_IOXGET_16(isp, &src->req_scsi_status, dst->req_scsi_status);
	ISP_IOXGET_16(isp, &src->req_completion_status, dst->req_completion_status);
	ISP_IOXGET_16(isp, &src->req_state_flags, dst->req_state_flags);
	ISP_IOXGET_16(isp, &src->req_status_flags, dst->req_status_flags);
	ISP_IOXGET_16(isp, &src->req_time, dst->req_time);
	ISP_IOXGET_16(isp, &src->req_sense_len, dst->req_sense_len);
	ISP_IOXGET_32(isp, &src->req_resid, dst->req_resid);
	for (i = 0; i < 8; i++) {
		ISP_IOXGET_8(isp, &src->req_response[i], dst->req_response[i]);
	}
	for (i = 0; i < 32; i++) {
		ISP_IOXGET_8(isp, &src->req_sense_data[i], dst->req_sense_data[i]);
	}
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
		ISP_IOXGET_8(isp, &src->abrt_reserved[i], dst->abrt_reserved[i]);
	}
	ISP_IOXGET_16(isp, &src->abrt_tidlo, dst->abrt_tidlo);
	ISP_IOXGET_8(isp, &src->abrt_tidhi, dst->abrt_tidhi);
	ISP_IOXGET_8(isp, &src->abrt_vpidx, dst->abrt_vpidx);
	for (i = 0; i < ASIZE(src->abrt_reserved1); i++) {
		ISP_IOXGET_8(isp, &src->abrt_reserved1[i], dst->abrt_reserved1[i]);
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
		ISP_IOXGET_16(isp, &r2src->req_handles[i], r2dst->req_handles[i]);
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
	for (i = 0; i < 12; i++) {
		ISP_IOXPUT_16(isp, src->icb_reserved2[i], &dst->icb_reserved2[i]);
	}
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
	for (i = 0; i < ASIZE(src->vp_port_portname); i++) {
		ISP_IOXGET_8(isp, &src->vp_port_portname[i], dst->vp_port_portname[i]);
	}
	for (i = 0; i < ASIZE(src->vp_port_nodename); i++) {
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
	for (i = 0; i < ASIZE(src->vp_ctrl_idmap); i++) {
		ISP_IOXPUT_16(isp, src->vp_ctrl_idmap[i], &dst->vp_ctrl_idmap[i]);
	}
	for (i = 0; i < ASIZE(src->vp_ctrl_reserved); i++) {
		ISP_IOXPUT_8(isp, src->vp_ctrl_idmap[i], &dst->vp_ctrl_idmap[i]);
	}
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
	for (i = 0; i < ASIZE(src->vp_ctrl_idmap); i++) {
		ISP_IOXGET_16(isp, &src->vp_ctrl_idmap[i], dst->vp_ctrl_idmap[i]);
	}
	for (i = 0; i < ASIZE(src->vp_ctrl_reserved); i++) {
		ISP_IOXGET_8(isp, &src->vp_ctrl_reserved[i], dst->vp_ctrl_reserved[i]);
	}
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
	for (i = 0; i < ASIZE(src->vp_mod_ports); i++) {
		ISP_IOXPUT_8(isp, src->vp_mod_ports[i].options, &dst->vp_mod_ports[i].options);
		ISP_IOXPUT_8(isp, src->vp_mod_ports[i].loopid, &dst->vp_mod_ports[i].loopid);
		ISP_IOXPUT_16(isp, src->vp_mod_ports[i].reserved1, &dst->vp_mod_ports[i].reserved1);
		for (j = 0; j < ASIZE(src->vp_mod_ports[i].wwpn); j++) {
			ISP_IOXPUT_8(isp, src->vp_mod_ports[i].wwpn[j], &dst->vp_mod_ports[i].wwpn[j]);
		}
		for (j = 0; j < ASIZE(src->vp_mod_ports[i].wwnn); j++) {
			ISP_IOXPUT_8(isp, src->vp_mod_ports[i].wwnn[j], &dst->vp_mod_ports[i].wwnn[j]);
		}
	}
	for (i = 0; i < ASIZE(src->vp_mod_reserved2); i++) {
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
	for (i = 0; i < ASIZE(src->vp_mod_ports); i++) {
		ISP_IOXGET_8(isp, &src->vp_mod_ports[i].options, dst->vp_mod_ports[i].options);
		ISP_IOXGET_8(isp, &src->vp_mod_ports[i].loopid, dst->vp_mod_ports[i].loopid);
		ISP_IOXGET_16(isp, &src->vp_mod_ports[i].reserved1, dst->vp_mod_ports[i].reserved1);
		for (j = 0; j < ASIZE(src->vp_mod_ports[i].wwpn); j++) {
			ISP_IOXGET_8(isp, &src->vp_mod_ports[i].wwpn[j], dst->vp_mod_ports[i].wwpn[j]);
		}
		for (j = 0; j < ASIZE(src->vp_mod_ports[i].wwnn); j++) {
			ISP_IOXGET_8(isp, &src->vp_mod_ports[i].wwnn[j], dst->vp_mod_ports[i].wwnn[j]);
		}
	}
	for (i = 0; i < ASIZE(src->vp_mod_reserved2); i++) {
		ISP_IOXGET_8(isp, &src->vp_mod_reserved2[i], dst->vp_mod_reserved2[i]);
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
		ISP_IOXGET_8(isp, &src->pdb_hardaddr_bits[i], dst->pdb_hardaddr_bits[i]);
	}
	for (i = 0; i < 4; i++) {
		ISP_IOXGET_8(isp, &src->pdb_portid_bits[i], dst->pdb_portid_bits[i]);
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
	ISP_IOXGET_16(isp, &src->ridacq_vp_port_lo, dst->ridacq_vp_port_lo);
	ISP_IOXGET_8(isp, &src->ridacq_vp_port_hi, dst->ridacq_vp_port_hi);
	ISP_IOXGET_8(isp, &src->ridacq_format, dst->ridacq_format);
	for (i = 0; i < sizeof (src->ridacq_map) / sizeof (src->ridacq_map[0]); i++) {
		ISP_IOXGET_16(isp, &src->ridacq_map[i], dst->ridacq_map[i]);
	}
	for (i = 0; i < sizeof (src->ridacq_reserved1) / sizeof (src->ridacq_reserved1[0]); i++) {
		ISP_IOXGET_16(isp, &src->ridacq_reserved1[i], dst->ridacq_reserved1[i]);
	}
	if (dst->ridacq_format == 0) {
		ISP_IOXGET_8(isp, &src->un.type0.ridacq_vp_acquired, dst->un.type0.ridacq_vp_acquired);
		ISP_IOXGET_8(isp, &src->un.type0.ridacq_vp_setup, dst->un.type0.ridacq_vp_setup);
		ISP_IOXGET_16(isp, &src->un.type0.ridacq_reserved0, dst->un.type0.ridacq_reserved0);
	} else if (dst->ridacq_format == 1) {
		ISP_IOXGET_16(isp, &src->un.type1.ridacq_vp_count, dst->un.type1.ridacq_vp_count);
		ISP_IOXGET_8(isp, &src->un.type1.ridacq_vp_index, dst->un.type1.ridacq_vp_index);
		ISP_IOXGET_8(isp, &src->un.type1.ridacq_vp_status, dst->un.type1.ridacq_vp_status);
	} else {
		ISP_MEMZERO(&dst->un, sizeof (dst->un));
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
		ISP_IOXGET_32(isp, &src->ms_dataseg[i].ds_base, dst->ms_dataseg[i].ds_base);
		ISP_IOXGET_32(isp, &src->ms_dataseg[i].ds_basehi, dst->ms_dataseg[i].ds_basehi);
		ISP_IOXGET_32(isp, &src->ms_dataseg[i].ds_count, dst->ms_dataseg[i].ds_count);
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
		ISP_IOXPUT_32(isp, src->ms_dataseg[i].ds_base, &dst->ms_dataseg[i].ds_base);
		ISP_IOXPUT_32(isp, src->ms_dataseg[i].ds_basehi, &dst->ms_dataseg[i].ds_basehi);
		ISP_IOXPUT_32(isp, src->ms_dataseg[i].ds_count, &dst->ms_dataseg[i].ds_count);
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
isp_put_gxn_id_request(ispsoftc_t *isp, sns_gxn_id_req_t *src, sns_gxn_id_req_t *dst)
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
isp_get_sns_response(ispsoftc_t *isp, sns_scrsp_t *src, sns_scrsp_t *dst, int nwords)
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
isp_get_gid_ft_response(ispsoftc_t *isp, sns_gid_ft_rsp_t *src, sns_gid_ft_rsp_t *dst, int nwords)
{
	int i;
	isp_get_ct_hdr(isp, &src->snscb_cthdr, &dst->snscb_cthdr);
	for (i = 0; i < nwords; i++) {
		int j;
		ISP_IOXGET_8(isp, &src->snscb_ports[i].control, dst->snscb_ports[i].control);
		for (j = 0; j < 3; j++) {
			ISP_IOXGET_8(isp, &src->snscb_ports[i].portid[j], dst->snscb_ports[i].portid[j]);
		}
		if (dst->snscb_ports[i].control & 0x80) {
			break;
		}
	}
}

void
isp_get_gxn_id_response(ispsoftc_t *isp, sns_gxn_id_rsp_t *src, sns_gxn_id_rsp_t *dst)
{
	int i;
	isp_get_ct_hdr(isp, &src->snscb_cthdr, &dst->snscb_cthdr);
	for (i = 0; i < 8; i++) {
		ISP_IOXGET_8(isp, &src->snscb_wwn[i], dst->snscb_wwn[i]);
	}
}

void
isp_get_gff_id_response(ispsoftc_t *isp, sns_gff_id_rsp_t *src, sns_gff_id_rsp_t *dst)
{
	int i;
	isp_get_ct_hdr(isp, &src->snscb_cthdr, &dst->snscb_cthdr);
	for (i = 0; i < 32; i++) {
		ISP_IOXGET_32(isp, &src->snscb_fc4_features[i], dst->snscb_fc4_features[i]);
	}
}

void
isp_get_ga_nxt_response(ispsoftc_t *isp, sns_ga_nxt_rsp_t *src, sns_ga_nxt_rsp_t *dst)
{
	int i;
	isp_get_ct_hdr(isp, &src->snscb_cthdr, &dst->snscb_cthdr);
	ISP_IOXGET_8(isp, &src->snscb_port_type, dst->snscb_port_type);
	for (i = 0; i < 3; i++) {
		ISP_IOXGET_8(isp, &src->snscb_port_id[i], dst->snscb_port_id[i]);
	}
	for (i = 0; i < 8; i++) {
		ISP_IOXGET_8(isp, &src->snscb_portname[i], dst->snscb_portname[i]);
	}
	ISP_IOXGET_8(isp, &src->snscb_pnlen, dst->snscb_pnlen);
	for (i = 0; i < 255; i++) {
		ISP_IOXGET_8(isp, &src->snscb_pname[i], dst->snscb_pname[i]);
	}
	for (i = 0; i < 8; i++) {
		ISP_IOXGET_8(isp, &src->snscb_nodename[i], dst->snscb_nodename[i]);
	}
	ISP_IOXGET_8(isp, &src->snscb_nnlen, dst->snscb_nnlen);
	for (i = 0; i < 255; i++) {
		ISP_IOXGET_8(isp, &src->snscb_nname[i], dst->snscb_nname[i]);
	}
	for (i = 0; i < 8; i++) {
		ISP_IOXGET_8(isp, &src->snscb_ipassoc[i], dst->snscb_ipassoc[i]);
	}
	for (i = 0; i < 16; i++) {
		ISP_IOXGET_8(isp, &src->snscb_ipaddr[i], dst->snscb_ipaddr[i]);
	}
	for (i = 0; i < 4; i++) {
		ISP_IOXGET_8(isp, &src->snscb_svc_class[i], dst->snscb_svc_class[i]);
	}
	for (i = 0; i < 32; i++) {
		ISP_IOXGET_8(isp, &src->snscb_fc4_types[i], dst->snscb_fc4_types[i]);
	}
	for (i = 0; i < 8; i++) {
		ISP_IOXGET_8(isp, &src->snscb_fpname[i], dst->snscb_fpname[i]);
	}
	ISP_IOXGET_8(isp, &src->snscb_reserved, dst->snscb_reserved);
	for (i = 0; i < 3; i++) {
		ISP_IOXGET_8(isp, &src->snscb_hardaddr[i], dst->snscb_hardaddr[i]);
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
		ISP_IOXGET_8(isp, &src->els_reserved4[i], dst->els_reserved4[i]);
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

/*
 * Command shipping- finish off first queue entry and do dma mapping and
 * additional segments as needed.
 *
 * Called with the first queue entry at least partially filled out.
 */
int
isp_send_tgt_cmd(ispsoftc_t *isp, void *fqe, void *segp, uint32_t nsegs, uint32_t totalcnt, isp_ddir_t ddir, void *snsptr, uint32_t snslen)
{
	uint8_t storage[QENTRY_LEN], storage2[QENTRY_LEN];
	uint8_t type, nqe;
	uint32_t seg, curseg, seglim, nxt, nxtnxt;
	ispds_t *dsp = NULL;
	ispds64_t *dsp64 = NULL;
	void *qe0, *qe1, *sqe = NULL;

	qe0 = isp_getrqentry(isp);
	if (qe0 == NULL) {
		return (CMD_EAGAIN);
	}
	nxt = ISP_NXT_QENTRY(isp->isp_reqidx, RQUEST_QUEUE_LEN(isp));

	type = ((isphdr_t *)fqe)->rqs_entry_type;
	nqe = 1;
	seglim = 0;

	/*
	 * If we have no data to transmit, just copy the first IOCB and start it up.
	 */
	if (ddir != ISP_NOXFR) {
		/*
		 * First, figure out how many pieces of data to transfer and what kind and how many we can put into the first queue entry.
		 */
		switch (type) {
		case RQSTYPE_CTIO:
			dsp = ((ct_entry_t *)fqe)->ct_dataseg;
			seglim = ISP_RQDSEG;
			break;
		case RQSTYPE_CTIO2:
		case RQSTYPE_CTIO3:
		{
			ct2_entry_t *ct = fqe, *ct2 = (ct2_entry_t *) storage2;
			uint16_t swd = ct->rsp.m0.ct_scsi_status & 0xff;

			if ((ct->ct_flags & CT2_SENDSTATUS) && (swd || ct->ct_resid)) {
				memcpy(ct2, ct, QENTRY_LEN);
				/*
				 * Clear fields from first CTIO2 that now need to be cleared
				 */
				ct->ct_header.rqs_seqno = 0;
				ct->ct_flags &= ~(CT2_SENDSTATUS|CT2_CCINCR|CT2_FASTPOST);
				ct->ct_resid = 0;
				ct->ct_syshandle = 0;
				ct->rsp.m0.ct_scsi_status = 0;

				/*
				 * Reset fields in the second CTIO2 as appropriate.
				 */
				ct2->ct_flags &= ~(CT2_FLAG_MMASK|CT2_DATAMASK|CT2_FASTPOST);
				ct2->ct_flags |= CT2_NO_DATA|CT2_FLAG_MODE1;
				ct2->ct_seg_count = 0;
				ct2->ct_reloff = 0;
				memset(&ct2->rsp, 0, sizeof (ct2->rsp));
				if (swd == SCSI_CHECK && snsptr && snslen) {
					ct2->rsp.m1.ct_senselen = min(snslen, MAXRESPLEN);
					memcpy(ct2->rsp.m1.ct_resp, snsptr, ct2->rsp.m1.ct_senselen);
					swd |= CT2_SNSLEN_VALID;
				}
				if (ct2->ct_resid > 0) {
					swd |= CT2_DATA_UNDER;
				} else if (ct2->ct_resid < 0) {
					swd |= CT2_DATA_OVER;
				}
				ct2->rsp.m1.ct_scsi_status = swd;
				sqe = storage2;
			}
			if (type == RQSTYPE_CTIO2) {
				dsp = ct->rsp.m0.u.ct_dataseg;
				seglim = ISP_RQDSEG_T2;
			} else {
				dsp64 = ct->rsp.m0.u.ct_dataseg64;
				seglim = ISP_RQDSEG_T3;
			}
			break;
		}
		case RQSTYPE_CTIO7:
		{
			ct7_entry_t *ct = fqe, *ct2 = (ct7_entry_t *)storage2;
			uint16_t swd = ct->ct_scsi_status & 0xff;

			dsp64 = &ct->rsp.m0.ds;
			seglim = 1;
			if ((ct->ct_flags & CT7_SENDSTATUS) && (swd || ct->ct_resid)) {
				memcpy(ct2, ct, sizeof (ct7_entry_t));

				/*
				 * Clear fields from first CTIO7 that now need to be cleared
				 */
				ct->ct_header.rqs_seqno = 0;
				ct->ct_flags &= ~CT7_SENDSTATUS;
				ct->ct_resid = 0;
				ct->ct_syshandle = 0;
				ct->ct_scsi_status = 0;

				/*
				 * Reset fields in the second CTIO7 as appropriate.
				 */
				ct2->ct_flags &= ~(CT7_FLAG_MMASK|CT7_DATAMASK);
				ct2->ct_flags |= CT7_NO_DATA|CT7_NO_DATA|CT7_FLAG_MODE1;
				ct2->ct_seg_count = 0;
				memset(&ct2->rsp, 0, sizeof (ct2->rsp));
				if (swd == SCSI_CHECK && snsptr && snslen) {
					ct2->rsp.m1.ct_resplen = min(snslen, MAXRESPLEN_24XX);
					memcpy(ct2->rsp.m1.ct_resp, snsptr, ct2->rsp.m1.ct_resplen);
					swd |= (FCP_SNSLEN_VALID << 8);
				}
				if (ct2->ct_resid < 0) {
					swd |= (FCP_RESID_OVERFLOW << 8);
				} else if (ct2->ct_resid > 0) {
					swd |= (FCP_RESID_UNDERFLOW << 8);
				}
				ct2->ct_scsi_status = swd;
				sqe = storage2;
			}
			break;
		}
		default:
			return (CMD_COMPLETE);
		}
	}

	/*
	 * Fill out the data transfer stuff in the first queue entry
	 */
	if (seglim > nsegs) {
		seglim = nsegs;
	}

	for (seg = curseg = 0; curseg < seglim; curseg++) {
		if (dsp64) {
			XS_GET_DMA64_SEG(dsp64++, segp, seg++);
		} else {
			XS_GET_DMA_SEG(dsp++, segp, seg++);
		}
	}

	/*
	 * First, if we are sending status with data and we have a non-zero
	 * status or non-zero residual, we have to make a synthetic extra CTIO
	 * that contains the status that we'll ship separately (FC cards only).
	 */

	/*
	 * Second, start building additional continuation segments as needed.
	 */
	while (seg < nsegs) {
		nxtnxt = ISP_NXT_QENTRY(nxt, RQUEST_QUEUE_LEN(isp));
		if (nxtnxt == isp->isp_reqodx) {
			return (CMD_EAGAIN);
		}
		ISP_MEMZERO(storage, QENTRY_LEN);
		qe1 = ISP_QUEUE_ENTRY(isp->isp_rquest, nxt);
		nxt = nxtnxt;
		if (dsp64) {
			ispcontreq64_t *crq = (ispcontreq64_t *) storage;
			seglim = ISP_CDSEG64;
			crq->req_header.rqs_entry_type = RQSTYPE_A64_CONT;
			crq->req_header.rqs_entry_count = 1;
			dsp64 = crq->req_dataseg;
		} else {
			ispcontreq_t *crq = (ispcontreq_t *) storage;
			seglim = ISP_CDSEG;
			crq->req_header.rqs_entry_type = RQSTYPE_DATASEG;
			crq->req_header.rqs_entry_count = 1;
			dsp = crq->req_dataseg;
		}
		if (seg + seglim > nsegs) {
			seglim = nsegs - seg;
		}
		for (curseg = 0; curseg < seglim; curseg++) {
			if (dsp64) {
				XS_GET_DMA64_SEG(dsp64++, segp, seg++);
			} else {
				XS_GET_DMA_SEG(dsp++, segp, seg++);
			}
		}
		if (dsp64) {
			isp_put_cont64_req(isp, (ispcontreq64_t *)storage, qe1);
		} else {
			isp_put_cont_req(isp, (ispcontreq_t *)storage, qe1);
		}
		if (isp->isp_dblev & ISP_LOGTDEBUG1) {
			isp_print_bytes(isp, "additional queue entry", QENTRY_LEN, storage);
		}
		nqe++;
        }

	/*
	 * If we have a synthetic queue entry to complete things, do it here.
	 */
	if (sqe) {
		nxtnxt = ISP_NXT_QENTRY(nxt, RQUEST_QUEUE_LEN(isp));
		if (nxtnxt == isp->isp_reqodx) {
			return (CMD_EAGAIN);
		}
		qe1 = ISP_QUEUE_ENTRY(isp->isp_rquest, nxt);
		nxt = nxtnxt;
		if (type == RQSTYPE_CTIO7) {
			isp_put_ctio7(isp, sqe, qe1);
		} else {
			isp_put_ctio2(isp, sqe, qe1);
		}
		if (isp->isp_dblev & ISP_LOGTDEBUG1) {
			isp_print_bytes(isp, "synthetic final queue entry", QENTRY_LEN, storage2);
		}
	}

	((isphdr_t *)fqe)->rqs_entry_count = nqe;
	switch (type) {
	case RQSTYPE_CTIO:
		((ct_entry_t *)fqe)->ct_seg_count = nsegs;
		isp_put_ctio(isp, fqe, qe0);
		break;
	case RQSTYPE_CTIO2:
	case RQSTYPE_CTIO3:
		((ct2_entry_t *)fqe)->ct_seg_count = nsegs;
		if (ISP_CAP_2KLOGIN(isp)) {
			isp_put_ctio2e(isp, fqe, qe0);
		} else {
			isp_put_ctio2(isp, fqe, qe0);
		}
		break;
	case RQSTYPE_CTIO7:
		((ct7_entry_t *)fqe)->ct_seg_count = nsegs;
		isp_put_ctio7(isp, fqe, qe0);
		break;
	default:
		return (CMD_COMPLETE);
	}
	if (isp->isp_dblev & ISP_LOGTDEBUG1) {
		isp_print_bytes(isp, "first queue entry", QENTRY_LEN, fqe);
	}
	ISP_ADD_REQUEST(isp, nxt);
	return (CMD_QUEUED);
}

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
	if (handle == 0 || IS_TARGET_HANDLE(handle) == 0 || (handle & ISP_HANDLE_MASK) > isp->isp_maxcmds) {
		isp_prt(isp, ISP_LOGERR, "bad handle %u in isp_find_xs_tgt", handle);
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
				uint32_t handle = i;
				handle += 1;
				handle &= ISP_HANDLE_MASK;
				handle |= 0x8000;
				return (handle);
			}
		}
	}
	return (0);
}

void
isp_destroy_tgt_handle(ispsoftc_t *isp, uint32_t handle)
{
	if (handle == 0 || IS_TARGET_HANDLE(handle) == 0 || (handle & ISP_HANDLE_MASK) > isp->isp_maxcmds) {
		isp_prt(isp, ISP_LOGERR, "bad handle in isp_destroy_tgt_handle");
	} else {
		isp->isp_tgtlist[(handle & ISP_HANDLE_MASK) - 1] = NULL;
	}
}

/*
 * Find target mode entries
 */
int
isp_find_pdb_by_wwn(ispsoftc_t *isp, int chan, uint64_t wwn, fcportdb_t **lptr)
{
	fcparam *fcp;
	int i;

	if (chan < isp->isp_nchan) {
		fcp = FCPARAM(isp, chan);
		for (i = 0; i < MAX_FC_TARG; i++) {
			fcportdb_t *lp = &fcp->portdb[i];

			if (lp->target_mode == 0) {
				continue;
			}
			if (lp->port_wwn == wwn) {
				*lptr = lp;
				return (1);
			}
		}
	}
	return (0);
}

int
isp_find_pdb_by_loopid(ispsoftc_t *isp, int chan, uint32_t loopid, fcportdb_t **lptr)
{
	fcparam *fcp;
	int i;

	if (chan < isp->isp_nchan) {
		fcp = FCPARAM(isp, chan);
		for (i = 0; i < MAX_FC_TARG; i++) {
			fcportdb_t *lp = &fcp->portdb[i];

			if (lp->target_mode == 0) {
				continue;
			}
			if (lp->handle == loopid) {
				*lptr = lp;
				return (1);
			}
		}
	}
	return (0);
}

int
isp_find_pdb_by_sid(ispsoftc_t *isp, int chan, uint32_t sid, fcportdb_t **lptr)
{
	fcparam *fcp;
	int i;

	if (chan >= isp->isp_nchan) {
		return (0);
	}

	fcp = FCPARAM(isp, chan);
	for (i = 0; i < MAX_FC_TARG; i++) {
		fcportdb_t *lp = &fcp->portdb[i];

		if (lp->target_mode == 0) {
			continue;
		}
		if (lp->portid == sid) {
			*lptr = lp;
			return (1);
		}
	}
	return (0);
}

void
isp_find_chan_by_did(ispsoftc_t *isp, uint32_t did, uint16_t *cp)
{
	uint16_t chan;

	*cp = ISP_NOCHAN;
	for (chan = 0; chan < isp->isp_nchan; chan++) {
		fcparam *fcp = FCPARAM(isp, chan);
		if ((fcp->role & ISP_ROLE_TARGET) == 0 || fcp->isp_fwstate != FW_READY || fcp->isp_loopstate < LOOP_PDB_RCVD) {
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
isp_add_wwn_entry(ispsoftc_t *isp, int chan, uint64_t ini, uint16_t nphdl, uint32_t s_id)
{
	fcparam *fcp;
	fcportdb_t *lp;
	isp_notify_t nt;
	int i;

	fcp = FCPARAM(isp, chan);

	if (nphdl >= MAX_NPORT_HANDLE) {
		isp_prt(isp, ISP_LOGWARN, "%s: Chan %d IID 0x%016llx bad N-Port handle 0x%04x Port ID 0x%06x",
		    __func__, chan, (unsigned long long) ini, nphdl, s_id);
		return;
	}

	lp = NULL;
	if (fcp->isp_tgt_map[nphdl]) {
		lp = &fcp->portdb[fcp->isp_tgt_map[nphdl] - 1];
	} else {
		/*
		 * Make sure the addition of a new target mode entry doesn't duplicate entries
		 * with the same N-Port handles, the same portids or the same Port WWN.
		 */
		for (i = 0; i < MAX_FC_TARG; i++) {
			lp = &fcp->portdb[i];
			if (lp->target_mode == 0) {
				lp = NULL;
				continue;
			}
			if (lp->handle == nphdl) {
				break;
			}
			if (s_id != PORT_ANY && lp->portid == s_id) {
				break;
			}
			if (VALID_INI(ini) && lp->port_wwn == ini) {
				break;
			}
			lp = NULL;
		}

	}

	if (lp) {
		int something = 0;
		if (lp->handle != nphdl) {
			isp_prt(isp, ISP_LOGWARN, "%s: Chan %d attempt to re-enter N-port handle 0x%04x IID 0x%016llx Port ID 0x%06x finds IID 0x%016llx N-Port Handle 0x%04x Port ID 0x%06x",
			    __func__, chan, nphdl, (unsigned long long)ini, s_id, (unsigned long long) lp->port_wwn, lp->handle, lp->portid);
			isp_dump_portdb(isp, chan);
			return;
		}
		if (s_id != PORT_NONE) {
			if (lp->portid == PORT_NONE) {
				lp->portid = s_id;
				isp_prt(isp, ISP_LOGTINFO, "%s: Chan %d N-port handle 0x%04x gets Port ID 0x%06x", __func__, chan, nphdl, s_id);
				something++;
			} else if (lp->portid != s_id) {
				isp_prt(isp, ISP_LOGTINFO, "%s: Chan %d N-port handle 0x%04x tries to change Port ID 0x%06x to 0x%06x", __func__, chan, nphdl,
				    lp->portid, s_id);
				isp_dump_portdb(isp, chan);
				return;
			}
		}
		if (VALID_INI(ini)) {
			if (!VALID_INI(lp->port_wwn)) {
				lp->port_wwn = ini;
				isp_prt(isp, ISP_LOGTINFO, "%s: Chan %d N-port handle 0x%04x gets WWN 0x%016llxx", __func__, chan, nphdl, (unsigned long long) ini);
				something++;
			} else if (lp->port_wwn != ini) {
				isp_prt(isp, ISP_LOGWARN, "%s: Chan %d N-port handle 0x%04x tries to change WWN 0x%016llx to 0x%016llx", __func__, chan, nphdl,
				    (unsigned long long) lp->port_wwn, (unsigned long long) ini);
				isp_dump_portdb(isp, chan);
				return;
			}
		}

		if (!something) {
			isp_prt(isp, ISP_LOGWARN, "%s: Chan %d IID 0x%016llx N-Port Handle 0x%04x Port ID 0x%06x reentered", __func__, chan,
			    (unsigned long long) lp->port_wwn, lp->handle, lp->portid);
		}
		return;
	}

	/*
	 * Find a new spot
	 */
	for (i = MAX_FC_TARG - 1; i >= 0; i--) {
		if (fcp->portdb[i].target_mode == 1) {
			continue;
		}
		if (fcp->portdb[i].state == FC_PORTDB_STATE_NIL) {
			break;
		}
	}
	if (i < 0) {
		isp_prt(isp, ISP_LOGWARN, "%s: Chan %d IID 0x%016llx N-Port Handle 0x%04x Port ID 0x%06x- no room in port database",
		    __func__, chan, (unsigned long long) ini, nphdl, s_id);
		return;
	}

	lp = &fcp->portdb[i];
	ISP_MEMZERO(lp, sizeof (fcportdb_t));
	lp->target_mode = 1;
	lp->handle = nphdl;
	lp->portid = s_id;
	lp->port_wwn = ini;
	fcp->isp_tgt_map[nphdl] = i + 1;

	isp_prt(isp, ISP_LOGTINFO, "%s: Chan %d IID 0x%016llx N-Port Handle 0x%04x Port ID 0x%06x vtgt %d added", __func__, chan, (unsigned long long) ini, nphdl, s_id, fcp->isp_tgt_map[nphdl] - 1);

	ISP_MEMZERO(&nt, sizeof (nt));
	nt.nt_hba = isp;
	nt.nt_wwn = ini;
	nt.nt_tgt = FCPARAM(isp, chan)->isp_wwpn;
	nt.nt_sid = s_id;
	nt.nt_did = FCPARAM(isp, chan)->isp_portid;
	nt.nt_nphdl = nphdl;
	nt.nt_channel = chan;
	nt.nt_ncode = NT_ARRIVED;
	isp_async(isp, ISPASYNC_TARGET_NOTIFY, &nt);
}

/*
 * Remove a target device to the port database
 */
void
isp_del_wwn_entry(ispsoftc_t *isp, int chan, uint64_t ini, uint16_t nphdl, uint32_t s_id)
{
	fcparam *fcp;
	isp_notify_t nt;
	fcportdb_t *lp;

	if (nphdl >= MAX_NPORT_HANDLE) {
		isp_prt(isp, ISP_LOGWARN, "%s: Chan %d IID 0x%016llx bad N-Port handle 0x%04x Port ID 0x%06x",
		    __func__, chan, (unsigned long long) ini, nphdl, s_id);
		return;
	}

	fcp = FCPARAM(isp, chan);
	if (fcp->isp_tgt_map[nphdl] == 0) {
		lp = NULL;
	} else {
		lp = &fcp->portdb[fcp->isp_tgt_map[nphdl] - 1];
		if (lp->target_mode == 0) {
			lp = NULL;
		}
	}
	if (lp == NULL) {
		isp_prt(isp, ISP_LOGWARN, "%s: Chan %d IID 0x%016llx N-Port Handle 0x%04x Port ID 0x%06x cannot be found to be cleared",
		    __func__, chan, (unsigned long long) ini, nphdl, s_id);
		isp_dump_portdb(isp, chan);
		return;
	}
	isp_prt(isp, ISP_LOGTINFO, "%s: Chan %d IID 0x%016llx N-Port Handle 0x%04x Port ID 0x%06x vtgt %d cleared",
	    __func__, chan, (unsigned long long) lp->port_wwn, nphdl, lp->portid, fcp->isp_tgt_map[nphdl] - 1);
	fcp->isp_tgt_map[nphdl] = 0;

	ISP_MEMZERO(&nt, sizeof (nt));
	nt.nt_hba = isp;
	nt.nt_wwn = lp->port_wwn;
	nt.nt_tgt = FCPARAM(isp, chan)->isp_wwpn;
	nt.nt_sid = lp->portid;
	nt.nt_did = FCPARAM(isp, chan)->isp_portid;
	nt.nt_nphdl = nphdl;
	nt.nt_channel = chan;
	nt.nt_ncode = NT_DEPARTED;
	isp_async(isp, ISPASYNC_TARGET_NOTIFY, &nt);
}

void
isp_del_all_wwn_entries(ispsoftc_t *isp, int chan)
{
	fcparam *fcp;
	int i;

	if (!IS_FC(isp)) {
		return;
	}

	/*
	 * Handle iterations over all channels via recursion
	 */
	if (chan == ISP_NOCHAN) {
		for (chan = 0; chan < isp->isp_nchan; chan++) {
			isp_del_all_wwn_entries(isp, chan);
		}
		return;
	}

	if (chan > isp->isp_nchan) {
		return;
	}

	fcp = FCPARAM(isp, chan);
	if (fcp == NULL) {
		return;
	}
	for (i = 0; i < MAX_NPORT_HANDLE; i++) {
		if (fcp->isp_tgt_map[i]) {
			fcportdb_t *lp = &fcp->portdb[fcp->isp_tgt_map[i] - 1];
			isp_del_wwn_entry(isp, chan, lp->port_wwn, lp->handle, lp->portid);
		}
	}
}

void
isp_del_wwn_entries(ispsoftc_t *isp, isp_notify_t *mp)
{
	fcportdb_t *lp;

	/*
	 * Handle iterations over all channels via recursion
	 */
	if (mp->nt_channel == ISP_NOCHAN) {
		for (mp->nt_channel = 0; mp->nt_channel < isp->isp_nchan; mp->nt_channel++) {
			isp_del_wwn_entries(isp, mp);
		}
		mp->nt_channel = ISP_NOCHAN;
		return;
	}

	/*
	 * We have an entry which is only partially identified.
	 *
	 * It's only known by WWN, N-Port handle, or Port ID.
	 * We need to find the actual entry so we can delete it.
	 */
	if (mp->nt_nphdl != NIL_HANDLE) {
		if (isp_find_pdb_by_loopid(isp, mp->nt_channel, mp->nt_nphdl, &lp)) {
			isp_del_wwn_entry(isp, mp->nt_channel, lp->port_wwn, lp->handle, lp->portid);
			return;
		}
	}
	if (mp->nt_wwn != INI_ANY) {
		if (isp_find_pdb_by_wwn(isp, mp->nt_channel, mp->nt_wwn, &lp)) {
			isp_del_wwn_entry(isp, mp->nt_channel, lp->port_wwn, lp->handle, lp->portid);
			return;
		}
	}
	if (mp->nt_sid != PORT_ANY && mp->nt_sid != PORT_NONE) {
		if (isp_find_pdb_by_sid(isp, mp->nt_channel, mp->nt_sid, &lp)) {
			isp_del_wwn_entry(isp, mp->nt_channel, lp->port_wwn, lp->handle, lp->portid);
			return;
		}
	}
	isp_prt(isp, ISP_LOGWARN, "%s: Chan %d unable to find entry to delete N-port handle 0x%04x initiator WWN 0x%016llx Port ID 0x%06x", __func__,
	    mp->nt_channel, mp->nt_nphdl, (unsigned long long) mp->nt_wwn, mp->nt_sid);
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
		ISP_IOXPUT_8(isp, src->at_scsi_status, &dst->at_scsi_status);
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
		ISP_IOXGET_8(isp, &src->at_scsi_status, dst->at_scsi_status);
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
		ISP_IOXPUT_16(isp, src->at_reserved2[i], &dst->at_reserved2[i]);
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
		ISP_IOXPUT_16(isp, src->at_reserved2[i], &dst->at_reserved2[i]);
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
		ISP_IOXGET_16(isp, &src->at_reserved2[i], dst->at_reserved2[i]);
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
		ISP_IOXGET_16(isp, &src->at_reserved2[i], dst->at_reserved2[i]);
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
		ISP_IOXPUT_32(isp, src->ct_dataseg[i].ds_base, &dst->ct_dataseg[i].ds_base);
		ISP_IOXPUT_32(isp, src->ct_dataseg[i].ds_count, &dst->ct_dataseg[i].ds_count);
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
		ISP_IOXGET_8(isp, &src->ct_scsi_status, dst->ct_scsi_status);
		ISP_IOXGET_8(isp, &src->ct_tag_val, dst->ct_tag_val);
		ISP_IOXGET_8(isp, &src->ct_tag_type, dst->ct_tag_type);
	}
	ISP_IOXGET_32(isp, &src->ct_flags, dst->ct_flags);
	ISP_IOXGET_32(isp, &src->ct_xfrlen, dst->ct_xfrlen);
	ISP_IOXGET_32(isp, &src->ct_resid, dst->ct_resid);
	ISP_IOXGET_16(isp, &src->ct_timeout, dst->ct_timeout);
	ISP_IOXGET_16(isp, &src->ct_seg_count, dst->ct_seg_count);
	for (i = 0; i < ISP_RQDSEG; i++) {
		ISP_IOXGET_32(isp, &src->ct_dataseg[i].ds_base, dst->ct_dataseg[i].ds_base);
		ISP_IOXGET_32(isp, &src->ct_dataseg[i].ds_count, dst->ct_dataseg[i].ds_count);
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
		ISP_IOXPUT_32(isp, src->rsp.m0._reserved, &dst->rsp.m0._reserved);
		ISP_IOXPUT_16(isp, src->rsp.m0._reserved2, &dst->rsp.m0._reserved2);
		ISP_IOXPUT_16(isp, src->rsp.m0.ct_scsi_status, &dst->rsp.m0.ct_scsi_status);
		ISP_IOXPUT_32(isp, src->rsp.m0.ct_xfrlen, &dst->rsp.m0.ct_xfrlen);
		if (src->ct_header.rqs_entry_type == RQSTYPE_CTIO2) {
			for (i = 0; i < ISP_RQDSEG_T2; i++) {
				ISP_IOXPUT_32(isp, src->rsp.m0.u.ct_dataseg[i].ds_base, &dst->rsp.m0.u.ct_dataseg[i].ds_base);
				ISP_IOXPUT_32(isp, src->rsp.m0.u.ct_dataseg[i].ds_count, &dst->rsp.m0.u.ct_dataseg[i].ds_count);
			}
		} else if (src->ct_header.rqs_entry_type == RQSTYPE_CTIO3) {
			for (i = 0; i < ISP_RQDSEG_T3; i++) {
				ISP_IOXPUT_32(isp, src->rsp.m0.u.ct_dataseg64[i].ds_base, &dst->rsp.m0.u.ct_dataseg64[i].ds_base);
				ISP_IOXPUT_32(isp, src->rsp.m0.u.ct_dataseg64[i].ds_basehi, &dst->rsp.m0.u.ct_dataseg64[i].ds_basehi);
				ISP_IOXPUT_32(isp, src->rsp.m0.u.ct_dataseg64[i].ds_count, &dst->rsp.m0.u.ct_dataseg64[i].ds_count);
			}
		} else if (src->ct_header.rqs_entry_type == RQSTYPE_CTIO4) {
			ISP_IOXPUT_16(isp, src->rsp.m0.u.ct_dslist.ds_type, &dst->rsp.m0.u.ct_dslist.ds_type); ISP_IOXPUT_32(isp, src->rsp.m0.u.ct_dslist.ds_segment,
			    &dst->rsp.m0.u.ct_dslist.ds_segment);
			ISP_IOXPUT_32(isp, src->rsp.m0.u.ct_dslist.ds_base, &dst->rsp.m0.u.ct_dslist.ds_base);
		}
	} else if ((src->ct_flags & CT2_FLAG_MMASK) == CT2_FLAG_MODE1) {
		ISP_IOXPUT_16(isp, src->rsp.m1._reserved, &dst->rsp.m1._reserved);
		ISP_IOXPUT_16(isp, src->rsp.m1._reserved2, &dst->rsp.m1._reserved2);
		ISP_IOXPUT_16(isp, src->rsp.m1.ct_senselen, &dst->rsp.m1.ct_senselen);
		ISP_IOXPUT_16(isp, src->rsp.m1.ct_scsi_status, &dst->rsp.m1.ct_scsi_status);
		ISP_IOXPUT_16(isp, src->rsp.m1.ct_resplen, &dst->rsp.m1.ct_resplen);
		for (i = 0; i < MAXRESPLEN; i++) {
			ISP_IOXPUT_8(isp, src->rsp.m1.ct_resp[i], &dst->rsp.m1.ct_resp[i]);
		}
	} else {
		ISP_IOXPUT_32(isp, src->rsp.m2._reserved, &dst->rsp.m2._reserved);
		ISP_IOXPUT_16(isp, src->rsp.m2._reserved2, &dst->rsp.m2._reserved2);
		ISP_IOXPUT_16(isp, src->rsp.m2._reserved3, &dst->rsp.m2._reserved3);
		ISP_IOXPUT_32(isp, src->rsp.m2.ct_datalen, &dst->rsp.m2.ct_datalen);
		ISP_IOXPUT_32(isp, src->rsp.m2.ct_fcp_rsp_iudata.ds_base, &dst->rsp.m2.ct_fcp_rsp_iudata.ds_base);
		ISP_IOXPUT_32(isp, src->rsp.m2.ct_fcp_rsp_iudata.ds_count, &dst->rsp.m2.ct_fcp_rsp_iudata.ds_count);
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
		ISP_IOXPUT_32(isp, src->rsp.m0._reserved, &dst->rsp.m0._reserved);
		ISP_IOXPUT_16(isp, src->rsp.m0._reserved2, &dst->rsp.m0._reserved2);
		ISP_IOXPUT_16(isp, src->rsp.m0.ct_scsi_status, &dst->rsp.m0.ct_scsi_status);
		ISP_IOXPUT_32(isp, src->rsp.m0.ct_xfrlen, &dst->rsp.m0.ct_xfrlen);
		if (src->ct_header.rqs_entry_type == RQSTYPE_CTIO2) {
			for (i = 0; i < ISP_RQDSEG_T2; i++) {
				ISP_IOXPUT_32(isp, src->rsp.m0.u.ct_dataseg[i].ds_base, &dst->rsp.m0.u.ct_dataseg[i].ds_base);
				ISP_IOXPUT_32(isp, src->rsp.m0.u.ct_dataseg[i].ds_count, &dst->rsp.m0.u.ct_dataseg[i].ds_count);
			}
		} else if (src->ct_header.rqs_entry_type == RQSTYPE_CTIO3) {
			for (i = 0; i < ISP_RQDSEG_T3; i++) {
				ISP_IOXPUT_32(isp, src->rsp.m0.u.ct_dataseg64[i].ds_base, &dst->rsp.m0.u.ct_dataseg64[i].ds_base);
				ISP_IOXPUT_32(isp, src->rsp.m0.u.ct_dataseg64[i].ds_basehi, &dst->rsp.m0.u.ct_dataseg64[i].ds_basehi);
				ISP_IOXPUT_32(isp, src->rsp.m0.u.ct_dataseg64[i].ds_count, &dst->rsp.m0.u.ct_dataseg64[i].ds_count);
			}
		} else if (src->ct_header.rqs_entry_type == RQSTYPE_CTIO4) {
			ISP_IOXPUT_16(isp, src->rsp.m0.u.ct_dslist.ds_type, &dst->rsp.m0.u.ct_dslist.ds_type);
			ISP_IOXPUT_32(isp, src->rsp.m0.u.ct_dslist.ds_segment, &dst->rsp.m0.u.ct_dslist.ds_segment);
			ISP_IOXPUT_32(isp, src->rsp.m0.u.ct_dslist.ds_base, &dst->rsp.m0.u.ct_dslist.ds_base);
		}
	} else if ((src->ct_flags & CT2_FLAG_MMASK) == CT2_FLAG_MODE1) {
		ISP_IOXPUT_16(isp, src->rsp.m1._reserved, &dst->rsp.m1._reserved);
		ISP_IOXPUT_16(isp, src->rsp.m1._reserved2, &dst->rsp.m1._reserved2);
		ISP_IOXPUT_16(isp, src->rsp.m1.ct_senselen, &dst->rsp.m1.ct_senselen);
		ISP_IOXPUT_16(isp, src->rsp.m1.ct_scsi_status, &dst->rsp.m1.ct_scsi_status);
		ISP_IOXPUT_16(isp, src->rsp.m1.ct_resplen, &dst->rsp.m1.ct_resplen);
		for (i = 0; i < MAXRESPLEN; i++) {
			ISP_IOXPUT_8(isp, src->rsp.m1.ct_resp[i], &dst->rsp.m1.ct_resp[i]);
		}
	} else {
		ISP_IOXPUT_32(isp, src->rsp.m2._reserved, &dst->rsp.m2._reserved);
		ISP_IOXPUT_16(isp, src->rsp.m2._reserved2, &dst->rsp.m2._reserved2);
		ISP_IOXPUT_16(isp, src->rsp.m2._reserved3, &dst->rsp.m2._reserved3);
		ISP_IOXPUT_32(isp, src->rsp.m2.ct_datalen, &dst->rsp.m2.ct_datalen);
		ISP_IOXPUT_32(isp, src->rsp.m2.ct_fcp_rsp_iudata.ds_base, &dst->rsp.m2.ct_fcp_rsp_iudata.ds_base);
		ISP_IOXPUT_32(isp, src->rsp.m2.ct_fcp_rsp_iudata.ds_count, &dst->rsp.m2.ct_fcp_rsp_iudata.ds_count);
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
		for (i = 0; i < (ASIZE(src->rsp.m1.ct_resp) >> 2); i++) {
			*b++ = ISP_SWAP32(isp, *a++);
		}
	} else {
		ISP_IOXPUT_32(isp, src->rsp.m2.reserved0, &dst->rsp.m2.reserved0);
		ISP_IOXPUT_32(isp, src->rsp.m2.ct_datalen, &dst->rsp.m2.ct_datalen);
		ISP_IOXPUT_32(isp, src->rsp.m2.reserved1, &dst->rsp.m2.reserved1);
		ISP_IOXPUT_32(isp, src->rsp.m2.ct_fcp_rsp_iudata.ds_base, &dst->rsp.m2.ct_fcp_rsp_iudata.ds_base);
		ISP_IOXPUT_32(isp, src->rsp.m2.ct_fcp_rsp_iudata.ds_basehi, &dst->rsp.m2.ct_fcp_rsp_iudata.ds_basehi);
		ISP_IOXPUT_32(isp, src->rsp.m2.ct_fcp_rsp_iudata.ds_count, &dst->rsp.m2.ct_fcp_rsp_iudata.ds_count);
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
		ISP_IOXGET_32(isp, &src->rsp.m0._reserved, dst->rsp.m0._reserved);
		ISP_IOXGET_16(isp, &src->rsp.m0._reserved2, dst->rsp.m0._reserved2);
		ISP_IOXGET_16(isp, &src->rsp.m0.ct_scsi_status, dst->rsp.m0.ct_scsi_status);
		ISP_IOXGET_32(isp, &src->rsp.m0.ct_xfrlen, dst->rsp.m0.ct_xfrlen);
		if (dst->ct_header.rqs_entry_type == RQSTYPE_CTIO2) {
			for (i = 0; i < ISP_RQDSEG_T2; i++) {
				ISP_IOXGET_32(isp, &src->rsp.m0.u.ct_dataseg[i].ds_base, dst->rsp.m0.u.ct_dataseg[i].ds_base);
				ISP_IOXGET_32(isp, &src->rsp.m0.u.ct_dataseg[i].ds_count, dst->rsp.m0.u.ct_dataseg[i].ds_count);
			}
		} else if (dst->ct_header.rqs_entry_type == RQSTYPE_CTIO3) {
			for (i = 0; i < ISP_RQDSEG_T3; i++) {
				ISP_IOXGET_32(isp, &src->rsp.m0.u.ct_dataseg64[i].ds_base, dst->rsp.m0.u.ct_dataseg64[i].ds_base);
				ISP_IOXGET_32(isp, &src->rsp.m0.u.ct_dataseg64[i].ds_basehi, dst->rsp.m0.u.ct_dataseg64[i].ds_basehi);
				ISP_IOXGET_32(isp, &src->rsp.m0.u.ct_dataseg64[i].ds_count, dst->rsp.m0.u.ct_dataseg64[i].ds_count);
			}
		} else if (dst->ct_header.rqs_entry_type == RQSTYPE_CTIO4) {
			ISP_IOXGET_16(isp, &src->rsp.m0.u.ct_dslist.ds_type, dst->rsp.m0.u.ct_dslist.ds_type);
			ISP_IOXGET_32(isp, &src->rsp.m0.u.ct_dslist.ds_segment, dst->rsp.m0.u.ct_dslist.ds_segment);
			ISP_IOXGET_32(isp, &src->rsp.m0.u.ct_dslist.ds_base, dst->rsp.m0.u.ct_dslist.ds_base);
		}
	} else if ((dst->ct_flags & CT2_FLAG_MMASK) == CT2_FLAG_MODE1) {
		ISP_IOXGET_16(isp, &src->rsp.m1._reserved, dst->rsp.m1._reserved);
		ISP_IOXGET_16(isp, &src->rsp.m1._reserved2, dst->rsp.m1._reserved2);
		ISP_IOXGET_16(isp, &src->rsp.m1.ct_senselen, dst->rsp.m1.ct_senselen);
		ISP_IOXGET_16(isp, &src->rsp.m1.ct_scsi_status, dst->rsp.m1.ct_scsi_status);
		ISP_IOXGET_16(isp, &src->rsp.m1.ct_resplen, dst->rsp.m1.ct_resplen);
		for (i = 0; i < MAXRESPLEN; i++) {
			ISP_IOXGET_8(isp, &src->rsp.m1.ct_resp[i], dst->rsp.m1.ct_resp[i]);
		}
	} else {
		ISP_IOXGET_32(isp, &src->rsp.m2._reserved, dst->rsp.m2._reserved);
		ISP_IOXGET_16(isp, &src->rsp.m2._reserved2, dst->rsp.m2._reserved2);
		ISP_IOXGET_16(isp, &src->rsp.m2._reserved3, dst->rsp.m2._reserved3);
		ISP_IOXGET_32(isp, &src->rsp.m2.ct_datalen, dst->rsp.m2.ct_datalen);
		ISP_IOXGET_32(isp, &src->rsp.m2.ct_fcp_rsp_iudata.ds_base, dst->rsp.m2.ct_fcp_rsp_iudata.ds_base);
		ISP_IOXGET_32(isp, &src->rsp.m2.ct_fcp_rsp_iudata.ds_count, dst->rsp.m2.ct_fcp_rsp_iudata.ds_count);
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
		ISP_IOXGET_32(isp, &src->rsp.m0._reserved, dst->rsp.m0._reserved);
		ISP_IOXGET_16(isp, &src->rsp.m0._reserved2, dst->rsp.m0._reserved2);
		ISP_IOXGET_16(isp, &src->rsp.m0.ct_scsi_status, dst->rsp.m0.ct_scsi_status);
		ISP_IOXGET_32(isp, &src->rsp.m0.ct_xfrlen, dst->rsp.m0.ct_xfrlen);
		if (src->ct_header.rqs_entry_type == RQSTYPE_CTIO2) {
			for (i = 0; i < ISP_RQDSEG_T2; i++) {
				ISP_IOXGET_32(isp, &src->rsp.m0.u.ct_dataseg[i].ds_base, dst->rsp.m0.u.ct_dataseg[i].ds_base);
				ISP_IOXGET_32(isp, &src->rsp.m0.u.ct_dataseg[i].ds_count, dst->rsp.m0.u.ct_dataseg[i].ds_count);
			}
		} else if (dst->ct_header.rqs_entry_type == RQSTYPE_CTIO3) {
			for (i = 0; i < ISP_RQDSEG_T3; i++) {
				ISP_IOXGET_32(isp, &src->rsp.m0.u.ct_dataseg64[i].ds_base, dst->rsp.m0.u.ct_dataseg64[i].ds_base);
				ISP_IOXGET_32(isp, &src->rsp.m0.u.ct_dataseg64[i].ds_basehi, dst->rsp.m0.u.ct_dataseg64[i].ds_basehi);
				ISP_IOXGET_32(isp, &src->rsp.m0.u.ct_dataseg64[i].ds_count, dst->rsp.m0.u.ct_dataseg64[i].ds_count);
			}
		} else if (dst->ct_header.rqs_entry_type == RQSTYPE_CTIO4) {
			ISP_IOXGET_16(isp, &src->rsp.m0.u.ct_dslist.ds_type, dst->rsp.m0.u.ct_dslist.ds_type);
			ISP_IOXGET_32(isp, &src->rsp.m0.u.ct_dslist.ds_segment, dst->rsp.m0.u.ct_dslist.ds_segment);
			ISP_IOXGET_32(isp, &src->rsp.m0.u.ct_dslist.ds_base, dst->rsp.m0.u.ct_dslist.ds_base);
		}
	} else if ((dst->ct_flags & CT2_FLAG_MMASK) == CT2_FLAG_MODE1) {
		ISP_IOXGET_16(isp, &src->rsp.m1._reserved, dst->rsp.m1._reserved);
		ISP_IOXGET_16(isp, &src->rsp.m1._reserved2, dst->rsp.m1._reserved2);
		ISP_IOXGET_16(isp, &src->rsp.m1.ct_senselen, dst->rsp.m1.ct_senselen);
		ISP_IOXGET_16(isp, &src->rsp.m1.ct_scsi_status, dst->rsp.m1.ct_scsi_status);
		ISP_IOXGET_16(isp, &src->rsp.m1.ct_resplen, dst->rsp.m1.ct_resplen);
		for (i = 0; i < MAXRESPLEN; i++) {
			ISP_IOXGET_8(isp, &src->rsp.m1.ct_resp[i], dst->rsp.m1.ct_resp[i]);
		}
	} else {
		ISP_IOXGET_32(isp, &src->rsp.m2._reserved, dst->rsp.m2._reserved);
		ISP_IOXGET_16(isp, &src->rsp.m2._reserved2, dst->rsp.m2._reserved2);
		ISP_IOXGET_16(isp, &src->rsp.m2._reserved3, dst->rsp.m2._reserved3);
		ISP_IOXGET_32(isp, &src->rsp.m2.ct_datalen, dst->rsp.m2.ct_datalen);
		ISP_IOXGET_32(isp, &src->rsp.m2.ct_fcp_rsp_iudata.ds_base, dst->rsp.m2.ct_fcp_rsp_iudata.ds_base);
		ISP_IOXGET_32(isp, &src->rsp.m2.ct_fcp_rsp_iudata.ds_count, dst->rsp.m2.ct_fcp_rsp_iudata.ds_count);
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
		for (i = 0; i < (ASIZE(src->rsp.m1.ct_resp) >> 2); i++) {
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
		ISP_IOXPUT_8(isp, lesrc->le_reserved3[i], &ledst->le_reserved3[i]);
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
		ISP_IOXGET_8(isp, &lesrc->le_reserved3[i], ledst->le_reserved3[i]);
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
		ISP_IOXPUT_8(isp, src->in_reserved3[i], &dst->in_reserved3[i]);
	}
	for (i = 0; i < QLTM_SENSELEN; i++) {
		ISP_IOXPUT_8(isp, src->in_sense[i], &dst->in_sense[i]);
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
		ISP_IOXGET_8(isp, &src->in_reserved3[i], dst->in_reserved3[i]);
	}
	for (i = 0; i < QLTM_SENSELEN; i++) {
		ISP_IOXGET_8(isp, &src->in_sense[i], dst->in_sense[i]);
	}
}

void
isp_put_notify_fc(ispsoftc_t *isp, in_fcentry_t *src, in_fcentry_t *dst)
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
isp_put_notify_fc_e(ispsoftc_t *isp, in_fcentry_e_t *src, in_fcentry_e_t *dst)
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
	ISP_IOXPUT_16(isp, src->in_reserved2, &dst->in_reserved2);
	ISP_IOXPUT_32(isp, src->in_rxid, &dst->in_rxid);
	ISP_IOXPUT_16(isp, src->in_srr_reloff_hi, &dst->in_srr_reloff_hi);
	ISP_IOXPUT_16(isp, src->in_srr_reloff_lo, &dst->in_srr_reloff_lo);
	ISP_IOXPUT_16(isp, src->in_srr_iu, &dst->in_srr_iu);
	ISP_IOXPUT_16(isp, src->in_srr_oxid, &dst->in_srr_oxid);
	ISP_IOXPUT_16(isp, src->in_nport_id_hi, &dst->in_nport_id_hi);
	ISP_IOXPUT_8(isp, src->in_nport_id_lo, &dst->in_nport_id_lo);
	ISP_IOXPUT_8(isp, src->in_reserved3, &dst->in_reserved3);
	ISP_IOXPUT_16(isp, src->in_np_handle, &dst->in_np_handle);
	for (i = 0; i < ASIZE(src->in_reserved4); i++) {
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
isp_get_notify_fc(ispsoftc_t *isp, in_fcentry_t *src, in_fcentry_t *dst)
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
isp_get_notify_fc_e(ispsoftc_t *isp, in_fcentry_e_t *src, in_fcentry_e_t *dst)
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
	ISP_IOXGET_16(isp, &src->in_reserved2, dst->in_reserved2);
	ISP_IOXGET_32(isp, &src->in_rxid, dst->in_rxid);
	ISP_IOXGET_16(isp, &src->in_srr_reloff_hi, dst->in_srr_reloff_hi);
	ISP_IOXGET_16(isp, &src->in_srr_reloff_lo, dst->in_srr_reloff_lo);
	ISP_IOXGET_16(isp, &src->in_srr_iu, dst->in_srr_iu);
	ISP_IOXGET_16(isp, &src->in_srr_oxid, dst->in_srr_oxid);
	ISP_IOXGET_16(isp, &src->in_nport_id_hi, dst->in_nport_id_hi);
	ISP_IOXGET_8(isp, &src->in_nport_id_lo, dst->in_nport_id_lo);
	ISP_IOXGET_8(isp, &src->in_reserved3, dst->in_reserved3);
	ISP_IOXGET_16(isp, &src->in_np_handle, dst->in_np_handle);
	for (i = 0; i < ASIZE(src->in_reserved4); i++) {
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
		ISP_IOXPUT_16(isp, src->na_reserved3[i], &dst->na_reserved3[i]);
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
		ISP_IOXGET_16(isp, &src->na_reserved3[i], dst->na_reserved3[i]);
	}
}

void
isp_put_notify_ack_fc(ispsoftc_t *isp, na_fcentry_t *src, na_fcentry_t *dst)
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
		ISP_IOXPUT_16(isp, src->na_reserved3[i], &dst->na_reserved3[i]);
	}
}

void
isp_put_notify_ack_fc_e(ispsoftc_t *isp, na_fcentry_e_t *src, na_fcentry_e_t *dst)
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
		ISP_IOXPUT_16(isp, src->na_reserved3[i], &dst->na_reserved3[i]);
	}
}

void
isp_put_notify_24xx_ack(ispsoftc_t *isp, na_fcentry_24xx_t *src, na_fcentry_24xx_t *dst)
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
isp_get_notify_ack_fc(ispsoftc_t *isp, na_fcentry_t *src, na_fcentry_t *dst)
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
		ISP_IOXGET_16(isp, &src->na_reserved3[i], dst->na_reserved3[i]);
	}
}

void
isp_get_notify_ack_fc_e(ispsoftc_t *isp, na_fcentry_e_t *src, na_fcentry_e_t *dst)
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
		ISP_IOXGET_16(isp, &src->na_reserved3[i], dst->na_reserved3[i]);
	}
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
#endif	/* ISP_TARGET_MODE */
/*
 * vim:ts=8:sw=8
 */
