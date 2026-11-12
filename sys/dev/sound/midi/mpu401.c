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
#include <sys/kobj.h>

#ifdef HAVE_KERNEL_OPTION_HEADERS
#include "opt_snd.h"
#endif

#include <dev/sound/midi/mpu401.h>
#include <dev/sound/midi/midi.h>

#include "mpu_if.h"
#include "mpufoi_if.h"

#define CMD(m,d)	MPUFOI_WRITE(m, m->cookie, MPU_CMDPORT,d)
#define STATUS(m)	MPUFOI_READ(m, m->cookie, MPU_STATPORT)
#define READ(m)		MPUFOI_READ(m, m->cookie, MPU_DATAPORT)
#define WRITE(m,d)	MPUFOI_WRITE(m, m->cookie, MPU_DATAPORT,d)

struct mpu401 {
	KOBJ_FIELDS;
	struct snd_midi *mid;
	int	flags;
	driver_intr_t *si;
	void   *cookie;
	struct callout timer;
};

static void mpu401_timeout(void *m);
static mpu401_intr_t mpu401_intr;

static int mpu401_minit(struct snd_midi *, void *);
static int mpu401_muninit(struct snd_midi *, void *);
static int mpu401_minqsize(struct snd_midi *, void *);
static int mpu401_moutqsize(struct snd_midi *, void *);
static void mpu401_mcallback(struct snd_midi *, void *, int);

static kobj_method_t mpu401_methods[] = {
	KOBJMETHOD(mpu_init, mpu401_minit),
	KOBJMETHOD(mpu_uninit, mpu401_muninit),
	KOBJMETHOD(mpu_inqsize, mpu401_minqsize),
	KOBJMETHOD(mpu_outqsize, mpu401_moutqsize),
	KOBJMETHOD(mpu_callback, mpu401_mcallback),
	KOBJMETHOD_END
};

DEFINE_CLASS(mpu401, mpu401_methods, 0);

void
mpu401_timeout(void *a)
{
	struct mpu401 *m = (struct mpu401 *)a;

	if (m->si)
		(m->si)(m->cookie);

}
static int
mpu401_intr(struct mpu401 *m)
{
#define MPU_INTR_BUF	16
	uint8_t b[MPU_INTR_BUF];
	int i;
	int s;

	i = 0;
	s = STATUS(m);
	while ((s & MPU_INPUTBUSY) == 0 && i < MPU_INTR_BUF) {
		b[i] = READ(m);
		i++;
		s = STATUS(m);
	}
	if (i)
		midi_in(m->mid, b, i);
	i = 0;
	while (!(s & MPU_OUTPUTBUSY) && i < MPU_INTR_BUF) {
		if (midi_out(m->mid, b, 1)) {

			WRITE(m, *b);
		} else {
			return 0;
		}
		i++;
		/* DELAY(100); */
		s = STATUS(m);
	}

	if ((m->flags & M_TXEN) && (m->si)) {
		callout_reset(&m->timer, 1, mpu401_timeout, m);
	}
	return (m->flags & M_TXEN) == M_TXEN;
}

struct mpu401 *
mpu401_init(kobj_class_t cls, void *cookie, driver_intr_t softintr,
    mpu401_intr_t ** cb)
{
	struct mpu401 *m;

	*cb = NULL;
	m = malloc(sizeof(*m), M_MIDI, M_WAITOK | M_ZERO);

	kobj_init((kobj_t)m, cls);

	callout_init(&m->timer, 1);

	m->si = softintr;
	m->cookie = cookie;

	m->mid = midi_init(&mpu401_class, m);
	if (!m->mid)
		goto err;
	*cb = mpu401_intr;
	return m;
err:
	printf("mpu401_init error\n");
	free(m, M_MIDI);
	return NULL;
}

int
mpu401_uninit(struct mpu401 *m)
{
	CMD(m, MPU_RESET);
	midi_uninit(m->mid);
	callout_drain(&m->timer);
	free(m, M_MIDI);
	return 0;
}

static int
mpu401_minit(struct snd_midi *sm __unused, void *arg)
{
	struct mpu401 *m = arg;

	CMD(m, MPU_RESET);
	CMD(m, MPU_UART);
	return 0;
}

static int
mpu401_muninit(struct snd_midi *sm __unused, void *arg)
{
	struct mpu401 *m = arg;

	return MPUFOI_UNINIT(m, m->cookie);
}

static int
mpu401_minqsize(struct snd_midi *sm __unused, void *arg __unused)
{
	return 128;
}

static int
mpu401_moutqsize(struct snd_midi *sm __unused, void *arg __unused)
{
	return 128;
}

static void
mpu401_mcallback(struct snd_midi *sm __unused, void *arg, int flags)
{
	struct mpu401 *m = arg;

	if (flags & M_TXEN && m->si) {
		callout_reset(&m->timer, 1, mpu401_timeout, m);
	}
	m->flags = flags;
}
