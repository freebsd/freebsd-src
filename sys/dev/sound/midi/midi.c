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
 * the template to a permanent descriptor (midi_info[unit] and
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
	/* bmaj */	-1
};

/*
 * descriptors for active devices. also used as the public softc
 * of a device.
 */
mididev_info midi_info[NMIDI_MAX];

u_long nmidi;	/* total number of midi devices, filled in by the driver */
u_long nsynth;	/* total number of synthesizers, filled in by the driver */

/* These make the buffer for /dev/midistat */
static int midistatbusy;
static char midistatbuf[4096];
static int midistatptr;

/*
 * This is the generic init routine
 */
int
midiinit(mididev_info *d, device_t dev)
{
	int unit;

	if (midi_devclass == NULL) {
		midi_devclass = device_get_devclass(dev);
		make_dev(&midi_cdevsw, MIDIMKMINOR(0, MIDI_DEV_STATUS),
			 UID_ROOT, GID_WHEEL, 0444, "midistat");
	}

	unit = device_get_unit(dev);
	make_dev(&midi_cdevsw, MIDIMKMINOR(unit, MIDI_DEV_MIDIN),
		 UID_ROOT, GID_WHEEL, 0666, "midi%d", unit);

	/*
	 * initialize standard parameters for the device. This can be
	 * overridden by device-specific configurations but better do
	 * here the generic things.
	 */

	d->unit = device_get_unit(dev);
	d->softc = device_get_softc(dev);
	d->dev = dev;
	d->magic = MAGIC(d->unit); /* debugging... */

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
	mididev_info *d = NULL;

	if (MIDIDEV(i_dev) != MIDI_DEV_MIDIN)
		return NULL;
	u = MIDIUNIT(i_dev);
	if (unit)
		*unit = u;

	if (u >= nmidi + nsynth) {
		DEB(printf("get_mididev_info: unit %d is not configured.\n", u));
		return NULL;
	}
	d = &midi_info[u];

	return d;
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
	switch (MIDIDEV(i_dev)) {
	case MIDI_DEV_MIDIN:
		return midi_open(i_dev, flags, mode, p);
	case MIDI_DEV_STATUS:
		return midistat_open(i_dev, flags, mode, p);
	}

	return (ENXIO);
}

static int
midiclose(dev_t i_dev, int flags, int mode, struct proc * p)
{
	switch (MIDIDEV(i_dev)) {
	case MIDI_DEV_MIDIN:
		return midi_close(i_dev, flags, mode, p);
	case MIDI_DEV_STATUS:
		return midistat_close(i_dev, flags, mode, p);
	}

	return (ENXIO);
}

static int
midiread(dev_t i_dev, struct uio * buf, int flag)
{
	switch (MIDIDEV(i_dev)) {
	case MIDI_DEV_MIDIN:
		return midi_read(i_dev, buf, flag);
	case MIDI_DEV_STATUS:
		return midistat_read(i_dev, buf, flag);
	}

	return (ENXIO);
}

static int
midiwrite(dev_t i_dev, struct uio * buf, int flag)
{
	switch (MIDIDEV(i_dev)) {
	case MIDI_DEV_MIDIN:
		return midi_write(i_dev, buf, flag);
	}

	return (ENXIO);
}

static int
midiioctl(dev_t i_dev, u_long cmd, caddr_t arg, int mode, struct proc * p)
{
	switch (MIDIDEV(i_dev)) {
	case MIDI_DEV_MIDIN:
		return midi_ioctl(i_dev, cmd, arg, mode, p);
	}

	return (ENXIO);
}

static int
midipoll(dev_t i_dev, int events, struct proc * p)
{
	switch (MIDIDEV(i_dev)) {
	case MIDI_DEV_MIDIN:
		return midi_poll(i_dev, events, p);
	}

	return (ENXIO);
}

/*
 * Followings are the generic methods in midi drivers.
 */

int
midi_open(dev_t i_dev, int flags, int mode, struct proc * p)
{
	int dev, unit, s, ret;
	mididev_info *d;

	dev = minor(i_dev);
	d = get_mididev_info(i_dev, &unit);

	DEB(printf("open midi%d subdev %d flags 0x%08x mode 0x%08x\n",
		   unit, dev & 0xf, flags, mode));

	if (d == NULL)
		return (ENXIO);

	s = splmidi();

	/* Mark this device busy. */
	device_busy(d->dev);
	if ((d->flags & MIDI_F_BUSY) != 0) {
		splx(s);
		DEB(printf("opl_open: unit %d is busy.\n", unit));
		return (EBUSY);
	}
	d->flags |= MIDI_F_BUSY;
	d->flags &= ~(MIDI_F_READING | MIDI_F_WRITING);
	d->fflags = flags;

	/* Init the queue. */
	if ((d->fflags & FREAD) != 0)
		midibuf_init(&d->midi_dbuf_in);
	if ((d->fflags & FWRITE) != 0) {
		midibuf_init(&d->midi_dbuf_out);
		midibuf_init(&d->midi_dbuf_passthru);
	}

	if (d->open == NULL)
		ret = 0;
	else
		ret = d->open(i_dev, flags, mode, p);

	splx(s);

	return (ret);
}

int
midi_close(dev_t i_dev, int flags, int mode, struct proc * p)
{
	int dev, unit, s, ret;
	mididev_info *d;

	dev = minor(i_dev);
	d = get_mididev_info(i_dev, &unit);

	DEB(printf("close midi%d subdev %d\n", unit, dev & 0xf));

	if (d == NULL)
		return (ENXIO);

	s = splmidi();

	/* Clear the queues. */
	if ((d->fflags & FREAD) != 0)
		midibuf_init(&d->midi_dbuf_in);
	if ((d->fflags & FWRITE) != 0) {
		midibuf_init(&d->midi_dbuf_out);
		midibuf_init(&d->midi_dbuf_passthru);
	}

	/* Stop playing and unmark this device busy. */
	d->flags &= ~MIDI_F_BUSY;
	d->fflags = 0;

	device_unbusy(d->dev);

	if (d->close == NULL)
		ret = 0;
	else
		ret = d->close(i_dev, flags, mode, p);

	splx(s);

	return (ret);
}

int
midi_read(dev_t i_dev, struct uio * buf, int flag)
{
	int dev, unit, s, len, ret;
	mididev_info *d ;

	dev = minor(i_dev);

	d = get_mididev_info(i_dev, &unit);
	DEB(printf("read midi%d subdev %d flag 0x%08x\n", unit, dev & 0xf, flag));

	if (d == NULL)
		return (ENXIO);

	ret = 0;
	s = splmidi();

	/* Begin recording. */
	d->callback(d, MIDI_CB_START | MIDI_CB_RD);

	/* Have we got the data to read? */
	if ((d->flags & MIDI_F_NBIO) != 0 && d->midi_dbuf_in.rl == 0)
		ret = EAGAIN;
	else {
		len = buf->uio_resid;
		ret = midibuf_uioread(&d->midi_dbuf_in, buf, len);
		if (ret < 0)
			ret = -ret;
		else
			ret = 0;
	}

	if (ret == 0 && d->read != NULL)
		ret = d->read(i_dev, buf, flag);

	splx(s);

	return (ret);
}

int
midi_write(dev_t i_dev, struct uio * buf, int flag)
{
	int dev, unit, s, len, ret;
	mididev_info *d;

	dev = minor(i_dev);
	d = get_mididev_info(i_dev, &unit);

	DEB(printf("write midi%d subdev %d flag 0x%08x\n", unit, dev & 0xf, flag));

	if (d == NULL)
		return (ENXIO);

	ret = 0;
	s = splmidi();

	/* Begin playing. */
	d->callback(d, MIDI_CB_START | MIDI_CB_WR);

	/* Have we got the data to write? */
	if ((d->flags & MIDI_F_NBIO) != 0 && d->midi_dbuf_out.fl == 0)
		ret = EAGAIN;
	else {
		len = buf->uio_resid;
		if (len > d->midi_dbuf_out.fl &&
		    (d->flags & MIDI_F_NBIO))
			len = d->midi_dbuf_out.fl;
		ret = midibuf_uiowrite(&d->midi_dbuf_out, buf, len);
		if (ret < 0)
			ret = -ret;
		else
			ret = 0;
	}

	/* Begin playing. */
	d->callback(d, MIDI_CB_START | MIDI_CB_WR);

	if (ret == 0 && d->write != NULL)
		ret = d->write(i_dev, buf, flag);

	splx(s);

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
	u_long s;

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
	s = splmidi();
	switch(cmd) {

		/*
		 * we start with the new ioctl interface.
		 */
	case AIONWRITE:	/* how many bytes can write ? */
		*(int *)arg = d->midi_dbuf_out.fl;
		break;

	case AIOSSIZE:     /* set the current blocksize */
		sndsize = (struct snd_size *)arg;
		if (sndsize->play_size <= d->midi_dbuf_out.unit_size && sndsize->rec_size <= d->midi_dbuf_in.unit_size) {
			d->flags &= ~MIDI_F_HAS_SIZE;
			d->midi_dbuf_out.blocksize = d->midi_dbuf_out.unit_size;
			d->midi_dbuf_in.blocksize = d->midi_dbuf_in.unit_size;
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
			d->flags |= MIDI_F_HAS_SIZE;
		}
		/* FALLTHROUGH */
	case AIOGSIZE:	/* get the current blocksize */
		sndsize = (struct snd_size *)arg;
		sndsize->play_size = d->midi_dbuf_out.blocksize;
		sndsize->rec_size = d->midi_dbuf_in.blocksize;

		ret = 0;
		break;

	case AIOSTOP:
		if (*(int *)arg == AIOSYNC_PLAY) /* play */
			*(int *)arg = d->callback(d, MIDI_CB_STOP | MIDI_CB_WR);
		else if (*(int *)arg == AIOSYNC_CAPTURE)
			*(int *)arg = d->callback(d, MIDI_CB_STOP | MIDI_CB_RD);
		else {
			splx(s);
			DEB(printf("AIOSTOP: bad channel 0x%x\n", *(int *)arg));
			*(int *)arg = 0 ;
		}
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
		if ( *(int *)arg == 0 )
			d->flags &= ~MIDI_F_NBIO ;
		else
			d->flags |= MIDI_F_NBIO ;
		break ;

	case MIOSPASSTHRU: /* set/clear passthru */
		if ( *(int *)arg == 0 )
			d->flags &= ~MIDI_F_PASSTHRU ;
		else
			d->flags |= MIDI_F_PASSTHRU ;

		/* Init the queue. */
		midibuf_init(&d->midi_dbuf_passthru);

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
	splx(s);
	return ret ;
}

int
midi_poll(dev_t i_dev, int events, struct proc * p)
{
	int unit, dev, ret, s, lim;
	mididev_info *d;

	dev = minor(i_dev);
	d = get_mididev_info(i_dev, &unit);

	if (d == NULL)
		return (ENXIO);

	if (d->poll)
		ret = d->poll(i_dev, events, p);

	ret = 0;
	s = splmidi();

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
	splx(s);

	return (ret);
}

void
midi_intr(mididev_info *d)
{
	if (d->intr != NULL)
		d->intr(d->intrarg, d);
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
	for (i = 0 ; i < NMIDI_MAX ; i++) {
		md = &midi_info[i];
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
	int s, len;

	s = splmidi();
	len = min(uio->uio_resid, strlen(&buf[*ptr]));
	if (len > 0) {
		uiomove(&buf[*ptr], len, uio);
		*ptr += len;
	}
	splx(s);

	return (0);
}
