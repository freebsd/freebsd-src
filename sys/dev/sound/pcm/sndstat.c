/*
 * Copyright (c) 2001 Cameron Grant <gandalf@vilnya.demon.co.uk>
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
 * $FreeBSD$
 */

#include <dev/sound/pcm/sound.h>
#include <dev/sound/pcm/vchan.h>
#include <sys/sbuf.h>

#include "feeder_if.h"

static d_open_t sndstat_open;
static d_close_t sndstat_close;
static d_read_t sndstat_read;

static struct cdevsw sndstat_cdevsw = {
	/* open */	sndstat_open,
	/* close */	sndstat_close,
	/* read */	sndstat_read,
	/* write */	nowrite,
	/* ioctl */	noioctl,
	/* poll */	nopoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"sndstat",
	/* maj */	SND_CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
};

static struct sbuf sndstat_sbuf;
static dev_t sndstat_dev = 0;
static int sndstat_isopen = 0;
static int sndstat_bufptr;

static int sndstat_verbose = 0;
TUNABLE_INT("hw.snd.verbose", &sndstat_verbose);

static int sndstat_prepare(struct sbuf *s);

static int
sysctl_hw_sndverbose(SYSCTL_HANDLER_ARGS)
{
	int error, verbose;

	verbose = sndstat_verbose;
	error = sysctl_handle_int(oidp, &verbose, sizeof(verbose), req);
	if (error == 0 && req->newptr != NULL) {
		if (verbose == 0 || verbose == 1)
			sndstat_verbose = verbose;
		else
			error = EINVAL;
	}
	return error;
}
SYSCTL_PROC(_hw_snd, OID_AUTO, verbose, CTLTYPE_INT | CTLFLAG_RW,
            0, sizeof(int), sysctl_hw_sndverbose, "I", "");

static int
sndstat_open(dev_t i_dev, int flags, int mode, struct proc *p)
{
	intrmask_t s;
	int err;

	s = spltty();
	if (sndstat_isopen) {
		splx(s);
		return EBUSY;
	}
	if (sbuf_new(&sndstat_sbuf, NULL, 4096, 0) == NULL) {
		splx(s);
		return ENXIO;
	}
	sndstat_bufptr = 0;
	err = (sndstat_prepare(&sndstat_sbuf) > 0)? 0 : ENOMEM;
	if (!err)
		sndstat_isopen = 1;

	splx(s);
	return err;
}

static int
sndstat_close(dev_t i_dev, int flags, int mode, struct proc *p)
{
	intrmask_t s;

	s = spltty();
	if (!sndstat_isopen) {
		splx(s);
		return EBADF;
	}
	sbuf_delete(&sndstat_sbuf);
	sndstat_isopen = 0;

	splx(s);
	return 0;
}

static int
sndstat_read(dev_t i_dev, struct uio *buf, int flag)
{
	intrmask_t s;
	int l, err;

	s = spltty();
	if (!sndstat_isopen) {
		splx(s);
		return EBADF;
	}
    	l = min(buf->uio_resid, sbuf_len(&sndstat_sbuf) - sndstat_bufptr);
	err = (l > 0)? uiomove(sbuf_data(&sndstat_sbuf) + sndstat_bufptr, l, buf) : 0;
	sndstat_bufptr += l;

	splx(s);
	return err;
}

static int
sndstat_prepare(struct sbuf *s)
{
    	int i, pc, rc, vc;
    	device_t dev;
    	struct snddev_info *d;
    	struct snddev_channel *sce;
	struct pcm_channel *c;
	struct pcm_feeder *f;

	sbuf_printf(s, "FreeBSD Audio Driver (newpcm) %s %s\n", __DATE__, __TIME__);
	if (!pcm_devclass || devclass_get_maxunit(pcm_devclass) == 0) {
		sbuf_printf(s, "No devices installed.\n");
		sbuf_finish(s);
    		return sbuf_len(s);
	} else
		sbuf_printf(s, "Installed devices:\n");

    	for (i = 0; i < devclass_get_maxunit(pcm_devclass); i++) {
		d = devclass_get_softc(pcm_devclass, i);
		if (!d)
			continue;
		snd_mtxlock(d->lock);
		dev = devclass_get_device(pcm_devclass, i);
		sbuf_printf(s, "pcm%d: <%s> %s", i, device_get_desc(dev), d->status);
		if (!SLIST_EMPTY(&d->channels)) {
			pc = rc = vc = 0;
			SLIST_FOREACH(sce, &d->channels, link) {
				c = sce->channel;
				if (c->direction == PCMDIR_PLAY) {
					if (c->flags & CHN_F_VIRTUAL)
						vc++;
					else
						pc++;
				} else
					rc++;
			}
			sbuf_printf(s, " (%dp/%dr/%dv channels%s%s)\n", pc, rc, vc,
					(d->flags & SD_F_SIMPLEX)? "" : " duplex",
#ifdef USING_DEVFS
					(i == snd_unit)? " default" : ""
#else
					""
#endif
					);
			if (!sndstat_verbose)
				goto skipverbose;
			SLIST_FOREACH(sce, &d->channels, link) {
				c = sce->channel;
				sbuf_printf(s, "\t%s[%s]: speed %d, format %08x, flags %08x",
					c->parentchannel? c->parentchannel->name : "",
					c->name, c->speed, c->format, c->flags);
				if (c->pid != -1)
					sbuf_printf(s, ", pid %d", c->pid);
				sbuf_printf(s, "\n\t");
				f = c->feeder;
				while (f) {
					sbuf_printf(s, "%s", f->class->name);
					if (f->desc->type == FEEDER_FMT)
						sbuf_printf(s, "(%08x <- %08x)", f->desc->out, f->desc->in);
					if (f->desc->type == FEEDER_RATE)
						sbuf_printf(s, "(%d <- %d)", FEEDER_GET(f, FEEDRATE_DST), FEEDER_GET(f, FEEDRATE_SRC));
					if (f->desc->type == FEEDER_ROOT || f->desc->type == FEEDER_MIXER)
						sbuf_printf(s, "(%08x)", f->desc->out);
					if (f->source)
						sbuf_printf(s, " <- ");
					f = f->source;
				}
				sbuf_printf(s, "\n");
			}
skipverbose:
		} else
			sbuf_printf(s, " (mixer only)\n");
		snd_mtxunlock(d->lock);
    	}
	sbuf_finish(s);
    	return sbuf_len(s);
}

static int
sndstat_init(void)
{
	sndstat_dev = make_dev(&sndstat_cdevsw, SND_DEV_STATUS, UID_ROOT, GID_WHEEL, 0444, "sndstat");

	return (sndstat_dev != 0)? 0 : ENXIO;
}

static int
sndstat_uninit(void)
{
	intrmask_t s;

	s = spltty();
	if (sndstat_isopen) {
		splx(s);
		return EBUSY;
	}

	if (sndstat_dev)
		destroy_dev(sndstat_dev);
	sndstat_dev = 0;

	splx(s);
	return 0;
}

static void
sndstat_sysinit(void *p)
{
	sndstat_init();
}

static void
sndstat_sysuninit(void *p)
{
	sndstat_uninit();
}

SYSINIT(sndstat_sysinit, SI_SUB_DRIVERS, SI_ORDER_MIDDLE, sndstat_sysinit, NULL);
SYSUNINIT(sndstat_sysuninit, SI_SUB_DRIVERS, SI_ORDER_MIDDLE, sndstat_sysuninit, NULL);


