/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003 Mathew Kanner
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 * Copyright (c) 2025-2026 The FreeBSD Foundation
 *
 * Portions of this software were developed by Christos Margiolis
 * <christos@FreeBSD.org> under sponsorship from the FreeBSD Foundation.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (augustss@netbsd.org).
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/poll.h>
#include <sys/selinfo.h>
#include <sys/uio.h>

#ifdef HAVE_KERNEL_OPTION_HEADERS
#include "opt_snd.h"
#endif

#include <dev/sound/midi/midi.h>


static MALLOC_DEFINE(M_MIDI, "midi buffers", "MIDI data allocation area");

struct midi_queue {
	uint8_t	*buf;
	size_t	size;
	size_t	used;			/* Bytes queued. */
};

struct midi_port {
	struct midi_softc *sc;
	int	chan;

	int	flags;			/* File flags */
	struct midi_queue inq, outq;
	int	rchan, wchan;
	struct selinfo rsel, wsel;
	size_t	hiwat;			/* Free space below which writes from
					 * userland are blocked. */
	struct cdev *cdev;
};

static d_open_t midi_open;
static d_close_t midi_close;
static d_ioctl_t midi_ioctl;
static d_read_t midi_read;
static d_write_t midi_write;
static d_poll_t midi_poll;

static struct cdevsw midi_cdevsw = {
	.d_version = D_VERSION,
	.d_open = midi_open,
	.d_close = midi_close,
	.d_read = midi_read,
	.d_write = midi_write,
	.d_ioctl = midi_ioctl,
	.d_poll = midi_poll,
	.d_name = "midi",
};

static size_t
midi_queue_avail(struct midi_queue *q)
{
	return (q->size - q->used);
}

static void
midi_queue_enq(struct midi_queue *q, uint8_t *src, size_t n)
{
	KASSERT(n <= midi_queue_avail(q), ("%s: bad length %zu", __func__, n));

	memcpy(q->buf + q->used, src, n);
	q->used += n;
}

static void
midi_queue_deq(struct midi_queue *q, uint8_t *dst, size_t n)
{
	KASSERT(n <= q->used, ("%s: bad length %zu", __func__, n));

	memcpy(dst, q->buf, n);
	q->used -= n;
	memmove(q->buf, q->buf + n, q->used);
}

static void
midi_destroy(struct midi_softc *sc)
{
	struct midi_port *m;
	int i;

	mtx_lock(&sc->lock);
	sc->dying = true;
	for (i = 0; i < sc->nchan; i++) {
		m = &sc->ports[i];
		wakeup(&m->rchan);
		wakeup(&m->wchan);
		m->rchan = 0;
		m->wchan = 0;
	}
	mtx_unlock(&sc->lock);

	for (i = 0; i < sc->nchan; i++) {
		m = &sc->ports[i];
		seldrain(&m->rsel);
		seldrain(&m->wsel);
		if (m->cdev != NULL)
			destroy_dev(m->cdev);
	}
	for (i = 0; i < sc->nchan; i++) {
		m = &sc->ports[i];
		free(m->inq.buf, M_MIDI);
		free(m->outq.buf, M_MIDI);
	}
	free(sc->ports, M_MIDI);
	sc->ports = NULL;
	sc->nchan = 0;
	mtx_destroy(&sc->lock);
}

/*
 * Register a new midi device with nchan ports, each with its own cdev.
 */
int
midi_register(device_t dev, midi_callback_t *cb, void *devinfo, int nchan,
    size_t inqsize, size_t outqsize)
{
	struct make_dev_args args;
	struct midi_softc *sc;
	struct midi_port *m;
	int err, i, unit;

	if (nchan <= 0 || (inqsize == 0 && outqsize == 0))
		return (EINVAL);

	unit = device_get_unit(dev);

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->cb = cb;
	sc->devinfo = devinfo;
	sc->nchan = nchan;
	sc->ports = malloc(nchan * sizeof(*sc->ports), M_MIDI,
	    M_WAITOK | M_ZERO);
	mtx_init(&sc->lock, device_get_nameunit(dev), "raw midi", MTX_DEF);

	for (i = 0; i < nchan; i++) {
		m = &sc->ports[i];
		m->sc = sc;
		m->chan = i;
		if (inqsize != 0) {
			m->inq.buf = malloc(inqsize, M_MIDI, M_WAITOK);
			m->inq.size = inqsize;
		}
		if (outqsize != 0) {
			m->outq.buf = malloc(outqsize, M_MIDI, M_WAITOK);
			m->outq.size = outqsize;
		}
		m->hiwat = outqsize / 2;
	}

	for (i = 0; i < nchan; i++) {
		m = &sc->ports[i];
		make_dev_args_init(&args);
		args.mda_devsw = &midi_cdevsw;
		args.mda_unit = unit;
		args.mda_uid = UID_ROOT;
		args.mda_gid = GID_AUDIO;
		args.mda_mode = 0660;
		args.mda_si_drv1 = m;
		err = make_dev_s(&args, &m->cdev, "midi%d.%d", unit, i);
		if (err != 0) {
			device_printf(dev,
			    "failed to create midi%d.%d: error %d\n",
			    unit, i, err);
			midi_destroy(sc);
			return (err);
		}
	}

	return (0);
}

int
midi_unregister(device_t dev)
{
	struct midi_softc *sc = device_get_softc(dev);

	midi_destroy(sc);

	return (0);
}

/*
 * midi_in: process all data until the queue is full, then discards the rest.
 * Since midi_in is a state machine, data discards can cause it to get out of
 * whack.  Process as much as possible.  It calls, wakeup, selnotify and
 * psignal at most once.
 */
size_t
midi_in(device_t dev, int chan, uint8_t *buf, size_t size)
{
	struct midi_softc *sc = device_get_softc(dev);
	struct midi_port *m;
	size_t used = 0;

	if (chan < 0 || chan >= sc->nchan)
		return (0);
	m = &sc->ports[chan];

	mtx_lock(&sc->lock);

	if (!(m->flags & M_RX)) {
		/* We should return 0 but this may stop receiving/sending. */
		mtx_unlock(&m->sc->lock);
		return (size);
	}

	if (size <= midi_queue_avail(&m->inq)) {
		used = size;
		midi_queue_enq(&m->inq, buf, size);
	} else {
		mtx_unlock(&m->sc->lock);
		return 0;
	}
	if (m->rchan) {
		wakeup(&m->rchan);
		m->rchan = 0;
	}
	selwakeup(&m->rsel);
	mtx_unlock(&m->sc->lock);
	return used;
}

/*
 * midi_out: The only clearer of the M_TXPEND flag.
 */
size_t
midi_out(device_t dev, int chan, uint8_t *buf, size_t size)
{
	struct midi_softc *sc = device_get_softc(dev);
	struct midi_port *m;
	size_t used;

	if (chan < 0 || chan >= sc->nchan)
		return (0);
	m = &sc->ports[chan];

	mtx_lock(&sc->lock);

	if (!(m->flags & M_TXPEND)) {
		mtx_unlock(&m->sc->lock);
		return (0);
	}

	used = ulmin(size, m->outq.used);
	if (used)
		midi_queue_deq(&m->outq, buf, used);
	if (m->outq.used == 0) {
		m->flags &= ~M_TXPEND;
		m->sc->cb(m->sc->devinfo, m->chan, m->flags);
	}
	if (used && midi_queue_avail(&m->outq) > m->hiwat) {
		if (m->wchan) {
			wakeup(&m->wchan);
			m->wchan = 0;
		}
		selwakeup(&m->wsel);
	}
	mtx_unlock(&m->sc->lock);
	return used;
}

int
midi_open(struct cdev *i_dev, int flags, int mode, struct thread *td)
{
	struct midi_port *m = i_dev->si_drv1;
	int retval = 0;

	if (m == NULL)
		return ENXIO;

	mtx_lock(&m->sc->lock);
	if (m->sc->dying) {
		mtx_unlock(&m->sc->lock);
		return (ENXIO);
	}

	if (flags & FREAD) {
		if (m->inq.size == 0)
			retval = ENXIO;
		else if (m->flags & M_RX)
			retval = EBUSY;
		if (retval)
			goto err;
	}
	if (flags & FWRITE) {
		if (m->outq.size == 0)
			retval = ENXIO;
		else if (m->flags & M_TX)
			retval = EBUSY;
		if (retval)
			goto err;
	}

	m->rchan = 0;
	m->wchan = 0;

	if (flags & FREAD) {
		m->flags |= M_RX;
		/*
	         * Only clear the inq, the outq might still have data to drain
	         * from a previous session
	         */
		m->inq.used = 0;
	}

	if (flags & FWRITE)
		m->flags |= M_TX;

	m->sc->cb(m->sc->devinfo, m->chan, m->flags);

err:
	mtx_unlock(&m->sc->lock);
	return retval;
}

int
midi_close(struct cdev *i_dev, int flags, int mode, struct thread *td)
{
	struct midi_port *m = i_dev->si_drv1;
	int retval = 0;

	if (m == NULL)
		return ENXIO;

	mtx_lock(&m->sc->lock);
	if (m->sc->dying) {
		mtx_unlock(&m->sc->lock);
		return (ENXIO);
	}

	if ((flags & FREAD && !(m->flags & M_RX)) ||
	    (flags & FWRITE && !(m->flags & M_TX))) {
		retval = ENXIO;
		goto err;
	}

	if (flags & FREAD)
		m->flags &= ~M_RX;
	if (flags & FWRITE)
		m->flags &= ~M_TX;

	m->sc->cb(m->sc->devinfo, m->chan, m->flags);

err:
	mtx_unlock(&m->sc->lock);
	return retval;
}

/*
 * TODO: midi_read, per oss programmer's guide pg. 42 should return as soon
 * as data is available.
 */
int
midi_read(struct cdev *i_dev, struct uio *uio, int ioflag)
{
#define MIDI_RSIZE 32
	struct midi_port *m = i_dev->si_drv1;
	int retval;
	size_t used;
	uint8_t buf[MIDI_RSIZE];

	retval = EIO;

	if (m == NULL)
		goto err0;

	mtx_lock(&m->sc->lock);

	if (!(m->flags & M_RX))
		goto err1;

	while (uio->uio_resid > 0) {
		while (m->inq.used == 0) {
			if (m->sc->dying) {
				retval = ENXIO;
				goto err1;
			}
			retval = EWOULDBLOCK;
			if (ioflag & O_NONBLOCK)
				goto err1;
			m->rchan = 1;
			retval = msleep(&m->rchan, &m->sc->lock,
			    PCATCH | PDROP, "midi RX", 0);
			if (retval)
				goto err0;
			mtx_lock(&m->sc->lock);
			m->rchan = 0;
		}
		/*
	         * At this point, it is certain that m->inq has data
	         */

		used = ulmin(m->inq.used, (size_t)uio->uio_resid);
		used = ulmin(used, MIDI_RSIZE);

		midi_queue_deq(&m->inq, buf, used);
		mtx_unlock(&m->sc->lock);
		retval = uiomove(buf, used, uio);
		if (retval)
			goto err0;
		mtx_lock(&m->sc->lock);
	}

	/*
	 * If we Made it here then transfer is good
	 */
	retval = 0;
err1:
	mtx_unlock(&m->sc->lock);
err0:
	return retval;
}

/*
 * midi_write: The only setter of M_TXPEND
 */

int
midi_write(struct cdev *i_dev, struct uio *uio, int ioflag)
{
#define MIDI_WSIZE 32
	struct midi_port *m = i_dev->si_drv1;
	int retval;
	size_t used;
	uint8_t buf[MIDI_WSIZE];

	retval = EIO;
	if (m == NULL)
		goto err0;

	mtx_lock(&m->sc->lock);

	if (!(m->flags & M_TX))
		goto err1;

	while (uio->uio_resid > 0) {
		while (midi_queue_avail(&m->outq) == 0) {
			if (m->sc->dying) {
				retval = ENXIO;
				goto err1;
			}
			retval = EWOULDBLOCK;
			if (ioflag & O_NONBLOCK)
				goto err1;
			m->wchan = 1;
			retval = msleep(&m->wchan, &m->sc->lock,
			    PCATCH | PDROP, "midi TX", 0);
			if (retval)
				goto err0;
			mtx_lock(&m->sc->lock);
			m->wchan = 0;
		}

		/*
	         * We are certain than data can be placed on the queue
	         */

		used = ulmin(midi_queue_avail(&m->outq), (size_t)uio->uio_resid);
		used = ulmin(used, MIDI_WSIZE);

		mtx_unlock(&m->sc->lock);
		retval = uiomove(buf, used, uio);
		if (retval)
			goto err0;
		mtx_lock(&m->sc->lock);
		midi_queue_enq(&m->outq, buf, used);
		/*
	         * Inform the bottom half that data can be written
	         */
		if (!(m->flags & M_TXPEND)) {
			m->flags |= M_TXPEND;
			m->sc->cb(m->sc->devinfo, m->chan, m->flags);
		}
	}
	/*
	 * If we Made it here then transfer is good
	 */
	retval = 0;
err1:
	mtx_unlock(&m->sc->lock);
err0:	return retval;
}

int
midi_ioctl(struct cdev *i_dev __unused, unsigned long cmd __unused,
    caddr_t arg __unused, int mode __unused, struct thread *td __unused)
{
	return ENXIO;
}

int
midi_poll(struct cdev *i_dev, int events, struct thread *td)
{
	struct midi_port *m = i_dev->si_drv1;
	int revents = 0;

	if (m == NULL)
		return 0;

	mtx_lock(&m->sc->lock);
	if (m->sc->dying) {
		mtx_unlock(&m->sc->lock);
		/* XXX many clients do not understand POLLNVAL */
		return (events & (POLLHUP | POLLPRI | POLLIN | POLLRDNORM |
		    POLLOUT | POLLWRNORM));
	}

	if (events & (POLLIN | POLLRDNORM)) {
		if (m->inq.used != 0)
			revents |= events & (POLLIN | POLLRDNORM);
		else
			selrecord(td, &m->rsel);
	}
	if (events & (POLLOUT | POLLWRNORM)) {
		if (midi_queue_avail(&m->outq) > m->hiwat)
			revents |= events & (POLLOUT | POLLWRNORM);
		else
			selrecord(td, &m->wsel);
	}

	mtx_unlock(&m->sc->lock);

	return (revents);
}
