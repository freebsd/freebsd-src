/*	$FreeBSD$	*/
/*	$NecBSD: scsi_low.c,v 1.24 1999/07/26 06:27:01 honda Exp $	*/
/*	$NetBSD$	*/

#define	SCSI_LOW_STATICS
#define SCSI_LOW_WARNINGS
#ifdef __NetBSD__
#define	SCSI_LOW_TARGET_OPEN
#define SCSI_LOW_INFORM
#endif
#ifdef __FreeBSD__
#define CAM
#endif

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1995, 1996, 1997, 1998, 1999
 *	NetBSD/pc98 porting staff. All rights reserved.
 *  Copyright (c) 1995, 1996, 1997, 1998, 1999
 *	Naofumi HONDA. All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* <On the nexus establishment>
 * When our host is reselected, 
 * nexus establish processes are little complicated.
 * Normal steps are followings:
 * 1) Our host selected by target => target nexus (slp->sl_nexus) 
 * 2) Identify msgin => lun nexus (ti->ti_li)
 * 3) Qtag msg => slccb nexus (ti->ti_nexus)
 */
#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#ifdef __NetBSD__
#include <sys/disklabel.h>
#endif
#if defined(__FreeBSD__) && __FreeBSD_version >= 500001
#include <sys/bio.h>
#endif
#include <sys/buf.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/device_port.h>
#include <sys/errno.h>

#ifdef __NetBSD__
#include <vm/vm.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/dvcfg.h>

#include <dev/cons.h>

#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsipiconf.h>
#include <dev/scsipi/scsipi_disk.h>
#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsiconf.h>

#include <i386/Cbus/dev/scsi_low.h>
#endif
#ifdef __FreeBSD__
#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_debug.h>
#include <cam/cam_periph.h>

#include <cam/scsi/scsi_all.h>

#include <cam/scsi/scsi_low.h>

#if !defined(__FreeBSD__) || __FreeBSD_version < 400001
#include <i386/i386/cons.h>
#else
#include <sys/cons.h>
#endif
#define delay(time) DELAY(time)

/* from sys/dev/usb/usb_port.h 
  XXX Change this when FreeBSD has memset
 */
#define memset(d, v, s)	\
		do{			\
		if ((v) == 0)		\
			bzero((d), (s));	\
		else			\
			panic("Non zero filler for memset, cannot handle!"); \
		} while (0)
#endif

#define	SCSI_LOW_DONE_COMPLETE	0
#define	SCSI_LOW_DONE_RETRY	1

static void scsi_low_engage __P((void *));
static void scsi_low_info __P((struct scsi_low_softc *, struct targ_info *, u_char *));
static void scsi_low_init_msgsys __P((struct scsi_low_softc *, struct targ_info *));
static struct slccb *scsi_low_establish_ccb __P((struct targ_info *, struct lun_info *, scsi_low_tag_t));
static int scsi_low_done __P((struct scsi_low_softc *, struct slccb *));
static void scsi_low_twiddle_wait __P((void));
static struct lun_info *scsi_low_alloc_li __P((struct targ_info *, int, int));
static struct targ_info *scsi_low_alloc_ti __P((struct scsi_low_softc *, int));
static void scsi_low_calcf __P((struct targ_info *, struct lun_info *));
static struct lun_info *scsi_low_establish_lun __P((struct targ_info *, int));
#ifndef CAM
static void scsi_low_scsi_minphys __P((struct buf *));
#endif
#ifdef	SCSI_LOW_TARGET_OPEN
static int scsi_low_target_open __P((struct scsipi_link *, struct cfdata *));
#endif	/* SCSI_LOW_TARGET_OPEN */
#ifdef CAM
void scsi_low_scsi_action(struct cam_sim *sim, union ccb *ccb);
static void scsi_low_poll (struct cam_sim *sim);
#else
static int scsi_low_scsi_cmd __P((struct scsipi_xfer *));
#endif
static void scsi_low_reset_nexus __P((struct scsi_low_softc *, int));
static int scsi_low_init __P((struct scsi_low_softc *, u_int));
static void scsi_low_start __P((struct scsi_low_softc *));
static void scsi_low_free_ti __P((struct scsi_low_softc *));
static void scsi_low_clear_ccb __P((struct slccb *));

#ifdef	SCSI_LOW_STATICS
struct scsi_low_statics {
	int nexus_win;
	int nexus_fail;
	int nexus_disconnected;
	int nexus_reselected;
	int nexus_conflict;
} scsi_low_statics;
#endif	/* SCSI_LOW_STATICS */
/**************************************************************
 * power control
 **************************************************************/
static void
scsi_low_engage(arg)
	void *arg;
{
	struct scsi_low_softc *slp = arg;
	int s = splbio();

	switch (slp->sl_rstep)
	{
	case 0:
		slp->sl_rstep ++;
		(*slp->sl_funcs->scsi_low_power) (slp, SCSI_LOW_ENGAGE);
#ifdef __FreeBSD__
		slp->engage_ch = 
#endif
		timeout(scsi_low_engage, slp, 1);
		break;

	case 1:
		slp->sl_rstep ++;
		slp->sl_flags &= ~HW_RESUME;
		scsi_low_start(slp);
		break;

	case 2:
		break;
	}
	splx(s);
}

static int
scsi_low_init(slp, flags)
	struct scsi_low_softc *slp;
	u_int flags;
{

	if ((slp->sl_flags & HW_POWERCTRL) != 0)
	{
#ifdef __FreeBSD__
		untimeout(scsi_low_engage, slp, slp->engage_ch);
#else /* NetBSD */
		untimeout(scsi_low_engage, slp);
#endif
		slp->sl_flags &= ~(HW_POWDOWN | HW_RESUME);
		slp->sl_active = 1;
		slp->sl_powc = SCSI_LOW_POWDOWN_TC;
	}

	/* reset current nexus */
	scsi_low_reset_nexus(slp, flags);
	if ((slp->sl_flags & HW_INACTIVE) != 0)
		return EBUSY;

	if (flags == SCSI_LOW_RESTART_SOFT)
		return 0;

	return ((*slp->sl_funcs->scsi_low_init) (slp, flags));
}

/**************************************************************
 * allocate lun_info
 **************************************************************/
static struct lun_info *
scsi_low_alloc_li(ti, lun, alloc)
	struct targ_info *ti;
	int lun;
	int alloc;
{
	struct scsi_low_softc *slp = ti->ti_sc;
	struct lun_info *li;

	li = LIST_FIRST(&ti->ti_litab); 
	if (li != NULL)
	{ 
		if (li->li_lun == lun)
			return li;

		while ((li = LIST_NEXT(li, lun_chain)) != NULL)
		{
			if (li->li_lun == lun)
			{
				LIST_REMOVE(li, lun_chain);
				LIST_INSERT_HEAD(&ti->ti_litab, li, lun_chain);
				return li;
			}
		}
	}

	if (alloc == 0)
		return li;

	li = malloc(ti->ti_lunsize, M_DEVBUF, M_NOWAIT);
	if (li == NULL)
		panic("no lun info mem\n");

	memset(li, 0, ti->ti_lunsize);
	li->li_lun = lun;
	li->li_ti = ti;
#if	defined(SDEV_NOPARITY) && defined(SDEV_NODISC)
	li->li_quirks = SDEV_NOPARITY | SDEV_NODISC;
#endif	/* SDEV_NOPARITY && SDEV_NODISC */
	li->li_cfgflags = 0xffff0000 | SCSI_LOW_SYNC;

	LIST_INSERT_HEAD(&ti->ti_litab, li, lun_chain);

	/* host specific structure initialization per lun */
	(void) ((*slp->sl_funcs->scsi_low_lun_init) (slp, ti, li));

	return li;
}

/**************************************************************
 * allocate targ_info
 **************************************************************/
static struct targ_info *
scsi_low_alloc_ti(slp, targ)
	struct scsi_low_softc *slp;
	int targ;
{
	struct targ_info *ti;

	if (slp->sl_titab.tqh_first == NULL)
		TAILQ_INIT(&slp->sl_titab);

	ti = malloc(sizeof(struct targ_info), M_DEVBUF, M_NOWAIT);
	if (ti == NULL)
		panic("%s short of memory\n", slp->sl_xname);

	memset(ti, 0, sizeof(struct targ_info));
	ti->ti_id = targ;
	ti->ti_sc = slp;

	slp->sl_ti[targ] = ti;
	TAILQ_INSERT_TAIL(&slp->sl_titab, ti, ti_chain);
	TAILQ_INIT(&ti->ti_discq);
	LIST_INIT(&ti->ti_litab);

	return ti;
}

static void
scsi_low_free_ti(slp)
	struct scsi_low_softc *slp;
{
	struct targ_info *ti, *tib;
	struct lun_info *li, *nli;

	for (ti = slp->sl_titab.tqh_first; ti; ti = tib)
	{
		tib = ti->ti_chain.tqe_next;
		for (li = LIST_FIRST(&ti->ti_litab); li != NULL; li = nli)
		{
			nli = LIST_NEXT(li, lun_chain);
			free(li, M_DEVBUF);
		}
		free(ti, M_DEVBUF);
	}
}

/**************************************************************
 * timeout
 **************************************************************/
void
scsi_low_timeout(arg)
	void *arg;
{
	struct scsi_low_softc *slp = arg;
	struct targ_info *ti;
	struct slccb *cb = NULL;		/* XXX */
	int s = splbio();

	/* check */
	if ((ti = slp->sl_nexus) != NULL && (cb = ti->ti_nexus) != NULL)
	{
		cb->ccb_tc -= SCSI_LOW_TIMEOUT_CHECK_INTERVAL;
		if (cb->ccb_tc < 0)
			goto bus_reset;
	}
	else if (slp->sl_disc > 0)
	{
		struct targ_info *ti;

		for (ti = slp->sl_titab.tqh_first; ti != NULL;
		     ti = ti->ti_chain.tqe_next)
		{
			for (cb = ti->ti_discq.tqh_first; cb != NULL;
			     cb = cb->ccb_chain.tqe_next)
			{
				cb->ccb_tc -= SCSI_LOW_TIMEOUT_CHECK_INTERVAL;
				if (cb->ccb_tc < 0)
					goto bus_reset;
			}
		}
	}
	else
	{
		cb = slp->sl_start.tqh_first;
		if (cb != NULL)
		{
			cb->ccb_tc -= SCSI_LOW_TIMEOUT_CHECK_INTERVAL;
			if (cb->ccb_tc < 0)
				goto bus_reset;
		}
		else if ((slp->sl_flags & HW_POWERCTRL) != 0)
		{
			if ((slp->sl_flags & (HW_POWDOWN | HW_RESUME)) != 0)
				goto out;

			if (slp->sl_active != 0)
			{
				slp->sl_powc = SCSI_LOW_POWDOWN_TC;
				slp->sl_active = 0;
				goto out;
			}

			slp->sl_powc --;
			if (slp->sl_powc < 0)
			{
				slp->sl_powc = SCSI_LOW_POWDOWN_TC;
				slp->sl_flags |= HW_POWDOWN;
				(*slp->sl_funcs->scsi_low_power)
						(slp, SCSI_LOW_POWDOWN);
			}
		}
	}

out:
#ifdef __FreeBSD__
	slp->timeout_ch = 
#endif
	timeout(scsi_low_timeout, slp, SCSI_LOW_TIMEOUT_CHECK_INTERVAL * hz);

	splx(s);
	return;

bus_reset:
	cb->ccb_error |= TIMEOUTIO;
	scsi_low_info(slp, NULL, "scsi bus hangup. try to recover.");
	scsi_low_init(slp, SCSI_LOW_RESTART_HARD);
	scsi_low_start(slp);
#ifdef __FreeBSD__
	slp->timeout_ch = 
#endif
	timeout(scsi_low_timeout, slp, SCSI_LOW_TIMEOUT_CHECK_INTERVAL * hz);

	splx(s);
}

/**************************************************************
 * CCB
 **************************************************************/
GENERIC_CCB_STATIC_ALLOC(scsi_low, slccb)
GENERIC_CCB(scsi_low, slccb, ccb_chain)

/**************************************************************
 * SCSI INTERFACE (XS)
 **************************************************************/
#define	SCSI_LOW_MINPHYS	0x10000

#ifdef __NetBSD__
struct scsipi_device scsi_low_dev = {
	NULL,	/* Use default error handler */
	NULL,	/* have a queue, served by this */
	NULL,	/* have no async handler */
	NULL,	/* Use default 'done' routine */
};
#endif

#ifdef CAM
static void
scsi_low_cam_rescan_callback(struct cam_periph *periph, union ccb *ccb)
{
	xpt_free_path(ccb->ccb_h.path);
	free(ccb, M_DEVBUF);
#if defined(__FreeBSD__) && __FreeBSD_version < 400001
	free(periph, M_DEVBUF);
#endif
}

static void
scsi_low_rescan_bus(struct scsi_low_softc *slp)
{
  	struct cam_path *path;
	union ccb *ccb = malloc(sizeof(union ccb), M_DEVBUF, M_WAITOK);
#if defined(__FreeBSD__) && __FreeBSD_version < 400001
	struct cam_periph *xpt_periph = malloc(sizeof(struct cam_periph),
					       M_DEVBUF, M_WAITOK);
#endif
	cam_status status;

	bzero(ccb, sizeof(union ccb));

	status = xpt_create_path(&path, xpt_periph,
				 cam_sim_path(slp->sim), -1, 0);
	if (status != CAM_REQ_CMP)
		return;

	xpt_setup_ccb(&ccb->ccb_h, path, 5);
	ccb->ccb_h.func_code = XPT_SCAN_BUS;
	ccb->ccb_h.cbfcnp = scsi_low_cam_rescan_callback;
	ccb->crcn.flags = CAM_FLAG_NONE;
	xpt_action(ccb);
}
#endif

int
scsi_low_attach(slp, openings, ntargs, nluns, lunsize)
	struct scsi_low_softc *slp;
	int openings, ntargs, nluns, lunsize;
{
	struct targ_info *ti;
	struct lun_info *li;
#ifdef CAM
	struct cam_devq *devq;
#else
	struct scsipi_adapter *sap;
#endif
	int i, nccb;

#ifdef CAM
	OS_DEPEND(sprintf(slp->sl_xname, "%s%d",
			  DEVPORT_DEVNAME(slp->sl_dev), DEVPORT_DEVUNIT(slp->sl_dev)));
#else
	OS_DEPEND(strncpy(slp->sl_xname, DEVPORT_DEVNAME(slp->sl_dev), 16));
#endif
	if (ntargs > SCSI_LOW_NTARGETS)
	{
		printf("scsi_low: %d targets are too large\n", ntargs);
		printf("change kernel options SCSI_LOW_NTARGETS");
	}

	if (lunsize < sizeof(struct lun_info))
		lunsize = sizeof(struct lun_info);

	for (i = 0; i < ntargs; i ++)
	{
		ti = scsi_low_alloc_ti(slp, i);
		ti->ti_lunsize = lunsize;
		li = scsi_low_alloc_li(ti, 0, 1);
	}

#ifndef CAM
	sap = malloc(sizeof(*sap), M_DEVBUF, M_NOWAIT);
	if (sap == NULL)
		return ENOMEM;

	memset(sap, 0, sizeof(*sap));
	sap->scsipi_cmd = scsi_low_scsi_cmd;
	sap->scsipi_minphys = scsi_low_scsi_minphys;
#ifdef	SCSI_LOW_TARGET_OPEN
	sap->open_target_lu = scsi_low_target_open;
#endif	/* SCSI_LOW_TARGET_OPEN */
#endif

	if (scsi_low_init(slp, SCSI_LOW_RESTART_HARD) != 0)
		return EINVAL;

	/* initialize queue */
	nccb = openings * (ntargs - 1);
	if (nccb >= SCSI_LOW_NCCB || nccb <= 0)
		nccb = SCSI_LOW_NCCB;
	scsi_low_init_ccbque(nccb);
	TAILQ_INIT(&slp->sl_start);

	slp->sl_openings = openings;
	slp->sl_ntargs = ntargs;
	slp->sl_nluns = nluns;

#ifdef CAM
	/*
	 * Prepare the scsibus_data area for the upperlevel
	 * scsi code.
	 */
	devq = cam_simq_alloc(256/*MAX_START*/);
	if (devq == NULL)
		return (0);
	/*	scbus->adapter_link = &slp->sc_link; */
	/*
	 * ask the adapter what subunits are present
	 */
	
	slp->sim = cam_sim_alloc(scsi_low_scsi_action, scsi_low_poll,
				 DEVPORT_DEVNAME(slp->sl_dev), slp,
				 DEVPORT_DEVUNIT(slp->sl_dev), 1, 32/*MAX_TAGS*/, devq);
       if (slp->sim == NULL) {
	 cam_simq_free(devq);
	 return 0;
       }

       if (xpt_bus_register(slp->sim, 0) != CAM_SUCCESS) {
	 free(slp->sim, M_DEVBUF);
	 return 0;
       }
       
       if (xpt_create_path(&slp->path, /*periph*/NULL,
                           cam_sim_path(slp->sim), CAM_TARGET_WILDCARD,
			   CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
	 xpt_bus_deregister(cam_sim_path(slp->sim));
	 cam_sim_free(slp->sim, /*free_simq*/TRUE);
	 free(slp->sim, M_DEVBUF);
	 return 0;
       }
#else /* !CAM */
	slp->sl_link.adapter_softc = slp;
	slp->sl_link.scsipi_scsi.adapter_target = slp->sl_hostid;
	slp->sl_link.scsipi_scsi.max_target = ntargs - 1;
	slp->sl_link.scsipi_scsi.max_lun = nluns - 1;
	slp->sl_link.scsipi_scsi.channel = SCSI_CHANNEL_ONLY_ONE;
	slp->sl_link.openings = openings;
	slp->sl_link.type = BUS_SCSI;
	slp->sl_link.adapter_softc = slp;
	slp->sl_link.adapter = sap;
	slp->sl_link.device = &scsi_low_dev;
#endif

	/* start watch dog */
	slp->sl_max_retry = SCSI_LOW_MAX_RETRY;
#ifdef __FreeBSD__
	slp->timeout_ch = 
#endif
	timeout(scsi_low_timeout, slp, SCSI_LOW_TIMEOUT_CHECK_INTERVAL * hz);
#ifdef CAM
	if (!cold)
		scsi_low_rescan_bus(slp);
#endif

	return 0;
}

#ifndef CAM
static void
scsi_low_scsi_minphys(bp)
	struct buf *bp;
{

	if (bp->b_bcount > SCSI_LOW_MINPHYS)
		bp->b_bcount = SCSI_LOW_MINPHYS;
	minphys(bp);
}
#endif

int
scsi_low_dettach(slp)
	struct scsi_low_softc *slp;
{

	if (slp->sl_disc > 0 || slp->sl_start.tqh_first != NULL)
		return EBUSY;

	/*
	 * scsipi does not have dettach bus fucntion.
	 *
	scsipi_dettach_scsibus(&slp->sl_link);
	*/

#ifdef CAM
	xpt_async(AC_LOST_DEVICE, slp->path, NULL);
	xpt_free_path(slp->path);
	xpt_bus_deregister(cam_sim_path(slp->sim));
	cam_sim_free(slp->sim, /* free_devq */ TRUE);
#endif

	scsi_low_free_ti(slp);
	return 0;
}

#ifdef CAM
static void
scsi_low_poll(struct cam_sim *sim)
{
	struct scsi_low_softc *slp = (struct scsi_low_softc *) cam_sim_softc(sim);
	(*slp->sl_funcs->scsi_low_poll) (slp);
}

void
scsi_low_scsi_action(struct cam_sim *sim, union ccb *ccb)
{
	struct scsi_low_softc *slp = (struct scsi_low_softc *) cam_sim_softc(sim);
	int s, target = (u_int) (ccb->ccb_h.target_id);
	struct targ_info *ti;
	struct lun_info *li;
	struct slccb *cb;

#if 0
	printf("scsi_low_scsi_action() func code %d Target: %d, LUN: %d\n",
	       ccb->ccb_h.func_code, target, ccb->ccb_h.target_lun);
#endif
	switch (ccb->ccb_h.func_code) {
	case XPT_SCSI_IO:	/* Execute the requested I/O operation */
		if (((cb = scsi_low_get_ccb()) == NULL)) {
			ccb->ccb_h.status = CAM_RESRC_UNAVAIL;
			xpt_done(ccb);
			return;
		}

		cb->ccb = ccb;
		cb->ccb_tag = SCSI_LOW_UNKTAG;
		cb->bp = (struct buf *)NULL;
		cb->ti = ti = slp->sl_ti[target];
		cb->li = scsi_low_alloc_li(ti, ccb->ccb_h.target_lun, 1);
		cb->ccb_flags = 0;
		cb->ccb_rcnt = 0;

		s = splcam();

		TAILQ_INSERT_TAIL(&slp->sl_start, cb, ccb_chain);

		if (slp->sl_nexus == NULL) {
			scsi_low_start(slp);
		}

		splx(s);
		break;
	case XPT_RESET_DEV:	/* Bus Device Reset the specified SCSI device */
	case XPT_EN_LUN:		/* Enable LUN as a target */
	case XPT_TARGET_IO:		/* Execute target I/O request */
	case XPT_ACCEPT_TARGET_IO:	/* Accept Host Target Mode CDB */
	case XPT_CONT_TARGET_IO:	/* Continue Host Target I/O Connection*/
	case XPT_ABORT:			/* Abort the specified CCB */
		/* XXX Implement */
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;
	case XPT_SET_TRAN_SETTINGS:
		/* XXX Implement */
		ccb->ccb_h.status = CAM_FUNC_NOTAVAIL;
		xpt_done(ccb);
		break;
	case XPT_GET_TRAN_SETTINGS: {
		struct ccb_trans_settings *cts;
		struct targ_info *ti;
		int lun = ccb->ccb_h.target_lun;
		/*int s;*/

		cts = &ccb->cts;
		ti = slp->sl_ti[ccb->ccb_h.target_id];
		li = LIST_FIRST(&ti->ti_litab); 
		if (li != NULL && li->li_lun != lun)
			while ((li = LIST_NEXT(li, lun_chain)) != NULL)
				if (li->li_lun == lun)
					break;
		s = splcam();
		if (li != NULL && (cts->flags & CCB_TRANS_USER_SETTINGS) != 0) {
			if (li->li_cfgflags & SCSI_LOW_DISC)
				cts->flags = CCB_TRANS_DISC_ENB;
			else
				cts->flags = 0;
			if (li->li_cfgflags & SCSI_LOW_QTAG)
				cts->flags |= CCB_TRANS_TAG_ENB;

			cts->bus_width = 0;/*HN2*/

			cts->valid = CCB_TRANS_SYNC_RATE_VALID
				   | CCB_TRANS_SYNC_OFFSET_VALID
				   | CCB_TRANS_BUS_WIDTH_VALID
				   | CCB_TRANS_DISC_VALID
				   | CCB_TRANS_TQ_VALID;
			ccb->ccb_h.status = CAM_REQ_CMP;
		} else
			ccb->ccb_h.status = CAM_FUNC_NOTAVAIL;

		splx(s);
		xpt_done(ccb);
		break;
	}
	case XPT_CALC_GEOMETRY: { /* not yet HN2 */
		struct	  ccb_calc_geometry *ccg;
		u_int32_t size_mb;
		u_int32_t secs_per_cylinder;
		int       extended;

		extended = 1;
		ccg = &ccb->ccg;
		size_mb = ccg->volume_size
			/ ((1024L * 1024L) / ccg->block_size);
		
		if (size_mb > 1024 && extended) {
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
	case XPT_RESET_BUS:		/* Reset the specified SCSI bus */
#if 0
	  	scsi_low_bus_reset(slp);
#endif
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	case XPT_TERM_IO:		/* Terminate the I/O process */
		/* XXX Implement */
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;
	case XPT_PATH_INQ: {		/* Path routing inquiry */
		struct ccb_pathinq *cpi = &ccb->cpi;
		
		cpi->version_num = 1; /* XXX??? */
		cpi->hba_inquiry = PI_SDTR_ABLE;
		cpi->target_sprt = 0;
		cpi->hba_misc = 0;
		cpi->hba_eng_cnt = 0;
		cpi->max_target = SCSI_LOW_NTARGETS - 1;
		cpi->max_lun = 7;
		cpi->initiator_id = 7; /* HOST_SCSI_ID */
		cpi->bus_id = cam_sim_bus(sim);
		cpi->base_transfer_speed = 3300;
		strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
		strncpy(cpi->hba_vid, "SCSI_LOW", HBA_IDLEN);
		strncpy(cpi->dev_name, cam_sim_name(sim), DEV_IDLEN);
		cpi->unit_number = cam_sim_unit(sim);
		cpi->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	default:
	        printf("scsi_low: non support func_code = %d ", ccb->ccb_h.func_code);
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;
	}
}
#else /* !CAM */
static int
scsi_low_scsi_cmd(xs)
	struct scsipi_xfer *xs;
{
	struct scsi_low_softc *slp = xs->sc_link->adapter_softc;
	struct targ_info *ti;
	struct slccb *cb;
	int s, lun, timeo;

	if (slp->sl_cfgflags & CFG_NOATTEN)
	{
		if (xs->sc_link->scsipi_scsi.lun > 0)
		{
			xs->error = XS_DRIVER_STUFFUP;
			return COMPLETE;
		}
	}

	if ((cb = scsi_low_get_ccb(xs->flags & SCSI_NOSLEEP)) == NULL)
		return TRY_AGAIN_LATER;

	lun = xs->sc_link->scsipi_scsi.lun;
	cb->xs = xs;
	cb->ccb_tag = SCSI_LOW_UNKTAG;
	cb->ti = ti = slp->sl_ti[xs->sc_link->scsipi_scsi.target];
	cb->li = scsi_low_alloc_li(ti, lun, 1);
	cb->ccb_flags = 0;
	cb->ccb_rcnt = 0;

	s = splbio();

	TAILQ_INSERT_TAIL(&slp->sl_start, cb, ccb_chain);
	if (slp->sl_nexus == NULL)
		scsi_low_start(slp);

	if ((xs->flags & SCSI_POLL) == 0)
	{
		splx(s);
		return SUCCESSFULLY_QUEUED;
	}

#define	SCSI_LOW_POLL_INTERVAL	1000		/* 1 ms */
	timeo = xs->timeout * (1000 / SCSI_LOW_POLL_INTERVAL);

	while ((xs->flags & ITSDONE) == 0 && timeo -- > 0)
	{
		delay(SCSI_LOW_POLL_INTERVAL);
		(*slp->sl_funcs->scsi_low_poll) (slp);
	}

	if ((xs->flags & ITSDONE) == 0)
	{
		cb->ccb_error |= (TIMEOUTIO | ABORTIO);
		SCSI_LOW_SETUP_MSGPHASE(slp, MSGPH_NULL);
		scsi_low_disconnected(slp, ti);
		scsi_low_init(slp, SCSI_LOW_RESTART_HARD);
	}

	scsipi_done(xs);
	splx(s);
	return COMPLETE;
}
#endif

/**************************************************************
 * Start & Done
 **************************************************************/
#ifdef __NetBSD__
static struct scsipi_start_stop ss_cmd = { START_STOP, 0, {0,0,}, SSS_START, };
static struct scsipi_test_unit_ready unit_ready_cmd;
#endif
#ifdef __FreeBSD__
static struct scsi_start_stop_unit ss_cmd = { START_STOP, 0, {0,0,}, SSS_START, };
static struct scsi_test_unit_ready unit_ready_cmd;
#endif
static void scsi_low_unit_ready_cmd __P((struct slccb *));

static void
scsi_low_unit_ready_cmd(cb)
	struct slccb *cb;
{

	cb->ccb_scp.scp_cmd = (u_int8_t *) &unit_ready_cmd;
	cb->ccb_scp.scp_cmdlen = sizeof(unit_ready_cmd);
	cb->ccb_scp.scp_datalen = 0;
	cb->ccb_scp.scp_direction = SCSI_LOW_READ;
	cb->ccb_tcmax = 15;
}

static void
scsi_low_start(slp)
	struct scsi_low_softc *slp;
{
#ifdef CAM
        union ccb *ccb;
#else
	struct scsipi_xfer *xs;
#endif
	struct targ_info *ti;
	struct lun_info *li;
	struct slccb *cb;
	int rv;

	/* check hardware exists ? */
	if ((slp->sl_flags & HW_INACTIVE) != 0)
		return;

	/* check hardware power up ? */
	if ((slp->sl_flags & HW_POWERCTRL) != 0)
	{
		slp->sl_active ++;
		if (slp->sl_flags & (HW_POWDOWN | HW_RESUME))
		{
			if (slp->sl_flags & HW_RESUME)
				return;
			slp->sl_flags &= ~HW_POWDOWN;
			if (slp->sl_funcs->scsi_low_power != NULL)
			{
				slp->sl_flags |= HW_RESUME;
				slp->sl_rstep = 0;
				(*slp->sl_funcs->scsi_low_power)
					(slp, SCSI_LOW_ENGAGE);
#ifdef __FreeBSD__
				slp->engage_ch =
#endif
				timeout(scsi_low_engage, slp, 1);
				return;
			}
		}
	}

	/* setup nexus */
#ifdef	SCSI_LOW_DIAGNOSTIC
	ti = slp->sl_nexus;
	if (ti != NULL)
	{
		scsi_low_info(slp, NULL, "NEXUS INCOSISTENT");
		panic("%s: inconsistent(target)\n", slp->sl_xname);
	}
#endif	/* SCSI_LOW_DIAGNOSTIC */

	for (cb = slp->sl_start.tqh_first; cb != NULL;
	     cb = cb->ccb_chain.tqe_next)
	{
		ti = cb->ti;
		li = cb->li;
		if (ti->ti_phase == PH_NULL)
			goto scsi_low_cmd_start;
		if (ti->ti_phase == PH_DISC && li->li_disc < li->li_maxnexus)
			goto scsi_low_cmd_start;
	}
	return;

scsi_low_cmd_start:
#ifdef CAM
	ccb = cb->ccb;
#else
	xs = cb->xs;
#endif
#ifdef	SCSI_LOW_DIAGNOSTIC
	if (ti->ti_nexus != NULL || ti->ti_li != NULL)
	{
		scsi_low_info(slp, NULL, "NEXUS INCOSISTENT");
		panic("%s: inconsistent(lun or ccb)\n", slp->sl_xname);
	}
#endif	/* SCSI_LOW_DIAGNOSTIC */

	/* clear all error flag bits (for restart) */
	cb->ccb_error = 0;

	/* setup nexus pointer */
	ti->ti_nexus = cb;
	ti->ti_li = li;
	slp->sl_nexus = ti;

	/* initialize msgsys */
	scsi_low_init_msgsys(slp, ti);

	/* target lun state check */
#ifdef CAM
		li->li_maxstate = UNIT_OK;
#else
	if ((xs->flags & SCSI_POLL) != 0)
		li->li_maxstate = UNIT_NEGSTART;
	else
		li->li_maxstate = UNIT_OK;
#endif

	/* exec cmds */
scsi_low_cmd_exec:
	if ((cb->ccb_flags & CCB_SENSE) != 0)
	{
		memset(&cb->ccb_sense, 0, sizeof(cb->ccb_sense));
#ifdef CAM
#else
		cb->ccb_sense_cmd.opcode = REQUEST_SENSE;
		cb->ccb_sense_cmd.byte2 = (li->li_lun << 5);
		cb->ccb_sense_cmd.length = sizeof(cb->ccb_sense);
#endif
		cb->ccb_scp.scp_cmd = (u_int8_t *) &cb->ccb_sense_cmd;
		cb->ccb_scp.scp_cmdlen = sizeof(cb->ccb_sense_cmd);
		cb->ccb_scp.scp_data = (u_int8_t *) &cb->ccb_sense;
		cb->ccb_scp.scp_datalen = sizeof(cb->ccb_sense);
		cb->ccb_scp.scp_direction = SCSI_LOW_READ;
		cb->ccb_tcmax = 15;
	}
	else if (li->li_state >= li->li_maxstate)
	{
#ifdef CAM
		cb->ccb_scp.scp_cmd = ccb->csio.cdb_io.cdb_bytes;
		cb->ccb_scp.scp_cmdlen = (int) ccb->csio.cdb_len;
		cb->ccb_scp.scp_data = ccb->csio.data_ptr;
		cb->ccb_scp.scp_datalen = (int) ccb->csio.dxfer_len;
		if((ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_OUT)
			cb->ccb_scp.scp_direction = SCSI_LOW_WRITE;
		else /* if((ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN) */
			cb->ccb_scp.scp_direction = SCSI_LOW_READ;
		cb->ccb_tcmax = (ccb->ccb_h.timeout >> 10);
#else
		cb->ccb_scp.scp_cmd = (u_int8_t *) xs->cmd;
		cb->ccb_scp.scp_cmdlen = xs->cmdlen;
		cb->ccb_scp.scp_data = xs->data;
		cb->ccb_scp.scp_datalen = xs->datalen;
		cb->ccb_scp.scp_direction = (xs->flags & SCSI_DATA_OUT) ? 
				SCSI_LOW_WRITE : SCSI_LOW_READ;
		cb->ccb_tcmax = (xs->timeout >> 10);
#endif

	}
	else switch(li->li_state)
	{
	case UNIT_SLEEP:
		scsi_low_unit_ready_cmd(cb);
		break;

	case UNIT_START:
		cb->ccb_scp.scp_cmd = (u_int8_t *) &ss_cmd;
		cb->ccb_scp.scp_cmdlen = sizeof(ss_cmd);
		cb->ccb_scp.scp_datalen = 0;
		cb->ccb_scp.scp_direction = SCSI_LOW_READ;
		cb->ccb_tcmax = 30;
		break;

	case UNIT_SYNCH:
		if (li->li_maxsynch.offset > 0)
		{
			scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_SYNCH, 0);
			scsi_low_unit_ready_cmd(cb);
			break;
		}
		li->li_state = UNIT_WIDE;

	case UNIT_WIDE:
#ifdef	SCSI_LOW_SUPPORT_WIDE
		if (li->li_width > 0)
		{
			scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_WIDE, 0);
			scsi_low_unit_ready_cmd(cb);
			break;
		}
#endif	/* SCSI_LOW_SUPPORT_WIDE */
		li->li_state = UNIT_OK;

	case UNIT_OK:
		goto scsi_low_cmd_exec;
	}

	/* timeout */
	if (cb->ccb_tcmax < SCSI_LOW_MIN_TOUT)
		cb->ccb_tcmax = SCSI_LOW_MIN_TOUT;
	cb->ccb_tc = cb->ccb_tcmax;

	/* setup saved scsi data pointer */
	cb->ccb_sscp = cb->ccb_scp;

	/* setup current scsi pointer */ 
	slp->sl_scp = cb->ccb_sscp;
	slp->sl_error = cb->ccb_error;

	/* selection start */
	slp->sl_selid = ti;
	rv = ((*slp->sl_funcs->scsi_low_start_bus) (slp, cb));
	if (rv == SCSI_LOW_START_OK)
	{
#ifdef	SCSI_LOW_STATICS
		scsi_low_statics.nexus_win ++;
#endif	/* SCSI_LOW_STATICS */
		return;
	}

#ifdef	SCSI_LOW_STATICS
	scsi_low_statics.nexus_fail ++;
#endif	/* SCSI_LOW_STATICS */
	SCSI_LOW_SETUP_PHASE(ti, PH_NULL);
	scsi_low_clear_nexus(slp, ti);
}

void
scsi_low_clear_nexus(slp, ti)
	struct scsi_low_softc *slp;
	struct targ_info *ti;
{

	/* clear all nexus pointer */
	ti->ti_nexus = NULL;
	ti->ti_li = NULL;
	slp->sl_nexus = NULL;

	/* clear selection assert */
	slp->sl_selid = NULL;

	/* clear nexus data */
	slp->sl_nexus_call = 0;
	slp->sl_scp.scp_direction = SCSI_LOW_RWUNK;
}

static int
scsi_low_done(slp, cb)
	struct scsi_low_softc *slp;
	struct slccb *cb;
{
#ifdef CAM
	union ccb *ccb;
#else
	struct scsipi_xfer *xs;
#endif
	struct targ_info *ti;
	struct lun_info *li;

	ti = cb->ti;
	li = cb->li;
#ifdef CAM
	ccb = cb->ccb;
#else
	xs = cb->xs;
#endif
	if (cb->ccb_error == 0)
	{
		if ((cb->ccb_flags & CCB_SENSE) != 0)
		{
			cb->ccb_flags &= ~CCB_SENSE;
#ifdef CAM
			ccb->csio.sense_data = cb->ccb_sense;
			/* ccb->ccb_h.status = CAM_AUTOSENSE_FAIL; */
			ccb->ccb_h.status = CAM_REQ_CMP;
			/* ccb->ccb_h.status = CAM_AUTOSNS_VALID|CAM_SCSI_STATUS_ERROR; */
#else
			xs->sense.scsi_sense = cb->ccb_sense;
			xs->error = XS_SENSE;
#endif
		}
		else switch (ti->ti_status)
		{
		case ST_GOOD:
			if (slp->sl_scp.scp_datalen == 0)
			{
#ifdef CAM
				ccb->ccb_h.status = CAM_REQ_CMP;
#else
				xs->error = XS_NOERROR;
#endif
				break;
			}

#define	SCSIPI_SCSI_CD_COMPLETELY_BUGGY	"YES"
#ifdef	SCSIPI_SCSI_CD_COMPLETELY_BUGGY
#ifdef CAM
			if (/* cb->bp == NULL &&  */
			    slp->sl_scp.scp_datalen < cb->ccb_scp.scp_datalen)
#else
			if (xs->bp == NULL &&
			    slp->sl_scp.scp_datalen < cb->ccb_scp.scp_datalen)
#endif
			{
#ifdef CAM
				ccb->ccb_h.status = CAM_REQ_CMP;
#else
				xs->error = XS_NOERROR;
#endif
				break;
			}	
#endif	/* SCSIPI_SCSI_CD_COMPLETELY_BUGGY */

			cb->ccb_error |= PDMAERR;
#ifdef CAM
			ccb->ccb_h.status = CAM_REQ_CMP_ERR;
#else
			xs->error = XS_DRIVER_STUFFUP;
#endif
			break;

		case ST_CHKCOND:
		case ST_MET:
			cb->ccb_flags |= CCB_SENSE;
#ifdef CAM
			ccb->ccb_h.status = CAM_AUTOSENSE_FAIL;
#else
			xs->error = XS_SENSE;
#endif
			goto retry;

		case ST_BUSY:
			cb->ccb_error |= BUSYERR;
#ifdef CAM
			ccb->ccb_h.status = CAM_BUSY; /* SCSI_STATUS_ERROR; */
#else
			xs->error = XS_BUSY;
#endif
			break;

		default:
			cb->ccb_error |= FATALIO;
#ifdef CAM
			ccb->ccb_h.status = CAM_REQ_CMP_ERR;
#else
			xs->error = XS_DRIVER_STUFFUP;
#endif
			break;
		}
	}
	else
	{
		cb->ccb_flags &= ~CCB_SENSE;
		if (ti->ti_phase == PH_SELSTART)
		{
#ifdef CAM
			ccb->ccb_h.status = CAM_CMD_TIMEOUT;
#else
			xs->error = XS_TIMEOUT;
#endif
			slp->sl_error |= SELTIMEOUTIO;
			if (li->li_state == UNIT_SLEEP)
				cb->ccb_error |= ABORTIO;
		}
		else
		{
#ifdef CAM
			ccb->ccb_h.status = CAM_REQ_CMP_ERR;
#else
			xs->error = XS_DRIVER_STUFFUP;
#endif
		}

		if ((cb->ccb_error & ABORTIO) != 0)
		{
			cb->ccb_rcnt = slp->sl_max_retry;
#ifdef CAM
			ccb->ccb_h.status = CAM_REQ_ABORTED;
#endif
		}
	}

	/* target state check */
	if (li->li_state < li->li_maxstate)
	{
		if (cb->ccb_rcnt < slp->sl_max_retry)
		{
			li->li_state ++;
			cb->ccb_rcnt = 0;
			goto retry;
		}
	}

	/* internal retry check */
#ifdef CAM
	if (ccb->ccb_h.status == CAM_REQ_CMP)
	{
		ccb->csio.resid = 0;
	}
	else
	{
#if 0
		if (ccb->ccb_h.status != CAM_AUTOSENSE_FAIL && 
		    cb->ccb_rcnt < slp->sl_max_retry)
			goto retry;
#endif
#else
	if (xs->error == XS_NOERROR)
	{
		xs->resid = 0;
	}
	else
	{
		if (xs->error != XS_SENSE && 
		    cb->ccb_rcnt < slp->sl_max_retry)
			goto retry;
#endif

#ifndef CAM
#ifdef SCSI_LOW_WARNINGS
		if (xs->bp != NULL)
		{
			scsi_low_print(slp, ti);
			printf("%s: WARNING: File system IO abort\n",
				slp->sl_xname);
		}
#endif	/* SCSI_LOW_WARNINGS */
#endif
	}

#ifdef CAM
	ccb->csio.scsi_status = ti->ti_status;
	xpt_done(ccb);
#else
	xs->flags |= ITSDONE;
	if ((xs->flags & SCSI_POLL) == 0)
		scsipi_done(xs);
#endif

	/* free our target */
	TAILQ_REMOVE(&slp->sl_start, cb, ccb_chain);
	scsi_low_free_ccb(cb);
	return SCSI_LOW_DONE_COMPLETE;

retry:
	cb->ccb_rcnt ++;
	if (slp->sl_start.tqh_first != cb)
	{
		TAILQ_REMOVE(&slp->sl_start, cb, ccb_chain);
		TAILQ_INSERT_HEAD(&slp->sl_start, cb, ccb_chain);
	}
	return SCSI_LOW_DONE_RETRY;
}

/**************************************************************
 * Reset
 **************************************************************/
static void
scsi_low_clear_ccb(cb)
	struct slccb *cb;
{

	cb->ccb_flags &= ~CCB_SENSE;
	cb->ccb_tag = SCSI_LOW_UNKTAG;
}

static void
scsi_low_reset_nexus(slp, fdone)
	struct scsi_low_softc *slp;
	int fdone;
{
	struct targ_info *ti;
	struct lun_info *li;
	struct slccb *cb, *ncb;

	/* current nexus */
	ti = slp->sl_nexus;
	if (ti != NULL && (cb = ti->ti_nexus) != NULL)
	{
		scsi_low_clear_ccb(cb);
		if (fdone != 0 && cb->ccb_rcnt ++ >= slp->sl_max_retry)
		{
			cb->ccb_error |= FATALIO;
			scsi_low_done(slp, cb);
		}
	}

	/* disconnected nexus */
	for (ti = slp->sl_titab.tqh_first; ti != NULL;
	     ti = ti->ti_chain.tqe_next)
	{
		for (cb = ti->ti_discq.tqh_first; cb != NULL; cb = ncb)
		{
		     	ncb = cb->ccb_chain.tqe_next;
			TAILQ_REMOVE(&ti->ti_discq, cb, ccb_chain);
			TAILQ_INSERT_HEAD(&slp->sl_start, cb, ccb_chain);
			scsi_low_clear_ccb(cb);
			if (fdone != 0 && cb->ccb_rcnt ++ >= slp->sl_max_retry)
			{
				cb->ccb_error |= FATALIO;
				scsi_low_done(slp, cb);
			}
		}

		for (li = LIST_FIRST(&ti->ti_litab); li != NULL;
		     li = LIST_NEXT(li, lun_chain))
		{
			li->li_state = UNIT_SLEEP;
			li->li_disc = 0;
			((*slp->sl_funcs->scsi_low_lun_init) (slp, ti, li));
			scsi_low_calcf(ti, li);
		}

		scsi_low_init_msgsys(slp, ti);
		scsi_low_clear_nexus(slp, ti);
		SCSI_LOW_SETUP_PHASE(ti, PH_NULL);
	}

	slp->sl_flags &= ~HW_PDMASTART;
	slp->sl_disc = 0;
}

/* misc */
static int tw_pos;
static char tw_chars[] = "|/-\\";

static void
scsi_low_twiddle_wait(void)
{

	cnputc('\b');
	cnputc(tw_chars[tw_pos++]);
	tw_pos %= (sizeof(tw_chars) - 1);
	delay(TWIDDLEWAIT);
}

void
scsi_low_bus_reset(slp)
	struct scsi_low_softc *slp;
{
	int i;

	(*slp->sl_funcs->scsi_low_bus_reset) (slp);

	printf("%s: try to reset scsi bus  ", slp->sl_xname);
	for (i = 0; i <= SCSI2_RESET_DELAY / TWIDDLEWAIT ; i++)
		scsi_low_twiddle_wait();
	cnputc('\b');
	printf("\n");
}

int
scsi_low_restart(slp, flags, s)
	struct scsi_low_softc *slp;
	int flags;
	u_char *s;
{
	int error;

	if (s != NULL)
		printf("%s: scsi bus restart. reason: %s\n", slp->sl_xname, s);

	if ((error = scsi_low_init(slp, flags)) != 0)
		return error;

	scsi_low_start(slp);
	return 0;
}

/**************************************************************
 * disconnect and reselect
 **************************************************************/
#define	MSGCMD_LUN(msg)	(msg & 0x07)

static struct lun_info *
scsi_low_establish_lun(ti, lun)
	struct targ_info *ti;
	int lun;
{
	struct lun_info *li;

	li = scsi_low_alloc_li(ti, lun, 0);
	if (li == NULL)
		return li;

	ti->ti_li = li;
	return li;
}

static struct slccb *
scsi_low_establish_ccb(ti, li, tag)
	struct targ_info *ti;
	struct lun_info *li;
	scsi_low_tag_t tag;
{
	struct scsi_low_softc *slp = ti->ti_sc;
	struct slccb *cb;

	/*
	 * Search ccb matching with lun and tag.
	 */
	cb = ti->ti_discq.tqh_first;
	for ( ; cb != NULL; cb = cb->ccb_chain.tqe_next)
		if (cb->li == li && cb->ccb_tag == tag)
			goto found;
	return cb;

	/* 
	 * establish our ccb nexus
	 */
found:
	TAILQ_REMOVE(&ti->ti_discq, cb, ccb_chain);
	TAILQ_INSERT_HEAD(&slp->sl_start, cb, ccb_chain);
	ti->ti_nexus = cb;

	slp->sl_scp = cb->ccb_sscp;
	slp->sl_error |= cb->ccb_error;

	slp->sl_disc --;
	li->li_disc --;

	/* inform "ccb nexus established" to the host driver */
	slp->sl_nexus_call = 1;
	(*slp->sl_funcs->scsi_low_establish_nexus) (slp, ti);
	return cb;
}

struct targ_info *
scsi_low_reselected(slp, targ)
	struct scsi_low_softc *slp;
	u_int targ;
{
	struct targ_info *ti;
	u_char *s;

	/* 
	 * Check select vs reselected collision.
	 */

	if ((ti = slp->sl_selid) != NULL)
	{
		scsi_low_clear_nexus(slp, ti);
		SCSI_LOW_SETUP_PHASE(ti, PH_NULL);
#ifdef	SCSI_LOW_STATICS
		scsi_low_statics.nexus_conflict ++;
#endif	/* SCSI_LOW_STATICS */
	}
	else if (slp->sl_nexus != NULL)
	{
		s = "host busy";
		goto world_restart;
	}

	/* 
	 * Check a valid target id asserted ?
	 */
	if (targ >= slp->sl_ntargs || targ == slp->sl_hostid)
	{
		s = "scsi id illegal";
		goto world_restart;
	}

	/* 
	 * Check the target scsi status.
	 */
	ti = slp->sl_ti[targ];
	if (ti->ti_phase != PH_DISC)
	{
		s = "phase mismatch";
		goto world_restart;
	}

	/* 
	 * Setup lun and init msgsys
	 */
	slp->sl_error = 0;
	scsi_low_init_msgsys(slp, ti);

	/* 
	 * Establish our target nexus
	 * Remark: ccb and scsi pointer not yet restored 
	 * 	   if lun != SCSI_LOW_UNKLUN.
	 */
	SCSI_LOW_SETUP_PHASE(ti, PH_RESEL);
	slp->sl_nexus = ti;
#ifdef	SCSI_LOW_STATICS
	scsi_low_statics.nexus_reselected ++;
#endif	/* SCSI_LOW_STATICS */
	return ti;

world_restart:
	printf("%s: reselect(%x:unknown) %s\n", slp->sl_xname, targ, s);
	scsi_low_restart(slp, SCSI_LOW_RESTART_HARD, 
		         "reselect: scsi world confused");
	return NULL;
}

int
scsi_low_disconnected(slp, ti)
	struct scsi_low_softc *slp;
	struct targ_info *ti;
{
	struct slccb *cb = ti->ti_nexus;

	/* check phase completion */
	switch (slp->sl_msgphase)
	{
	case MSGPH_DISC:
		if (cb != NULL)
		{
			TAILQ_REMOVE(&slp->sl_start, cb, ccb_chain);
			TAILQ_INSERT_TAIL(&ti->ti_discq, cb, ccb_chain);
			cb->ccb_error |= slp->sl_error;
			cb->li->li_disc ++;
			slp->sl_disc ++;
		}
		SCSI_LOW_SETUP_PHASE(ti, PH_DISC);
#ifdef	SCSI_LOW_STATICS
		scsi_low_statics.nexus_disconnected ++;
#endif	/* SCSI_LOW_STATICS */
		break;

	case MSGPH_NULL:
		slp->sl_error |= FATALIO;

	case MSGPH_CMDC:
		if (cb != NULL)
		{
			cb->ccb_error |= slp->sl_error;
			scsi_low_done(slp, cb);
		}
		SCSI_LOW_SETUP_PHASE(ti, PH_NULL);
		break;
	}

	scsi_low_clear_nexus(slp, ti);	
	scsi_low_start(slp);
	return 1;
}

/**************************************************************
 * cmd out pointer setup
 **************************************************************/
int
scsi_low_cmd(slp, ti)
	struct scsi_low_softc *slp;
	struct targ_info *ti;
{
	struct slccb *cb = ti->ti_nexus;
	
	if (cb == NULL)
	{
		/*
		 * no slccb, abort!
		 */
		slp->sl_scp.scp_cmd = (u_int8_t *) &unit_ready_cmd;
		slp->sl_scp.scp_cmdlen = sizeof(unit_ready_cmd);
		slp->sl_scp.scp_datalen = 0;
		slp->sl_scp.scp_direction = SCSI_LOW_READ;
		scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_ABORT, 1);
		scsi_low_info(slp, ti, "CMDOUT: slccb nexus not found");
	}
	else if (slp->sl_nexus_call == 0)
	{
		slp->sl_nexus_call = 1;
		(*slp->sl_funcs->scsi_low_establish_nexus) (slp, ti);
	}
	return 0;
}

/**************************************************************
 * data out pointer setup
 **************************************************************/
int
scsi_low_data(slp, ti, bp, direction)
	struct scsi_low_softc *slp;
	struct targ_info *ti;
	struct buf **bp;
	int direction;
{
	struct slccb *cb = ti->ti_nexus;

	if (cb == NULL)
	{
		scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_ABORT, 1);
		scsi_low_info(slp, ti, "DATA PHASE: slccb nexus not found");
		return EINVAL;
	}

	if (direction != cb->ccb_scp.scp_direction)
	{
		scsi_low_info(slp, ti, "DATA PHASE: xfer direction mismatch");
		return EINVAL;
	}

#ifdef CAM
	*bp = NULL; /* (cb->ccb == NULL) ? NULL : cb->bp; */
#else
	*bp = (cb->xs == NULL) ? NULL : cb->xs->bp;
#endif
	return 0;
}

/**************************************************************
 * MSG_SYS 
 **************************************************************/
#define	MSGINPTR_CLR(ti) {(ti)->ti_msginptr = 0; (ti)->ti_msginlen = 0;}
#define	MSGIN_PERIOD(ti) ((ti)->ti_msgin[3])
#define	MSGIN_OFFSET(ti) ((ti)->ti_msgin[4])
#define	MSGIN_DATA_LAST	0x30

static int scsi_low_errfunc_synch __P((struct targ_info *, u_int));
static int scsi_low_errfunc_wide __P((struct targ_info *, u_int));
static int scsi_low_errfunc_identify __P((struct targ_info *, u_int));

static int scsi_low_msgfunc_synch __P((struct targ_info *));
static int scsi_low_msgfunc_wide __P((struct targ_info *));
static int scsi_low_msgfunc_identify __P((struct targ_info *));
static int scsi_low_msgfunc_user __P((struct targ_info *));
static int scsi_low_msgfunc_abort __P((struct targ_info *));

struct scsi_low_msgout_data {
	u_int	md_flags;
	u_int8_t md_msg;
	int (*md_msgfunc) __P((struct targ_info *));
	int (*md_errfunc) __P((struct targ_info *, u_int));
};

struct scsi_low_msgout_data scsi_low_msgout_data[] = {
/* 0 */	{SCSI_LOW_MSG_RESET, MSG_RESET, scsi_low_msgfunc_abort, NULL},
/* 1 */ {SCSI_LOW_MSG_ABORT, MSG_ABORT, scsi_low_msgfunc_abort, NULL},
/* 2 */ {SCSI_LOW_MSG_REJECT, MSG_REJECT, NULL, NULL},
/* 3 */ {SCSI_LOW_MSG_PARITY, MSG_PARITY, NULL, NULL},
/* 4 */ {SCSI_LOW_MSG_ERROR, MSG_I_ERROR, NULL, NULL},
/* 5 */ {SCSI_LOW_MSG_IDENTIFY, 0, scsi_low_msgfunc_identify, scsi_low_errfunc_identify},
/* 6 */ {SCSI_LOW_MSG_SYNCH, 0, scsi_low_msgfunc_synch, scsi_low_errfunc_synch},
/* 7 */ {SCSI_LOW_MSG_WIDE, 0, scsi_low_msgfunc_wide, scsi_low_errfunc_wide},
/* 8 */ {SCSI_LOW_MSG_USER, 0, scsi_low_msgfunc_user, NULL},
/* 9 */ {SCSI_LOW_MSG_NOOP, MSG_NOOP, NULL, NULL},
/* 10 */ {SCSI_LOW_MSG_ALL, 0},
};

static int scsi_low_msginfunc_ext __P((struct targ_info *));
static int scsi_low_synch __P((struct targ_info *));
static int scsi_low_msginfunc_msg_reject __P((struct targ_info *));
static int scsi_low_msginfunc_rejop __P((struct targ_info *));
static int scsi_low_msginfunc_rdp __P((struct targ_info *));
static int scsi_low_msginfunc_sdp __P((struct targ_info *));
static int scsi_low_msginfunc_disc __P((struct targ_info *));
static int scsi_low_msginfunc_cc __P((struct targ_info *));
static int scsi_low_msginfunc_parity __P((struct targ_info *));
static int scsi_low_msginfunc_noop __P((struct targ_info *));
static void scsi_low_retry_phase __P((struct targ_info *));

struct scsi_low_msgin_data {
	u_int md_len;
	int (*md_msgfunc) __P((struct targ_info *));
};

struct scsi_low_msgin_data scsi_low_msgin_data[] = {
/* 0 */	{1,	scsi_low_msginfunc_cc},
/* 1 */ {2,	scsi_low_msginfunc_ext},
/* 2 */ {1,	scsi_low_msginfunc_sdp},
/* 3 */ {1,	scsi_low_msginfunc_rdp},
/* 4 */ {1,	scsi_low_msginfunc_disc},
/* 5 */ {1,	scsi_low_msginfunc_rejop},
/* 6 */ {1,	scsi_low_msginfunc_rejop},
/* 7 */ {1,	scsi_low_msginfunc_msg_reject},
/* 8 */ {1,	scsi_low_msginfunc_noop},
/* 9 */ {1,	scsi_low_msginfunc_parity},
/* a */ {1,	scsi_low_msginfunc_rejop},
/* b */ {1,	scsi_low_msginfunc_rejop},
/* c */ {1,	scsi_low_msginfunc_rejop},
/* d */ {2,	scsi_low_msginfunc_rejop},
/* e */ {1,	scsi_low_msginfunc_rejop},
/* f */ {1,	scsi_low_msginfunc_rejop},
/* 0x10 */ {1,	scsi_low_msginfunc_rejop},
/* 0x11 */ {1,	scsi_low_msginfunc_rejop},
/* 0x12 */ {1,	scsi_low_msginfunc_rejop},
/* 0x13 */ {1,	scsi_low_msginfunc_rejop},
/* 0x14 */ {1,	scsi_low_msginfunc_rejop},
/* 0x15 */ {1,	scsi_low_msginfunc_rejop},
/* 0x16 */ {1,	scsi_low_msginfunc_rejop},
/* 0x17 */ {1,	scsi_low_msginfunc_rejop},
/* 0x18 */ {1,	scsi_low_msginfunc_rejop},
/* 0x19 */ {1,	scsi_low_msginfunc_rejop},
/* 0x1a */ {1,	scsi_low_msginfunc_rejop},
/* 0x1b */ {1,	scsi_low_msginfunc_rejop},
/* 0x1c */ {1,	scsi_low_msginfunc_rejop},
/* 0x1d */ {1,	scsi_low_msginfunc_rejop},
/* 0x1e */ {1,	scsi_low_msginfunc_rejop},
/* 0x1f */ {1,	scsi_low_msginfunc_rejop},
/* 0x20 */ {2,	scsi_low_msginfunc_rejop},
/* 0x21 */ {2,	scsi_low_msginfunc_rejop},
/* 0x22 */ {2,	scsi_low_msginfunc_rejop},
/* 0x23 */ {2,	scsi_low_msginfunc_rejop},
/* 0x24 */ {2,	scsi_low_msginfunc_rejop},
/* 0x25 */ {2,	scsi_low_msginfunc_rejop},
/* 0x26 */ {2,	scsi_low_msginfunc_rejop},
/* 0x27 */ {2,	scsi_low_msginfunc_rejop},
/* 0x28 */ {2,	scsi_low_msginfunc_rejop},
/* 0x29 */ {2,	scsi_low_msginfunc_rejop},
/* 0x2a */ {2,	scsi_low_msginfunc_rejop},
/* 0x2b */ {2,	scsi_low_msginfunc_rejop},
/* 0x2c */ {2,	scsi_low_msginfunc_rejop},
/* 0x2d */ {2,	scsi_low_msginfunc_rejop},
/* 0x2e */ {2,	scsi_low_msginfunc_rejop},
/* 0x2f */ {2,	scsi_low_msginfunc_rejop},
/* 0x30 */ {1,	scsi_low_msginfunc_rejop}	/* default rej op */
};

static void
scsi_low_init_msgsys(slp, ti)
	struct scsi_low_softc *slp;
	struct targ_info *ti;
{

	ti->ti_msginptr = 0;
	ti->ti_emsgflags = ti->ti_msgflags = ti->ti_omsgflags = 0;
	ti->ti_tflags &= ~TARG_ASSERT_ATN;
	SCSI_LOW_SETUP_MSGPHASE(slp, MSGPH_NULL);
}

/**************************************************************
 * msgout
 **************************************************************/
static int
scsi_low_msgfunc_synch(ti)
	struct targ_info *ti;
{
	struct lun_info *li = ti->ti_li;
	int ptr = ti->ti_msgoutlen;

	if (li == NULL)
	{
		scsi_low_assert_msg(ti->ti_sc, ti, SCSI_LOW_MSG_ABORT, 0);
		return EINVAL;
	}

	ti->ti_msgoutstr[ptr + 0] = MSG_EXTEND;
	ti->ti_msgoutstr[ptr + 1] = MSG_EXTEND_SYNCHLEN;
	ti->ti_msgoutstr[ptr + 2] = MSG_EXTEND_SYNCHCODE;
	ti->ti_msgoutstr[ptr + 3] = li->li_maxsynch.period;
	ti->ti_msgoutstr[ptr + 4] = li->li_maxsynch.offset;
	return MSG_EXTEND_SYNCHLEN + 2;
}

static int
scsi_low_msgfunc_wide(ti)
	struct targ_info *ti;
{
	struct lun_info *li = ti->ti_li;
	int ptr = ti->ti_msgoutlen;

	if (li == NULL)
	{
		scsi_low_assert_msg(ti->ti_sc, ti, SCSI_LOW_MSG_ABORT, 0);
		return EINVAL;
	}

	ti->ti_msgoutstr[ptr + 0] = MSG_EXTEND;
	ti->ti_msgoutstr[ptr + 1] = MSG_EXTEND_WIDELEN;
	ti->ti_msgoutstr[ptr + 2] = MSG_EXTEND_WIDECODE;
	ti->ti_msgoutstr[ptr + 3] = li->li_width;
	return MSG_EXTEND_WIDELEN + 2;
}

static int
scsi_low_msgfunc_identify(ti)
	struct targ_info *ti;
{
	int ptr = ti->ti_msgoutlen;;

	if (ti->ti_li == NULL)
	{
		ti->ti_msgoutstr[ptr + 0] = 0x80;
		scsi_low_info(ti->ti_sc, ti, "MSGOUT: lun unknown");
		scsi_low_assert_msg(ti->ti_sc, ti, SCSI_LOW_MSG_ABORT, 0);
	}
	else
	{
		ti->ti_msgoutstr[ptr + 0] = ID_MSG_SETUP(ti);
	}
	return 1;
}

static int
scsi_low_msgfunc_user(ti)
	struct targ_info *ti;
{
#ifdef	SCSI_LOW_SUPPORT_USER_MSGOUT
	struct slccb *cb = ti->ti_nexus;
	int ptr = ti->ti_msgoutlen;;

	if (ti->ti_nexus == NULL)
	{
		ti->ti_msgoutstr[ptr + 0] = MSG_NOOP;
		return 1;
	}
	else
	{
		bcopy(cb->msgout, ti->ti_msgoutstr + ptr, SCSI_LOW_MAX_MSGLEN);
		return cb->msgoutlen;
	}
#else	/* !SCSI_LOW_SUPPORT_USER_MSGOUT */
	return 0;
#endif	/* !SCSI_LOW_SUPPORT_USER_MSGOUT */
}

static int
scsi_low_msgfunc_abort(ti)
	struct targ_info *ti;
{
	struct scsi_low_softc *slp = ti->ti_sc;

	/* The target should releases bus */
	SCSI_LOW_SETUP_MSGPHASE(slp, MSGPH_CMDC);
	slp->sl_error |= /* ABORTIO */ FATALIO;
	return 1;
}

/*
 * The following functions are called when targets give unexpected
 * responces in msgin (after msgout).
 */
static int
scsi_low_errfunc_identify(ti, msgflags)
	struct targ_info *ti;
	u_int msgflags;
{
	struct lun_info *li = ti->ti_li;

	li->li_flags &= ~SCSI_LOW_DISC;
	return 0;
}

static int
scsi_low_errfunc_synch(ti, msgflags)
	struct targ_info *ti;
	u_int msgflags;
{

	/* XXX:
	 * illegal behavior, however
	 * there are buggy devices!
	 */
	MSGIN_PERIOD(ti) = 0;
	MSGIN_OFFSET(ti) = 0;
	scsi_low_synch(ti);
	return 0;
}

static int
scsi_low_errfunc_wide(ti, msgflags)
	struct targ_info *ti;
	u_int msgflags;
{
	struct lun_info *li = ti->ti_li;

	li->li_width = 0;
	return 0;
}

int
scsi_low_msgout(slp, ti)
	struct scsi_low_softc *slp;
	struct targ_info *ti;
{
	struct scsi_low_msgout_data *mdp;
	int len = 0;

	/* STEP I.
	 * Scsi phase changes.
	 * Previously msgs asserted are accepted by our target or
	 * processed by scsi_low_msgin.
	 * Thus clear all saved informations.
	 */
	if (ti->ti_ophase != ti->ti_phase)
	{
		ti->ti_omsgflags = 0;
		ti->ti_emsgflags = 0;
	}

	/* STEP II.
	 * We did not assert attention, however still our target required
	 * msgs. Resend previous msgs. 
	 */
	if (ti->ti_ophase == PH_MSGOUT && !(ti->ti_tflags & TARG_ASSERT_ATN))
	{
		ti->ti_msgflags |= ti->ti_omsgflags;
#ifdef	SCSI_LOW_DIAGNOSTIC
		printf("%s: scsi_low_msgout: retry msgout\n", slp->sl_xname);
#endif	/* SCSI_LOW_DIAGNOSTIC */
	}

	/*
	 * OK. clear flags.
	 */
	ti->ti_tflags &= ~TARG_ASSERT_ATN;

	/* STEP III.
	 * We have no msgs. send MSG_LOOP (OK?)
	 */
	if (scsi_low_is_msgout_continue(ti) == 0)
		scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_NOOP, 0);

	/* STEP IV.
	 * Process all msgs
	 */
	ti->ti_msgoutlen = 0;
	mdp = &scsi_low_msgout_data[0];
	for ( ; mdp->md_flags != SCSI_LOW_MSG_ALL; mdp ++)
	{
		if ((ti->ti_msgflags & mdp->md_flags) != 0)
		{
			ti->ti_omsgflags |= mdp->md_flags;
			ti->ti_msgflags &= ~mdp->md_flags;
			ti->ti_emsgflags = mdp->md_flags;

			ti->ti_msgoutstr[ti->ti_msgoutlen] = mdp->md_msg;
			if (mdp->md_msgfunc != NULL)
				len = (*mdp->md_msgfunc) (ti);
			else
				len = 1;

			ti->ti_msgoutlen += len;
			if ((slp->sl_cfgflags & CFG_MSGUNIFY) == 0 || 
			    ti->ti_msgflags == 0)
				break;
			if (ti->ti_msgoutlen >= SCSI_LOW_MAX_MSGLEN - 5)
				break;
		}
	}

	if (scsi_low_is_msgout_continue(ti) != 0)
	{
#ifdef	SCSI_LOW_DIAGNOSTIC
		printf("SCSI_LOW_ATTENTION(msgout): 0x%x\n", ti->ti_msgflags);
#endif	/* SCSI_LOW_DIAGNOSTIC */
		scsi_low_attention(slp, ti);
	}

	/*
	 * OK. advance old phase.
	 */
	ti->ti_ophase = ti->ti_phase;
	return ti->ti_msgoutlen;
}

/**************************************************************
 * msgin
 **************************************************************/
static int
scsi_low_msginfunc_noop(ti)
	struct targ_info *ti;
{

	return 0;
}

static int
scsi_low_msginfunc_rejop(ti)
	struct targ_info *ti;
{
	struct scsi_low_softc *slp = ti->ti_sc;
	u_int8_t msg = ti->ti_msgin[0];

	printf("%s: MSGIN: msg 0x%x reject\n", slp->sl_xname, (u_int) msg);
	scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_REJECT, 0);
	return 0;
}

static int
scsi_low_msginfunc_cc(ti)
	struct targ_info *ti;
{
	struct scsi_low_softc *slp = ti->ti_sc;

	SCSI_LOW_SETUP_MSGPHASE(slp, MSGPH_CMDC);
	return 0;
}

static int
scsi_low_msginfunc_disc(ti)
	struct targ_info *ti;
{
	struct scsi_low_softc *slp = ti->ti_sc;

	SCSI_LOW_SETUP_MSGPHASE(slp, MSGPH_DISC);
	return 0;
}

static int
scsi_low_msginfunc_sdp(ti)
	struct targ_info *ti;
{
	struct scsi_low_softc *slp = ti->ti_sc;

	if (ti->ti_nexus != NULL)
		ti->ti_nexus->ccb_sscp = slp->sl_scp;
	else
		scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_REJECT, 0);
	return 0;
}

static int
scsi_low_msginfunc_rdp(ti)
	struct targ_info *ti;
{
	struct scsi_low_softc *slp = ti->ti_sc;

	if (ti->ti_nexus != NULL)
		slp->sl_scp = ti->ti_nexus->ccb_sscp;
	else
		scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_REJECT, 0);
	return 0;
}

static int
scsi_low_synch(ti)
	struct targ_info *ti;
{
	struct scsi_low_softc *slp = ti->ti_sc;
	struct lun_info *li = ti->ti_li;
	u_int period = 0, offset = 0, speed;
	u_char *s;
	int error;

	if (MSGIN_PERIOD(ti) >= li->li_maxsynch.period &&
	    MSGIN_OFFSET(ti) <= li->li_maxsynch.offset)
	{
		if ((offset = MSGIN_OFFSET(ti)) != 0)
			period = MSGIN_PERIOD(ti);
		s = offset ? "synchronous" : "async";
	}
	else
	{
		/* XXX:
		 * Target seems to be brain damaged.
		 * Force async transfer.
		 */
		li->li_maxsynch.period = 0;
		li->li_maxsynch.offset = 0;
		printf("%s: target brain damaged. async transfer\n",
			slp->sl_xname);
		return EINVAL;
	}

	li->li_maxsynch.period = period;
	li->li_maxsynch.offset = offset;

	error = (*slp->sl_funcs->scsi_low_msg) (slp, ti, SCSI_LOW_MSG_SYNCH);
	if (error != 0)
	{
		/* XXX:
		 * Current period and offset are not acceptable 
		 * for our adapter.
		 * The adapter changes max synch and max offset.
		 */
		printf("%s: synch neg failed. retry synch msg neg ...\n",
			slp->sl_xname);
		return error;
	}

#ifdef SCSI_LOW_INFORM
	/* inform data */
	printf("%s(%d:%d): <%s> offset %d period %dns ",
		slp->sl_xname, ti->ti_id, li->li_lun, s, offset, period * 4);
	if (period != 0)
	{
		speed = 1000 * 10 / (period * 4);
		printf("%d.%d M/s", speed / 10, speed % 10);
	}
	printf("\n");
#endif

	return 0;
}

static int
scsi_low_msginfunc_ext(ti)
	struct targ_info *ti;
{
	struct scsi_low_softc *slp = ti->ti_sc;
	struct slccb *cb = ti->ti_nexus;
	struct lun_info *li = ti->ti_li;
	int count, retry;
	u_int32_t *ptr;

	if (ti->ti_msginptr == 2)
	{
		ti->ti_msginlen = ti->ti_msgin[1] + 2;
		return 0;
	}

	switch (MKMSG_EXTEND(ti->ti_msgin[1], ti->ti_msgin[2]))
	{
	case MKMSG_EXTEND(MSG_EXTEND_MDPLEN, MSG_EXTEND_MDPCODE):
		if (cb == NULL)
			break;

		ptr = (u_int32_t *)(&ti->ti_msgin[3]);
		count = (int) htonl((long) (*ptr));
		if(slp->sl_scp.scp_datalen - count < 0 || 
		   slp->sl_scp.scp_datalen - count > cb->ccb_scp.scp_datalen)
			break;

		slp->sl_scp.scp_datalen -= count;
		slp->sl_scp.scp_data += count;
		return 0;

	case MKMSG_EXTEND(MSG_EXTEND_SYNCHLEN, MSG_EXTEND_SYNCHCODE):
		if (li == NULL)
			break;

		retry = scsi_low_synch(ti);
		if (retry != 0 || (ti->ti_emsgflags & SCSI_LOW_MSG_SYNCH) == 0)
			scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_SYNCH, 0);
		return 0;

	case MKMSG_EXTEND(MSG_EXTEND_WIDELEN, MSG_EXTEND_WIDECODE):
		if (li == NULL)
			break;

		scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_WIDE, 0);
		return 0;

	default:
		break;
	}

	scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_REJECT, 0);
	return EINVAL;
}

static void
scsi_low_retry_phase(ti)
	struct targ_info *ti;
{

	switch (ti->ti_sphase)
	{
	case PH_MSGOUT:
		ti->ti_msgflags |= ti->ti_omsgflags;	
		break;

	default:
		break;
	}
}

static int
scsi_low_msginfunc_parity(ti)
	struct targ_info *ti;
{
	struct scsi_low_softc *slp = ti->ti_sc;

	if (ti->ti_sphase != PH_MSGOUT)
		slp->sl_error |= PARITYERR;
	scsi_low_retry_phase(ti);
	return 0;
}

static int
scsi_low_msginfunc_msg_reject(ti)
	struct targ_info *ti;
{
	struct scsi_low_softc *slp = ti->ti_sc;
	struct lun_info *li = ti->ti_li;
	struct scsi_low_msgout_data *mdp;
	u_int msgflags;

	if (li == NULL)
	{
		/* not yet lun nexus established! */
		goto out;
	}

	switch (ti->ti_sphase)
	{
	case PH_CMD:
		slp->sl_error |= CMDREJECT;
		break;

	case PH_MSGOUT:
		if (ti->ti_emsgflags == 0)
			break;

		msgflags = SCSI_LOW_MSG_REJECT;
		mdp = &scsi_low_msgout_data[0];
		for ( ; mdp->md_flags != SCSI_LOW_MSG_ALL; mdp ++)
		{
			if ((ti->ti_emsgflags & mdp->md_flags) != 0)
			{
				ti->ti_emsgflags &= ~mdp->md_flags;
				if (mdp->md_errfunc != NULL)
					(*mdp->md_errfunc) (ti, msgflags);
				break;
			}
		}
		break;

	default:
		break;
	}

out:
	scsi_low_info(slp, ti, "msg rejected");
	slp->sl_error |= MSGERR;
	return 0;
}

void
scsi_low_msgin(slp, ti, c)
	struct scsi_low_softc *slp;
	struct targ_info *ti;
	u_int8_t c;
{
	struct scsi_low_msgin_data *sdp;
	struct lun_info *li;
	u_int8_t msg;

	/*
	 * Phase changes, clear the pointer.
	 */
	if (ti->ti_ophase != ti->ti_phase)
	{
		ti->ti_sphase = ti->ti_ophase;
		ti->ti_ophase = ti->ti_phase;
		MSGINPTR_CLR(ti);
#ifdef	SCSI_LOW_DIAGNOSTIC
		ti->ti_msgin_hist_pointer = 0;
#endif	/* SCSI_LOW_DIAGNOSTIC */
	}

	/*
	 * Store a current messages byte into buffer and 
	 * wait for the completion of the current msg.
	 */
	ti->ti_msgin[ti->ti_msginptr ++] = c;
	if (ti->ti_msginptr >= SCSI_LOW_MAX_MSGLEN)
	{
		ti->ti_msginptr = SCSI_LOW_MAX_MSGLEN - 1;
		scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_REJECT, 0);
	}	

	/*
	 * Calculate messages length.
	 */
	msg = ti->ti_msgin[0];
	if (msg < MSGIN_DATA_LAST)
		sdp = &scsi_low_msgin_data[msg];
	else
		sdp = &scsi_low_msgin_data[MSGIN_DATA_LAST];

	if (ti->ti_msginlen == 0)
	{
		ti->ti_msginlen = sdp->md_len;
#ifdef	SCSI_LOW_DIAGNOSTIC
	    	if (ti->ti_msgin_hist_pointer < MSGIN_HISTORY_LEN)
		{
			ti->ti_msgin_history[ti->ti_msgin_hist_pointer] = msg;
			ti->ti_msgin_hist_pointer ++;
		}
#endif	/* SCSI_LOW_DIAGNOSTIC */
	}

	/*
	 * Check comletion.
	 */
	if (ti->ti_msginptr < ti->ti_msginlen)
		return;

	/*
	 * Do process.
	 */
	if ((msg & MSG_IDENTIFY) == 0)
	{
		(void) ((*sdp->md_msgfunc) (ti));
	}
	else
	{
		li = ti->ti_li;
		if (li == NULL)
		{
			li = scsi_low_establish_lun(ti, MSGCMD_LUN(msg));
			if (li == NULL)
				goto badlun;
		}	

		if (ti->ti_nexus == NULL)
		{
			/* XXX:
		 	 * move the following functions to
			 * tag queue msg process in the future.
			 */
			if (!scsi_low_establish_ccb(ti, li, SCSI_LOW_UNKTAG))
				goto badlun;
		}

		if (MSGCMD_LUN(msg) != li->li_lun)
			goto badlun;
	}

	/*
	 * Msg process completed, reset msin pointer and assert ATN if desired.
	 */
	if (ti->ti_msginptr >= ti->ti_msginlen)
	{
		ti->ti_sphase = ti->ti_phase;
		MSGINPTR_CLR(ti);

		if (scsi_low_is_msgout_continue(ti) != 0)
		{
#ifdef	SCSI_LOW_DIAGNOSTIC
			printf("SCSI_LOW_ATTETION(msgin): 0x%x\n", 
				ti->ti_msgflags);
#endif	/* SCSI_LOW_DIAGNOSTIC */
			scsi_low_attention(slp, ti);
		}
	}
	return;

badlun:
	scsi_low_info(slp, ti, "MSGIN: identify lun mismatch");
	scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_ABORT, 0);
}

/**************************************************************
 * Qurik setup
 **************************************************************/
#define	MAXOFFSET	0x10

static void
scsi_low_calcf(ti, li)
	struct targ_info *ti;
	struct lun_info *li;
{
	u_int period;
	u_int8_t offset;
	struct scsi_low_softc *slp = ti->ti_sc;

	li->li_flags &= ~SCSI_LOW_DISC;
	if ((slp->sl_cfgflags & CFG_NODISC) == 0 &&
#ifdef	SDEV_NODISC
	    (li->li_quirks & SDEV_NODISC) == 0 &&
#endif	/* SDEV_NODISC */
	    (li->li_cfgflags & SCSI_LOW_DISC) != 0)
		li->li_flags |= SCSI_LOW_DISC;

	li->li_flags |= SCSI_LOW_NOPARITY;
	if ((slp->sl_cfgflags & CFG_NOPARITY) == 0 &&
#ifdef	SDEV_NOPARITY
	    (li->li_quirks & SDEV_NOPARITY) == 0 &&
#endif	/* SDEV_NOPARITY */
	    (li->li_cfgflags & SCSI_LOW_NOPARITY) == 0)
		li->li_flags &= ~SCSI_LOW_NOPARITY;

	li->li_flags &= ~SCSI_LOW_SYNC;
	if ((li->li_cfgflags & SCSI_LOW_SYNC) &&
	    (slp->sl_cfgflags & CFG_ASYNC) == 0)
	{
		offset = SCSI_LOW_OFFSET(li->li_cfgflags);
		if (offset > li->li_maxsynch.offset)
			offset = li->li_maxsynch.offset;
		li->li_flags |= SCSI_LOW_SYNC;
	}
	else
		offset = 0;
	
	if (offset > 0)
	{
		period = SCSI_LOW_PERIOD(li->li_cfgflags);
		if (period > SCSI_LOW_MAX_SYNCH_SPEED)
			period = SCSI_LOW_MAX_SYNCH_SPEED;
		if (period != 0)
			period = 1000 * 10 / (period * 4);
		if (period < li->li_maxsynch.period)
			period = li->li_maxsynch.period;
	}
	else
		period = 0;

	li->li_maxsynch.offset = offset;
	li->li_maxsynch.period = period;
}

#ifdef	SCSI_LOW_TARGET_OPEN
static int
scsi_low_target_open(link, cf)
	struct scsipi_link *link;
	struct cfdata *cf;
{
	u_int target = link->scsipi_scsi.target;
	u_int lun = link->scsipi_scsi.lun;
	struct scsi_low_softc *slp;
	struct targ_info *ti;
	struct lun_info *li;

	slp = (struct scsi_low_softc *) link->adapter_softc;
	ti = slp->sl_ti[target];
	li = scsi_low_alloc_li(ti, lun, 0);
	if (li == NULL)
		return 0;

	li->li_quirks = (u_int) link->quirks;
	li->li_cfgflags = cf->cf_flags;
	if (li->li_state > UNIT_SYNCH)
		li->li_state = UNIT_SYNCH;

	scsi_low_calcf(ti, li);

	printf("%s(%d:%d): max period(%dns) max offset(%d) flags 0x%b\n",
		slp->sl_xname, target, lun,
		li->li_maxsynch.period * 4,
		li->li_maxsynch.offset,
		li->li_flags, SCSI_LOW_BITS);
	return 0;
}
#endif	/* SCSI_LOW_TARGET_OPEN */

/**********************************************************
 * DEBUG SECTION
 **********************************************************/
static void
scsi_low_info(slp, ti, s)
	struct scsi_low_softc *slp;
	struct targ_info *ti;
	u_char *s;
{

	printf("%s: SCSI_LOW: %s\n", slp->sl_xname, s);
	if (ti == NULL)
	{
		for (ti = slp->sl_titab.tqh_first; ti != NULL;
		     ti = ti->ti_chain.tqe_next)
			scsi_low_print(slp, ti);
	}
	else
		scsi_low_print(slp, ti);
			
}

static u_char *phase[] =
{
	"FREE", "ARBSTART", "SELSTART", "SELECTED",
	"CMDOUT", "DATA", "MSGIN", "MSGOUT", "STATIN", "DISC", "RESEL"
};

void
scsi_low_print(slp, ti)
	struct scsi_low_softc *slp;
	struct targ_info *ti;
{
	struct slccb *cb = NULL;

	if (ti == NULL)
		ti = slp->sl_nexus;
	if (ti != NULL)
		cb = ti->ti_nexus;

	printf("%s: TARGET(0x%lx) T_NEXUS(0x%lx) C_NEXUS(0x%lx) NDISCS(%d)\n",
	       slp->sl_xname, (u_long) ti, (u_long) slp->sl_nexus,
	       (u_long) cb, slp->sl_disc);

	/* target stat */
	if (ti != NULL)
	{
		struct sc_p *sp = &slp->sl_scp;
		struct lun_info *li = ti->ti_li;
		u_int flags = 0;
		int lun = -1;

		if (li != NULL)
		{
			lun = li->li_lun;
			flags = li->li_flags;
		}

		printf("%s(%d:%d) ph<%s> => ph<%s>\n", slp->sl_xname,
		       ti->ti_id, lun, phase[(int) ti->ti_ophase], 
		       phase[(int) ti->ti_phase]);

printf("MSGIN: ptr(%x) [%x][%x][%x][%x][%x] STATUSIN: 0x%x T_FLAGS: 0x%x\n",
		       (u_int) (ti->ti_msginptr), 
		       (u_int) (ti->ti_msgin[0]),
		       (u_int) (ti->ti_msgin[1]),
		       (u_int) (ti->ti_msgin[2]),
		       (u_int) (ti->ti_msgin[3]),
		       (u_int) (ti->ti_msgin[4]),
		       ti->ti_status, ti->ti_tflags);
#ifdef	SCSI_LOW_DIAGNOSTIC
printf("MSGIN HISTORY: (%d) [0x%x] => [0x%x] => [0x%x] => [0x%x] => [0x%x]\n",
			ti->ti_msgin_hist_pointer,
			(u_int) (ti->ti_msgin_history[0]),
			(u_int) (ti->ti_msgin_history[1]),
			(u_int) (ti->ti_msgin_history[2]),
			(u_int) (ti->ti_msgin_history[3]),
			(u_int) (ti->ti_msgin_history[4]));
#endif	/* SCSI_LOW_DIAGNOSTIC */

printf("MSGOUT: msgflags 0x%x [%x][%x][%x][%x][%x] msgoutlen %d C_FLAGS: %b\n",
			(u_int) ti->ti_msgflags,
			(u_int) (ti->ti_msgoutstr[0]), 
			(u_int) (ti->ti_msgoutstr[1]), 
			(u_int) (ti->ti_msgoutstr[2]), 
			(u_int) (ti->ti_msgoutstr[3]), 
			(u_int) (ti->ti_msgoutstr[4]), 
		        ti->ti_msgoutlen,
			flags, SCSI_LOW_BITS);

printf("SCP: datalen 0x%x dataaddr 0x%lx ",
			sp->scp_datalen,
			(u_long) sp->scp_data);

		if (cb != NULL)
		{
printf("CCB: cmdlen %x cmdaddr %lx cmd[0] %x datalen %x",
		       cb->ccb_scp.scp_cmdlen, 
		       (u_long) cb->ccb_scp.scp_cmd,
		       (u_int) cb->ccb_scp.scp_cmd[0],
		       cb->ccb_scp.scp_datalen);
		}
		printf("\n");
	}
	printf("error flags %b\n", slp->sl_error, SCSI_LOW_ERRORBITS);
}
