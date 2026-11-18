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
#include <sys/condvar.h>
#include <sys/conf.h>
#include <sys/event.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/poll.h>
#include <sys/selinfo.h>
#include <sys/uio.h>

#include <dev/sound/midi/midi.h>
#include <dev/sound/sndstat.h>

#define MIDI_RSIZE	32
#define MIDI_WSIZE	32

static MALLOC_DEFINE(M_MIDI, "midi buffers", "MIDI data allocation area");

struct midi_queue {
	uint8_t	*buf;
	size_t	size;
	size_t	used;	/* Bytes queued. */
};

struct midi_port {
	struct midi_softc *sc;
	int		chan;

	int		flags;
	struct midi_queue inq, outq;
	struct cv	rcv, wcv;
	struct selinfo	rsel, wsel;
	/* Free space below which writes from userland are blocked. */
	size_t		hiwat;
	struct cdev	*cdev;
};

static d_open_t midi_open;
static d_close_t midi_close;
static d_ioctl_t midi_ioctl;
static d_read_t midi_read;
static d_write_t midi_write;
static d_poll_t midi_poll;
static d_kqfilter_t midi_kqfilter;

static struct cdevsw midi_cdevsw = {
	.d_version = D_VERSION,
	.d_open = midi_open,
	.d_close = midi_close,
	.d_read = midi_read,
	.d_write = midi_write,
	.d_ioctl = midi_ioctl,
	.d_poll = midi_poll,
	.d_kqfilter = midi_kqfilter,
	.d_name = "midi",
};

static size_t
midi_queue_avail(const struct midi_queue *q)
{
	return (q->size - q->used);
}

static void
midi_queue_enq(struct midi_queue *q, const uint8_t *src, size_t n)
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
	struct midi_port *m, *ports;
	int i, nchan;

	mtx_lock(sc->lock);
	sc->dying = true;
	for (i = 0; i < sc->nchan; i++) {
		m = &sc->ports[i];
		cv_broadcast(&m->rcv);
		cv_broadcast(&m->wcv);
		KNOTE_LOCKED(&m->rsel.si_note, 0);
		KNOTE_LOCKED(&m->wsel.si_note, 0);
	}
	mtx_unlock(sc->lock);

	for (i = 0; i < sc->nchan; i++) {
		m = &sc->ports[i];
		seldrain(&m->rsel);
		seldrain(&m->wsel);
		if (m->cdev != NULL)
			destroy_dev(m->cdev);
	}

	/*
	 * Interrupt handlers may call midi_in() and midi_out(), so make the
	 * ports unreachable before freeing them.
	 */
	mtx_lock(sc->lock);
	ports = sc->ports;
	nchan = sc->nchan;
	sc->ports = NULL;
	sc->nchan = 0;
	mtx_unlock(sc->lock);

	for (i = 0; i < nchan; i++) {
		m = &ports[i];
		free(m->inq.buf, M_MIDI);
		free(m->outq.buf, M_MIDI);
		knlist_clear(&m->rsel.si_note, 0);
		knlist_clear(&m->wsel.si_note, 0);
		knlist_destroy(&m->rsel.si_note);
		knlist_destroy(&m->wsel.si_note);
		cv_destroy(&m->rcv);
		cv_destroy(&m->wcv);
	}
	free(ports, M_MIDI);
}

/*
 * Register a new midi device with nchan ports, each with its own cdev.
 */
int
midi_register(device_t dev, struct mtx *lock, midi_callback_t *cb,
    void *devinfo, int nchan, size_t inqsize, size_t outqsize)
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
	sc->lock = lock;
	sc->cb = cb;
	sc->devinfo = devinfo;
	sc->nchan = nchan;
	sc->ports = malloc(nchan * sizeof(*sc->ports), M_MIDI,
	    M_WAITOK | M_ZERO);

	for (i = 0; i < nchan; i++) {
		m = &sc->ports[i];
		m->sc = sc;
		m->chan = i;
		cv_init(&m->rcv, "midi RX");
		cv_init(&m->wcv, "midi TX");
		knlist_init_mtx(&m->rsel.si_note, sc->lock);
		knlist_init_mtx(&m->wsel.si_note, sc->lock);
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

	sndstat_register(dev, SNDST_TYPE_MIDI, NULL);

	return (0);
}

int
midi_unregister(device_t dev)
{
	struct midi_softc *sc = device_get_softc(dev);

	sndstat_unregister(dev);
	midi_destroy(sc);

	return (0);
}

/*
 * Keep backwards compatibility, for providers that created their device nodes
 * themselves before they used this framework (e.g., snd_uaudio(4)).
 */
void
midi_alias(device_t dev, const char *name)
{
	struct midi_softc *sc = device_get_softc(dev);
	int i, unit;

	unit = device_get_unit(dev);
	for (i = 0; i < sc->nchan; i++)
		make_dev_alias(sc->ports[i].cdev, "%s%d.%d", name, unit, i);
}

/*
 * Queue data received by the provider and wake up readers. Return the number
 * of bytes consumed.
 */
size_t
midi_in(device_t dev, int chan, const uint8_t *buf, size_t size)
{
	struct midi_softc *sc = device_get_softc(dev);
	struct midi_port *m;

	mtx_assert(sc->lock, MA_OWNED);

	if (chan < 0 || chan >= sc->nchan)
		return (0);
	m = &sc->ports[chan];

	if ((m->flags & M_RX) == 0) {
		/*
		 * Nobody is listening. Drop the data, but report it as
		 * consumed, since a provider may take 0 as a reason to stop
		 * receiving.
		 */
		return (size);
	}

	if (size > midi_queue_avail(&m->inq))
		return (0);
	midi_queue_enq(&m->inq, buf, size);
	cv_broadcast(&m->rcv);
	selwakeup(&m->rsel);
	KNOTE_LOCKED(&m->rsel.si_note, 0);

	return (size);
}

/*
 * Dequeue data for the provider to transmit and wake up writers. Clear
 * M_TXPEND and notify the provider once the queue is empty.
 */
size_t
midi_out(device_t dev, int chan, uint8_t *buf, size_t size)
{
	struct midi_softc *sc = device_get_softc(dev);
	struct midi_port *m;
	size_t used;

	mtx_assert(sc->lock, MA_OWNED);

	if (chan < 0 || chan >= sc->nchan)
		return (0);
	m = &sc->ports[chan];

	if ((m->flags & M_TXPEND) == 0)
		return (0);

	used = ulmin(size, m->outq.used);
	if (used != 0)
		midi_queue_deq(&m->outq, buf, used);
	if (m->outq.used == 0) {
		m->flags &= ~M_TXPEND;
		m->sc->cb(m->sc->devinfo, m->chan, m->flags);
	}
	if (used != 0 && midi_queue_avail(&m->outq) > m->hiwat) {
		cv_broadcast(&m->wcv);
		selwakeup(&m->wsel);
		KNOTE_LOCKED(&m->wsel.si_note, 0);
	}

	return (used);
}

static int
midi_open(struct cdev *i_dev, int flags, int mode, struct thread *td)
{
	struct midi_port *m = i_dev->si_drv1;
	int err = 0;

	if (m == NULL)
		return (ENXIO);

	mtx_lock(m->sc->lock);
	if (m->sc->dying) {
		mtx_unlock(m->sc->lock);
		return (ENXIO);
	}

	if ((flags & FREAD) != 0) {
		if (m->inq.size == 0)
			err = ENXIO;
		else if ((m->flags & M_RX) != 0)
			err = EBUSY;
		if (err != 0)
			goto out;
	}
	if ((flags & FWRITE) != 0) {
		if (m->outq.size == 0)
			err = ENXIO;
		else if ((m->flags & M_TX) != 0)
			err = EBUSY;
		if (err != 0)
			goto out;
	}

	if ((flags & FREAD) != 0) {
		m->flags |= M_RX;
		/*
		 * Only clear the inq, the outq might still have data to drain
		 * from a previous session.
		 */
		m->inq.used = 0;
	}

	if ((flags & FWRITE) != 0)
		m->flags |= M_TX;

	m->sc->cb(m->sc->devinfo, m->chan, m->flags);

out:
	mtx_unlock(m->sc->lock);
	return (err);
}

static int
midi_close(struct cdev *i_dev, int flags, int mode, struct thread *td)
{
	struct midi_port *m = i_dev->si_drv1;
	int err = 0;

	if (m == NULL)
		return (ENXIO);

	mtx_lock(m->sc->lock);
	if (m->sc->dying) {
		mtx_unlock(m->sc->lock);
		return (ENXIO);
	}

	if (((flags & FREAD) != 0 && (m->flags & M_RX) == 0) ||
	    ((flags & FWRITE) != 0 && (m->flags & M_TX) == 0)) {
		err = ENXIO;
		goto out;
	}

	if ((flags & FREAD) != 0)
		m->flags &= ~M_RX;
	if ((flags & FWRITE) != 0)
		m->flags &= ~M_TX;

	m->sc->cb(m->sc->devinfo, m->chan, m->flags);

out:
	mtx_unlock(m->sc->lock);
	return (err);
}

static int
midi_read(struct cdev *i_dev, struct uio *uio, int ioflag)
{
	struct midi_port *m = i_dev->si_drv1;
	ssize_t resid;
	size_t used;
	int err = EIO;
	uint8_t buf[MIDI_RSIZE];

	if (m == NULL)
		goto out;

	mtx_lock(m->sc->lock);

	if ((m->flags & M_RX) == 0)
		goto out_unlock;

	resid = uio->uio_resid;

	while (uio->uio_resid > 0) {
		while (m->inq.used == 0) {
			if (m->sc->dying) {
				err = ENXIO;
				goto out_unlock;
			}
			/* Return what we have instead of waiting for more. */
			if (uio->uio_resid != resid)
				goto done;
			err = EWOULDBLOCK;
			if ((ioflag & O_NONBLOCK) != 0)
				goto out_unlock;
			err = cv_wait_sig(&m->rcv, m->sc->lock);
			if (err != 0)
				goto out_unlock;
		}
		used = ulmin(m->inq.used, (size_t)uio->uio_resid);
		used = ulmin(used, MIDI_RSIZE);

		midi_queue_deq(&m->inq, buf, used);
		mtx_unlock(m->sc->lock);
		err = uiomove(buf, used, uio);
		if (err != 0)
			goto out;
		mtx_lock(m->sc->lock);
	}

done:
	err = 0;
out_unlock:
	mtx_unlock(m->sc->lock);
out:
	return (err);
}

static int
midi_write(struct cdev *i_dev, struct uio *uio, int ioflag)
{
	struct midi_port *m = i_dev->si_drv1;
	size_t used, n;
	int err = EIO;
	uint8_t buf[MIDI_WSIZE];

	if (m == NULL)
		goto out;

	mtx_lock(m->sc->lock);

	if ((m->flags & M_TX) == 0)
		goto out_unlock;

	while (uio->uio_resid > 0) {
		while (midi_queue_avail(&m->outq) == 0) {
			if (m->sc->dying) {
				err = ENXIO;
				goto out_unlock;
			}
			err = EWOULDBLOCK;
			if ((ioflag & O_NONBLOCK) != 0)
				goto out_unlock;
			err = cv_wait_sig(&m->wcv, m->sc->lock);
			if (err != 0)
				goto out_unlock;
		}

		used = ulmin(midi_queue_avail(&m->outq), (size_t)uio->uio_resid);
		used = ulmin(used, MIDI_WSIZE);

		mtx_unlock(m->sc->lock);
		err = uiomove(buf, used, uio);
		if (err != 0)
			goto out;
		mtx_lock(m->sc->lock);
		/*
		 * Another thread may have taken the space while the lock was
		 * dropped.
		 */
		n = ulmin(used, midi_queue_avail(&m->outq));
		midi_queue_enq(&m->outq, buf, n);
		if ((m->flags & M_TXPEND) == 0) {
			m->flags |= M_TXPEND;
			m->sc->cb(m->sc->devinfo, m->chan, m->flags);
		}
		/*
		 * Give back the bytes that did not fit, so that they do not
		 * get dropped silently.
		 */
		if (n < used) {
			uio->uio_resid += used - n;
			break;
		}
	}
	err = 0;
out_unlock:
	mtx_unlock(m->sc->lock);
out:
	return (err);
}

static int
midi_ioctl(struct cdev *i_dev __unused, unsigned long cmd __unused,
    caddr_t arg __unused, int mode __unused, struct thread *td __unused)
{
	return (ENXIO);
}

static int
midi_poll(struct cdev *i_dev, int events, struct thread *td)
{
	struct midi_port *m = i_dev->si_drv1;
	int revents = 0;

	if (m == NULL)
		return (0);

	mtx_lock(m->sc->lock);
	if (m->sc->dying) {
		mtx_unlock(m->sc->lock);
		/* XXX many clients do not understand POLLNVAL */
		return (events & (POLLHUP | POLLPRI | POLLIN | POLLRDNORM |
		    POLLOUT | POLLWRNORM));
	}

	if ((events & (POLLIN | POLLRDNORM)) != 0) {
		if (m->inq.used != 0)
			revents |= events & (POLLIN | POLLRDNORM);
		else
			selrecord(td, &m->rsel);
	}
	if ((events & (POLLOUT | POLLWRNORM)) != 0) {
		if (midi_queue_avail(&m->outq) > m->hiwat)
			revents |= events & (POLLOUT | POLLWRNORM);
		else
			selrecord(td, &m->wsel);
	}

	mtx_unlock(m->sc->lock);

	return (revents);
}

static void
midi_kqdetach(struct knote *kn)
{
	struct midi_port *m = kn->kn_hook;
	struct knlist *knl;

	knl = (kn->kn_filter == EVFILT_READ) ? &m->rsel.si_note :
	    &m->wsel.si_note;
	mtx_lock(m->sc->lock);
	knlist_remove(knl, kn, 1);
	mtx_unlock(m->sc->lock);
}

static int
midi_kqevent(struct knote *kn, long hint)
{
	struct midi_port *m = kn->kn_hook;

	mtx_assert(m->sc->lock, MA_OWNED);

	if (m->sc->dying) {
		kn->kn_flags |= EV_EOF;
		return (1);
	}
	if (kn->kn_filter == EVFILT_READ) {
		kn->kn_data = m->inq.used;
		return (kn->kn_data > 0);
	}
	kn->kn_data = midi_queue_avail(&m->outq);

	return (kn->kn_data > m->hiwat);
}

static const struct filterops midi_filtops = {
	.f_isfd = 1,
	.f_detach = midi_kqdetach,
	.f_event = midi_kqevent,
};

static int
midi_kqfilter(struct cdev *i_dev, struct knote *kn)
{
	struct midi_port *m = i_dev->si_drv1;
	struct knlist *knl;

	if (m == NULL)
		return (ENXIO);

	switch (kn->kn_filter) {
	case EVFILT_READ:
		knl = &m->rsel.si_note;
		break;
	case EVFILT_WRITE:
		knl = &m->wsel.si_note;
		break;
	default:
		return (EINVAL);
	}
	kn->kn_fop = &midi_filtops;

	mtx_lock(m->sc->lock);
	if (m->sc->dying) {
		mtx_unlock(m->sc->lock);
		return (ENXIO);
	}
	kn->kn_hook = m;
	knlist_add(knl, kn, 1);
	mtx_unlock(m->sc->lock);

	return (0);
}
