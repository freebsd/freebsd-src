/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 The FreeBSD Foundation
 *
 * This software was developed by Christos Margiolis <christos@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
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
 */

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>

#ifdef HAVE_KERNEL_OPTION_HEADERS
#include "opt_snd.h"
#endif

#include <dev/sound/pcm/sound.h>
#include <mixer_if.h>

#define DUMMY_NPCHAN	1
#define DUMMY_NRCHAN	1
#define DUMMY_NCHAN	(DUMMY_NPCHAN + DUMMY_NRCHAN)

struct dummy_chan {
	struct dummy_softc *sc;
	struct pcm_channel *chan;
	struct snd_dbuf *buf;
	struct pcmchan_caps *caps;
	uint32_t ptr;
	int dir;
	int run;
};

struct dummy_softc {
	struct snddev_info info;
	device_t dev;
	uint32_t cap_fmts[4];
	struct pcmchan_caps caps;
	int chnum;
	struct dummy_chan chans[DUMMY_NCHAN];
	struct callout callout;
	struct mtx *lock;
};

static void
dummy_chan_io(void *arg)
{
	struct dummy_softc *sc = arg;
	struct dummy_chan *ch;
	int i = 0;

	snd_mtxlock(sc->lock);

	for (i = 0; i < sc->chnum; i++) {
		ch = &sc->chans[i];
		if (!ch->run)
			continue;
		if (ch->dir == PCMDIR_PLAY)
			ch->ptr += sndbuf_getblksz(ch->buf);
		else
			sndbuf_fillsilence(ch->buf);
		snd_mtxunlock(sc->lock);
		chn_intr(ch->chan);
		snd_mtxlock(sc->lock);
	}
	callout_schedule(&sc->callout, 1);

	snd_mtxunlock(sc->lock);
}

static int
dummy_chan_free(kobj_t obj, void *data)
{
	struct dummy_chan *ch =data;
	uint8_t *buf;

	buf = sndbuf_getbuf(ch->buf);
	if (buf != NULL)
		free(buf, M_DEVBUF);

	return (0);
}

static void *
dummy_chan_init(kobj_t obj, void *devinfo, struct snd_dbuf *b,
    struct pcm_channel *c, int dir)
{
	struct dummy_softc *sc;
	struct dummy_chan *ch;
	uint8_t *buf;
	size_t bufsz;

	sc = devinfo;

	snd_mtxlock(sc->lock);

	ch = &sc->chans[sc->chnum++];
	ch->sc = sc;
	ch->dir = dir;
	ch->chan = c;
	ch->buf = b;
	ch->caps = &sc->caps;

	snd_mtxunlock(sc->lock);

	bufsz = pcm_getbuffersize(sc->dev, 2048, 2048, 65536);
	buf = malloc(bufsz, M_DEVBUF, M_WAITOK | M_ZERO);
	if (sndbuf_setup(ch->buf, buf, bufsz) != 0) {
		dummy_chan_free(obj, ch);
		return (NULL);
	}

	return (ch);
}

static int
dummy_chan_setformat(kobj_t obj, void *data, uint32_t format)
{
	struct dummy_chan *ch = data;
	int i;

	for (i = 0; ch->caps->fmtlist[i]; i++)
		if (format == ch->caps->fmtlist[i])
			return (0);

	return (EINVAL);
}

static uint32_t
dummy_chan_setspeed(kobj_t obj, void *data, uint32_t speed)
{
	struct dummy_chan *ch = data;

	RANGE(speed, ch->caps->minspeed, ch->caps->maxspeed);

	return (speed);
}

static uint32_t
dummy_chan_setblocksize(kobj_t obj, void *data, uint32_t blocksize)
{
	struct dummy_chan *ch = data;

	return (sndbuf_getblksz(ch->buf));
}

static int
dummy_chan_trigger(kobj_t obj, void *data, int go)
{
	struct dummy_chan *ch = data;
	struct dummy_softc *sc = ch->sc;

	snd_mtxlock(sc->lock);

	switch (go) {
	case PCMTRIG_START:
		if (!callout_active(&sc->callout))
			callout_reset(&sc->callout, 1, dummy_chan_io, sc);
		ch->ptr = 0;
		ch->run = 1;
		break;
	case PCMTRIG_STOP:
	case PCMTRIG_ABORT:
		ch->run = 0;
		if (callout_active(&sc->callout))
			callout_stop(&sc->callout);
	default:
		break;
	}

	snd_mtxunlock(sc->lock);

	return (0);
}

static uint32_t
dummy_chan_getptr(kobj_t obj, void *data)
{
	struct dummy_chan *ch = data;

	return (ch->run ? ch->ptr : 0);
}

static struct pcmchan_caps *
dummy_chan_getcaps(kobj_t obj, void *data)
{
	struct dummy_chan *ch = data;

	return (ch->caps);
}

static kobj_method_t dummy_chan_methods[] = {
	KOBJMETHOD(channel_init,	dummy_chan_init),
	KOBJMETHOD(channel_free,	dummy_chan_free),
	KOBJMETHOD(channel_setformat,	dummy_chan_setformat),
	KOBJMETHOD(channel_setspeed,	dummy_chan_setspeed),
	KOBJMETHOD(channel_setblocksize,dummy_chan_setblocksize),
	KOBJMETHOD(channel_trigger,	dummy_chan_trigger),
	KOBJMETHOD(channel_getptr,	dummy_chan_getptr),
	KOBJMETHOD(channel_getcaps,	dummy_chan_getcaps),
	KOBJMETHOD_END
};

CHANNEL_DECLARE(dummy_chan);

static int
dummy_mixer_init(struct snd_mixer *m)
{
	struct dummy_softc *sc;

	sc = mix_getdevinfo(m);
	if (sc == NULL)
		return (-1);

	pcm_setflags(sc->dev, pcm_getflags(sc->dev) | SD_F_SOFTPCMVOL);
	mix_setdevs(m, SOUND_MASK_PCM | SOUND_MASK_VOLUME | SOUND_MASK_RECLEV);
	mix_setrecdevs(m, SOUND_MASK_RECLEV);

	return (0);
}

static int
dummy_mixer_set(struct snd_mixer *m, unsigned dev, unsigned left, unsigned right)
{
	return (0);
}

static uint32_t
dummy_mixer_setrecsrc(struct snd_mixer *m, uint32_t src)
{
	return (src == SOUND_MASK_RECLEV ? src : 0);
}

static kobj_method_t dummy_mixer_methods[] = {
	KOBJMETHOD(mixer_init,		dummy_mixer_init),
	KOBJMETHOD(mixer_set,		dummy_mixer_set),
	KOBJMETHOD(mixer_setrecsrc,	dummy_mixer_setrecsrc),
	KOBJMETHOD_END
};

MIXER_DECLARE(dummy_mixer);

static void
dummy_identify(driver_t *driver, device_t parent)
{
	if (device_find_child(parent, driver->name, -1) != NULL)
		return;
	if (BUS_ADD_CHILD(parent, 0, driver->name, -1) == NULL)
		device_printf(parent, "add child failed\n");
}

static int
dummy_probe(device_t dev)
{
	device_set_desc(dev, "Dummy Audio Device");

	return (0);
}

static int
dummy_attach(device_t dev)
{
	struct dummy_softc *sc;
	char status[SND_STATUSLEN];
	int i = 0;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->lock = snd_mtxcreate(device_get_nameunit(dev), "snd_dummy softc");

	sc->cap_fmts[0] = SND_FORMAT(AFMT_S32_LE, 2, 0);
	sc->cap_fmts[1] = SND_FORMAT(AFMT_S24_LE, 2, 0);
	sc->cap_fmts[2] = SND_FORMAT(AFMT_S16_LE, 2, 0);
	sc->cap_fmts[3] = 0;
	sc->caps = (struct pcmchan_caps){
		8000,		/* minspeed */
		96000,		/* maxspeed */
		sc->cap_fmts,	/* fmtlist */
		0,		/* caps */
	};

	pcm_setflags(dev, pcm_getflags(dev) | SD_F_MPSAFE);
	if (pcm_register(dev, sc, DUMMY_NPCHAN, DUMMY_NRCHAN))
		return (ENXIO);
	for (i = 0; i < DUMMY_NPCHAN; i++)
		pcm_addchan(dev, PCMDIR_PLAY, &dummy_chan_class, sc);
	for (i = 0; i < DUMMY_NRCHAN; i++)
		pcm_addchan(dev, PCMDIR_REC, &dummy_chan_class, sc);

	snprintf(status, SND_STATUSLEN, "on %s",
	    device_get_nameunit(device_get_parent(dev)));
	pcm_setstatus(dev, status);
	mixer_init(dev, &dummy_mixer_class, sc);
	callout_init(&sc->callout, 1);

	return (0);
}

static int
dummy_detach(device_t dev)
{
	struct dummy_softc *sc = device_get_softc(dev);
	int err;

	callout_drain(&sc->callout);
	err = pcm_unregister(dev);
	snd_mtxfree(sc->lock);

	return (err);
}

static device_method_t dummy_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	dummy_identify),
	DEVMETHOD(device_probe,		dummy_probe),
	DEVMETHOD(device_attach,	dummy_attach),
	DEVMETHOD(device_detach,	dummy_detach),
	DEVMETHOD_END
};

static driver_t dummy_driver = {
	"pcm",
	dummy_methods,
	sizeof(struct dummy_softc),
};

DRIVER_MODULE(snd_dummy, nexus, dummy_driver, 0, 0);
MODULE_DEPEND(snd_dummy, sound, SOUND_MINVER, SOUND_PREFVER, SOUND_MAXVER);
MODULE_VERSION(snd_dummy, 1);
