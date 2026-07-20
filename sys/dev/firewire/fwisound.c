/*
 * Copyright (c) 2026 Abdelkader Boudih <freebsd@seuros.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * fwisound(4) - Apple FireWire audio driver
 *
 * Protocol reverse-engineered by Clemens Ladisch (Linux isight_audio).
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

#include <dev/firewire/firewire.h>
#include <dev/firewire/firewirereg.h>
#include <dev/firewire/iec13213.h>
#include <dev/firewire/fwisound.h>
#include <dev/firewire/fw_helpers.h>

static MALLOC_DEFINE(M_FWISOUND, "fwisound", "Apple FireWire Audio");

static int debug = 0;
static int iso_channel = FWISOUND_ISO_CHANNEL;
SYSCTL_DECL(_hw_firewire);
static SYSCTL_NODE(_hw_firewire, OID_AUTO, fwisound,
    CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "Apple FireWire Audio");
SYSCTL_INT(_hw_firewire_fwisound, OID_AUTO, debug, CTLFLAG_RWTUN, &debug, 0,
    "fwisound debug level");
SYSCTL_INT(_hw_firewire_fwisound, OID_AUTO, iso_channel, CTLFLAG_RWTUN,
    &iso_channel, 0, "ISO channel for isochronous receive (default 1)");

#define FWISOUND_DEBUG(lev, fmt, ...)					\
	do {								\
		if (debug >= (lev))					\
			printf("fwisound: " fmt, ## __VA_ARGS__);	\
	} while (0)

static int	fwisound_probe(device_t);
static int	fwisound_attach(device_t);
static int	fwisound_detach(device_t);
static void	fwisound_probe_task(void *, int);
static void	fwisound_start_task(void *, int);
static void	fwisound_stop_task(void *, int);
static void	fwisound_iso_input(struct fw_xferq *);

static int
fwisound_read_quadlet(struct fwisound_softc *sc, uint32_t offset, uint32_t *val)
{
	uint16_t dst;
	uint8_t spd;
	int err;

	FWISOUND_LOCK(sc);
	if (sc->fwdev == NULL) {
		FWISOUND_UNLOCK(sc);
		return (ENXIO);
	}
	dst = FWLOCALBUS | sc->fwdev->dst;
	spd = min(sc->fwdev->speed, FWSPD_S400);
	FWISOUND_UNLOCK(sc);

	err = fw_read_quadlet(sc->fd.fc, M_FWISOUND, dst, spd,
	    sc->cmd_hi, sc->cmd_lo + offset, val);
	if (err)
		FWISOUND_DEBUG(1, "read_quadlet: offset=0x%x err=%d\n",
		    offset, err);
	return (err);
}

static int
fwisound_write_quadlet(struct fwisound_softc *sc, uint32_t offset, uint32_t val)
{
	uint16_t dst;
	uint8_t spd;
	int err;

	FWISOUND_LOCK(sc);
	if (sc->fwdev == NULL) {
		FWISOUND_UNLOCK(sc);
		return (ENXIO);
	}
	dst = FWLOCALBUS | sc->fwdev->dst;
	spd = min(sc->fwdev->speed, FWSPD_S400);
	FWISOUND_UNLOCK(sc);

	err = fw_write_quadlet(sc->fd.fc, M_FWISOUND, dst, spd,
	    sc->cmd_hi, sc->cmd_lo + offset, val);
	if (err)
		FWISOUND_DEBUG(1, "write_quadlet: offset=0x%x err=%d\n",
		    offset, err);
	return (err);
}

/*
 * Extract the audio command base register (key 0x40) from the unit directory.
 */
static uint32_t
fwisound_find_audio_base(struct fw_unit *unit)
{
	struct fw_device *fwdev = unit->fwdev;
	struct csrdirectory *udir;
	struct csrreg *reg;
	uint32_t udir_qoff, udir_len, rom_quads;
	int i;

	rom_quads = CSRROMSIZE / 4;
	udir_qoff = unit->dir_offset / 4;
	if (udir_qoff >= rom_quads)
		return (0);

	udir = (struct csrdirectory *)&fwdev->csrrom[udir_qoff];
	udir_len = udir->crc_len;
	if (udir_qoff + 1 + udir_len > rom_quads)
		return (0);
	if (!crom_crc_valid((uint32_t *)&udir->entry[0], udir_len, udir->crc)) {
		if (firewire_debug)
			printf("fwisound: bad CRC in unit directory at 0x%x\n",
			    udir_qoff);
	}

	for (i = 0; i < (int)udir_len; i++) {
		if (udir_qoff + 1 + i >= rom_quads)
			break;
		reg = &udir->entry[i];
		if (reg->key == IIDC_CROM_CMD_BASE && reg->val != 0)
			return (reg->val);
	}
	return (0);
}

static void
fwisound_probe_task(void *arg, int pending __unused)
{
	struct fwisound_softc *sc = (struct fwisound_softc *)arg;
	uint32_t val;
	int err;

	if (sc->state == FWISOUND_STATE_DETACHING || sc->fwdev == NULL)
		return;

	err = fwisound_write_quadlet(sc, FWISOUND_REG_SAMPLE_RATE,
	    FWISOUND_RATE_48000);
	if (err) {
		device_printf(sc->dev,
		    "failed to set sample rate: %d\n", err);
		return;
	}

	if (fwisound_read_quadlet(sc, FWISOUND_REG_GAIN_RAW_START, &val) == 0)
		sc->gain_raw_min = (int32_t)val;
	if (fwisound_read_quadlet(sc, FWISOUND_REG_GAIN_RAW_END, &val) == 0)
		sc->gain_raw_max = (int32_t)val;

	FWISOUND_LOCK(sc);
	if (sc->state == FWISOUND_STATE_DETACHING) {
		FWISOUND_UNLOCK(sc);
		return;
	}
	sc->state = FWISOUND_STATE_PROBED;
	FWISOUND_UNLOCK(sc);
}

static void
fwisound_start_task(void *arg, int pending __unused)
{
	struct fwisound_softc *sc = (struct fwisound_softc *)arg;

	if (sc->state == FWISOUND_STATE_DETACHING)
		return;

	fwisound_iso_start(sc);
}

static void
fwisound_stop_task(void *arg, int pending __unused)
{
	struct fwisound_softc *sc = (struct fwisound_softc *)arg;

	fwisound_iso_stop(sc);

	FWISOUND_LOCK(sc);
	if (sc->state != FWISOUND_STATE_DETACHING)
		sc->state = FWISOUND_STATE_PROBED;
	FWISOUND_UNLOCK(sc);
}

int
fwisound_iso_start(struct fwisound_softc *sc)
{
	struct firewire_comm *fc = sc->fd.fc;
	struct fw_xferq *xferq;
	uint32_t val;
	uint8_t speed;
	int dma_ch, err;

	mtx_assert(&sc->mtx, MA_NOTOWNED);

	FWISOUND_LOCK(sc);
	if (sc->dma_ch >= 0 || sc->state == FWISOUND_STATE_STREAMING) {
		FWISOUND_UNLOCK(sc);
		return (0);
	}
	if (sc->state == FWISOUND_STATE_DETACHING || sc->fwdev == NULL) {
		FWISOUND_UNLOCK(sc);
		return (ENXIO);
	}
	speed = min(sc->fwdev->speed, FWSPD_S400);
	FWISOUND_UNLOCK(sc);

	dma_ch = fw_open_isodma(fc, 0);
	if (dma_ch < 0) {
		device_printf(sc->dev, "no IR DMA channel available\n");
		return (EBUSY);
	}

	FWISOUND_LOCK(sc);
	if (sc->dma_ch >= 0) {
		FWISOUND_UNLOCK(sc);
		fc->ir[dma_ch]->flag &= ~FWXFERQ_OPEN;
		return (0);
	}
	FWISOUND_UNLOCK(sc);

	xferq = fc->ir[dma_ch];
	xferq->flag |= FWXFERQ_EXTBUF | FWXFERQ_HANDLER | FWXFERQ_STREAM;

	xferq->flag &= ~FWXFERQ_CHTAGMASK;
	xferq->flag |= sc->iso_channel & FWXFERQ_CHTAGMASK;

	xferq->sc = (caddr_t)sc;
	xferq->hand = fwisound_iso_input;
	xferq->bnchunk = FWISOUND_ISO_NCHUNK;
	xferq->bnpacket = 1;
	xferq->psize = FWISOUND_ISO_PKTSIZE;
	xferq->queued = 0;
	xferq->buf = NULL;

	xferq->bulkxfer = malloc(
	    sizeof(struct fw_bulkxfer) * xferq->bnchunk,
	    M_FWISOUND, M_WAITOK | M_ZERO);

	fw_iso_init_chunks(xferq);

	sc->sample_total = 0;

	val = sc->iso_channel | ((uint32_t)speed << 16);
	err = fwisound_write_quadlet(sc, FWISOUND_REG_ISO_TX_CONFIG, val);
	if (err) {
		device_printf(sc->dev,
		    "failed to set ISO_TX_CONFIG: %d\n", err);
		goto fail;
	}

	err = fwisound_write_quadlet(sc, FWISOUND_REG_AUDIO_ENABLE,
	    FWISOUND_AUDIO_ENABLE);
	if (err) {
		device_printf(sc->dev,
		    "failed to enable audio: %d\n", err);
		goto fail;
	}

	err = fc->irx_enable(fc, dma_ch);
	if (err) {
		device_printf(sc->dev, "failed to start IR DMA: %d\n", err);
		fwisound_write_quadlet(sc, FWISOUND_REG_AUDIO_ENABLE, 0);
		goto fail;
	}

	FWISOUND_LOCK(sc);
	if (sc->state == FWISOUND_STATE_DETACHING) {
		FWISOUND_UNLOCK(sc);
		fc->irx_disable(fc, dma_ch);
		fwisound_write_quadlet(sc, FWISOUND_REG_AUDIO_ENABLE, 0);
		err = ENXIO;
		goto fail;
	}
	sc->dma_ch = dma_ch;
	sc->state = FWISOUND_STATE_STREAMING;
	FWISOUND_UNLOCK(sc);

	return (0);

fail:
	fw_iso_free_chunks(xferq, M_FWISOUND);
	xferq->flag &= ~(FWXFERQ_MODEMASK | FWXFERQ_OPEN | FWXFERQ_STREAM |
	    FWXFERQ_EXTBUF | FWXFERQ_HANDLER | FWXFERQ_CHTAGMASK);
	xferq->hand = NULL;
	sc->dma_ch = -1;
	return (err);
}

void
fwisound_iso_stop(struct fwisound_softc *sc)
{
	struct firewire_comm *fc = sc->fd.fc;
	struct fw_xferq *xferq;
	int dma_ch;

	FWISOUND_LOCK(sc);
	dma_ch = sc->dma_ch;
	if (dma_ch < 0) {
		FWISOUND_UNLOCK(sc);
		return;
	}
	sc->dma_ch = -1;
	FWISOUND_UNLOCK(sc);

	xferq = fc->ir[dma_ch];

	xferq->flag &= ~FWXFERQ_OPEN;
	xferq->hand = NULL;

	if (xferq->flag & FWXFERQ_RUNNING)
		fc->irx_disable(fc, dma_ch);

	if (sc->fwdev != NULL)
		fwisound_write_quadlet(sc, FWISOUND_REG_AUDIO_ENABLE, 0);

	FWISOUND_LOCK(sc);
	fw_iso_wait_inactive_locked(&sc->mtx, &sc->iso_active, "fwisis");
	FWISOUND_UNLOCK(sc);

	xferq->flag &= ~(FWXFERQ_MODEMASK | FWXFERQ_STREAM |
	    FWXFERQ_EXTBUF | FWXFERQ_HANDLER | FWXFERQ_CHTAGMASK);

	fw_iso_free_chunks(xferq, M_FWISOUND);
}

static void
fwisound_iso_input(struct fw_xferq *xferq)
{
	struct fwisound_softc *sc = (struct fwisound_softc *)xferq->sc;
	struct fwisound_chan *chan;
	struct fw_bulkxfer *sxfer;
	struct fw_pkt *fp;
	struct fwisound_payload *pay;
	struct mbuf *m;
	uint8_t *ring;
	uint32_t plen, nbytes, sample_count, ring_size, pos, chunk, old_ptr;
	int dma_ch, need_intr;

	FWISOUND_LOCK(sc);
	dma_ch = sc->dma_ch;
	if (dma_ch < 0) {
		FWISOUND_UNLOCK(sc);
		return;
	}
	sc->iso_active = 1;
	chan = sc->chan;
	FWISOUND_UNLOCK(sc);

	while ((sxfer = STAILQ_FIRST(&xferq->stvalid)) != NULL) {
		STAILQ_REMOVE_HEAD(&xferq->stvalid, link);

		m = fw_iso_dequeue(xferq, sxfer, sc->fd.fc);
		if (m == NULL)
			continue;

		fp = mtod(m, struct fw_pkt *);
		plen = fp->mode.stream.len;
		if (plen < 20 || (uint32_t)m->m_len < 4 + plen) {
			m_freem(m);
			continue;
		}

		pay = (struct fwisound_payload *)(mtod(m, uint8_t *) + 4);
		if (ntohl(pay->signature) != FWISOUND_SIGNATURE) {
			m_freem(m);
			continue;
		}

		sample_count = ntohl(pay->sample_count);
		if (sample_count == 0 || sample_count > FWISOUND_MAX_SAMPLES) {
			m_freem(m);
			continue;
		}

		nbytes = sample_count * 4;
		if (plen < 16 + nbytes) {
			m_freem(m);
			continue;
		}

		if (sc->sample_total != 0 &&
		    ntohl(pay->sample_total) != sc->sample_total)
			sc->dropped++;
		sc->sample_total = ntohl(pay->sample_total) + sample_count;

		need_intr = 0;
		if (chan != NULL && chan->running &&
		    chan->buf != NULL && chan->pcm_chan != NULL) {
			ring = sndbuf_getbufofs(chan->buf, 0);
			ring_size = chan->buf->bufsize;
			old_ptr = chan->hw_ptr;

			uint8_t *src = (uint8_t *)pay->samples;
			uint32_t rem = nbytes;

			while (rem > 0) {
				pos = chan->hw_ptr % ring_size;
				chunk = MIN(rem, ring_size - pos);
				memcpy(ring + pos, src, chunk);
				chan->hw_ptr += chunk;
				src += chunk;
				rem -= chunk;
			}

			if (chan->buf->blksz == 0 ||
			    (old_ptr / chan->buf->blksz) !=
			    (chan->hw_ptr / chan->buf->blksz))
				need_intr = 1;
		}

		m_freem(m);

		if (need_intr) {
			chn_intr(chan->pcm_chan);
		}
	}

	fw_iso_rearm_done(xferq, sc->fd.fc, &sc->mtx, &sc->iso_active,
	    &sc->dma_ch, dma_ch);
}


static int
fwisound_probe(device_t dev)
{
	struct fw_unit *unit;

	unit = fw_get_unit(dev);
	if (unit == NULL)
		return (ENXIO);

	if (unit->spec_id != FWISOUND_SPEC_ID ||
	    unit->sw_version != FWISOUND_VERSION)
		return (ENXIO);

	device_set_desc(dev, "Apple FireWire Audio");
	return (BUS_PROBE_DEFAULT);
}

static int
fwisound_attach(device_t dev)
{
	struct fwisound_softc *sc;
	struct fw_unit *unit;
	uint32_t audio_base;

	unit = fw_get_unit(dev);
	if (unit == NULL || unit->fwdev == NULL)
		return (ENXIO);

	audio_base = fwisound_find_audio_base(unit);
	if (audio_base == 0) {
		device_printf(dev, "no audio base in unit directory\n");
		return (ENXIO);
	}

	sc = device_get_softc(dev);
	sc->fd.dev = dev;
	sc->fd.fc = fw_get_comm(dev);
	sc->dev = dev;
	mtx_init(&sc->mtx, "fwisound", NULL, MTX_DEF);

	sc->fwdev = unit->fwdev;
	sc->cmd_hi = 0xffff;
	sc->cmd_lo = 0xf0000000 | (audio_base << 2);
	sc->state = FWISOUND_STATE_IDLE;
	sc->dma_ch = -1;
	sc->iso_active = 0;
	sc->iso_channel = (uint8_t)(iso_channel & FWXFERQ_CHTAGMASK);
	sc->chan = NULL;
	TASK_INIT(&sc->probe_task, 0, fwisound_probe_task, sc);
	TASK_INIT(&sc->start_task, 0, fwisound_start_task, sc);
	TASK_INIT(&sc->stop_task, 0, fwisound_stop_task, sc);

	sc->pcm_dev = device_add_child(dev, "pcm", DEVICE_UNIT_ANY);
	if (sc->pcm_dev == NULL) {
		device_printf(dev, "failed to add pcm child\n");
		mtx_destroy(&sc->mtx);
		return (ENXIO);
	}

	bus_attach_children(dev);

	if (taskqueue_enqueue(taskqueue_thread, &sc->probe_task) != 0)
		device_printf(dev, "failed to enqueue probe task\n");

	return (0);
}

static int
fwisound_detach(device_t dev)
{
	struct fwisound_softc *sc;

	sc = device_get_softc(dev);

	FWISOUND_LOCK(sc);
	sc->state = FWISOUND_STATE_DETACHING;
	FWISOUND_UNLOCK(sc);

	taskqueue_drain(taskqueue_thread, &sc->probe_task);
	taskqueue_drain(taskqueue_thread, &sc->start_task);
	taskqueue_drain(taskqueue_thread, &sc->stop_task);
	fwisound_iso_stop(sc);

	if (sc->pcm_dev != NULL)
		device_delete_children(dev);

	FWISOUND_LOCK(sc);
	sc->fwdev = NULL;
	FWISOUND_UNLOCK(sc);

	mtx_destroy(&sc->mtx);
	return (0);
}

static device_method_t fwisound_methods[] = {
	DEVMETHOD(device_probe,		fwisound_probe),
	DEVMETHOD(device_attach,	fwisound_attach),
	DEVMETHOD(device_detach,	fwisound_detach),

	DEVMETHOD_END
};

static driver_t fwisound_driver = {
	"fwisound",
	fwisound_methods,
	sizeof(struct fwisound_softc),
};

DRIVER_MODULE(fwisound, firewire, fwisound_driver, 0, 0);
MODULE_VERSION(fwisound, 1);
MODULE_DEPEND(fwisound, firewire, 1, 1, 1);
MODULE_DEPEND(fwisound, sound, SOUND_MINVER, SOUND_PREFVER, SOUND_MAXVER);
