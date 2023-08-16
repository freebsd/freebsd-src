/*-
 * Copyright (c) 2016, 2020 Vladimir Kondratyev <wulf@FreeBSD.org>
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
/*-
 * Copyright (c) 2015, 2016 Ulf Brosziewski
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
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

struct evdev_mt {
	int			last_reported_slot;
	uint16_t		tracking_id;
	int32_t			tracking_ids[MAX_MT_SLOTS];
	bool			type_a;
	u_int			mtst_events;
	/* the set of slots with active touches */
	slotset_t		touches;
	/* the set of slots with unsynchronized state */
	slotset_t		frame;
	/* the set of slots to match with active touches */
	slotset_t		match_frame;
	int			match_slot;
	union evdev_mt_slot	*match_slots;
	int			*matrix;
	union evdev_mt_slot	slots[];
};

static void	evdev_mt_support_st_compat(struct evdev_dev *);
static void	evdev_mt_send_st_compat(struct evdev_dev *);
static void	evdev_mt_send_autorel(struct evdev_dev *);
static void	evdev_mt_replay_events(struct evdev_dev *);

static inline int
ffc_slot(struct evdev_dev *evdev, slotset_t slots)
{
	return (ffs(~slots & ((2U << MAXIMAL_MT_SLOT(evdev)) - 1)) - 1);
}

void
evdev_mt_init(struct evdev_dev *evdev)
{
	struct evdev_mt *mt;
	size_t size = offsetof(struct evdev_mt, slots);
	int slot, slots;
	bool type_a;

	type_a = !bit_test(evdev->ev_abs_flags, ABS_MT_SLOT);
	if (type_a) {
		/* Add events produced by MT type A to type B converter */
		evdev_support_abs(evdev,
		    ABS_MT_SLOT, 0, MAX_MT_SLOTS - 1, 0, 0, 0);
		evdev_support_abs(evdev,
		    ABS_MT_TRACKING_ID, -1, MAX_MT_SLOTS - 1, 0, 0, 0);
	}

	slots = MAXIMAL_MT_SLOT(evdev) + 1;
	size += sizeof(mt->slots[0]) * slots;
	if (bit_test(evdev->ev_flags, EVDEV_FLAG_MT_TRACK)) {
		size += sizeof(mt->match_slots[0]) * slots;
		size += sizeof(mt->matrix[0]) * (slots + 6) * slots;
	}

	mt = malloc(size, M_EVDEV, M_WAITOK | M_ZERO);
	evdev->ev_mt = mt;
	mt->type_a = type_a;

	if (bit_test(evdev->ev_flags, EVDEV_FLAG_MT_TRACK)) {
		mt->match_slots = mt->slots + slots;
		mt->matrix = (int *)(mt->match_slots + slots);
	}

	/* Initialize multitouch protocol type B states */
	for (slot = 0; slot < slots; slot++)
		mt->slots[slot].id = -1;

	if (!bit_test(evdev->ev_flags, EVDEV_FLAG_MT_KEEPID))
		evdev_support_abs(evdev,
		    ABS_MT_TRACKING_ID, -1, UINT16_MAX, 0, 0, 0);
	if (bit_test(evdev->ev_flags, EVDEV_FLAG_MT_STCOMPAT))
		evdev_mt_support_st_compat(evdev);
}

void
evdev_mt_free(struct evdev_dev *evdev)
{
	free(evdev->ev_mt, M_EVDEV);
}

void
evdev_mt_sync_frame(struct evdev_dev *evdev)
{
	if (bit_test(evdev->ev_flags, EVDEV_FLAG_MT_TRACK))
		evdev_mt_replay_events(evdev);
	if (bit_test(evdev->ev_flags, EVDEV_FLAG_MT_AUTOREL))
		evdev_mt_send_autorel(evdev);
	if (evdev->ev_report_opened &&
	    bit_test(evdev->ev_flags, EVDEV_FLAG_MT_STCOMPAT))
		evdev_mt_send_st_compat(evdev);
	evdev->ev_mt->frame = 0;
}

static void
evdev_mt_send_slot(struct evdev_dev *evdev, int slot,
    union evdev_mt_slot *state)
{
	int i;
	bool type_a = !bit_test(evdev->ev_abs_flags, ABS_MT_SLOT);

	EVDEV_LOCK_ASSERT(evdev);
	MPASS(type_a || (slot >= 0 && slot <= MAXIMAL_MT_SLOT(evdev)));
	MPASS(!type_a || state != NULL);

	if (!type_a) {
		evdev_send_event(evdev, EV_ABS, ABS_MT_SLOT, slot);
		if (state == NULL) {
			evdev_send_event(evdev, EV_ABS, ABS_MT_TRACKING_ID, -1);
			return;
		}
	}
	bit_foreach_at(evdev->ev_abs_flags, ABS_MT_FIRST, ABS_MT_LAST + 1, i)
		evdev_send_event(evdev, EV_ABS, i,
		    state->val[ABS_MT_INDEX(i)]);
	if (type_a)
		evdev_send_event(evdev, EV_SYN, SYN_MT_REPORT, 1);
}

int
evdev_mt_push_slot(struct evdev_dev *evdev, int slot,
    union evdev_mt_slot *state)
{
	struct evdev_mt *mt = evdev->ev_mt;
	bool type_a = !bit_test(evdev->ev_abs_flags, ABS_MT_SLOT);

	if ((type_a || (mt != NULL && mt->type_a)) && state == NULL)
		return (EINVAL);
	if (!type_a && (slot < 0 || slot > MAXIMAL_MT_SLOT(evdev)))
		return (EINVAL);

	EVDEV_ENTER(evdev);
	if (bit_test(evdev->ev_flags, EVDEV_FLAG_MT_TRACK) && mt->type_a) {
		mt->match_slots[mt->match_slot] = *state;
		evdev_mt_record_event(evdev, EV_SYN, SYN_MT_REPORT, 1);
	} else if (bit_test(evdev->ev_flags, EVDEV_FLAG_MT_TRACK)) {
		evdev_mt_record_event(evdev, EV_ABS, ABS_MT_SLOT, slot);
		if (state != NULL)
			mt->match_slots[mt->match_slot] = *state;
		else
			evdev_mt_record_event(evdev, EV_ABS,
			    ABS_MT_TRACKING_ID, -1);
	} else
		evdev_mt_send_slot(evdev, slot, state);
	EVDEV_EXIT(evdev);

	return (0);
}

/*
 * Find a minimum-weight matching for an m-by-n matrix.
 *
 * m must be greater than or equal to n. The size of the buffer must be
 * at least 3m + 3n.
 *
 * On return, the first m elements of the buffer contain the row-to-
 * column mappings, i.e., buffer[i] is the column index for row i, or -1
 * if there is no assignment for that row (which may happen if n < m).
 *
 * Wrong results because of overflows will not occur with input values
 * in the range of 0 to INT_MAX / 2 inclusive.
 *
 * The function applies the Dinic-Kronrod algorithm. It is not modern or
 * popular, but it seems to be a good choice for small matrices at least.
 * The original form of the algorithm is modified as follows: There is no
 * initial search for row minima, the initial assignments are in a
 * "virtual" column with the index -1 and zero values. This permits inputs
 * with n < m, and it simplifies the reassignments.
 */
static void
evdev_mt_matching(int *matrix, int m, int n, int *buffer)
{
	int i, j, k, d, e, row, col, delta;
	int *p;
	int *r2c = buffer;	/* row-to-column assignments */
	int *red = r2c + m;	/* reduced values of the assignments */
	int *mc = red + m;	/* row-wise minimal elements of cs */
	int *cs = mc + m;	/* the column set */
	int *c2r = cs + n;	/* column-to-row assignments in cs */
	int *cd = c2r + n;	/* column deltas (reduction) */

	for (p = r2c; p < red; *p++ = -1) {}
	for (; p < mc; *p++ = 0) {}
	for (col = 0; col < n; col++) {
		delta = INT_MAX;
		for (i = 0, p = matrix + col; i < m; i++, p += n) {
			d = *p - red[i];
			if (d < delta || (d == delta && r2c[i] < 0)) {
				delta = d;
				row = i;
			}
		}
		cd[col] = delta;
		if (r2c[row] < 0) {
			r2c[row] = col;
			continue;
		}
		for (p = mc; p < cs; *p++ = col) {}
		for (k = 0; (j = r2c[row]) >= 0;) {
			cs[k++] = j;
			c2r[j] = row;
			mc[row] -= n;
			delta = INT_MAX;
			for (i = 0, p = matrix; i < m; i++, p += n)
				if (mc[i] >= 0) {
					d = p[mc[i]] - cd[mc[i]];
					e = p[j] - cd[j];
					if (e < d) {
						d = e;
						mc[i] = j;
					}
					d -= red[i];
					if (d < delta || (d == delta
					    && r2c[i] < 0)) {
						delta = d;
						row = i;
					}
				}
			cd[col] += delta;
			for (i = 0; i < k; i++) {
				cd[cs[i]] += delta;
				red[c2r[cs[i]]] -= delta;
			}
		}
		for (j = mc[row]; (r2c[row] = j) != col;) {
			row = c2r[j];
			j = mc[row] + n;
		}
	}
}

/*
 * Assign tracking IDs to the points in the pt array.  The tracking ID
 * assignment pairs the points with points of the previous frame in
 * such a way that the sum of the squared distances is minimal.  Using
 * squares instead of simple distances favours assignments with more uniform
 * distances, and it is faster.
 * Set tracking id to -1 for unassigned (new) points.
 */
void
evdev_mt_match_frame(struct evdev_dev *evdev, union evdev_mt_slot *pt,
    int size)
{
	struct evdev_mt *mt = evdev->ev_mt;
	int i, j, m, n, dx, dy, slot, num_touches;
	int *p, *r2c, *c2r;

	EVDEV_LOCK_ASSERT(evdev);
	MPASS(mt->matrix != NULL);
	MPASS(size >= 0 && size <= MAXIMAL_MT_SLOT(evdev) + 1);

	if (size == 0)
		return;

	p = mt->matrix;
	num_touches = bitcount(mt->touches);
	if (num_touches >= size) {
		FOREACHBIT(mt->touches, slot)
			for (i = 0; i < size; i++) {
				dx = pt[i].x - mt->slots[slot].x;
				dy = pt[i].y - mt->slots[slot].y;
				*p++ = dx * dx + dy * dy;
			}
		m = num_touches;
		n = size;
	} else {
		for (i = 0; i < size; i++)
			FOREACHBIT(mt->touches, slot) {
				dx = pt[i].x - mt->slots[slot].x;
				dy = pt[i].y - mt->slots[slot].y;
				*p++ = dx * dx + dy * dy;
			}
		m = size;
		n = num_touches;
	}
	evdev_mt_matching(mt->matrix, m, n, p);

	r2c = p;
	c2r = p + m;
	for (i = 0; i < m; i++)
		if ((j = r2c[i]) >= 0)
			c2r[j] = i;

	p = (n == size ? c2r : r2c);
	for (i = 0; i < size; i++)
		if (*p++ < 0)
			pt[i].id = -1;

	p = (n == size ? r2c : c2r);
	FOREACHBIT(mt->touches, slot)
		if ((i = *p++) >= 0)
			pt[i].id = mt->tracking_ids[slot];
}

static void
evdev_mt_send_frame(struct evdev_dev *evdev, union evdev_mt_slot *pt, int size)
{
	struct evdev_mt *mt = evdev->ev_mt;
	union evdev_mt_slot *slot;

	EVDEV_LOCK_ASSERT(evdev);
	MPASS(size >= 0 && size <= MAXIMAL_MT_SLOT(evdev) + 1);

	/*
	 * While MT-matching assign tracking IDs of new contacts to be equal
	 * to a slot number to make things simpler.
	 */
	for (slot = pt; slot < pt + size; slot++) {
		if (slot->id < 0)
			slot->id = ffc_slot(evdev, mt->touches | mt->frame);
		if (slot->id >= 0)
			evdev_mt_send_slot(evdev, slot->id, slot);
	}
}

int
evdev_mt_push_frame(struct evdev_dev *evdev, union evdev_mt_slot *pt, int size)
{
	if (size < 0 || size > MAXIMAL_MT_SLOT(evdev) + 1)
		return (EINVAL);

	EVDEV_ENTER(evdev);
	evdev_mt_send_frame(evdev, pt, size);
	EVDEV_EXIT(evdev);

	return (0);
}

bool
evdev_mt_record_event(struct evdev_dev *evdev, uint16_t type, uint16_t code,
    int32_t value)
{
	struct evdev_mt *mt = evdev->ev_mt;

	EVDEV_LOCK_ASSERT(evdev);

	switch (type) {
	case EV_SYN:
		if (code == SYN_MT_REPORT) {
			/* MT protocol type A support */
			KASSERT(mt->type_a, ("Not a MT type A protocol"));
			mt->match_frame |= 1U << mt->match_slot;
			mt->match_slot++;
			return (true);
		}
		break;
	case EV_ABS:
		if (code == ABS_MT_SLOT) {
			/* MT protocol type B support */
			KASSERT(!mt->type_a, ("Not a MT type B protocol"));
			KASSERT(value >= 0, ("Negative slot number"));
			mt->match_slot = value;
			mt->match_frame |= 1U << mt->match_slot;
			return (true);
		} else if (code == ABS_MT_TRACKING_ID) {
			KASSERT(!mt->type_a, ("Not a MT type B protocol"));
			if (value == -1)
				mt->match_frame &= ~(1U << mt->match_slot);
			return (true);
		} else if (ABS_IS_MT(code)) {
			KASSERT(mt->match_slot >= 0, ("Negative slot"));
			KASSERT(mt->match_slot <= MAXIMAL_MT_SLOT(evdev),
			    ("Slot number too big"));
			mt->match_slots[mt->match_slot].
			    val[ABS_MT_INDEX(code)] = value;
			return (true);
		}
		break;
	default:
		break;
	}

	return (false);
}

static void
evdev_mt_replay_events(struct evdev_dev *evdev)
{
	struct evdev_mt *mt = evdev->ev_mt;
	int slot, size = 0;

	EVDEV_LOCK_ASSERT(evdev);

	FOREACHBIT(mt->match_frame, slot) {
		if (slot != size)
			mt->match_slots[size] = mt->match_slots[slot];
		size++;
	}
	evdev_mt_match_frame(evdev, mt->match_slots, size);
	evdev_mt_send_frame(evdev, mt->match_slots, size);
	mt->match_slot = 0;
	mt->match_frame = 0;
}

union evdev_mt_slot *
evdev_mt_get_match_slots(struct evdev_dev *evdev)
{
	return (evdev->ev_mt->match_slots);
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
evdev_mt_id_to_slot(struct evdev_dev *evdev, int32_t tracking_id)
{
	struct evdev_mt *mt = evdev->ev_mt;
	int slot;

	KASSERT(!mt->type_a, ("Not a MT type B protocol"));

	/*
	 * Ignore tracking_id if slot assignment is performed by evdev.
	 * Events are written sequentially to temporary matching buffer.
	 */
	if (bit_test(evdev->ev_flags, EVDEV_FLAG_MT_TRACK))
		return (ffc_slot(evdev, mt->match_frame));

	FOREACHBIT(mt->touches, slot)
		if (mt->tracking_ids[slot] == tracking_id)
			return (slot);
	/*
	 * Do not allow allocation of new slot in a place of just
	 * released one within the same report.
	 */
	return (ffc_slot(evdev, mt->touches | mt->frame));
}

int32_t
evdev_mt_reassign_id(struct evdev_dev *evdev, int slot, int32_t id)
{
	struct evdev_mt *mt = evdev->ev_mt;
	int32_t nid;

	if (id == -1 || bit_test(evdev->ev_flags, EVDEV_FLAG_MT_KEEPID)) {
		mt->tracking_ids[slot] = id;
		return (id);
	}

	nid = evdev_mt_get_value(evdev, slot, ABS_MT_TRACKING_ID);
	if (nid != -1) {
		KASSERT(id == mt->tracking_ids[slot],
		    ("MT-slot tracking id has changed"));
		return (nid);
	}

	mt->tracking_ids[slot] = id;
again:
	nid = mt->tracking_id++;
	FOREACHBIT(mt->touches, slot)
		if (evdev_mt_get_value(evdev, slot, ABS_MT_TRACKING_ID) == nid)
			goto again;

	return (nid);
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

static void
evdev_mt_support_st_compat(struct evdev_dev *evdev)
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

static void
evdev_mt_send_autorel(struct evdev_dev *evdev)
{
	struct evdev_mt *mt = evdev->ev_mt;
	int slot;

	EVDEV_LOCK_ASSERT(evdev);
	KASSERT(mt->match_frame == 0, ("Unmatched events exist"));

	FOREACHBIT(mt->touches & ~mt->frame, slot)
		evdev_mt_send_slot(evdev, slot, NULL);
}

void
evdev_mt_push_autorel(struct evdev_dev *evdev)
{
	EVDEV_ENTER(evdev);
	evdev_mt_send_autorel(evdev);
	EVDEV_EXIT(evdev);
}
