/*
 * Copyright (c) 2026 Abdelkader Boudih <freebsd@seuros.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * fwcam(4) - IIDC 1394-based Digital Camera driver
 *
 * Implements the IIDC v1.30 specification (TA Document 1999023) for
 * FireWire digital cameras.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>
#include <sys/mbuf.h>
#include <sys/videoio.h>

#include <dev/firewire/firewire.h>
#include <dev/firewire/firewirereg.h>
#include <dev/firewire/iec13213.h>
#include <dev/firewire/fwcam.h>
#include <dev/firewire/fw_helpers.h>

#include <dev/video/video.h>

#include "video_if.h"

static MALLOC_DEFINE(M_FWCAM, "fwcam", "IIDC FireWire Camera");

static int debug = 0;
static int iso_channel = 0;
SYSCTL_DECL(_hw_firewire);
static SYSCTL_NODE(_hw_firewire, OID_AUTO, fwcam, CTLFLAG_RD | CTLFLAG_MPSAFE,
    0, "IIDC Camera");
SYSCTL_INT(_hw_firewire_fwcam, OID_AUTO, debug, CTLFLAG_RWTUN, &debug, 0,
    "fwcam debug level");
SYSCTL_INT(_hw_firewire_fwcam, OID_AUTO, iso_channel, CTLFLAG_RWTUN,
    &iso_channel, 0, "ISO channel for isochronous receive (default 0)");

#define FWCAM_DEBUG(lev, fmt, ...)					\
	do {								\
		if (debug >= (lev))					\
			printf("fwcam: " fmt, ## __VA_ARGS__);		\
	} while (0)

static int	fwcam_probe(device_t);
static int	fwcam_attach(device_t);
static int	fwcam_detach(device_t);
static void	fwcam_probe_task(void *, int);
static int	fwcam_iso_start(struct fwcam_softc *);
static void	fwcam_iso_stop(struct fwcam_softc *);
static void	fwcam_iso_input(struct fw_xferq *);
static void	fwcam_frame_done(struct fwcam_softc *);

static int	fwcam_hw_open(device_t);
static int	fwcam_hw_querycap(device_t, struct video_caps *);
static int	fwcam_hw_enum_format(device_t, uint32_t, struct video_format *);
static int	fwcam_hw_get_format(device_t, struct video_format *);
static int	fwcam_hw_try_format(device_t, struct video_format *);
static int	fwcam_hw_set_format(device_t, const struct video_format *);
static int	fwcam_hw_enum_framesizes(device_t, struct video_frmsizeenum *);
static int	fwcam_hw_enum_input(device_t, uint32_t, struct video_input *);
static int	fwcam_hw_get_input(device_t, uint32_t *);
static int	fwcam_hw_set_input(device_t, uint32_t);
static int	fwcam_hw_query_control(device_t, struct video_control_desc *);
static int	fwcam_hw_get_control(device_t, struct video_control *);
static int	fwcam_hw_set_control(device_t, const struct video_control *);
static int	fwcam_hw_start_stream(device_t);
static void	fwcam_hw_stop_stream(device_t);

/*
 * Format_0 (VGA) mode descriptors for V4L2 mapping.
 */
struct fwcam_v4l2_mode {
	uint32_t	pixelformat;	/* V4L2 fourcc */
	uint32_t	width;
	uint32_t	height;
	uint32_t	bytesperline;
	uint32_t	sizeimage;
};

static const struct fwcam_v4l2_mode fwcam_fmt0_v4l2[] = {
	/* mode 0: 160x120 YUV444 (24bpp packed) */
	{ V4L2_PIX_FMT_YUV444, 160, 120, 160 * 3, 160 * 120 * 3 },
	/* mode 1: 320x240 YUV422 (UYVY, 16bpp packed) */
	{ V4L2_PIX_FMT_UYVY,   320, 240, 320 * 2, 320 * 240 * 2 },
	/* mode 2: 640x480 YUV411 (12bpp) */
	{ V4L2_PIX_FMT_Y41P,   640, 480, 640 * 3 / 2, 640 * 480 * 3 / 2 },
	/* mode 3: 640x480 YUV422 (UYVY, 16bpp packed) */
	{ V4L2_PIX_FMT_UYVY,   640, 480, 640 * 2, 640 * 480 * 2 },
	/* mode 4: 640x480 RGB8 (24bpp) */
	{ V4L2_PIX_FMT_RGB24,  640, 480, 640 * 3, 640 * 480 * 3 },
	/* mode 5: 640x480 Mono8 */
	{ V4L2_PIX_FMT_GREY,   640, 480, 640, 640 * 480 },
	/* mode 6: 640x480 Mono16 */
	{ V4L2_PIX_FMT_Y16,    640, 480, 640 * 2, 640 * 480 * 2 },
};

#define	FWCAM_FMT0_V4L2_NMODES	nitems(fwcam_fmt0_v4l2)

/*
 * Search a CSR directory for the IIDC command base register (key 0x40).
 * The iSight places cmd_base inside a logical_unit_directory nested
 * within the IIDC unit directory, so we recurse into sub-directories
 * up to max_depth levels.
 */
static uint32_t
fwcam_search_dir_cmd_base(uint32_t *csrrom, uint32_t dir_qoff,
    uint32_t rom_quads, int max_depth)
{
	struct csrdirectory *dir;
	struct csrreg *reg;
	uint32_t dir_len, sub_qoff, val;
	int i;

	if (dir_qoff >= rom_quads || max_depth <= 0)
		return (0);
	dir = (struct csrdirectory *)&csrrom[dir_qoff];
	dir_len = dir->crc_len;
	if (dir_qoff + 1 + dir_len > rom_quads)
		return (0);
	if (!crom_crc_valid((uint32_t *)&dir->entry[0], dir_len, dir->crc)) {
		if (firewire_debug)
			printf("fwcam: bad CRC in directory at 0x%x\n",
			    dir_qoff);
	}

	for (i = 0; i < (int)dir_len; i++) {
		if (dir_qoff + 1 + i >= rom_quads)
			break;
		reg = &dir->entry[i];
		if (reg->key == IIDC_CROM_CMD_BASE && reg->val != 0)
			return (reg->val);
		if ((reg->key & CSRTYPE_MASK) == CSRTYPE_D) {
			sub_qoff = dir_qoff + 1 + i + reg->val;
			val = fwcam_search_dir_cmd_base(csrrom, sub_qoff,
			    rom_quads, max_depth - 1);
			if (val != 0)
				return (val);
		}
	}
	return (0);
}

/*
 * Extract IIDC command base register from the unit directory.
 * Searches the unit directory and any sub-directories (the iSight
 * places cmd_base inside a logical_unit_directory).
 */
static uint32_t
fwcam_find_iidc_cmd_base(struct fw_unit *unit)
{

	return (fwcam_search_dir_cmd_base(unit->fwdev->csrrom,
	    unit->dir_offset / 4, CSRROMSIZE / 4, 2));
}

static int
fwcam_read_quadlet(struct fwcam_softc *sc, uint32_t offset, uint32_t *val)
{
	uint16_t dst;
	uint8_t spd;
	int err;

	FWCAM_LOCK(sc);
	if (sc->fwdev == NULL) {
		FWCAM_UNLOCK(sc);
		return (ENXIO);
	}
	dst = FWLOCALBUS | sc->fwdev->dst;
	spd = min(sc->fwdev->speed, FWSPD_S400);
	FWCAM_UNLOCK(sc);

	err = fw_read_quadlet(sc->fd.fc, M_FWCAM, dst, spd,
	    sc->cmd_hi, sc->cmd_lo + offset, val);
	if (err)
		FWCAM_DEBUG(1, "read_quadlet: offset=0x%x err=%d\n",
		    offset, err);
	return (err);
}

static int
fwcam_write_quadlet(struct fwcam_softc *sc, uint32_t offset, uint32_t val)
{
	uint16_t dst;
	uint8_t spd;
	int err;

	FWCAM_LOCK(sc);
	if (sc->fwdev == NULL) {
		FWCAM_UNLOCK(sc);
		return (ENXIO);
	}
	dst = FWLOCALBUS | sc->fwdev->dst;
	spd = min(sc->fwdev->speed, FWSPD_S400);
	FWCAM_UNLOCK(sc);

	err = fw_write_quadlet(sc->fd.fc, M_FWCAM, dst, spd,
	    sc->cmd_hi, sc->cmd_lo + offset, val);
	if (err)
		FWCAM_DEBUG(1, "write_quadlet: offset=0x%x err=%d\n",
		    offset, err);
	return (err);
}

static int
fwcam_read_capabilities(struct fwcam_softc *sc)
{
	int err, f, m;

	err = fwcam_read_quadlet(sc, IIDC_V_FORMAT_INQ, &sc->formats);
	if (err) {
		device_printf(sc->fd.dev,
		    "failed to read V_FORMAT_INQ: %d\n", err);
		return (err);
	}

	err = fwcam_read_quadlet(sc, IIDC_BASIC_FUNC_INQ, &sc->basic_func);
	if (err) {
		device_printf(sc->fd.dev,
		    "failed to read BASIC_FUNC_INQ: %d\n", err);
		return (err);
	}

	/* Read mode and rate inquiry registers for each supported format */
	for (f = 0; f < 8; f++) {
		if (!(sc->formats & (1 << (31 - f)))) {
			sc->modes[f] = 0;
			continue;
		}
		err = fwcam_read_quadlet(sc, IIDC_V_MODE_INQ(f),
		    &sc->modes[f]);
		if (err) {
			sc->modes[f] = 0;
			continue;
		}
		for (m = 0; m < 8; m++) {
			if (!(sc->modes[f] & (1 << (31 - m)))) {
				sc->rates[f][m] = 0;
				continue;
			}
			err = fwcam_read_quadlet(sc, IIDC_V_RATE_INQ(f, m),
			    &sc->rates[f][m]);
			if (err)
				sc->rates[f][m] = 0;
		}
	}

	err = fwcam_read_quadlet(sc, IIDC_FEATURE_HI_INQ, &sc->features_hi);
	if (err)
		sc->features_hi = 0;

	err = fwcam_read_quadlet(sc, IIDC_FEATURE_LO_INQ, &sc->features_lo);
	if (err)
		sc->features_lo = 0;

	return (0);
}

static int
fwcam_power_on(struct fwcam_softc *sc)
{
	uint32_t val;
	int err, retries;

	err = fwcam_read_quadlet(sc, IIDC_BASIC_FUNC_INQ, &val);
	if (err) {
		device_printf(sc->fd.dev,
		    "cannot read BASIC_FUNC_INQ: %d\n", err);
		return (err);
	}

	if ((val & IIDC_CAM_POWER_CTRL) == 0) {
		FWCAM_DEBUG(1, "no power control, assuming powered\n");
		return (0);
	}

	err = fwcam_read_quadlet(sc, IIDC_CAMERA_POWER, &val);
	if (err == 0 && (val & IIDC_POWER_ON)) {
		FWCAM_DEBUG(1, "camera already powered on\n");
		return (0);
	}

	err = fwcam_write_quadlet(sc, IIDC_CAMERA_POWER, IIDC_POWER_ON);
	if (err) {
		device_printf(sc->fd.dev,
		    "failed to write CAMERA_POWER: %d\n", err);
		return (err);
	}

	for (retries = 0; retries < 50; retries++) {
		pause("fwcampw", hz / 10);

		if (sc->state == FWCAM_STATE_DETACHING)
			return (ENXIO);

		err = fwcam_read_quadlet(sc, IIDC_CAMERA_POWER, &val);
		if (err)
			continue;	/* read may fail while powering up */

		if (val & IIDC_POWER_ON)
			return (0);
	}

	device_printf(sc->fd.dev, "camera power-on timeout (5s)\n");
	return (ETIMEDOUT);
}

static void
fwcam_probe_task(void *arg, int pending __unused)
{
	struct fwcam_softc *sc = (struct fwcam_softc *)arg;
	int err;

	if (sc->state == FWCAM_STATE_DETACHING || sc->fwdev == NULL)
		return;

	err = fwcam_power_on(sc);
	if (err) {
		device_printf(sc->fd.dev,
		    "power-on failed (%d), trying to read anyway\n", err);
	}

	if (sc->state == FWCAM_STATE_DETACHING)
		return;

	if (fwcam_read_capabilities(sc) == 0) {
		uint32_t val;

		if (fwcam_read_quadlet(sc, IIDC_CUR_V_FORMAT, &val) == 0)
			sc->cur_format = (val >> 29) & 0x7;
		if (fwcam_read_quadlet(sc, IIDC_CUR_V_MODE, &val) == 0)
			sc->cur_mode = (val >> 29) & 0x7;
		if (fwcam_read_quadlet(sc, IIDC_CUR_V_FRM_RATE, &val) == 0)
			sc->cur_framerate = (val >> 29) & 0x7;

		FWCAM_LOCK(sc);
		if (sc->state == FWCAM_STATE_DETACHING) {
			FWCAM_UNLOCK(sc);
			return;
		}
		sc->state = FWCAM_STATE_PROBED;
		wakeup(sc);
		FWCAM_UNLOCK(sc);

		if (sc->dma_ch >= 0 &&
		    sc->state != FWCAM_STATE_DETACHING)
			fwcam_iso_start(sc);
	} else {
		FWCAM_LOCK(sc);
		if (sc->state == FWCAM_STATE_PROBING)
			sc->state = FWCAM_STATE_IDLE;
		wakeup(sc);
		FWCAM_UNLOCK(sc);
	}
}

/*
 * Compute expected frame size for current format/mode.
 * Format_0 (VGA) modes:
 *   Mode 0: 160x120 YUV444  = 160*120*3 = 57600
 *   Mode 1: 320x240 YUV422  = 320*240*2 = 153600
 *   Mode 2: 640x480 YUV411  = 640*480*3/2 = 460800
 *   Mode 3: 640x480 YUV422  = 640*480*2 = 614400
 *   Mode 4: 640x480 RGB8    = 640*480*3 = 921600
 *   Mode 5: 640x480 Mono8   = 640*480 = 307200
 */
static uint32_t
fwcam_frame_size(struct fwcam_softc *sc)
{

	if (sc->cur_format == IIDC_FMT_VGA &&
	    sc->cur_mode < FWCAM_FMT0_V4L2_NMODES)
		return (fwcam_fmt0_v4l2[sc->cur_mode].sizeimage);

	/* Default to largest VGA mode */
	return (FWCAM_MAX_FRAME_SIZE);
}

static int
fwcam_iso_start(struct fwcam_softc *sc)
{
	struct firewire_comm *fc = sc->fd.fc;
	struct fw_xferq *xferq;
	uint32_t val;
	int dma_ch, err;

	mtx_assert(&sc->mtx, MA_NOTOWNED);

	FWCAM_LOCK(sc);
	if (sc->dma_ch >= 0 || sc->state == FWCAM_STATE_STREAMING) {
		FWCAM_UNLOCK(sc);
		return (0);	/* already running or starting */
	}
	if (sc->state == FWCAM_STATE_DETACHING) {
		FWCAM_UNLOCK(sc);
		return (ENXIO);
	}
	FWCAM_UNLOCK(sc);

	dma_ch = fw_open_isodma(fc, 0);
	if (dma_ch < 0) {
		device_printf(sc->fd.dev, "no IR DMA channel available\n");
		return (EBUSY);
	}

	FWCAM_LOCK(sc);
	if (sc->dma_ch >= 0) {
		FWCAM_UNLOCK(sc);
		fc->ir[dma_ch]->flag &= ~FWXFERQ_OPEN;
		return (0);
	}
	FWCAM_UNLOCK(sc);

	xferq = fc->ir[dma_ch];
	xferq->flag |= FWXFERQ_EXTBUF | FWXFERQ_HANDLER | FWXFERQ_STREAM;

	sc->iso_channel = (uint8_t)(iso_channel & FWXFERQ_CHTAGMASK);
	xferq->flag &= ~FWXFERQ_CHTAGMASK;
	xferq->flag |= sc->iso_channel & FWXFERQ_CHTAGMASK;

	xferq->sc = (caddr_t)sc;
	xferq->hand = fwcam_iso_input;
	xferq->bnchunk = FWCAM_ISO_NCHUNK;
	xferq->bnpacket = 1;
	xferq->psize = FWCAM_ISO_PKTSIZE;
	xferq->queued = 0;
	xferq->buf = NULL;

	xferq->bulkxfer = malloc(sizeof(struct fw_bulkxfer) * xferq->bnchunk,
	    M_FWCAM, M_WAITOK | M_ZERO);

	fw_iso_init_chunks(xferq);

	sc->frame_size = fwcam_frame_size(sc);
	sc->frame_buf = malloc(sc->frame_size, M_FWCAM, M_WAITOK | M_ZERO);
	sc->frame_offset = 0;
	sc->frame_dropped = 0;

	/* IIDC spec s3.1: set video mode registers before ISO enable */
	err = fwcam_write_quadlet(sc, IIDC_CUR_V_FORMAT,
	    (uint32_t)sc->cur_format << IIDC_CUR_V_SHIFT);
	if (err) {
		device_printf(sc->fd.dev,
		    "failed to set CUR_V_FORMAT: %d\n", err);
		goto fail;
	}
	err = fwcam_write_quadlet(sc, IIDC_CUR_V_MODE,
	    (uint32_t)sc->cur_mode << IIDC_CUR_V_SHIFT);
	if (err) {
		device_printf(sc->fd.dev,
		    "failed to set CUR_V_MODE: %d\n", err);
		goto fail;
	}
	err = fwcam_write_quadlet(sc, IIDC_CUR_V_FRM_RATE,
	    (uint32_t)sc->cur_framerate << IIDC_CUR_V_SHIFT);
	if (err) {
		device_printf(sc->fd.dev,
		    "failed to set CUR_V_FRM_RATE: %d\n", err);
		goto fail;
	}

	/* Check Vmode_Error_Status - camera rejects ISO_EN on error */
	err = fwcam_read_quadlet(sc, IIDC_VMODE_ERR_STATUS, &val);
	if (err == 0 && (val & IIDC_VMODE_ERROR)) {
		device_printf(sc->fd.dev,
		    "Vmode_Error_Status set: format=%d mode=%d rate=%d "
		    "speed=%d\n", sc->cur_format, sc->cur_mode,
		    sc->cur_framerate, sc->iso_speed);
		err = EINVAL;
		goto fail;
	}

	val = ((uint32_t)sc->iso_channel << IIDC_ISO_CH_SHIFT) |
	    ((uint32_t)sc->iso_speed << IIDC_ISO_SPEED_SHIFT);
	err = fwcam_write_quadlet(sc, IIDC_ISO_CHANNEL, val);
	if (err) {
		device_printf(sc->fd.dev,
		    "failed to set ISO_CHANNEL: %d\n", err);
		goto fail;
	}

	err = fwcam_write_quadlet(sc, IIDC_ISO_EN, IIDC_ISO_EN_ON);
	if (err == EIO) {
		/*
		 * Cameras with a lens cover might power down the sensor
		 * when the cover is closed and reject streaming requests.
		 * Try to re-power and retry once before giving up.
		 */
		err = fwcam_power_on(sc);
		if (err == 0)
			err = fwcam_write_quadlet(sc, IIDC_ISO_EN,
			    IIDC_ISO_EN_ON);
		if (err) {
			device_printf(sc->fd.dev,
			    "ISO enable refused (lens cover closed?)\n");
			goto fail;
		}
	} else if (err) {
		device_printf(sc->fd.dev,
		    "failed to enable ISO: %d\n", err);
		goto fail;
	}

	err = fc->irx_enable(fc, dma_ch);
	if (err) {
		device_printf(sc->fd.dev,
		    "failed to start IR DMA: %d\n", err);
		fwcam_write_quadlet(sc, IIDC_ISO_EN, 0);
		goto fail;
	}

	FWCAM_LOCK(sc);
	if (sc->state == FWCAM_STATE_DETACHING) {
		FWCAM_UNLOCK(sc);
		fc->irx_disable(fc, dma_ch);
		fwcam_write_quadlet(sc, IIDC_ISO_EN, 0);
		err = ENXIO;
		goto fail;
	}
	sc->dma_ch = dma_ch;
	sc->state = FWCAM_STATE_STREAMING;
	FWCAM_UNLOCK(sc);

	return (0);

fail:
	if (xferq->flag & FWXFERQ_RUNNING)
		fc->irx_disable(fc, dma_ch);

	fw_iso_free_chunks(xferq, M_FWCAM);
	xferq->flag &= ~(FWXFERQ_MODEMASK | FWXFERQ_OPEN | FWXFERQ_STREAM |
	    FWXFERQ_EXTBUF | FWXFERQ_HANDLER | FWXFERQ_CHTAGMASK);
	xferq->hand = NULL;

	free(sc->frame_buf, M_FWCAM);
	sc->frame_buf = NULL;
	sc->dma_ch = -1;

	return (err);
}

static void
fwcam_iso_stop(struct fwcam_softc *sc)
{
	struct firewire_comm *fc = sc->fd.fc;
	struct fw_xferq *xferq;
	int dma_ch;

	FWCAM_LOCK(sc);
	dma_ch = sc->dma_ch;
	if (dma_ch < 0) {
		FWCAM_UNLOCK(sc);
		return;
	}
	sc->dma_ch = -1;	/* claim ownership of teardown */
	FWCAM_UNLOCK(sc);

	xferq = fc->ir[dma_ch];

	if (xferq->flag & FWXFERQ_RUNNING)
		fc->irx_disable(fc, dma_ch);

	if (sc->fwdev != NULL)
		fwcam_write_quadlet(sc, IIDC_ISO_EN, 0);

	FWCAM_LOCK(sc);
	fw_iso_wait_inactive_locked(&sc->mtx, &sc->iso_active, "fwcamis");
	FWCAM_UNLOCK(sc);

	xferq->flag &= ~(FWXFERQ_MODEMASK | FWXFERQ_OPEN | FWXFERQ_STREAM |
	    FWXFERQ_EXTBUF | FWXFERQ_HANDLER | FWXFERQ_CHTAGMASK);
	xferq->hand = NULL;

	fw_iso_free_chunks(xferq, M_FWCAM);

	free(sc->frame_buf, M_FWCAM);
	sc->frame_buf = NULL;
}

static void
fwcam_frame_done(struct fwcam_softc *sc)
{
	struct video_buf *vb;

	vb = video_buf_acquire(sc->sc_vd);
	if (vb == NULL)
		return;

	if (video_buf_write(vb, 0, sc->frame_buf, sc->frame_offset) != 0) {
		video_buf_error(vb);
		return;
	}
	video_buf_done(vb, sc->frame_offset, sc->sc_sequence++);
}

static void
fwcam_iso_input(struct fw_xferq *xferq)
{
	struct fwcam_softc *sc = (struct fwcam_softc *)xferq->sc;
	struct fw_bulkxfer *sxfer;
	struct fw_pkt *fp;
	struct mbuf *m;
	uint8_t *payload;
	uint32_t plen;
	int dma_ch;

	FWCAM_LOCK(sc);
	dma_ch = sc->dma_ch;
	if (dma_ch < 0 || sc->frame_buf == NULL) {
		FWCAM_UNLOCK(sc);
		return;
	}
	sc->iso_active = 1;
	FWCAM_UNLOCK(sc);

	while ((sxfer = STAILQ_FIRST(&xferq->stvalid)) != NULL) {
		STAILQ_REMOVE_HEAD(&xferq->stvalid, link);

		m = fw_iso_dequeue(xferq, sxfer, sc->fd.fc);
		if (m == NULL)
			continue;

		fp = mtod(m, struct fw_pkt *);
		plen = fp->mode.stream.len;
		if (plen == 0) {
			m_freem(m);
			continue;	/* empty packet (padding) */
		}

		if (fp->mode.stream.sy == 1) {
			if (sc->frame_offset > 0 &&
			    sc->frame_offset == sc->frame_size) {
				fwcam_frame_done(sc);
			} else if (sc->frame_offset > 0) {
				struct video_buf *vb;

				vb = video_buf_acquire(sc->sc_vd);
				if (vb != NULL)
					video_buf_error(vb);
				sc->frame_dropped++;
			}
			sc->frame_offset = 0;
		}

		if (m->m_len < 4) {
			m_freem(m);
			continue;
		}
		payload = mtod(m, uint8_t *) + 4;
		if (plen > (uint32_t)(m->m_len - 4))
			plen = m->m_len - 4;

		if (sc->frame_offset + plen <= sc->frame_size) {
			memcpy(sc->frame_buf + sc->frame_offset, payload, plen);
			sc->frame_offset += plen;
		} else {
			struct video_buf *vb;

			vb = video_buf_acquire(sc->sc_vd);
			if (vb != NULL)
				video_buf_error(vb);
			sc->frame_dropped++;
			sc->frame_offset = 0;
		}

		m_freem(m);
	}

	fw_iso_rearm_done(xferq, sc->fd.fc, &sc->mtx, &sc->iso_active,
	    &sc->dma_ch, dma_ch);
}

static const uint32_t fwcam_feat_inq[] = {
	[FWCAM_FEAT_BRIGHTNESS]   = IIDC_BRIGHTNESS_INQ,
	[FWCAM_FEAT_AUTO_EXPOSURE] = IIDC_AUTO_EXPOSURE_INQ,
	[FWCAM_FEAT_SHARPNESS]    = IIDC_SHARPNESS_INQ,
	[FWCAM_FEAT_WHITE_BALANCE] = IIDC_WHITE_BAL_INQ,
	[FWCAM_FEAT_HUE]         = IIDC_HUE_INQ,
	[FWCAM_FEAT_SATURATION]  = IIDC_SATURATION_INQ,
	[FWCAM_FEAT_GAMMA]       = IIDC_GAMMA_INQ,
	[FWCAM_FEAT_SHUTTER]     = IIDC_SHUTTER_INQ,
	[FWCAM_FEAT_GAIN]        = IIDC_GAIN_INQ,
	[FWCAM_FEAT_IRIS]        = IIDC_IRIS_INQ,
	[FWCAM_FEAT_FOCUS]       = IIDC_FOCUS_INQ,
	[FWCAM_FEAT_TEMPERATURE] = IIDC_TEMPERATURE_INQ,
	[FWCAM_FEAT_TRIGGER]     = IIDC_TRIGGER_INQ,
};

static const uint32_t fwcam_feat_ctrl[] = {
	[FWCAM_FEAT_BRIGHTNESS]   = IIDC_BRIGHTNESS,
	[FWCAM_FEAT_AUTO_EXPOSURE] = IIDC_AUTO_EXPOSURE,
	[FWCAM_FEAT_SHARPNESS]    = IIDC_SHARPNESS,
	[FWCAM_FEAT_WHITE_BALANCE] = IIDC_WHITE_BALANCE,
	[FWCAM_FEAT_HUE]         = IIDC_HUE,
	[FWCAM_FEAT_SATURATION]  = IIDC_SATURATION,
	[FWCAM_FEAT_GAMMA]       = IIDC_GAMMA,
	[FWCAM_FEAT_SHUTTER]     = IIDC_SHUTTER,
	[FWCAM_FEAT_GAIN]        = IIDC_GAIN,
	[FWCAM_FEAT_IRIS]        = IIDC_IRIS,
	[FWCAM_FEAT_FOCUS]       = IIDC_FOCUS,
	[FWCAM_FEAT_TEMPERATURE] = IIDC_TEMPERATURE,
	[FWCAM_FEAT_TRIGGER]     = IIDC_TRIGGER_MODE,
	[FWCAM_FEAT_ZOOM]        = IIDC_ZOOM,
	[FWCAM_FEAT_PAN]         = IIDC_PAN,
	[FWCAM_FEAT_TILT]        = IIDC_TILT,
};

/*
 * Map IIDC feature IDs to V4L2 control IDs.
 */
static const uint32_t fwcam_feat_v4l2[] = {
	[FWCAM_FEAT_BRIGHTNESS]    = V4L2_CID_BRIGHTNESS,
	[FWCAM_FEAT_AUTO_EXPOSURE] = V4L2_CID_EXPOSURE_AUTO,
	[FWCAM_FEAT_SHARPNESS]     = V4L2_CID_SHARPNESS,
	[FWCAM_FEAT_WHITE_BALANCE] = V4L2_CID_AUTO_WHITE_BALANCE,
	[FWCAM_FEAT_HUE]          = V4L2_CID_HUE,
	[FWCAM_FEAT_SATURATION]   = V4L2_CID_SATURATION,
	[FWCAM_FEAT_GAMMA]        = V4L2_CID_GAMMA,
	[FWCAM_FEAT_SHUTTER]      = V4L2_CID_EXPOSURE_ABSOLUTE,
	[FWCAM_FEAT_GAIN]         = V4L2_CID_GAIN,
	[FWCAM_FEAT_FOCUS]        = V4L2_CID_FOCUS_ABSOLUTE,
};

#define	FWCAM_V4L2_CTRL_COUNT	nitems(fwcam_feat_v4l2)

/*
 * Find the IIDC feature ID for a V4L2 control ID.
 * Returns -1 if not found.
 */
static int
fwcam_find_feat_by_v4l2(uint32_t v4l2_id)
{
	int i;

	for (i = 0; i < (int)FWCAM_V4L2_CTRL_COUNT; i++) {
		if (fwcam_feat_v4l2[i] == v4l2_id)
			return (i);
	}
	return (-1);
}

static int
fwcam_get_feature(struct fwcam_softc *sc, struct fwcam_feature *feat)
{
	uint32_t inq, ctrl;
	int err;

	if (feat->id >= FWCAM_FEAT_MAX)
		return (EINVAL);

	feat->flags = 0;
	feat->min = 0;
	feat->max = 0;
	if (feat->id < nitems(fwcam_feat_inq) &&
	    fwcam_feat_inq[feat->id] != 0) {
		err = fwcam_read_quadlet(sc, fwcam_feat_inq[feat->id], &inq);
		if (err)
			return (err);

		if (inq & (1 << 31))
			feat->flags |= FWCAM_FEATF_PRESENT;
		if (inq & (1 << 28))
			feat->flags |= FWCAM_FEATF_ONOFF;
		if (inq & (1 << 27))
			feat->flags |= FWCAM_FEATF_AUTO;
		if (inq & (1 << 26))
			feat->flags |= FWCAM_FEATF_MANUAL;
		feat->min = (inq >> 12) & 0xfff;
		feat->max = inq & 0xfff;
	}

	if (feat->id >= nitems(fwcam_feat_ctrl) ||
	    fwcam_feat_ctrl[feat->id] == 0)
		return (EINVAL);

	err = fwcam_read_quadlet(sc, fwcam_feat_ctrl[feat->id], &ctrl);
	if (err)
		return (err);

	feat->value = ctrl & 0xfff;
	/* White balance has U/B in bits [20:31] and V/R in bits [8:19] */
	if (feat->id == FWCAM_FEAT_WHITE_BALANCE)
		feat->value2 = (ctrl >> 12) & 0xfff;
	else
		feat->value2 = 0;

	return (0);
}

static int
fwcam_set_feature(struct fwcam_softc *sc, struct fwcam_feature *feat)
{
	uint32_t ctrl, val;
	int err;

	if (feat->id >= FWCAM_FEAT_MAX)
		return (EINVAL);
	if (feat->id >= nitems(fwcam_feat_ctrl) ||
	    fwcam_feat_ctrl[feat->id] == 0)
		return (EINVAL);

	err = fwcam_read_quadlet(sc, fwcam_feat_ctrl[feat->id], &ctrl);
	if (err)
		return (err);

	if (feat->id == FWCAM_FEAT_WHITE_BALANCE) {
		/* White balance: value=V/R (low 12), value2=U/B (high 12) */
		val = (ctrl & 0xff000000) |
		    ((feat->value2 & 0xfff) << 12) |
		    (feat->value & 0xfff);
	} else {
		/* Preserve upper bits, set new value in lower 12 */
		val = (ctrl & 0xfffff000) | (feat->value & 0xfff);
	}

	return (fwcam_write_quadlet(sc, fwcam_feat_ctrl[feat->id], val));
}

/*
 * Ensure the camera has been probed (powered on, capabilities read).
 * Called from hw callbacks that need device state.
 */
static int
fwcam_ensure_probed(struct fwcam_softc *sc)
{
	int err;

	FWCAM_LOCK(sc);
	if (sc->state == FWCAM_STATE_DETACHING) {
		FWCAM_UNLOCK(sc);
		return (ENXIO);
	}

	if (sc->state == FWCAM_STATE_IDLE) {
		sc->state = FWCAM_STATE_PROBING;
		FWCAM_UNLOCK(sc);
		taskqueue_enqueue(taskqueue_thread, &sc->probe_task);
		FWCAM_LOCK(sc);
	}

	while (sc->state == FWCAM_STATE_PROBING) {
		err = msleep(sc, &sc->mtx, PCATCH, "fwcampr", 10 * hz);
		if (err) {
			FWCAM_UNLOCK(sc);
			return (err == EWOULDBLOCK ? ETIMEDOUT : err);
		}
	}

	if (sc->state != FWCAM_STATE_PROBED &&
	    sc->state != FWCAM_STATE_STREAMING) {
		FWCAM_UNLOCK(sc);
		return (ENXIO);
	}

	FWCAM_UNLOCK(sc);
	return (0);
}

static int
fwcam_hw_open(device_t dev)
{
	struct fwcam_softc *sc = device_get_softc(dev);

	return (fwcam_ensure_probed(sc));
}

static int
fwcam_hw_querycap(device_t dev, struct video_caps *caps)
{

	bzero(caps, sizeof(*caps));
	strlcpy(caps->driver, "fwcam", sizeof(caps->driver));
	strlcpy(caps->card, "IIDC FireWire Camera", sizeof(caps->card));
	strlcpy(caps->bus_info, "firewire", sizeof(caps->bus_info));
	caps->version = (1 << 16) | (0 << 8) | 0;	/* 1.0.0 */
	caps->capabilities = VIDEO_CAP_CAPTURE |
	    VIDEO_CAP_READWRITE | VIDEO_CAP_STREAMING;

	return (0);
}

/*
 * Map a sequential enum index to an IIDC mode number.
 * Returns -1 if index is out of range.
 */
static int
fwcam_index_to_mode(struct fwcam_softc *sc, uint32_t index)
{
	int m;
	uint32_t count = 0;

	if (!(sc->formats & IIDC_FORMAT_VGA))
		return (-1);

	for (m = 0; m < (int)FWCAM_FMT0_V4L2_NMODES; m++) {
		if (sc->modes[IIDC_FMT_VGA] & (1 << (31 - m))) {
			if (count == index)
				return (m);
			count++;
		}
	}
	return (-1);
}

static int
fwcam_hw_enum_format(device_t dev, uint32_t index, struct video_format *fmt)
{
	struct fwcam_softc *sc = device_get_softc(dev);
	const struct fwcam_v4l2_mode *vm;
	int m;

	m = fwcam_index_to_mode(sc, index);
	if (m < 0)
		return (EINVAL);

	vm = &fwcam_fmt0_v4l2[m];

	bzero(fmt, sizeof(*fmt));
	fmt->pixelformat = vm->pixelformat;
	fmt->width = vm->width;
	fmt->height = vm->height;
	fmt->bytesperline = vm->bytesperline;
	fmt->sizeimage = vm->sizeimage;
	fmt->field = V4L2_FIELD_NONE;

	return (0);
}

static int
fwcam_hw_get_format(device_t dev, struct video_format *fmt)
{
	struct fwcam_softc *sc = device_get_softc(dev);
	const struct fwcam_v4l2_mode *vm;

	if (sc->cur_format != IIDC_FMT_VGA ||
	    sc->cur_mode >= FWCAM_FMT0_V4L2_NMODES)
		return (EIO);

	vm = &fwcam_fmt0_v4l2[sc->cur_mode];

	bzero(fmt, sizeof(*fmt));
	fmt->pixelformat = vm->pixelformat;
	fmt->width = vm->width;
	fmt->height = vm->height;
	fmt->bytesperline = vm->bytesperline;
	fmt->sizeimage = vm->sizeimage;
	fmt->field = V4L2_FIELD_NONE;

	return (0);
}

/*
 * Find the IIDC mode that best matches a V4L2 format request.
 * Returns -1 if no match.
 */
static int
fwcam_find_mode_for_format(struct fwcam_softc *sc,
    const struct video_format *fmt)
{
	int m, best = -1;

	if (!(sc->formats & IIDC_FORMAT_VGA))
		return (-1);

	for (m = 0; m < (int)FWCAM_FMT0_V4L2_NMODES; m++) {
		if (!(sc->modes[IIDC_FMT_VGA] & (1 << (31 - m))))
			continue;
		if (fwcam_fmt0_v4l2[m].pixelformat == fmt->pixelformat &&
		    fwcam_fmt0_v4l2[m].width == fmt->width &&
		    fwcam_fmt0_v4l2[m].height == fmt->height) {
			best = m;
			break;
		}
	}

	/* If exact match failed, try matching just pixelformat */
	if (best < 0) {
		for (m = 0; m < (int)FWCAM_FMT0_V4L2_NMODES; m++) {
			if (!(sc->modes[IIDC_FMT_VGA] & (1 << (31 - m))))
				continue;
			if (fwcam_fmt0_v4l2[m].pixelformat ==
			    fmt->pixelformat) {
				best = m;
				break;
			}
		}
	}

	return (best);
}

static int
fwcam_hw_try_format(device_t dev, struct video_format *fmt)
{
	struct fwcam_softc *sc = device_get_softc(dev);
	const struct fwcam_v4l2_mode *vm;
	int m;

	m = fwcam_find_mode_for_format(sc, fmt);
	if (m < 0)
		return (EINVAL);

	vm = &fwcam_fmt0_v4l2[m];

	fmt->pixelformat = vm->pixelformat;
	fmt->width = vm->width;
	fmt->height = vm->height;
	fmt->bytesperline = vm->bytesperline;
	fmt->sizeimage = vm->sizeimage;
	fmt->field = V4L2_FIELD_NONE;

	return (0);
}

static int
fwcam_hw_set_format(device_t dev, const struct video_format *fmt)
{
	struct fwcam_softc *sc = device_get_softc(dev);
	int m, err;

	m = fwcam_find_mode_for_format(sc, fmt);
	if (m < 0)
		return (EINVAL);

	if (!(sc->rates[IIDC_FMT_VGA][m] &
	    (1 << (31 - sc->cur_framerate)))) {
		/* Current framerate not supported in new mode, pick first */
		int r;
		for (r = 0; r < 8; r++) {
			if (sc->rates[IIDC_FMT_VGA][m] & (1 << (31 - r))) {
				sc->cur_framerate = r;
				break;
			}
		}
	}

	err = fwcam_write_quadlet(sc, IIDC_CUR_V_FORMAT,
	    (uint32_t)IIDC_FMT_VGA << IIDC_CUR_V_SHIFT);
	if (err == 0)
		err = fwcam_write_quadlet(sc, IIDC_CUR_V_MODE,
		    (uint32_t)m << IIDC_CUR_V_SHIFT);
	if (err == 0)
		err = fwcam_write_quadlet(sc, IIDC_CUR_V_FRM_RATE,
		    (uint32_t)sc->cur_framerate << IIDC_CUR_V_SHIFT);

	if (err == 0) {
		sc->cur_format = IIDC_FMT_VGA;
		sc->cur_mode = m;
	}

	return (err);
}

static int
fwcam_hw_enum_framesizes(device_t dev, struct video_frmsizeenum *fse)
{
	struct fwcam_softc *sc = device_get_softc(dev);
	int m;
	uint32_t count = 0;

	if (!(sc->formats & IIDC_FORMAT_VGA))
		return (EINVAL);

	for (m = 0; m < (int)FWCAM_FMT0_V4L2_NMODES; m++) {
		if (!(sc->modes[IIDC_FMT_VGA] & (1 << (31 - m))))
			continue;
		if (fwcam_fmt0_v4l2[m].pixelformat != fse->pixelformat)
			continue;
		if (count == fse->index) {
			fse->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			fse->discrete.width = fwcam_fmt0_v4l2[m].width;
			fse->discrete.height = fwcam_fmt0_v4l2[m].height;
			return (0);
		}
		count++;
	}

	return (EINVAL);
}

static int
fwcam_hw_enum_input(device_t dev, uint32_t index, struct video_input *inp)
{

	if (index != 0)
		return (EINVAL);

	bzero(inp, sizeof(*inp));
	inp->index = 0;
	strlcpy(inp->name, "IIDC Camera", sizeof(inp->name));
	inp->type = VIDEO_INPUT_TYPE_CAMERA;

	return (0);
}

static int
fwcam_hw_get_input(device_t dev, uint32_t *index)
{

	*index = 0;
	return (0);
}

static int
fwcam_hw_set_input(device_t dev, uint32_t index)
{

	if (index != 0)
		return (EINVAL);
	return (0);
}

static int
fwcam_hw_query_control(device_t dev, struct video_control_desc *qc)
{
	struct fwcam_softc *sc = device_get_softc(dev);
	struct fwcam_feature feat;
	int fid;

	fid = fwcam_find_feat_by_v4l2(qc->id);
	if (fid < 0)
		return (EINVAL);

	feat.id = fid;
	if (fwcam_get_feature(sc, &feat) != 0)
		return (EINVAL);

	if (!(feat.flags & FWCAM_FEATF_PRESENT))
		return (EINVAL);

	qc->type = V4L2_CTRL_TYPE_INTEGER;
	strlcpy(qc->name, fwcam_feat_names[fid], sizeof(qc->name));
	qc->minimum = feat.min;
	qc->maximum = feat.max;
	qc->step = 1;
	qc->default_value = feat.min;
	qc->flags = 0;

	return (0);
}

static int
fwcam_hw_get_control(device_t dev, struct video_control *ctrl)
{
	struct fwcam_softc *sc = device_get_softc(dev);
	struct fwcam_feature feat;
	int fid;

	fid = fwcam_find_feat_by_v4l2(ctrl->id);
	if (fid < 0)
		return (EINVAL);

	feat.id = fid;
	if (fwcam_get_feature(sc, &feat) != 0)
		return (EINVAL);

	ctrl->value = feat.value;
	return (0);
}

static int
fwcam_hw_set_control(device_t dev, const struct video_control *ctrl)
{
	struct fwcam_softc *sc = device_get_softc(dev);
	struct fwcam_feature feat;
	int fid;

	fid = fwcam_find_feat_by_v4l2(ctrl->id);
	if (fid < 0)
		return (EINVAL);

	feat.id = fid;
	feat.value = ctrl->value;
	feat.value2 = 0;
	return (fwcam_set_feature(sc, &feat));
}

static int
fwcam_hw_start_stream(device_t dev)
{
	struct fwcam_softc *sc = device_get_softc(dev);
	int err;

	err = fwcam_ensure_probed(sc);
	if (err)
		return (err);

	sc->sc_sequence = 0;

	return (fwcam_iso_start(sc));
}

static void
fwcam_hw_stop_stream(device_t dev)
{
	struct fwcam_softc *sc = device_get_softc(dev);

	fwcam_iso_stop(sc);

	FWCAM_LOCK(sc);
	if (sc->state != FWCAM_STATE_DETACHING)
		sc->state = FWCAM_STATE_PROBED;
	FWCAM_UNLOCK(sc);
}

static int
fwcam_probe(device_t dev)
{
	struct fw_unit *unit;

	unit = fw_get_unit(dev);
	if (unit == NULL)
		return (ENXIO);

	if (unit->spec_id != CSRVAL_1394TA)
		return (ENXIO);
	if (unit->sw_version != CSR_PROTCAM130 &&
	    unit->sw_version != CSR_PROTCAM120 &&
	    unit->sw_version != CSR_PROTCAM104)
		return (ENXIO);

	device_set_desc(dev, "IIDC Digital Camera over FireWire");
	return (BUS_PROBE_DEFAULT);
}

static int
fwcam_attach(device_t dev)
{
	struct fwcam_softc *sc;
	struct fw_unit *unit;
	uint32_t cmd_base;
	int err;

	unit = fw_get_unit(dev);
	if (unit == NULL || unit->fwdev == NULL)
		return (ENXIO);

	cmd_base = fwcam_find_iidc_cmd_base(unit);
	if (cmd_base == 0) {
		device_printf(dev, "no IIDC command base in unit directory\n");
		return (ENXIO);
	}

	sc = device_get_softc(dev);
	sc->fd.dev = dev;
	sc->fd.fc = fw_get_comm(dev);
	mtx_init(&sc->mtx, "fwcam", NULL, MTX_DEF);

	sc->fwdev = unit->fwdev;
	sc->cmd_hi = 0xffff;
	sc->cmd_lo = 0xf0000000 | (cmd_base << 2);
	sc->iso_speed = min(unit->fwdev->speed, FWSPD_S400);
	sc->state = FWCAM_STATE_IDLE;
	sc->dma_ch = -1;
	sc->iso_active = 0;
	sc->sc_sequence = 0;
	TASK_INIT(&sc->probe_task, 0, fwcam_probe_task, sc);

	err = video_register(dev, &sc->sc_vd);
	if (err != 0) {
		device_printf(dev, "failed to register video device\n");
		mtx_destroy(&sc->mtx);
		return (err);
	}

	return (0);
}

static int
fwcam_detach(device_t dev)
{
	struct fwcam_softc *sc;

	sc = device_get_softc(dev);

	if (sc->sc_vd != NULL)
		video_unregister(sc->sc_vd);

	FWCAM_LOCK(sc);
	sc->state = FWCAM_STATE_DETACHING;
	wakeup(sc);
	FWCAM_UNLOCK(sc);

	taskqueue_drain(taskqueue_thread, &sc->probe_task);
	fwcam_iso_stop(sc);

	FWCAM_LOCK(sc);
	sc->fwdev = NULL;
	FWCAM_UNLOCK(sc);

	mtx_destroy(&sc->mtx);

	return (0);
}

static device_method_t fwcam_methods[] = {
	DEVMETHOD(device_probe,		fwcam_probe),
	DEVMETHOD(device_attach,	fwcam_attach),
	DEVMETHOD(device_detach,	fwcam_detach),

	/* video(4) interface */
	DEVMETHOD(video_open,		fwcam_hw_open),
	DEVMETHOD(video_querycap,	fwcam_hw_querycap),
	DEVMETHOD(video_enum_format,	fwcam_hw_enum_format),
	DEVMETHOD(video_get_format,	fwcam_hw_get_format),
	DEVMETHOD(video_try_format,	fwcam_hw_try_format),
	DEVMETHOD(video_set_format,	fwcam_hw_set_format),
	DEVMETHOD(video_enum_framesizes, fwcam_hw_enum_framesizes),
	DEVMETHOD(video_enum_input,	fwcam_hw_enum_input),
	DEVMETHOD(video_get_input,	fwcam_hw_get_input),
	DEVMETHOD(video_set_input,	fwcam_hw_set_input),
	DEVMETHOD(video_query_control,	fwcam_hw_query_control),
	DEVMETHOD(video_get_control,	fwcam_hw_get_control),
	DEVMETHOD(video_set_control,	fwcam_hw_set_control),
	DEVMETHOD(video_start_stream,	fwcam_hw_start_stream),
	DEVMETHOD(video_stop_stream,	fwcam_hw_stop_stream),

	DEVMETHOD_END
};

static driver_t fwcam_driver = {
	"fwcam",
	fwcam_methods,
	sizeof(struct fwcam_softc),
};

DRIVER_MODULE(fwcam, firewire, fwcam_driver, 0, 0);
MODULE_VERSION(fwcam, 1);
MODULE_DEPEND(fwcam, firewire, 1, 1, 1);
MODULE_DEPEND(fwcam, video, 1, 1, 1);
