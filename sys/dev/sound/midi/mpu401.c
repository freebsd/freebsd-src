/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003 Mathew Kanner
 * Copyright (c) 2025 Nicolas Provost <dev@nicolas-provost.fr>
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
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/kobj.h>
#include <sys/malloc.h>
#include <sys/bus.h>			/* to get driver_intr_t */

#ifdef HAVE_KERNEL_OPTION_HEADERS
#include "opt_snd.h"
#endif

#include <dev/sound/midi/mpu401.h>
#include <dev/sound/midi/midi.h>

#include "mpu_if.h"
#include "mpufoi_if.h"

#ifndef KOBJMETHOD_END
#define KOBJMETHOD_END	{ NULL, NULL }
#endif

#define MPU_DATAPORT   0
#define MPU_CMDPORT    1
#define MPU_STATPORT   1
#define MPU_RESET      0xff
#define MPU_UART       0x3f
#define MPU_ACK        0xfe
#define MPU_STATMASK   0xc0
#define MPU_OUTPUTBUSY 0x40
#define MPU_INPUTBUSY  0x80
#define MPU_TRYDATA 50
#define MPU_DELAY   2500
#define MPU_INTR_BUF	64

#define CMD(m,d)	MPUFOI_WRITE(m, m->cookie, MPU_CMDPORT,d)
#define STATUS(m)	MPUFOI_READ(m, m->cookie, MPU_STATPORT)
#define READ(m)		MPUFOI_READ(m, m->cookie, MPU_DATAPORT)
#define WRITE(m,d)	MPUFOI_WRITE(m, m->cookie, MPU_DATAPORT,d)
#define RXRDY(m) ( (STATUS(m) & MPU_INPUTBUSY) == 0)
#define TXRDY(m) ( (STATUS(m) & MPU_OUTPUTBUSY) == 0)

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
static void mpu401_mcallbackp(struct snd_midi *, void *, int);
static const char *mpu401_mdescr(struct snd_midi *, void *, int);
static const char *mpu401_mprovider(struct snd_midi *, void *);

static kobj_method_t mpu401_methods[] = {
	KOBJMETHOD(mpu_init, mpu401_minit),
	KOBJMETHOD(mpu_uninit, mpu401_muninit),
	KOBJMETHOD(mpu_inqsize, mpu401_minqsize),
	KOBJMETHOD(mpu_outqsize, mpu401_moutqsize),
	KOBJMETHOD(mpu_callback, mpu401_mcallback),
	KOBJMETHOD(mpu_callbackp, mpu401_mcallbackp),
	KOBJMETHOD(mpu_descr, mpu401_mdescr),
	KOBJMETHOD(mpu_provider, mpu401_mprovider),
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

#define MPU_TIMEOUT 50
#define MPU_PAUSE pause_sbt("mpusetup", SBT_1MS, 0, 0)

static int
mpu401_waitfortx(struct mpu401* m)
{
	int i;

	for (i = 0; i < MPU_TIMEOUT; i++) {
		if (TXRDY(m))
			return (1);
		else if (RXRDY(m))
			(void) READ(m);
		else
			MPU_PAUSE;
	}
	return (0);
}

static int
mpu401_waitforack(struct mpu401* m)
{
	int i;

	for (i = 0; i < MPU_TIMEOUT; i++) {
		if (RXRDY(m) && READ(m) == MPU_ACK)
			return (1);
		else
			MPU_PAUSE;
	}
	return (0);
}

/* Some cards only support UART mode but others will start in
 * "Intelligent" mode, so we must try to switch to UART mode.
 * Cheap cards may not even have a COMMAND register..
 */
static void
mpu401_setup(struct mpu401 *m)
{
	int res = 0;

	/* first, we try to send a reset and get an ACK */
	if (mpu401_waitfortx(m)) {
		CMD(m, MPU_RESET);
		if (mpu401_waitforack(m)) {
			/* ok, send UART command (we can also try on timeout) */
			mpu401_waitfortx(m);
			CMD(m, MPU_UART);
			res = mpu401_waitforack(m);
			printf("mpu401: %s mode\n", res ? "UART" : "unknown");
		}
		else
			printf("mpu401: no ack, probable UART-only device\n");
	}
	else
		printf("mpu401: setup failed\n");
}

static int
mpu401_intr(struct mpu401 *m)
{
	MIDI_TYPE b;
	int i;

	/* Read and buffer all input bytes, then send data.
	 * Note that pending input may inhibits data sending.
	 * Spurious cards may also be sticky..
	 */
	for (i = 0; RXRDY(m) && i < 512; i++) {
		b = READ(m);
		midi_in(m->mid, &b, 1);
	}
	for (i = 0; TXRDY(m) && i < 512; i++) {
		if (midi_out(m->mid, &b, 1))
			WRITE(m, b);
		else
			break;
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
	m = malloc(sizeof(*m), M_MIDI, M_NOWAIT | M_ZERO);

	if (!m)
		return NULL;

	kobj_init((kobj_t)m, cls);

	callout_init(&m->timer, 1);

	m->si = softintr;
	m->cookie = cookie;
	m->flags = 0;
	m->mid = midi_init(&mpu401_class, 0, 0, m);
	if (!m->mid)
		goto err;
	*cb = mpu401_intr;
	mpu401_setup(m);
	return (m);
err:
	printf("mpu401: init error\n");
	free(m, M_MIDI);
	return (NULL);
}

int
mpu401_uninit(struct mpu401 *m)
{
	int retval;

	CMD(m, MPU_RESET);
	retval = midi_uninit(m->mid);
	if (retval)
		return retval;
	free(m, M_MIDI);
	return (0);
}

static int
mpu401_minit(struct snd_midi *sm, void *arg)
{
	return (0);
}

int
mpu401_muninit(struct snd_midi *sm, void *arg)
{
	struct mpu401 *m = arg;

	return (MPUFOI_UNINIT(m, m->cookie));
}

int
mpu401_minqsize(struct snd_midi *sm, void *arg)
{
	return (128);
}

int
mpu401_moutqsize(struct snd_midi *sm, void *arg)
{
	return (128);
}

static void
mpu401_mcallback(struct snd_midi *sm, void *arg, int flags)
{
	struct mpu401 *m = arg;

	if (flags & M_TXEN && m->si) {
		callout_reset(&m->timer, 1, mpu401_timeout, m);
	}
	m->flags = flags;
}

static void
mpu401_mcallbackp(struct snd_midi *sm, void *arg, int flags)
{
	mpu401_mcallback(sm, arg, flags);
}

static const char *
mpu401_mdescr(struct snd_midi *sm, void *arg, int verbosity)
{
	return ("mpu401");
}

static const char *
mpu401_mprovider(struct snd_midi *m, void *arg)
{
	return ("provider mpu401");
}
