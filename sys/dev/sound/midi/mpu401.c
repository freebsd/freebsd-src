/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003 Mathew Kanner
 * All rights reserved.
 * Copyright (c) 2026 The FreeBSD Foundation
 *
 * Portions of this software were developed by Christos Margiolis
 * <christos@FreeBSD.org> under sponsorship from the FreeBSD Foundation.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <dev/sound/midi/midi.h>
#include <dev/sound/midi/mpu401.h>

#define MPU_QUEUE_SIZE	128
#define MPU_INTR_BUF	16

#define MPU_CMD(m, d)	(m)->ops->write((m)->devinfo, MPU_CMDPORT, (d))
#define MPU_STATUS(m)	(m)->ops->read((m)->devinfo, MPU_STATPORT)
#define MPU_READ(m)	(m)->ops->read((m)->devinfo, MPU_DATAPORT)
#define MPU_WRITE(m, d)	(m)->ops->write((m)->devinfo, MPU_DATAPORT, (d))

struct mpu401 {
	/*
	 * Must be first so that it can be easily fetched with
	 * device_get_softc().
	 */
	struct midi_softc	midi;
	struct mtx		lock;
	const struct mpu401_ops	*ops;
	void			*devinfo;
	int			flags;
	struct callout		timer;
};

static void
mpu401_mcallback(void *arg, int chan __unused, int flags)
{
	struct mpu401 *m = arg;

	if ((flags & M_TXPEND) != 0)
		callout_reset(&m->timer, 1, mpu401_intr, m);
	m->flags = flags;
}

void
mpu401_intr(void *arg)
{
	struct mpu401 *m = arg;
	size_t i;
	uint8_t buf[MPU_INTR_BUF], s;

	mtx_lock(&m->lock);

	i = 0;
	s = MPU_STATUS(m);
	while ((s & MPU_INPUTBUSY) == 0 && i < MPU_INTR_BUF) {
		buf[i] = MPU_READ(m);
		i++;
		s = MPU_STATUS(m);
	}
	if (i != 0)
		midi_in(m->midi.dev, 0, buf, i);
	i = 0;
	while ((s & MPU_OUTPUTBUSY) == 0 && i < MPU_INTR_BUF) {
		if (midi_out(m->midi.dev, 0, buf, 1) == 0)
			break;
		MPU_WRITE(m, *buf);
		i++;
		s = MPU_STATUS(m);
	}

	if ((m->flags & M_TXPEND) != 0)
		callout_reset(&m->timer, 1, mpu401_intr, m);

	mtx_unlock(&m->lock);
}

static int
mpu401_probe(device_t dev)
{
	struct mpu401_ivars *ivars = device_get_ivars(dev);

	if (ivars == NULL)
		return (ENXIO);

	device_set_desc(dev, ivars->desc != NULL ? ivars->desc :
	    "MPU-401 MIDI");

	return (BUS_PROBE_DEFAULT);
}

static int
mpu401_attach(device_t dev)
{
	struct mpu401 *m = device_get_softc(dev);
	struct mpu401_ivars *ivars = device_get_ivars(dev);
	int err;

	m->ops = ivars->ops;
	m->devinfo = ivars->devinfo;
	mtx_init(&m->lock, device_get_nameunit(dev), "mpu401", MTX_DEF);
	callout_init(&m->timer, 1);

	MPU_CMD(m, MPU_RESET);
	MPU_CMD(m, MPU_UART);

	err = midi_register(dev, &m->lock, mpu401_mcallback, m, 1,
	    MPU_QUEUE_SIZE, MPU_QUEUE_SIZE);
	if (err != 0) {
		callout_drain(&m->timer);
		mtx_destroy(&m->lock);
		return (err);
	}

	return (0);
}

static int
mpu401_detach(device_t dev)
{
	struct mpu401 *m = device_get_softc(dev);
	int err;

	MPU_CMD(m, MPU_RESET);
	err = midi_unregister(dev);
	/*
	 * The ports are gone, but a driver interrupt handler can still
	 * call mpu401_intr(), so stop it re-arming the callout.
	 */
	mtx_lock(&m->lock);
	m->flags = 0;
	mtx_unlock(&m->lock);
	callout_drain(&m->timer);
	mtx_destroy(&m->lock);

	return (err);
}

static device_method_t mpu401_methods[] = {
	DEVMETHOD(device_probe,		mpu401_probe),
	DEVMETHOD(device_attach,	mpu401_attach),
	DEVMETHOD(device_detach,	mpu401_detach),
	DEVMETHOD_END
};

static driver_t mpu401_driver = {
	"midi",
	mpu401_methods,
	sizeof(struct mpu401),
};

DRIVER_MODULE(midi, pcm, mpu401_driver, 0, 0);
DRIVER_MODULE(midi, csa, mpu401_driver, 0, 0);
DRIVER_MODULE(midi, emu10kx, mpu401_driver, 0, 0);
