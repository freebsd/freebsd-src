/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003 Mathew Kanner
 * All rights reserved.
 * Copyright (c) 2025-2026 The FreeBSD Foundation
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

#ifndef MIDI_H
#define MIDI_H

#include <sys/types.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#define M_RX		0x01
#define M_TX		0x02
#define M_TXPEND	0x04

struct midi_port;

typedef void midi_callback_t(void *devinfo, int chan, int flags);

struct midi_softc {
	device_t	dev;
	struct mtx	*lock;
	midi_callback_t	*cb;
	void		*devinfo;
	int		nchan;
	bool		dying;
	struct midi_port	*ports;
};

int	midi_register(device_t dev, struct mtx *lock, midi_callback_t *cb,
	    void *devinfo, int nchan, size_t inqsize, size_t outqsize);
int	midi_unregister(device_t dev);

/* Must be called with the lock held. */
size_t	midi_in(device_t dev, int chan, uint8_t *buf, size_t size);
size_t	midi_out(device_t dev, int chan, uint8_t *buf, size_t size);

#endif
