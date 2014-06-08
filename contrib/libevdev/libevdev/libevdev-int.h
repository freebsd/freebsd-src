/*
 * Copyright © 2013 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#ifndef LIBEVDEV_INT_H
#define LIBEVDEV_INT_H

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include "libevdev.h"
#include "libevdev-util.h"

#define MAX_NAME 256
#define ABS_MT_MIN ABS_MT_SLOT
#define ABS_MT_MAX ABS_MT_TOOL_Y
#define ABS_MT_CNT (ABS_MT_MAX - ABS_MT_MIN + 1)
#define LIBEVDEV_EXPORT __attribute__((visibility("default")))
#define ALIAS(_to) __attribute__((alias(#_to)))

/**
 * Sync state machine:
 * default state: SYNC_NONE
 *
 * SYNC_NONE → SYN_DROPPED or forced sync → SYNC_NEEDED
 * SYNC_NEEDED → libevdev_next_event(LIBEVDEV_READ_FLAG_SYNC) → SYNC_IN_PROGRESS
 * SYNC_NEEDED → libevdev_next_event(LIBEVDEV_READ_FLAG_SYNC_NONE) → SYNC_NONE
 * SYNC_IN_PROGRESS → libevdev_next_event(LIBEVDEV_READ_FLAG_SYNC_NONE) → SYNC_NONE
 * SYNC_IN_PROGRESS → no sync events left → SYNC_NONE
 *
 */
enum SyncState {
	SYNC_NONE,
	SYNC_NEEDED,
	SYNC_IN_PROGRESS,
};

struct mt_sync_state {
	int code;
	int val[];
};

/**
 * Internal only: log data used to send messages to the respective log
 * handler. We re-use the same struct for a global and inside
 * struct libevdev.
 * For the global, device_handler is NULL, for per-device instance
 * global_handler is NULL.
 */
struct logdata {
	enum libevdev_log_priority priority;		/** minimum logging priority */
	libevdev_log_func_t global_handler;		/** global handler function */
	libevdev_device_log_func_t device_handler;	/** per-device handler function */
	void *userdata;					/** user-defined data pointer */
};

struct libevdev {
	int fd;
	bool initialized;
	char *name;
	char *phys;
	char *uniq;
	struct input_id ids;
	int driver_version;
	unsigned long bits[NLONGS(EV_CNT)];
	unsigned long props[NLONGS(INPUT_PROP_CNT)];
	unsigned long key_bits[NLONGS(KEY_CNT)];
	unsigned long rel_bits[NLONGS(REL_CNT)];
	unsigned long abs_bits[NLONGS(ABS_CNT)];
	unsigned long led_bits[NLONGS(LED_CNT)];
	unsigned long msc_bits[NLONGS(MSC_CNT)];
	unsigned long sw_bits[NLONGS(SW_CNT)];
	unsigned long rep_bits[NLONGS(REP_CNT)]; /* convenience, always 1 */
	unsigned long ff_bits[NLONGS(FF_CNT)];
	unsigned long snd_bits[NLONGS(SND_CNT)];
	unsigned long key_values[NLONGS(KEY_CNT)];
	unsigned long led_values[NLONGS(LED_CNT)];
	unsigned long sw_values[NLONGS(SW_CNT)];
	struct input_absinfo abs_info[ABS_CNT];
	int *mt_slot_vals; /* [num_slots * ABS_MT_CNT] */
	int num_slots; /**< valid slots in mt_slot_vals */
	int current_slot;
	int rep_values[REP_CNT];

	enum SyncState sync_state;
	enum libevdev_grab_mode grabbed;

	struct input_event *queue;
	size_t queue_size; /**< size of queue in elements */
	size_t queue_next; /**< next event index */
	size_t queue_nsync; /**< number of sync events */

	struct timeval last_event_time;

	struct {
		struct mt_sync_state *mt_state;
		size_t mt_state_sz;		 /* in bytes */
		unsigned long *slot_update;
		size_t slot_update_sz;		 /* in bytes */
		unsigned long *tracking_id_changes;
		size_t tracking_id_changes_sz;	 /* in bytes */
	} mt_sync;

	struct logdata log;
};

#define log_msg_cond(dev, priority, ...) \
	do { \
		if (log_priority(dev) >= priority) \
			log_msg(dev, priority, __FILE__, __LINE__, __func__, __VA_ARGS__); \
	} while(0)

#define log_error(dev, ...) log_msg_cond(dev, LIBEVDEV_LOG_ERROR, __VA_ARGS__)
#define log_info(dev, ...) log_msg_cond(dev, LIBEVDEV_LOG_INFO, __VA_ARGS__)
#define log_dbg(dev, ...) log_msg_cond(dev, LIBEVDEV_LOG_DEBUG, __VA_ARGS__)
#define log_bug(dev, ...) log_msg_cond(dev, LIBEVDEV_LOG_ERROR, "BUG: "__VA_ARGS__)

extern void
log_msg(const struct libevdev *dev,
	enum libevdev_log_priority priority,
	const char *file, int line, const char *func,
	const char *format, ...) LIBEVDEV_ATTRIBUTE_PRINTF(6, 7);
extern enum libevdev_log_priority
log_priority(const struct libevdev *dev);

/**
 * @return a pointer to the next element in the queue, or NULL if the queue
 * is full.
 */
static inline struct input_event*
queue_push(struct libevdev *dev)
{
	if (dev->queue_next >= dev->queue_size)
		return NULL;

	return &dev->queue[dev->queue_next++];
}

/**
 * Set ev to the last element in the queue, removing it from the queue.
 *
 * @return 0 on success, 1 if the queue is empty.
 */
static inline int
queue_pop(struct libevdev *dev, struct input_event *ev)
{
	if (dev->queue_next == 0)
		return 1;

	*ev = dev->queue[--dev->queue_next];

	return 0;
}

static inline int
queue_peek(struct libevdev *dev, size_t idx, struct input_event *ev)
{
	if (dev->queue_next == 0 || idx > dev->queue_next)
		return 1;
	*ev = dev->queue[idx];
	return 0;
}


/**
 * Shift the first n elements into ev and return the number of elements
 * shifted.
 * ev must be large enough to store n elements.
 *
 * @param ev The buffer to copy into, or NULL
 * @return The number of elements in ev.
 */
static inline int
queue_shift_multiple(struct libevdev *dev, size_t n, struct input_event *ev)
{
	size_t i;

	if (dev->queue_next == 0)
		return 0;

	n = min(n, dev->queue_next);

	if (ev) {
		for (i = 0; i < n; i++)
			ev[i] = dev->queue[i];
	}

	for (i = 0; i < dev->queue_next - n; i++)
		dev->queue[i] = dev->queue[n + i];

	dev->queue_next -= n;
	return n;
}

/**
 * Set ev to the first element in the queue, shifting everything else
 * forward by one.
 *
 * @return 0 on success, 1 if the queue is empty.
 */
static inline int
queue_shift(struct libevdev *dev, struct input_event *ev)
{
	return queue_shift_multiple(dev, 1, ev) == 1 ? 0 : 1;
}

static inline int
queue_alloc(struct libevdev *dev, size_t size)
{
	if (size == 0)
		return -ENOMEM;

	dev->queue = calloc(size, sizeof(struct input_event));
	if (!dev->queue)
		return -ENOMEM;

	dev->queue_size = size;
	dev->queue_next = 0;
	return 0;
}

static inline void
queue_free(struct libevdev *dev)
{
	free(dev->queue);
	dev->queue_size = 0;
	dev->queue_next = 0;
}

static inline size_t
queue_num_elements(struct libevdev *dev)
{
	return dev->queue_next;
}

static inline size_t
queue_size(struct libevdev *dev)
{
	return dev->queue_size;
}

static inline size_t
queue_num_free_elements(struct libevdev *dev)
{
	if (dev->queue_size == 0)
		return 0;

	return dev->queue_size - dev->queue_next;
}

static inline struct input_event *
queue_next_element(struct libevdev *dev)
{
	if (dev->queue_next == dev->queue_size)
		return NULL;

	return &dev->queue[dev->queue_next];
}

static inline int
queue_set_num_elements(struct libevdev *dev, size_t nelem)
{
	if (nelem > dev->queue_size)
		return 1;

	dev->queue_next = nelem;

	return 0;
}

#define max_mask(uc, lc) \
	case EV_##uc: \
			*mask = dev->lc##_bits; \
			max = libevdev_event_type_get_max(type); \
		break;


static inline int
type_to_mask_const(const struct libevdev *dev, unsigned int type, const unsigned long **mask)
{
	int max;

	switch(type) {
		max_mask(ABS, abs);
		max_mask(REL, rel);
		max_mask(KEY, key);
		max_mask(LED, led);
		max_mask(MSC, msc);
		max_mask(SW, sw);
		max_mask(FF, ff);
		max_mask(REP, rep);
		max_mask(SND, snd);
		default:
		     max = -1;
		     break;
	}

	return max;
}

static inline int
type_to_mask(struct libevdev *dev, unsigned int type, unsigned long **mask)
{
	int max;

	switch(type) {
		max_mask(ABS, abs);
		max_mask(REL, rel);
		max_mask(KEY, key);
		max_mask(LED, led);
		max_mask(MSC, msc);
		max_mask(SW, sw);
		max_mask(FF, ff);
		max_mask(REP, rep);
		max_mask(SND, snd);
		default:
		     max = -1;
		     break;
	}

	return max;
}

#undef max_mask
#endif

