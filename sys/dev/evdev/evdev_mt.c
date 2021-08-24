/*-
 * Copyright (c) 2016 Vladimir Kondratyev <wulf@FreeBSD.org>
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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/systm.h>

#include <dev/evdev/evdev.h>
#include <dev/evdev/evdev_private.h>
#include <dev/evdev/input.h>

#ifdef DEBUG
#define	debugf(fmt, args...)	printf("evdev: " fmt "\n", ##args)
#else
#define	debugf(fmt, args...)
#endif

typedef	u_int	slotset_t;

_Static_assert(MAX_MT_SLOTS < sizeof(slotset_t) * 8, "MAX_MT_SLOTS too big");

#define FOREACHBIT(v, i) \
	for ((i) = ffs(v) - 1; (i) != -1; (i) = ffs((v) & (~1 << (i))) - 1)

struct {
	uint16_t	mt;
	uint16_t	st;
	int32_t		max;
} static evdev_mtstmap[] = {
	{ ABS_MT_POSITION_X,	ABS_X,		0 },
	{ ABS_MT_POSITION_Y,	ABS_Y,		0 },
	{ ABS_MT_PRESSURE,	ABS_PRESSURE,	255 },
	{ ABS_MT_TOUCH_MAJOR,	ABS_TOOL_WIDTH,	15 },
};

struct evdev_mt_slot {
	int32_t		val[MT_CNT];
};

struct evdev_mt {
	int			last_reported_slot;
	u_int			mtst_events;
	/* the set of slots with active touches */
	slotset_t		touches;
	/* the set of slots with unsynchronized state */
	slotset_t		frame;
	struct evdev_mt_slot	slots[];
};

static void	evdev_mt_send_st_compat(struct evdev_dev *);
static void	evdev_mt_send_autorel(struct evdev_dev *);

static inline int
ffc_slot(struct evdev_dev *evdev, slotset_t slots)
{
	return (ffs(~slots & (2U << MAXIMAL_MT_SLOT(evdev)) - 1) - 1);
}

void
evdev_mt_init(struct evdev_dev *evdev)
{
	int slot, slots;

	slots = MAXIMAL_MT_SLOT(evdev) + 1;

	evdev->ev_mt = malloc(offsetof(struct evdev_mt, slots) +
	     sizeof(struct evdev_mt_slot) * slots, M_EVDEV, M_WAITOK | M_ZERO);

	/* Initialize multitouch protocol type B states */
	for (slot = 0; slot < slots; slot++)
		evdev->ev_mt->slots[slot].val[ABS_MT_INDEX(ABS_MT_TRACKING_ID)]
		    = -1;

	if (bit_test(evdev->ev_flags, EVDEV_FLAG_MT_STCOMPAT))
		evdev_support_mt_compat(evdev);
}

void
evdev_mt_free(struct evdev_dev *evdev)
{
	free(evdev->ev_mt, M_EVDEV);
}

void
evdev_mt_sync_frame(struct evdev_dev *evdev)
{
	if (bit_test(evdev->ev_flags, EVDEV_FLAG_MT_AUTOREL))
		evdev_mt_send_autorel(evdev);
	if (evdev->ev_report_opened &&
	    bit_test(evdev->ev_flags, EVDEV_FLAG_MT_STCOMPAT))
		evdev_mt_send_st_compat(evdev);
	evdev->ev_mt->frame = 0;
}

int
evdev_mt_get_last_slot(struct evdev_dev *evdev)
{
	return (evdev->ev_mt->last_reported_slot);
}

void
evdev_mt_set_last_slot(struct evdev_dev *evdev, int slot)
{
	struct evdev_mt *mt = evdev->ev_mt;

	MPASS(slot >= 0 && slot <= MAXIMAL_MT_SLOT(evdev));

	mt->frame |= 1U << slot;
	mt->last_reported_slot = slot;
}

int32_t
evdev_mt_get_value(struct evdev_dev *evdev, int slot, int16_t code)
{
	struct evdev_mt *mt = evdev->ev_mt;

	MPASS(slot >= 0 && slot <= MAXIMAL_MT_SLOT(evdev));

	return (mt->slots[slot].val[ABS_MT_INDEX(code)]);
}

void
evdev_mt_set_value(struct evdev_dev *evdev, int slot, int16_t code,
    int32_t value)
{
	struct evdev_mt *mt = evdev->ev_mt;

	MPASS(slot >= 0 && slot <= MAXIMAL_MT_SLOT(evdev));

	if (code == ABS_MT_TRACKING_ID) {
		if (value != -1)
			mt->touches |= 1U << slot;
		else
			mt->touches &= ~(1U << slot);
	}
	mt->slots[slot].val[ABS_MT_INDEX(code)] = value;
}

int
evdev_get_mt_slot_by_tracking_id(struct evdev_dev *evdev, int32_t tracking_id)
{
	struct evdev_mt *mt = evdev->ev_mt;
	int slot;

	FOREACHBIT(mt->touches, slot)
		if (evdev_mt_get_value(evdev, slot, ABS_MT_TRACKING_ID) ==
		    tracking_id)
			return (slot);
	/*
	 * Do not allow allocation of new slot in a place of just
	 * released one within the same report.
	 */
	return (ffc_slot(evdev, mt->touches | mt->frame));
}

static inline int32_t
evdev_mt_normalize(int32_t value, int32_t mtmin, int32_t mtmax, int32_t stmax)
{
	if (stmax != 0 && mtmax != mtmin) {
		value = (value - mtmin) * stmax / (mtmax - mtmin);
		value = MAX(MIN(value, stmax), 0);
	}
	return (value);
}

void
evdev_support_mt_compat(struct evdev_dev *evdev)
{
	struct input_absinfo *ai;
	int i;

	if (evdev->ev_absinfo == NULL)
		return;

	evdev_support_event(evdev, EV_KEY);
	evdev_support_key(evdev, BTN_TOUCH);

	/* Touchscreens should not advertise tap tool capabilities */
	if (!bit_test(evdev->ev_prop_flags, INPUT_PROP_DIRECT))
		evdev_support_nfingers(evdev, MAXIMAL_MT_SLOT(evdev) + 1);

	/* Echo 0-th MT-slot as ST-slot */
	for (i = 0; i < nitems(evdev_mtstmap); i++) {
		if (!bit_test(evdev->ev_abs_flags, evdev_mtstmap[i].mt) ||
		     bit_test(evdev->ev_abs_flags, evdev_mtstmap[i].st))
			continue;
		ai = evdev->ev_absinfo + evdev_mtstmap[i].mt;
		evdev->ev_mt->mtst_events |= 1U << i;
		if (evdev_mtstmap[i].max != 0)
			evdev_support_abs(evdev, evdev_mtstmap[i].st,
			    0,
			    evdev_mtstmap[i].max,
			    0,
			    evdev_mt_normalize(
			      ai->flat, 0, ai->maximum, evdev_mtstmap[i].max),
			    0);
		else
			evdev_support_abs(evdev, evdev_mtstmap[i].st,
			    ai->minimum,
			    ai->maximum,
			    0,
			    ai->flat,
			    ai->resolution);
	}
}

static void
evdev_mt_send_st_compat(struct evdev_dev *evdev)
{
	struct evdev_mt *mt = evdev->ev_mt;
	int nfingers, i, st_slot;

	EVDEV_LOCK_ASSERT(evdev);

	nfingers = bitcount(mt->touches);
	evdev_send_event(evdev, EV_KEY, BTN_TOUCH, nfingers > 0);

	/* Send first active MT-slot state as single touch report */
	st_slot = ffs(mt->touches) - 1;
	if (st_slot != -1)
		FOREACHBIT(mt->mtst_events, i)
			evdev_send_event(evdev, EV_ABS, evdev_mtstmap[i].st,
			    evdev_mt_normalize(evdev_mt_get_value(evdev,
			      st_slot, evdev_mtstmap[i].mt),
			      evdev->ev_absinfo[evdev_mtstmap[i].mt].minimum,
			      evdev->ev_absinfo[evdev_mtstmap[i].mt].maximum,
			      evdev_mtstmap[i].max));

	/* Touchscreens should not report tool taps */
	if (!bit_test(evdev->ev_prop_flags, INPUT_PROP_DIRECT))
		evdev_send_nfingers(evdev, nfingers);

	if (nfingers == 0)
		evdev_send_event(evdev, EV_ABS, ABS_PRESSURE, 0);
}

void
evdev_push_mt_compat(struct evdev_dev *evdev)
{

	EVDEV_ENTER(evdev);
	evdev_mt_send_st_compat(evdev);
	EVDEV_EXIT(evdev);
}

static void
evdev_mt_send_autorel(struct evdev_dev *evdev)
{
	struct evdev_mt *mt = evdev->ev_mt;
	int slot;

	EVDEV_LOCK_ASSERT(evdev);

	FOREACHBIT(mt->touches & ~mt->frame, slot) {
		evdev_send_event(evdev, EV_ABS, ABS_MT_SLOT, slot);
		evdev_send_event(evdev, EV_ABS, ABS_MT_TRACKING_ID, -1);
	}
}

void
evdev_mt_push_autorel(struct evdev_dev *evdev)
{
	EVDEV_ENTER(evdev);
	evdev_mt_send_autorel(evdev);
	EVDEV_EXIT(evdev);
}
