/* $FreeBSD$ */
/*
 * Machine and OS Independent Target Mode Code for the Qlogic SCSI/FC adapters.
 *
 * Copyright (c) 1999, 2000, 2001 by Matthew Jacob
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
 */

/*
 * Bug fixes gratefully acknowledged from:
 *	Oded Kedem <oded@kashya.com>
 */
/*
 * Include header file appropriate for platform we're building on.
 */

#ifdef	__NetBSD__
#include <dev/ic/isp_netbsd.h>
#endif
#ifdef	__FreeBSD__
#include <dev/isp/isp_freebsd.h>
#endif
#ifdef	__OpenBSD__
#include <dev/ic/isp_openbsd.h>
#endif
#ifdef	__linux__
#include "isp_linux.h"
#endif

#ifdef	ISP_TARGET_MODE
static const char atiocope[] =
    "ATIO returned for lun %d because it was in the middle of Bus Device Reset "
    "on bus %d";
static const char atior[] =
    "ATIO returned on for lun %d on from IID %d because a Bus Reset occurred "
    "on bus %d";

static void isp_got_msg(struct ispsoftc *, int, in_entry_t *);
static void isp_got_msg_fc(struct ispsoftc *, int, in_fcentry_t *);
static void isp_notify_ack(struct ispsoftc *, void *);
static void isp_handle_atio(struct ispsoftc *, at_entry_t *);
static void isp_handle_atio2(struct ispsoftc *, at2_entry_t *);
static void isp_handle_ctio(struct ispsoftc *, ct_entry_t *);
static void isp_handle_ctio2(struct ispsoftc *, ct2_entry_t *);

/*
 * The Qlogic driver gets an interrupt to look at response queue entries.
 * Some of these are status completions for initiatior mode commands, but
 * if target mode is enabled, we get a whole wad of response queue entries
 * to be handled here.
 *
 * Basically the split into 3 main groups: Lun Enable/Modification responses,
 * SCSI Command processing, and Immediate Notification events.
 *
 * You start by writing a request queue entry to enable target mode (and
 * establish some resource limitations which you can modify later).
 * The f/w responds with a LUN ENABLE or LUN MODIFY response with
 * the status of this action. If the enable was successful, you can expect...
 *
 * Response queue entries with SCSI commands encapsulate show up in an ATIO
 * (Accept Target IO) type- sometimes with enough info to stop the command at
 * this level. Ultimately the driver has to feed back to the f/w's request
 * queue a sequence of CTIOs (continue target I/O) that describe data to
 * be moved and/or status to be sent) and finally finishing with sending
 * to the f/w's response queue an ATIO which then completes the handshake
 * with the f/w for that command. There's a lot of variations on this theme,
 * including flags you can set in the CTIO for the Qlogic 2X00 fibre channel
 * cards that 'auto-replenish' the f/w's ATIO count, but this is the basic
 * gist of it.
 *
 * The third group that can show up in the response queue are Immediate
 * Notification events. These include things like notifications of SCSI bus
 * resets, or Bus Device Reset messages or other messages received. This
 * a classic oddbins area. It can get  a little weird because you then turn
 * around and acknowledge the Immediate Notify by writing an entry onto the
 * request queue and then the f/w turns around and gives you an acknowledgement
 * to *your* acknowledgement on the response queue (the idea being to let
 * the f/w tell you when the event is *really* over I guess).
 *
 */


/*
 * A new response queue entry has arrived. The interrupt service code
 * has already swizzled it into the platform dependent from canonical form.
 *
 * Because of the way this driver is designed, unfortunately most of the
 * actual synchronization work has to be done in the platform specific
 * code- we have no synchroniation primitives in the common code.
 */

int
isp_target_notify(struct ispsoftc *isp, void *vptr, u_int16_t *optrp)
{
	u_int16_t status, seqid;
	union {
		at_entry_t	*atiop;
		at2_entry_t	*at2iop;
		ct_entry_t	*ctiop;
		ct2_entry_t	*ct2iop;
		lun_entry_t	*lunenp;
		in_entry_t	*inotp;
		in_fcentry_t	*inot_fcp;
		na_entry_t	*nackp;
		na_fcentry_t	*nack_fcp;
		isphdr_t	*hp;
		void *		*vp;
#define	atiop		unp.atiop
#define	at2iop		unp.at2iop
#define	ctiop		unp.ctiop
#define	ct2iop		unp.ct2iop
#define	lunenp		unp.lunenp
#define	inotp		unp.inotp
#define	inot_fcp	unp.inot_fcp
#define	nackp		unp.nackp
#define	nack_fcp	unp.nack_fcp
#define	hdrp		unp.hp
	} unp;
	u_int8_t local[QENTRY_LEN];
	int bus, type, rval = 1;

	type = isp_get_response_type(isp, (isphdr_t *)vptr);
	unp.vp = vptr;

	ISP_TDQE(isp, "isp_target_notify", (int) *optrp, vptr);

	switch(type) {
	case RQSTYPE_ATIO:
		isp_get_atio(isp, atiop, (at_entry_t *) local);
		isp_handle_atio(isp, (at_entry_t *) local);
		break;
	case RQSTYPE_CTIO:
		isp_get_ctio(isp, ctiop, (ct_entry_t *) local);
		isp_handle_ctio(isp, (ct_entry_t *) local);
		break;
	case RQSTYPE_ATIO2:
		isp_get_atio2(isp, at2iop, (at2_entry_t *) local);
		isp_handle_atio2(isp, (at2_entry_t *) local);
		break;
	case RQSTYPE_CTIO2:
		isp_get_ctio2(isp, ct2iop, (ct2_entry_t *) local);
		isp_handle_ctio2(isp, (ct2_entry_t *) local);
		break;
	case RQSTYPE_ENABLE_LUN:
	case RQSTYPE_MODIFY_LUN:
		isp_get_enable_lun(isp, lunenp, (lun_entry_t *) local);
		(void) isp_async(isp, ISPASYNC_TARGET_ACTION, local);
		break;

	case RQSTYPE_NOTIFY:
		/*
		 * Either the ISP received a SCSI message it can't
		 * handle, or it's returning an Immed. Notify entry
		 * we sent. We can send Immed. Notify entries to
		 * increment the firmware's resource count for them
		 * (we set this initially in the Enable Lun entry).
		 */
		bus = 0;
		if (IS_FC(isp)) {
			isp_get_notify_fc(isp, inot_fcp, (in_fcentry_t *)local);
			inot_fcp = (in_fcentry_t *) local;
			status = inot_fcp->in_status;
			seqid = inot_fcp->in_seqid;
		} else {
			isp_get_notify(isp, inotp, (in_entry_t *)local);
			inotp = (in_entry_t *) local;
			status = inotp->in_status & 0xff;
			seqid = inotp->in_seqid;
			if (IS_DUALBUS(isp)) {
				bus = GET_BUS_VAL(inotp->in_iid);
				SET_BUS_VAL(inotp->in_iid, 0);
			}
		}
		isp_prt(isp, ISP_LOGTDEBUG0,
		    "Immediate Notify On Bus %d, status=0x%x seqid=0x%x",
		    bus, status, seqid);

		/*
		 * ACK it right away.
		 */
		isp_notify_ack(isp, (status == IN_RESET)? NULL : local);
		switch (status) {
		case IN_RESET:
			(void) isp_async(isp, ISPASYNC_BUS_RESET, &bus);
			break;
		case IN_MSG_RECEIVED:
		case IN_IDE_RECEIVED:
			if (IS_FC(isp)) {
				isp_got_msg_fc(isp, bus, (in_fcentry_t *)local);
			} else {
				isp_got_msg(isp, bus, (in_entry_t *)local);
			}
			break;
		case IN_RSRC_UNAVAIL:
			isp_prt(isp, ISP_LOGWARN, "Firmware out of ATIOs");
			break;
		case IN_PORT_LOGOUT:
		case IN_ABORT_TASK:
		case IN_PORT_CHANGED:
		case IN_GLOBAL_LOGO:
			(void) isp_async(isp, ISPASYNC_TARGET_ACTION, &local);
			break;
		default:
			isp_prt(isp, ISP_LOGERR,
			    "bad status (0x%x) in isp_target_notify", status);
			break;
		}
		break;

	case RQSTYPE_NOTIFY_ACK:
		/*
		 * The ISP is acknowledging our acknowledgement of an
		 * Immediate Notify entry for some asynchronous event.
		 */
		if (IS_FC(isp)) {
			isp_get_notify_ack_fc(isp, nack_fcp,
			    (na_fcentry_t *)local);
			nack_fcp = (na_fcentry_t *)local;
			isp_prt(isp, ISP_LOGTDEBUG1,
			    "Notify Ack status=0x%x seqid 0x%x",
			    nack_fcp->na_status, nack_fcp->na_seqid);
		} else {
			isp_get_notify_ack(isp, nackp, (na_entry_t *)local);
			nackp = (na_entry_t *)local;
			isp_prt(isp, ISP_LOGTDEBUG1,
			    "Notify Ack event 0x%x status=0x%x seqid 0x%x",
			    nackp->na_event, nackp->na_status, nackp->na_seqid);
		}
		break;
	default:
		isp_prt(isp, ISP_LOGERR,
		    "Unknown entry type 0x%x in isp_target_notify", type);
		rval = 0;
		break;
	}
#undef	atiop
#undef	at2iop
#undef	ctiop
#undef	ct2iop
#undef	lunenp
#undef	inotp
#undef	inot_fcp
#undef	nackp
#undef	nack_fcp
#undef	hdrp
	return (rval);
}

 
/*
 * Toggle (on/off) target mode for bus/target/lun
 *
 * The caller has checked for overlap and legality.
 *
 * Note that not all of bus, target or lun can be paid attention to.
 * Note also that this action will not be complete until the f/w writes
 * response entry. The caller is responsible for synchronizing this.
 */
int
isp_lun_cmd(struct ispsoftc *isp, int cmd, int bus, int tgt, int lun,
    int cmd_cnt, int inot_cnt, u_int32_t opaque)
{
	lun_entry_t el;
	u_int16_t nxti, optr;
	void *outp;


	MEMZERO(&el, sizeof (el));
	if (IS_DUALBUS(isp)) {
		el.le_rsvd = (bus & 0x1) << 7;
	}
	el.le_cmd_count = cmd_cnt;
	el.le_in_count = inot_cnt;
	if (cmd == RQSTYPE_ENABLE_LUN) {
		if (IS_SCSI(isp)) {
			el.le_flags = LUN_TQAE|LUN_DISAD;
			el.le_cdb6len = 12;
			el.le_cdb7len = 12;
		}
	} else if (cmd == -RQSTYPE_ENABLE_LUN) {
		cmd = RQSTYPE_ENABLE_LUN;
		el.le_cmd_count = 0;
		el.le_in_count = 0;
	} else if (cmd == -RQSTYPE_MODIFY_LUN) {
		cmd = RQSTYPE_MODIFY_LUN;
		el.le_ops = LUN_CCDECR | LUN_INDECR;
	} else {
		el.le_ops = LUN_CCINCR | LUN_ININCR;
	}
	el.le_header.rqs_entry_type = cmd;
	el.le_header.rqs_entry_count = 1;
	el.le_reserved = opaque;
	if (IS_SCSI(isp)) {
		el.le_tgt = tgt;
		el.le_lun = lun;
	} else if ((FCPARAM(isp)->isp_fwattr & ISP_FW_ATTR_SCCLUN) == 0) {
		el.le_lun = lun;
	}
	el.le_timeout = 2;

	if (isp_getrqentry(isp, &nxti, &optr, &outp)) {
		isp_prt(isp, ISP_LOGERR,
		    "Request Queue Overflow in isp_lun_cmd");
		return (-1);
	}
	ISP_TDQE(isp, "isp_lun_cmd", (int) optr, &el);
	isp_put_enable_lun(isp, &el, outp);
	ISP_ADD_REQUEST(isp, nxti);
	return (0);
}


int
isp_target_put_entry(struct ispsoftc *isp, void *ap)
{
	void *outp;
	u_int16_t nxti, optr;
	u_int8_t etype = ((isphdr_t *) ap)->rqs_entry_type;

	if (isp_getrqentry(isp, &nxti, &optr, &outp)) {
		isp_prt(isp, ISP_LOGWARN,
		    "Request Queue Overflow in isp_target_put_entry");
		return (-1);
	}
	switch (etype) {
	case RQSTYPE_ATIO:
		isp_put_atio(isp, (at_entry_t *) ap, (at_entry_t *) outp);
		break;
	case RQSTYPE_ATIO2:
		isp_put_atio2(isp, (at2_entry_t *) ap, (at2_entry_t *) outp);
		break;
	case RQSTYPE_CTIO:
		isp_put_ctio(isp, (ct_entry_t *) ap, (ct_entry_t *) outp);
		break;
	case RQSTYPE_CTIO2:
		isp_put_ctio2(isp, (ct2_entry_t *) ap, (ct2_entry_t *) outp);
		break;
	default:
		isp_prt(isp, ISP_LOGERR,
		    "Unknown type 0x%x in isp_put_entry", etype);
		return (-1);
	}

	ISP_TDQE(isp, "isp_target_put_entry", (int) optr, ap);;
	ISP_ADD_REQUEST(isp, nxti);
	return (0);
}

int
isp_target_put_atio(struct ispsoftc *isp, void *arg)
{
	union {
		at_entry_t _atio;
		at2_entry_t _atio2;
	} atun;

	MEMZERO(&atun, sizeof atun);
	if (IS_FC(isp)) {
		at2_entry_t *aep = arg;
		atun._atio2.at_header.rqs_entry_type = RQSTYPE_ATIO2;
		atun._atio2.at_header.rqs_entry_count = 1;
		if (FCPARAM(isp)->isp_fwattr & ISP_FW_ATTR_SCCLUN) {
			atun._atio2.at_scclun = (u_int16_t) aep->at_scclun;
		} else {
			atun._atio2.at_lun = (u_int8_t) aep->at_lun;
		}
		atun._atio2.at_iid = aep->at_iid;
		atun._atio2.at_rxid = aep->at_rxid;
		atun._atio2.at_status = CT_OK;
	} else {
		at_entry_t *aep = arg;
		atun._atio.at_header.rqs_entry_type = RQSTYPE_ATIO;
		atun._atio.at_header.rqs_entry_count = 1;
		atun._atio.at_handle = aep->at_handle;
		atun._atio.at_iid = aep->at_iid;
		atun._atio.at_tgt = aep->at_tgt;
		atun._atio.at_lun = aep->at_lun;
		atun._atio.at_tag_type = aep->at_tag_type;
		atun._atio.at_tag_val = aep->at_tag_val;
		atun._atio.at_status = (aep->at_flags & AT_TQAE);
		atun._atio.at_status |= CT_OK;
	}
	return (isp_target_put_entry(isp, &atun));
}

/*
 * Command completion- both for handling cases of no resources or
 * no blackhole driver, or other cases where we have to, inline,
 * finish the command sanely, or for normal command completion.
 *
 * The 'completion' code value has the scsi status byte in the low 8 bits.
 * If status is a CHECK CONDITION and bit 8 is nonzero, then bits 12..15 have
 * the sense key and  bits 16..23 have the ASCQ and bits 24..31 have the ASC
 * values.
 *
 * NB: the key, asc, ascq, cannot be used for parallel SCSI as it doesn't
 * NB: inline SCSI sense reporting. As such, we lose this information. XXX.
 *
 * For both parallel && fibre channel, we use the feature that does
 * an automatic resource autoreplenish so we don't have then later do
 * put of an atio to replenish the f/w's resource count.
 */

int
isp_endcmd(struct ispsoftc *isp, void *arg, u_int32_t code, u_int16_t hdl)
{
	int sts;
	union {
		ct_entry_t _ctio;
		ct2_entry_t _ctio2;
	} un;

	MEMZERO(&un, sizeof un);
	sts = code & 0xff;

	if (IS_FC(isp)) {
		at2_entry_t *aep = arg;
		ct2_entry_t *cto = &un._ctio2;

		cto->ct_header.rqs_entry_type = RQSTYPE_CTIO2;
		cto->ct_header.rqs_entry_count = 1;
		cto->ct_iid = aep->at_iid;
		if ((FCPARAM(isp)->isp_fwattr & ISP_FW_ATTR_SCCLUN) == 0) {
			cto->ct_lun = aep->at_lun;
		}
		cto->ct_rxid = aep->at_rxid;
		cto->rsp.m1.ct_scsi_status = sts & 0xff;
		cto->ct_flags = CT2_SENDSTATUS | CT2_NO_DATA | CT2_FLAG_MODE1;
		if (hdl == 0) {
			cto->ct_flags |= CT2_CCINCR;
		}
		if (aep->at_datalen) {
			cto->ct_resid = aep->at_datalen;
			cto->rsp.m1.ct_scsi_status |= CT2_DATA_UNDER;
		}
		if ((sts & 0xff) == SCSI_CHECK && (sts & ECMD_SVALID)) {
			cto->rsp.m1.ct_resp[0] = 0xf0;
			cto->rsp.m1.ct_resp[2] = (code >> 12) & 0xf;
			cto->rsp.m1.ct_resp[7] = 8;
			cto->rsp.m1.ct_resp[12] = (code >> 24) & 0xff;
			cto->rsp.m1.ct_resp[13] = (code >> 16) & 0xff;
			cto->rsp.m1.ct_senselen = 16;
			cto->rsp.m1.ct_scsi_status |= CT2_SNSLEN_VALID;
		}
		cto->ct_syshandle = hdl;
	} else {
		at_entry_t *aep = arg;
		ct_entry_t *cto = &un._ctio;

		cto->ct_header.rqs_entry_type = RQSTYPE_CTIO;
		cto->ct_header.rqs_entry_count = 1;
		cto->ct_fwhandle = aep->at_handle;
		cto->ct_iid = aep->at_iid;
		cto->ct_tgt = aep->at_tgt;
		cto->ct_lun = aep->at_lun;
		cto->ct_tag_type = aep->at_tag_type;
		cto->ct_tag_val = aep->at_tag_val;
		if (aep->at_flags & AT_TQAE) {
			cto->ct_flags |= CT_TQAE;
		}
		cto->ct_flags = CT_SENDSTATUS | CT_NO_DATA;
		if (hdl == 0) {
			cto->ct_flags |= CT_CCINCR;
		}
		cto->ct_scsi_status = sts;
		cto->ct_syshandle = hdl;
	}
	return (isp_target_put_entry(isp, &un));
}

int
isp_target_async(struct ispsoftc *isp, int bus, int event)
{
	tmd_event_t evt;
	tmd_msg_t msg;

	switch (event) {
	/*
	 * These three we handle here to propagate an effective bus reset
	 * upstream, but these do not require any immediate notify actions
	 * so we return when done.
	 */
	case ASYNC_LIP_F8:
	case ASYNC_LIP_OCCURRED:
	case ASYNC_LOOP_UP:
	case ASYNC_LOOP_DOWN:
	case ASYNC_LOOP_RESET:
	case ASYNC_PTPMODE:
		/*
		 * These don't require any immediate notify actions. We used
		 * treat them like SCSI Bus Resets, but that was just plain
		 * wrong. Let the normal CTIO completion report what occurred.
		 */
                return (0);

	case ASYNC_BUS_RESET:
	case ASYNC_TIMEOUT_RESET:
		if (IS_FC(isp)) {
			return (0); /* we'll be getting an inotify instead */
		}
		evt.ev_bus = bus;
		evt.ev_event = event;
		(void) isp_async(isp, ISPASYNC_TARGET_EVENT, &evt);
		break;
	case ASYNC_DEVICE_RESET:
		/*
		 * Bus Device Reset resets a specific target, so
		 * we pass this as a synthesized message.
		 */
		MEMZERO(&msg, sizeof msg);
		if (IS_FC(isp)) {
			msg.nt_iid = FCPARAM(isp)->isp_loopid;
		} else {
			msg.nt_iid = SDPARAM(isp)->isp_initiator_id;
		}
		msg.nt_bus = bus;
		msg.nt_msg[0] = MSG_BUS_DEV_RESET;
		(void) isp_async(isp, ISPASYNC_TARGET_MESSAGE, &msg);
		break;
	default:
		isp_prt(isp, ISP_LOGERR,
		    "isp_target_async: unknown event 0x%x", event);
		break;
	}
	if (isp->isp_state == ISP_RUNSTATE)
		isp_notify_ack(isp, NULL);
	return(0);
}


/*
 * Process a received message.
 * The ISP firmware can handle most messages, there are only
 * a few that we need to deal with:
 * - abort: clean up the current command
 * - abort tag and clear queue
 */

static void
isp_got_msg(struct ispsoftc *isp, int bus, in_entry_t *inp)
{
	u_int8_t status = inp->in_status & ~QLTM_SVALID;

	if (status == IN_IDE_RECEIVED || status == IN_MSG_RECEIVED) {
		tmd_msg_t msg;

		MEMZERO(&msg, sizeof (msg));
		msg.nt_bus = bus;
		msg.nt_iid = inp->in_iid;
		msg.nt_tgt = inp->in_tgt;
		msg.nt_lun = inp->in_lun;
		msg.nt_tagtype = inp->in_tag_type;
		msg.nt_tagval = inp->in_tag_val;
		MEMCPY(msg.nt_msg, inp->in_msg, IN_MSGLEN);
		(void) isp_async(isp, ISPASYNC_TARGET_MESSAGE, &msg);
	} else {
		isp_prt(isp, ISP_LOGERR,
		    "unknown immediate notify status 0x%x", inp->in_status);
	}
}

/*
 * Synthesize a message from the task management flags in a FCP_CMND_IU.
 */
static void
isp_got_msg_fc(struct ispsoftc *isp, int bus, in_fcentry_t *inp)
{
	int lun;
	static const char f1[] = "%s from iid %d lun %d seq 0x%x";
	static const char f2[] = 
	    "unknown %s 0x%x lun %d iid %d task flags 0x%x seq 0x%x\n";

	if (FCPARAM(isp)->isp_fwattr & ISP_FW_ATTR_SCCLUN) {
		lun = inp->in_scclun;
	} else {
		lun = inp->in_lun;
	}

	if (inp->in_status != IN_MSG_RECEIVED) {
		isp_prt(isp, ISP_LOGINFO, f2, "immediate notify status",
		    inp->in_status, lun, inp->in_iid,
		    inp->in_task_flags,  inp->in_seqid);
	} else {
		tmd_msg_t msg;

		MEMZERO(&msg, sizeof (msg));
		msg.nt_bus = bus;
		msg.nt_iid = inp->in_iid;
		msg.nt_tagval = inp->in_seqid;
		msg.nt_lun = lun;

		if (inp->in_task_flags & TASK_FLAGS_ABORT_TASK) {
			isp_prt(isp, ISP_LOGINFO, f1, "ABORT TASK",
			    inp->in_iid, lun, inp->in_seqid);
			msg.nt_msg[0] = MSG_ABORT_TAG;
		} else if (inp->in_task_flags & TASK_FLAGS_CLEAR_TASK_SET) {
			isp_prt(isp, ISP_LOGINFO, f1, "CLEAR TASK SET",
			    inp->in_iid, lun, inp->in_seqid);
			msg.nt_msg[0] = MSG_CLEAR_QUEUE;
		} else if (inp->in_task_flags & TASK_FLAGS_TARGET_RESET) {
			isp_prt(isp, ISP_LOGINFO, f1, "TARGET RESET",
			    inp->in_iid, lun, inp->in_seqid);
			msg.nt_msg[0] = MSG_BUS_DEV_RESET;
		} else if (inp->in_task_flags & TASK_FLAGS_CLEAR_ACA) {
			isp_prt(isp, ISP_LOGINFO, f1, "CLEAR ACA",
			    inp->in_iid, lun, inp->in_seqid);
			/* ???? */
			msg.nt_msg[0] = MSG_REL_RECOVERY;
		} else if (inp->in_task_flags & TASK_FLAGS_TERMINATE_TASK) {
			isp_prt(isp, ISP_LOGINFO, f1, "TERMINATE TASK",
			    inp->in_iid, lun, inp->in_seqid);
			msg.nt_msg[0] = MSG_TERM_IO_PROC;
		} else {
			isp_prt(isp, ISP_LOGWARN, f2, "task flag",
			    inp->in_status, lun, inp->in_iid,
			    inp->in_task_flags,  inp->in_seqid);
		}
		if (msg.nt_msg[0]) {
			(void) isp_async(isp, ISPASYNC_TARGET_MESSAGE, &msg);
		}
	}
}

static void
isp_notify_ack(struct ispsoftc *isp, void *arg)
{
	char storage[QENTRY_LEN];
	u_int16_t nxti, optr;
	void *outp;

	if (isp_getrqentry(isp, &nxti, &optr, &outp)) {
		isp_prt(isp, ISP_LOGWARN,
		    "Request Queue Overflow For isp_notify_ack");
		return;
	}

	MEMZERO(storage, QENTRY_LEN);

	if (IS_FC(isp)) {
		na_fcentry_t *na = (na_fcentry_t *) storage;
		if (arg) {
			in_fcentry_t *inp = arg;
			MEMCPY(storage, arg, sizeof (isphdr_t));
			na->na_iid = inp->in_iid;
			if (FCPARAM(isp)->isp_fwattr & ISP_FW_ATTR_SCCLUN) {
				na->na_lun = inp->in_scclun;
			} else {
				na->na_lun = inp->in_lun;
			}
			na->na_task_flags = inp->in_task_flags;
			na->na_seqid = inp->in_seqid;
			na->na_flags = NAFC_RCOUNT;
			na->na_status = inp->in_status;
			if (inp->in_status == IN_RESET) {
				na->na_flags |= NAFC_RST_CLRD;
			}
		} else {
			na->na_flags = NAFC_RST_CLRD;
		}
		na->na_header.rqs_entry_type = RQSTYPE_NOTIFY_ACK;
		na->na_header.rqs_entry_count = 1;
		isp_put_notify_ack_fc(isp, na, (na_fcentry_t *)outp);
	} else {
		na_entry_t *na = (na_entry_t *) storage;
		if (arg) {
			in_entry_t *inp = arg;
			MEMCPY(storage, arg, sizeof (isphdr_t));
			na->na_iid = inp->in_iid;
			na->na_lun = inp->in_lun;
			na->na_tgt = inp->in_tgt;
			na->na_seqid = inp->in_seqid;
			if (inp->in_status == IN_RESET) {
				na->na_event = NA_RST_CLRD;
			}
		} else {
			na->na_event = NA_RST_CLRD;
		}
		na->na_header.rqs_entry_type = RQSTYPE_NOTIFY_ACK;
		na->na_header.rqs_entry_count = 1;
		isp_put_notify_ack(isp, na, (na_entry_t *)outp);
	}
	ISP_TDQE(isp, "isp_notify_ack", (int) optr, storage);
	ISP_ADD_REQUEST(isp, nxti);
}

static void
isp_handle_atio(struct ispsoftc *isp, at_entry_t *aep)
{
	int lun;
	lun = aep->at_lun;
	/*
	 * The firmware status (except for the QLTM_SVALID bit) indicates
	 * why this ATIO was sent to us.
	 *
	 * If QLTM_SVALID is set, the firware has recommended Sense Data.
	 *
	 * If the DISCONNECTS DISABLED bit is set in the flags field,
	 * we're still connected on the SCSI bus - i.e. the initiator
	 * did not set DiscPriv in the identify message. We don't care
	 * about this so it's ignored.
	 */

	switch(aep->at_status & ~QLTM_SVALID) {
	case AT_PATH_INVALID:
		/*
		 * ATIO rejected by the firmware due to disabled lun.
		 */
		isp_prt(isp, ISP_LOGERR,
		    "rejected ATIO for disabled lun %d", lun);
		break;
	case AT_NOCAP:
		/*
		 * Requested Capability not available
		 * We sent an ATIO that overflowed the firmware's
		 * command resource count.
		 */
		isp_prt(isp, ISP_LOGERR,
		    "rejected ATIO for lun %d because of command count"
		    " overflow", lun);
		break;

	case AT_BDR_MSG:
		/*
		 * If we send an ATIO to the firmware to increment
		 * its command resource count, and the firmware is
		 * recovering from a Bus Device Reset, it returns
		 * the ATIO with this status. We set the command
		 * resource count in the Enable Lun entry and do
		 * not increment it. Therefore we should never get
		 * this status here.
		 */
		isp_prt(isp, ISP_LOGERR, atiocope, lun,
		    GET_BUS_VAL(aep->at_iid));
		break;

	case AT_CDB:		/* Got a CDB */
	case AT_PHASE_ERROR:	/* Bus Phase Sequence Error */
		/*
		 * Punt to platform specific layer.
		 */
		(void) isp_async(isp, ISPASYNC_TARGET_ACTION, aep);
		break;

	case AT_RESET:
		/*
		 * A bus reset came along and blew away this command. Why
		 * they do this in addition the async event code stuff,
		 * I dunno.
		 *
		 * Ignore it because the async event will clear things
		 * up for us.
		 */
		isp_prt(isp, ISP_LOGWARN, atior, lun,
		    GET_IID_VAL(aep->at_iid), GET_BUS_VAL(aep->at_iid));
		break;


	default:
		isp_prt(isp, ISP_LOGERR,
		    "Unknown ATIO status 0x%x from initiator %d for lun %d",
		    aep->at_status, aep->at_iid, lun);
		(void) isp_target_put_atio(isp, aep);
		break;
	}
}

static void
isp_handle_atio2(struct ispsoftc *isp, at2_entry_t *aep)
{
	int lun;

	if (FCPARAM(isp)->isp_fwattr & ISP_FW_ATTR_SCCLUN) {
		lun = aep->at_scclun;
	} else {
		lun = aep->at_lun;
	}

	/*
	 * The firmware status (except for the QLTM_SVALID bit) indicates
	 * why this ATIO was sent to us.
	 *
	 * If QLTM_SVALID is set, the firware has recommended Sense Data.
	 *
	 * If the DISCONNECTS DISABLED bit is set in the flags field,
	 * we're still connected on the SCSI bus - i.e. the initiator
	 * did not set DiscPriv in the identify message. We don't care
	 * about this so it's ignored.
	 */

	switch(aep->at_status & ~QLTM_SVALID) {
	case AT_PATH_INVALID:
		/*
		 * ATIO rejected by the firmware due to disabled lun.
		 */
		isp_prt(isp, ISP_LOGERR,
		    "rejected ATIO2 for disabled lun %d", lun);
		break;
	case AT_NOCAP:
		/*
		 * Requested Capability not available
		 * We sent an ATIO that overflowed the firmware's
		 * command resource count.
		 */
		isp_prt(isp, ISP_LOGERR,
		    "rejected ATIO2 for lun %d- command count overflow", lun);
		break;

	case AT_BDR_MSG:
		/*
		 * If we send an ATIO to the firmware to increment
		 * its command resource count, and the firmware is
		 * recovering from a Bus Device Reset, it returns
		 * the ATIO with this status. We set the command
		 * resource count in the Enable Lun entry and no
		 * not increment it. Therefore we should never get
		 * this status here.
		 */
		isp_prt(isp, ISP_LOGERR, atiocope, lun, 0);
		break;

	case AT_CDB:		/* Got a CDB */
		/*
		 * Punt to platform specific layer.
		 */
		(void) isp_async(isp, ISPASYNC_TARGET_ACTION, aep);
		break;

	case AT_RESET:
		/*
		 * A bus reset came along an blew away this command. Why
		 * they do this in addition the async event code stuff,
		 * I dunno.
		 *
		 * Ignore it because the async event will clear things
		 * up for us.
		 */
		isp_prt(isp, ISP_LOGERR, atior, lun, aep->at_iid, 0);
		break;


	default:
		isp_prt(isp, ISP_LOGERR,
		    "Unknown ATIO2 status 0x%x from initiator %d for lun %d",
		    aep->at_status, aep->at_iid, lun);
		(void) isp_target_put_atio(isp, aep);
		break;
	}
}

static void
isp_handle_ctio(struct ispsoftc *isp, ct_entry_t *ct)
{
	void *xs;
	int pl = ISP_LOGTDEBUG2;
	char *fmsg = NULL;

	if (ct->ct_syshandle) {
		xs = isp_find_xs(isp, ct->ct_syshandle);
		if (xs == NULL)
			pl = ISP_LOGALL;
	} else {
		xs = NULL;
	}

	switch(ct->ct_status & ~QLTM_SVALID) {
	case CT_OK:
		/*
		 * There are generally 3 possibilities as to why we'd get
		 * this condition:
		 * 	We disconnected after receiving a CDB.
		 * 	We sent or received data.
		 * 	We sent status & command complete.
		 */

		if (ct->ct_flags & CT_SENDSTATUS) {
			break;
		} else if ((ct->ct_flags & CT_DATAMASK) == CT_NO_DATA) {
			/*
			 * Nothing to do in this case.
			 */
			isp_prt(isp, pl, "CTIO- iid %d disconnected OK",
			    ct->ct_iid);
			return;
		}
		break;

	case CT_BDR_MSG:
		/*
		 * Bus Device Reset message received or the SCSI Bus has
		 * been Reset; the firmware has gone to Bus Free.
		 *
		 * The firmware generates an async mailbox interupt to
		 * notify us of this and returns outstanding CTIOs with this
		 * status. These CTIOs are handled in that same way as
		 * CT_ABORTED ones, so just fall through here.
		 */
		fmsg = "Bus Device Reset";
		/*FALLTHROUGH*/
	case CT_RESET:
		if (fmsg == NULL)
			fmsg = "Bus Reset";
		/*FALLTHROUGH*/
	case CT_ABORTED:
		/*
		 * When an Abort message is received the firmware goes to
		 * Bus Free and returns all outstanding CTIOs with the status
		 * set, then sends us an Immediate Notify entry.
		 */
		if (fmsg == NULL)
			fmsg = "ABORT TAG message sent by Initiator";

		isp_prt(isp, ISP_LOGWARN, "CTIO destroyed by %s", fmsg);
		break;

	case CT_INVAL:
		/*
		 * CTIO rejected by the firmware due to disabled lun.
		 * "Cannot Happen".
		 */
		isp_prt(isp, ISP_LOGERR,
		    "Firmware rejected CTIO for disabled lun %d",
		    ct->ct_lun);
		break;

	case CT_NOPATH:
		/*
		 * CTIO rejected by the firmware due "no path for the
		 * nondisconnecting nexus specified". This means that
		 * we tried to access the bus while a non-disconnecting
		 * command is in process.
		 */
		isp_prt(isp, ISP_LOGERR,
		    "Firmware rejected CTIO for bad nexus %d/%d/%d",
		    ct->ct_iid, ct->ct_tgt, ct->ct_lun);
		break;

	case CT_RSELTMO:
		fmsg = "Reselection";
		/*FALLTHROUGH*/
	case CT_TIMEOUT:
		if (fmsg == NULL)
			fmsg = "Command";
		isp_prt(isp, ISP_LOGERR, "Firmware timed out on %s", fmsg);
		break;

	case	CT_PANIC:
		if (fmsg == NULL)
			fmsg = "Unrecoverable Error";
		/*FALLTHROUGH*/
	case CT_ERR:
		if (fmsg == NULL)
			fmsg = "Completed with Error";
		/*FALLTHROUGH*/
	case CT_PHASE_ERROR:
		if (fmsg == NULL)
			fmsg = "Phase Sequence Error";
		/*FALLTHROUGH*/
	case CT_TERMINATED:
		if (fmsg == NULL)
			fmsg = "terminated by TERMINATE TRANSFER";
		/*FALLTHROUGH*/
	case CT_NOACK:
		if (fmsg == NULL)
			fmsg = "unacknowledged Immediate Notify pending";
		isp_prt(isp, ISP_LOGERR, "CTIO returned by f/w- %s", fmsg);
		break;
	default:
		isp_prt(isp, ISP_LOGERR, "Unknown CTIO status 0x%x",
		    ct->ct_status & ~QLTM_SVALID);
		break;
	}

	if (xs == NULL) {
		/*
		 * There may be more than one CTIO for a data transfer,
		 * or this may be a status CTIO we're not monitoring.
		 *
		 * The assumption is that they'll all be returned in the
		 * order we got them.
		 */
		if (ct->ct_syshandle == 0) {
			if ((ct->ct_flags & CT_SENDSTATUS) == 0) {
				isp_prt(isp, pl,
				    "intermediate CTIO completed ok");
			} else {
				isp_prt(isp, pl,
				    "unmonitored CTIO completed ok");
			}
		} else {
			isp_prt(isp, pl,
			    "NO xs for CTIO (handle 0x%x) status 0x%x",
			    ct->ct_syshandle, ct->ct_status & ~QLTM_SVALID);
		}
	} else {
		/*
		 * Final CTIO completed. Release DMA resources and
		 * notify platform dependent layers.
		 */
		if ((ct->ct_flags & CT_DATAMASK) != CT_NO_DATA) {
			ISP_DMAFREE(isp, xs, ct->ct_syshandle);
		}
		isp_prt(isp, pl, "final CTIO complete");
		/*
		 * The platform layer will destroy the handle if appropriate.
		 */
		(void) isp_async(isp, ISPASYNC_TARGET_ACTION, ct);
	}
}

static void
isp_handle_ctio2(struct ispsoftc *isp, ct2_entry_t *ct)
{
	XS_T *xs;
	int pl = ISP_LOGTDEBUG2;
	char *fmsg = NULL;

	if (ct->ct_syshandle) {
		xs = isp_find_xs(isp, ct->ct_syshandle);
		if (xs == NULL)
			pl = ISP_LOGALL;
	} else {
		xs = NULL;
	}

	switch(ct->ct_status & ~QLTM_SVALID) {
	case CT_BUS_ERROR:
		isp_prt(isp, ISP_LOGERR, "PCI DMA Bus Error");
		/* FALL Through */
	case CT_DATA_OVER:
	case CT_DATA_UNDER:
	case CT_OK:
		/*
		 * There are generally 2 possibilities as to why we'd get
		 * this condition:
		 * 	We sent or received data.
		 * 	We sent status & command complete.
		 */

		break;

	case CT_BDR_MSG:
		/*
		 * Target Reset function received.
		 *
		 * The firmware generates an async mailbox interupt to
		 * notify us of this and returns outstanding CTIOs with this
		 * status. These CTIOs are handled in that same way as
		 * CT_ABORTED ones, so just fall through here.
		 */
		fmsg = "TARGET RESET Task Management Function Received";
		/*FALLTHROUGH*/
	case CT_RESET:
		if (fmsg == NULL)
			fmsg = "LIP Reset";
		/*FALLTHROUGH*/
	case CT_ABORTED:
		/*
		 * When an Abort message is received the firmware goes to
		 * Bus Free and returns all outstanding CTIOs with the status
		 * set, then sends us an Immediate Notify entry.
		 */
		if (fmsg == NULL)
			fmsg = "ABORT Task Management Function Received";

		isp_prt(isp, ISP_LOGERR, "CTIO2 destroyed by %s", fmsg);
		break;

	case CT_INVAL:
		/*
		 * CTIO rejected by the firmware - invalid data direction.
		 */
		isp_prt(isp, ISP_LOGERR, "CTIO2 had wrong data directiond");
		break;

	case CT_RSELTMO:
		fmsg = "failure to reconnect to initiator";
		/*FALLTHROUGH*/
	case CT_TIMEOUT:
		if (fmsg == NULL)
			fmsg = "command";
		isp_prt(isp, ISP_LOGERR, "Firmware timed out on %s", fmsg);
		break;

	case CT_ERR:
		fmsg = "Completed with Error";
		/*FALLTHROUGH*/
	case CT_LOGOUT:
		if (fmsg == NULL)
			fmsg = "Port Logout";
		/*FALLTHROUGH*/
	case CT_PORTNOTAVAIL:
		if (fmsg == NULL)
			fmsg = "Port not available";
	case CT_PORTCHANGED:
		if (fmsg == NULL)
			fmsg = "Port Changed";
	case CT_NOACK:
		if (fmsg == NULL)
			fmsg = "unacknowledged Immediate Notify pending";
		isp_prt(isp, ISP_LOGERR, "CTIO returned by f/w- %s", fmsg);
		break;

	case CT_INVRXID:
		/*
		 * CTIO rejected by the firmware because an invalid RX_ID.
		 * Just print a message.
		 */
		isp_prt(isp, ISP_LOGERR,
		    "CTIO2 completed with Invalid RX_ID 0x%x", ct->ct_rxid);
		break;

	default:
		isp_prt(isp, ISP_LOGERR, "Unknown CTIO2 status 0x%x",
		    ct->ct_status & ~QLTM_SVALID);
		break;
	}

	if (xs == NULL) {
		/*
		 * There may be more than one CTIO for a data transfer,
		 * or this may be a status CTIO we're not monitoring.
		 *
		 * The assumption is that they'll all be returned in the
		 * order we got them.
		 */
		if (ct->ct_syshandle == 0) {
			if ((ct->ct_flags & CT_SENDSTATUS) == 0) {
				isp_prt(isp, pl,
				    "intermediate CTIO completed ok");
			} else {
				isp_prt(isp, pl,
				    "unmonitored CTIO completed ok");
			}
		} else {
			isp_prt(isp, pl,
			    "NO xs for CTIO (handle 0x%x) status 0x%x",
			    ct->ct_syshandle, ct->ct_status & ~QLTM_SVALID);
		}
	} else {
		if ((ct->ct_flags & CT2_DATAMASK) != CT2_NO_DATA) {
			ISP_DMAFREE(isp, xs, ct->ct_syshandle);
		}
		if (ct->ct_flags & CT_SENDSTATUS) {
			/*
			 * Sent status and command complete.
			 *
			 * We're now really done with this command, so we
			 * punt to the platform dependent layers because
			 * only there can we do the appropriate command
			 * complete thread synchronization.
			 */
			isp_prt(isp, pl, "status CTIO complete");
		} else {
			/*
			 * Final CTIO completed. Release DMA resources and
			 * notify platform dependent layers.
			 */
			isp_prt(isp, pl, "data CTIO complete");
		}
		(void) isp_async(isp, ISPASYNC_TARGET_ACTION, ct);
		/*
		 * The platform layer will destroy the handle if appropriate.
		 */
	}
}
#endif
