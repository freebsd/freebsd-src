/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Jessica Clarke <jrtc27@FreeBSD.org>
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>

#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>

#include "config.h"
#include "mevent.h"
#include "rtc_pl031.h"

#define	RTCDR		0x000
#define	RTCMR		0x004
#define	RTCLR		0x008
#define	RTCCR		0x00C
#define	RTCIMSC		0x010
#define	RTCRIS		0x014
#define	RTCMIS		0x018
#define	RTCICR		0x01C

#define	RTCPeriphID0	0xFE0
#define	RTCPeriphID1	0xFE4
#define	RTCPeriphID2	0xFE8
#define	RTCPeriphID3	0xFEC
#define	 _RTCPeriphID_VAL	0x00141031
#define	 RTCPeriphID_VAL(_n)	((_RTCPeriphID_VAL >> (8 * (_n))) & 0xff)

#define	RTCCellID0	0xFF0
#define	RTCCellID1	0xFF4
#define	RTCCellID2	0xFF8
#define	RTCCellID3	0xFFC
#define	 _RTCCellID_VAL		0xb105f00d
#define	 RTCCellID_VAL(_n)	((_RTCCellID_VAL >> (8 * (_n))) & 0xff)

struct rtc_pl031_softc {
	pthread_mutex_t		mtx;

	time_t			last_tick;
	uint32_t		dr;
	uint32_t		mr;
	uint32_t		lr;
	uint8_t			imsc;
	uint8_t			ris;
	uint8_t			prev_mis;

	struct mevent		*mevp;

	void			*arg;
	rtc_pl031_intr_func_t	intr_assert;
	rtc_pl031_intr_func_t	intr_deassert;
};

static void	rtc_pl031_callback(int fd, enum ev_type type, void *param);

/*
 * Returns the current RTC time as number of seconds since 00:00:00 Jan 1, 1970
 */
static time_t
rtc_pl031_time(void)
{
	struct tm tm;
	time_t t;

	time(&t);
	if (get_config_bool_default("rtc.use_localtime", false)) {
		localtime_r(&t, &tm);
		t = timegm(&tm);
	}
	return (t);
}

static void
rtc_pl031_update_mis(struct rtc_pl031_softc *sc)
{
	uint8_t mis;

	mis = sc->ris & sc->imsc;
	if (mis == sc->prev_mis)
		return;

	sc->prev_mis = mis;
	if (mis)
		(*sc->intr_assert)(sc->arg);
	else
		(*sc->intr_deassert)(sc->arg);
}

static uint64_t
rtc_pl031_next_match_ticks(struct rtc_pl031_softc *sc)
{
	uint32_t ticks;

	ticks = sc->mr - sc->dr;
	if (ticks == 0)
		return ((uint64_t)1 << 32);

	return (ticks);
}

static int
rtc_pl031_next_timer_msecs(struct rtc_pl031_softc *sc)
{
	uint64_t ticks;

	ticks = rtc_pl031_next_match_ticks(sc);
	return (MIN(ticks * 1000, INT_MAX));
}

static void
rtc_pl031_update_timer(struct rtc_pl031_softc *sc)
{
	mevent_timer_update(sc->mevp, rtc_pl031_next_timer_msecs(sc));
}

static void
rtc_pl031_tick(struct rtc_pl031_softc *sc, bool from_timer)
{
	bool match;
	time_t now, ticks;

	now = rtc_pl031_time();
	ticks = now - sc->last_tick;
	match = ticks >= 0 &&
	    (uint64_t)ticks >= rtc_pl031_next_match_ticks(sc);
	sc->dr += ticks;
	sc->last_tick = now;

	if (match) {
		sc->ris = 1;
		rtc_pl031_update_mis(sc);
	}

	if (match || from_timer || ticks < 0)
		rtc_pl031_update_timer(sc);
}

static void
rtc_pl031_callback(int fd __unused, enum ev_type type __unused, void *param)
{
	struct rtc_pl031_softc *sc = param;

	pthread_mutex_lock(&sc->mtx);
	rtc_pl031_tick(sc, true);
	pthread_mutex_unlock(&sc->mtx);
}

void
rtc_pl031_write(struct rtc_pl031_softc *sc, int offset, uint32_t value)
{
	pthread_mutex_lock(&sc->mtx);
	rtc_pl031_tick(sc, false);
	switch (offset) {
	case RTCMR:
		sc->mr = value;
		rtc_pl031_update_timer(sc);
		break;
	case RTCLR:
		sc->lr = value;
		sc->dr = sc->lr;
		rtc_pl031_update_timer(sc);
		break;
	case RTCIMSC:
		sc->imsc = value & 1;
		rtc_pl031_update_mis(sc);
		break;
	case RTCICR:
		sc->ris &= ~value;
		rtc_pl031_update_mis(sc);
		break;
	default:
		/* Ignore writes to read-only/unassigned/ID registers */
		break;
	}
	pthread_mutex_unlock(&sc->mtx);
}

uint32_t
rtc_pl031_read(struct rtc_pl031_softc *sc, int offset)
{
	uint32_t reg;

	pthread_mutex_lock(&sc->mtx);
	rtc_pl031_tick(sc, false);
	switch (offset) {
	case RTCDR:
		reg = sc->dr;
		break;
	case RTCMR:
		reg = sc->mr;
		break;
	case RTCLR:
		reg = sc->lr;
		break;
	case RTCCR:
		/* RTC enabled from reset */
		reg = 1;
		break;
	case RTCIMSC:
		reg = sc->imsc;
		break;
	case RTCRIS:
		reg = sc->ris;
		break;
	case RTCMIS:
		reg = sc->ris & sc->imsc;
		break;
	case RTCPeriphID0:
	case RTCPeriphID1:
	case RTCPeriphID2:
	case RTCPeriphID3:
		reg = RTCPeriphID_VAL(offset - RTCPeriphID0);
		break;
	case RTCCellID0:
	case RTCCellID1:
	case RTCCellID2:
	case RTCCellID3:
		reg = RTCCellID_VAL(offset - RTCCellID0);
		break;
	default:
		/* Return 0 in reads from unasigned registers */
		reg = 0;
		break;
	}
	pthread_mutex_unlock(&sc->mtx);

	return (reg);
}

struct rtc_pl031_softc *
rtc_pl031_init(rtc_pl031_intr_func_t intr_assert,
    rtc_pl031_intr_func_t intr_deassert, void *arg)
{
	struct rtc_pl031_softc *sc;
	time_t now;

	sc = calloc(1, sizeof(struct rtc_pl031_softc));

	pthread_mutex_init(&sc->mtx, NULL);

	now = rtc_pl031_time();
	sc->dr = now;
	sc->last_tick = now;
	sc->arg = arg;
	sc->intr_assert = intr_assert;
	sc->intr_deassert = intr_deassert;

	sc->mevp = mevent_add(rtc_pl031_next_timer_msecs(sc), EVF_TIMER,
	    rtc_pl031_callback, sc);

	return (sc);
}
