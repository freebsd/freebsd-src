/*
 * Copyright (c) 1999 Cameron Grant <gandalf@vilnya.demon.co.uk>
 * (C) 1997 Luigi Rizzo (luigi@iet.unipi.it)
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
#include <sys/sysctl.h>
#include <sys/sbuf.h>

#include "feeder_if.h"

#undef	SNDSTAT_VERBOSE

static dev_t status_dev = 0;
static int do_status(int action, struct uio *buf);

static d_open_t sndopen;
static d_close_t sndclose;
static d_ioctl_t sndioctl;
static d_read_t sndread;
static d_write_t sndwrite;
static d_mmap_t sndmmap;
static d_poll_t sndpoll;

#define CDEV_MAJOR 30
static struct cdevsw snd_cdevsw = {
	/* open */	sndopen,
	/* close */	sndclose,
	/* read */	sndread,
	/* write */	sndwrite,
	/* ioctl */	sndioctl,
	/* poll */	sndpoll,
	/* mmap */	sndmmap,
	/* strategy */	nostrategy,
	/* name */	"snd",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	D_TRACKCLOSE,
};

/*
PROPOSAL:
each unit needs:
status, mixer, dsp, dspW, audio, sequencer, midi-in, seq2, sndproc = 9 devices
dspW and audio are deprecated.
dsp needs min 64 channels, will give it 256

minor = (unit << 20) + (dev << 16) + channel
currently minor = (channel << 16) + (unit << 4) + dev

nomenclature:
	/dev/pcmX/dsp.(0..255)
	/dev/pcmX/dspW
	/dev/pcmX/audio
	/dev/pcmX/status
	/dev/pcmX/mixer
	[etc.]
*/

#define PCMMINOR(x) (minor(x))
#define PCMCHAN(x) ((PCMMINOR(x) & 0x00ff0000) >> 16)
#define PCMUNIT(x) ((PCMMINOR(x) & 0x000000f0) >> 4)
#define PCMDEV(x)   (PCMMINOR(x) & 0x0000000f)
#define PCMMKMINOR(u, d, c) ((((c) & 0xff) << 16) | (((u) & 0x0f) << 4) | ((d) & 0x0f))

static devclass_t pcm_devclass;

#ifdef USING_DEVFS
static int snd_unit = 0;
TUNABLE_INT("hw.snd.unit", &snd_unit);
#endif

SYSCTL_NODE(_hw, OID_AUTO, snd, CTLFLAG_RD, 0, "Sound driver");

void *
snd_mtxcreate(const char *desc)
{
#ifdef USING_MUTEX
	struct mtx *m;

	m = malloc(sizeof(*m), M_DEVBUF, M_WAITOK | M_ZERO);
	if (m == NULL)
		return NULL;
	mtx_init(m, desc, MTX_RECURSE);
	return m;
#else
	return (void *)0xcafebabe;
#endif
}

void
snd_mtxfree(void *m)
{
#ifdef USING_MUTEX
	struct mtx *mtx = m;

	mtx_assert(mtx, MA_OWNED);
	mtx_destroy(mtx);
	free(mtx, M_DEVBUF);
#endif
}

void
snd_mtxassert(void *m)
{
#ifdef USING_MUTEX
	struct mtx *mtx = m;

	mtx_assert(mtx, MA_OWNED);
#endif
}

void
snd_mtxlock(void *m)
{
#ifdef USING_MUTEX
	struct mtx *mtx = m;

	mtx_lock(mtx);
#endif
}

void
snd_mtxunlock(void *m)
{
#ifdef USING_MUTEX
	struct mtx *mtx = m;

	mtx_unlock(mtx);
#endif
}

int
snd_setup_intr(device_t dev, struct resource *res, int flags, driver_intr_t hand, void *param, void **cookiep)
{
#ifdef USING_MUTEX
	flags &= INTR_MPSAFE;
	flags |= INTR_TYPE_TTY;
#else
	flags = INTR_TYPE_TTY;
#endif
	return bus_setup_intr(dev, res, flags, hand, param, cookiep);
}

struct pcm_channel *
pcm_chnalloc(struct snddev_info *d, int direction)
{
	struct pcm_channel *c;
    	struct snddev_channel *sce;

	snd_mtxlock(d->lock);
	SLIST_FOREACH(sce, &d->channels, link) {
		c = sce->channel;
		CHN_LOCK(c);
		if ((c->direction == direction) && !(c->flags & CHN_F_BUSY)) {
			c->flags |= CHN_F_BUSY;
			CHN_UNLOCK(c);
			snd_mtxunlock(d->lock);
			return c;
		}
		CHN_UNLOCK(c);
	}
	snd_mtxunlock(d->lock);
	return NULL;
}

int
pcm_chnfree(struct pcm_channel *c)
{
	CHN_LOCK(c);
	c->flags &= ~CHN_F_BUSY;
	CHN_UNLOCK(c);
	return 0;
}

int
pcm_chnref(struct pcm_channel *c, int ref)
{
	int r;

	CHN_LOCK(c);
	c->refcount += ref;
	r = c->refcount;
	CHN_UNLOCK(c);
	return r;
}

#ifdef USING_DEVFS
static void
pcm_makelinks(void *dummy)
{
	int unit;
	dev_t pdev;
	static dev_t dsp = 0, dspW = 0, audio = 0, mixer = 0;
    	struct snddev_info *d;

	if (pcm_devclass == NULL || devfs_present == 0)
		return;
	if (dsp) {
		destroy_dev(dsp);
		dsp = 0;
	}
	if (dspW) {
		destroy_dev(dspW);
		dspW = 0;
	}
	if (audio) {
		destroy_dev(audio);
		audio = 0;
	}
	if (mixer) {
		destroy_dev(mixer);
		mixer = 0;
	}

	unit = snd_unit;
	if (unit < 0 || unit > devclass_get_maxunit(pcm_devclass))
		return;
	d = devclass_get_softc(pcm_devclass, unit);
	if (d == NULL || d->chancount == 0)
		return;

	pdev = makedev(CDEV_MAJOR, PCMMKMINOR(unit, SND_DEV_DSP, 0));
	dsp = make_dev_alias(pdev, "dsp");
	pdev = makedev(CDEV_MAJOR, PCMMKMINOR(unit, SND_DEV_DSP16, 0));
	dspW = make_dev_alias(pdev, "dspW");
	pdev = makedev(CDEV_MAJOR, PCMMKMINOR(unit, SND_DEV_AUDIO, 0));
	audio = make_dev_alias(pdev, "audio");
	pdev = makedev(CDEV_MAJOR, PCMMKMINOR(unit, SND_DEV_CTL, 0));
	mixer = make_dev_alias(pdev, "mixer");
}

static int
sysctl_hw_sndunit(SYSCTL_HANDLER_ARGS)
{
	int error, unit;

	unit = snd_unit;
	error = sysctl_handle_int(oidp, &unit, sizeof(unit), req);
	if (error == 0 && req->newptr != NULL) {
		snd_unit = unit;
		pcm_makelinks(NULL);
	}
	return (error);
}
SYSCTL_PROC(_hw_snd, OID_AUTO, unit, CTLTYPE_INT | CTLFLAG_RW,
            0, sizeof(int), sysctl_hw_sndunit, "I", "");
#endif

struct pcm_channel *
pcm_chn_create(struct snddev_info *d, struct pcm_channel *parent, kobj_class_t cls, int dir, void *devinfo)
{
	struct pcm_channel *ch;
	char *dirs;
    	int err;

	switch(dir) {
	case PCMDIR_PLAY:
		dirs = "play";
		break;
	case PCMDIR_REC:
		dirs = "record";
		break;
	case PCMDIR_VIRTUAL:
		dirs = "virtual";
		dir = PCMDIR_PLAY;
		break;
	default:
		return NULL;
	}

	ch = malloc(sizeof(*ch), M_DEVBUF, M_WAITOK | M_ZERO);
	if (!ch)
		return NULL;

	ch->methods = kobj_create(cls, M_DEVBUF, M_WAITOK);
	if (!ch->methods) {
		free(ch, M_DEVBUF);
		return NULL;
	}

	ch->pid = -1;
	ch->parentsnddev = d;
	ch->parentchannel = parent;
	snprintf(ch->name, 32, "%s:%d:%s", device_get_nameunit(d->dev), d->chancount, dirs);

	err = chn_init(ch, devinfo, dir);
	if (err) {
		device_printf(d->dev, "chn_init() for channel %d (%s) failed: err = %d\n", d->chancount, dirs, err);
		kobj_delete(ch->methods, M_DEVBUF);
		free(ch, M_DEVBUF);
		return NULL;
	}

	return ch;
}

int
pcm_chn_destroy(struct pcm_channel *ch)
{
	int err;

	err = chn_kill(ch);
	if (err) {
		device_printf(ch->parentsnddev->dev, "chn_kill() for %s failed, err = %d\n", ch->name, err);
		return err;
	}

	kobj_delete(ch->methods, M_DEVBUF);
	free(ch, M_DEVBUF);

	return 0;
}

int
pcm_chn_add(struct snddev_info *d, struct pcm_channel *ch)
{
    	struct snddev_channel *sce;
    	int unit = device_get_unit(d->dev);

	sce = malloc(sizeof(*sce), M_DEVBUF, M_WAITOK | M_ZERO);
	if (!sce) {
		free(ch, M_DEVBUF);
		return ENOMEM;
	}

	snd_mtxlock(d->lock);
	sce->channel = ch;
	SLIST_INSERT_HEAD(&d->channels, sce, link);

	make_dev(&snd_cdevsw, PCMMKMINOR(unit, SND_DEV_DSP, d->chancount),
		 UID_ROOT, GID_WHEEL, 0666, "dsp%d.%d", unit, d->chancount);
	make_dev(&snd_cdevsw, PCMMKMINOR(unit, SND_DEV_DSP16, d->chancount),
		 UID_ROOT, GID_WHEEL, 0666, "dspW%d.%d", unit, d->chancount);
	make_dev(&snd_cdevsw, PCMMKMINOR(unit, SND_DEV_AUDIO, d->chancount),
		 UID_ROOT, GID_WHEEL, 0666, "audio%d.%d", unit, d->chancount);
	/* XXX SND_DEV_NORESET? */

#ifdef USING_DEVFS
    	if (d->chancount++ == 0)
		pcm_makelinks(NULL);
#endif
	snd_mtxunlock(d->lock);

	return 0;
}

int
pcm_chn_remove(struct snddev_info *d, struct pcm_channel *ch)
{
    	struct snddev_channel *sce;
    	int unit = device_get_unit(d->dev);
	dev_t pdev;

	snd_mtxlock(d->lock);
	SLIST_FOREACH(sce, &d->channels, link) {
		if (sce->channel == ch)
			goto gotit;
	}
	snd_mtxunlock(d->lock);
	return EINVAL;
gotit:
	d->chancount--;
	SLIST_REMOVE(&d->channels, sce, snddev_channel, link);
	free(sce, M_DEVBUF);

#ifdef USING_DEVFS
    	if (d->chancount == 0)
		pcm_makelinks(NULL);
#endif
	pdev = makedev(CDEV_MAJOR, PCMMKMINOR(unit, SND_DEV_DSP, d->chancount));
	destroy_dev(pdev);
	pdev = makedev(CDEV_MAJOR, PCMMKMINOR(unit, SND_DEV_DSP16, d->chancount));
	destroy_dev(pdev);
	pdev = makedev(CDEV_MAJOR, PCMMKMINOR(unit, SND_DEV_AUDIO, d->chancount));
	destroy_dev(pdev);
	snd_mtxunlock(d->lock);

	return 0;
}

int
pcm_addchan(device_t dev, int dir, kobj_class_t cls, void *devinfo)
{
    	struct snddev_info *d = device_get_softc(dev);
	struct pcm_channel *ch;
    	int err;

	ch = pcm_chn_create(d, NULL, cls, dir, devinfo);
	if (!ch) {
		device_printf(d->dev, "pcm_chn_create(%s, %d, %p) failed\n", cls->name, dir, devinfo);
		return ENODEV;
	}
	err = pcm_chn_add(d, ch);
	if (err) {
		device_printf(d->dev, "pcm_chn_add(%s) failed, err=%d\n", ch->name, err);
		pcm_chn_destroy(ch);
	}

	return err;
}

static int
pcm_killchan(device_t dev)
{
    	struct snddev_info *d = device_get_softc(dev);
    	struct snddev_channel *sce;

	snd_mtxlock(d->lock);
	sce = SLIST_FIRST(&d->channels);
	snd_mtxunlock(d->lock);

	return pcm_chn_remove(d, sce->channel);
}

int
pcm_setstatus(device_t dev, char *str)
{
    	struct snddev_info *d = device_get_softc(dev);

	snd_mtxlock(d->lock);
	strncpy(d->status, str, SND_STATUSLEN);
	snd_mtxunlock(d->lock);
	return 0;
}

u_int32_t
pcm_getflags(device_t dev)
{
    	struct snddev_info *d = device_get_softc(dev);

	return d->flags;
}

void
pcm_setflags(device_t dev, u_int32_t val)
{
    	struct snddev_info *d = device_get_softc(dev);

	d->flags = val;
}

void *
pcm_getdevinfo(device_t dev)
{
    	struct snddev_info *d = device_get_softc(dev);

	return d->devinfo;
}

/* This is the generic init routine */
int
pcm_register(device_t dev, void *devinfo, int numplay, int numrec)
{
    	int sz, unit = device_get_unit(dev);
    	struct snddev_info *d = device_get_softc(dev);

	d->lock = snd_mtxcreate(device_get_nameunit(dev));
	snd_mtxlock(d->lock);
    	if (!pcm_devclass) {
    		pcm_devclass = device_get_devclass(dev);
		status_dev = make_dev(&snd_cdevsw, PCMMKMINOR(0, SND_DEV_STATUS, 0),
			 UID_ROOT, GID_WHEEL, 0444, "sndstat");
	}

	make_dev(&snd_cdevsw, PCMMKMINOR(unit, SND_DEV_CTL, 0),
		 UID_ROOT, GID_WHEEL, 0666, "mixer%d", unit);

	d->dev = dev;
	d->devinfo = devinfo;
	d->chancount = 0;
	d->maxchans = numplay + numrec;
    	sz = d->maxchans * sizeof(struct pcm_channel *);

	if (sz > 0) {
		d->aplay = (struct pcm_channel **)malloc(sz, M_DEVBUF, M_WAITOK | M_ZERO);
    		d->arec = (struct pcm_channel **)malloc(sz, M_DEVBUF, M_WAITOK | M_ZERO);
    		if (!d->arec || !d->aplay) goto no;

		if (numplay == 0 || numrec == 0)
			d->flags |= SD_F_SIMPLEX;

		d->fakechan = fkchan_setup(dev);
		chn_init(d->fakechan, NULL, 0);
	}

#ifdef SND_DYNSYSCTL
	sysctl_ctx_init(&d->sysctl_tree);
	d->sysctl_tree_top = SYSCTL_ADD_NODE(&d->sysctl_tree,
				 SYSCTL_STATIC_CHILDREN(_hw_snd), OID_AUTO,
				 device_get_nameunit(dev), CTLFLAG_RD, 0, "");
	if (d->sysctl_tree_top == NULL) {
		sysctl_ctx_free(&d->sysctl_tree);
		goto no;
	}
#endif
#if 1
	vchan_initsys(d);
#endif
	snd_mtxunlock(d->lock);
    	return 0;
no:
	if (d->aplay) free(d->aplay, M_DEVBUF);
	if (d->arec) free(d->arec, M_DEVBUF);
	/* snd_mtxunlock(d->lock); */
	snd_mtxfree(d->lock);
	return ENXIO;
}

int
pcm_unregister(device_t dev)
{
    	int unit = device_get_unit(dev);
    	struct snddev_info *d = device_get_softc(dev);
    	struct snddev_channel *sce;
	dev_t pdev;

	snd_mtxlock(d->lock);
	SLIST_FOREACH(sce, &d->channels, link) {
		if (sce->channel->refcount > 0) {
			device_printf(dev, "unregister: channel busy");
			snd_mtxunlock(d->lock);
			return EBUSY;
		}
	}
	if (mixer_isbusy(d->mixer)) {
		device_printf(dev, "unregister: mixer busy");
		snd_mtxunlock(d->lock);
		return EBUSY;
	}

#ifdef SND_DYNSYSCTL
	d->sysctl_tree_top = NULL;
	sysctl_ctx_free(&d->sysctl_tree);
#endif

	pdev = makedev(CDEV_MAJOR, PCMMKMINOR(unit, SND_DEV_CTL, 0));
	destroy_dev(pdev);
	mixer_uninit(dev);

	while (d->chancount > 0)
		pcm_killchan(dev);

	if (d->aplay) free(d->aplay, M_DEVBUF);
	if (d->arec) free(d->arec, M_DEVBUF);

	chn_kill(d->fakechan);
	fkchan_kill(d->fakechan);

	/* snd_mtxunlock(d->lock); */
	snd_mtxfree(d->lock);
	return 0;
}

/*
 * a small utility function which, given a device number, returns
 * a pointer to the associated struct snddev_info struct, and sets the unit
 * number.
 */
static struct snddev_info *
get_snddev_info(dev_t i_dev, int *unit, int *dev, int *chan)
{
	struct snddev_info *sc;
    	int u, d, c;

    	u = PCMUNIT(i_dev);
    	d = PCMDEV(i_dev);
    	c = PCMCHAN(i_dev);
    	if (u > devclass_get_maxunit(pcm_devclass)) u = -1;
    	if (unit) *unit = u;
    	if (dev) *dev = d;
    	if (chan) *chan = c;
    	if (u < 0) return NULL;

	sc = devclass_get_softc(pcm_devclass, u);
	if (sc == NULL) return NULL;

	switch(d) {
    	case SND_DEV_CTL:	/* /dev/mixer handled by pcm */
    	case SND_DEV_STATUS: /* /dev/sndstat handled by pcm */
    	case SND_DEV_DSP:
    	case SND_DEV_DSP16:
    	case SND_DEV_AUDIO:
		return sc;

    	case SND_DEV_SEQ: /* XXX when enabled... */
    	case SND_DEV_SEQ2:
    	case SND_DEV_MIDIN:
    	case SND_DEV_SNDPROC:	/* /dev/sndproc handled by pcm */
    	default:
		printf("unsupported subdevice %d\n", d);
		return NULL;
    	}
}

static int
sndopen(dev_t i_dev, int flags, int mode, struct proc *p)
{
    	int dev, unit, chan;
    	struct snddev_info *d = get_snddev_info(i_dev, &unit, &dev, &chan);

    	DEB(printf("open snd%d subdev %d flags 0x%08x mode 0x%08x\n",
		unit, dev, flags, mode));

    	switch(dev) {
    	case SND_DEV_STATUS:
		return do_status(0, NULL);

    	case SND_DEV_CTL:
		return d? mixer_busy(d->mixer, 1) : ENXIO;

    	case SND_DEV_AUDIO:
    	case SND_DEV_DSP:
    	case SND_DEV_DSP16:
	case SND_DEV_NORESET:
		return d? dsp_open(d, chan, flags, dev, p->p_pid) : ENXIO;

    	default:
    		return ENXIO;
    	}
}

static int
sndclose(dev_t i_dev, int flags, int mode, struct proc *p)
{
    	int dev, unit, chan;
    	struct snddev_info *d = get_snddev_info(i_dev, &unit, &dev, &chan);

    	DEB(printf("close snd%d subdev %d\n", unit, dev));

    	switch(dev) { /* only those for which close makes sense */
    	case SND_DEV_STATUS:
		return do_status(1, NULL);

    	case SND_DEV_CTL:
		return d? mixer_busy(d->mixer, 0) : ENXIO;

    	case SND_DEV_AUDIO:
    	case SND_DEV_DSP:
    	case SND_DEV_DSP16:
		return d? dsp_close(d, chan, dev) : ENXIO;

    	default:
		return ENXIO;
    	}
}

static int
sndread(dev_t i_dev, struct uio *buf, int flag)
{
    	int dev, unit, chan;
    	struct snddev_info *d = get_snddev_info(i_dev, &unit, &dev, &chan);
    	DEB(printf("read snd%d subdev %d flag 0x%08x\n", unit, dev, flag));

    	switch(dev) {
    	case SND_DEV_STATUS:
		return do_status(2, buf);

    	case SND_DEV_AUDIO:
    	case SND_DEV_DSP:
    	case SND_DEV_DSP16:
        	return d? dsp_read(d, chan, buf, flag) : EBADF;

    	default:
    		return ENXIO;
    	}
}

static int
sndwrite(dev_t i_dev, struct uio *buf, int flag)
{
    	int dev, unit, chan;
    	struct snddev_info *d = get_snddev_info(i_dev, &unit, &dev, &chan);

    	DEB(printf("write snd%d subdev %d flag 0x%08x\n", unit, dev & 0xf, flag));

    	switch(dev) {	/* only writeable devices */
    	case SND_DEV_DSP:
    	case SND_DEV_DSP16:
    	case SND_DEV_AUDIO:
		return d? dsp_write(d, chan, buf, flag) : EBADF;

    	default:
		return EPERM; /* for non-writeable devices ; */
    	}
}

static int
sndioctl(dev_t i_dev, u_long cmd, caddr_t arg, int mode, struct proc * p)
{
    	int dev, chan;
    	struct snddev_info *d = get_snddev_info(i_dev, NULL, &dev, &chan);

    	if (d == NULL) return ENXIO;

    	switch(dev) {
    	case SND_DEV_CTL:
		return mixer_ioctl(d, cmd, arg);

    	case SND_DEV_AUDIO:
    	case SND_DEV_DSP:
    	case SND_DEV_DSP16:
		if (IOCGROUP(cmd) == 'M')
			return mixer_ioctl(d, cmd, arg);
		else
			return dsp_ioctl(d, chan, cmd, arg);

    	default:
    		return ENXIO;
    	}
}

static int
sndpoll(dev_t i_dev, int events, struct proc *p)
{
    	int dev, chan;
    	struct snddev_info *d = get_snddev_info(i_dev, NULL, &dev, &chan);

	DEB(printf("sndpoll d 0x%p dev 0x%04x events 0x%08x\n", d, dev, events));

    	if (d == NULL) return ENXIO;

    	switch(dev) {
    	case SND_DEV_AUDIO:
    	case SND_DEV_DSP:
    	case SND_DEV_DSP16:
		return dsp_poll(d, chan, events, p);

    	default:
    		return (events &
       		       (POLLIN | POLLOUT | POLLRDNORM | POLLWRNORM)) | POLLHUP;
    	}
}

/*
 * The mmap interface allows access to the play and read buffer,
 * plus the device descriptor.
 * The various blocks are accessible at the following offsets:
 *
 * 0x00000000 ( 0   ) : write buffer ;
 * 0x01000000 (16 MB) : read buffer ;
 * 0x02000000 (32 MB) : device descriptor (dangerous!)
 *
 * WARNING: the mmap routines assume memory areas are aligned. This
 * is true (probably) for the dma buffers, but likely false for the
 * device descriptor. As a consequence, we do not know where it is
 * located in the requested area.
 */
static int
sndmmap(dev_t i_dev, vm_offset_t offset, int nprot)
{
    	int unit, dev, chan;
    	struct snddev_info *d = get_snddev_info(i_dev, &unit, &dev, &chan);

    	DEB(printf("sndmmap d 0x%p dev 0x%04x ofs 0x%08x nprot 0x%08x\n",
		   d, dev, offset, nprot));

    	if (d == NULL || nprot & PROT_EXEC)	return -1; /* forbidden */

    	switch(dev) {
    	case SND_DEV_AUDIO:
    	case SND_DEV_DSP:
    	case SND_DEV_DSP16:
		return dsp_mmap(d, chan, offset, nprot);

    	default:
    		return -1;
    	}
}

static int
status_init(struct sbuf *s)
{
    	int i, pc, rc, vc;
    	device_t dev;
    	struct snddev_info *d;
    	struct snddev_channel *sce;
	struct pcm_channel *c;
#ifdef SNDSTAT_VERBOSE
	struct pcm_feeder *f;
#endif

	sbuf_printf(s, "FreeBSD Audio Driver (newpcm) %s %s\nInstalled devices:\n",
		 	__DATE__, __TIME__);

    	for (i = 0; i <= devclass_get_maxunit(pcm_devclass); i++) {
		d = devclass_get_softc(pcm_devclass, i);
		if (!d)
			continue;
		snd_mtxlock(d->lock);
		dev = devclass_get_device(pcm_devclass, i);
		sbuf_printf(s, "pcm%d: <%s> %s", i, device_get_desc(dev), d->status);
		if (d->chancount > 0) {
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
#ifdef SNDSTAT_VERBOSE
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
#endif
		} else
			sbuf_printf(s, " (mixer only)\n");
		snd_mtxunlock(d->lock);
    	}
	sbuf_finish(s);
    	return sbuf_len(s);
}

static int
do_status(int action, struct uio *buf)
{
	static struct sbuf s;
    	static int bufptr = 0;
	static int status_open = 0;
    	int l, err;

	switch(action) {
	case 0: /* open */
		if (status_open)
			return EBUSY;
		if (sbuf_new(&s, NULL, 4096, 0) == NULL)
			return ENXIO;
		bufptr = 0;
		err = (status_init(&s) > 0)? 0 : ENOMEM;
		if (!err)
			status_open = 1;
		return err;

	case 1: /* close */
		if (!status_open)
			return EBADF;
		sbuf_delete(&s);
		status_open = 0;
		return 0;

	case 2:
		if (!status_open)
			return EBADF;
	    	l = min(buf->uio_resid, sbuf_len(&s) - bufptr);
		err = (l > 0)? uiomove(sbuf_data(&s) + bufptr, l, buf) : 0;
    		bufptr += l;
    		return err;

	case 3:
		return status_open;
	}

	return EBADF;
}

static int
sndpcm_modevent(module_t mod, int type, void *data)
{

	switch (type) {
	case MOD_LOAD:
		break;
	case MOD_UNLOAD:
		if (do_status(3, NULL))
			return EBUSY;
		if (status_dev)
			destroy_dev(status_dev);
		status_dev = 0;
		break;
	default:
		break;
	}
	return 0;
}

static moduledata_t sndpcm_mod = {
	"snd_pcm",
	sndpcm_modevent,
	NULL
};
DECLARE_MODULE(snd_pcm, sndpcm_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);
MODULE_VERSION(snd_pcm, PCM_MODVER);
