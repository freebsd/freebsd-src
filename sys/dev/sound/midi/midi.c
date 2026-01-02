/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003 Mathew Kanner
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 * Copyright (c) 2025 The FreeBSD Foundation
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
#include <sys/kobj.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/poll.h>
#include <sys/selinfo.h>
#include <sys/uio.h>

#ifdef HAVE_KERNEL_OPTION_HEADERS
#include "opt_snd.h"
#endif

#include <dev/sound/midi/midi.h>
#include <dev/sound/midi/midiq.h>

#include "mpu_if.h"

MALLOC_DEFINE(M_MIDI, "midi buffers", "Midi data allocation area");

#define MIDI_NAMELEN   16
struct snd_midi {
	KOBJ_FIELDS;
	struct mtx lock;
	void   *cookie;

	int	unit;
	int	channel;

	int	flags;			/* File flags */
	MIDIQ_HEAD(, char) inq, outq;
	int	rchan, wchan;
	struct selinfo rsel, wsel;
	int	hiwat;			/* QLEN(outq)>High-water -> disable
					 * writes from userland */
	struct cdev *dev;
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

struct unrhdr *dev_unr = NULL;
struct unrhdr *chn_unr = NULL;

/*
 * Register a new midi device.
 *
 * "cookie" is passed to the MPU calls, and is normally set to the driver's
 * softc.
 */
struct snd_midi *
midi_init(kobj_class_t cls, void *cookie)
{
	struct snd_midi *m;
	int inqsize, outqsize;
	uint8_t *ibuf = NULL;
	uint8_t *obuf = NULL;

	m = malloc(sizeof(*m), M_MIDI, M_WAITOK | M_ZERO);
	kobj_init((kobj_t)m, cls);
	inqsize = MPU_INQSIZE(m, cookie);
	outqsize = MPU_OUTQSIZE(m, cookie);

	if (!inqsize && !outqsize)
		goto err1;

	mtx_init(&m->lock, "raw midi", NULL, 0);

	if (inqsize)
		ibuf = malloc(inqsize, M_MIDI, M_WAITOK);
	if (outqsize)
		obuf = malloc(outqsize, M_MIDI, M_WAITOK);

	mtx_lock(&m->lock);

	m->hiwat = outqsize / 2;

	MIDIQ_INIT(m->inq, ibuf, inqsize);
	MIDIQ_INIT(m->outq, obuf, outqsize);

	m->flags = 0;
	m->unit = alloc_unr(dev_unr);
	m->channel = alloc_unr(chn_unr);
	m->cookie = cookie;

	if (MPU_INIT(m, cookie))
		goto err2;

	mtx_unlock(&m->lock);

	m->dev = make_dev(&midi_cdevsw, m->unit, UID_ROOT, GID_WHEEL, 0666,
	    "midi%d.%d", m->unit, m->channel);
	m->dev->si_drv1 = m;

	return m;

err2:
	mtx_destroy(&m->lock);

	free(MIDIQ_BUF(m->inq), M_MIDI);
	free(MIDIQ_BUF(m->outq), M_MIDI);
err1:
	free(m, M_MIDI);
	return NULL;
}

int
midi_uninit(struct snd_midi *m)
{
	mtx_lock(&m->lock);
	if (m->rchan) {
		wakeup(&m->rchan);
		m->rchan = 0;
	}
	if (m->wchan) {
		wakeup(&m->wchan);
		m->wchan = 0;
	}
	mtx_unlock(&m->lock);
	MPU_UNINIT(m, m->cookie);
	destroy_dev(m->dev);
	free_unr(dev_unr, m->unit);
	free_unr(chn_unr, m->channel);
	free(MIDIQ_BUF(m->inq), M_MIDI);
	free(MIDIQ_BUF(m->outq), M_MIDI);
	mtx_destroy(&m->lock);
	free(m, M_MIDI);

	return (0);
}

/*
 * midi_in: process all data until the queue is full, then discards the rest.
 * Since midi_in is a state machine, data discards can cause it to get out of
 * whack.  Process as much as possible.  It calls, wakeup, selnotify and
 * psignal at most once.
 */
int
midi_in(struct snd_midi *m, uint8_t *buf, int size)
{
	int used;

	mtx_lock(&m->lock);

	if (!(m->flags & M_RX)) {
		/* We should return 0 but this may stop receiving/sending. */
		mtx_unlock(&m->lock);
		return (size);
	}

	used = 0;

	if (MIDIQ_AVAIL(m->inq) > size) {
		used = size;
		MIDIQ_ENQ(m->inq, buf, size);
	} else {
		mtx_unlock(&m->lock);
		return 0;
	}
	if (m->rchan) {
		wakeup(&m->rchan);
		m->rchan = 0;
	}
	selwakeup(&m->rsel);
	mtx_unlock(&m->lock);
	return used;
}

/*
 * midi_out: The only clearer of the M_TXEN flag.
 */
int
midi_out(struct snd_midi *m, uint8_t *buf, int size)
{
	int used;

	mtx_lock(&m->lock);

	if (!(m->flags & M_TXEN)) {
		mtx_unlock(&m->lock);
		return (0);
	}

	used = MIN(size, MIDIQ_LEN(m->outq));
	if (used)
		MIDIQ_DEQ(m->outq, buf, used);
	if (MIDIQ_EMPTY(m->outq)) {
		m->flags &= ~M_TXEN;
		MPU_CALLBACK(m, m->cookie, m->flags);
	}
	if (used && MIDIQ_AVAIL(m->outq) > m->hiwat) {
		if (m->wchan) {
			wakeup(&m->wchan);
			m->wchan = 0;
		}
		selwakeup(&m->wsel);
	}
	mtx_unlock(&m->lock);
	return used;
}

int
midi_open(struct cdev *i_dev, int flags, int mode, struct thread *td)
{
	struct snd_midi *m = i_dev->si_drv1;
	int retval;

	if (m == NULL)
		return ENXIO;

	mtx_lock(&m->lock);

	retval = 0;

	if (flags & FREAD) {
		if (MIDIQ_SIZE(m->inq) == 0)
			retval = ENXIO;
		else if (m->flags & M_RX)
			retval = EBUSY;
		if (retval)
			goto err;
	}
	if (flags & FWRITE) {
		if (MIDIQ_SIZE(m->outq) == 0)
			retval = ENXIO;
		else if (m->flags & M_TX)
			retval = EBUSY;
		if (retval)
			goto err;
	}

	m->rchan = 0;
	m->wchan = 0;

	if (flags & FREAD) {
		m->flags |= M_RX | M_RXEN;
		/*
	         * Only clear the inq, the outq might still have data to drain
	         * from a previous session
	         */
		MIDIQ_CLEAR(m->inq);
	}

	if (flags & FWRITE)
		m->flags |= M_TX;

	MPU_CALLBACK(m, m->cookie, m->flags);

err:
	mtx_unlock(&m->lock);
	return retval;
}

int
midi_close(struct cdev *i_dev, int flags, int mode, struct thread *td)
{
	struct snd_midi *m = i_dev->si_drv1;
	int retval;
	int oldflags;

	if (m == NULL)
		return ENXIO;

	mtx_lock(&m->lock);

	if ((flags & FREAD && !(m->flags & M_RX)) ||
	    (flags & FWRITE && !(m->flags & M_TX))) {
		retval = ENXIO;
		goto err;
	}

	oldflags = m->flags;

	if (flags & FREAD)
		m->flags &= ~(M_RX | M_RXEN);
	if (flags & FWRITE)
		m->flags &= ~M_TX;

	if ((m->flags & (M_TXEN | M_RXEN)) != (oldflags & (M_RXEN | M_TXEN)))
		MPU_CALLBACK(m, m->cookie, m->flags);

	mtx_unlock(&m->lock);
	retval = 0;
err:	return retval;
}

/*
 * TODO: midi_read, per oss programmer's guide pg. 42 should return as soon
 * as data is available.
 */
int
midi_read(struct cdev *i_dev, struct uio *uio, int ioflag)
{
#define MIDI_RSIZE 32
	struct snd_midi *m = i_dev->si_drv1;
	int retval;
	int used;
	char buf[MIDI_RSIZE];

	retval = EIO;

	if (m == NULL)
		goto err0;

	mtx_lock(&m->lock);

	if (!(m->flags & M_RX))
		goto err1;

	while (uio->uio_resid > 0) {
		while (MIDIQ_EMPTY(m->inq)) {
			retval = EWOULDBLOCK;
			if (ioflag & O_NONBLOCK)
				goto err1;
			m->rchan = 1;
			retval = msleep(&m->rchan, &m->lock,
			    PCATCH | PDROP, "midi RX", 0);
			/*
			 * We slept, maybe things have changed since last
			 * dying check
			 */
			if (retval == EINTR)
				goto err0;
			if (m != i_dev->si_drv1)
				retval = ENXIO;
			if (retval)
				goto err0;
			mtx_lock(&m->lock);
			m->rchan = 0;
		}
		/*
	         * At this point, it is certain that m->inq has data
	         */

		used = MIN(MIDIQ_LEN(m->inq), uio->uio_resid);
		used = MIN(used, MIDI_RSIZE);

		MIDIQ_DEQ(m->inq, buf, used);
		retval = uiomove(buf, used, uio);
		if (retval)
			goto err1;
	}

	/*
	 * If we Made it here then transfer is good
	 */
	retval = 0;
err1:
	mtx_unlock(&m->lock);
err0:
	return retval;
}

/*
 * midi_write: The only setter of M_TXEN
 */

int
midi_write(struct cdev *i_dev, struct uio *uio, int ioflag)
{
#define MIDI_WSIZE 32
	struct snd_midi *m = i_dev->si_drv1;
	int retval;
	int used;
	char buf[MIDI_WSIZE];

	retval = 0;
	if (m == NULL)
		goto err0;

	mtx_lock(&m->lock);

	if (!(m->flags & M_TX))
		goto err1;

	while (uio->uio_resid > 0) {
		while (MIDIQ_AVAIL(m->outq) == 0) {
			retval = EWOULDBLOCK;
			if (ioflag & O_NONBLOCK)
				goto err1;
			m->wchan = 1;
			retval = msleep(&m->wchan, &m->lock,
			    PCATCH | PDROP, "midi TX", 0);
			/*
			 * We slept, maybe things have changed since last
			 * dying check
			 */
			if (retval == EINTR)
				goto err0;
			if (m != i_dev->si_drv1)
				retval = ENXIO;
			if (retval)
				goto err0;
			mtx_lock(&m->lock);
			m->wchan = 0;
		}

		/*
	         * We are certain than data can be placed on the queue
	         */

		used = MIN(MIDIQ_AVAIL(m->outq), uio->uio_resid);
		used = MIN(used, MIDI_WSIZE);

		retval = uiomove(buf, used, uio);
		if (retval)
			goto err1;
		MIDIQ_ENQ(m->outq, buf, used);
		/*
	         * Inform the bottom half that data can be written
	         */
		if (!(m->flags & M_TXEN)) {
			m->flags |= M_TXEN;
			MPU_CALLBACK(m, m->cookie, m->flags);
		}
	}
	/*
	 * If we Made it here then transfer is good
	 */
	retval = 0;
err1:
	mtx_unlock(&m->lock);
err0:	return retval;
}

int
midi_ioctl(struct cdev *i_dev, u_long cmd, caddr_t arg, int mode,
    struct thread *td)
{
	return ENXIO;
}

int
midi_poll(struct cdev *i_dev, int events, struct thread *td)
{
	struct snd_midi *m = i_dev->si_drv1;
	int revents;

	if (m == NULL)
		return 0;

	revents = 0;

	mtx_lock(&m->lock);

	if (events & (POLLIN | POLLRDNORM)) {
		if (!MIDIQ_EMPTY(m->inq))
			revents |= events & (POLLIN | POLLRDNORM);
		else
			selrecord(td, &m->rsel);
	}
	if (events & (POLLOUT | POLLWRNORM)) {
		if (MIDIQ_AVAIL(m->outq) < m->hiwat)
			revents |= events & (POLLOUT | POLLWRNORM);
		else
			selrecord(td, &m->wsel);
	}

	mtx_unlock(&m->lock);

	return (revents);
}

static void
midi_sysinit(void *data __unused)
{
	dev_unr = new_unrhdr(0, INT_MAX, NULL);
	chn_unr = new_unrhdr(0, INT_MAX, NULL);
}
SYSINIT(midi_sysinit, SI_SUB_DRIVERS, SI_ORDER_FIRST, midi_sysinit, NULL);

static void
midi_sysuninit(void *data __unused)
{
	if (dev_unr != NULL)
		delete_unrhdr(dev_unr);
	if (chn_unr != NULL)
		delete_unrhdr(chn_unr);
}
SYSUNINIT(midi_sysuninit, SI_SUB_DRIVERS, SI_ORDER_ANY, midi_sysuninit, NULL);
