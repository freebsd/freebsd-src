/*
 * Main midi driver for FreeBSD. This file provides the main
 * entry points for probe/attach and all i/o demultiplexing, including
 * default routines for generic devices.
 * 
 * (C) 1999 Seigo Tanimura
 * 
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * For each card type a template "mididev_info" structure contains
 * all the relevant parameters, both for configuration and runtime.
 *
 * In this file we build tables of pointers to the descriptors for
 * the various supported cards. The generic probe routine scans
 * the table(s) looking for a matching entry, then invokes the
 * board-specific probe routine. If successful, a pointer to the
 * correct mididev_info is stored in mididev_last_probed, for subsequent
 * use in the attach routine. The generic attach routine copies
 * the template to a permanent descriptor (midi_info and
 * friends), initializes all generic parameters, and calls the
 * board-specific attach routine.
 *
 * On device calls, the generic routines do the checks on unit and
 * device parameters, then call the board-specific routines if
 * available, or try to perform the task using the default code.
 *
 * $FreeBSD$
 *
 */

#include <dev/sound/midi/midi.h>

static devclass_t midi_devclass;

static d_open_t midiopen;
static d_close_t midiclose;
static d_ioctl_t midiioctl;
static d_read_t midiread;
static d_write_t midiwrite;
static d_poll_t midipoll;

/* These functions are local. */
static d_open_t midistat_open;
static d_close_t midistat_close;
static d_read_t midistat_read;
static int midi_initstatus(char *buf, int size);
static int midi_readstatus(char *buf, int *ptr, struct uio *uio);

#define CDEV_MAJOR MIDI_CDEV_MAJOR
static struct cdevsw midi_cdevsw = {
	/* open */	midiopen,
	/* close */	midiclose,
	/* read */	midiread,
	/* write */	midiwrite,
	/* ioctl */	midiioctl,
	/* poll */	midipoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"midi",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
};

/*
 * descriptors for active devices. also used as the public softc
 * of a device.
 */
static TAILQ_HEAD(,_mididev_info) midi_info;
static int nmidi, nsynth;
/* Mutex to protect midi_info, nmidi and nsynth. */
static struct mtx midiinfo_mtx;
static int midiinfo_mtx_init;

/* These make the buffer for /dev/midistat */
static int midistatbusy;
static char midistatbuf[4096];
static int midistatptr;

/*
 * This is the generic init routine.
 * Must be called after device-specific init.
 */
int
midiinit(mididev_info *d, device_t dev)
{
	int unit;

	/*
	 * initialize standard parameters for the device. This can be
	 * overridden by device-specific configurations but better do
	 * here the generic things.
	 */

	unit = d->unit;
	d->softc = device_get_softc(dev);
	d->dev = dev;
	d->magic = MAGIC(d->unit); /* debugging... */
	d->flags = 0;
	d->fflags = 0;
	d->midi_dbuf_in.unit_size = 1;
	d->midi_dbuf_out.unit_size = 1;
	d->midi_dbuf_passthru.unit_size = 1;

	mtx_unlock(&d->flagqueue_mtx);

	if (midi_devclass == NULL) {
		midi_devclass = device_get_devclass(dev);
		make_dev(&midi_cdevsw, MIDIMKMINOR(0, MIDI_DEV_STATUS),
			 UID_ROOT, GID_WHEEL, 0444, "midistat");
	}
	make_dev(&midi_cdevsw, MIDIMKMINOR(unit, MIDI_DEV_MIDIN),
		 UID_ROOT, GID_WHEEL, 0666, "midi%d", unit);

	return 0 ;
}

/*
 * a small utility function which, given a device number, returns
 * a pointer to the associated mididev_info struct, and sets the unit
 * number.
 */
mididev_info *
get_mididev_info(dev_t i_dev, int *unit)
{
	int u;

	if (MIDIDEV(i_dev) != MIDI_DEV_MIDIN)
		return NULL;
	u = MIDIUNIT(i_dev);
	if (unit)
		*unit = u;

	return get_mididev_info_unit(u);
}

/*
 * a small utility function which, given a unit number, returns
 * a pointer to the associated mididev_info struct.
 */
mididev_info *
get_mididev_info_unit(int unit)
{
	mididev_info *md;

	/* XXX */
	if (!midiinfo_mtx_init) {
		midiinfo_mtx_init = 1;
		mtx_init(&midiinfo_mtx, "midinf", MTX_DEF);
		TAILQ_INIT(&midi_info);
	}

	mtx_lock(&midiinfo_mtx);
	TAILQ_FOREACH(md, &midi_info, md_link) {
		if (md->unit == unit)
			break;
	}
	mtx_unlock(&midiinfo_mtx);

	return md;
}

/* Create a new midi device info structure. */
/* TODO: lock md, then exit. */
mididev_info *
create_mididev_info_unit(int type, mididev_info *mdinf, synthdev_info *syninf)
{
	int unit;
	mididev_info *md, *mdnew;

	/* XXX */
	if (!midiinfo_mtx_init) {
		midiinfo_mtx_init = 1;
		mtx_init(&midiinfo_mtx, "midinf", MTX_DEF);
		TAILQ_INIT(&midi_info);
	}

	/* As malloc(9) might block, allocate mididev_info now. */
	mdnew = malloc(sizeof(mididev_info), M_DEVBUF, M_WAITOK | M_ZERO);
	if (mdnew == NULL)
		return NULL;
	bcopy(mdinf, mdnew, sizeof(mididev_info));
	bcopy(syninf, &mdnew->synth, sizeof(synthdev_info));
	midibuf_init(&mdnew->midi_dbuf_in);
	midibuf_init(&mdnew->midi_dbuf_out);
	midibuf_init(&mdnew->midi_dbuf_passthru);
	mtx_init(&mdnew->flagqueue_mtx, "midflq", MTX_DEF);
	mtx_init(&mdnew->synth.vc_mtx, "synsvc", MTX_DEF);
	mtx_init(&mdnew->synth.status_mtx, "synsst", MTX_DEF);

	mtx_lock(&midiinfo_mtx);

	/* XXX midi_info is still static. */
	switch (type) {
	case MDT_MIDI:
		nmidi++;
		break;
	case MDT_SYNTH:
		nsynth++;
		break;
	default:
		mtx_unlock(&midiinfo_mtx);
		midibuf_destroy(&mdnew->midi_dbuf_in);
		midibuf_destroy(&mdnew->midi_dbuf_out);
		midibuf_destroy(&mdnew->midi_dbuf_passthru);
		mtx_destroy(&mdnew->flagqueue_mtx);
		mtx_destroy(&mdnew->synth.vc_mtx);
		mtx_destroy(&mdnew->synth.status_mtx);
		free(mdnew, M_DEVBUF);
		panic("unsupported device type");
		return NULL;
	}

	for (unit = 0 ; ; unit++) {
		TAILQ_FOREACH(md, &midi_info, md_link) {
			if (md->unit == unit)
				break;
		}
		if (md == NULL)
			break;
	}

	mdnew->unit = unit;
	mtx_lock(&mdnew->flagqueue_mtx);
	TAILQ_INSERT_TAIL(&midi_info, mdnew, md_link);

	mtx_unlock(&midiinfo_mtx);

	return mdnew;
}

/* Return the number of configured devices. */
int
mididev_info_number(void)
{
	return nmidi + nsynth;
}

/*
 * here are the switches for the main functions. The switches do
 * all necessary checks on the device number to make sure
 * that the device is configured. They also provide some default
 * functionalities so that device-specific drivers have to deal
 * only with special cases.
 */

static int
midiopen(dev_t i_dev, int flags, int mode, struct proc * p)
{
	int ret;

	switch (MIDIDEV(i_dev)) {
	case MIDI_DEV_MIDIN:
		ret = midi_open(i_dev, flags, mode, p);
		break;
	case MIDI_DEV_STATUS:
		ret = midistat_open(i_dev, flags, mode, p);
		break;
	default:
		ret = ENXIO;
		break;
	}

	return (ret);
}

static int
midiclose(dev_t i_dev, int flags, int mode, struct proc * p)
{
	int ret;

	switch (MIDIDEV(i_dev)) {
	case MIDI_DEV_MIDIN:
		ret = midi_close(i_dev, flags, mode, p);
		break;
	case MIDI_DEV_STATUS:
		ret = midistat_close(i_dev, flags, mode, p);
		break;
	default:
		ret = ENXIO;
		break;
	}

	return (ret);
}

static int
midiread(dev_t i_dev, struct uio * buf, int flag)
{
	int ret;

	switch (MIDIDEV(i_dev)) {
	case MIDI_DEV_MIDIN:
		ret = midi_read(i_dev, buf, flag);
		break;
	case MIDI_DEV_STATUS:
		ret = midistat_read(i_dev, buf, flag);
		break;
	default:
		ret = ENXIO;
		break;
	}

	return (ret);
}

static int
midiwrite(dev_t i_dev, struct uio * buf, int flag)
{
	int ret;

	switch (MIDIDEV(i_dev)) {
	case MIDI_DEV_MIDIN:
		ret = midi_write(i_dev, buf, flag);
		break;
	default:
		ret = ENXIO;
		break;
	}

	return (ret);
}

static int
midiioctl(dev_t i_dev, u_long cmd, caddr_t arg, int mode, struct proc * p)
{
	int ret;

	switch (MIDIDEV(i_dev)) {
	case MIDI_DEV_MIDIN:
		ret = midi_ioctl(i_dev, cmd, arg, mode, p);
		break;
	default:
		ret = ENXIO;
		break;
	}

	return (ret);
}

static int
midipoll(dev_t i_dev, int events, struct proc * p)
{
	int ret;

	switch (MIDIDEV(i_dev)) {
	case MIDI_DEV_MIDIN:
		ret = midi_poll(i_dev, events, p);
		break;
	default:
		ret = ENXIO;
		break;
	}

	return (ret);
}

/*
 * Followings are the generic methods in midi drivers.
 */

int
midi_open(dev_t i_dev, int flags, int mode, struct proc * p)
{
	int dev, unit, ret;
	mididev_info *d;

	dev = minor(i_dev);
	d = get_mididev_info(i_dev, &unit);

	DEB(printf("open midi%d subdev %d flags 0x%08x mode 0x%08x\n",
		   unit, dev & 0xf, flags, mode));

	if (d == NULL)
		return (ENXIO);

	/* Mark this device busy. */
	mtx_lock(&d->flagqueue_mtx);
	device_busy(d->dev);
	if ((d->flags & MIDI_F_BUSY) != 0) {
		mtx_unlock(&d->flagqueue_mtx);
		DEB(printf("opl_open: unit %d is busy.\n", unit));
		return (EBUSY);
	}
	d->flags |= MIDI_F_BUSY;
	d->flags &= ~(MIDI_F_READING | MIDI_F_WRITING);
	d->fflags = flags;

	/* Init the queue. */
	if ((flags & FREAD) != 0)
		midibuf_clear(&d->midi_dbuf_in);
	if ((flags & FWRITE) != 0) {
		midibuf_clear(&d->midi_dbuf_out);
		midibuf_clear(&d->midi_dbuf_passthru);
	}

	mtx_unlock(&d->flagqueue_mtx);

	if (d->open == NULL)
		ret = 0;
	else
		ret = d->open(i_dev, flags, mode, p);

	return (ret);
}

int
midi_close(dev_t i_dev, int flags, int mode, struct proc * p)
{
	int dev, unit, ret;
	mididev_info *d;

	dev = minor(i_dev);
	d = get_mididev_info(i_dev, &unit);

	DEB(printf("close midi%d subdev %d\n", unit, dev & 0xf));

	if (d == NULL)
		return (ENXIO);

	mtx_lock(&d->flagqueue_mtx);

	/* Stop recording and playing. */
	if ((d->flags & MIDI_F_READING) != 0)
		d->callback(d, MIDI_CB_ABORT | MIDI_CB_RD);
	if ((d->flags & MIDI_F_WRITING) != 0)
		d->callback(d, MIDI_CB_ABORT | MIDI_CB_WR);

	/* Clear the queues. */
	if ((d->fflags & FREAD) != 0)
		midibuf_clear(&d->midi_dbuf_in);
	if ((d->fflags & FWRITE) != 0) {
		midibuf_clear(&d->midi_dbuf_out);
		midibuf_clear(&d->midi_dbuf_passthru);
	}

	/* Stop playing and unmark this device busy. */
	d->flags &= ~MIDI_F_BUSY;
	d->fflags = 0;

	device_unbusy(d->dev);

	mtx_unlock(&d->flagqueue_mtx);

	if (d->close == NULL)
		ret = 0;
	else
		ret = d->close(i_dev, flags, mode, p);

	return (ret);
}

int
midi_read(dev_t i_dev, struct uio * buf, int flag)
{
	int dev, unit, len, ret;
	mididev_info *d ;

	dev = minor(i_dev);

	d = get_mididev_info(i_dev, &unit);
	DEB(printf("read midi%d subdev %d flag 0x%08x\n", unit, dev & 0xf, flag));

	if (d == NULL)
		return (ENXIO);

	ret = 0;

	mtx_lock(&d->flagqueue_mtx);

	/* Begin recording. */
	d->callback(d, MIDI_CB_START | MIDI_CB_RD);

	len = 0;

	/* Have we got the data to read? */
	if ((d->flags & MIDI_F_NBIO) != 0 && d->midi_dbuf_in.rl == 0)
		ret = EAGAIN;
	else {
		len = buf->uio_resid;
		ret = midibuf_uioread(&d->midi_dbuf_in, buf, len, &d->flagqueue_mtx);
		if (ret < 0)
			ret = -ret;
		else
			ret = 0;
	}

	mtx_unlock(&d->flagqueue_mtx);

	return (ret);
}

int
midi_write(dev_t i_dev, struct uio * buf, int flag)
{
	int dev, unit, len, ret;
	mididev_info *d;

	dev = minor(i_dev);
	d = get_mididev_info(i_dev, &unit);

	DEB(printf("write midi%d subdev %d flag 0x%08x\n", unit, dev & 0xf, flag));

	if (d == NULL)
		return (ENXIO);

	ret = 0;

	mtx_lock(&d->flagqueue_mtx);

	/* Have we got the data to write? */
	if ((d->flags & MIDI_F_NBIO) != 0 && d->midi_dbuf_out.fl == 0) {
		/* Begin playing. */
		d->callback(d, MIDI_CB_START | MIDI_CB_WR);
		ret = EAGAIN;
	} else {
		len = buf->uio_resid;
		if (len > d->midi_dbuf_out.fl &&
		    (d->flags & MIDI_F_NBIO))
			len = d->midi_dbuf_out.fl;
		ret = midibuf_uiowrite(&d->midi_dbuf_out, buf, len, &d->flagqueue_mtx);
		if (ret < 0)
			ret = -ret;
		else {
			/* Begin playing. */
			d->callback(d, MIDI_CB_START | MIDI_CB_WR);
			ret = 0;
		}
	}

	mtx_unlock(&d->flagqueue_mtx);

	return (ret);
}

/*
 * generic midi ioctl. Functions of the default driver can be
 * overridden by the device-specific ioctl call.
 * If a device-specific call returns ENOSYS (Function not implemented),
 * the default driver is called. Otherwise, the returned value
 * is passed up.
 *
 * The default handler, for many parameters, sets the value in the
 * descriptor, sets MIDI_F_INIT, and calls the callback function with
 * reason INIT. If successful, the callback returns 1 and the caller
 * can update the parameter.
 */

int
midi_ioctl(dev_t i_dev, u_long cmd, caddr_t arg, int mode, struct proc * p)
{
	int ret = ENOSYS, dev, unit;
	mididev_info *d;
	struct snd_size *sndsize;

	dev = minor(i_dev);
	d = get_mididev_info(i_dev, &unit);

	if (d == NULL)
		return (ENXIO);

	if (d->ioctl)
		ret = d->ioctl(i_dev, cmd, arg, mode, p);
	if (ret != ENOSYS)
		return ret;

	/*
	 * pass control to the default ioctl handler. Set ret to 0 now.
	 */
	ret = 0;

	/*
	 * all routines are called with int. blocked. Make sure that
	 * ints are re-enabled when calling slow or blocking functions!
	 */
	switch(cmd) {

		/*
		 * we start with the new ioctl interface.
		 */
	case AIONWRITE:	/* how many bytes can write ? */
		*(int *)arg = d->midi_dbuf_out.fl;
		break;

	case AIOSSIZE:     /* set the current blocksize */
		sndsize = (struct snd_size *)arg;
		mtx_lock(&d->flagqueue_mtx);
		if (sndsize->play_size <= d->midi_dbuf_out.unit_size && sndsize->rec_size <= d->midi_dbuf_in.unit_size) {
			d->midi_dbuf_out.blocksize = d->midi_dbuf_out.unit_size;
			d->midi_dbuf_in.blocksize = d->midi_dbuf_in.unit_size;
			sndsize->play_size = d->midi_dbuf_out.blocksize;
			sndsize->rec_size = d->midi_dbuf_in.blocksize;
			d->flags &= ~MIDI_F_HAS_SIZE;
			mtx_unlock(&d->flagqueue_mtx);
		}
		else {
			if (sndsize->play_size > d->midi_dbuf_out.bufsize / 4)
				sndsize->play_size = d->midi_dbuf_out.bufsize / 4;
			if (sndsize->rec_size > d->midi_dbuf_in.bufsize / 4)
				sndsize->rec_size = d->midi_dbuf_in.bufsize / 4;
			/* Round up the size to the multiple of EV_SZ. */
			d->midi_dbuf_out.blocksize =
			    ((sndsize->play_size + d->midi_dbuf_out.unit_size - 1)
			     / d->midi_dbuf_out.unit_size) * d->midi_dbuf_out.unit_size;
			d->midi_dbuf_in.blocksize =
			    ((sndsize->rec_size + d->midi_dbuf_in.unit_size - 1)
			     / d->midi_dbuf_in.unit_size) * d->midi_dbuf_in.unit_size;
			sndsize->play_size = d->midi_dbuf_out.blocksize;
			sndsize->rec_size = d->midi_dbuf_in.blocksize;
			d->flags |= MIDI_F_HAS_SIZE;
			mtx_unlock(&d->flagqueue_mtx);
		}

		ret = 0;
		break;

	case AIOGSIZE:	/* get the current blocksize */
		sndsize = (struct snd_size *)arg;
		mtx_lock(&d->flagqueue_mtx);
		sndsize->play_size = d->midi_dbuf_out.blocksize;
		sndsize->rec_size = d->midi_dbuf_in.blocksize;
		mtx_unlock(&d->flagqueue_mtx);

		ret = 0;
		break;

	case AIOSTOP:
		mtx_lock(&d->flagqueue_mtx);
		if (*(int *)arg == AIOSYNC_PLAY) /* play */
			*(int *)arg = d->callback(d, MIDI_CB_STOP | MIDI_CB_WR);
		else if (*(int *)arg == AIOSYNC_CAPTURE)
			*(int *)arg = d->callback(d, MIDI_CB_STOP | MIDI_CB_RD);
		else {
			DEB(printf("AIOSTOP: bad channel 0x%x\n", *(int *)arg));
			*(int *)arg = 0 ;
		}
		mtx_unlock(&d->flagqueue_mtx);
		break ;

	case AIOSYNC:
		DEB(printf("AIOSYNC chan 0x%03lx pos %lu unimplemented\n",
			   ((snd_sync_parm *)arg)->chan,
			   ((snd_sync_parm *)arg)->pos));
		break;
		/*
		 * here follow the standard ioctls (filio.h etc.)
		 */
	case FIONREAD: /* get # bytes to read */
		*(int *)arg = d->midi_dbuf_in.rl;
		break;

	case FIOASYNC: /*set/clear async i/o */
		DEB( printf("FIOASYNC\n") ; )
		    break;

	case FIONBIO: /* set/clear non-blocking i/o */
		mtx_lock(&d->flagqueue_mtx);
		if ( *(int *)arg == 0 )
			d->flags &= ~MIDI_F_NBIO ;
		else
			d->flags |= MIDI_F_NBIO ;
		mtx_unlock(&d->flagqueue_mtx);
		break ;

	case MIOSPASSTHRU: /* set/clear passthru */
		mtx_lock(&d->flagqueue_mtx);
		if ( *(int *)arg == 0 )
			d->flags &= ~MIDI_F_PASSTHRU ;
		else
			d->flags |= MIDI_F_PASSTHRU ;

		/* Init the queue. */
		midibuf_clear(&d->midi_dbuf_passthru);

		mtx_unlock(&d->flagqueue_mtx);

		/* FALLTHROUGH */
	case MIOGPASSTHRU: /* get passthru */
		if ((d->flags & MIDI_F_PASSTHRU) != 0)
			(int *)arg = 1;
		else
			(int *)arg = 0;
		break ;

	default:
		DEB(printf("default ioctl midi%d subdev %d fn 0x%08x fail\n",
			   unit, dev & 0xf, cmd));
		ret = EINVAL;
		break ;
	}
	return ret ;
}

int
midi_poll(dev_t i_dev, int events, struct proc * p)
{
	int unit, dev, ret, lim;
	mididev_info *d;

	dev = minor(i_dev);
	d = get_mididev_info(i_dev, &unit);

	if (d == NULL)
		return (ENXIO);

	ret = 0;

	mtx_lock(&d->flagqueue_mtx);

	/* Look up the apropriate queue and select it. */
	if ((events & (POLLOUT | POLLWRNORM)) != 0) {
		/* Start playing. */
		d->callback(d, MIDI_CB_START | MIDI_CB_WR);

		/* Find out the boundary. */
		if ((d->flags & MIDI_F_HAS_SIZE) != 0)
			lim = d->midi_dbuf_out.blocksize;
		else
			lim = d->midi_dbuf_out.unit_size;
		if (d->midi_dbuf_out.fl < lim)
			/* No enough space, record select. */
			selrecord(p, &d->midi_dbuf_out.sel);
		else
			/* We can write now. */
			ret |= events & (POLLOUT | POLLWRNORM);
	}
	if ((events & (POLLIN | POLLRDNORM)) != 0) {
		/* Start recording. */
		d->callback(d, MIDI_CB_START | MIDI_CB_RD);

		/* Find out the boundary. */
		if ((d->flags & MIDI_F_HAS_SIZE) != 0)
			lim = d->midi_dbuf_in.blocksize;
		else
			lim = d->midi_dbuf_in.unit_size;
		if (d->midi_dbuf_in.rl < lim)
			/* No data ready, record select. */
			selrecord(p, &d->midi_dbuf_in.sel);
		else
			/* We can write now. */
			ret |= events & (POLLIN | POLLRDNORM);
	}

	mtx_unlock(&d->flagqueue_mtx);

	return (ret);
}

void
midi_intr(mididev_info *d)
{
	if (d->intr != NULL)
		d->intr(d->intrarg, d);
}

/* Flush the output queue. */
#define MIDI_SYNC_TIMEOUT 1
int
midi_sync(mididev_info *d)
{
	int i, rl;

	mtx_assert(&d->flagqueue_mtx, MA_OWNED);

	while (d->midi_dbuf_out.rl > 0) {
		if ((d->flags & MIDI_F_WRITING) == 0)
			d->callback(d, MIDI_CB_START | MIDI_CB_WR);
		rl = d->midi_dbuf_out.rl;
		i = msleep(&d->midi_dbuf_out.tsleep_out, &d->flagqueue_mtx, PRIBIO | PCATCH, "midsnc", (d->midi_dbuf_out.bufsize * 10 * hz / 38400) + MIDI_SYNC_TIMEOUT * hz);
		if (i == EINTR || i == ERESTART) {
			if (i == EINTR)
				d->callback(d, MIDI_CB_STOP | MIDI_CB_WR);
			return (i);
		}
		if (i == EWOULDBLOCK && rl == d->midi_dbuf_out.rl) {
			/* A queue seems to be stuck up. Give up and clear the queue. */
			d->callback(d, MIDI_CB_STOP | MIDI_CB_WR);
			midibuf_clear(&d->midi_dbuf_out);
			return (0);
		}
	}

	return 0;
}

/*
 * These handle the status message of the midi drivers.
 */

int
midistat_open(dev_t i_dev, int flags, int mode, struct proc * p)
{
	if (midistatbusy)
		return (EBUSY);

	bzero(midistatbuf, sizeof(midistatbuf));
	midistatptr = 0;
	if (midi_initstatus(midistatbuf, sizeof(midistatbuf) - 1))
		return (ENOMEM);

	midistatbusy = 1;

	return (0);
}

int
midistat_close(dev_t i_dev, int flags, int mode, struct proc * p)
{
	midistatbusy = 0;

	return (0);
}

int
midistat_read(dev_t i_dev, struct uio * buf, int flag)
{
	return midi_readstatus(midistatbuf, &midistatptr, buf);
}

/*
 * finally, some "libraries"
 */

/* Inits the buffer for /dev/midistat. */
static int
midi_initstatus(char *buf, int size)
{
	int i, p;
	device_t dev;
	mididev_info *md;

	p = 0;
	p += snprintf(buf, size, "FreeBSD Midi Driver (newmidi) %s %s\nInstalled devices:\n", __DATE__, __TIME__);
	for (i = 0 ; i < mididev_info_number() ; i++) {
		md = get_mididev_info_unit(i);
		if (!MIDICONFED(md))
			continue;
		dev = devclass_get_device(midi_devclass, i);
		if (p < size)
			p += snprintf(&buf[p], size - p, "midi%d: <%s> %s\n", i, device_get_desc(dev), md->midistat);
		else
			return (1);
	}

	return (0);
}

/* Reads the status message. */
static int
midi_readstatus(char *buf, int *ptr, struct uio *uio)
{
	int len;

	len = min(uio->uio_resid, strlen(&buf[*ptr]));
	if (len > 0) {
		uiomove(&buf[*ptr], len, uio);
		*ptr += len;
	}

	return (0);
}
