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
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>
#include <sys/fcntl.h>
#include <sys/poll.h>
#include <sys/selinfo.h>
#include <sys/uio.h>
#include <sys/mbuf.h>

#include <dev/firewire/firewire.h>
#include <dev/firewire/firewirereg.h>
#include <dev/firewire/iec13213.h>
#include <dev/firewire/fwcam.h>
#include <dev/firewire/fw_helpers.h>

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

static void	fwcam_identify(driver_t *, device_t);
static int	fwcam_probe(device_t);
static int	fwcam_attach(device_t);
static int	fwcam_detach(device_t);
static void	fwcam_post_busreset(void *);
static void	fwcam_probe_task(void *, int);
static void	fwcam_post_explore(void *);
static int	fwcam_iso_start(struct fwcam_softc *);
static void	fwcam_iso_stop(struct fwcam_softc *);
static void	fwcam_iso_input(struct fw_xferq *);

static d_open_t		fwcam_cdev_open;
static d_close_t	fwcam_cdev_close;
static d_read_t		fwcam_cdev_read;
static d_poll_t		fwcam_cdev_poll;
static d_ioctl_t	fwcam_cdev_ioctl;

static struct cdevsw fwcam_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	D_TRACKCLOSE,
	.d_open =	fwcam_cdev_open,
	.d_close =	fwcam_cdev_close,
	.d_read =	fwcam_cdev_read,
	.d_poll =	fwcam_cdev_poll,
	.d_ioctl =	fwcam_cdev_ioctl,
	.d_name =	"fwcam",
};

static uint32_t
fwcam_find_iidc(struct fw_device *fwdev)
{
	struct crom_context cc;
	struct csrreg *reg;
	uint32_t cmd_base;

	if (!crom_has_specver(fwdev->csrrom, CSRVAL_1394TA, CSR_PROTCAM130) &&
	    !crom_has_specver(fwdev->csrrom, CSRVAL_1394TA, CSR_PROTCAM120) &&
	    !crom_has_specver(fwdev->csrrom, CSRVAL_1394TA, CSR_PROTCAM104))
		return (0);

	cmd_base = 0;
	crom_init_context(&cc, fwdev->csrrom);
	reg = crom_search_key(&cc, IIDC_CROM_CMD_BASE);
	if (reg != NULL && reg->val != 0)
		cmd_base = reg->val;

	return (cmd_base);
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

#if 0
static const char *
fwcam_format_name(int format)
{
	static const char *names[] = {
		"VGA (Format_0)",
		"Super VGA 1 (Format_1)",
		"Super VGA 2 (Format_2)",
		"Reserved",
		"Reserved",
		"Reserved",
		"Still Image (Format_6)",
		"Partial Image (Format_7)",
	};

	if (format >= 0 && format <= 7)
		return (names[format]);
	return ("Unknown");
}
#endif

static int
fwcam_read_capabilities(struct fwcam_softc *sc)
{
	int err;

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
		FWCAM_UNLOCK(sc);

		if (sc->open_count > 0 &&
		    sc->state != FWCAM_STATE_DETACHING)
			fwcam_iso_start(sc);
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
	static const uint32_t fmt0_sizes[] = {
		160 * 120 * 3,		/* mode 0: YUV444 */
		320 * 240 * 2,		/* mode 1: YUV422 */
		640 * 480 * 3 / 2,	/* mode 2: YUV411 */
		640 * 480 * 2,		/* mode 3: YUV422 */
		640 * 480 * 3,		/* mode 4: RGB8 */
		640 * 480,		/* mode 5: Mono8 */
	};

	if (sc->cur_format == IIDC_FMT_VGA && sc->cur_mode < nitems(fmt0_sizes))
		return (fmt0_sizes[sc->cur_mode]);

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
	sc->read_buf = malloc(sc->frame_size, M_FWCAM, M_WAITOK | M_ZERO);
	sc->frame_offset = 0;
	sc->frame_ready = 0;
	sc->frame_dropped = 0;

	val = ((uint32_t)sc->iso_channel << IIDC_ISO_CH_SHIFT) |
	    ((uint32_t)sc->iso_speed << IIDC_ISO_SPEED_SHIFT);
	err = fwcam_write_quadlet(sc, IIDC_ISO_CHANNEL, val);
	if (err) {
		device_printf(sc->fd.dev,
		    "failed to set ISO_CHANNEL: %d\n", err);
		goto fail;
	}

	err = fwcam_write_quadlet(sc, IIDC_ISO_EN, IIDC_ISO_EN_ON);
	if (err) {
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
	fw_iso_free_chunks(xferq, M_FWCAM);
	xferq->flag &= ~(FWXFERQ_MODEMASK | FWXFERQ_OPEN | FWXFERQ_STREAM |
	    FWXFERQ_EXTBUF | FWXFERQ_HANDLER | FWXFERQ_CHTAGMASK);
	xferq->hand = NULL;

	free(sc->frame_buf, M_FWCAM);
	free(sc->read_buf, M_FWCAM);
	sc->frame_buf = NULL;
	sc->read_buf = NULL;
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
	sc->frame_ready = 0;
	while (sc->read_in_progress)
		msleep(&sc->read_in_progress, &sc->mtx, PWAIT, "fwcamst", hz);
	FWCAM_UNLOCK(sc);

	xferq->flag &= ~(FWXFERQ_MODEMASK | FWXFERQ_OPEN | FWXFERQ_STREAM |
	    FWXFERQ_EXTBUF | FWXFERQ_HANDLER | FWXFERQ_CHTAGMASK);
	xferq->hand = NULL;

	fw_iso_free_chunks(xferq, M_FWCAM);

	free(sc->frame_buf, M_FWCAM);
	free(sc->read_buf, M_FWCAM);
	sc->frame_buf = NULL;
	sc->read_buf = NULL;
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
	uint8_t *tmp;
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
			if (sc->frame_offset > 0) {
				if (sc->frame_offset == sc->frame_size) {
					FWCAM_LOCK(sc);
					if (sc->read_in_progress) {
						sc->frame_dropped++;
					} else {
						if (sc->frame_ready)
							sc->frame_dropped++;
						tmp = sc->read_buf;
						sc->read_buf = sc->frame_buf;
						sc->frame_buf = tmp;
						sc->frame_ready = 1;
						wakeup(sc);
						selwakeup(&sc->rsel);
					}
					FWCAM_UNLOCK(sc);
				} else {
					sc->frame_dropped++;
				}
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
			sc->frame_dropped++;
			sc->frame_offset = 0;
		}

		m_freem(m);
	}

	fw_iso_rearm_done(xferq, sc->fd.fc, &sc->mtx, &sc->iso_active,
	    &sc->dma_ch, dma_ch);
}

static int
fwcam_cdev_open(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	struct fwcam_softc *sc = dev->si_drv1;
	int err = 0;

	FWCAM_LOCK(sc);
	if (sc->state == FWCAM_STATE_DETACHING) {
		FWCAM_UNLOCK(sc);
		return (ENXIO);
	}
	if (sc->state != FWCAM_STATE_PROBED &&
	    sc->state != FWCAM_STATE_STREAMING) {
		FWCAM_UNLOCK(sc);
		return (EAGAIN);	/* not yet probed */
	}

	sc->open_count++;
	if (sc->open_count == 1 && sc->state == FWCAM_STATE_PROBED) {
		FWCAM_UNLOCK(sc);
		err = fwcam_iso_start(sc);
		if (err) {
			FWCAM_LOCK(sc);
			sc->open_count--;
			FWCAM_UNLOCK(sc);
		}
	} else {
		FWCAM_UNLOCK(sc);
	}
	return (err);
}

static int
fwcam_cdev_close(struct cdev *dev, int fflag, int devtype, struct thread *td)
{
	struct fwcam_softc *sc = dev->si_drv1;

	FWCAM_LOCK(sc);
	sc->open_count--;
	if (sc->open_count <= 0) {
		sc->open_count = 0;
		if (sc->state == FWCAM_STATE_STREAMING) {
			FWCAM_UNLOCK(sc);
			fwcam_iso_stop(sc);
			FWCAM_LOCK(sc);
			if (sc->state != FWCAM_STATE_DETACHING)
				sc->state = FWCAM_STATE_PROBED;
		}
	}
	FWCAM_UNLOCK(sc);
	return (0);
}

static int
fwcam_cdev_read(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct fwcam_softc *sc = dev->si_drv1;
	int err;

	FWCAM_LOCK(sc);
	while (!sc->frame_ready) {
		if (sc->state != FWCAM_STATE_STREAMING) {
			FWCAM_UNLOCK(sc);
			return (ENXIO);
		}
		if (ioflag & FNONBLOCK) {
			FWCAM_UNLOCK(sc);
			return (EAGAIN);
		}
		err = msleep(sc, &sc->mtx, PCATCH, "fwcamrd", 5 * hz);
		if (err) {
			FWCAM_UNLOCK(sc);
			return (err);
		}
	}

	sc->frame_ready = 0;
	if (sc->read_buf == NULL) {
		FWCAM_UNLOCK(sc);
		return (ENXIO);
	}
	sc->read_in_progress = 1;
	FWCAM_UNLOCK(sc);

	err = uiomove(sc->read_buf,
	    MIN(uio->uio_resid, sc->frame_size), uio);

	FWCAM_LOCK(sc);
	sc->read_in_progress = 0;
	wakeup(&sc->read_in_progress);
	FWCAM_UNLOCK(sc);

	return (err);
}

static int
fwcam_cdev_poll(struct cdev *dev, int events, struct thread *td)
{
	struct fwcam_softc *sc = dev->si_drv1;
	int revents = 0;

	FWCAM_LOCK(sc);
	if (events & (POLLIN | POLLRDNORM)) {
		if (sc->frame_ready)
			revents |= events & (POLLIN | POLLRDNORM);
		else
			selrecord(td, &sc->rsel);
	}
	FWCAM_UNLOCK(sc);
	return (revents);
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

static int
fwcam_cdev_ioctl(struct cdev *dev, u_long cmd, caddr_t data,
    int fflag, struct thread *td)
{
	struct fwcam_softc *sc = dev->si_drv1;
	struct fwcam_mode *mode;
	struct fwcam_feature *feat;
	struct fwcam_info *info;
	int err;

	if (sc->fwdev == NULL)
		return (ENXIO);

	switch (cmd) {
	case FWCAM_GMODE:
		mode = (struct fwcam_mode *)data;
		mode->format = sc->cur_format;
		mode->mode = sc->cur_mode;
		mode->framerate = sc->cur_framerate;
		mode->frame_size = sc->frame_size ?
		    sc->frame_size : fwcam_frame_size(sc);
		return (0);

	case FWCAM_SMODE:
	    {
		int was_streaming = 0;

		mode = (struct fwcam_mode *)data;
		if (mode->format > 7 || mode->mode > 7 || mode->framerate > 7)
			return (EINVAL);

		if (mode->format != IIDC_FMT_VGA)
			return (EINVAL);
		if (!(sc->formats & (1 << (31 - mode->format))))
			return (EINVAL);

		FWCAM_LOCK(sc);
		if (sc->state == FWCAM_STATE_DETACHING) {
			FWCAM_UNLOCK(sc);
			return (ENXIO);
		}
		if (sc->state == FWCAM_STATE_STREAMING) {
			was_streaming = 1;
			FWCAM_UNLOCK(sc);
			fwcam_iso_stop(sc);
			FWCAM_LOCK(sc);
			if (sc->state != FWCAM_STATE_DETACHING)
				sc->state = FWCAM_STATE_PROBED;
		}
		FWCAM_UNLOCK(sc);

		err = fwcam_write_quadlet(sc, IIDC_CUR_V_FORMAT,
		    (uint32_t)mode->format << 29);
		if (err == 0)
			err = fwcam_write_quadlet(sc, IIDC_CUR_V_MODE,
			    (uint32_t)mode->mode << 29);
		if (err == 0)
			err = fwcam_write_quadlet(sc, IIDC_CUR_V_FRM_RATE,
			    (uint32_t)mode->framerate << 29);

		if (err == 0) {
			sc->cur_format = mode->format;
			sc->cur_mode = mode->mode;
			sc->cur_framerate = mode->framerate;
			mode->frame_size = fwcam_frame_size(sc);
		}

		if (was_streaming)
			fwcam_iso_start(sc);
		return (err);
	    }

	case FWCAM_GFEAT:
		feat = (struct fwcam_feature *)data;
		return (fwcam_get_feature(sc, feat));

	case FWCAM_SFEAT:
		feat = (struct fwcam_feature *)data;
		return (fwcam_set_feature(sc, feat));

	case FWCAM_GINFO:
		info = (struct fwcam_info *)data;
		info->formats = sc->formats;
		info->basic_func = sc->basic_func;
		info->features_hi = sc->features_hi;
		info->features_lo = sc->features_lo;
		info->cur_format = sc->cur_format;
		info->cur_mode = sc->cur_mode;
		info->cur_framerate = sc->cur_framerate;
		info->state = sc->state;
		info->frame_size = sc->frame_size ?
		    sc->frame_size : fwcam_frame_size(sc);
		info->frame_dropped = sc->frame_dropped;
		info->iso_channel = sc->iso_channel;
		info->_pad[0] = info->_pad[1] = info->_pad[2] = 0;
		return (0);

	default:
		return (ENOTTY);
	}
}

static void
fwcam_post_explore(void *arg)
{
	struct fwcam_softc *sc = (struct fwcam_softc *)arg;
	struct fw_device *fwdev;
	uint32_t cmd_base;
	int was_streaming, err;

	FWCAM_LOCK(sc);

	if (sc->state == FWCAM_STATE_DETACHING) {
		FWCAM_UNLOCK(sc);
		return;
	}

	if (sc->fwdev != NULL) {
		STAILQ_FOREACH(fwdev, &sc->fd.fc->devices, link) {
			if (fwdev == sc->fwdev &&
			    fwdev->status == FWDEVATTACHED)
				break;
		}
		if (fwdev == NULL) {
			was_streaming = (sc->state == FWCAM_STATE_STREAMING);
			device_printf(sc->fd.dev, "camera disconnected%s\n",
			    was_streaming ? " (was streaming)" : "");
			sc->fwdev = NULL;
			sc->state = FWCAM_STATE_IDLE;
			wakeup(sc);		/* unblock readers */
			selwakeup(&sc->rsel);
			FWCAM_UNLOCK(sc);

			if (was_streaming)
				fwcam_iso_stop(sc);
			FWCAM_LOCK(sc);
		}
	}

	if (sc->fwdev == NULL) {
		STAILQ_FOREACH(fwdev, &sc->fd.fc->devices, link) {
			if (fwdev->status != FWDEVATTACHED)
				continue;

			cmd_base = fwcam_find_iidc(fwdev);
			if (cmd_base == 0)
				continue;

			sc->fwdev = fwdev;
			sc->cmd_hi = 0xffff;
			sc->cmd_lo = 0xf0000000 | (cmd_base << 2);

			FWCAM_UNLOCK(sc);
			err = taskqueue_enqueue(taskqueue_thread,
			    &sc->probe_task);
			if (err)
				device_printf(sc->fd.dev,
				    "taskqueue_enqueue failed: %d\n", err);
			return;
		}
	}

	FWCAM_UNLOCK(sc);
}

static void
fwcam_post_busreset(void *arg __unused)
{

}

static void
fwcam_identify(driver_t *driver, device_t parent)
{

	if (device_find_child(parent, "fwcam", DEVICE_UNIT_ANY) == NULL)
		BUS_ADD_CHILD(parent, 0, "fwcam", DEVICE_UNIT_ANY);
}

static int
fwcam_probe(device_t dev)
{

	device_set_desc(dev, "IIDC Digital Camera over FireWire");
	return (0);
}

static int
fwcam_attach(device_t dev)
{
	struct fwcam_softc *sc;

	sc = device_get_softc(dev);
	sc->fd.dev = dev;
	sc->fd.fc = device_get_ivars(dev);
	mtx_init(&sc->mtx, "fwcam", NULL, MTX_DEF);

	sc->fwdev = NULL;
	sc->state = FWCAM_STATE_IDLE;
	sc->dma_ch = -1;
	sc->iso_active = 0;
	sc->open_count = 0;
	knlist_init_mtx(&sc->rsel.si_note, &sc->mtx);
	TASK_INIT(&sc->probe_task, 0, fwcam_probe_task, sc);

	sc->cdev = make_dev(&fwcam_cdevsw, device_get_unit(dev),
	    UID_ROOT, GID_OPERATOR, 0660, "fwcam%d", device_get_unit(dev));
	sc->cdev->si_drv1 = sc;

	sc->fd.post_busreset = fwcam_post_busreset;
	sc->fd.post_explore = fwcam_post_explore;

	fwcam_post_explore(sc);

	return (0);
}

static int
fwcam_detach(device_t dev)
{
	struct fwcam_softc *sc;

	sc = device_get_softc(dev);

	FWCAM_LOCK(sc);
	sc->state = FWCAM_STATE_DETACHING;
	wakeup(sc);	/* wake any sleeping readers */
	FWCAM_UNLOCK(sc);

	taskqueue_drain(taskqueue_thread, &sc->probe_task);
	fwcam_iso_stop(sc);

	if (sc->cdev != NULL)
		destroy_dev(sc->cdev);

	seldrain(&sc->rsel);
	knlist_destroy(&sc->rsel.si_note);

	FWCAM_LOCK(sc);
	sc->fwdev = NULL;
	FWCAM_UNLOCK(sc);

	mtx_destroy(&sc->mtx);

	return (0);
}

static device_method_t fwcam_methods[] = {
	DEVMETHOD(device_identify,	fwcam_identify),
	DEVMETHOD(device_probe,		fwcam_probe),
	DEVMETHOD(device_attach,	fwcam_attach),
	DEVMETHOD(device_detach,	fwcam_detach),

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
