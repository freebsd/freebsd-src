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

#ifdef HAVE_KERNEL_OPTION_HEADERS
#include "opt_snd.h"
#endif

#include <dev/sound/midi/mpu401.h>
#include <dev/sound/midi/midi.h>


#define MPU_QUEUE_SIZE	128

#define CMD(m,d)	(m)->ops->write((m)->cookie, MPU_CMDPORT, (d))
#define STATUS(m)	(m)->ops->read((m)->cookie, MPU_STATPORT)
#define READ(m)		(m)->ops->read((m)->cookie, MPU_DATAPORT)
#define WRITE(m,d)	(m)->ops->write((m)->cookie, MPU_DATAPORT, (d))

struct mpu401 {
	const struct mpu401_ops *ops;
	struct snd_midi *mid;
	int	flags;
	void   *cookie;
	struct callout timer;
};

static void
mpu401_mcallback(void *arg, int flags)
{
	struct mpu401 *m = arg;

	if (flags & M_TXPEND)
		callout_reset(&m->timer, 1, mpu401_intr, m);
	m->flags = flags;
}

void
mpu401_intr(void *arg)
{
#define MPU_INTR_BUF	16
	struct mpu401 *m = arg;
	uint8_t b[MPU_INTR_BUF];
	size_t i;
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
		if (midi_out(m->mid, b, 1))
			WRITE(m, *b);
		else
			return;
		i++;
		/* DELAY(100); */
		s = STATUS(m);
	}

	if (m->flags & M_TXPEND)
		callout_reset(&m->timer, 1, mpu401_intr, m);
}

struct mpu401 *
mpu401_init(const struct mpu401_ops *ops, void *cookie)
{
	struct mpu401 *m;

	m = malloc(sizeof(*m), M_MIDI, M_WAITOK | M_ZERO);

	m->ops = ops;
	callout_init(&m->timer, 1);

	m->cookie = cookie;

	CMD(m, MPU_RESET);
	CMD(m, MPU_UART);

	m->mid = midi_init(mpu401_mcallback, m, MPU_QUEUE_SIZE,
	    MPU_QUEUE_SIZE);
	if (!m->mid)
		goto err;
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
