/* $FreeBSD: src/sys/dev/isp/isp_freebsd.c,v 1.33 2000/03/13 16:30:00 mjacob Exp $ */
/*
 * Platform (FreeBSD) dependent common attachment code for Qlogic adapters.
 *
 *---------------------------------------
 * Copyright (c) 1997, 1998, 1999 by Matthew Jacob
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
 */
#include <dev/isp/isp_freebsd.h>
#include <sys/malloc.h>

static void isp_cam_async(void *, u_int32_t, struct cam_path *, void *);
static void isp_poll(struct cam_sim *);
static void isp_relsim(void *);
static void isp_action(struct cam_sim *, union ccb *);

static struct ispsoftc *isplist = NULL;
/* #define	ISP_LUN0_ONLY	1 */
#ifdef	DEBUG
int isp_debug = 2;
#elif defined(CAMDEBUG) || defined(DIAGNOSTIC)
int isp_debug = 1;
#else
int isp_debug = 0;
#endif

void
isp_attach(struct ispsoftc *isp)
{
	int primary, secondary;
	struct ccb_setasync csa;
	struct cam_devq *devq;
	struct cam_sim *sim;
	struct cam_path *path;

	/*
	 * Establish (in case of 12X0) which bus is the primary.
	 */

	primary = 0;
	secondary = 1;

	/*
	 * Create the device queue for our SIM(s).
	 */
	devq = cam_simq_alloc(isp->isp_maxcmds);
	if (devq == NULL) {
		return;
	}

	/*
	 * Construct our SIM entry.
	 */
	sim = cam_sim_alloc(isp_action, isp_poll, "isp", isp,
	    isp->isp_unit, 1, isp->isp_maxcmds, devq);
	if (sim == NULL) {
		cam_simq_free(devq);
		return;
	}
	if (xpt_bus_register(sim, primary) != CAM_SUCCESS) {
		cam_sim_free(sim, TRUE);
		return;
	}

	if (xpt_create_path(&path, NULL, cam_sim_path(sim),
	    CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		xpt_bus_deregister(cam_sim_path(sim));
		cam_sim_free(sim, TRUE);
		return;
	}

	xpt_setup_ccb(&csa.ccb_h, path, 5);
	csa.ccb_h.func_code = XPT_SASYNC_CB;
	csa.event_enable = AC_LOST_DEVICE;
	csa.callback = isp_cam_async;
	csa.callback_arg = sim;
	xpt_action((union ccb *)&csa);
	isp->isp_sim = sim;
	isp->isp_path = path;

	/*
	 * If we have a second channel, construct SIM entry for that.
	 */
	if (IS_DUALBUS(isp)) {
		sim = cam_sim_alloc(isp_action, isp_poll, "isp", isp,
		    isp->isp_unit, 1, isp->isp_maxcmds, devq);
		if (sim == NULL) {
			xpt_bus_deregister(cam_sim_path(isp->isp_sim));
			xpt_free_path(isp->isp_path);
			cam_simq_free(devq);
			return;
		}
		if (xpt_bus_register(sim, secondary) != CAM_SUCCESS) {
			xpt_bus_deregister(cam_sim_path(isp->isp_sim));
			xpt_free_path(isp->isp_path);
			cam_sim_free(sim, TRUE);
			return;
		}

		if (xpt_create_path(&path, NULL, cam_sim_path(sim),
		    CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
			xpt_bus_deregister(cam_sim_path(isp->isp_sim));
			xpt_free_path(isp->isp_path);
			xpt_bus_deregister(cam_sim_path(sim));
			cam_sim_free(sim, TRUE);
			return;
		}

		xpt_setup_ccb(&csa.ccb_h, path, 5);
		csa.ccb_h.func_code = XPT_SASYNC_CB;
		csa.event_enable = AC_LOST_DEVICE;
		csa.callback = isp_cam_async;
		csa.callback_arg = sim;
		xpt_action((union ccb *)&csa);
		isp->isp_sim2 = sim;
		isp->isp_path2 = path;
	}
	isp->isp_state = ISP_RUNSTATE;
	if (isplist == NULL) {
		isplist = isp;
	} else {
		struct ispsoftc *tmp = isplist;
		while (tmp->isp_osinfo.next) {
			tmp = tmp->isp_osinfo.next;
		}
		tmp->isp_osinfo.next = isp;
	}
}


/*
 * Put the target mode functions here, because some are inlines
 */

#ifdef	ISP_TARGET_MODE
#include "targbh.h"

static __inline int is_lun_enabled(struct ispsoftc *, lun_id_t);
static __inline int are_any_luns_enabled(struct ispsoftc *);
static __inline tstate_t *get_lun_statep(struct ispsoftc *, lun_id_t);
static __inline void rls_lun_statep(struct ispsoftc *, tstate_t *);
static __inline int isp_psema_sig_rqe(struct ispsoftc *);
static __inline int isp_cv_wait_timed_rqe(struct ispsoftc *, int);
static __inline void isp_cv_signal_rqe(struct ispsoftc *, int);
static __inline void isp_vsema_rqe(struct ispsoftc *);
static cam_status
create_lun_state(struct ispsoftc *, struct cam_path *, tstate_t **);
static void destroy_lun_state(struct ispsoftc *, tstate_t *);
static void isp_en_lun(struct ispsoftc *, union ccb *);
static cam_status isp_abort_tgt_ccb(struct ispsoftc *, union ccb *);
static cam_status isp_target_start_ctio(struct ispsoftc *, union ccb *);


static int isp_handle_platform_atio(struct ispsoftc *, at_entry_t *);
static int isp_handle_platform_atio2(struct ispsoftc *, at2_entry_t *);
static int isp_handle_platform_ctio(struct ispsoftc *, void *);

static __inline int
is_lun_enabled(struct ispsoftc *isp, lun_id_t lun)
{
	tstate_t *tptr;
	int s = splsoftcam();
	if ((tptr = isp->isp_osinfo.lun_hash[LUN_HASH_FUNC(lun)]) == NULL) {
		splx(s);
		return (0);
	}
	do {
		if (tptr->lun == (lun_id_t) lun) {
			splx(s);
			return (1);
		}
	} while ((tptr = tptr->next) != NULL);
	splx(s);
	return (0);
}

static __inline int
are_any_luns_enabled(struct ispsoftc *isp)
{
	int i;
	for (i = 0; i < LUN_HASH_SIZE; i++) {
		if (isp->isp_osinfo.lun_hash[i]) {
			return (1);
		}
	}
	return (0);
}

static __inline tstate_t *
get_lun_statep(struct ispsoftc *isp, lun_id_t lun)
{
	tstate_t *tptr;
	int s;

	s = splsoftcam();
	if (lun == CAM_LUN_WILDCARD) {
		tptr = &isp->isp_osinfo.tsdflt;
		tptr->hold++;
		splx(s);
		return (tptr);
	} else {
		tptr = isp->isp_osinfo.lun_hash[LUN_HASH_FUNC(lun)];
	}
	if (tptr == NULL) {
		splx(s);
		return (NULL);
	}

	do {
		if (tptr->lun == lun) {
			tptr->hold++;
			splx(s);
			return (tptr);
		}
	} while ((tptr = tptr->next) != NULL);
	splx(s);
	return (tptr);
}

static __inline void
rls_lun_statep(struct ispsoftc *isp, tstate_t *tptr)
{
	if (tptr->hold)
		tptr->hold--;
}

static __inline int
isp_psema_sig_rqe(struct ispsoftc *isp)
{
	int s = splcam();
	while (isp->isp_osinfo.tmflags & TM_BUSY) {
		isp->isp_osinfo.tmflags |= TM_WANTED;
		if (tsleep(&isp->isp_osinfo.tmflags, PRIBIO|PCATCH, "i0", 0)) {
			splx(s);
			return (-1);
		}
		isp->isp_osinfo.tmflags |= TM_BUSY;
	}
	splx(s);
	return (0);
}

static __inline int
isp_cv_wait_timed_rqe(struct ispsoftc *isp, int timo)
{
	int s = splcam();
	if (tsleep(&isp->isp_osinfo.rstatus, PRIBIO, "qt1", timo)) {
		splx(s);
		return (-1);
	}
	splx(s);
	return (0);
}

static __inline void
isp_cv_signal_rqe(struct ispsoftc *isp, int status)
{
	isp->isp_osinfo.rstatus = status;
	wakeup(&isp->isp_osinfo.rstatus);
}

static __inline void
isp_vsema_rqe(struct ispsoftc *isp)
{
	int s = splcam();
	if (isp->isp_osinfo.tmflags & TM_WANTED) {
		isp->isp_osinfo.tmflags &= ~TM_WANTED;
		wakeup(&isp->isp_osinfo.tmflags);
	}
	isp->isp_osinfo.tmflags &= ~TM_BUSY;
	splx(s);
}

static cam_status
create_lun_state(struct ispsoftc *isp, struct cam_path *path, tstate_t **rslt)
{
	int s;
	cam_status status;
	lun_id_t lun;
	tstate_t *tptr, *new;

	lun = xpt_path_lun_id(path);
	if (lun < 0) {
		return (CAM_LUN_INVALID);
	}
	if (is_lun_enabled(isp, lun)) {
		return (CAM_LUN_ALRDY_ENA);
	}
	new = (tstate_t *) malloc(sizeof (tstate_t), M_DEVBUF, M_NOWAIT);
	if (new == NULL) {
		return (CAM_RESRC_UNAVAIL);
	}
	bzero(new, sizeof (tstate_t));

	status = xpt_create_path(&new->owner, NULL, xpt_path_path_id(path),
	    xpt_path_target_id(path), xpt_path_lun_id(path));
	if (status != CAM_REQ_CMP) {
		free(new, M_DEVBUF);
		return (status);
	}
	new->lun = lun;
	SLIST_INIT(&new->atios);
	SLIST_INIT(&new->inots);
	new->hold = 1;

	s = splsoftcam();
	if ((tptr = isp->isp_osinfo.lun_hash[LUN_HASH_FUNC(lun)]) == NULL) {
		isp->isp_osinfo.lun_hash[LUN_HASH_FUNC(lun)] = new;
	} else {
		while (tptr->next)
			tptr = tptr->next;
		tptr->next = new;
	}
	splx(s);
	*rslt = new;
	return (CAM_REQ_CMP);
}

static __inline void
destroy_lun_state(struct ispsoftc *isp, tstate_t *tptr)
{
	tstate_t *lw, *pw;
	int s;

	s = splsoftcam();
	if (tptr->hold) {
		splx(s);
		return;
	}
	pw = isp->isp_osinfo.lun_hash[LUN_HASH_FUNC(tptr->lun)];
	if (pw == NULL) {
		splx(s);
		return;
	} else if (pw->lun == tptr->lun) {
		isp->isp_osinfo.lun_hash[LUN_HASH_FUNC(tptr->lun)] = pw->next;
	} else {
		lw = pw;
		pw = lw->next;
		while (pw) {
			if (pw->lun == tptr->lun) {
				lw->next = pw->next;
				break;
			}
			lw = pw;
			pw = pw->next;
		}
		if (pw == NULL) {
			splx(s);
			return;
		}
	}
	free(tptr, M_DEVBUF);
	splx(s);
}

static void
isp_en_lun(struct ispsoftc *isp, union ccb *ccb)
{
	const char *lfmt = "Lun now %sabled for target mode\n";
	struct ccb_en_lun *cel = &ccb->cel;
	tstate_t *tptr;
	u_int16_t rstat;
	int bus, s;
	lun_id_t lun;
	target_id_t tgt;


	bus = XS_CHANNEL(ccb);
	tgt = ccb->ccb_h.target_id;
	lun = ccb->ccb_h.target_lun;

	/*
	 * First, check to see if we're enabling on fibre channel
	 * and don't yet have a notion of who the heck we are (no
	 * loop yet). We do this by
	 */
	if (IS_FC(isp) && cel->enable &&
	    (isp->isp_osinfo.tmflags & TM_TMODE_ENABLED) == 0) {
		int rv;
		fcparam *fcp = isp->isp_param;

		s = splcam();
		rv = isp_control(isp, ISPCTL_FCLINK_TEST, NULL);
		(void) splx(s);
		if (rv || fcp->isp_fwstate != FW_READY) {
			xpt_print_path(ccb->ccb_h.path);
			printf("link status not good yet\n");
			ccb->ccb_h.status = CAM_REQ_CMP_ERR;
			return;
		}
		s = splcam();
		rv = isp_control(isp, ISPCTL_PDB_SYNC, NULL);
		(void) splx(s);
		if (rv || fcp->isp_loopstate != LOOP_READY) {
			xpt_print_path(ccb->ccb_h.path);
			printf("could not get a good port database\n");
			ccb->ccb_h.status = CAM_REQ_CMP_ERR;
			return;
		}
	}


	/*
	 * Next check to see whether this is a target/lun wildcard action.
	 *
	 * If so, we enable/disable target mode but don't do any lun enabling.
	 */
	if (lun == CAM_LUN_WILDCARD && tgt == CAM_TARGET_WILDCARD) {
		int av;
		tptr = &isp->isp_osinfo.tsdflt;
		if (cel->enable) {
			if (isp->isp_osinfo.tmflags & TM_TMODE_ENABLED) {
				ccb->ccb_h.status = CAM_LUN_ALRDY_ENA;
				return;
			}
			ccb->ccb_h.status =
			    xpt_create_path(&tptr->owner, NULL,
			    xpt_path_path_id(ccb->ccb_h.path),
			    xpt_path_target_id(ccb->ccb_h.path),
			    xpt_path_lun_id(ccb->ccb_h.path));
			if (ccb->ccb_h.status != CAM_REQ_CMP) {
				return;
			}
			SLIST_INIT(&tptr->atios);
			SLIST_INIT(&tptr->inots);
			av = 1;
			s = splcam();
			av = isp_control(isp, ISPCTL_TOGGLE_TMODE, &av);
			if (av) {
				ccb->ccb_h.status = CAM_FUNC_NOTAVAIL;
				xpt_free_path(tptr->owner);
				splx(s);
				return;
			}
			isp->isp_osinfo.tmflags |= TM_TMODE_ENABLED;
			splx(s);
		} else {
			if ((isp->isp_osinfo.tmflags & TM_TMODE_ENABLED) == 0) {
				ccb->ccb_h.status = CAM_LUN_INVALID;
				return;
			}
			if (are_any_luns_enabled(isp)) {
				ccb->ccb_h.status = CAM_SCSI_BUSY;
				return;
			}
			av = 0;
			s = splcam();
			av = isp_control(isp, ISPCTL_TOGGLE_TMODE, &av);
			if (av) {
				ccb->ccb_h.status = CAM_FUNC_NOTAVAIL;
				splx(s);
				return;
			}
			isp->isp_osinfo.tmflags &= ~TM_TMODE_ENABLED;
			splx(s);
			ccb->ccb_h.status = CAM_REQ_CMP;
		}
		xpt_print_path(ccb->ccb_h.path);
		printf(lfmt, (cel->enable) ? "en" : "dis");
		return;
	}

	/*
	 * Do some sanity checking first.
	 */

	if (IS_SCSI(isp)) {
		if (lun < 0 || lun >= 32) {
			ccb->ccb_h.status = CAM_LUN_INVALID;
			return;
		}
		if (tgt != CAM_TARGET_WILDCARD &&
		    tgt != ((sdparam *) isp->isp_param)->isp_initiator_id) {
			ccb->ccb_h.status = CAM_TID_INVALID;
			return;
		}
	} else {
#ifdef	ISP2100_SCCLUN
		if (lun < 0 || lun >= 65536) {
			ccb->ccb_h.status = CAM_LUN_INVALID;
			return;
		}
#else
		if (lun < 0 || lun >= 16) {
			ccb->ccb_h.status = CAM_LUN_INVALID;
			return;
		}
#endif
		if (tgt != CAM_TARGET_WILDCARD &&
		    tgt != ((fcparam *) isp->isp_param)->isp_loopid) {
			ccb->ccb_h.status = CAM_TID_INVALID;
			return;
		}
	}


	if (cel->enable) {
		ccb->ccb_h.status =
		    create_lun_state(isp, ccb->ccb_h.path, &tptr);
		if (ccb->ccb_h.status != CAM_REQ_CMP) {
			return;
		}
	} else {
		tptr = get_lun_statep(isp, lun);
		if (tptr == NULL) {
			ccb->ccb_h.status = CAM_LUN_INVALID;
			return;
		}
	}

	if (isp_psema_sig_rqe(isp)) {
		rls_lun_statep(isp, tptr);
		if (cel->enable)
			destroy_lun_state(isp, tptr);
		ccb->ccb_h.status = CAM_REQ_CMP_ERR;
		return;
	}

	s = splcam();
	if (cel->enable) {
		u_int32_t seq = isp->isp_osinfo.rollinfo++;
		rstat = LUN_ERR;
		if (isp_lun_cmd(isp, RQSTYPE_ENABLE_LUN, bus, tgt, lun, seq)) {
			xpt_print_path(ccb->ccb_h.path);
			printf("isp_lun_cmd failed\n");
			goto out;
		}
		if (isp_cv_wait_timed_rqe(isp, 30 * hz)) {
			xpt_print_path(ccb->ccb_h.path);
			printf("wait for ENABLE LUN timed out\n");
			goto out;
		}
		rstat = isp->isp_osinfo.rstatus;
		if (rstat != LUN_OK) {
			xpt_print_path(ccb->ccb_h.path);
			printf("ENABLE LUN returned 0x%x\n", rstat);
			goto out;
		}
	} else {
		u_int32_t seq;

		seq = isp->isp_osinfo.rollinfo++;
		rstat = LUN_ERR;

		if (isp_lun_cmd(isp, -RQSTYPE_MODIFY_LUN, bus, tgt, lun, seq)) {
			xpt_print_path(ccb->ccb_h.path);
			printf("isp_lun_cmd failed\n");
			goto out;
		}
		if (isp_cv_wait_timed_rqe(isp, 30 * hz)) {
			xpt_print_path(ccb->ccb_h.path);
			printf("wait for MODIFY LUN timed out\n");
			goto out;
		}
		rstat = isp->isp_osinfo.rstatus;
		if (rstat != LUN_OK) {
			xpt_print_path(ccb->ccb_h.path);
			printf("MODIFY LUN returned 0x%x\n", rstat);
			goto out;
		}
		rstat = LUN_ERR;
		seq = isp->isp_osinfo.rollinfo++;

		if (isp_lun_cmd(isp, -RQSTYPE_ENABLE_LUN, bus, tgt, lun, seq)) {
			xpt_print_path(ccb->ccb_h.path);
			printf("isp_lun_cmd failed\n");
			goto out;
		}
		if (isp_cv_wait_timed_rqe(isp, 30 * hz)) {
			xpt_print_path(ccb->ccb_h.path);
			printf("wait for ENABLE LUN timed out\n");
			goto out;
		}
		rstat = isp->isp_osinfo.rstatus;
		if (rstat != LUN_OK) {
			xpt_print_path(ccb->ccb_h.path);
			printf("ENABLE LUN returned 0x%x\n", rstat);
			goto out;
		}
	}
out:
	isp_vsema_rqe(isp);
	splx(s);

	if (rstat != LUN_OK) {
		xpt_print_path(ccb->ccb_h.path);
		printf("lun %sable failed\n", (cel->enable) ? "en" : "dis");
		ccb->ccb_h.status = CAM_REQ_CMP_ERR;
		rls_lun_statep(isp, tptr);
		if (cel->enable)
			destroy_lun_state(isp, tptr);
	} else {
		xpt_print_path(ccb->ccb_h.path);
		printf(lfmt, (cel->enable) ? "en" : "dis");
		rls_lun_statep(isp, tptr);
		if (cel->enable == 0) {
			destroy_lun_state(isp, tptr);
		}
		ccb->ccb_h.status = CAM_REQ_CMP;
	}
}

static cam_status
isp_abort_tgt_ccb(struct ispsoftc *isp, union ccb *ccb)
{
	tstate_t *tptr;
	struct ccb_hdr_slist *lp;
	struct ccb_hdr *curelm;
	int found;
	union ccb *accb = ccb->cab.abort_ccb;

	if (accb->ccb_h.target_id != CAM_TARGET_WILDCARD) {
		if (IS_FC(isp) && (accb->ccb_h.target_id != 
		    ((fcparam *) isp->isp_param)->isp_loopid)) {
			return (CAM_PATH_INVALID);
		} else if (IS_SCSI(isp) && (accb->ccb_h.target_id != 
		    ((sdparam *) isp->isp_param)->isp_initiator_id)) {
			return (CAM_PATH_INVALID);
		}
	}
	tptr = get_lun_statep(isp, accb->ccb_h.target_lun);
	if (tptr == NULL) {
		return (CAM_PATH_INVALID);
	}
	if (accb->ccb_h.func_code == XPT_ACCEPT_TARGET_IO) {
		lp = &tptr->atios;
	} else if (accb->ccb_h.func_code == XPT_IMMED_NOTIFY) {
		lp = &tptr->inots;
	} else {
		rls_lun_statep(isp, tptr);
		return (CAM_UA_ABORT);
	}
	curelm = SLIST_FIRST(lp);
	found = 0;
	if (curelm == &accb->ccb_h) {
		found = 1;
		SLIST_REMOVE_HEAD(lp, sim_links.sle);
	} else {
		while(curelm != NULL) {
			struct ccb_hdr *nextelm;

			nextelm = SLIST_NEXT(curelm, sim_links.sle);
			if (nextelm == &accb->ccb_h) {
				found = 1;
				SLIST_NEXT(curelm, sim_links.sle) =
				    SLIST_NEXT(nextelm, sim_links.sle);
				break;
			}
			curelm = nextelm;
		}
	}
	rls_lun_statep(isp, tptr);
	if (found) {
		accb->ccb_h.status = CAM_REQ_ABORTED;
		return (CAM_REQ_CMP);
	}
	return(CAM_PATH_INVALID);
}

static cam_status
isp_target_start_ctio(struct ispsoftc *isp, union ccb *ccb)
{
	void *qe;
	struct ccb_scsiio *cso = &ccb->csio;
	u_int32_t *hp, save_handle;
	u_int16_t iptr, optr;

	if (isp_getrqentry(isp, &iptr, &optr, &qe)) {
		xpt_print_path(ccb->ccb_h.path);
		printf("Request Queue Overflow in isp_target_start_ctio\n");
		return (CAM_RESRC_UNAVAIL);
	}
	bzero(qe, QENTRY_LEN);

	/*
	 * We're either moving data or completing a command here.
	 */

	if (IS_FC(isp)) {
		ct2_entry_t *cto = qe;
		u_int16_t *ssptr = NULL;

		cto->ct_header.rqs_entry_type = RQSTYPE_CTIO2;
		cto->ct_header.rqs_entry_count = 1;
		cto->ct_iid = cso->init_id;
#ifndef	ISP2100_SCCLUN
		cto->ct_lun = ccb->ccb_h.target_lun;
#endif
		cto->ct_rxid = cso->tag_id;
		cto->ct_flags = CT2_CCINCR;
		if (cso->dxfer_len == 0) {
			cto->ct_flags |= CT2_FLAG_MODE1 | CT2_NO_DATA;
			KASSERT(ccb->ccb_h.flags & CAM_SEND_STATUS,
			    ("a CTIO with no data and no status?"));
			cto->ct_flags |= CT2_SENDSTATUS;
			ssptr = &cto->rsp.m1.ct_scsi_status;
			*ssptr = cso->scsi_status;
			if ((ccb->ccb_h.flags & CAM_SEND_SENSE) != 0) {
				int m = min(cso->sense_len, MAXRESPLEN);
				bcopy(&cso->sense_data, cto->rsp.m1.ct_resp, m);
				cto->rsp.m1.ct_senselen = m;
				cto->rsp.m1.ct_scsi_status |= CT2_SNSLEN_VALID;
			}
		} else {
			cto->ct_flags |= CT2_FLAG_MODE0;
			if ((cso->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN) {
				cto->ct_flags |= CT2_DATA_IN;
			} else {
				cto->ct_flags |= CT2_DATA_OUT;
			}
			if ((ccb->ccb_h.flags & CAM_SEND_STATUS) != 0) {
				ssptr = &cto->rsp.m0.ct_scsi_status;
				cto->ct_flags |= CT2_SENDSTATUS;
				cto->rsp.m0.ct_scsi_status = cso->scsi_status;
			}
			ccb->ccb_h.flags &= ~CAM_SEND_SENSE;
		}
		if (ssptr && cso->resid) {
			cto->ct_resid = cso->resid;
			if (cso->resid < 0)
				*ssptr |= CT2_DATA_OVER;
			else
				*ssptr |= CT2_DATA_UNDER;
		}
		if (isp_tdebug > 1 && ssptr &&
		    (cso->scsi_status != SCSI_STATUS_OK || cso->resid)) {
			printf("%s:CTIO2 RX_ID 0x%x SCSI STATUS 0x%x "
			    "resid %d\n", isp->isp_name, cto->ct_rxid,
			    cso->scsi_status, cso->resid);
		}
		hp = &cto->ct_reserved;
	} else {
		ct_entry_t *cto = qe;

		cto->ct_header.rqs_entry_type = RQSTYPE_CTIO;
		cto->ct_header.rqs_entry_count = 1;
		cto->ct_iid = cso->init_id;
		cto->ct_tgt = ccb->ccb_h.target_id;
		cto->ct_lun = ccb->ccb_h.target_lun;
		cto->ct_tag_type = cso->tag_action;
		cto->ct_tag_val = cso->tag_id;
		cto->ct_flags = CT_CCINCR;
		if (cso->dxfer_len) {
			cto->ct_flags |= CT_NO_DATA;
		} else if ((cso->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN) {
			cto->ct_flags |= CT_DATA_IN;
		} else {
			cto->ct_flags |= CT_DATA_OUT;
		}
		if ((ccb->ccb_h.flags & CAM_SEND_STATUS) != 0) {
			cto->ct_flags |= CT_SENDSTATUS;
			cto->ct_scsi_status = cso->scsi_status;
			cto->ct_resid = cso->resid;
		}
		if (isp_tdebug > 1 &&
		    (cso->scsi_status != SCSI_STATUS_OK || cso->resid)) {
			printf("%s:CTIO SCSI STATUS 0x%x resid %d\n",
			    isp->isp_name, cso->scsi_status, cso->resid);
		}
		hp = &cto->ct_reserved;
		ccb->ccb_h.flags &= ~CAM_SEND_SENSE;
	}

	if (isp_save_xs(isp, (ISP_SCSI_XFER_T *)ccb, hp)) {
		xpt_print_path(ccb->ccb_h.path);
		printf("No XFLIST pointers for isp_target_start_ctio\n");
		return (CAM_RESRC_UNAVAIL);
	}


	/*
	 * Call the dma setup routines for this entry (and any subsequent
	 * CTIOs) if there's data to move, and then tell the f/w it's got
	 * new things to play with. As with ispscsicmd's usage of DMA setup,
	 * any swizzling is done in the machine dependent layer. Because
	 * of this, we put the request onto the queue area first in native
	 * format.
	 */

	save_handle = *hp;
	switch (ISP_DMASETUP(isp, cso, qe, &iptr, optr)) {
	case CMD_QUEUED:
		MemoryBarrier();
		ISP_ADD_REQUEST(isp, iptr);
		return (CAM_REQ_INPROG);

	case CMD_EAGAIN:
		ccb->ccb_h.status = CAM_RESRC_UNAVAIL;
		isp_destroy_handle(isp, save_handle);
		return (CAM_RESRC_UNAVAIL);

	default:
		isp_destroy_handle(isp, save_handle);
		return (ccb->ccb_h.spriv_field0);
	}
}

/*
 * Handle ATIO stuff that the generic code can't.
 * This means handling CDBs.
 */

static int
isp_handle_platform_atio(struct ispsoftc *isp, at_entry_t *aep)
{
	tstate_t *tptr;
	int status;
	struct ccb_accept_tio *atiop;

	/*
	 * The firmware status (except for the QLTM_SVALID bit)
	 * indicates why this ATIO was sent to us.
	 *
	 * If QLTM_SVALID is set, the firware has recommended Sense Data.
	 *
	 * If the DISCONNECTS DISABLED bit is set in the flags field,
	 * we're still connected on the SCSI bus - i.e. the initiator
	 * did not set DiscPriv in the identify message. We don't care
	 * about this so it's ignored.
	 */
	status = aep->at_status;
	if ((status & ~QLTM_SVALID) == AT_PHASE_ERROR) {
		/*
		 * Bus Phase Sequence error. We should have sense data
		 * suggested by the f/w. I'm not sure quite yet what
		 * to do about this for CAM.
		 */
		printf("%s: PHASE ERROR\n", isp->isp_name);
		isp_endcmd(isp, aep, SCSI_STATUS_BUSY, 0);
		return (0);
	}
	if ((status & ~QLTM_SVALID) != AT_CDB) {
		printf("%s: bogus atio (0x%x) leaked to platform\n",
		    isp->isp_name, status);
		isp_endcmd(isp, aep, SCSI_STATUS_BUSY, 0);
		return (0);
	}

	tptr = get_lun_statep(isp, aep->at_lun);
	if (tptr == NULL) {
		tptr = get_lun_statep(isp, CAM_LUN_WILDCARD);
	}

	if (tptr == NULL) {
		/*
		 * Because we can't autofeed sense data back with
		 * a command for parallel SCSI, we can't give back
		 * a CHECK CONDITION. We'll give back a BUSY status
		 * instead. This works out okay because the only
		 * time we should, in fact, get this, is in the
		 * case that somebody configured us without the
		 * blackhole driver, so they get what they deserve.
		 */
		isp_endcmd(isp, aep, SCSI_STATUS_BUSY, 0);
		return (0);
	}

	atiop = (struct ccb_accept_tio *) SLIST_FIRST(&tptr->atios);
	if (atiop == NULL) {
		/*
		 * Because we can't autofeed sense data back with
		 * a command for parallel SCSI, we can't give back
		 * a CHECK CONDITION. We'll give back a QUEUE FULL status
		 * instead. This works out okay because the only time we
		 * should, in fact, get this, is in the case that we've
		 * run out of ATIOS.
		 */
		xpt_print_path(tptr->owner);
		printf("no ATIOS for lun %d from initiator %d\n",
		    aep->at_lun, aep->at_iid);
		rls_lun_statep(isp, tptr);
		if (aep->at_flags & AT_TQAE)
			isp_endcmd(isp, aep, SCSI_STATUS_QUEUE_FULL, 0);
		else
			isp_endcmd(isp, aep, SCSI_STATUS_BUSY, 0);
		return (0);
	}
	SLIST_REMOVE_HEAD(&tptr->atios, sim_links.sle);
	if (tptr == &isp->isp_osinfo.tsdflt) {
		atiop->ccb_h.target_id = aep->at_tgt;
		atiop->ccb_h.target_lun = aep->at_lun;
	}
	if (aep->at_flags & AT_NODISC) {
		xpt_print_path(tptr->owner);
		printf("incoming command that cannot disconnect\n");
	}


	atiop->init_id = aep->at_iid;
	atiop->cdb_len = aep->at_cdblen;
	MEMCPY(atiop->cdb_io.cdb_bytes, aep->at_cdb, aep->at_cdblen);
	atiop->ccb_h.status = CAM_CDB_RECVD;
	if ((atiop->tag_action = aep->at_tag_type) != 0) {
		atiop->tag_id = aep->at_tag_val;
		atiop->ccb_h.status |= CAM_TAG_ACTION_VALID;
	}
	xpt_done((union ccb*)atiop);
	if (isp_tdebug > 1) {
		printf("%s:ATIO CDB=0x%x iid%d->lun%d tag 0x%x ttype 0x%x\n",
		    isp->isp_name, aep->at_cdb[0] & 0xff, aep->at_iid,
		    aep->at_lun, aep->at_tag_val & 0xff, aep->at_tag_type);
	}
	rls_lun_statep(isp, tptr);
	return (0);
}

static int
isp_handle_platform_atio2(struct ispsoftc *isp, at2_entry_t *aep)
{
	lun_id_t lun;
	tstate_t *tptr;
	struct ccb_accept_tio *atiop;

	/*
	 * The firmware status (except for the QLTM_SVALID bit)
	 * indicates why this ATIO was sent to us.
	 *
	 * If QLTM_SVALID is set, the firware has recommended Sense Data.
	 */
	if ((aep->at_status & ~QLTM_SVALID) != AT_CDB) {
		printf("%s: bogus atio (0x%x) leaked to platform\n",
		    isp->isp_name, aep->at_status);
		isp_endcmd(isp, aep, SCSI_STATUS_BUSY, 0);
		return (0);
	}

#ifdef	ISP2100_SCCLUN
	lun = aep->at_scclun;
#else
	lun = aep->at_lun;
#endif
	tptr = get_lun_statep(isp, lun);
	if (tptr == NULL) {
		tptr = get_lun_statep(isp, CAM_LUN_WILDCARD);
	}

	if (tptr == NULL) {
#if	0
		/* XXX WE REALLY NEED A HARDWIRED SENSE/INQ CTIO TO USE XXX */
		u_int32_t ccode = SCSI_STATUS_CHECK_COND | ECMD_SVALID;
#if	NTARGBH > 0
		/* Not Ready, Unit Not Self-Configured yet.... */
		ccode |= (SSD_KEY_NOT_READY << 8) | (0x3E << 24);
#else
		/* Illegal Request, Unit Not Self-Configured yet.... */
		ccode |= (SSD_KEY_ILLEGAL_REQUEST << 8) | (0x25 << 24);
#endif
#else
		u_int32_t ccode = SCSI_STATUS_BUSY;
#endif
		
		/*
		 * Because we can't autofeed sense data back with
		 * a command for parallel SCSI, we can't give back
		 * a CHECK CONDITION. We'll give back a BUSY status
		 * instead. This works out okay because the only
		 * time we should, in fact, get this, is in the
		 * case that somebody configured us without the
		 * blackhole driver, so they get what they deserve.
		 */
		isp_endcmd(isp, aep, ccode, 0);
		return (0);
	}

	atiop = (struct ccb_accept_tio *) SLIST_FIRST(&tptr->atios);
	if (atiop == NULL) {
		/*
		 * Because we can't autofeed sense data back with
		 * a command for parallel SCSI, we can't give back
		 * a CHECK CONDITION. We'll give back a QUEUE FULL status
		 * instead. This works out okay because the only time we
		 * should, in fact, get this, is in the case that we've
		 * run out of ATIOS.
		 */
		xpt_print_path(tptr->owner);
		printf("no ATIOS for lun %d from initiator %d\n",
		    lun, aep->at_iid);
		rls_lun_statep(isp, tptr);
		if (aep->at_flags & AT_TQAE)
			isp_endcmd(isp, aep, SCSI_STATUS_QUEUE_FULL, 0);
		else
			isp_endcmd(isp, aep, SCSI_STATUS_BUSY, 0);
		return (0);
	}
	SLIST_REMOVE_HEAD(&tptr->atios, sim_links.sle);
	if (tptr == &isp->isp_osinfo.tsdflt) {
		atiop->ccb_h.target_id =
			((fcparam *)isp->isp_param)->isp_loopid;
		atiop->ccb_h.target_lun = lun;
	}
	atiop->init_id = aep->at_iid;
	atiop->cdb_len = ATIO2_CDBLEN;
	MEMCPY(atiop->cdb_io.cdb_bytes, aep->at_cdb, ATIO2_CDBLEN);
	atiop->ccb_h.status = CAM_CDB_RECVD;
	atiop->tag_id = aep->at_rxid;
	switch (aep->at_taskflags & ATIO2_TC_ATTR_MASK) {
	case ATIO2_TC_ATTR_SIMPLEQ:
		atiop->tag_action = MSG_SIMPLE_Q_TAG;
		break;
        case ATIO2_TC_ATTR_HEADOFQ:
		atiop->tag_action = MSG_HEAD_OF_Q_TAG;
		break;
        case ATIO2_TC_ATTR_ORDERED:
		atiop->tag_action = MSG_ORDERED_Q_TAG;
		break;
        case ATIO2_TC_ATTR_ACAQ:		/* ?? */
	case ATIO2_TC_ATTR_UNTAGGED:
	default:
		atiop->tag_action = 0;
		break;
	}
	if (atiop->tag_action != 0) {
		atiop->ccb_h.status |= CAM_TAG_ACTION_VALID;
	}
	xpt_done((union ccb*)atiop);
	if (isp_tdebug > 1) {
		printf("%s:ATIO2 RX_ID 0x%x CDB=0x%x iid%d->lun%d tattr 0x%x\n",
		    isp->isp_name, aep->at_rxid & 0xffff, aep->at_cdb[0] & 0xff,
		    aep->at_iid, lun, aep->at_taskflags);
	}
	rls_lun_statep(isp, tptr);
	return (0);
}

static int
isp_handle_platform_ctio(struct ispsoftc *isp, void * arg)
{
	union ccb *ccb;
	int sentstatus, ok;

	/*
	 * CTIO and CTIO2 are close enough....
	 */

	ccb = (union ccb *) isp_find_xs(isp, ((ct_entry_t *)arg)->ct_reserved);
	KASSERT((ccb != NULL), ("null ccb in isp_handle_platform_ctio"));
	isp_destroy_handle(isp, ((ct_entry_t *)arg)->ct_reserved);

	if (IS_FC(isp)) {
		ct2_entry_t *ct = arg;
		sentstatus = ct->ct_flags & CT2_SENDSTATUS;
		ok = (ct->ct_status & ~QLTM_SVALID) == CT_OK;
		if (ok && ccb->ccb_h.flags & CAM_SEND_SENSE) {
			ccb->ccb_h.status |= CAM_SENT_SENSE;
		}
		if (isp_tdebug > 1) {
			printf("%s:CTIO2 RX_ID 0x%x sts 0x%x flg 0x%x sns "
			    "%d FIN\n", isp->isp_name, ct->ct_rxid,
			    ct->ct_status, ct->ct_flags,
			    (ccb->ccb_h.status & CAM_SENT_SENSE) != 0);
		}
	} else {
		ct_entry_t *ct = arg;
		sentstatus = ct->ct_flags & CT_SENDSTATUS;
		ok = (ct->ct_status  & ~QLTM_SVALID) == CT_OK;
		if (isp_tdebug > 1) {
			printf("%s:CTIO tag 0x%x sts 0x%x flg 0x%x FIN\n",
			    isp->isp_name, ct->ct_tag_val, ct->ct_status,
			    ct->ct_flags);
		}
	}

	/*
	 * We're here either because data transfers are done (and
	 * it's time to send a final status CTIO) or because the final
	 * status CTIO is done. We don't get called for all intermediate
	 * CTIOs that happen for a large data transfer.
	 *
	 * In any case, for this platform, the upper layers figure out
	 * what to do next, so all we do here is collect status and
	 * pass information along.
	 */

	if (sentstatus) {
		/*
		 * Data transfer done. See if all went okay.
		 */
		if (ok) {
			ccb->csio.resid = 0;
		} else {
			ccb->csio.resid = ccb->csio.dxfer_len;
		}
	}


	if ((ccb->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_INPROG) {
		ccb->ccb_h.status |= CAM_REQ_CMP;
	}
	ccb->ccb_h.status &= ~CAM_SIM_QUEUED;
	if (isp->isp_osinfo.simqfrozen & SIMQFRZ_RESOURCE) {
		isp->isp_osinfo.simqfrozen &= ~SIMQFRZ_RESOURCE;
		if (isp->isp_osinfo.simqfrozen == 0) {
			if ((ccb->ccb_h.status & CAM_DEV_QFRZN) == 0) {
				IDPRINTF(3, ("%s: isp_done -> relsimq\n",
				    isp->isp_name));
				ccb->ccb_h.status |= CAM_RELEASE_SIMQ;
			} else {
				IDPRINTF(3, ("%s: isp_done -> devq frozen\n",
				    isp->isp_name));
			}
		} else {
			IDPRINTF(3, ("%s: isp_done -> simqfrozen = %x\n",
			    isp->isp_name, isp->isp_osinfo.simqfrozen));
		}
	}
	xpt_done(ccb);
	return (0);
}
#endif

static void
isp_cam_async(void *cbarg, u_int32_t code, struct cam_path *path, void *arg)
{
	struct cam_sim *sim;
	struct ispsoftc *isp;

	sim = (struct cam_sim *)cbarg;
	isp = (struct ispsoftc *) cam_sim_softc(sim);
	switch (code) {
	case AC_LOST_DEVICE:
		if (IS_SCSI(isp)) {
			u_int16_t oflags, nflags;
			sdparam *sdp = isp->isp_param;
			int s, rvf, tgt;

			tgt = xpt_path_target_id(path);
			rvf = ISP_FW_REVX(isp->isp_fwrev);
			s = splcam();
			sdp += cam_sim_bus(sim);
			isp->isp_update |= (1 << cam_sim_bus(sim));
			nflags = DPARM_SAFE_DFLT;
			if (rvf >= ISP_FW_REV(7, 55, 0) ||
			   (ISP_FW_REV(4, 55, 0) <= rvf &&
			   (rvf < ISP_FW_REV(5, 0, 0)))) {
				nflags |= DPARM_NARROW | DPARM_ASYNC;
			}
			oflags = sdp->isp_devparam[tgt].dev_flags;
			sdp->isp_devparam[tgt].dev_flags = nflags;
			sdp->isp_devparam[tgt].dev_update = 1;
			(void) isp_control(isp, ISPCTL_UPDATE_PARAMS, NULL);
			sdp->isp_devparam[tgt].dev_flags = oflags;
			(void) splx(s);
		}
		break;
	default:
		printf("%s: isp_attach Async Code 0x%x\n", isp->isp_name, code);
		break;
	}
}

static void
isp_poll(struct cam_sim *sim)
{
	isp_intr((struct ispsoftc *) cam_sim_softc(sim));
}

static void
isp_relsim(void *arg)
{
	struct ispsoftc *isp = arg;
	int s = splcam();
	if (isp->isp_osinfo.simqfrozen & SIMQFRZ_TIMED) {
		int wasfrozen = isp->isp_osinfo.simqfrozen & SIMQFRZ_TIMED;
		isp->isp_osinfo.simqfrozen &= ~SIMQFRZ_TIMED;
		if (wasfrozen && isp->isp_osinfo.simqfrozen == 0) {
			xpt_release_simq(isp->isp_sim, 1);
			IDPRINTF(3, ("%s: timed relsimq\n", isp->isp_name));
		}
	}
	splx(s);
}

static void
isp_action(struct cam_sim *sim, union ccb *ccb)
{
	int s, bus, tgt, error;
	struct ispsoftc *isp;
	struct ccb_trans_settings *cts;

	CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_TRACE, ("isp_action\n"));
	
	isp = (struct ispsoftc *)cam_sim_softc(sim);
	ccb->ccb_h.sim_priv.entries[0].field = 0;
	ccb->ccb_h.sim_priv.entries[1].ptr = isp;
	if (isp->isp_state != ISP_RUNSTATE &&
	    ccb->ccb_h.func_code == XPT_SCSI_IO) {
		s = splcam();
		DISABLE_INTS(isp);
		isp_init(isp);
		if (isp->isp_state != ISP_INITSTATE) {
			(void) splx(s);
			/*
			 * Lie. Say it was a selection timeout.
			 */
			ccb->ccb_h.status = CAM_SEL_TIMEOUT;
			ccb->ccb_h.status |= CAM_DEV_QFRZN;
			xpt_freeze_devq(ccb->ccb_h.path, 1);
			xpt_done(ccb);
			return;
		}
		isp->isp_state = ISP_RUNSTATE;
		ENABLE_INTS(isp);
		(void) splx(s);
	}
	IDPRINTF(4, ("%s: isp_action code %x\n", isp->isp_name,
	    ccb->ccb_h.func_code));

	switch (ccb->ccb_h.func_code) {
	case XPT_SCSI_IO:	/* Execute the requested I/O operation */
		/*
		 * Do a couple of preliminary checks...
		 */
		if ((ccb->ccb_h.flags & CAM_CDB_POINTER) != 0) {
			if ((ccb->ccb_h.flags & CAM_CDB_PHYS) != 0) {
				ccb->ccb_h.status = CAM_REQ_INVALID;
				xpt_done(ccb);
				break;
			}
		}
#ifdef	DIAGNOSTIC
		if (ccb->ccb_h.target_id > (ISP_MAX_TARGETS(isp) - 1)) {
			ccb->ccb_h.status = CAM_PATH_INVALID;
		} else if (ccb->ccb_h.target_lun > (ISP_MAX_LUNS(isp) - 1)) {
			ccb->ccb_h.status = CAM_PATH_INVALID;
		}
		if (ccb->ccb_h.status == CAM_PATH_INVALID) {
			printf("%s: invalid tgt/lun (%d.%d) in XPT_SCSI_IO\n",
			    isp->isp_name, ccb->ccb_h.target_id,
			    ccb->ccb_h.target_lun);
			xpt_done(ccb);
			break;
		}
#endif
		((struct ccb_scsiio *) ccb)->scsi_status = SCSI_STATUS_OK;
		s = splcam();
		DISABLE_INTS(isp);
		error = ispscsicmd((ISP_SCSI_XFER_T *) ccb);
		ENABLE_INTS(isp);
		splx(s);
		switch (error) {
		case CMD_QUEUED:
			ccb->ccb_h.status |= CAM_SIM_QUEUED;
			break;
		case CMD_RQLATER:
			if (isp->isp_osinfo.simqfrozen == 0) {
				IDPRINTF(3, ("%s: RQLATER freeze simq\n",
				    isp->isp_name));
				isp->isp_osinfo.simqfrozen |= SIMQFRZ_TIMED;
				timeout(isp_relsim, isp, 500);
				xpt_freeze_simq(sim, 1);
			}
			ccb->ccb_h.status &= ~CAM_STATUS_MASK;
                        ccb->ccb_h.status |= CAM_REQUEUE_REQ;
			xpt_done(ccb);
			break;
		case CMD_EAGAIN:
			if (isp->isp_osinfo.simqfrozen == 0) {
				xpt_freeze_simq(sim, 1);
				IDPRINTF(3, ("%s: EAGAIN freeze simq\n",
				    isp->isp_name));
			}
			isp->isp_osinfo.simqfrozen |= SIMQFRZ_RESOURCE;
			ccb->ccb_h.status &= ~CAM_STATUS_MASK;
                        ccb->ccb_h.status |= CAM_REQUEUE_REQ;
			xpt_done(ccb);
			break;
		case CMD_COMPLETE:
			isp_done((struct ccb_scsiio *) ccb);
			break;
		default:
			printf("%s: What's this? 0x%x at %d in file %s\n",
			    isp->isp_name, error, __LINE__, __FILE__);
			ccb->ccb_h.status &= ~CAM_STATUS_MASK;
			ccb->ccb_h.status |= CAM_REQ_CMP_ERR;
			xpt_done(ccb);
		}
		break;

#ifdef	ISP_TARGET_MODE
	case XPT_EN_LUN:		/* Enable LUN as a target */
		isp_en_lun(isp, ccb);
		xpt_done(ccb);
		break;

	case XPT_NOTIFY_ACK:		/* recycle notify ack */
xpt_print_path(ccb->ccb_h.path);
printf("notify ack\n");
	case XPT_IMMED_NOTIFY:		/* Add Immediate Notify Resource */
	case XPT_ACCEPT_TARGET_IO:	/* Add Accept Target IO Resource */
	{
		tstate_t *tptr = get_lun_statep(isp, ccb->ccb_h.target_lun);
		if (tptr == NULL) {
			ccb->ccb_h.status = CAM_LUN_INVALID;
			xpt_done(ccb);
			break;
		}
		s = splsoftcam();
		if (ccb->ccb_h.func_code == XPT_ACCEPT_TARGET_IO) {
			SLIST_INSERT_HEAD(&tptr->atios,
			    &ccb->ccb_h, sim_links.sle);
		} else {
			SLIST_INSERT_HEAD(&tptr->inots, &ccb->ccb_h,
			    sim_links.sle);
		}
		splx(s);
		rls_lun_statep(isp, tptr);
		ccb->ccb_h.status = CAM_REQ_INPROG;
		break;
	}
	case XPT_CONT_TARGET_IO:
	{
		s = splcam();
		ccb->ccb_h.status = isp_target_start_ctio(isp, ccb);
		if (ccb->ccb_h.status != CAM_REQ_INPROG) {
			if (isp->isp_osinfo.simqfrozen == 0) {
				xpt_freeze_simq(sim, 1);
				xpt_print_path(ccb->ccb_h.path);
				printf("XPT_CONT_TARGET_IO freeze simq\n");
			}
			isp->isp_osinfo.simqfrozen |= SIMQFRZ_RESOURCE;
			ccb->ccb_h.status &= ~CAM_STATUS_MASK;
                        ccb->ccb_h.status |= CAM_REQUEUE_REQ;
			xpt_done(ccb);
		} else {
			ccb->ccb_h.status |= CAM_SIM_QUEUED;
		}
		splx(s);
		break;
	}
#endif
	case XPT_RESET_DEV:		/* BDR the specified SCSI device */

		bus = cam_sim_bus(xpt_path_sim(ccb->ccb_h.path));
		tgt = ccb->ccb_h.target_id;
		tgt |= (bus << 16);

		s = splcam();
		error = isp_control(isp, ISPCTL_RESET_DEV, &tgt);
		(void) splx(s);
		if (error) {
			ccb->ccb_h.status = CAM_REQ_CMP_ERR;
		} else {
			ccb->ccb_h.status = CAM_REQ_CMP;
		}
		xpt_done(ccb);
		break;
	case XPT_ABORT:			/* Abort the specified CCB */
	{
		union ccb *accb = ccb->cab.abort_ccb;
		switch (accb->ccb_h.func_code) {
#ifdef	ISP_TARGET_MODE
		case XPT_ACCEPT_TARGET_IO:
		case XPT_IMMED_NOTIFY:
        		ccb->ccb_h.status = isp_abort_tgt_ccb(isp, ccb);
			break;
		case XPT_CONT_TARGET_IO:
			PRINTF("%s: cannot abort CTIOs yet\n", isp->isp_name);
			ccb->ccb_h.status = CAM_UA_ABORT;
			break;
#endif
		case XPT_SCSI_IO:
			s = splcam();
			error = isp_control(isp, ISPCTL_ABORT_CMD, ccb);
			(void) splx(s);
			if (error) {
				ccb->ccb_h.status = CAM_UA_ABORT;
			} else {
				ccb->ccb_h.status = CAM_REQ_CMP;
			}
			break;
		default:
			ccb->ccb_h.status = CAM_REQ_INVALID;
			break;
		}
		xpt_done(ccb);
		break;
	}
	case XPT_SET_TRAN_SETTINGS:	/* Nexus Settings */

		cts = &ccb->cts;
		tgt = cts->ccb_h.target_id;
		s = splcam();
		if (IS_SCSI(isp)) {
			sdparam *sdp = isp->isp_param;
			u_int16_t *dptr;

			bus = cam_sim_bus(xpt_path_sim(cts->ccb_h.path));

			sdp += bus;
#if	0
			if (cts->flags & CCB_TRANS_CURRENT_SETTINGS)
				dptr = &sdp->isp_devparam[tgt].cur_dflags;
			else
				dptr = &sdp->isp_devparam[tgt].dev_flags;
#else
			/*
			 * We always update (internally) from dev_flags
			 * so any request to change settings just gets
			 * vectored to that location.
			 */
			dptr = &sdp->isp_devparam[tgt].dev_flags;
#endif

			/*
			 * Note that these operations affect the
			 * the goal flags (dev_flags)- not
			 * the current state flags. Then we mark
			 * things so that the next operation to
			 * this HBA will cause the update to occur.
			 */
			if (cts->valid & CCB_TRANS_DISC_VALID) {
				if ((cts->flags & CCB_TRANS_DISC_ENB) != 0) {
					*dptr |= DPARM_DISC;
				} else {
					*dptr &= ~DPARM_DISC;
				}
			}
			if (cts->valid & CCB_TRANS_TQ_VALID) {
				if ((cts->flags & CCB_TRANS_TAG_ENB) != 0) {
					*dptr |= DPARM_TQING;
				} else {
					*dptr &= ~DPARM_TQING;
				}
			}
			if (cts->valid & CCB_TRANS_BUS_WIDTH_VALID) {
				switch (cts->bus_width) {
				case MSG_EXT_WDTR_BUS_16_BIT:
					*dptr |= DPARM_WIDE;
					break;
				default:
					*dptr &= ~DPARM_WIDE;
				}
			}
			/*
			 * Any SYNC RATE of nonzero and SYNC_OFFSET
			 * of nonzero will cause us to go to the
			 * selected (from NVRAM) maximum value for
			 * this device. At a later point, we'll
			 * allow finer control.
			 */
			if ((cts->valid & CCB_TRANS_SYNC_RATE_VALID) &&
			    (cts->valid & CCB_TRANS_SYNC_OFFSET_VALID) &&
			    (cts->sync_offset > 0)) {
				*dptr |= DPARM_SYNC;
			} else {
				*dptr &= ~DPARM_SYNC;
			}
			*dptr |= DPARM_SAFE_DFLT;
			if (bootverbose || isp->isp_dblev >= 3)
				printf("%s: %d.%d set %s period 0x%x offset "
				    "0x%x flags 0x%x\n", isp->isp_name, bus,
				    tgt,
				    (cts->flags & CCB_TRANS_CURRENT_SETTINGS)?
				    "current" : "user", 
				    sdp->isp_devparam[tgt].sync_period,
				    sdp->isp_devparam[tgt].sync_offset,
				    sdp->isp_devparam[tgt].dev_flags);
			sdp->isp_devparam[tgt].dev_update = 1;
			isp->isp_update |= (1 << bus);
		}
		(void) splx(s);
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;

	case XPT_GET_TRAN_SETTINGS:

		cts = &ccb->cts;
		tgt = cts->ccb_h.target_id;
		if (IS_FC(isp)) {
			/*
			 * a lot of normal SCSI things don't make sense.
			 */
			cts->flags = CCB_TRANS_TAG_ENB | CCB_TRANS_DISC_ENB;
			cts->valid = CCB_TRANS_DISC_VALID | CCB_TRANS_TQ_VALID;
			/*
			 * How do you measure the width of a high
			 * speed serial bus? Well, in bytes.
			 *
			 * Offset and period make no sense, though, so we set
			 * (above) a 'base' transfer speed to be gigabit.
			 */
			cts->bus_width = MSG_EXT_WDTR_BUS_8_BIT;
		} else {
			sdparam *sdp = isp->isp_param;
			u_int16_t dval, pval, oval;
			int bus = cam_sim_bus(xpt_path_sim(cts->ccb_h.path));

			sdp += bus;
			if (cts->flags & CCB_TRANS_CURRENT_SETTINGS) {
				s = splcam();
				sdp->isp_devparam[tgt].dev_refresh = 1;
				isp->isp_update |= (1 << bus);
				(void) isp_control(isp, ISPCTL_UPDATE_PARAMS,
				    NULL);
				(void) splx(s);
				dval = sdp->isp_devparam[tgt].cur_dflags;
				oval = sdp->isp_devparam[tgt].cur_offset;
				pval = sdp->isp_devparam[tgt].cur_period;
			} else {
				dval = sdp->isp_devparam[tgt].dev_flags;
				oval = sdp->isp_devparam[tgt].sync_offset;
				pval = sdp->isp_devparam[tgt].sync_period;
			}

			s = splcam();
			cts->flags &= ~(CCB_TRANS_DISC_ENB|CCB_TRANS_TAG_ENB);

			if (dval & DPARM_DISC) {
				cts->flags |= CCB_TRANS_DISC_ENB;
			}
			if (dval & DPARM_TQING) {
				cts->flags |= CCB_TRANS_TAG_ENB;
			}
			if (dval & DPARM_WIDE) {
				cts->bus_width = MSG_EXT_WDTR_BUS_16_BIT;
			} else {
				cts->bus_width = MSG_EXT_WDTR_BUS_8_BIT;
			}
			cts->valid = CCB_TRANS_BUS_WIDTH_VALID |
			    CCB_TRANS_DISC_VALID | CCB_TRANS_TQ_VALID;

			if ((dval & DPARM_SYNC) && oval != 0) {
				cts->sync_period = pval;
				cts->sync_offset = oval;
				cts->valid |=
				    CCB_TRANS_SYNC_RATE_VALID |
				    CCB_TRANS_SYNC_OFFSET_VALID;
			}
			splx(s);
			if (bootverbose || isp->isp_dblev >= 3)
				printf("%s: %d.%d get %s period 0x%x offset "
				    "0x%x flags 0x%x\n", isp->isp_name, bus,
				    tgt,
			    	    (cts->flags & CCB_TRANS_CURRENT_SETTINGS)?
				    "current" : "user", pval, oval, dval);
		}
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;

	case XPT_CALC_GEOMETRY:
	{
		struct ccb_calc_geometry *ccg;
		u_int32_t secs_per_cylinder;
		u_int32_t size_mb;

		ccg = &ccb->ccg;
		if (ccg->block_size == 0) {
			printf("%s: %d.%d XPT_CALC_GEOMETRY block size 0?\n",
				isp->isp_name, ccg->ccb_h.target_id,
				ccg->ccb_h.target_lun);
			ccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(ccb);
			break;
		}
		size_mb = ccg->volume_size /((1024L * 1024L) / ccg->block_size);
		if (size_mb > 1024) {
			ccg->heads = 255;
			ccg->secs_per_track = 63;
		} else {
			ccg->heads = 64;
			ccg->secs_per_track = 32;
		}
		secs_per_cylinder = ccg->heads * ccg->secs_per_track;
		ccg->cylinders = ccg->volume_size / secs_per_cylinder;
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	case XPT_RESET_BUS:		/* Reset the specified bus */
		bus = cam_sim_bus(sim);
		s = splcam();
		error = isp_control(isp, ISPCTL_RESET_BUS, &bus);
		(void) splx(s);
		if (error)
			ccb->ccb_h.status = CAM_REQ_CMP_ERR;
		else {
			if (cam_sim_bus(sim) && isp->isp_path2 != NULL)
				xpt_async(AC_BUS_RESET, isp->isp_path2, NULL);
			else if (isp->isp_path != NULL)
				xpt_async(AC_BUS_RESET, isp->isp_path, NULL);
			ccb->ccb_h.status = CAM_REQ_CMP;
		}
		xpt_done(ccb);
		break;

	case XPT_TERM_IO:		/* Terminate the I/O process */
		/* Does this need to be implemented? */
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;

	case XPT_PATH_INQ:		/* Path routing inquiry */
	{
		struct ccb_pathinq *cpi = &ccb->cpi;

		cpi->version_num = 1;
#ifdef	ISP_TARGET_MODE
		cpi->target_sprt = PIT_PROCESSOR | PIT_DISCONNECT | PIT_TERM_IO;
#else
		cpi->target_sprt = 0;
#endif
		cpi->hba_eng_cnt = 0;
		cpi->max_target = ISP_MAX_TARGETS(isp) - 1;
		cpi->max_lun = ISP_MAX_LUNS(isp) - 1;
		cpi->bus_id = cam_sim_bus(sim);
		if (IS_FC(isp)) {
			cpi->hba_misc = PIM_NOBUSRESET;
			/*
			 * Because our loop ID can shift from time to time,
			 * make our initiator ID out of range of our bus.
			 */
			cpi->initiator_id = cpi->max_target + 1;

			/*
			 * Set base transfer capabilities for Fibre Channel.
			 * Technically not correct because we don't know
			 * what media we're running on top of- but we'll
			 * look good if we always say 100MB/s.
			 */
			cpi->base_transfer_speed = 100000;
			cpi->hba_inquiry = PI_TAG_ABLE;
		} else {
			sdparam *sdp = isp->isp_param;
			sdp += cam_sim_bus(xpt_path_sim(cpi->ccb_h.path));
			cpi->hba_inquiry = PI_SDTR_ABLE|PI_TAG_ABLE|PI_WIDE_16;
			if (IS_ULTRA2(isp))
				cpi->hba_misc = PIM_NOBUSRESET;
			else
				cpi->hba_misc = 0;
			cpi->initiator_id = sdp->isp_initiator_id;
			cpi->base_transfer_speed = 3300;
		}
		strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
		strncpy(cpi->hba_vid, "Qlogic", HBA_IDLEN);
		strncpy(cpi->dev_name, cam_sim_name(sim), DEV_IDLEN);
		cpi->unit_number = cam_sim_unit(sim);
		cpi->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	default:
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;
	}
}

#define	ISPDDB	(CAM_DEBUG_INFO|CAM_DEBUG_TRACE|CAM_DEBUG_CDB)
void
isp_done(struct ccb_scsiio *sccb)
{
	struct ispsoftc *isp = XS_ISP(sccb);

	if (XS_NOERR(sccb))
		XS_SETERR(sccb, CAM_REQ_CMP);
	sccb->ccb_h.status &= ~CAM_STATUS_MASK;
	sccb->ccb_h.status |= sccb->ccb_h.spriv_field0;
	if ((sccb->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_CMP &&
	    (sccb->scsi_status != SCSI_STATUS_OK)) {
		sccb->ccb_h.status &= ~CAM_STATUS_MASK;
		if ((sccb->scsi_status == SCSI_STATUS_CHECK_COND) && 
		    (sccb->ccb_h.status & CAM_AUTOSNS_VALID) == 0) {
			sccb->ccb_h.status |= CAM_AUTOSENSE_FAIL;
		} else {
			sccb->ccb_h.status |= CAM_SCSI_STATUS_ERROR;
		}
	}
	sccb->ccb_h.status &= ~CAM_SIM_QUEUED;
	if ((sccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		if ((sccb->ccb_h.status & CAM_DEV_QFRZN) == 0) {
			sccb->ccb_h.status |= CAM_DEV_QFRZN;
			xpt_freeze_devq(sccb->ccb_h.path, 1);
			if (sccb->scsi_status != SCSI_STATUS_OK)
				IDPRINTF(3, ("%s: fdevq %d.%d %x %x\n",
				    isp->isp_name, sccb->ccb_h.target_id,
				    sccb->ccb_h.target_lun, sccb->ccb_h.status,
				    sccb->scsi_status));
		}
	}
	/*
	 * If we were frozen waiting resources, clear that we were frozen
	 * waiting for resources. If we are no longer frozen, and the devq
	 * isn't frozen, mark the completing CCB to have the XPT layer
	 * release the simq.
	 */
	if (isp->isp_osinfo.simqfrozen & SIMQFRZ_RESOURCE) {
		isp->isp_osinfo.simqfrozen &= ~SIMQFRZ_RESOURCE;
		if (isp->isp_osinfo.simqfrozen == 0) {
			if ((sccb->ccb_h.status & CAM_DEV_QFRZN) == 0) {
				IDPRINTF(3, ("%s: isp_done -> relsimq\n",
				    isp->isp_name));
				sccb->ccb_h.status |= CAM_RELEASE_SIMQ;
			} else {
				IDPRINTF(3, ("%s: isp_done -> devq frozen\n",
				    isp->isp_name));
			}
		} else {
			IDPRINTF(3, ("%s: isp_done -> simqfrozen = %x\n",
			    isp->isp_name, isp->isp_osinfo.simqfrozen));
		}
	}
	if (CAM_DEBUGGED(sccb->ccb_h.path, ISPDDB) &&
	    (sccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		xpt_print_path(sccb->ccb_h.path);
		printf("cam completion status 0x%x\n", sccb->ccb_h.status);
	}
	xpt_done((union ccb *) sccb);
}

int
isp_async(struct ispsoftc *isp, ispasync_t cmd, void *arg)
{
	int bus, rv = 0;
	switch (cmd) {
	case ISPASYNC_NEW_TGT_PARAMS:
	{
		int flags, tgt;
		sdparam *sdp = isp->isp_param;
		struct ccb_trans_settings neg;
		struct cam_path *tmppath;

		tgt = *((int *)arg);
		bus = (tgt >> 16) & 0xffff;
		tgt &= 0xffff;
		sdp += bus;
		if (xpt_create_path(&tmppath, NULL,
		    cam_sim_path(bus? isp->isp_sim2 : isp->isp_sim),
		    tgt, CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
			xpt_print_path(isp->isp_path);
			printf("isp_async cannot make temp path for "
			    "target %d bus %d\n", tgt, bus);
			rv = -1;
			break;
		}
		flags = sdp->isp_devparam[tgt].cur_dflags;
		neg.valid = CCB_TRANS_DISC_VALID | CCB_TRANS_TQ_VALID;
		if (flags & DPARM_DISC) {
			neg.flags |= CCB_TRANS_DISC_ENB;
		}
		if (flags & DPARM_TQING) {
			neg.flags |= CCB_TRANS_TAG_ENB;
		}
		neg.valid |= CCB_TRANS_BUS_WIDTH_VALID;
		neg.bus_width = (flags & DPARM_WIDE)?
		    MSG_EXT_WDTR_BUS_8_BIT : MSG_EXT_WDTR_BUS_16_BIT;
		neg.sync_period = sdp->isp_devparam[tgt].cur_period;
		neg.sync_offset = sdp->isp_devparam[tgt].cur_offset;
		if (flags & DPARM_SYNC) {
			neg.valid |=
			    CCB_TRANS_SYNC_RATE_VALID |
			    CCB_TRANS_SYNC_OFFSET_VALID;
		}
		IDPRINTF(3, ("%s: NEW_TGT_PARAMS bus %d tgt %d period "
		    "0x%x offset 0x%x flags 0x%x\n", isp->isp_name,
		    bus, tgt, neg.sync_period, neg.sync_offset, flags));
		xpt_setup_ccb(&neg.ccb_h, tmppath, 1);
		xpt_async(AC_TRANSFER_NEG, tmppath, &neg);
		xpt_free_path(tmppath);
		break;
	}
	case ISPASYNC_BUS_RESET:
		bus = *((int *)arg);
		printf("%s: SCSI bus reset on bus %d detected\n",
		    isp->isp_name, bus);
		if (bus > 0 && isp->isp_path2) {
			xpt_async(AC_BUS_RESET, isp->isp_path2, NULL);
		} else if (isp->isp_path) {
			xpt_async(AC_BUS_RESET, isp->isp_path, NULL);
		}
		break;
	case ISPASYNC_LOOP_DOWN:
		if (isp->isp_path) {
			if (isp->isp_osinfo.simqfrozen == 0) {
				IDPRINTF(3, ("%s: loop down freeze simq\n",
				    isp->isp_name));
				xpt_freeze_simq(isp->isp_sim, 1);
			}
			isp->isp_osinfo.simqfrozen |= SIMQFRZ_LOOPDOWN;
		}
		printf("%s: Loop DOWN\n", isp->isp_name);
		break;
	case ISPASYNC_LOOP_UP:
		if (isp->isp_path) {
			int wasfrozen =
			    isp->isp_osinfo.simqfrozen & SIMQFRZ_LOOPDOWN;
			isp->isp_osinfo.simqfrozen &= ~SIMQFRZ_LOOPDOWN;
			if (wasfrozen && isp->isp_osinfo.simqfrozen == 0) {
				xpt_release_simq(isp->isp_sim, 1);
				IDPRINTF(3, ("%s: loop up release simq\n",
					isp->isp_name));
			}
		}
		printf("%s: Loop UP\n", isp->isp_name);
		break;
	case ISPASYNC_PDB_CHANGED:
	{
		const char *fmt = "%s: Target %d (Loop 0x%x) Port ID 0x%x "
		    "role %s %s\n Port WWN 0x%08x%08x\n Node WWN 0x%08x%08x\n";
		const static char *roles[4] = {
		    "(none)", "Target", "Initiator", "Target/Initiator"
		};
		char *ptr;
		fcparam *fcp = isp->isp_param;
		int tgt = *((int *) arg);
		struct lportdb *lp = &fcp->portdb[tgt]; 

		if (lp->valid) {
			ptr = "arrived";
		} else {
			ptr = "disappeared";
		}
		printf(fmt, isp->isp_name, tgt, lp->loopid, lp->portid,
		    roles[lp->roles & 0x3], ptr,
		    (u_int32_t) (lp->port_wwn >> 32),
		    (u_int32_t) (lp->port_wwn & 0xffffffffLL),
		    (u_int32_t) (lp->node_wwn >> 32),
		    (u_int32_t) (lp->node_wwn & 0xffffffffLL));
		break;
	}
	case ISPASYNC_CHANGE_NOTIFY:
		printf("%s: Name Server Database Changed\n", isp->isp_name);
		break;
#ifdef	ISP2100_FABRIC
	case ISPASYNC_FABRIC_DEV:
	{
		int target;
		struct lportdb *lp;
		char *pt;
		sns_ganrsp_t *resp = (sns_ganrsp_t *) arg;
		u_int32_t portid;
		u_int64_t wwpn, wwnn;
		fcparam *fcp = isp->isp_param;

		rv = -1;

		portid =
		    (((u_int32_t) resp->snscb_port_id[0]) << 16) |
		    (((u_int32_t) resp->snscb_port_id[1]) << 8) |
		    (((u_int32_t) resp->snscb_port_id[2]));

		wwpn =
		    (((u_int64_t)resp->snscb_portname[0]) << 56) |
		    (((u_int64_t)resp->snscb_portname[1]) << 48) |
		    (((u_int64_t)resp->snscb_portname[2]) << 40) |
		    (((u_int64_t)resp->snscb_portname[3]) << 32) |
		    (((u_int64_t)resp->snscb_portname[4]) << 24) |
		    (((u_int64_t)resp->snscb_portname[5]) << 16) |
		    (((u_int64_t)resp->snscb_portname[6]) <<  8) |
		    (((u_int64_t)resp->snscb_portname[7]));

		wwnn =
		    (((u_int64_t)resp->snscb_nodename[0]) << 56) |
		    (((u_int64_t)resp->snscb_nodename[1]) << 48) |
		    (((u_int64_t)resp->snscb_nodename[2]) << 40) |
		    (((u_int64_t)resp->snscb_nodename[3]) << 32) |
		    (((u_int64_t)resp->snscb_nodename[4]) << 24) |
		    (((u_int64_t)resp->snscb_nodename[5]) << 16) |
		    (((u_int64_t)resp->snscb_nodename[6]) <<  8) |
		    (((u_int64_t)resp->snscb_nodename[7]));
		if (portid == 0 || wwpn == 0) {
			rv = 0;
			break;
		}

		switch (resp->snscb_port_type) {
		case 1:
			pt = "   N_Port";
			break;
		case 2:
			pt = "  NL_Port";
			break;
		case 3:
			pt = "F/NL_Port";
			break;
		case 0x7f:
			pt = "  Nx_Port";
			break;
		case 0x81:
			pt = "  F_port";
			break;
		case 0x82:
			pt = "  FL_Port";
			break;
		case 0x84:
			pt = "   E_port";
			break;
		default:
			pt = "?";
			break;
		}
		CFGPRINTF("%s: %s @ 0x%x, Node 0x%08x%08x Port %08x%08x\n",
		    isp->isp_name, pt, portid,
		    ((u_int32_t) (wwnn >> 32)), ((u_int32_t) wwnn),
		    ((u_int32_t) (wwpn >> 32)), ((u_int32_t) wwpn));
#if	0
		if ((resp->snscb_fc4_types[1] & 0x1) == 0) {
			rv = 0;
			printf("Types 0..3: 0x%x 0x%x 0x%x 0x%x\n",
			    resp->snscb_fc4_types[0], resp->snscb_fc4_types[1],
			    resp->snscb_fc4_types[3], resp->snscb_fc4_types[3]);
			break;
		}
#endif
		for (target = FC_SNS_ID+1; target < MAX_FC_TARG; target++) {
			lp = &fcp->portdb[target];
			if (lp->port_wwn == wwpn && lp->node_wwn == wwnn)
				break;
		}
		if (target < MAX_FC_TARG) {
			rv = 0;
			break;
		}
		for (target = FC_SNS_ID+1; target < MAX_FC_TARG; target++) {
			lp = &fcp->portdb[target];
			if (lp->port_wwn == 0)
				break;
		}
		if (target == MAX_FC_TARG) {
			printf("%s: no more space for fabric devices\n",
			    isp->isp_name);
			break;
		}
		lp->node_wwn = wwnn;
		lp->port_wwn = wwpn;
		lp->portid = portid;
		rv = 0;
		break;
	}
#endif
#ifdef	ISP_TARGET_MODE
	case ISPASYNC_TARGET_MESSAGE:
	{
		tmd_msg_t *mp = arg;
		ITDEBUG(2, ("%s: bus %d iid %d tgt %d lun %d ttype %x tval %x"
		    " msg[0]=0x%x\n", isp->isp_name, mp->nt_bus,
		    (int) mp->nt_iid, (int) mp->nt_tgt, (int) mp->nt_lun,
		    mp->nt_tagtype, mp->nt_tagval, mp->nt_msg[0]));
		break;
	}
	case ISPASYNC_TARGET_EVENT:
	{
		tmd_event_t *ep = arg;
		ITDEBUG(2, ("%s: bus %d event code 0x%x\n", isp->isp_name,
		    ep->ev_bus, ep->ev_event));
		break;
	}
	case ISPASYNC_TARGET_ACTION:
		switch (((isphdr_t *)arg)->rqs_entry_type) {
		default:
			printf("%s: event 0x%x for unhandled target action\n",
			    isp->isp_name, ((isphdr_t *)arg)->rqs_entry_type);
			break;
		case RQSTYPE_ATIO:
			rv = isp_handle_platform_atio(isp, (at_entry_t *) arg);
			break;
		case RQSTYPE_ATIO2:
			rv = isp_handle_platform_atio2(isp, (at2_entry_t *)arg);
			break;
		case RQSTYPE_CTIO2:
		case RQSTYPE_CTIO:
			rv = isp_handle_platform_ctio(isp, arg);
			break;
		case RQSTYPE_ENABLE_LUN:
		case RQSTYPE_MODIFY_LUN:
			isp_cv_signal_rqe(isp, ((lun_entry_t *)arg)->le_status);
			break;
		}
		break;
#endif
	default:
		PRINTF("%s: unknown isp_async event %d\n", isp->isp_name, cmd);
		rv = -1;
		break;
	}
	return (rv);
}


/*
 * Locks are held before coming here.
 */
void
isp_uninit(struct ispsoftc *isp)
{
	ISP_WRITE(isp, HCCR, HCCR_CMD_RESET);
	DISABLE_INTS(isp);
}
