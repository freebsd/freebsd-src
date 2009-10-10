/*-
 * Copyright (c) 2005-2008 Daniel Braniss <danny@cs.huji.ac.il>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_iscsi_initiator.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/callout.h>
#if __FreeBSD_version >= 700000
#include <sys/lock.h>
#include <sys/mutex.h>
#endif
#include <sys/conf.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/uio.h>
#include <sys/sysctl.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_periph.h>

#include <dev/iscsi/initiator/iscsi.h>
#include <dev/iscsi/initiator/iscsivar.h>

// XXX: untested/incomplete
void
ic_freeze(isc_session_t *sp)
{
     debug_called(8);
#if 0
     sdebug(2, "freezing path=%p", sp->cam_path == NULL? 0: sp->cam_path);
     if((sp->cam_path != NULL) && !(sp->flags & ISC_FROZEN)) {
	  xpt_freeze_devq(sp->cam_path, 1);
     }
#endif
     sp->flags |= ISC_FROZEN;
}

// XXX: untested/incomplete
void
ic_release(isc_session_t *sp)
{
     debug_called(8);
#if 0
     sdebug(2, "release path=%p", sp->cam_path == NULL? 0: sp->cam_path);
     if((sp->cam_path != NULL) && (sp->flags & ISC_FROZEN)) {
	  xpt_release_devq(sp->cam_path, 1, TRUE);
     }
#endif
     sp->flags &= ~ISC_FROZEN;
}

void
ic_lost_target(isc_session_t *sp, int target)
{
     struct isc_softc   *isp = sp->isc;

     debug_called(8);
     sdebug(2, "target=%d", target);
     if(sp->cam_path != NULL) {
	  mtx_lock(&isp->cam_mtx);
	  xpt_async(AC_LOST_DEVICE, sp->cam_path, NULL);
	  xpt_free_path(sp->cam_path);
	  mtx_unlock(&isp->cam_mtx);
	  sp->cam_path = 0; // XXX
     }
}

static void
_scan_callback(struct cam_periph *periph, union ccb *ccb)
{
     isc_session_t *sp = (isc_session_t *)ccb->ccb_h.spriv_ptr0;

     debug_called(8);

     free(ccb, M_TEMP);

     if(sp->flags & ISC_FFPWAIT) {
	  sp->flags &= ~ISC_FFPWAIT;
	  wakeup(sp);
     }
}

static void
_scan_target(isc_session_t *sp, int target)
{
     union ccb		*ccb;

     debug_called(8);
     sdebug(2, "target=%d", target);

     if((ccb = malloc(sizeof(union ccb), M_TEMP, M_WAITOK | M_ZERO)) == NULL) {
	  xdebug("scan failed (can't allocate CCB)");
	  return;
     }
     CAM_LOCK(sp->isc);
     xpt_setup_ccb(&ccb->ccb_h, sp->cam_path, 5/*priority (low)*/);
     ccb->ccb_h.func_code	= XPT_SCAN_BUS;
     ccb->ccb_h.cbfcnp		= _scan_callback;
     ccb->crcn.flags		= CAM_FLAG_NONE;
     ccb->ccb_h.spriv_ptr0	= sp;

     xpt_action(ccb);
     CAM_UNLOCK(sp->isc);
}

int
ic_fullfeature(struct cdev *dev)
{
     struct isc_softc 	*isp = dev->si_drv1;
     isc_session_t	*sp = (isc_session_t *)dev->si_drv2;

     debug_called(8);
     sdebug(3, "dev=%d sc=%p", dev2unit(dev), isp);

     sp->flags &= ~ISC_FFPHASE;
     sp->flags |= ISC_FFPWAIT;

     CAM_LOCK(isp);
     if(xpt_create_path(&sp->cam_path, xpt_periph, cam_sim_path(sp->isc->cam_sim),
			sp->sid, CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
	  xdebug("can't create cam path");
	  CAM_UNLOCK(isp);
	  return ENODEV; // XXX
     }
     CAM_UNLOCK(isp);

     _scan_target(sp, sp->sid);

     while(sp->flags & ISC_FFPWAIT)
	  tsleep(sp, PRIBIO, "ffp", 5*hz); // the timeout time should
					    // be configurable
     if(sp->target_nluns > 0) {
	  sp->flags |= ISC_FFPHASE;
	  return 0;
     }

     return ENODEV;
}

static void
_inq(struct cam_sim *sim, union ccb *ccb, int maxluns)
{
     struct ccb_pathinq *cpi = &ccb->cpi;

     debug_called(4);

     cpi->version_num = 1; /* XXX??? */
     cpi->hba_inquiry = PI_SDTR_ABLE | PI_TAG_ABLE | PI_WIDE_32;
     cpi->target_sprt = 0;
     cpi->hba_misc = 0;
     cpi->hba_eng_cnt = 0;
     cpi->max_target = ISCSI_MAX_TARGETS - 1;
     cpi->initiator_id = ISCSI_MAX_TARGETS;
     cpi->max_lun = maxluns;
     cpi->bus_id = cam_sim_bus(sim);
     cpi->base_transfer_speed = 3300;
     strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
     strncpy(cpi->hba_vid, "iSCSI", HBA_IDLEN);
     strncpy(cpi->dev_name, cam_sim_name(sim), DEV_IDLEN);
     cpi->unit_number = cam_sim_unit(sim);
     cpi->ccb_h.status = CAM_REQ_CMP;
}

static __inline int
_scsi_encap(struct cam_sim *sim, union ccb *ccb)
{
     int		ret;

#if __FreeBSD_version < 700000
     ret = scsi_encap(sim, ccb);
#else
     struct isc_softc	*isp = (struct isc_softc *)cam_sim_softc(sim);

     mtx_unlock(&isp->cam_mtx);
     ret = scsi_encap(sim, ccb);
     mtx_lock(&isp->cam_mtx);
#endif
     return ret;
}

static void
ic_action(struct cam_sim *sim, union ccb *ccb)
{
     struct ccb_hdr	*ccb_h = &ccb->ccb_h;
     struct isc_softc	*isp = (struct isc_softc *)cam_sim_softc(sim);
     isc_session_t	*sp;

     debug_called(8);

     if((ccb_h->target_id != CAM_TARGET_WILDCARD) && (ccb_h->target_id < MAX_SESSIONS))
	  sp = isp->sessions[ccb_h->target_id];
     else
	  sp = NULL;

     ccb_h->spriv_ptr0 = sp;

     debug(4, "func_code=0x%x flags=0x%x status=0x%x target=%d lun=%d retry_count=%d timeout=%d",
	   ccb_h->func_code, ccb->ccb_h.flags, ccb->ccb_h.status,
	   ccb->ccb_h.target_id, ccb->ccb_h.target_lun, 
	   ccb->ccb_h.retry_count, ccb_h->timeout);
     /*
      | first quick check
      */
     switch(ccb_h->func_code) {
     default:
	  // XXX: maybe check something else?
	  break;

     case XPT_SCSI_IO:
     case XPT_RESET_DEV:
     case XPT_GET_TRAN_SETTINGS:
     case XPT_SET_TRAN_SETTINGS:
     case XPT_CALC_GEOMETRY:
	  if(sp == NULL) {
	       ccb->ccb_h.status = CAM_DEV_NOT_THERE;
#if __FreeBSD_version < 700000
	       XPT_DONE(isp, ccb);
#else
	       xpt_done(ccb);
#endif
	       return;
	  }
	  break;

     case XPT_PATH_INQ:
     case XPT_NOOP:
	  if(sp == NULL && ccb->ccb_h.target_id != CAM_TARGET_WILDCARD) {
	       ccb->ccb_h.status = CAM_DEV_NOT_THERE;
#if __FreeBSD_version < 700000
	       XPT_DONE(isp, ccb);
#else
	       xpt_done(ccb);
#endif
	       debug(4, "status = CAM_DEV_NOT_THERE");
	       return;
	  }
     }

     switch(ccb_h->func_code) {

     case XPT_PATH_INQ:
	  _inq(sim, ccb, (sp? sp->opt.maxluns: ISCSI_MAX_LUNS) - 1);
	  break;

     case XPT_RESET_BUS: // (can just be a stub that does nothing and completes)
     {
	  struct ccb_pathinq *cpi = &ccb->cpi;

	  debug(3, "XPT_RESET_BUS");
	  cpi->ccb_h.status = CAM_REQ_CMP;
	  break;
     }

     case XPT_SCSI_IO: 
     {
	  struct ccb_scsiio* csio = &ccb->csio;

	  debug(4, "XPT_SCSI_IO cmd=0x%x", csio->cdb_io.cdb_bytes[0]);
	  if(sp == NULL) {
	       ccb_h->status = CAM_REQ_INVALID; //CAM_NO_NEXUS;
	       debug(4, "xpt_done.status=%d", ccb_h->status);
	       break;
	  }
	  if(ccb_h->target_lun == CAM_LUN_WILDCARD) {
	       debug(3, "target=%d: bad lun (-1)", ccb_h->target_id);
	       ccb_h->status = CAM_LUN_INVALID;
	       break;
	  }
	  if(_scsi_encap(sim, ccb) != 0)
	       return;
	  break;
     }
 
     case XPT_CALC_GEOMETRY:
     {
	  struct	ccb_calc_geometry *ccg;

	  ccg = &ccb->ccg;
	  debug(6, "XPT_CALC_GEOMETRY vsize=%jd bsize=%d", ccg->volume_size, ccg->block_size);
	  if(ccg->block_size == 0 ||
	     (ccg->volume_size < ccg->block_size)) {
	       // print error message  ...
	       /* XXX: what error is appropiate? */
	       break;
	  } else
	       cam_calc_geometry(ccg, /*extended*/1);
	  break;
     }

     case XPT_GET_TRAN_SETTINGS:
     default:
	  ccb_h->status = CAM_REQ_INVALID;
	  break;
     }
#if __FreeBSD_version < 700000
     XPT_DONE(isp, ccb);
#else
     xpt_done(ccb);
#endif
     return;
}

static void
ic_poll(struct cam_sim *sim)
{
     debug_called(8);

}

int
ic_getCamVals(isc_session_t *sp, iscsi_cam_t *cp)
{
     int	i;

     debug_called(8);

     if(sp && sp->isc->cam_sim) {
	  cp->path_id = cam_sim_path(sp->isc->cam_sim);
	  cp->target_id = sp->sid;
	  cp->target_nluns = sp->target_nluns; // XXX: -1?
	  for(i = 0; i < cp->target_nluns; i++)
	       cp->target_lun[i] = sp->target_lun[i];
	  return 0;
     }
     return ENXIO;
}

void
ic_destroy(struct isc_softc *isp)
{
     debug_called(8);

     CAM_LOCK(isp); // can't harm :-)

     xpt_async(AC_LOST_DEVICE, isp->cam_path, NULL);
     xpt_free_path(isp->cam_path);
     
     xpt_bus_deregister(cam_sim_path(isp->cam_sim));
     cam_sim_free(isp->cam_sim, TRUE /*free_devq*/);

     CAM_UNLOCK(isp);
}

int
ic_init(struct isc_softc *isp)
{
     struct cam_sim	*sim;
     struct cam_devq	*devq;
     struct cam_path	*path;

     if((devq = cam_simq_alloc(256)) == NULL)
	  return ENOMEM;

#if __FreeBSD_version >= 700000
     mtx_init(&isp->cam_mtx, "isc-cam", NULL, MTX_DEF);
#else
     isp->cam_mtx = Giant;
#endif
     sim = cam_sim_alloc(ic_action, ic_poll,
			 "iscsi", isp, 0/*unit*/,
#if __FreeBSD_version >= 700000
			 &isp->cam_mtx,
#endif
			 1/*max_dev_transactions*/,
			 100/*max_tagged_dev_transactions*/,
			 devq);
     if(sim == NULL) {
	  cam_simq_free(devq);
#if __FreeBSD_version >= 700000
	  mtx_destroy(&isp->cam_mtx);
#endif
	  return ENXIO;
     }
     CAM_LOCK(isp);
     if(xpt_bus_register(sim,
#if __FreeBSD_version >= 700000
			 NULL,
#endif
			 0/*bus_number*/) != CAM_SUCCESS)
	  goto bad;

     if(xpt_create_path(&path, xpt_periph, cam_sim_path(sim),
			CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
	  xpt_bus_deregister(cam_sim_path(sim));
	  goto bad;
     }

     CAM_UNLOCK(isp);

     isp->cam_sim = sim;
     isp->cam_path = path;

     debug(2, "cam subsystem initialized"); // XXX: add dev ...
     debug(4, "sim=%p path=%p", sim, path);
     return 0;

 bad:
     cam_sim_free(sim, /*free_devq*/TRUE);
     CAM_UNLOCK(isp);
#if __FreeBSD_version >= 700000
     mtx_destroy(&isp->cam_mtx);
#endif
     return ENXIO;
}
