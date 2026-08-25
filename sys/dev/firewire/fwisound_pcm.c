/*
 * Copyright (c) 2026 Abdelkader Boudih <freebsd@seuros.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/* newpcm channel integration for fwisound(4). */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/taskqueue.h>

#include <dev/firewire/firewire.h>
#include <dev/firewire/firewirereg.h>
#include <dev/firewire/fwisound.h>

#include <dev/sound/pcm/sound.h>

#define FWISOUND_BUFSZ	65536


static void *
fwisound_chan_init(kobj_t obj, void *devinfo, struct snd_dbuf *b,
    struct pcm_channel *c, int dir)
{
	struct fwisound_softc *sc = devinfo;
	struct fwisound_chan *chan;
	void *buf;
	unsigned int bufsz;

	if (dir != PCMDIR_REC)
		return (NULL);

	chan = malloc(sizeof(*chan), M_DEVBUF, M_WAITOK | M_ZERO);
	chan->sc = sc;
	chan->pcm_chan = c;
	chan->buf = b;

	chan->fmts[0] = SND_FORMAT(AFMT_S16_BE, 2, 0);
	chan->fmts[1] = 0;

	chan->caps.minspeed = 48000;
	chan->caps.maxspeed = 48000;
	chan->caps.fmtlist = chan->fmts;
	chan->caps.caps = 0;

	bufsz = pcm_getbuffersize(sc->pcm_dev, 4096, FWISOUND_BUFSZ, 131072);
	buf = malloc(bufsz, M_DEVBUF, M_WAITOK | M_ZERO);
	if (sndbuf_setup(b, buf, bufsz) != 0) {
		free(buf, M_DEVBUF);
		free(chan, M_DEVBUF);
		return (NULL);
	}

	FWISOUND_LOCK(sc);
	sc->chan = chan;
	FWISOUND_UNLOCK(sc);

	return (chan);
}

static int
fwisound_chan_free(kobj_t obj, void *data)
{
	struct fwisound_chan *chan = data;
	struct fwisound_softc *sc = chan->sc;
	void *buf;

	if (chan->running) {
		taskqueue_enqueue(taskqueue_thread, &sc->stop_task);
		taskqueue_drain(taskqueue_thread, &sc->stop_task);
	}

	FWISOUND_LOCK(sc);
	sc->chan = NULL;
	FWISOUND_UNLOCK(sc);

	buf = sndbuf_getbufofs(chan->buf, 0);
	if (buf != NULL)
		free(buf, M_DEVBUF);

	free(chan, M_DEVBUF);
	return (0);
}

static int
fwisound_chan_setformat(kobj_t obj, void *data, uint32_t fmt)
{

	if (fmt != SND_FORMAT(AFMT_S16_BE, 2, 0))
		return (EINVAL);
	return (0);
}

static uint32_t
fwisound_chan_setspeed(kobj_t obj, void *data, uint32_t speed)
{

	return (48000);
}

static uint32_t
fwisound_chan_setblocksize(kobj_t obj, void *data, uint32_t blksz)
{
	struct fwisound_chan *chan = data;

	blksz = rounddown(blksz, 4);
	if (blksz < 4)
		blksz = 4;

	sndbuf_resize(chan->buf, chan->buf->bufsize / blksz, blksz);

	return (chan->buf->blksz);
}

static int
fwisound_chan_trigger(kobj_t obj, void *data, int go)
{
	struct fwisound_chan *chan = data;
	struct fwisound_softc *sc = chan->sc;

	switch (go) {
	case PCMTRIG_START:
		chan->hw_ptr = 0;
		sc->dropped = 0;
		sc->sample_total = 0;
		chan->running = 1;
		taskqueue_enqueue(taskqueue_thread, &sc->start_task);
		return (0);

	case PCMTRIG_STOP:
	case PCMTRIG_ABORT:
		chan->running = 0;
		taskqueue_drain(taskqueue_thread, &sc->start_task);
		taskqueue_enqueue(taskqueue_thread, &sc->stop_task);
		return (0);

	default:
		return (0);
	}
}

static uint32_t
fwisound_chan_getptr(kobj_t obj, void *data)
{
	struct fwisound_chan *chan = data;

	return (chan->hw_ptr % chan->buf->bufsize);
}

static struct pcmchan_caps *
fwisound_chan_getcaps(kobj_t obj, void *data)
{
	struct fwisound_chan *chan = data;

	return (&chan->caps);
}

static kobj_method_t fwisound_chan_methods[] = {
	KOBJMETHOD(channel_init,	fwisound_chan_init),
	KOBJMETHOD(channel_free,	fwisound_chan_free),
	KOBJMETHOD(channel_setformat,	fwisound_chan_setformat),
	KOBJMETHOD(channel_setspeed,	fwisound_chan_setspeed),
	KOBJMETHOD(channel_setblocksize, fwisound_chan_setblocksize),
	KOBJMETHOD(channel_trigger,	fwisound_chan_trigger),
	KOBJMETHOD(channel_getptr,	fwisound_chan_getptr),
	KOBJMETHOD(channel_getcaps,	fwisound_chan_getcaps),
	KOBJMETHOD_END
};

CHANNEL_DECLARE(fwisound_chan);

static int
fwisound_pcm_probe(device_t dev)
{

	device_set_desc(dev, "Apple FireWire audio");
	return (0);
}

static int
fwisound_pcm_attach(device_t dev)
{
	struct fwisound_softc *sc;
	char status[SND_STATUSLEN];
	int err;

	sc = device_get_softc(device_get_parent(dev));
	sc->pcm_dev = dev;

	pcm_init(dev, sc);
	pcm_setflags(dev, pcm_getflags(dev) | SD_F_MPSAFE);

	err = pcm_addchan(dev, PCMDIR_REC, &fwisound_chan_class, sc);
	if (err != 0) {
		device_printf(dev, "pcm_addchan failed: %d\n", err);
		return (err);
	}

	snprintf(status, sizeof(status), "Apple FireWire audio at %s",
	    device_get_nameunit(device_get_parent(dev)));
	return (pcm_register(dev, status));
}

static int
fwisound_pcm_detach(device_t dev)
{

	return (pcm_unregister(dev));
}

static device_method_t fwisound_pcm_methods[] = {
	DEVMETHOD(device_probe,		fwisound_pcm_probe),
	DEVMETHOD(device_attach,	fwisound_pcm_attach),
	DEVMETHOD(device_detach,	fwisound_pcm_detach),

	DEVMETHOD_END
};

static driver_t fwisound_pcm_driver = {
	"pcm",
	fwisound_pcm_methods,
	PCM_SOFTC_SIZE,
};

DRIVER_MODULE(fwisound_pcm, fwisound, fwisound_pcm_driver, 0, 0);
MODULE_DEPEND(fwisound_pcm, fwisound, 1, 1, 1);
MODULE_DEPEND(fwisound_pcm, sound, SOUND_MINVER, SOUND_PREFVER, SOUND_MAXVER);
