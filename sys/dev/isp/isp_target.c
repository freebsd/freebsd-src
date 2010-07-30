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
 * Machine and OS Independent Target Mode Code for the Qlogic SCSI/FC adapters.
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

#ifdef	ISP_TARGET_MODE
static const char atiocope[] = "ATIO returned for lun %d because it was in the middle of Bus Device Reset on bus %d";
static const char atior[] = "ATIO returned on for lun %d on from loopid %d because a Bus Reset occurred on bus %d";
static const char rqo[] = "%s: Request Queue Overflow";

static void isp_got_msg(ispsoftc_t *, in_entry_t *);
static void isp_got_msg_fc(ispsoftc_t *, in_fcentry_t *);
static void isp_got_tmf_24xx(ispsoftc_t *, at7_entry_t *);
static void isp_handle_atio(ispsoftc_t *, at_entry_t *);
static void isp_handle_atio2(ispsoftc_t *, at2_entry_t *);
static void isp_handle_ctio(ispsoftc_t *, ct_entry_t *);
static void isp_handle_ctio2(ispsoftc_t *, ct2_entry_t *);
static void isp_handle_ctio7(ispsoftc_t *, ct7_entry_t *);
static void isp_handle_24xx_inotify(ispsoftc_t *, in_fcentry_24xx_t *);

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
isp_target_notify(ispsoftc_t *isp, void *vptr, uint32_t *optrp)
{
	uint16_t status;
	uint32_t seqid;
	union {
		at_entry_t	*atiop;
		at2_entry_t	*at2iop;
		at2e_entry_t	*at2eiop;
		at7_entry_t	*at7iop;
		ct_entry_t	*ctiop;
		ct2_entry_t	*ct2iop;
		ct2e_entry_t	*ct2eiop;
		ct7_entry_t	*ct7iop;
		lun_entry_t	*lunenp;
		in_entry_t	*inotp;
		in_fcentry_t	*inot_fcp;
		in_fcentry_e_t	*inote_fcp;
		in_fcentry_24xx_t *inot_24xx;
		na_entry_t	*nackp;
		na_fcentry_t	*nack_fcp;
		na_fcentry_e_t	*nacke_fcp;
		na_fcentry_24xx_t *nack_24xx;
		isphdr_t	*hp;
		abts_t		*abts;
		abts_rsp_t	*abts_rsp;
		els_t		*els;
		void *		*vp;
#define	atiop		unp.atiop
#define	at2iop		unp.at2iop
#define	at2eiop		unp.at2eiop
#define	at7iop		unp.at7iop
#define	ctiop		unp.ctiop
#define	ct2iop		unp.ct2iop
#define	ct2eiop		unp.ct2eiop
#define	ct7iop		unp.ct7iop
#define	lunenp		unp.lunenp
#define	inotp		unp.inotp
#define	inot_fcp	unp.inot_fcp
#define	inote_fcp	unp.inote_fcp
#define	inot_24xx	unp.inot_24xx
#define	nackp		unp.nackp
#define	nack_fcp	unp.nack_fcp
#define	nacke_fcp	unp.nacke_fcp
#define	nack_24xx	unp.nack_24xx
#define	abts		unp.abts
#define	abts_rsp	unp.abts_rsp
#define els		unp.els
#define	hdrp		unp.hp
	} unp;
	uint8_t local[QENTRY_LEN];
	uint16_t iid;
	int bus, type, level, rval = 1;
	isp_notify_t notify;

	type = isp_get_response_type(isp, (isphdr_t *)vptr);
	unp.vp = vptr;

	ISP_TDQE(isp, "isp_target_notify", (int) *optrp, vptr);

	switch (type) {
	case RQSTYPE_ATIO:
		if (IS_24XX(isp)) {
			int len;

			isp_get_atio7(isp, at7iop, (at7_entry_t *) local);
			at7iop = (at7_entry_t *) local;
			/*
			 * Check for and do something with commands whose
			 * IULEN extends past a single queue entry.
			 */
			len = at7iop->at_ta_len & 0xfffff;
			if (len > (QENTRY_LEN - 8)) {
				len -= (QENTRY_LEN - 8);
				isp_prt(isp, ISP_LOGINFO, "long IU length (%d) ignored", len);
				while (len > 0) {
					*optrp =  ISP_NXT_QENTRY(*optrp, RESULT_QUEUE_LEN(isp));
					len -= QENTRY_LEN;
				}
			}
			/*
			 * Check for a task management function
			 */
			if (at7iop->at_cmnd.fcp_cmnd_task_management) {
				isp_got_tmf_24xx(isp, at7iop);
				break;
			}
			/*
			 * Just go straight to outer layer for this one.
			 */
			isp_async(isp, ISPASYNC_TARGET_ACTION, local);
		} else {
			isp_get_atio(isp, atiop, (at_entry_t *) local);
			isp_handle_atio(isp, (at_entry_t *) local);
		}
		break;

	case RQSTYPE_CTIO:
		isp_get_ctio(isp, ctiop, (ct_entry_t *) local);
		isp_handle_ctio(isp, (ct_entry_t *) local);
		break;

	case RQSTYPE_ATIO2:
		if (ISP_CAP_2KLOGIN(isp)) {
			isp_get_atio2e(isp, at2eiop, (at2e_entry_t *) local);
		} else {
			isp_get_atio2(isp, at2iop, (at2_entry_t *) local);
		}
		isp_handle_atio2(isp, (at2_entry_t *) local);
		break;

	case RQSTYPE_CTIO3:
	case RQSTYPE_CTIO2:
		if (ISP_CAP_2KLOGIN(isp)) {
			isp_get_ctio2e(isp, ct2eiop, (ct2e_entry_t *) local);
		} else {
			isp_get_ctio2(isp, ct2iop, (ct2_entry_t *) local);
		}
		isp_handle_ctio2(isp, (ct2_entry_t *) local);
		break;

	case RQSTYPE_CTIO7:
		isp_get_ctio7(isp, ct7iop, (ct7_entry_t *) local);
		isp_handle_ctio7(isp, (ct7_entry_t *) local);
		break;

	case RQSTYPE_ENABLE_LUN:
	case RQSTYPE_MODIFY_LUN:
		isp_get_enable_lun(isp, lunenp, (lun_entry_t *) local);
		isp_async(isp, ISPASYNC_TARGET_ACTION, local);
		break;

	case RQSTYPE_NOTIFY:
		bus = 0;
		if (IS_24XX(isp)) {
			isp_get_notify_24xx(isp, inot_24xx, (in_fcentry_24xx_t *)local);
			inot_24xx = (in_fcentry_24xx_t *) local;
			isp_handle_24xx_inotify(isp, inot_24xx);
			break;
		}
		if (IS_FC(isp)) {
			if (ISP_CAP_2KLOGIN(isp)) {
				in_fcentry_e_t *ecp = (in_fcentry_e_t *)local;
				isp_get_notify_fc_e(isp, inote_fcp, ecp);
				iid = ecp->in_iid;
				status = ecp->in_status;
				seqid = ecp->in_seqid;
			} else {
				in_fcentry_t *fcp = (in_fcentry_t *)local;
				isp_get_notify_fc(isp, inot_fcp, fcp);
				iid = fcp->in_iid;
				status = fcp->in_status;
				seqid = fcp->in_seqid;
			}
		} else {
			in_entry_t *inp = (in_entry_t *)local;
			isp_get_notify(isp, inotp, inp);
			status = inp->in_status & 0xff;
			seqid = inp->in_seqid;
			iid = inp->in_iid;
			if (IS_DUALBUS(isp)) {
				bus = GET_BUS_VAL(inp->in_iid);
				SET_BUS_VAL(inp->in_iid, 0);
			}
		}

		isp_prt(isp, ISP_LOGTDEBUG0, "Immediate Notify On Bus %d, status=0x%x seqid=0x%x", bus, status, seqid);

		switch (status) {
		case IN_MSG_RECEIVED:
		case IN_IDE_RECEIVED:
			if (IS_FC(isp)) {
				isp_got_msg_fc(isp, (in_fcentry_t *)local);
			} else {
				isp_got_msg(isp, (in_entry_t *)local);
			}
			break;
		case IN_RSRC_UNAVAIL:
			isp_prt(isp, ISP_LOGINFO, "Firmware out of ATIOs");
			(void) isp_notify_ack(isp, local);
			break;

		case IN_RESET:
			ISP_MEMZERO(&notify, sizeof (isp_notify_t));
			notify.nt_hba = isp;
			notify.nt_wwn = INI_ANY;
			notify.nt_tgt = TGT_ANY;
			notify.nt_nphdl = iid;
			notify.nt_sid = PORT_ANY;
			notify.nt_did = PORT_ANY;
			notify.nt_lun = LUN_ANY;
			notify.nt_tagval = TAG_ANY;
			notify.nt_tagval |= (((uint64_t)(isp->isp_serno++)) << 32);
			notify.nt_ncode = NT_BUS_RESET;
			notify.nt_need_ack = 1;
			notify.nt_lreserved = local;
			isp_async(isp, ISPASYNC_TARGET_NOTIFY, &notify);
			break;

		case IN_PORT_LOGOUT:
			ISP_MEMZERO(&notify, sizeof (isp_notify_t));
			notify.nt_hba = isp;
			notify.nt_wwn = INI_ANY;
			notify.nt_nphdl = iid;
			notify.nt_sid = PORT_ANY;
			notify.nt_did = PORT_ANY;
			notify.nt_ncode = NT_LOGOUT;
			notify.nt_need_ack = 1;
			notify.nt_lreserved = local;
			isp_async(isp, ISPASYNC_TARGET_NOTIFY, &notify);
			break;

		case IN_ABORT_TASK:
			ISP_MEMZERO(&notify, sizeof (isp_notify_t));
			notify.nt_hba = isp;
			notify.nt_wwn = INI_ANY;
			notify.nt_nphdl = iid;
			notify.nt_sid = PORT_ANY;
			notify.nt_did = PORT_ANY;
			notify.nt_ncode = NT_ABORT_TASK;
			notify.nt_need_ack = 1;
			notify.nt_lreserved = local;
			isp_async(isp, ISPASYNC_TARGET_NOTIFY, &notify);
			break;

		case IN_GLOBAL_LOGO:
			isp_prt(isp, ISP_LOGTINFO, "%s: all ports logged out", __func__);
			ISP_MEMZERO(&notify, sizeof (isp_notify_t));
			notify.nt_hba = isp;
			notify.nt_wwn = INI_ANY;
			notify.nt_nphdl = NIL_HANDLE;
			notify.nt_sid = PORT_ANY;
			notify.nt_did = PORT_ANY;
			notify.nt_ncode = NT_GLOBAL_LOGOUT;
			isp_async(isp, ISPASYNC_TARGET_NOTIFY, &notify);
			(void) isp_notify_ack(isp, local);
			break;

		case IN_PORT_CHANGED:
			isp_prt(isp, ISP_LOGTINFO, "%s: port changed", __func__);
			(void) isp_notify_ack(isp, local);
			break;

		default:
			ISP_SNPRINTF(local, sizeof local, "%s: unknown status to RQSTYPE_NOTIFY (0x%x)", __func__, status);
			isp_print_bytes(isp, local, QENTRY_LEN, vptr);
			(void) isp_notify_ack(isp, local);
			break;
		}
		break;

	case RQSTYPE_NOTIFY_ACK:
		/*
		 * The ISP is acknowledging our acknowledgement of an
		 * Immediate Notify entry for some asynchronous event.
		 */
		if (IS_24XX(isp)) {
			isp_get_notify_ack_24xx(isp, nack_24xx, (na_fcentry_24xx_t *) local);
			nack_24xx = (na_fcentry_24xx_t *) local;
			if (nack_24xx->na_status != NA_OK) {
				level = ISP_LOGINFO;
			} else {
				level = ISP_LOGTDEBUG1;
			}
			isp_prt(isp, level, "Notify Ack Status=0x%x; Subcode 0x%x seqid=0x%x", nack_24xx->na_status, nack_24xx->na_status_subcode, nack_24xx->na_rxid);
		} else if (IS_FC(isp)) {
			if (ISP_CAP_2KLOGIN(isp)) {
				isp_get_notify_ack_fc_e(isp, nacke_fcp, (na_fcentry_e_t *)local);
			} else {
				isp_get_notify_ack_fc(isp, nack_fcp, (na_fcentry_t *)local);
			}
			nack_fcp = (na_fcentry_t *)local;
			if (nack_fcp->na_status != NA_OK) {
				level = ISP_LOGINFO;
			} else {
				level = ISP_LOGTDEBUG1;
			}
			isp_prt(isp, level, "Notify Ack Status=0x%x seqid 0x%x", nack_fcp->na_status, nack_fcp->na_seqid);
		} else {
			isp_get_notify_ack(isp, nackp, (na_entry_t *)local);
			nackp = (na_entry_t *)local;
			if (nackp->na_status != NA_OK) {
				level = ISP_LOGINFO;
			} else {
				level = ISP_LOGTDEBUG1;
			}
			isp_prt(isp, level, "Notify Ack event 0x%x status=0x%x seqid 0x%x", nackp->na_event, nackp->na_status, nackp->na_seqid);
		}
		break;

	case RQSTYPE_ABTS_RCVD:
		isp_get_abts(isp, abts, (abts_t *)local);
		isp_async(isp, ISPASYNC_TARGET_ACTION, &local);
		break;
	case RQSTYPE_ABTS_RSP:
		isp_get_abts_rsp(isp, abts_rsp, (abts_rsp_t *)local);
		abts_rsp = (abts_rsp_t *) local;
		if (abts_rsp->abts_rsp_status) {
			level = ISP_LOGINFO;
		} else {
			level = ISP_LOGTDEBUG0;
		}
		isp_prt(isp, level, "ABTS RSP response[0x%x]: status=0x%x sub=(0x%x 0x%x)", abts_rsp->abts_rsp_rxid_task, abts_rsp->abts_rsp_status,
		    abts_rsp->abts_rsp_payload.rsp.subcode1, abts_rsp->abts_rsp_payload.rsp.subcode2);
		break;
	default:
		isp_prt(isp, ISP_LOGERR, "%s: unknown entry type 0x%x", __func__, type);
		rval = 0;
		break;
	}
#undef	atiop
#undef	at2iop
#undef	at2eiop
#undef	at7iop
#undef	ctiop
#undef	ct2iop
#undef	ct2eiop
#undef	ct7iop
#undef	lunenp
#undef	inotp
#undef	inot_fcp
#undef	inote_fcp
#undef	inot_24xx
#undef	nackp
#undef	nack_fcp
#undef	nacke_fcp
#undef	hack_24xx
#undef	abts
#undef	abts_rsp
#undef	els
#undef	hdrp
	return (rval);
}


/*
 * Toggle (on/off) target mode for bus/target/lun.
 *
 * The caller has checked for overlap and legality.
 *
 * Note that not all of bus, target or lun can be paid attention to.
 * Note also that this action will not be complete until the f/w writes
 * a response entry. The caller is responsible for synchronizing with this.
 */
int
isp_lun_cmd(ispsoftc_t *isp, int cmd, int bus, int lun, int cmd_cnt, int inot_cnt)
{
	lun_entry_t el;
	void *outp;

	ISP_MEMZERO(&el, sizeof (el));
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
	if (IS_SCSI(isp)) {
		el.le_tgt = SDPARAM(isp, bus)->isp_initiator_id;
		el.le_lun = lun;
	} else if (ISP_CAP_SCCFW(isp) == 0) {
		el.le_lun = lun;
	}
	el.le_timeout = 30;

	outp = isp_getrqentry(isp);
	if (outp == NULL) {
		isp_prt(isp, ISP_LOGERR, rqo, __func__);
		return (-1);
	}
	isp_put_enable_lun(isp, &el, outp);
	ISP_TDQE(isp, "isp_lun_cmd", isp->isp_reqidx, &el);
	ISP_SYNC_REQUEST(isp);
	return (0);
}

int
isp_target_put_entry(ispsoftc_t *isp, void *ap)
{
	void *outp;
	uint8_t etype = ((isphdr_t *) ap)->rqs_entry_type;

	outp = isp_getrqentry(isp);
	if (outp == NULL) {
		isp_prt(isp, ISP_LOGWARN, rqo, __func__); 
		return (-1);
	}
	switch (etype) {
	case RQSTYPE_ATIO:
		isp_put_atio(isp, (at_entry_t *) ap, (at_entry_t *) outp);
		break;
	case RQSTYPE_ATIO2:
		if (ISP_CAP_2KLOGIN(isp)) {
			isp_put_atio2e(isp, (at2e_entry_t *) ap, (at2e_entry_t *) outp);
		} else {
			isp_put_atio2(isp, (at2_entry_t *) ap, (at2_entry_t *) outp);
		}
		break;
	case RQSTYPE_CTIO:
		isp_put_ctio(isp, (ct_entry_t *) ap, (ct_entry_t *) outp);
		break;
	case RQSTYPE_CTIO2:
		if (ISP_CAP_2KLOGIN(isp)) {
			isp_put_ctio2e(isp, (ct2e_entry_t *) ap, (ct2e_entry_t *) outp);
		} else {
			isp_put_ctio2(isp, (ct2_entry_t *) ap, (ct2_entry_t *) outp);
		}
		break;
	case RQSTYPE_CTIO7:
		isp_put_ctio7(isp, (ct7_entry_t *) ap, (ct7_entry_t *) outp);
		break;
	default:
		isp_prt(isp, ISP_LOGERR, "%s: Unknown type 0x%x", __func__, etype);
		return (-1);
	}
	ISP_TDQE(isp, __func__, isp->isp_reqidx, ap);
	ISP_SYNC_REQUEST(isp);
	return (0);
}

int
isp_target_put_atio(ispsoftc_t *isp, void *arg)
{
	union {
		at_entry_t _atio;
		at2_entry_t _atio2;
		at2e_entry_t _atio2e;
	} atun;

	ISP_MEMZERO(&atun, sizeof atun);
	if (IS_FC(isp)) {
		at2_entry_t *aep = arg;
		atun._atio2.at_header.rqs_entry_type = RQSTYPE_ATIO2;
		atun._atio2.at_header.rqs_entry_count = 1;
		if (ISP_CAP_SCCFW(isp)) {
			atun._atio2.at_scclun = aep->at_scclun;
		} else {
			atun._atio2.at_lun = (uint8_t) aep->at_lun;
		}
		if (ISP_CAP_2KLOGIN(isp)) {
			atun._atio2e.at_iid = ((at2e_entry_t *)aep)->at_iid;
		} else {
			atun._atio2.at_iid = aep->at_iid;
		}
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
isp_endcmd(ispsoftc_t *isp, ...)
{
	uint32_t code, hdl;
	uint8_t sts;
	union {
		ct_entry_t _ctio;
		ct2_entry_t _ctio2;
		ct2e_entry_t _ctio2e;
		ct7_entry_t _ctio7;
	} un;
	va_list ap;

	ISP_MEMZERO(&un, sizeof un);

	if (IS_24XX(isp)) {
		int vpidx, nphdl;
		at7_entry_t *aep;
		ct7_entry_t *cto = &un._ctio7;

		va_start(ap, isp);
		aep = va_arg(ap, at7_entry_t *);
		nphdl = va_arg(ap, int);
		/*
		 * Note that vpidx may equal 0xff (unknown) here
		 */
		vpidx = va_arg(ap, int);
		code = va_arg(ap, uint32_t);
		hdl = va_arg(ap, uint32_t);
		va_end(ap);
		isp_prt(isp, ISP_LOGTDEBUG0, "%s: [RX_ID 0x%x] chan %d code %x", __func__, aep->at_rxid, vpidx, code);

		sts = code & 0xff;
		cto->ct_header.rqs_entry_type = RQSTYPE_CTIO7;
		cto->ct_header.rqs_entry_count = 1;
		cto->ct_nphdl = nphdl;
		cto->ct_rxid = aep->at_rxid;
		cto->ct_iid_lo = (aep->at_hdr.s_id[1] << 8) | aep->at_hdr.s_id[2];
		cto->ct_iid_hi = aep->at_hdr.s_id[0];
		cto->ct_oxid = aep->at_hdr.ox_id;
		cto->ct_scsi_status = sts;
		cto->ct_vpidx = vpidx;
		cto->ct_flags = CT7_NOACK;
		if (code & ECMD_TERMINATE) {
			cto->ct_flags |= CT7_TERMINATE;
		} else if (code & ECMD_SVALID) {
			cto->ct_flags |= CT7_FLAG_MODE1 | CT7_SENDSTATUS;
			cto->ct_scsi_status |= (FCP_SNSLEN_VALID << 8);
			cto->rsp.m1.ct_resplen = cto->ct_senselen = min(16, MAXRESPLEN_24XX);
			ISP_MEMZERO(cto->rsp.m1.ct_resp, sizeof (cto->rsp.m1.ct_resp));
			cto->rsp.m1.ct_resp[0] = 0xf0;
			cto->rsp.m1.ct_resp[2] = (code >> 12) & 0xf;
			cto->rsp.m1.ct_resp[7] = 8;
			cto->rsp.m1.ct_resp[12] = (code >> 24) & 0xff;
			cto->rsp.m1.ct_resp[13] = (code >> 16) & 0xff;
		} else {
			cto->ct_flags |= CT7_FLAG_MODE1 | CT7_SENDSTATUS;
		}
		if (aep->at_cmnd.cdb_dl.sf.fcp_cmnd_dl) {
			cto->ct_resid = aep->at_cmnd.cdb_dl.sf.fcp_cmnd_dl;
			if (cto->ct_resid < 0) {
				 cto->ct_scsi_status |= (FCP_RESID_OVERFLOW << 8);
			} else if (cto->ct_resid > 0) {
				 cto->ct_scsi_status |= (FCP_RESID_UNDERFLOW << 8);
			}
		}
		cto->ct_syshandle = hdl;
	} else if (IS_FC(isp)) {
		at2_entry_t *aep;
		ct2_entry_t *cto = &un._ctio2;

		va_start(ap, isp);
		aep = va_arg(ap, at2_entry_t *);
		code = va_arg(ap, uint32_t);
		hdl = va_arg(ap, uint32_t);
		va_end(ap);

		isp_prt(isp, ISP_LOGTDEBUG0, "%s: [RX_ID 0x%x] code %x", __func__, aep->at_rxid, code);

		sts = code & 0xff;
		cto->ct_header.rqs_entry_type = RQSTYPE_CTIO2;
		cto->ct_header.rqs_entry_count = 1;
		if (ISP_CAP_SCCFW(isp) == 0) {
			cto->ct_lun = aep->at_lun;
		}
		if (ISP_CAP_2KLOGIN(isp)) {
			un._ctio2e.ct_iid = ((at2e_entry_t *)aep)->at_iid;
		} else {
			cto->ct_iid = aep->at_iid;
		}
		cto->ct_rxid = aep->at_rxid;
		cto->rsp.m1.ct_scsi_status = sts;
		cto->ct_flags = CT2_SENDSTATUS | CT2_NO_DATA | CT2_FLAG_MODE1;
		if (hdl == 0) {
			cto->ct_flags |= CT2_CCINCR;
		}
		if (aep->at_datalen) {
			cto->ct_resid = aep->at_datalen;
			cto->rsp.m1.ct_scsi_status |= CT2_DATA_UNDER;
		}
		if (sts == SCSI_CHECK && (code & ECMD_SVALID)) {
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
		at_entry_t *aep;
		ct_entry_t *cto = &un._ctio;

		va_start(ap, isp);
		aep = va_arg(ap, at_entry_t *);
		code = va_arg(ap, uint32_t);
		hdl = va_arg(ap, uint32_t);
		va_end(ap);
		isp_prt(isp, ISP_LOGTDEBUG0, "%s: [IID %d] code %x", __func__, aep->at_iid, code);
		sts = code;

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

/*
 * These are either broadcast events or specifically CTIO fast completion
 */

int
isp_target_async(ispsoftc_t *isp, int bus, int event)
{
	isp_notify_t notify;

	ISP_MEMZERO(&notify, sizeof (isp_notify_t));
	notify.nt_hba = isp;
	notify.nt_wwn = INI_ANY;
	notify.nt_nphdl = NIL_HANDLE;
	notify.nt_sid = PORT_ANY;
	notify.nt_did = PORT_ANY;
	notify.nt_tgt = TGT_ANY;
	notify.nt_channel = bus;
	notify.nt_lun = LUN_ANY;
	notify.nt_tagval = TAG_ANY;
	notify.nt_tagval |= (((uint64_t)(isp->isp_serno++)) << 32);

	switch (event) {
	case ASYNC_LOOP_UP:
	case ASYNC_PTPMODE:
		isp_prt(isp, ISP_LOGTDEBUG0, "%s: LOOP UP", __func__);
		notify.nt_ncode = NT_LINK_UP;
		isp_async(isp, ISPASYNC_TARGET_NOTIFY, &notify);
		break;
	case ASYNC_LOOP_DOWN:
		isp_prt(isp, ISP_LOGTDEBUG0, "%s: LOOP DOWN", __func__);
		notify.nt_ncode = NT_LINK_DOWN;
		isp_async(isp, ISPASYNC_TARGET_NOTIFY, &notify);
		break;
	case ASYNC_LIP_ERROR:
	case ASYNC_LIP_F8:
	case ASYNC_LIP_OCCURRED:
	case ASYNC_LOOP_RESET:
		isp_prt(isp, ISP_LOGTDEBUG0, "%s: LIP RESET", __func__);
		notify.nt_ncode = NT_LIP_RESET;
		isp_async(isp, ISPASYNC_TARGET_NOTIFY, &notify);
		break;
	case ASYNC_BUS_RESET:
	case ASYNC_TIMEOUT_RESET:	/* XXX: where does this come from ? */
		isp_prt(isp, ISP_LOGTDEBUG0, "%s: BUS RESET", __func__);
		notify.nt_ncode = NT_BUS_RESET;
		isp_async(isp, ISPASYNC_TARGET_NOTIFY, &notify);
		break;
	case ASYNC_DEVICE_RESET:
		isp_prt(isp, ISP_LOGTDEBUG0, "%s: DEVICE RESET", __func__);
		notify.nt_ncode = NT_TARGET_RESET;
		isp_async(isp, ISPASYNC_TARGET_NOTIFY, &notify);
		break;
	case ASYNC_CTIO_DONE:
	{
		uint8_t storage[QENTRY_LEN];
		isp_prt(isp, ISP_LOGTDEBUG0, "%s: CTIO DONE", __func__);
		memset(storage, 0, QENTRY_LEN);
		if (IS_24XX(isp)) {
			ct7_entry_t *ct = (ct7_entry_t *) storage;
			ct->ct_header.rqs_entry_type = RQSTYPE_CTIO7;
			ct->ct_nphdl = CT7_OK;
			ct->ct_syshandle = bus;
			ct->ct_flags = CT7_SENDSTATUS;
		} else if (IS_FC(isp)) {
            		/* This should also suffice for 2K login code */
			ct2_entry_t *ct = (ct2_entry_t *) storage;
			ct->ct_header.rqs_entry_type = RQSTYPE_CTIO2;
			ct->ct_status = CT_OK;
			ct->ct_syshandle = bus;
			ct->ct_flags = CT2_SENDSTATUS|CT2_FASTPOST;
		} else {
			ct_entry_t *ct = (ct_entry_t *) storage;
			ct->ct_header.rqs_entry_type = RQSTYPE_CTIO;
			ct->ct_status = CT_OK;
			ct->ct_syshandle = bus;
			/* we skip fwhandle here */
			ct->ct_fwhandle = 0;
			ct->ct_flags = CT_SENDSTATUS;
		}
		isp_async(isp, ISPASYNC_TARGET_ACTION, storage);
		break;
	}
	default:
		isp_prt(isp, ISP_LOGERR, "%s: unknown event 0x%x", __func__, event);
		if (isp->isp_state == ISP_RUNSTATE) {
			(void) isp_notify_ack(isp, NULL);
		}
		break;
	}
	return (0);
}


/*
 * Process a received message.
 * The ISP firmware can handle most messages, there are only
 * a few that we need to deal with:
 * - abort: clean up the current command
 * - abort tag and clear queue
 */

static void
isp_got_msg(ispsoftc_t *isp, in_entry_t *inp)
{
	isp_notify_t notify;
	uint8_t status = inp->in_status & ~QLTM_SVALID;

	ISP_MEMZERO(&notify, sizeof (notify));
	notify.nt_hba = isp;
	notify.nt_wwn = INI_ANY;
	notify.nt_nphdl = GET_IID_VAL(inp->in_iid);
	notify.nt_sid = PORT_ANY;
	notify.nt_did = PORT_ANY;
	notify.nt_channel = GET_BUS_VAL(inp->in_iid);
	notify.nt_tgt = inp->in_tgt;
	notify.nt_lun = inp->in_lun;
	IN_MAKE_TAGID(notify.nt_tagval, inp);
	notify.nt_tagval |= (((uint64_t)(isp->isp_serno++)) << 32);
	notify.nt_lreserved = inp;

	if (status == IN_IDE_RECEIVED || status == IN_MSG_RECEIVED) {
		switch (inp->in_msg[0]) {
		case MSG_ABORT:
			notify.nt_ncode = NT_ABORT_TASK_SET;
			break;
		case MSG_BUS_DEV_RESET:
			notify.nt_ncode = NT_TARGET_RESET;
			break;
		case MSG_ABORT_TAG:
			notify.nt_ncode = NT_ABORT_TASK;
			break;
		case MSG_CLEAR_QUEUE:
			notify.nt_ncode = NT_CLEAR_TASK_SET;
			break;
		case MSG_REL_RECOVERY:
			notify.nt_ncode = NT_CLEAR_ACA;
			break;
		case MSG_TERM_IO_PROC:
			notify.nt_ncode = NT_ABORT_TASK;
			break;
		case MSG_LUN_RESET:
			notify.nt_ncode = NT_LUN_RESET;
			break;
		default:
			isp_prt(isp, ISP_LOGERR, "%s: unhandled message 0x%x", __func__, inp->in_msg[0]);
			(void) isp_notify_ack(isp, inp);
			return;
		}
		isp_async(isp, ISPASYNC_TARGET_NOTIFY, &notify);
	} else {
		isp_prt(isp, ISP_LOGERR, "%s: unknown immediate notify status 0x%x", __func__, inp->in_status);
		(void) isp_notify_ack(isp, inp);
	}
}

/*
 * Synthesize a message from the task management flags in a FCP_CMND_IU.
 */
static void
isp_got_msg_fc(ispsoftc_t *isp, in_fcentry_t *inp)
{
	isp_notify_t notify;
	static const char f1[] = "%s from N-port handle 0x%x lun %d seq 0x%x";
	static const char f2[] = "unknown %s 0x%x lun %d N-Port handle 0x%x task flags 0x%x seq 0x%x\n";
	uint16_t seqid, loopid;

	ISP_MEMZERO(&notify, sizeof (isp_notify_t));
	notify.nt_hba = isp;
	notify.nt_wwn = INI_ANY;
	if (ISP_CAP_2KLOGIN(isp)) {
		notify.nt_nphdl = ((in_fcentry_e_t *)inp)->in_iid;
		loopid = ((in_fcentry_e_t *)inp)->in_iid;
		seqid = ((in_fcentry_e_t *)inp)->in_seqid;
	} else {
		notify.nt_nphdl = inp->in_iid;
		loopid = inp->in_iid;
		seqid = inp->in_seqid;
	}
	notify.nt_sid = PORT_ANY;
	notify.nt_did = PORT_ANY;

	/* nt_tgt set in outer layers */
	if (ISP_CAP_SCCFW(isp)) {
		notify.nt_lun = inp->in_scclun;
	} else {
		notify.nt_lun = inp->in_lun;
	}
	notify.nt_tagval = seqid;
	notify.nt_tagval |= (((uint64_t)(isp->isp_serno++)) << 32);
	notify.nt_need_ack = 1;
	notify.nt_lreserved = inp;

	if (inp->in_status != IN_MSG_RECEIVED) {
		isp_prt(isp, ISP_LOGINFO, f2, "immediate notify status", inp->in_status, notify.nt_lun, loopid, inp->in_task_flags, inp->in_seqid);
		(void) isp_notify_ack(isp, inp);
		return;
	}

	if (inp->in_task_flags & TASK_FLAGS_ABORT_TASK_SET) {
		isp_prt(isp, ISP_LOGINFO, f1, "ABORT TASK SET", loopid, notify.nt_lun, inp->in_seqid);
		notify.nt_ncode = NT_ABORT_TASK_SET;
	} else if (inp->in_task_flags & TASK_FLAGS_CLEAR_TASK_SET) {
		isp_prt(isp, ISP_LOGINFO, f1, "CLEAR TASK SET", loopid, notify.nt_lun, inp->in_seqid);
		notify.nt_ncode = NT_CLEAR_TASK_SET;
	} else if (inp->in_task_flags & TASK_FLAGS_LUN_RESET) {
		isp_prt(isp, ISP_LOGINFO, f1, "LUN RESET", loopid, notify.nt_lun, inp->in_seqid);
		notify.nt_ncode = NT_LUN_RESET;
	} else if (inp->in_task_flags & TASK_FLAGS_TARGET_RESET) {
		isp_prt(isp, ISP_LOGINFO, f1, "TARGET RESET", loopid, notify.nt_lun, inp->in_seqid);
		notify.nt_ncode = NT_TARGET_RESET;
	} else if (inp->in_task_flags & TASK_FLAGS_CLEAR_ACA) {
		isp_prt(isp, ISP_LOGINFO, f1, "CLEAR ACA", loopid, notify.nt_lun, inp->in_seqid);
		notify.nt_ncode = NT_CLEAR_ACA;
	} else {
		isp_prt(isp, ISP_LOGWARN, f2, "task flag", inp->in_status, notify.nt_lun, loopid, inp->in_task_flags,  inp->in_seqid);
		(void) isp_notify_ack(isp, inp);
		return;
	}
	isp_async(isp, ISPASYNC_TARGET_NOTIFY, &notify);
}

static void
isp_got_tmf_24xx(ispsoftc_t *isp, at7_entry_t *aep)
{
	isp_notify_t notify;
	static const char f1[] = "%s from PortID 0x%06x lun %d seq 0x%08x";
	static const char f2[] = "unknown Task Flag 0x%x lun %d PortID 0x%x tag 0x%08x";
	uint16_t chan;
	uint32_t sid, did;

	ISP_MEMZERO(&notify, sizeof (isp_notify_t));
	notify.nt_hba = isp;
	notify.nt_wwn = INI_ANY;
	notify.nt_lun = (aep->at_cmnd.fcp_cmnd_lun[0] << 8) | (aep->at_cmnd.fcp_cmnd_lun[1]);
	notify.nt_tagval = aep->at_rxid;
	notify.nt_tagval |= (((uint64_t)(isp->isp_serno++)) << 32);
	notify.nt_lreserved = aep;
	sid = (aep->at_hdr.s_id[0] << 16) | (aep->at_hdr.s_id[1] <<  8) | (aep->at_hdr.s_id[2]);

	/* Channel has to derived from D_ID */
	did = (aep->at_hdr.d_id[0] << 16) | (aep->at_hdr.d_id[1] << 8) | aep->at_hdr.d_id[2];
	for (chan = 0; chan < isp->isp_nchan; chan++) {
		if (FCPARAM(isp, chan)->isp_portid == did) {
			break;
		}
	}
	if (chan == isp->isp_nchan) {
		isp_prt(isp, ISP_LOGWARN, "%s: D_ID 0x%x not found on any channel", __func__, did);
		/* just drop on the floor */
		return;
	}
	notify.nt_nphdl = NIL_HANDLE; /* unknown here */
	notify.nt_sid = sid;
	notify.nt_did = did;
	notify.nt_channel = chan;
	if (aep->at_cmnd.fcp_cmnd_task_management & FCP_CMND_TMF_ABORT_TASK_SET) {
		isp_prt(isp, ISP_LOGINFO, f1, "ABORT TASK SET", sid, notify.nt_lun, aep->at_rxid);
		notify.nt_ncode = NT_ABORT_TASK_SET;
	} else if (aep->at_cmnd.fcp_cmnd_task_management & FCP_CMND_TMF_CLEAR_TASK_SET) {
		isp_prt(isp, ISP_LOGINFO, f1, "CLEAR TASK SET", sid, notify.nt_lun, aep->at_rxid);
		notify.nt_ncode = NT_CLEAR_TASK_SET;
	} else if (aep->at_cmnd.fcp_cmnd_task_management & FCP_CMND_TMF_LUN_RESET) {
		isp_prt(isp, ISP_LOGINFO, f1, "LUN RESET", sid, notify.nt_lun, aep->at_rxid);
		notify.nt_ncode = NT_LUN_RESET;
	} else if (aep->at_cmnd.fcp_cmnd_task_management & FCP_CMND_TMF_TGT_RESET) {
		isp_prt(isp, ISP_LOGINFO, f1, "TARGET RESET", sid, notify.nt_lun, aep->at_rxid);
		notify.nt_ncode = NT_TARGET_RESET;
	} else if (aep->at_cmnd.fcp_cmnd_task_management & FCP_CMND_TMF_CLEAR_ACA) {
		isp_prt(isp, ISP_LOGINFO, f1, "CLEAR ACA", sid, notify.nt_lun, aep->at_rxid);
		notify.nt_ncode = NT_CLEAR_ACA;
	} else {
		isp_prt(isp, ISP_LOGWARN, f2, aep->at_cmnd.fcp_cmnd_task_management, notify.nt_lun, sid, aep->at_rxid);
		notify.nt_ncode = NT_UNKNOWN;
		return;
	}
	isp_async(isp, ISPASYNC_TARGET_NOTIFY, &notify);
}

int
isp_notify_ack(ispsoftc_t *isp, void *arg)
{
	char storage[QENTRY_LEN];
	void *outp;

	/*
	 * This is in case a Task Management Function ends up here.
	 */
	if (IS_24XX(isp) && arg != NULL && (((isphdr_t *)arg)->rqs_entry_type == RQSTYPE_ATIO)) {
		at7_entry_t *aep = arg;
		return (isp_endcmd(isp, aep, NIL_HANDLE, 0, 0, 0));
	}

	outp = isp_getrqentry(isp);
	if (outp == NULL) {
		isp_prt(isp, ISP_LOGWARN, rqo, __func__);
		return (1);
	}

	ISP_MEMZERO(storage, QENTRY_LEN);

	if (IS_24XX(isp)) {
		na_fcentry_24xx_t *na = (na_fcentry_24xx_t *) storage;
		if (arg) {
			in_fcentry_24xx_t *in = arg;
			na->na_nphdl = in->in_nphdl;
			na->na_flags = in->in_flags & IN24XX_FLAG_PUREX_IOCB;
			na->na_status = in->in_status;
			na->na_status_subcode = in->in_status_subcode;
			na->na_rxid = in->in_rxid;
			na->na_oxid = in->in_oxid;
			na->na_vpidx = in->in_vpidx;
			if (in->in_status == IN24XX_SRR_RCVD) {
				na->na_srr_rxid = in->in_srr_rxid;
				na->na_srr_reloff_hi = in->in_srr_reloff_hi;
				na->na_srr_reloff_lo = in->in_srr_reloff_lo;
				na->na_srr_iu = in->in_srr_iu;
				na->na_srr_flags = 1;
				na->na_srr_reject_vunique = 0;
				na->na_srr_reject_explanation = 1;
				na->na_srr_reject_code = 1;
			}
		}
		na->na_header.rqs_entry_type = RQSTYPE_NOTIFY_ACK;
		na->na_header.rqs_entry_count = 1;
		isp_put_notify_24xx_ack(isp, na, (na_fcentry_24xx_t *)outp);
	} else if (IS_FC(isp)) {
		na_fcentry_t *na = (na_fcentry_t *) storage;
		int iid = 0;

		if (arg) {
			in_fcentry_t *inp = arg;
			ISP_MEMCPY(storage, arg, sizeof (isphdr_t));
			if (ISP_CAP_2KLOGIN(isp)) {
				((na_fcentry_e_t *)na)->na_iid = ((in_fcentry_e_t *)inp)->in_iid;
				iid = ((na_fcentry_e_t *)na)->na_iid;
			} else {
				na->na_iid = inp->in_iid;
				iid = na->na_iid;
			}
			na->na_task_flags = inp->in_task_flags & TASK_FLAGS_RESERVED_MASK;
			na->na_seqid = inp->in_seqid;
			na->na_flags = NAFC_RCOUNT;
			na->na_status = inp->in_status;
			if (inp->in_status == IN_RESET) {
				na->na_flags |= NAFC_RST_CLRD;
			}
			if (inp->in_status == IN_MSG_RECEIVED) {
				na->na_flags |= NAFC_TVALID;
				na->na_response = 0;	/* XXX SUCCEEDED XXX */
			}
		} else {
			na->na_flags = NAFC_RST_CLRD;
		}
		na->na_header.rqs_entry_type = RQSTYPE_NOTIFY_ACK;
		na->na_header.rqs_entry_count = 1;
		if (ISP_CAP_2KLOGIN(isp)) {
			isp_put_notify_ack_fc_e(isp, (na_fcentry_e_t *) na, (na_fcentry_e_t *)outp);
		} else {
			isp_put_notify_ack_fc(isp, na, (na_fcentry_t *)outp);
		}
		isp_prt(isp, ISP_LOGTDEBUG0, "notify ack loopid %u seqid %x flags %x tflags %x response %x", iid, na->na_seqid,
		    na->na_flags, na->na_task_flags, na->na_response);
	} else {
		na_entry_t *na = (na_entry_t *) storage;
		if (arg) {
			in_entry_t *inp = arg;
			ISP_MEMCPY(storage, arg, sizeof (isphdr_t));
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
		isp_prt(isp, ISP_LOGTDEBUG0, "notify ack loopid %u lun %u tgt %u seqid %x event %x", na->na_iid, na->na_lun, na->na_tgt, na->na_seqid, na->na_event);
	}
	ISP_TDQE(isp, "isp_notify_ack", isp->isp_reqidx, storage);
	ISP_SYNC_REQUEST(isp);
	return (0);
}

int
isp_acknak_abts(ispsoftc_t *isp, void *arg, int errno)
{
	char storage[QENTRY_LEN];
	uint16_t tmpw;
	uint8_t tmpb;
	abts_t *abts = arg;
	abts_rsp_t *rsp = (abts_rsp_t *) storage;
	void *outp;

	if (!IS_24XX(isp)) {
		isp_prt(isp, ISP_LOGERR, "%s: called for non-24XX card", __func__);
		return (0);
	}

	if (abts->abts_header.rqs_entry_type != RQSTYPE_ABTS_RCVD) {
		isp_prt(isp, ISP_LOGERR, "%s: called for non-ABTS entry (0x%x)", __func__, abts->abts_header.rqs_entry_type);
		return (0);
	}

	outp = isp_getrqentry(isp);
	if (outp == NULL) {
		isp_prt(isp, ISP_LOGWARN, rqo, __func__);
		return (1);
	}

	ISP_MEMCPY(rsp, abts, QENTRY_LEN);
	rsp->abts_rsp_header.rqs_entry_type = RQSTYPE_ABTS_RSP;

	/*
	 * Swap destination and source for response.
	 */
	rsp->abts_rsp_r_ctl = BA_ACC;
	tmpw = rsp->abts_rsp_did_lo;
	tmpb = rsp->abts_rsp_did_hi;
	rsp->abts_rsp_did_lo = rsp->abts_rsp_sid_lo;
	rsp->abts_rsp_did_hi = rsp->abts_rsp_sid_hi;
	rsp->abts_rsp_sid_lo = tmpw;
	rsp->abts_rsp_sid_hi = tmpb;

	rsp->abts_rsp_f_ctl_hi ^= 0x80; 	/* invert Exchange Context */
	rsp->abts_rsp_f_ctl_hi &= ~0x7f;	/* clear Sequence Initiator and other bits */
	rsp->abts_rsp_f_ctl_hi |= 0x10;		/* abort the whole exchange */
	rsp->abts_rsp_f_ctl_hi |= 0x8;		/* last data frame of sequence */
	rsp->abts_rsp_f_ctl_hi |= 0x1;		/* transfer Sequence Initiative */
	rsp->abts_rsp_f_ctl_lo = 0;

	if (errno == 0) {
		uint16_t rx_id, ox_id;

		rx_id = rsp->abts_rsp_rx_id;
		ox_id = rsp->abts_rsp_ox_id;
		ISP_MEMZERO(&rsp->abts_rsp_payload.ba_acc, sizeof (rsp->abts_rsp_payload.ba_acc));
                isp_prt(isp, ISP_LOGTINFO, "[0x%x] ABTS of 0x%x being BA_ACC'd", rsp->abts_rsp_rxid_abts, rsp->abts_rsp_rxid_task);
                rsp->abts_rsp_payload.ba_acc.aborted_rx_id = rx_id;
                rsp->abts_rsp_payload.ba_acc.aborted_ox_id = ox_id;
                rsp->abts_rsp_payload.ba_acc.high_seq_cnt = 0xffff;
	} else {
		ISP_MEMZERO(&rsp->abts_rsp_payload.ba_rjt, sizeof (rsp->abts_rsp_payload.ba_acc));
		switch (errno) {
		case ENOMEM:
			rsp->abts_rsp_payload.ba_rjt.reason = 5;	/* Logical Busy */
			break;
		default:
			rsp->abts_rsp_payload.ba_rjt.reason = 9;	/* Unable to perform command request */
			break;
		}
	}

	/*
	 * The caller will have set response values as appropriate
	 * in the ABTS structure just before calling us.
	 */
	isp_put_abts_rsp(isp, rsp, (abts_rsp_t *)outp);
	ISP_TDQE(isp, "isp_acknak_abts", isp->isp_reqidx, storage);
	ISP_SYNC_REQUEST(isp);
	return (0);
}

static void
isp_handle_atio(ispsoftc_t *isp, at_entry_t *aep)
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

	switch (aep->at_status & ~QLTM_SVALID) {
	case AT_PATH_INVALID:
		/*
		 * ATIO rejected by the firmware due to disabled lun.
		 */
		isp_prt(isp, ISP_LOGERR, "rejected ATIO for disabled lun %d", lun);
		break;
	case AT_NOCAP:
		/*
		 * Requested Capability not available
		 * We sent an ATIO that overflowed the firmware's
		 * command resource count.
		 */
		isp_prt(isp, ISP_LOGERR, "rejected ATIO for lun %d because of command count overflow", lun);
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
		isp_prt(isp, ISP_LOGERR, atiocope, lun, GET_BUS_VAL(aep->at_iid));
		break;

	case AT_CDB:		/* Got a CDB */
	case AT_PHASE_ERROR:	/* Bus Phase Sequence Error */
		/*
		 * Punt to platform specific layer.
		 */
		isp_async(isp, ISPASYNC_TARGET_ACTION, aep);
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
		isp_prt(isp, ISP_LOGWARN, atior, lun, GET_IID_VAL(aep->at_iid), GET_BUS_VAL(aep->at_iid));
		break;


	default:
		isp_prt(isp, ISP_LOGERR, "Unknown ATIO status 0x%x from loopid %d for lun %d", aep->at_status, aep->at_iid, lun);
		(void) isp_target_put_atio(isp, aep);
		break;
	}
}

static void
isp_handle_atio2(ispsoftc_t *isp, at2_entry_t *aep)
{
	int lun, iid;

	if (ISP_CAP_SCCFW(isp)) {
		lun = aep->at_scclun;
	} else {
		lun = aep->at_lun;
	}

	if (ISP_CAP_2KLOGIN(isp)) {
		iid = ((at2e_entry_t *)aep)->at_iid;
	} else {
		iid = aep->at_iid;
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

	switch (aep->at_status & ~QLTM_SVALID) {
	case AT_PATH_INVALID:
		/*
		 * ATIO rejected by the firmware due to disabled lun.
		 */
		isp_prt(isp, ISP_LOGERR, "rejected ATIO2 for disabled lun %d", lun);
		break;
	case AT_NOCAP:
		/*
		 * Requested Capability not available
		 * We sent an ATIO that overflowed the firmware's
		 * command resource count.
		 */
		isp_prt(isp, ISP_LOGERR, "rejected ATIO2 for lun %d- command count overflow", lun);
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
		isp_async(isp, ISPASYNC_TARGET_ACTION, aep);
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
		isp_prt(isp, ISP_LOGERR, atior, lun, iid, 0);
		break;


	default:
		isp_prt(isp, ISP_LOGERR, "Unknown ATIO2 status 0x%x from loopid %d for lun %d", aep->at_status, iid, lun);
		(void) isp_target_put_atio(isp, aep);
		break;
	}
}

static void
isp_handle_ctio(ispsoftc_t *isp, ct_entry_t *ct)
{
	void *xs;
	int pl = ISP_LOGTDEBUG2;
	char *fmsg = NULL;

	if (ct->ct_syshandle) {
		xs = isp_find_xs_tgt(isp, ct->ct_syshandle);
		if (xs == NULL) {
			pl = ISP_LOGALL;
		}
	} else {
		xs = NULL;
	}

	switch (ct->ct_status & ~QLTM_SVALID) {
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
			isp_prt(isp, pl, "CTIO- iid %d disconnected OK", ct->ct_iid);
			return;
		}
		break;

	case CT_BDR_MSG:
		/*
		 * Bus Device Reset message received or the SCSI Bus has
		 * been Reset; the firmware has gone to Bus Free.
		 *
		 * The firmware generates an async mailbox interrupt to
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
		isp_prt(isp, ISP_LOGTDEBUG0, "CTIO destroyed by %s", fmsg);
		break;

	case CT_INVAL:
		/*
		 * CTIO rejected by the firmware due to disabled lun.
		 * "Cannot Happen".
		 */
		isp_prt(isp, ISP_LOGERR, "Firmware rejected CTIO for disabled lun %d", ct->ct_lun);
		break;

	case CT_NOPATH:
		/*
		 * CTIO rejected by the firmware due "no path for the
		 * nondisconnecting nexus specified". This means that
		 * we tried to access the bus while a non-disconnecting
		 * command is in process.
		 */
		isp_prt(isp, ISP_LOGERR, "Firmware rejected CTIO for bad nexus %d/%d/%d", ct->ct_iid, ct->ct_tgt, ct->ct_lun);
		break;

	case CT_RSELTMO:
		fmsg = "Reselection";
		/*FALLTHROUGH*/
	case CT_TIMEOUT:
		if (fmsg == NULL)
			fmsg = "Command";
		isp_prt(isp, ISP_LOGWARN, "Firmware timed out on %s", fmsg);
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
		isp_prt(isp, ISP_LOGERR, "Unknown CTIO status 0x%x", ct->ct_status & ~QLTM_SVALID);
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
				isp_prt(isp, pl, "intermediate CTIO completed ok");
			} else {
				isp_prt(isp, pl, "unmonitored CTIO completed ok");
			}
		} else {
			isp_prt(isp, pl, "NO xs for CTIO (handle 0x%x) status 0x%x", ct->ct_syshandle, ct->ct_status & ~QLTM_SVALID);
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
		isp_async(isp, ISPASYNC_TARGET_ACTION, ct);
	}
}

static void
isp_handle_ctio2(ispsoftc_t *isp, ct2_entry_t *ct)
{
	void *xs;
	int pl = ISP_LOGTDEBUG2;
	char *fmsg = NULL;

	if (ct->ct_syshandle) {
		xs = isp_find_xs_tgt(isp, ct->ct_syshandle);
		if (xs == NULL) {
			pl = ISP_LOGALL;
		}
	} else {
		xs = NULL;
	}

	switch (ct->ct_status & ~QLTM_SVALID) {
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
		 * The firmware generates an async mailbox interrupt to
		 * notify us of this and returns outstanding CTIOs with this
		 * status. These CTIOs are handled in that same way as
		 * CT_ABORTED ones, so just fall through here.
		 */
		fmsg = "TARGET RESET";
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
		if (fmsg == NULL) {
			fmsg = "ABORT";
		}

		isp_prt(isp, ISP_LOGTDEBUG0, "CTIO2 destroyed by %s: RX_ID=0x%x", fmsg, ct->ct_rxid);
		break;

	case CT_INVAL:
		/*
		 * CTIO rejected by the firmware - invalid data direction.
		 */
		isp_prt(isp, ISP_LOGERR, "CTIO2 had wrong data direction");
		break;

	case CT_RSELTMO:
		fmsg = "failure to reconnect to initiator";
		/*FALLTHROUGH*/
	case CT_TIMEOUT:
		if (fmsg == NULL)
			fmsg = "command";
		isp_prt(isp, ISP_LOGWARN, "Firmware timed out on %s", fmsg);
		break;

	case CT_ERR:
		fmsg = "Completed with Error";
		/*FALLTHROUGH*/
	case CT_LOGOUT:
		if (fmsg == NULL)
			fmsg = "Port Logout";
		/*FALLTHROUGH*/
	case CT_PORTUNAVAIL:
		if (fmsg == NULL)
			fmsg = "Port not available";
		/*FALLTHROUGH*/
	case CT_PORTCHANGED:
		if (fmsg == NULL)
			fmsg = "Port Changed";
		/*FALLTHROUGH*/
	case CT_NOACK:
		if (fmsg == NULL)
			fmsg = "unacknowledged Immediate Notify pending";
		isp_prt(isp, ISP_LOGWARN, "CTIO returned by f/w- %s", fmsg);
		break;

	case CT_INVRXID:
		/*
		 * CTIO rejected by the firmware because an invalid RX_ID.
		 * Just print a message.
		 */
		isp_prt(isp, ISP_LOGWARN, "CTIO2 completed with Invalid RX_ID 0x%x", ct->ct_rxid);
		break;

	default:
		isp_prt(isp, ISP_LOGERR, "Unknown CTIO2 status 0x%x", ct->ct_status & ~QLTM_SVALID);
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
			if ((ct->ct_flags & CT2_SENDSTATUS) == 0) {
				isp_prt(isp, pl, "intermediate CTIO completed ok");
			} else {
				isp_prt(isp, pl, "unmonitored CTIO completed ok");
			}
		} else {
			isp_prt(isp, pl, "NO xs for CTIO (handle 0x%x) status 0x%x", ct->ct_syshandle, ct->ct_status & ~QLTM_SVALID);
		}
	} else {
		if ((ct->ct_flags & CT2_DATAMASK) != CT2_NO_DATA) {
			ISP_DMAFREE(isp, xs, ct->ct_syshandle);
		}
		if (ct->ct_flags & CT2_SENDSTATUS) {
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
		isp_async(isp, ISPASYNC_TARGET_ACTION, ct);
		/*
		 * The platform layer will destroy the handle if appropriate.
		 */
	}
}

static void
isp_handle_ctio7(ispsoftc_t *isp, ct7_entry_t *ct)
{
	void *xs;
	int pl = ISP_LOGTDEBUG2;
	char *fmsg = NULL;

	if (ct->ct_syshandle) {
		xs = isp_find_xs_tgt(isp, ct->ct_syshandle);
		if (xs == NULL) {
			pl = ISP_LOGALL;
		}
	} else {
		xs = NULL;
	}

	switch (ct->ct_nphdl) {
	case CT7_BUS_ERROR:
		isp_prt(isp, ISP_LOGERR, "PCI DMA Bus Error");
		/* FALL Through */
	case CT7_DATA_OVER:
	case CT7_DATA_UNDER:
	case CT7_OK:
		/*
		 * There are generally 2 possibilities as to why we'd get
		 * this condition:
		 * 	We sent or received data.
		 * 	We sent status & command complete.
		 */

		break;

	case CT7_RESET:
		if (fmsg == NULL) {
			fmsg = "LIP Reset";
		}
		/*FALLTHROUGH*/
	case CT7_ABORTED:
		/*
		 * When an Abort message is received the firmware goes to
		 * Bus Free and returns all outstanding CTIOs with the status
		 * set, then sends us an Immediate Notify entry.
		 */
		if (fmsg == NULL) {
			fmsg = "ABORT";
		}
		isp_prt(isp, ISP_LOGTDEBUG0, "CTIO7 destroyed by %s: RX_ID=0x%x", fmsg, ct->ct_rxid);
		break;

	case CT7_TIMEOUT:
		if (fmsg == NULL) {
			fmsg = "command";
		}
		isp_prt(isp, ISP_LOGWARN, "Firmware timed out on %s", fmsg);
		break;

	case CT7_ERR:
		fmsg = "Completed with Error";
		/*FALLTHROUGH*/
	case CT7_LOGOUT:
		if (fmsg == NULL) {
			fmsg = "Port Logout";
		}
		/*FALLTHROUGH*/
	case CT7_PORTUNAVAIL:
		if (fmsg == NULL) {
			fmsg = "Port not available";
		}
		/*FALLTHROUGH*/
	case CT7_PORTCHANGED:
		if (fmsg == NULL) {
			fmsg = "Port Changed";
		}
		isp_prt(isp, ISP_LOGWARN, "CTIO returned by f/w- %s", fmsg);
		break;

	case CT7_INVRXID:
		/*
		 * CTIO rejected by the firmware because an invalid RX_ID.
		 * Just print a message.
		 */
		isp_prt(isp, ISP_LOGWARN, "CTIO7 completed with Invalid RX_ID 0x%x", ct->ct_rxid);
		break;

	case CT7_REASSY_ERR:
		isp_prt(isp, ISP_LOGWARN, "reassembly error");
		break;

	case CT7_SRR:
		isp_prt(isp, ISP_LOGWARN, "SRR received");
		break;

	default:
		isp_prt(isp, ISP_LOGERR, "Unknown CTIO7 status 0x%x", ct->ct_nphdl);
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
			if (ct->ct_flags & CT7_TERMINATE) {
				isp_prt(isp, ISP_LOGINFO, "termination of 0x%x complete", ct->ct_rxid);
			} else if ((ct->ct_flags & CT7_SENDSTATUS) == 0) {
				isp_prt(isp, pl, "intermediate CTIO completed ok");
			} else {
				isp_prt(isp, pl, "unmonitored CTIO completed ok");
			}
		} else {
			isp_prt(isp, pl, "NO xs for CTIO (handle 0x%x) status 0x%x", ct->ct_syshandle, ct->ct_nphdl);
		}
	} else {
		if ((ct->ct_flags & CT7_DATAMASK) != CT7_NO_DATA) {
			ISP_DMAFREE(isp, xs, ct->ct_syshandle);
		}
		if (ct->ct_flags & CT7_SENDSTATUS) {
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
		isp_async(isp, ISPASYNC_TARGET_ACTION, ct);
		/*
		 * The platform layer will destroy the handle if appropriate.
		 */
	}
}

static void
isp_handle_24xx_inotify(ispsoftc_t *isp, in_fcentry_24xx_t *inot_24xx)
{
	uint8_t ochan, chan, lochan, hichan;

	/*
	 * Check to see whether we got a wildcard channel.
	 * If so, we have to iterate over all channels.
	 */
	ochan = chan = ISP_GET_VPIDX(isp, inot_24xx->in_vpidx);
	if (chan == 0xff) {
		lochan = 0;
		hichan = isp->isp_nchan;
	} else {
		if (chan >= isp->isp_nchan) {
			char buf[64];
			ISP_SNPRINTF(buf, sizeof buf, "%s: bad channel %d for status 0x%x", __func__, chan, inot_24xx->in_status);
			isp_print_bytes(isp, buf, QENTRY_LEN, inot_24xx);
			(void) isp_notify_ack(isp, inot_24xx);
			return;
		}
		lochan = chan;
		hichan = chan + 1;
	}
	isp_prt(isp, ISP_LOGTDEBUG1, "%s: Immediate Notify Channels %d..%d status=0x%x seqid=0x%x", __func__, lochan, hichan-1, inot_24xx->in_status, inot_24xx->in_rxid);
	for (chan = lochan; chan < hichan; chan++) {
		switch (inot_24xx->in_status) {
		case IN24XX_LIP_RESET:
		case IN24XX_LINK_RESET:
		case IN24XX_PORT_LOGOUT:
		case IN24XX_PORT_CHANGED:
		case IN24XX_LINK_FAILED:
		case IN24XX_SRR_RCVD:
		case IN24XX_ELS_RCVD:
			inot_24xx->in_vpidx = chan;
			isp_async(isp, ISPASYNC_TARGET_ACTION, inot_24xx);
			break;
		default:
			isp_prt(isp, ISP_LOGINFO, "%s: unhandled status (0x%x) for chan %d", __func__, inot_24xx->in_status, chan);
			(void) isp_notify_ack(isp, inot_24xx);
			break;
		}
	}
	inot_24xx->in_vpidx = ochan;
}
#endif
