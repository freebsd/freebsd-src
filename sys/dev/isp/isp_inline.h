/* $FreeBSD$ */
/*
 * Qlogic Host Adapter Inline Functions
 *
 * Copyright (c) 1999 by Matthew Jacob
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
#ifndef	_ISP_INLINE_H
#define	_ISP_INLINE_H

static INLINE void isp_prtstst __P((ispstatusreq_t *));
static INLINE char *isp2100_fw_statename __P((int));
static INLINE char *isp2100_pdb_statename __P((int));


static INLINE void
isp_prtstst(sp)
	ispstatusreq_t *sp;
{
	char buf[172];
	sprintf(buf, "states=>");
	if (sp->req_state_flags & RQSF_GOT_BUS) {
		STRNCAT(buf, " GOT_BUS", sizeof buf);
	}
	if (sp->req_state_flags & RQSF_GOT_TARGET) {
		STRNCAT(buf, " GOT_TGT", sizeof buf);
	}
	if (sp->req_state_flags & RQSF_SENT_CDB) {
		STRNCAT(buf, " SENT_CDB", sizeof buf);
	}
	if (sp->req_state_flags & RQSF_XFRD_DATA) {
		STRNCAT(buf, " XFRD_DATA", sizeof buf);
	}
	if (sp->req_state_flags & RQSF_GOT_STATUS) {
		STRNCAT(buf, " GOT_STS", sizeof buf);
	}
	if (sp->req_state_flags & RQSF_GOT_SENSE) {
		STRNCAT(buf, " GOT_SNS", sizeof buf);
	}
	if (sp->req_state_flags & RQSF_XFER_COMPLETE) {
		STRNCAT(buf, " XFR_CMPLT", sizeof buf);
	}
	STRNCAT(buf, "\nstatus=>", sizeof buf);
	if (sp->req_status_flags & RQSTF_DISCONNECT) {
		STRNCAT(buf, " Disconnect", sizeof buf);
	}
	if (sp->req_status_flags & RQSTF_SYNCHRONOUS) {
		STRNCAT(buf, " Sync_xfr", sizeof buf);
	}
	if (sp->req_status_flags & RQSTF_PARITY_ERROR) {
		STRNCAT(buf, " Parity", sizeof buf);
	}
	if (sp->req_status_flags & RQSTF_BUS_RESET) {
		STRNCAT(buf, " Bus_Reset", sizeof buf);
	}
	if (sp->req_status_flags & RQSTF_DEVICE_RESET) {
		STRNCAT(buf, " Device_Reset", sizeof buf);
	}
	if (sp->req_status_flags & RQSTF_ABORTED) {
		STRNCAT(buf, " Aborted", sizeof buf);
	}
	if (sp->req_status_flags & RQSTF_TIMEOUT) {
		STRNCAT(buf, " Timeout", sizeof buf);
	}
	if (sp->req_status_flags & RQSTF_NEGOTIATION) {
		STRNCAT(buf, " Negotiation", sizeof buf);
	}
	PRINTF(buf, "%s\n", buf);
}

static INLINE char *
isp2100_fw_statename(state)
	int state;
{
	static char buf[16];
	switch(state) {
	case FW_CONFIG_WAIT:	return "Config Wait";
	case FW_WAIT_AL_PA:	return "Waiting for AL_PA";
	case FW_WAIT_LOGIN:	return "Wait Login";
	case FW_READY:		return "Ready";
	case FW_LOSS_OF_SYNC:	return "Loss Of Sync";
	case FW_ERROR:		return "Error";
	case FW_REINIT:		return "Re-Init";
	case FW_NON_PART:	return "Nonparticipating";
	default:
		sprintf(buf, "0x%x", state);
		return buf;
	}
}

static INLINE char *isp2100_pdb_statename(int pdb_state)
{
	static char buf[16];
	switch(pdb_state) {
	case PDB_STATE_DISCOVERY:	return "Port Discovery";
	case PDB_STATE_WDISC_ACK:	return "Waiting Port Discovery ACK";
	case PDB_STATE_PLOGI:		return "Port Login";
	case PDB_STATE_PLOGI_ACK:	return "Wait Port Login ACK";
	case PDB_STATE_PRLI:		return "Process Login";
	case PDB_STATE_PRLI_ACK:	return "Wait Process Login ACK";
	case PDB_STATE_LOGGED_IN:	return "Logged In";
	case PDB_STATE_PORT_UNAVAIL:	return "Port Unavailable";
	case PDB_STATE_PRLO:		return "Process Logout";
	case PDB_STATE_PRLO_ACK:	return "Wait Process Logout ACK";
	case PDB_STATE_PLOGO:		return "Port Logout";
	case PDB_STATE_PLOG_ACK:	return "Wait Port Logout ACK";
	default:
		sprintf(buf, "0x%x", pdb_state);
		return buf;
	}
}

/*
 * Handle Functions.
 * For each outstanding command there will be a non-zero handle.
 * There will be at most isp_maxcmds handles, and isp_lasthdls
 * will be a seed for the last handled allocated.
 */

static INLINE int
isp_save_xs __P((struct ispsoftc *, ISP_SCSI_XFER_T *, u_int32_t *));

static INLINE ISP_SCSI_XFER_T *
isp_find_xs __P((struct ispsoftc *, u_int32_t));

static INLINE u_int32_t
isp_find_handle __P((struct ispsoftc *, ISP_SCSI_XFER_T *));

static INLINE int
isp_handle_index __P((u_int32_t));

static INLINE void
isp_destroy_handle __P((struct ispsoftc *, u_int32_t));

static INLINE void
isp_remove_handle __P((struct ispsoftc *, ISP_SCSI_XFER_T *));

static INLINE int
isp_save_xs(isp, xs, handlep)
	struct ispsoftc *isp;
	ISP_SCSI_XFER_T *xs;
	u_int32_t *handlep;
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
	isp->isp_lasthdls = j;
	return (0);
}

static INLINE ISP_SCSI_XFER_T *
isp_find_xs(isp, handle)
	struct ispsoftc *isp;
	u_int32_t handle;
{
	if (handle < 1 || handle > (u_int32_t) isp->isp_maxcmds) {
		return (NULL);
	} else {
		return (isp->isp_xflist[handle - 1]);
	}
}

static INLINE u_int32_t
isp_find_handle(isp, xs)
	struct ispsoftc *isp;
	ISP_SCSI_XFER_T *xs;
{
	int i;
	if (xs != NULL) {
		for (i = 0; i < isp->isp_maxcmds; i++) {
			if (isp->isp_xflist[i] == xs) {
				return ((u_int32_t) i+1);
			}
		}
	}
	return (0);
}

static INLINE int
isp_handle_index(handle)
	u_int32_t handle;
{
	return (handle-1);
}

static INLINE void
isp_destroy_handle(isp, handle)
	struct ispsoftc *isp;
	u_int32_t handle;
{
	if (handle > 0 && handle <= (u_int32_t) isp->isp_maxcmds) {
		isp->isp_xflist[isp_handle_index(handle)] = NULL;
	}
}

static INLINE void
isp_remove_handle(isp, xs)
	struct ispsoftc *isp;
	ISP_SCSI_XFER_T *xs;
{
	isp_destroy_handle(isp, isp_find_handle(isp, xs));
}

static INLINE int
isp_getrqentry __P((struct ispsoftc *, u_int16_t *, u_int16_t *, void **));

static INLINE int
isp_getrqentry(isp, iptrp, optrp, resultp)
	struct ispsoftc *isp;
	u_int16_t *iptrp;
	u_int16_t *optrp;
	void **resultp;
{
	volatile u_int16_t iptr, optr;

	optr = isp->isp_reqodx = ISP_READ(isp, OUTMAILBOX4);
	iptr = isp->isp_reqidx;
	*resultp = ISP_QUEUE_ENTRY(isp->isp_rquest, iptr);
	iptr = ISP_NXT_QENTRY(iptr, RQUEST_QUEUE_LEN);
	if (iptr == optr) {
		return (1);
	}
	if (optrp)
		*optrp = optr;
	if (iptrp)
		*iptrp = iptr;
	return (0);
}

static INLINE void
isp_print_qentry __P((struct ispsoftc *, char *, int, void *));

static INLINE void
isp_print_qentry(isp, msg, idx, arg)
	struct ispsoftc *isp;
	char *msg;
	int idx;
	void *arg;
{
	int amt, i, j;
	u_int8_t *ptr = arg;
	PRINTF("%s %s index %d:\n", isp->isp_name, msg, idx);
	for (amt = i = 0; i < 4; i++) {
		for (j = 0; j < (QENTRY_LEN >> 2); j++) {
			PRINTF(" %02x", ptr[amt++] & 0xff);
		}
		PRINTF("\n");
	}
}
#endif	/* _ISP_INLINE_H */
