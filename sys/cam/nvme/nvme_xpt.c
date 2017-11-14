/*-
 * Copyright (c) 2015 Netflix, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * derived from ata_xpt.c: Copyright (c) 2009 Alexander Motin <mav@FreeBSD.org>
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/time.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/interrupt.h>
#include <sys/sbuf.h>

#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_queue.h>
#include <cam/cam_periph.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_xpt_periph.h>
#include <cam/cam_xpt_internal.h>
#include <cam/cam_debug.h>

#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>
#include <cam/nvme/nvme_all.h>
#include <machine/stdarg.h>	/* for xpt_print below */
#include "opt_cam.h"

struct nvme_quirk_entry {
	u_int quirks;
#define CAM_QUIRK_MAXTAGS 1
	u_int mintags;
	u_int maxtags;
};

/* Not even sure why we need this */
static periph_init_t nvme_probe_periph_init;

static struct periph_driver nvme_probe_driver =
{
	nvme_probe_periph_init, "nvme_probe",
	TAILQ_HEAD_INITIALIZER(nvme_probe_driver.units), /* generation */ 0,
	CAM_PERIPH_DRV_EARLY
};

PERIPHDRIVER_DECLARE(nvme_probe, nvme_probe_driver);

typedef enum {
	NVME_PROBE_IDENTIFY,
	NVME_PROBE_DONE,
	NVME_PROBE_INVALID,
	NVME_PROBE_RESET
} nvme_probe_action;

static char *nvme_probe_action_text[] = {
	"NVME_PROBE_IDENTIFY",
	"NVME_PROBE_DONE",
	"NVME_PROBE_INVALID",
	"NVME_PROBE_RESET",
};

#define NVME_PROBE_SET_ACTION(softc, newaction)	\
do {									\
	char **text;							\
	text = nvme_probe_action_text;					\
	CAM_DEBUG((softc)->periph->path, CAM_DEBUG_PROBE,		\
	    ("Probe %s to %s\n", text[(softc)->action],			\
	    text[(newaction)]));					\
	(softc)->action = (newaction);					\
} while(0)

typedef enum {
	NVME_PROBE_NO_ANNOUNCE	= 0x04
} nvme_probe_flags;

typedef struct {
	TAILQ_HEAD(, ccb_hdr) request_ccbs;
	nvme_probe_action	action;
	nvme_probe_flags	flags;
	int		restart;
	struct cam_periph *periph;
} nvme_probe_softc;

static struct nvme_quirk_entry nvme_quirk_table[] =
{
	{
//		{
//		  T_ANY, SIP_MEDIA_REMOVABLE|SIP_MEDIA_FIXED,
//		  /*vendor*/"*", /*product*/"*", /*revision*/"*"
//		},
		.quirks = 0, .mintags = 0, .maxtags = 0
	},
};

static const int nvme_quirk_table_size =
	sizeof(nvme_quirk_table) / sizeof(*nvme_quirk_table);

static cam_status	nvme_probe_register(struct cam_periph *periph,
				      void *arg);
static void	 nvme_probe_schedule(struct cam_periph *nvme_probe_periph);
static void	 nvme_probe_start(struct cam_periph *periph, union ccb *start_ccb);
static void	 nvme_probe_cleanup(struct cam_periph *periph);
//static void	 nvme_find_quirk(struct cam_ed *device);
static void	 nvme_scan_lun(struct cam_periph *periph,
			       struct cam_path *path, cam_flags flags,
			       union ccb *ccb);
static struct cam_ed *
		 nvme_alloc_device(struct cam_eb *bus, struct cam_et *target,
				   lun_id_t lun_id);
static void	 nvme_device_transport(struct cam_path *path);
static void	 nvme_dev_async(u_int32_t async_code,
				struct cam_eb *bus,
				struct cam_et *target,
				struct cam_ed *device,
				void *async_arg);
static void	 nvme_action(union ccb *start_ccb);
static void	 nvme_announce_periph(struct cam_periph *periph);
static void	 nvme_proto_announce(struct cam_ed *device);
static void	 nvme_proto_denounce(struct cam_ed *device);
static void	 nvme_proto_debug_out(union ccb *ccb);

static struct xpt_xport_ops nvme_xport_ops = {
	.alloc_device = nvme_alloc_device,
	.action = nvme_action,
	.async = nvme_dev_async,
	.announce = nvme_announce_periph,
};
#define NVME_XPT_XPORT(x, X)			\
static struct xpt_xport nvme_xport_ ## x = {	\
	.xport = XPORT_ ## X,			\
	.name = #x,				\
	.ops = &nvme_xport_ops,			\
};						\
CAM_XPT_XPORT(nvme_xport_ ## x);

NVME_XPT_XPORT(nvme, NVME);

#undef NVME_XPT_XPORT

static struct xpt_proto_ops nvme_proto_ops = {
	.announce = nvme_proto_announce,
	.denounce = nvme_proto_denounce,
	.debug_out = nvme_proto_debug_out,
};
static struct xpt_proto nvme_proto = {
	.proto = PROTO_NVME,
	.name = "nvme",
	.ops = &nvme_proto_ops,
};
CAM_XPT_PROTO(nvme_proto);

static void
nvme_probe_periph_init()
{

}

static cam_status
nvme_probe_register(struct cam_periph *periph, void *arg)
{
	union ccb *request_ccb;	/* CCB representing the probe request */
	cam_status status;
	nvme_probe_softc *softc;

	request_ccb = (union ccb *)arg;
	if (request_ccb == NULL) {
		printf("nvme_probe_register: no probe CCB, "
		       "can't register device\n");
		return(CAM_REQ_CMP_ERR);
	}

	softc = (nvme_probe_softc *)malloc(sizeof(*softc), M_CAMXPT, M_ZERO | M_NOWAIT);

	if (softc == NULL) {
		printf("nvme_probe_register: Unable to probe new device. "
		       "Unable to allocate softc\n");
		return(CAM_REQ_CMP_ERR);
	}
	TAILQ_INIT(&softc->request_ccbs);
	TAILQ_INSERT_TAIL(&softc->request_ccbs, &request_ccb->ccb_h,
			  periph_links.tqe);
	softc->flags = 0;
	periph->softc = softc;
	softc->periph = periph;
	softc->action = NVME_PROBE_INVALID;
	status = cam_periph_acquire(periph);
	if (status != CAM_REQ_CMP) {
		return (status);
	}
	CAM_DEBUG(periph->path, CAM_DEBUG_PROBE, ("Probe started\n"));

//	nvme_device_transport(periph->path);
	nvme_probe_schedule(periph);

	return(CAM_REQ_CMP);
}

static void
nvme_probe_schedule(struct cam_periph *periph)
{
	union ccb *ccb;
	nvme_probe_softc *softc;

	softc = (nvme_probe_softc *)periph->softc;
	ccb = (union ccb *)TAILQ_FIRST(&softc->request_ccbs);

	NVME_PROBE_SET_ACTION(softc, NVME_PROBE_IDENTIFY);

	if (ccb->crcn.flags & CAM_EXPECT_INQ_CHANGE)
		softc->flags |= NVME_PROBE_NO_ANNOUNCE;
	else
		softc->flags &= ~NVME_PROBE_NO_ANNOUNCE;

	xpt_schedule(periph, CAM_PRIORITY_XPT);
}

static void
nvme_probe_start(struct cam_periph *periph, union ccb *start_ccb)
{
	struct ccb_nvmeio *nvmeio;
	struct ccb_scsiio *csio;
	nvme_probe_softc *softc;
	struct cam_path *path;
	const struct nvme_namespace_data *nvme_data;
	lun_id_t lun;

	CAM_DEBUG(start_ccb->ccb_h.path, CAM_DEBUG_TRACE, ("nvme_probe_start\n"));

	softc = (nvme_probe_softc *)periph->softc;
	path = start_ccb->ccb_h.path;
	nvmeio = &start_ccb->nvmeio;
	csio = &start_ccb->csio;
	nvme_data = periph->path->device->nvme_data;

	if (softc->restart) {
		softc->restart = 0;
		if (periph->path->device->flags & CAM_DEV_UNCONFIGURED)
			NVME_PROBE_SET_ACTION(softc, NVME_PROBE_RESET);
		else
			NVME_PROBE_SET_ACTION(softc, NVME_PROBE_IDENTIFY);
	}

	/*
	 * Other transports have to ask their SIM to do a lot of action.
	 * NVMe doesn't, so don't do the dance. Just do things
	 * directly.
	 */
	switch (softc->action) {
	case NVME_PROBE_RESET:
		/* FALLTHROUGH */
	case NVME_PROBE_IDENTIFY:
		nvme_device_transport(path);
		/*
		 * Test for lun == CAM_LUN_WILDCARD is lame, but
		 * appears to be necessary here. XXX
		 */
		lun = xpt_path_lun_id(periph->path);
		if (lun == CAM_LUN_WILDCARD ||
		    periph->path->device->flags & CAM_DEV_UNCONFIGURED) {
			path->device->flags &= ~CAM_DEV_UNCONFIGURED;
			xpt_acquire_device(path->device);
			start_ccb->ccb_h.func_code = XPT_GDEV_TYPE;
			xpt_action(start_ccb);
			xpt_async(AC_FOUND_DEVICE, path, start_ccb);
		}
		NVME_PROBE_SET_ACTION(softc, NVME_PROBE_DONE);
		break;
	default:
		panic("nvme_probe_start: invalid action state 0x%x\n", softc->action);
	}
	/*
	 * Probing is now done. We need to complete any lingering items
	 * in the queue, though there shouldn't be any.
	 */
	xpt_release_ccb(start_ccb);
	CAM_DEBUG(periph->path, CAM_DEBUG_PROBE, ("Probe completed\n"));
	while ((start_ccb = (union ccb *)TAILQ_FIRST(&softc->request_ccbs))) {
		TAILQ_REMOVE(&softc->request_ccbs,
		    &start_ccb->ccb_h, periph_links.tqe);
		start_ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(start_ccb);
	}
	cam_periph_invalidate(periph);
	/* Can't release periph since we hit a (possibly bogus) assertion */
//	cam_periph_release_locked(periph);
}

static void
nvme_probe_cleanup(struct cam_periph *periph)
{

	free(periph->softc, M_CAMXPT);
}

#if 0
/* XXX should be used, don't delete */
static void
nvme_find_quirk(struct cam_ed *device)
{
	struct nvme_quirk_entry *quirk;
	caddr_t	match;

	match = cam_quirkmatch((caddr_t)&device->nvme_data,
			       (caddr_t)nvme_quirk_table,
			       nvme_quirk_table_size,
			       sizeof(*nvme_quirk_table), nvme_identify_match);

	if (match == NULL)
		panic("xpt_find_quirk: device didn't match wildcard entry!!");

	quirk = (struct nvme_quirk_entry *)match;
	device->quirk = quirk;
	if (quirk->quirks & CAM_QUIRK_MAXTAGS) {
		device->mintags = quirk->mintags;
		device->maxtags = quirk->maxtags;
	}
}
#endif

static void
nvme_scan_lun(struct cam_periph *periph, struct cam_path *path,
	     cam_flags flags, union ccb *request_ccb)
{
	struct ccb_pathinq cpi;
	cam_status status;
	struct cam_periph *old_periph;
	int lock;

	CAM_DEBUG(path, CAM_DEBUG_TRACE, ("nvme_scan_lun\n"));

	xpt_setup_ccb(&cpi.ccb_h, path, CAM_PRIORITY_NONE);
	cpi.ccb_h.func_code = XPT_PATH_INQ;
	xpt_action((union ccb *)&cpi);

	if (cpi.ccb_h.status != CAM_REQ_CMP) {
		if (request_ccb != NULL) {
			request_ccb->ccb_h.status = cpi.ccb_h.status;
			xpt_done(request_ccb);
		}
		return;
	}

	if (xpt_path_lun_id(path) == CAM_LUN_WILDCARD) {
		CAM_DEBUG(path, CAM_DEBUG_TRACE, ("nvme_scan_lun ignoring bus\n"));
		request_ccb->ccb_h.status = CAM_REQ_CMP;	/* XXX signal error ? */
		xpt_done(request_ccb);
		return;
	}

	lock = (xpt_path_owned(path) == 0);
	if (lock)
		xpt_path_lock(path);
	if ((old_periph = cam_periph_find(path, "nvme_probe")) != NULL) {
		if ((old_periph->flags & CAM_PERIPH_INVALID) == 0) {
			nvme_probe_softc *softc;

			softc = (nvme_probe_softc *)old_periph->softc;
			TAILQ_INSERT_TAIL(&softc->request_ccbs,
				&request_ccb->ccb_h, periph_links.tqe);
			softc->restart = 1;
			CAM_DEBUG(path, CAM_DEBUG_TRACE,
			    ("restarting nvme_probe device\n"));
		} else {
			request_ccb->ccb_h.status = CAM_REQ_CMP_ERR;
			CAM_DEBUG(path, CAM_DEBUG_TRACE,
			    ("Failing to restart nvme_probe device\n"));
			xpt_done(request_ccb);
		}
	} else {
		CAM_DEBUG(path, CAM_DEBUG_TRACE,
		    ("Adding nvme_probe device\n"));
		status = cam_periph_alloc(nvme_probe_register, NULL, nvme_probe_cleanup,
					  nvme_probe_start, "nvme_probe",
					  CAM_PERIPH_BIO,
					  request_ccb->ccb_h.path, NULL, 0,
					  request_ccb);

		if (status != CAM_REQ_CMP) {
			xpt_print(path, "xpt_scan_lun: cam_alloc_periph "
			    "returned an error, can't continue probe\n");
			request_ccb->ccb_h.status = status;
			xpt_done(request_ccb);
		}
	}
	if (lock)
		xpt_path_unlock(path);
}

static struct cam_ed *
nvme_alloc_device(struct cam_eb *bus, struct cam_et *target, lun_id_t lun_id)
{
	struct nvme_quirk_entry *quirk;
	struct cam_ed *device;

	device = xpt_alloc_device(bus, target, lun_id);
	if (device == NULL)
		return (NULL);

	/*
	 * Take the default quirk entry until we have inquiry
	 * data from nvme and can determine a better quirk to use.
	 */
	quirk = &nvme_quirk_table[nvme_quirk_table_size - 1];
	device->quirk = (void *)quirk;
	device->mintags = 0;
	device->maxtags = 0;
	device->inq_flags = 0;
	device->queue_flags = 0;
	device->device_id = NULL;	/* XXX Need to set this somewhere */
	device->device_id_len = 0;
	device->serial_num = NULL;	/* XXX Need to set this somewhere */
	device->serial_num_len = 0;
	return (device);
}

static void
nvme_device_transport(struct cam_path *path)
{
	struct ccb_pathinq cpi;
	struct ccb_trans_settings cts;
	/* XXX get data from nvme namespace and other info ??? */

	/* Get transport information from the SIM */
	xpt_setup_ccb(&cpi.ccb_h, path, CAM_PRIORITY_NONE);
	cpi.ccb_h.func_code = XPT_PATH_INQ;
	xpt_action((union ccb *)&cpi);

	path->device->transport = cpi.transport;
	path->device->transport_version = cpi.transport_version;

	path->device->protocol = cpi.protocol;
	path->device->protocol_version = cpi.protocol_version;

	/* Tell the controller what we think */
	xpt_setup_ccb(&cts.ccb_h, path, CAM_PRIORITY_NONE);
	cts.ccb_h.func_code = XPT_SET_TRAN_SETTINGS;
	cts.type = CTS_TYPE_CURRENT_SETTINGS;
	cts.transport = path->device->transport;
	cts.transport_version = path->device->transport_version;
	cts.protocol = path->device->protocol;
	cts.protocol_version = path->device->protocol_version;
	cts.proto_specific.valid = 0;
	cts.xport_specific.valid = 0;
	xpt_action((union ccb *)&cts);
}

static void
nvme_dev_advinfo(union ccb *start_ccb)
{
	struct cam_ed *device;
	struct ccb_dev_advinfo *cdai;
	off_t amt; 

	start_ccb->ccb_h.status = CAM_REQ_INVALID;
	device = start_ccb->ccb_h.path->device;
	cdai = &start_ccb->cdai;
	switch(cdai->buftype) {
	case CDAI_TYPE_SCSI_DEVID:
		if (cdai->flags & CDAI_FLAG_STORE)
			return;
		cdai->provsiz = device->device_id_len;
		if (device->device_id_len == 0)
			break;
		amt = device->device_id_len;
		if (cdai->provsiz > cdai->bufsiz)
			amt = cdai->bufsiz;
		memcpy(cdai->buf, device->device_id, amt);
		break;
	case CDAI_TYPE_SERIAL_NUM:
		if (cdai->flags & CDAI_FLAG_STORE)
			return;
		cdai->provsiz = device->serial_num_len;
		if (device->serial_num_len == 0)
			break;
		amt = device->serial_num_len;
		if (cdai->provsiz > cdai->bufsiz)
			amt = cdai->bufsiz;
		memcpy(cdai->buf, device->serial_num, amt);
		break;
	case CDAI_TYPE_PHYS_PATH:
		if (cdai->flags & CDAI_FLAG_STORE) {
			if (device->physpath != NULL)
				free(device->physpath, M_CAMXPT);
			device->physpath_len = cdai->bufsiz;
			/* Clear existing buffer if zero length */
			if (cdai->bufsiz == 0)
				break;
			device->physpath = malloc(cdai->bufsiz, M_CAMXPT, M_NOWAIT);
			if (device->physpath == NULL) {
				start_ccb->ccb_h.status = CAM_REQ_ABORTED;
				return;
			}
			memcpy(device->physpath, cdai->buf, cdai->bufsiz);
		} else {
			cdai->provsiz = device->physpath_len;
			if (device->physpath_len == 0)
				break;
			amt = device->physpath_len;
			if (cdai->provsiz > cdai->bufsiz)
				amt = cdai->bufsiz;
			memcpy(cdai->buf, device->physpath, amt);
		}
		break;
	case CDAI_TYPE_NVME_CNTRL:
		if (cdai->flags & CDAI_FLAG_STORE)
			return;
		amt = sizeof(struct nvme_controller_data);
		cdai->provsiz = amt;
		if (amt > cdai->bufsiz)
			amt = cdai->bufsiz;
		memcpy(cdai->buf, device->nvme_cdata, amt);
		break;
	case CDAI_TYPE_NVME_NS:
		if (cdai->flags & CDAI_FLAG_STORE)
			return;
		amt = sizeof(struct nvme_namespace_data);
		cdai->provsiz = amt;
		if (amt > cdai->bufsiz)
			amt = cdai->bufsiz;
		memcpy(cdai->buf, device->nvme_data, amt);
		break;
	default:
		return;
	}
	start_ccb->ccb_h.status = CAM_REQ_CMP;

	if (cdai->flags & CDAI_FLAG_STORE) {
		xpt_async(AC_ADVINFO_CHANGED, start_ccb->ccb_h.path,
			  (void *)(uintptr_t)cdai->buftype);
	}
}

static void
nvme_action(union ccb *start_ccb)
{
	CAM_DEBUG(start_ccb->ccb_h.path, CAM_DEBUG_TRACE,
	    ("nvme_action: func= %#x\n", start_ccb->ccb_h.func_code));

	switch (start_ccb->ccb_h.func_code) {
	case XPT_SCAN_BUS:
	case XPT_SCAN_TGT:
	case XPT_SCAN_LUN:
		nvme_scan_lun(start_ccb->ccb_h.path->periph,
			      start_ccb->ccb_h.path, start_ccb->crcn.flags,
			      start_ccb);
		break;
	case XPT_DEV_ADVINFO:
		nvme_dev_advinfo(start_ccb);
		break;

	default:
		xpt_action_default(start_ccb);
		break;
	}
}

/*
 * Handle any per-device event notifications that require action by the XPT.
 */
static void
nvme_dev_async(u_int32_t async_code, struct cam_eb *bus, struct cam_et *target,
	      struct cam_ed *device, void *async_arg)
{

	/*
	 * We only need to handle events for real devices.
	 */
	if (target->target_id == CAM_TARGET_WILDCARD
	 || device->lun_id == CAM_LUN_WILDCARD)
		return;

	if (async_code == AC_LOST_DEVICE &&
	    (device->flags & CAM_DEV_UNCONFIGURED) == 0) {
		device->flags |= CAM_DEV_UNCONFIGURED;
		xpt_release_device(device);
	}
}

static void
nvme_announce_periph(struct cam_periph *periph)
{
	struct	ccb_pathinq cpi;
	struct	ccb_trans_settings cts;
	struct	cam_path *path = periph->path;

	cam_periph_assert(periph, MA_OWNED);

	xpt_setup_ccb(&cts.ccb_h, path, CAM_PRIORITY_NORMAL);
	cts.ccb_h.func_code = XPT_GET_TRAN_SETTINGS;
	cts.type = CTS_TYPE_CURRENT_SETTINGS;
	xpt_action((union ccb*)&cts);
	if ((cts.ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP)
		return;
	/* Ask the SIM for its base transfer speed */
	xpt_setup_ccb(&cpi.ccb_h, path, CAM_PRIORITY_NORMAL);
	cpi.ccb_h.func_code = XPT_PATH_INQ;
	xpt_action((union ccb *)&cpi);
	/* XXX NVME STUFF HERE */
	printf("\n");
}

static void
nvme_proto_announce(struct cam_ed *device)
{

	nvme_print_ident(device->nvme_cdata, device->nvme_data);
}

static void
nvme_proto_denounce(struct cam_ed *device)
{

	nvme_print_ident(device->nvme_cdata, device->nvme_data);
}

static void
nvme_proto_debug_out(union ccb *ccb)
{
	char cdb_str[(sizeof(struct nvme_command) * 3) + 1];

	if (ccb->ccb_h.func_code != XPT_NVME_IO)
		return;

	CAM_DEBUG(ccb->ccb_h.path,
	    CAM_DEBUG_CDB,("%s. NCB: %s\n", nvme_op_string(&ccb->nvmeio.cmd),
		nvme_cmd_string(&ccb->nvmeio.cmd, cdb_str, sizeof(cdb_str))));
}

