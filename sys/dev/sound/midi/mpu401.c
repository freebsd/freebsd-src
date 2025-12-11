/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003 Mathew Kanner
 * Copyright (c) 2025 Nicolas Provost
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
#define MPU_INTR_BUF   16

#define CMD(m,d)	MPUFOI_WRITE(m, m->cookie, MPU_CMDPORT,d)
#define STATUS(m)	MPUFOI_READ(m, m->cookie, MPU_STATPORT)
#define READ(m)		MPUFOI_READ(m, m->cookie, MPU_DATAPORT)
#define WRITE(m,d)	MPUFOI_WRITE(m, m->cookie, MPU_DATAPORT,d)
#define RXRDY(m) 	((STATUS(m) & MPU_INPUTBUSY) == 0)
#define TXRDY(m) 	((STATUS(m) & MPU_OUTPUTBUSY) == 0)

struct mpu401 {
	KOBJ_FIELDS;
	struct snd_midi *mid;
	int	flags;
	void   *cookie;
};

static mpu401_intr_t mpu401_intr;

static int mpu401_minit(struct snd_midi *, void *);
static int mpu401_muninit(struct snd_midi *, void *);
static int mpu401_minqsize(struct snd_midi *, void *);
static int mpu401_moutqsize(struct snd_midi *, void *);
static void mpu401_mcallback(struct snd_midi *, void *, int);
static void mpu401_mcallbackp(struct snd_midi *, void *, int);

static kobj_method_t mpu401_methods[] = {
	KOBJMETHOD(mpu_init, mpu401_minit),
	KOBJMETHOD(mpu_uninit, mpu401_muninit),
	KOBJMETHOD(mpu_inqsize, mpu401_minqsize),
	KOBJMETHOD(mpu_outqsize, mpu401_moutqsize),
	KOBJMETHOD(mpu_callback, mpu401_mcallback),
	KOBJMETHOD(mpu_callbackp, mpu401_mcallbackp),
	KOBJMETHOD_END
};

DEFINE_CLASS(mpu401, mpu401_methods, 0);

static inline void
mpu401_pause(void)
{
	pause_sbt("mpusetup", SBT_1MS, 0, 0);
}

static int
mpu401_waitfortx(struct mpu401 *m)
{
	int i;

	for (i = 0; i < MPU_TRYDATA; i++) {
		if (TXRDY(m))
			return (1);
		else if (RXRDY(m)) {
			/*
			 * We're initializing: discard input data, if any.
			 */
			(void) READ(m);
		} else {
			mpu401_pause();
		}
	}
	return (0);
}

static int
mpu401_waitforack(struct mpu401 *m)
{
	int i;

	for (i = 0; i < MPU_TRYDATA; i++) {
		if (RXRDY(m) && READ(m) == MPU_ACK)
			return (1);
		else {
			mpu401_pause();
		}
	}
	return (0);
}

/*
 * Some cards only support UART mode but others may start in "Intelligent"
 * mode; in this case, we must switch to UART mode to use raw bytes instead
 * of getting altered MIDI sequences.
 * Old cards may not even have a COMMAND register to do the switch, but there
 * is no such card in the FreeBSD tree.
 * Returns 0 if we're not sure of the MPU state.
 */
static int
mpu401_setup(struct mpu401 *m)
{
	int res;

	/*
	 * First, we try to send a reset and get back one ACK.
	 */
	if (mpu401_waitfortx(m)) {
		CMD(m, MPU_RESET);
		if (mpu401_waitforack(m)) {
			/*
			 * Ok, send the UART command (we can try even on
			 * timeout).
			 */
			mpu401_waitfortx(m);
			CMD(m, MPU_UART);
			res = mpu401_waitforack(m);
			printf("mpu401: %s mode\n", res ? "UART" : "unknown");
		} else {
			/*
			 * For example, this may be the case when reloading a
			 * driver.  But no way to know exactly.
			 */
			printf("mpu401: no ack, device may already be in "
				"UART mode\n");
		}
		return (1);
	} else {
		/*
		 * Failure, the card seems to no accept any command.
		 */
		printf("mpu401: cannot enable UART mode\n");
		return (0);
	}
}

static int
mpu401_intr(struct mpu401 *m)
{
	uint8_t b;
	int i;

	/*
	 * Read and buffer all input bytes, then send data.
	 * Note that pending input may inhibit data sending.
	 * XXX: ideally, we should ensure the input queue of the midi device,
	 * in the midi_in function, has some room available before reading
	 * anything, else we're leaking bytes.
	 */
	for (i = 0; RXRDY(m) && i < MPU_INTR_BUF; i++) {
		b = READ(m);
		midi_in(m->mid, &b, 1);
	}
	for (i = 0; TXRDY(m) && i < MPU_INTR_BUF; i++) {
		if (midi_out(m->mid, &b, 1))
			WRITE(m, b);
		else {
			return (0);
		}
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

	m->cookie = cookie;
	m->flags = 0;

	m->mid = midi_init(&mpu401_class, 0, 0, m);
	if (!m->mid)
		goto err;
	*cb = mpu401_intr;
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
	return 0;
}

static int
mpu401_minit(struct snd_midi *sm, void *arg)
{
	struct mpu401 *m = arg;

	/*
	 * Setting up the MPU in UART mode must be done before the interrupt
	 * handler is enabled by the caller, else we may miss the ACK, which
	 * is read back from the data port of the MPU.
	 */
	if (mpu401_setup(m) == 0) {
		/*
		 * We don't know the exact state of the device, or the device
		 * is not responsive.
		 */
		return (1);
	}
	return (0);
}

int
mpu401_muninit(struct snd_midi *sm, void *arg)
{
	struct mpu401 *m = arg;

	return MPUFOI_UNINIT(m, m->cookie);
}

int
mpu401_minqsize(struct snd_midi *sm, void *arg)
{
	return 128;
}

int
mpu401_moutqsize(struct snd_midi *sm, void *arg)
{
	return 128;
}

static void
mpu401_mcallback(struct snd_midi *sm, void *arg, int flags)
{
	struct mpu401 *m = arg;

	m->flags = flags;
}

static void
mpu401_mcallbackp(struct snd_midi *sm, void *arg, int flags)
{
	mpu401_mcallback(sm, arg, flags);
}
