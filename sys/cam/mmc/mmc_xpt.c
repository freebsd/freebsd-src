/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2013,2014 Ilya Bakulin <ilya@bakulin.de>
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
 */

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
#include <sys/condvar.h>

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

#include <cam/mmc/mmc.h>
#include <cam/mmc/mmc_bus.h>

#include <machine/stdarg.h>	/* for xpt_print below */
#include <machine/_inttypes.h>  /* for PRIu64 */

FEATURE(mmccam, "CAM-based MMC/SD/SDIO stack");

static struct cam_ed * mmc_alloc_device(struct cam_eb *bus,
    struct cam_et *target, lun_id_t lun_id);
static void mmc_dev_async(uint32_t async_code, struct cam_eb *bus,
    struct cam_et *target, struct cam_ed *device, void *async_arg);
static void	 mmc_action(union ccb *start_ccb);
static void	 mmc_dev_advinfo(union ccb *start_ccb);
static void	 mmc_announce_periph_sbuf(struct cam_periph *periph,
    struct sbuf *sb);
static void	 mmc_scan_lun(struct cam_periph *periph,
    struct cam_path *path, cam_flags flags, union ccb *ccb);

/* mmcprobe methods */
static cam_status mmcprobe_register(struct cam_periph *periph, void *arg);
static void	 mmcprobe_start(struct cam_periph *periph, union ccb *start_ccb);
static void	 mmcprobe_cleanup(struct cam_periph *periph);
static void	 mmcprobe_done(struct cam_periph *periph, union ccb *done_ccb);

static void mmc_proto_announce_sbuf(struct cam_ed *device, struct sbuf *sb);
static void mmc_proto_denounce_sbuf(struct cam_ed *device, struct sbuf *sb);
static void mmc_proto_debug_out(union ccb *ccb);

typedef enum {
	PROBE_RESET,
	PROBE_IDENTIFY,
	PROBE_POWER_OFF,
	PROBE_GET_HOST_OCR,
	PROBE_RESET_BUS,
	PROBE_SET_ID_FREQ,
	PROBE_SET_CS,
	PROBE_GO_IDLE_STATE,
	PROBE_SDIO_RESET,
	PROBE_SEND_IF_COND,
	PROBE_SDIO_INIT,
	PROBE_MMC_INIT,
	PROBE_SEND_APP_OP_COND,
	PROBE_GET_CID,
	PROBE_GET_CSD,
	PROBE_SEND_RELATIVE_ADDR,
	PROBE_MMC_SET_RELATIVE_ADDR,
	PROBE_SELECT_CARD,
	PROBE_DONE,
	PROBE_INVALID
} probe_action;

static char *probe_action_text[] = {
	"PROBE_RESET",
	"PROBE_IDENTIFY",
	"PROBE_POWER_OFF",
	"PROBE_GET_HOST_OCR",
	"PROBE_RESET_BUS",
	"PROBE_SET_ID_FREQ",
	"PROBE_SET_CS",
	"PROBE_GO_IDLE_STATE",
	"PROBE_SDIO_RESET",
	"PROBE_SEND_IF_COND",
	"PROBE_SDIO_INIT",
	"PROBE_MMC_INIT",
	"PROBE_SEND_APP_OP_COND",
	"PROBE_GET_CID",
	"PROBE_GET_CSD",
	"PROBE_SEND_RELATIVE_ADDR",
	"PROBE_MMC_SET_RELATIVE_ADDR",
	"PROBE_SELECT_CARD",
	"PROBE_DONE",
	"PROBE_INVALID"
};

#define PROBE_SET_ACTION(softc, newaction)	\
do {									\
	char **text;							\
	text = probe_action_text;					\
	CAM_DEBUG((softc)->periph->path, CAM_DEBUG_PROBE,		\
	    ("Probe %s to %s\n", text[(softc)->action],			\
	    text[(newaction)]));					\
	(softc)->action = (newaction);					\
} while(0)

static struct xpt_xport_ops mmc_xport_ops = {
	.alloc_device = mmc_alloc_device,
	.action = mmc_action,
	.async = mmc_dev_async,
	.announce_sbuf = mmc_announce_periph_sbuf,
};

#define MMC_XPT_XPORT(x, X)				\
	static struct xpt_xport mmc_xport_ ## x = {	\
		.xport = XPORT_ ## X,			\
		.name = #x,				\
		.ops = &mmc_xport_ops,			\
	};						\
	CAM_XPT_XPORT(mmc_xport_ ## x);

MMC_XPT_XPORT(mmc, MMCSD);

static struct xpt_proto_ops mmc_proto_ops = {
	.announce_sbuf = mmc_proto_announce_sbuf,
	.denounce_sbuf = mmc_proto_denounce_sbuf,
	.debug_out = mmc_proto_debug_out,
};

static struct xpt_proto mmc_proto = {
	.proto = PROTO_MMCSD,
	.name = "mmcsd",
	.ops = &mmc_proto_ops,
};
CAM_XPT_PROTO(mmc_proto);

typedef struct {
	probe_action	action;
	int             restart;
	uint32_t	host_ocr;
	uint32_t	flags;
#define PROBE_FLAG_ACMD_SENT	0x1 /* CMD55 is sent, card expects ACMD */
#define PROBE_FLAG_HOST_CAN_DO_18V   0x2 /* Host can do 1.8V signaling */
	uint8_t         acmd41_count; /* how many times ACMD41 has been issued */
	struct cam_periph *periph;
} mmcprobe_softc;

/* XPort functions -- an interface to CAM at periph side */

static struct cam_ed *
mmc_alloc_device(struct cam_eb *bus, struct cam_et *target, lun_id_t lun_id)
{
	struct cam_ed *device;

	device = xpt_alloc_device(bus, target, lun_id);
	if (device == NULL)
		return (NULL);

	device->quirk = NULL;
	device->mintags = 0;
	device->maxtags = 0;
	bzero(&device->inq_data, sizeof(device->inq_data));
	device->inq_flags = 0;
	device->queue_flags = 0;
	device->serial_num = NULL;
	device->serial_num_len = 0;
	return (device);
}

static void
mmc_dev_async(uint32_t async_code, struct cam_eb *bus, struct cam_et *target,
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

/* Taken from nvme_scan_lun, thanks to bsdimp@ */
static void
mmc_scan_lun(struct cam_periph *periph, struct cam_path *path,
	     cam_flags flags, union ccb *request_ccb)
{
	struct ccb_pathinq cpi;
	cam_status status;
	struct cam_periph *old_periph;
	int lock;

	CAM_DEBUG(path, CAM_DEBUG_TRACE, ("mmc_scan_lun\n"));

	xpt_path_inq(&cpi, path);

	if (cpi.ccb_h.status != CAM_REQ_CMP) {
		if (request_ccb != NULL) {
			request_ccb->ccb_h.status = cpi.ccb_h.status;
			xpt_done(request_ccb);
		}
		return;
	}

	if (xpt_path_lun_id(path) == CAM_LUN_WILDCARD) {
		CAM_DEBUG(path, CAM_DEBUG_TRACE, ("mmd_scan_lun ignoring bus\n"));
		request_ccb->ccb_h.status = CAM_REQ_CMP;	/* XXX signal error ? */
		xpt_done(request_ccb);
		return;
	}

	lock = (xpt_path_owned(path) == 0);
	if (lock)
		xpt_path_lock(path);

	if ((old_periph = cam_periph_find(path, "mmcprobe")) != NULL) {
		if ((old_periph->flags & CAM_PERIPH_INVALID) == 0) {
//			mmcprobe_softc *softc;
//			softc = (mmcprobe_softc *)old_periph->softc;
//                      Not sure if we need request ccb queue for mmc
//			TAILQ_INSERT_TAIL(&softc->request_ccbs,
//				&request_ccb->ccb_h, periph_links.tqe);
//			softc->restart = 1;
                        CAM_DEBUG(path, CAM_DEBUG_INFO,
                                  ("Got scan request, but mmcprobe already exists\n"));
			request_ccb->ccb_h.status = CAM_REQ_CMP_ERR;
                        xpt_done(request_ccb);
		} else {
			request_ccb->ccb_h.status = CAM_REQ_CMP_ERR;
			xpt_done(request_ccb);
		}
	} else {
		CAM_DEBUG(path, CAM_DEBUG_INFO,
		    (" Set up the mmcprobe device...\n"));

                status = cam_periph_alloc(mmcprobe_register, NULL,
					  mmcprobe_cleanup,
					  mmcprobe_start,
					  "mmcprobe",
					  CAM_PERIPH_BIO,
					  path, NULL, 0,
					  request_ccb);
                if (status != CAM_REQ_CMP) {
			xpt_print(path,
	"xpt_scan_lun: cam_alloc_periph returned an error, can't continue probe\n");
		}
		request_ccb->ccb_h.status = status;
		xpt_done(request_ccb);
	}

	if (lock)
		xpt_path_unlock(path);
}

static void
mmc_action(union ccb *start_ccb)
{
	CAM_DEBUG(start_ccb->ccb_h.path, CAM_DEBUG_TRACE,
		  ("mmc_action! func_code=%x, action %s\n", start_ccb->ccb_h.func_code,
		   xpt_action_name(start_ccb->ccb_h.func_code)));
	switch (start_ccb->ccb_h.func_code) {
	case XPT_SCAN_BUS:
                /* FALLTHROUGH */
	case XPT_SCAN_TGT:
                /* FALLTHROUGH */
	case XPT_SCAN_LUN:
		CAM_DEBUG(start_ccb->ccb_h.path, CAM_DEBUG_INFO,
			  ("XPT_SCAN_{BUS,TGT,LUN}\n"));
		mmc_scan_lun(start_ccb->ccb_h.path->periph,
			     start_ccb->ccb_h.path, start_ccb->crcn.flags,
			     start_ccb);
		break;

	case XPT_DEV_ADVINFO:
	{
		mmc_dev_advinfo(start_ccb);
		break;
	}

	default:
		xpt_action_default(start_ccb);
		break;
	}
}

static void
mmc_dev_advinfo(union ccb *start_ccb)
{
	struct cam_ed *device;
	struct ccb_dev_advinfo *cdai;
	off_t amt;

	xpt_path_assert(start_ccb->ccb_h.path, MA_OWNED);
	start_ccb->ccb_h.status = CAM_REQ_INVALID;
	device = start_ccb->ccb_h.path->device;
	cdai = &start_ccb->cdai;
	CAM_DEBUG(start_ccb->ccb_h.path, CAM_DEBUG_TRACE,
		  ("%s: request %x\n", __func__, cdai->buftype));

        /* We don't support writing any data */
        if (cdai->flags & CDAI_FLAG_STORE)
                panic("Attempt to store data?!");

	switch(cdai->buftype) {
	case CDAI_TYPE_SCSI_DEVID:
		cdai->provsiz = device->device_id_len;
		if (device->device_id_len == 0)
			break;
		amt = MIN(cdai->provsiz, cdai->bufsiz);
		memcpy(cdai->buf, device->device_id, amt);
		break;
	case CDAI_TYPE_SERIAL_NUM:
		cdai->provsiz = device->serial_num_len;
		if (device->serial_num_len == 0)
			break;
		amt = MIN(cdai->provsiz, cdai->bufsiz);
		memcpy(cdai->buf, device->serial_num, amt);
		break;
        case CDAI_TYPE_PHYS_PATH: /* pass(4) wants this */
                cdai->provsiz = 0;
                break;
	case CDAI_TYPE_MMC_PARAMS:
		cdai->provsiz = sizeof(struct mmc_params);
		amt = MIN(cdai->provsiz, cdai->bufsiz);
		memcpy(cdai->buf, &device->mmc_ident_data, amt);
		break;
	default:
                panic("Unknown buftype");
		return;
	}
	start_ccb->ccb_h.status = CAM_REQ_CMP;
}

static void
mmc_announce_periph_sbuf(struct cam_periph *periph, struct sbuf *sb)
{
	struct	ccb_pathinq cpi;
	struct	ccb_trans_settings cts;
	struct	cam_path *path = periph->path;

	cam_periph_assert(periph, MA_OWNED);

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("mmc_announce_periph_sbuf"));

	memset(&cts, 0, sizeof(cts));
	xpt_setup_ccb(&cts.ccb_h, path, CAM_PRIORITY_NORMAL);
	cts.ccb_h.func_code = XPT_GET_TRAN_SETTINGS;
	cts.type = CTS_TYPE_CURRENT_SETTINGS;
	xpt_action((union ccb*)&cts);
	if ((cts.ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP)
		return;
	xpt_path_inq(&cpi, periph->path);
	CAM_DEBUG(path, CAM_DEBUG_INFO,
	    ("XPT info: CLK %04d, ...\n", cts.proto_specific.mmc.ios.clock));
}

void
mmccam_start_discovery(struct cam_sim *sim)
{
	union ccb *ccb;
	uint32_t pathid;

	pathid = cam_sim_path(sim);
	ccb = xpt_alloc_ccb();

	/*
	 * We create a rescan request for BUS:0:0, since the card
	 * will be at lun 0.
	 */
	if (xpt_create_path(&ccb->ccb_h.path, NULL, pathid,
		/* target */ 0, /* lun */ 0) != CAM_REQ_CMP) {
		xpt_free_ccb(ccb);
		return;
	}

	KASSERT(xpt_path_sim_device(ccb->ccb_h.path) != NULL,
	    ("%s(%s): device is not initialized on sim's path",
	    __func__, cam_sim_name(sim)));
	xpt_rescan(ccb);
}

/* This func is called per attached device :-( */
static void
mmc_print_ident(struct mmc_params *ident_data, struct sbuf *sb)
{
	bool space = false;

	sbuf_printf(sb, "Relative addr: %08x\n", ident_data->card_rca);
	sbuf_cat(sb, "Card features: <");
	if (ident_data->card_features & CARD_FEATURE_MMC) {
		sbuf_cat(sb, "MMC");
		space = true;
	}
	if (ident_data->card_features & CARD_FEATURE_MEMORY) {
		sbuf_printf(sb, "%sMemory", space ? " " : "");
		space = true;
	}
	if (ident_data->card_features & CARD_FEATURE_SDHC) {
		sbuf_printf(sb, "%sHigh-Capacity", space ? " " : "");
		space = true;
	}
	if (ident_data->card_features & CARD_FEATURE_SD20) {
		sbuf_printf(sb, "%sSD2.0-Conditions", space ? " " : "");
		space = true;
	}
	if (ident_data->card_features & CARD_FEATURE_SDIO) {
		sbuf_printf(sb, "%sSDIO", space ? " " : "");
		space = true;
	}
	if (ident_data->card_features & CARD_FEATURE_18V) {
		sbuf_printf(sb, "%s1.8-Signaling", space ? " " : "");
	}
	sbuf_cat(sb, ">\n");

	if (ident_data->card_features & CARD_FEATURE_MEMORY)
		sbuf_printf(sb, "Card memory OCR: %08x\n",
		    ident_data->card_ocr);

	if (ident_data->card_features & CARD_FEATURE_SDIO) {
		sbuf_printf(sb, "Card IO OCR: %08x\n", ident_data->io_ocr);
		sbuf_printf(sb, "Number of functions: %u\n",
		    ident_data->sdio_func_count);
	}
}

static void
mmc_proto_announce_sbuf(struct cam_ed *device, struct sbuf *sb)
{
	mmc_print_ident(&device->mmc_ident_data, sb);
}

static void
mmc_proto_denounce_sbuf(struct cam_ed *device, struct sbuf *sb)
{
	mmc_proto_announce_sbuf(device, sb);
}

static void
mmc_proto_debug_out(union ccb *ccb)
{
	if (ccb->ccb_h.func_code != XPT_MMC_IO)
		return;

	CAM_DEBUG(ccb->ccb_h.path,
	    CAM_DEBUG_CDB,("mmc_proto_debug_out\n"));
}

static periph_init_t probe_periph_init;

static struct periph_driver probe_driver =
{
	probe_periph_init, "mmcprobe",
	TAILQ_HEAD_INITIALIZER(probe_driver.units), /* generation */ 0,
	CAM_PERIPH_DRV_EARLY
};

PERIPHDRIVER_DECLARE(mmcprobe, probe_driver);

#define	CARD_ID_FREQUENCY 400000 /* Spec requires 400kHz max during ID phase. */

static void
probe_periph_init(void)
{
}

static cam_status
mmcprobe_register(struct cam_periph *periph, void *arg)
{
	mmcprobe_softc *softc;
	union ccb *request_ccb;	/* CCB representing the probe request */
	int status;

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("mmcprobe_register\n"));

	request_ccb = (union ccb *)arg;
	if (request_ccb == NULL) {
		printf(
		    "mmcprobe_register: no probe CCB, can't register device\n");
		return(CAM_REQ_CMP_ERR);
	}

	softc = (mmcprobe_softc *)malloc(sizeof(*softc), M_CAMXPT, M_NOWAIT);

	if (softc == NULL) {
		printf(
	"proberegister: Unable to probe new device. Unable to allocate softc\n");
		return(CAM_REQ_CMP_ERR);
	}

	softc->flags = 0;
	softc->acmd41_count = 0;
	periph->softc = softc;
	softc->periph = periph;
	softc->action = PROBE_INVALID;
        softc->restart = 0;
	status = cam_periph_acquire(periph);

        memset(&periph->path->device->mmc_ident_data, 0, sizeof(struct mmc_params));
	if (status != 0) {
		printf(
		    "proberegister: cam_periph_acquire failed (status=%d)\n",
		    status);
		return (CAM_REQ_CMP_ERR);
	}
	CAM_DEBUG(periph->path, CAM_DEBUG_PROBE, ("Probe started\n"));

	if (periph->path->device->flags & CAM_DEV_UNCONFIGURED)
		PROBE_SET_ACTION(softc, PROBE_RESET);
	else
		PROBE_SET_ACTION(softc, PROBE_IDENTIFY);

	/* This will kick the ball */
	xpt_schedule(periph, CAM_PRIORITY_XPT);

	return(CAM_REQ_CMP);
}

static int
mmc_highest_voltage(uint32_t ocr)
{
	int i;

	for (i = MMC_OCR_MAX_VOLTAGE_SHIFT;
	    i >= MMC_OCR_MIN_VOLTAGE_SHIFT; i--)
		if (ocr & (1 << i))
			return (i);
	return (-1);
}

static inline void
init_standard_ccb(union ccb *ccb, uint32_t cmd)
{
	ccb->ccb_h.func_code = cmd;
	ccb->ccb_h.flags = CAM_DIR_OUT;
	ccb->ccb_h.retry_count = 0;
	ccb->ccb_h.timeout = 15 * 1000;
	ccb->ccb_h.cbfcnp = mmcprobe_done;
}

static void
mmcprobe_start(struct cam_periph *periph, union ccb *start_ccb)
{
	mmcprobe_softc *softc;
	struct cam_path *path;
	struct ccb_mmcio *mmcio;
	struct ccb_trans_settings_mmc *cts;

	CAM_DEBUG(start_ccb->ccb_h.path, CAM_DEBUG_PROBE, ("mmcprobe_start\n"));
	softc = (mmcprobe_softc *)periph->softc;
	path = start_ccb->ccb_h.path;
	mmcio = &start_ccb->mmcio;
	cts = &start_ccb->cts.proto_specific.mmc;
	struct mmc_params *mmcp = &path->device->mmc_ident_data;

	memset(&mmcio->cmd, 0, sizeof(struct mmc_command));

	if (softc->restart) {
		softc->restart = 0;
		if (path->device->flags & CAM_DEV_UNCONFIGURED)
			softc->action = PROBE_RESET;
		else
			softc->action = PROBE_IDENTIFY;
	}

	/* Here is the place where the identify fun begins */
	switch (softc->action) {
	case PROBE_RESET:
		CAM_DEBUG(start_ccb->ccb_h.path, CAM_DEBUG_PROBE, ("Start with PROBE_RESET\n"));
		/* FALLTHROUGH */
	case PROBE_IDENTIFY:
		xpt_path_inq(&start_ccb->cpi, periph->path);
		CAM_DEBUG(start_ccb->ccb_h.path, CAM_DEBUG_PROBE, ("Start with PROBE_IDENTIFY\n"));
		init_standard_ccb(start_ccb, XPT_MMC_GET_TRAN_SETTINGS);
		break;

	case PROBE_POWER_OFF:
		CAM_DEBUG(start_ccb->ccb_h.path, CAM_DEBUG_PROBE, ("power off the card\n"));
		init_standard_ccb(start_ccb, XPT_MMC_SET_TRAN_SETTINGS);
		cts->ios.power_mode = power_off;
		cts->ios_valid = MMC_PM;
		break;

	case PROBE_GET_HOST_OCR:
		CAM_DEBUG(start_ccb->ccb_h.path, CAM_DEBUG_PROBE, ("get the host ocr\n"));
		init_standard_ccb(start_ccb, XPT_MMC_GET_TRAN_SETTINGS);
		break;

	case PROBE_RESET_BUS:
	{
		uint32_t host_caps = cts->host_caps;
		if (host_caps & MMC_CAP_SIGNALING_180)
			softc->flags |= PROBE_FLAG_HOST_CAN_DO_18V;
		uint32_t hv = mmc_highest_voltage(softc->host_ocr);
		CAM_DEBUG(start_ccb->ccb_h.path, CAM_DEBUG_PROBE, ("reseting the bus\n"));
		init_standard_ccb(start_ccb, XPT_MMC_SET_TRAN_SETTINGS);
		cts->ios.vdd = hv;
		cts->ios.bus_mode = opendrain;
		cts->ios.chip_select = cs_dontcare;
		cts->ios.power_mode = power_up;
		cts->ios.bus_width = bus_width_1;
		cts->ios.clock = 0;
		cts->ios_valid = MMC_VDD | MMC_PM | MMC_BM |
			MMC_CS | MMC_BW | MMC_CLK;
		break;
	}

	case PROBE_SET_ID_FREQ:
		CAM_DEBUG(start_ccb->ccb_h.path, CAM_DEBUG_PROBE, ("setting the ID freq\n"));
		init_standard_ccb(start_ccb, XPT_MMC_SET_TRAN_SETTINGS);
		cts->ios.power_mode = power_on;
		cts->ios.clock = CARD_ID_FREQUENCY;
		cts->ios.timing = bus_timing_normal;
		cts->ios_valid = MMC_PM | MMC_CLK | MMC_BT;
		break;

	case PROBE_SET_CS:
		/* Begin mmc_idle_cards() */
		init_standard_ccb(start_ccb, XPT_MMC_SET_TRAN_SETTINGS);
		cts->ios.chip_select = cs_high;
		cts->ios_valid = MMC_CS;
		break;

	case PROBE_GO_IDLE_STATE:
		CAM_DEBUG(start_ccb->ccb_h.path, CAM_DEBUG_PROBE, ("Send first XPT_MMC_IO\n"));
		init_standard_ccb(start_ccb, XPT_MMC_IO);
		mmcio->cmd.opcode = MMC_GO_IDLE_STATE; /* CMD 0 */
		mmcio->cmd.arg = 0;
		mmcio->cmd.flags = MMC_RSP_NONE | MMC_CMD_BC;
		mmcio->cmd.data = NULL;
		mmcio->stop.opcode = 0;

		/* XXX Reset I/O portion as well */
		break;

	case PROBE_SDIO_RESET:
		CAM_DEBUG(start_ccb->ccb_h.path, CAM_DEBUG_PROBE,
			  ("Start with PROBE_SDIO_RESET\n"));
		uint32_t mmc_arg = SD_IO_RW_ADR(SD_IO_CCCR_CTL)
			| SD_IO_RW_DAT(CCCR_CTL_RES) | SD_IO_RW_WR | SD_IO_RW_RAW;
		cam_fill_mmcio(&start_ccb->mmcio,
			       /*retries*/ 0,
			       /*cbfcnp*/ mmcprobe_done,
			       /*flags*/ CAM_DIR_NONE,
			       /*mmc_opcode*/ SD_IO_RW_DIRECT,
			       /*mmc_arg*/ mmc_arg,
			       /*mmc_flags*/ MMC_RSP_R5 | MMC_CMD_AC,
			       /*mmc_data*/ NULL,
			       /*timeout*/ 1000);
		break;
	case PROBE_SEND_IF_COND:
		CAM_DEBUG(start_ccb->ccb_h.path, CAM_DEBUG_PROBE,
			  ("Start with PROBE_SEND_IF_COND\n"));
		init_standard_ccb(start_ccb, XPT_MMC_IO);
		mmcio->cmd.opcode = SD_SEND_IF_COND; /* CMD 8 */
		mmcio->cmd.arg = (1 << 8) + 0xAA;
		mmcio->cmd.flags = MMC_RSP_R7 | MMC_CMD_BCR;
		mmcio->stop.opcode = 0;
		break;

	case PROBE_SDIO_INIT:
		CAM_DEBUG(start_ccb->ccb_h.path, CAM_DEBUG_PROBE,
			  ("Start with PROBE_SDIO_INIT\n"));
		init_standard_ccb(start_ccb, XPT_MMC_IO);
		mmcio->cmd.opcode = IO_SEND_OP_COND; /* CMD 5 */
		mmcio->cmd.arg = mmcp->io_ocr;
		mmcio->cmd.flags = MMC_RSP_R4;
		mmcio->stop.opcode = 0;
		break;

	case PROBE_MMC_INIT:
		CAM_DEBUG(start_ccb->ccb_h.path, CAM_DEBUG_PROBE,
			  ("Start with PROBE_MMC_INIT\n"));
		init_standard_ccb(start_ccb, XPT_MMC_IO);
		mmcio->cmd.opcode = MMC_SEND_OP_COND; /* CMD 1 */
		mmcio->cmd.arg = MMC_OCR_CCS | mmcp->card_ocr; /* CCS + ocr */;
		mmcio->cmd.flags = MMC_RSP_R3 | MMC_CMD_BCR;
		mmcio->stop.opcode = 0;
		break;

	case PROBE_SEND_APP_OP_COND:
		init_standard_ccb(start_ccb, XPT_MMC_IO);
		if (softc->flags & PROBE_FLAG_ACMD_SENT) {
			mmcio->cmd.opcode = ACMD_SD_SEND_OP_COND; /* CMD 41 */
			/*
			 * We set CCS bit because we do support SDHC cards.
			 * XXX: Don't set CCS if no response to CMD8.
			 */
			uint32_t cmd_arg = MMC_OCR_CCS | mmcp->card_ocr; /* CCS + ocr */
			if (softc->acmd41_count < 10 && mmcp->card_ocr != 0 )
				cmd_arg |= MMC_OCR_S18R;
			mmcio->cmd.arg = cmd_arg;
			mmcio->cmd.flags = MMC_RSP_R3 | MMC_CMD_BCR;
			softc->acmd41_count++;
		} else {
			mmcio->cmd.opcode = MMC_APP_CMD; /* CMD 55 */
			mmcio->cmd.arg = 0; /* rca << 16 */
			mmcio->cmd.flags = MMC_RSP_R1 | MMC_CMD_AC;
		}
		mmcio->stop.opcode = 0;
		break;

	case PROBE_GET_CID: /* XXX move to mmc_da */
		init_standard_ccb(start_ccb, XPT_MMC_IO);
		mmcio->cmd.opcode = MMC_ALL_SEND_CID;
		mmcio->cmd.arg = 0;
		mmcio->cmd.flags = MMC_RSP_R2 | MMC_CMD_BCR;
		mmcio->stop.opcode = 0;
		break;
	case PROBE_SEND_RELATIVE_ADDR:
		init_standard_ccb(start_ccb, XPT_MMC_IO);
		mmcio->cmd.opcode = SD_SEND_RELATIVE_ADDR;
		mmcio->cmd.arg = 0;
		mmcio->cmd.flags = MMC_RSP_R6 | MMC_CMD_BCR;
		mmcio->stop.opcode = 0;
		break;
	case PROBE_MMC_SET_RELATIVE_ADDR:
		init_standard_ccb(start_ccb, XPT_MMC_IO);
		mmcio->cmd.opcode = MMC_SET_RELATIVE_ADDR;
		mmcio->cmd.arg = MMC_PROPOSED_RCA << 16;
		mmcio->cmd.flags = MMC_RSP_R1 | MMC_CMD_AC;
		mmcio->stop.opcode = 0;
		break;
	case PROBE_SELECT_CARD:
		init_standard_ccb(start_ccb, XPT_MMC_IO);
		mmcio->cmd.opcode = MMC_SELECT_CARD;
		mmcio->cmd.arg = (uint32_t)path->device->mmc_ident_data.card_rca << 16;
		mmcio->cmd.flags = MMC_RSP_R1B | MMC_CMD_AC;
		mmcio->stop.opcode = 0;
		break;
	case PROBE_GET_CSD: /* XXX move to mmc_da */
		init_standard_ccb(start_ccb, XPT_MMC_IO);
		mmcio->cmd.opcode = MMC_SEND_CSD;
		mmcio->cmd.arg = (uint32_t)path->device->mmc_ident_data.card_rca << 16;
		mmcio->cmd.flags = MMC_RSP_R2 | MMC_CMD_BCR;
		mmcio->stop.opcode = 0;
		break;
	case PROBE_DONE:
		CAM_DEBUG(start_ccb->ccb_h.path, CAM_DEBUG_PROBE, ("Start with PROBE_DONE\n"));
		init_standard_ccb(start_ccb, XPT_SET_TRAN_SETTINGS);
		cts->ios.bus_mode = pushpull;
		cts->ios_valid = MMC_BM;
		xpt_action(start_ccb);
		return;
		/* NOTREACHED */
		break;
	case PROBE_INVALID:
		break;
	default:
		CAM_DEBUG(start_ccb->ccb_h.path, CAM_DEBUG_PROBE, ("probestart: invalid action state 0x%x\n", softc->action));
		panic("default: case in mmc_probe_start()");
	}

	start_ccb->ccb_h.flags |= CAM_DEV_QFREEZE;
	xpt_action(start_ccb);
}

static void mmcprobe_cleanup(struct cam_periph *periph)
{
	free(periph->softc, M_CAMXPT);
}

static void
mmcprobe_done(struct cam_periph *periph, union ccb *done_ccb)
{
	mmcprobe_softc *softc;
	struct cam_path *path;

	int err;
	struct ccb_mmcio *mmcio;
	uint32_t  priority;

	CAM_DEBUG(done_ccb->ccb_h.path, CAM_DEBUG_PROBE, ("mmcprobe_done\n"));
	softc = (mmcprobe_softc *)periph->softc;
	path = done_ccb->ccb_h.path;
	priority = done_ccb->ccb_h.pinfo.priority;

	switch (softc->action) {
	case PROBE_RESET:
		/* FALLTHROUGH */
	case PROBE_IDENTIFY:
	{
		CAM_DEBUG(done_ccb->ccb_h.path, CAM_DEBUG_PROBE, ("done with PROBE_RESET\n"));
		PROBE_SET_ACTION(softc, PROBE_POWER_OFF);
		break;
	}
	case PROBE_POWER_OFF:
	{
		CAM_DEBUG(done_ccb->ccb_h.path, CAM_DEBUG_PROBE, ("done with PROBE_POWER_OFF\n"));
		PROBE_SET_ACTION(softc, PROBE_GET_HOST_OCR);
		break;
	}
	case PROBE_GET_HOST_OCR:
	{
		struct ccb_trans_settings_mmc *cts;
		cts = &done_ccb->cts.proto_specific.mmc;
		softc->host_ocr = cts->host_ocr;
		CAM_DEBUG(done_ccb->ccb_h.path, CAM_DEBUG_PROBE, ("done with PROBE_GET_HOST_OCR (Got OCR=%x\n", softc->host_ocr));
		PROBE_SET_ACTION(softc, PROBE_RESET_BUS);
		break;
	}
	case PROBE_RESET_BUS:
	{
		CAM_DEBUG(done_ccb->ccb_h.path, CAM_DEBUG_PROBE, ("done with PROBE_RESET_BUS\n"));
		PROBE_SET_ACTION(softc, PROBE_SET_ID_FREQ);
		break;
	}
	case PROBE_SET_ID_FREQ:
	{
		CAM_DEBUG(done_ccb->ccb_h.path, CAM_DEBUG_PROBE, ("done with PROBE_SET_ID_FREQ\n"));
		PROBE_SET_ACTION(softc, PROBE_SET_CS);
		break;
	}
	case PROBE_SET_CS:
	{
		CAM_DEBUG(done_ccb->ccb_h.path, CAM_DEBUG_PROBE, ("done with PROBE_SET_CS\n"));
		PROBE_SET_ACTION(softc, PROBE_GO_IDLE_STATE);
		break;
	}
	case PROBE_GO_IDLE_STATE:
	{
		CAM_DEBUG(done_ccb->ccb_h.path, CAM_DEBUG_PROBE, ("done with PROBE_GO_IDLE_STATE\n"));
		mmcio = &done_ccb->mmcio;
		err = mmcio->cmd.error;

		if (err != MMC_ERR_NONE) {
			CAM_DEBUG(done_ccb->ccb_h.path, CAM_DEBUG_PROBE,
				  ("GO_IDLE_STATE failed with error %d\n",
				   err));

			/* There was a device there, but now it's gone... */
			if ((path->device->flags & CAM_DEV_UNCONFIGURED) == 0) {
				CAM_DEBUG(done_ccb->ccb_h.path, CAM_DEBUG_PROBE,
				  ("Device lost!\n"));

				xpt_async(AC_LOST_DEVICE, path, NULL);
			}
			PROBE_SET_ACTION(softc, PROBE_INVALID);
			break;
		}
		path->device->protocol = PROTO_MMCSD;
		PROBE_SET_ACTION(softc, PROBE_SEND_IF_COND);
		break;
	}
	case PROBE_SEND_IF_COND:
	{
		mmcio = &done_ccb->mmcio;
		err = mmcio->cmd.error;
		struct mmc_params *mmcp = &path->device->mmc_ident_data;

		if (err != MMC_ERR_NONE || mmcio->cmd.resp[0] != 0x1AA) {
			CAM_DEBUG(done_ccb->ccb_h.path, CAM_DEBUG_PROBE,
				  ("IF_COND: error %d, pattern %08x\n",
				   err, mmcio->cmd.resp[0]));
		} else {
			mmcp->card_features |= CARD_FEATURE_SD20;
			CAM_DEBUG(done_ccb->ccb_h.path, CAM_DEBUG_PROBE,
				  ("SD 2.0 interface conditions: OK\n"));
		}
                PROBE_SET_ACTION(softc, PROBE_SDIO_RESET);
		break;
	}
	case PROBE_SDIO_RESET:
	{
		mmcio = &done_ccb->mmcio;
		err = mmcio->cmd.error;

		CAM_DEBUG(done_ccb->ccb_h.path, CAM_DEBUG_PROBE,
		    ("SDIO_RESET: error %d, CCCR CTL register: %08x\n",
		    err, mmcio->cmd.resp[0]));
		PROBE_SET_ACTION(softc, PROBE_SDIO_INIT);
		break;
	}
	case PROBE_SDIO_INIT:
	{
		mmcio = &done_ccb->mmcio;
		err = mmcio->cmd.error;
		struct mmc_params *mmcp = &path->device->mmc_ident_data;

		CAM_DEBUG(done_ccb->ccb_h.path, CAM_DEBUG_PROBE,
		    ("SDIO_INIT: error %d, %08x %08x %08x %08x\n",
		    err, mmcio->cmd.resp[0],
		    mmcio->cmd.resp[1],
		    mmcio->cmd.resp[2],
		    mmcio->cmd.resp[3]));

		/*
		 * Error here means that this card is not SDIO,
		 * so proceed with memory init as if nothing has happened
		 */
		if (err != MMC_ERR_NONE) {
			PROBE_SET_ACTION(softc, PROBE_SEND_APP_OP_COND);
			break;
		}
		mmcp->card_features |= CARD_FEATURE_SDIO;
		uint32_t ioifcond = mmcio->cmd.resp[0];
		uint32_t io_ocr = ioifcond & R4_IO_OCR_MASK;

		mmcp->sdio_func_count = R4_IO_NUM_FUNCTIONS(ioifcond);
		CAM_DEBUG(done_ccb->ccb_h.path, CAM_DEBUG_PROBE,
		    ("SDIO card: %d functions\n", mmcp->sdio_func_count));
		if (io_ocr == 0) {
			CAM_DEBUG(done_ccb->ccb_h.path, CAM_DEBUG_PROBE,
			  ("SDIO OCR invalid, retrying\n"));
			break; /* Retry */
		}

		if (io_ocr != 0 && mmcp->io_ocr == 0) {
			mmcp->io_ocr = io_ocr;
			break; /* Retry, this time with non-0 OCR */
		}
		CAM_DEBUG(done_ccb->ccb_h.path, CAM_DEBUG_PROBE,
		    ("SDIO OCR: %08x\n", mmcp->io_ocr));

		if (ioifcond & R4_IO_MEM_PRESENT) {
			/* Combo card -- proceed to memory initialization */
			PROBE_SET_ACTION(softc, PROBE_SEND_APP_OP_COND);
		} else {
			/* No memory portion -- get RCA and select card */
			PROBE_SET_ACTION(softc, PROBE_SEND_RELATIVE_ADDR);
		}
		break;
	}
	case PROBE_MMC_INIT:
	{
		mmcio = &done_ccb->mmcio;
		err = mmcio->cmd.error;
		struct mmc_params *mmcp = &path->device->mmc_ident_data;

		if (err != MMC_ERR_NONE) {
			CAM_DEBUG(done_ccb->ccb_h.path, CAM_DEBUG_PROBE,
				  ("MMC_INIT: error %d, resp %08x\n",
				   err, mmcio->cmd.resp[0]));
			PROBE_SET_ACTION(softc, PROBE_INVALID);
			break;
		}
		CAM_DEBUG(done_ccb->ccb_h.path, CAM_DEBUG_PROBE,
		    ("MMC card, OCR %08x\n", mmcio->cmd.resp[0]));

		if (mmcp->card_ocr == 0) {
			/* We haven't sent the OCR to the card yet -- do it */
			mmcp->card_ocr = mmcio->cmd.resp[0];
			CAM_DEBUG(done_ccb->ccb_h.path, CAM_DEBUG_PROBE,
			    ("-> sending OCR to card\n"));
			break;
		}

		if (!(mmcio->cmd.resp[0] & MMC_OCR_CARD_BUSY)) {
			CAM_DEBUG(done_ccb->ccb_h.path, CAM_DEBUG_PROBE,
			    ("Card is still powering up\n"));
			break;
		}

		mmcp->card_features |= CARD_FEATURE_MMC | CARD_FEATURE_MEMORY;
		PROBE_SET_ACTION(softc, PROBE_GET_CID);
		break;
	}
	case PROBE_SEND_APP_OP_COND:
	{
		mmcio = &done_ccb->mmcio;
		err = mmcio->cmd.error;

		if (err != MMC_ERR_NONE) {
			CAM_DEBUG(done_ccb->ccb_h.path, CAM_DEBUG_PROBE,
				  ("APP_OP_COND: error %d, resp %08x\n",
				   err, mmcio->cmd.resp[0]));
			PROBE_SET_ACTION(softc, PROBE_MMC_INIT);
			break;
		}

		if (!(softc->flags & PROBE_FLAG_ACMD_SENT)) {
			/* Don't change the state */
			softc->flags |= PROBE_FLAG_ACMD_SENT;
			break;
		}

		softc->flags &= ~PROBE_FLAG_ACMD_SENT;
		if ((mmcio->cmd.resp[0] & MMC_OCR_CARD_BUSY) ||
		    (mmcio->cmd.arg & MMC_OCR_VOLTAGE) == 0) {
			struct mmc_params *mmcp = &path->device->mmc_ident_data;
			CAM_DEBUG(done_ccb->ccb_h.path, CAM_DEBUG_PROBE,
			    ("Card OCR: %08x\n",  mmcio->cmd.resp[0]));
			if (mmcp->card_ocr == 0) {
				mmcp->card_ocr = mmcio->cmd.resp[0];
				/* Now when we know OCR that we want -- send it to card */
				CAM_DEBUG(done_ccb->ccb_h.path, CAM_DEBUG_PROBE,
				    ("-> sending OCR to card\n"));
			} else {
				/* We already know the OCR and despite of that we
				 * are processing the answer to ACMD41 -> move on
				 */
				PROBE_SET_ACTION(softc, PROBE_GET_CID);
			}
			/* Getting an answer to ACMD41 means the card has memory */
			mmcp->card_features |= CARD_FEATURE_MEMORY;

			/* Standard capacity vs High Capacity memory card */
			if (mmcio->cmd.resp[0] & MMC_OCR_CCS) {
				CAM_DEBUG(done_ccb->ccb_h.path, CAM_DEBUG_PROBE,
				    ("Card is SDHC\n"));
				mmcp->card_features |= CARD_FEATURE_SDHC;
			}

			/* Whether the card supports 1.8V signaling */
			if (mmcio->cmd.resp[0] & MMC_OCR_S18A) {
				CAM_DEBUG(done_ccb->ccb_h.path, CAM_DEBUG_PROBE,
					  ("Card supports 1.8V signaling\n"));
				mmcp->card_features |= CARD_FEATURE_18V;
				if (softc->flags & PROBE_FLAG_HOST_CAN_DO_18V) {
					CAM_DEBUG(done_ccb->ccb_h.path, CAM_DEBUG_PROBE,
						  ("Host supports 1.8V signaling. Switch voltage!\n"));
					done_ccb->ccb_h.func_code = XPT_SET_TRAN_SETTINGS;
					done_ccb->ccb_h.flags = CAM_DIR_NONE;
					done_ccb->ccb_h.retry_count = 0;
					done_ccb->ccb_h.timeout = 100;
					done_ccb->ccb_h.cbfcnp = NULL;
					done_ccb->cts.proto_specific.mmc.ios.vccq = vccq_180;
					done_ccb->cts.proto_specific.mmc.ios_valid = MMC_VCCQ;
					xpt_action(done_ccb);
				}
			}
		} else {
			CAM_DEBUG(done_ccb->ccb_h.path, CAM_DEBUG_PROBE,
				  ("Card not ready: %08x\n",  mmcio->cmd.resp[0]));
			/* Send CMD55+ACMD41 once again  */
			PROBE_SET_ACTION(softc, PROBE_SEND_APP_OP_COND);
		}

		break;
	}
	case PROBE_GET_CID: /* XXX move to mmc_da */
	{
		mmcio = &done_ccb->mmcio;
		err = mmcio->cmd.error;

		if (err != MMC_ERR_NONE) {
			CAM_DEBUG(done_ccb->ccb_h.path, CAM_DEBUG_PROBE,
				  ("PROBE_GET_CID: error %d\n", err));
			PROBE_SET_ACTION(softc, PROBE_INVALID);
			break;
		}

		struct mmc_params *mmcp = &path->device->mmc_ident_data;
		memcpy(mmcp->card_cid, mmcio->cmd.resp, 4 * sizeof(uint32_t));
		CAM_DEBUG(done_ccb->ccb_h.path, CAM_DEBUG_PROBE,
		    ("CID %08x%08x%08x%08x\n",
		      mmcp->card_cid[0],
		      mmcp->card_cid[1],
		      mmcp->card_cid[2],
		      mmcp->card_cid[3]));
		if (mmcp->card_features & CARD_FEATURE_MMC)
			PROBE_SET_ACTION(softc, PROBE_MMC_SET_RELATIVE_ADDR);
		else
			PROBE_SET_ACTION(softc, PROBE_SEND_RELATIVE_ADDR);
		break;
	}
	case PROBE_SEND_RELATIVE_ADDR: {
		mmcio = &done_ccb->mmcio;
		err = mmcio->cmd.error;
		struct mmc_params *mmcp = &path->device->mmc_ident_data;
		uint16_t rca = mmcio->cmd.resp[0] >> 16;
		CAM_DEBUG(done_ccb->ccb_h.path, CAM_DEBUG_PROBE,
		    ("Card published RCA: %u\n", rca));
		path->device->mmc_ident_data.card_rca = rca;
		if (err != MMC_ERR_NONE) {
			CAM_DEBUG(done_ccb->ccb_h.path, CAM_DEBUG_PROBE,
				  ("PROBE_SEND_RELATIVE_ADDR: error %d\n", err));
			PROBE_SET_ACTION(softc, PROBE_INVALID);
			break;
		}

		/* If memory is present, get CSD, otherwise select card */
		if (mmcp->card_features & CARD_FEATURE_MEMORY)
			PROBE_SET_ACTION(softc, PROBE_GET_CSD);
		else
			PROBE_SET_ACTION(softc, PROBE_SELECT_CARD);
		break;
	}
	case PROBE_MMC_SET_RELATIVE_ADDR:
		mmcio = &done_ccb->mmcio;
		err = mmcio->cmd.error;
		if (err != MMC_ERR_NONE) {
			CAM_DEBUG(done_ccb->ccb_h.path, CAM_DEBUG_PROBE,
			    ("PROBE_MMC_SET_RELATIVE_ADDR: error %d\n", err));
			PROBE_SET_ACTION(softc, PROBE_INVALID);
			break;
		}
		path->device->mmc_ident_data.card_rca = MMC_PROPOSED_RCA;
		PROBE_SET_ACTION(softc, PROBE_GET_CSD);
		break;
	case PROBE_GET_CSD: {
		mmcio = &done_ccb->mmcio;
		err = mmcio->cmd.error;

		if (err != MMC_ERR_NONE) {
			CAM_DEBUG(done_ccb->ccb_h.path, CAM_DEBUG_PROBE,
				  ("PROBE_GET_CSD: error %d\n", err));
			PROBE_SET_ACTION(softc, PROBE_INVALID);
			break;
		}

		struct mmc_params *mmcp = &path->device->mmc_ident_data;
		memcpy(mmcp->card_csd, mmcio->cmd.resp, 4 * sizeof(uint32_t));
		CAM_DEBUG(done_ccb->ccb_h.path, CAM_DEBUG_PROBE,
		    ("CSD %08x%08x%08x%08x\n",
		      mmcp->card_csd[0],
		      mmcp->card_csd[1],
		      mmcp->card_csd[2],
		      mmcp->card_csd[3]));
		PROBE_SET_ACTION(softc, PROBE_SELECT_CARD);
		break;
	}
	case PROBE_SELECT_CARD: {
		mmcio = &done_ccb->mmcio;
		err = mmcio->cmd.error;
		if (err != MMC_ERR_NONE) {
			CAM_DEBUG(done_ccb->ccb_h.path, CAM_DEBUG_PROBE,
				  ("PROBE_SEND_RELATIVE_ADDR: error %d\n", err));
			PROBE_SET_ACTION(softc, PROBE_INVALID);
			break;
		}

		PROBE_SET_ACTION(softc, PROBE_DONE);
		break;
	}
	default:
		CAM_DEBUG(done_ccb->ccb_h.path, CAM_DEBUG_PROBE,
			  ("mmcprobe_done: invalid action state 0x%x\n", softc->action));
		panic("default: case in mmc_probe_done()");
	}

	if (softc->action == PROBE_INVALID &&
	  (path->device->flags & CAM_DEV_UNCONFIGURED) == 0) {
		xpt_async(AC_LOST_DEVICE, path, NULL);
	}

	if (softc->action != PROBE_INVALID)
		xpt_schedule(periph, priority);
	/* Drop freeze taken due to CAM_DEV_QFREEZE flag set. */
	int frozen = cam_release_devq(path, 0, 0, 0, FALSE);
	CAM_DEBUG(done_ccb->ccb_h.path, CAM_DEBUG_PROBE,
	  ("mmcprobe_done: remaining freeze count %d\n", frozen));

	if (softc->action == PROBE_DONE) {
                /* Notify the system that the device is found! */
		if (periph->path->device->flags & CAM_DEV_UNCONFIGURED) {
			path->device->flags &= ~CAM_DEV_UNCONFIGURED;
			xpt_acquire_device(path->device);
			done_ccb->ccb_h.func_code = XPT_GDEV_TYPE;
			xpt_action(done_ccb);
			xpt_async(AC_FOUND_DEVICE, path, done_ccb);
		}
	}
	xpt_release_ccb(done_ccb);
	if (softc->action == PROBE_DONE || softc->action == PROBE_INVALID) {
		cam_periph_invalidate(periph);
		cam_periph_release_locked(periph);
	}
}

void
mmc_path_inq(struct ccb_pathinq *cpi, const char *hba,
    const struct cam_sim *sim, size_t maxio)
{

	cpi->version_num = 1;
	cpi->hba_inquiry = 0;
	cpi->target_sprt = 0;
	cpi->hba_misc = PIM_NOBUSRESET | PIM_SEQSCAN;
	cpi->hba_eng_cnt = 0;
	cpi->max_target = 0;
	cpi->max_lun = 0;
	cpi->initiator_id = 1;
	cpi->maxio = maxio;
	strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
	strncpy(cpi->hba_vid, hba, HBA_IDLEN);
	strncpy(cpi->dev_name, cam_sim_name(sim), DEV_IDLEN);
	cpi->unit_number = cam_sim_unit(sim);
	cpi->bus_id = cam_sim_bus(sim);
	cpi->protocol = PROTO_MMCSD;
	cpi->protocol_version = SCSI_REV_0;
	cpi->transport = XPORT_MMCSD;
	cpi->transport_version = 1;

	cpi->base_transfer_speed = 100; /* XXX WTF? */

	cpi->ccb_h.status = CAM_REQ_CMP;
}
