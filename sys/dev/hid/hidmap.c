/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2020 Vladimir Kondratyev <wulf@FreeBSD.org>
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
__FBSDID("$FreeBSD$");

/*
 * Abstract 1 to 1 HID input usage to evdev event mapper driver.
 */

#include "opt_hid.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <dev/evdev/input.h>
#include <dev/evdev/evdev.h>

#include <dev/hid/hid.h>
#include <dev/hid/hidbus.h>
#include <dev/hid/hidmap.h>

#ifdef HID_DEBUG
#define DPRINTFN(hm, n, fmt, ...) do {					\
	if ((hm)->debug_var != NULL && *(hm)->debug_var >= (n)) {	\
		device_printf((hm)->dev, "%s: " fmt,			\
		    __FUNCTION__ ,##__VA_ARGS__);			\
	}								\
} while (0)
#define DPRINTF(hm, ...)	DPRINTFN(hm, 1, __VA_ARGS__)
#else
#define DPRINTF(...) do { } while (0)
#define DPRINTFN(...) do { } while (0)
#endif

static evdev_open_t hidmap_ev_open;
static evdev_close_t hidmap_ev_close;

#define	HIDMAP_WANT_MERGE_KEYS(hm)	((hm)->key_rel != NULL)

#define HIDMAP_FOREACH_ITEM(hm, mi, uoff)				\
	for (u_int _map = 0, _item = 0, _uoff_priv = -1;		\
	    ((mi) = hidmap_get_next_map_item(				\
		(hm), &_map, &_item, &_uoff_priv, &(uoff))) != NULL;)

static inline bool
hidmap_get_next_map_index(const struct hidmap_item *map, int nmap_items,
    uint32_t *index, uint16_t *usage_offset)
{

	++*usage_offset;
	if ((*index != 0 || *usage_offset != 0) &&
	    *usage_offset >= map[*index].nusages) {
		++*index;
		*usage_offset = 0;
	}

	return (*index < nmap_items);
}

static inline const struct hidmap_item *
hidmap_get_next_map_item(struct hidmap *hm, u_int *map, u_int *item,
    u_int *uoff_priv, uint16_t *uoff)
{

	*uoff = *uoff_priv;
	while (!hidmap_get_next_map_index(
	   hm->map[*map], hm->nmap_items[*map], item, uoff)) {
		++*map;
		*item = 0;
		*uoff = -1;
		if (*map >= hm->nmaps)
			return (NULL);
	}
	*uoff_priv = *uoff;

	return (hm->map[*map] + *item);
}

void
_hidmap_set_debug_var(struct hidmap *hm, int *debug_var)
{
#ifdef HID_DEBUG
	hm->debug_var = debug_var;
#endif
}

static int
hidmap_ev_close(struct evdev_dev *evdev)
{
	return (hidbus_intr_stop(evdev_get_softc(evdev)));
}

static int
hidmap_ev_open(struct evdev_dev *evdev)
{
	return (hidbus_intr_start(evdev_get_softc(evdev)));
}

void
hidmap_support_key(struct hidmap *hm, uint16_t key)
{
	if (hm->key_press == NULL) {
		hm->key_press = malloc(howmany(KEY_CNT, 8), M_DEVBUF,
		    M_ZERO | M_WAITOK);
		evdev_support_event(hm->evdev, EV_KEY);
		hm->key_min = key;
		hm->key_max = key;
	}
	hm->key_min = MIN(hm->key_min, key);
	hm->key_max = MAX(hm->key_max, key);
	if (isset(hm->key_press, key)) {
		if (hm->key_rel == NULL)
			hm->key_rel = malloc(howmany(KEY_CNT, 8), M_DEVBUF,
			    M_ZERO | M_WAITOK);
	} else {
		setbit(hm->key_press, key);
		evdev_support_key(hm->evdev, key);
	}
}

void
hidmap_push_key(struct hidmap *hm, uint16_t key, int32_t value)
{
	if (HIDMAP_WANT_MERGE_KEYS(hm))
		setbit(value != 0 ? hm->key_press : hm->key_rel, key);
	else
		evdev_push_key(hm->evdev, key, value);
}

static void
hidmap_sync_keys(struct hidmap *hm)
{
	int i, j;
	bool press, rel;

	for (j = hm->key_min / 8; j <= hm->key_max / 8; j++) {
		if (hm->key_press[j] != hm->key_rel[j]) {
			for (i = j * 8; i < j * 8 + 8; i++) {
				press = isset(hm->key_press, i);
				rel = isset(hm->key_rel, i);
				if (press != rel)
					evdev_push_key(hm->evdev, i, press);
			}
		}
	}
	bzero(hm->key_press, howmany(KEY_CNT, 8));
	bzero(hm->key_rel, howmany(KEY_CNT, 8));
}

void
hidmap_intr(void *context, void *buf, hid_size_t len)
{
	struct hidmap *hm = context;
	struct hidmap_hid_item *hi;
	const struct hidmap_item *mi;
	int32_t usage;
	int32_t data;
	uint16_t key, uoff;
	uint8_t id = 0;
	bool found, do_sync = false;

	DPRINTFN(hm, 6, "hm=%p len=%d\n", hm, len);
	DPRINTFN(hm, 6, "data = %*D\n", len, buf, " ");

	/* Strip leading "report ID" byte */
	if (hm->hid_items[0].id) {
		id = *(uint8_t *)buf;
		len--;
		buf = (uint8_t *)buf + 1;
	}

	hm->intr_buf = buf;
	hm->intr_len = len;

	for (hi = hm->hid_items; hi < hm->hid_items + hm->nhid_items; hi++) {
		/* At first run callbacks that not tied to HID items */
		if (hi->type == HIDMAP_TYPE_FINALCB) {
			DPRINTFN(hm, 6, "type=%d item=%*D\n", hi->type,
			    (int)sizeof(hi->cb), &hi->cb, " ");
			if (hi->cb(hm, hi, (union hidmap_cb_ctx){.rid = id})
			    == 0)
				do_sync = true;
			continue;
		}

		/* Ignore irrelevant reports */
		if (id != hi->id)
			continue;

		/*
		 * 5.8. If Logical Minimum and Logical Maximum are both
		 * positive values then the contents of a field can be assumed
		 * to be an unsigned value. Otherwise, all integer values are
		 * signed values represented in 2’s complement format.
		 */
		data = hi->lmin < 0 || hi->lmax < 0
		    ? hid_get_data(buf, len, &hi->loc)
		    : hid_get_udata(buf, len, &hi->loc);

		DPRINTFN(hm, 6, "type=%d data=%d item=%*D\n", hi->type, data,
		    (int)sizeof(hi->cb), &hi->cb, " ");

		if (hi->invert_value && hi->type < HIDMAP_TYPE_ARR_LIST)
			data = hi->evtype == EV_REL
			    ? -data
			    : hi->lmin + hi->lmax - data;

		switch (hi->type) {
		case HIDMAP_TYPE_CALLBACK:
			if (hi->cb(hm, hi, (union hidmap_cb_ctx){.data = data})
			    != 0)
				continue;
			break;

		case HIDMAP_TYPE_VAR_NULLST:
			/*
			 * 5.10. If the host or the device receives an
			 * out-of-range value then the current value for the
			 * respective control will not be modified.
			 */
			if (data < hi->lmin || data > hi->lmax)
				continue;
			/* FALLTHROUGH */
		case HIDMAP_TYPE_VARIABLE:
			/*
			 * Ignore reports for absolute data if the data did not
			 * change and for relative data if data is 0.
			 * Evdev layer filters out them anyway.
			 */
			if (data == (hi->evtype == EV_REL ? 0 : hi->last_val))
				continue;
			if (hi->evtype == EV_KEY)
				hidmap_push_key(hm, hi->code, data);
			else
				evdev_push_event(hm->evdev, hi->evtype,
				    hi->code, data);
			hi->last_val = data;
			break;

		case HIDMAP_TYPE_ARR_LIST:
			key = KEY_RESERVED;
			/*
			 * 6.2.2.5. An out-of range value in an array field
			 * is considered no controls asserted.
			 */
			if (data < hi->lmin || data > hi->lmax)
				goto report_key;
			/*
			 * 6.2.2.5. Rather than returning a single bit for each
			 * button in the group, an array returns an index in
			 * each field that corresponds to the pressed button.
			 */
			key = hi->codes[data - hi->lmin];
			if (key == KEY_RESERVED)
				DPRINTF(hm, "Can not map unknown HID "
				    "array index: %08x\n", data);
			goto report_key;

		case HIDMAP_TYPE_ARR_RANGE:
			key = KEY_RESERVED;
			/*
			 * 6.2.2.5. An out-of range value in an array field
			 * is considered no controls asserted.
			 */
			if (data < hi->lmin || data > hi->lmax)
				goto report_key;
			/*
			 * When the input field is an array and the usage is
			 * specified with a range instead of an ID, we have to
			 * derive the actual usage by using the item value as
			 * an index in the usage range list.
			 */
			usage = data - hi->lmin + hi->umin;
			found = false;
			HIDMAP_FOREACH_ITEM(hm, mi, uoff) {
				if (usage == mi->usage + uoff &&
				    mi->type == EV_KEY && !mi->has_cb) {
					key = mi->code;
					found = true;
					break;
				}
			}
			if (!found)
				DPRINTF(hm, "Can not map unknown HID "
				    "usage: %08x\n", usage);
report_key:
			if (key == HIDMAP_KEY_NULL || key == hi->last_key)
				continue;
			if (hi->last_key != KEY_RESERVED)
				hidmap_push_key(hm, hi->last_key, 0);
			if (key != KEY_RESERVED)
				hidmap_push_key(hm, key, 1);
			hi->last_key = key;
			break;

		default:
			KASSERT(0, ("Unknown map type (%d)", hi->type));
		}
		do_sync = true;
	}

	if (do_sync) {
		if (HIDMAP_WANT_MERGE_KEYS(hm))
			hidmap_sync_keys(hm);
		evdev_sync(hm->evdev);
	}
}

static inline bool
can_map_callback(struct hid_item *hi, const struct hidmap_item *mi,
    uint16_t usage_offset)
{

	return (mi->has_cb && !mi->final_cb &&
	    hi->usage == mi->usage + usage_offset &&
	    (mi->relabs == HIDMAP_RELABS_ANY ||
	    !(hi->flags & HIO_RELATIVE) == !(mi->relabs == HIDMAP_RELATIVE)));
}

static inline bool
can_map_variable(struct hid_item *hi, const struct hidmap_item *mi,
    uint16_t usage_offset)
{

	return ((hi->flags & HIO_VARIABLE) != 0 && !mi->has_cb &&
	    hi->usage == mi->usage + usage_offset &&
	    (mi->relabs == HIDMAP_RELABS_ANY ||
	    !(hi->flags & HIO_RELATIVE) == !(mi->relabs == HIDMAP_RELATIVE)));
}

static inline bool
can_map_arr_range(struct hid_item *hi, const struct hidmap_item *mi,
    uint16_t usage_offset)
{

	return ((hi->flags & HIO_VARIABLE) == 0 && !mi->has_cb &&
	    hi->usage_minimum <= mi->usage + usage_offset &&
	    hi->usage_maximum >= mi->usage + usage_offset &&
	    mi->type == EV_KEY &&
	    (mi->code != KEY_RESERVED && mi->code != HIDMAP_KEY_NULL));
}

static inline bool
can_map_arr_list(struct hid_item *hi, const struct hidmap_item *mi,
    uint32_t usage, uint16_t usage_offset)
{

	return ((hi->flags & HIO_VARIABLE) == 0 && !mi->has_cb &&
	    usage == mi->usage + usage_offset &&
	    mi->type == EV_KEY &&
	    (mi->code != KEY_RESERVED && mi->code != HIDMAP_KEY_NULL));
}

static bool
hidmap_probe_hid_item(struct hid_item *hi, const struct hidmap_item *map,
    int nitems_map, hidmap_caps_t caps)
{
	u_int i, j;
	uint16_t uoff;
	bool found = false;

#define	HIDMAP_FOREACH_INDEX(map, nitems, idx, uoff)	\
	for ((idx) = 0, (uoff) = -1;			\
	     hidmap_get_next_map_index((map), (nitems), &(idx), &(uoff));)

	HIDMAP_FOREACH_INDEX(map, nitems_map, i, uoff) {
		if (can_map_callback(hi, map + i, uoff)) {
			if (map[i].cb(NULL, NULL,
			    (union hidmap_cb_ctx){.hi = hi}) != 0)
				break;
			setbit(caps, i);
			return (true);
		}
	}

	if (hi->flags & HIO_VARIABLE) {
		HIDMAP_FOREACH_INDEX(map, nitems_map, i, uoff) {
			if (can_map_variable(hi, map + i, uoff)) {
				KASSERT(map[i].type == EV_KEY ||
					map[i].type == EV_REL ||
					map[i].type == EV_ABS ||
					map[i].type == EV_SW,
				    ("Unsupported event type"));
				setbit(caps, i);
				return (true);
			}
		}
		return (false);
	}

	if (hi->usage_minimum != 0 || hi->usage_maximum != 0) {
		HIDMAP_FOREACH_INDEX(map, nitems_map, i, uoff) {
			if (can_map_arr_range(hi, map + i, uoff)) {
				setbit(caps, i);
				found = true;
			}
		}
		return (found);
	}

	for (j = 0; j < hi->nusages; j++) {
		HIDMAP_FOREACH_INDEX(map, nitems_map, i, uoff) {
			if (can_map_arr_list(hi, map+i, hi->usages[j], uoff)) {
				setbit(caps, i);
				found = true;
			}
		}
	}

	return (found);
}

static uint32_t
hidmap_probe_hid_descr(void *d_ptr, hid_size_t d_len, uint8_t tlc_index,
    const struct hidmap_item *map, int nitems_map, hidmap_caps_t caps)
{
	struct hid_data *hd;
	struct hid_item hi;
	uint32_t i, items = 0;
	bool do_free = false;

	if (caps == NULL) {
		caps = malloc(HIDMAP_CAPS_SZ(nitems_map), M_DEVBUF,
		    M_WAITOK | M_ZERO);
		do_free = true;
	} else
		bzero (caps, HIDMAP_CAPS_SZ(nitems_map));

	/* Parse inputs */
	hd = hid_start_parse(d_ptr, d_len, 1 << hid_input);
	HIDBUS_FOREACH_ITEM(hd, &hi, tlc_index) {
		if (hi.kind != hid_input)
			continue;
		if (hi.flags & HIO_CONST)
			continue;
		for (i = 0; i < hi.loc.count; i++, hi.loc.pos += hi.loc.size)
			if (hidmap_probe_hid_item(&hi, map, nitems_map, caps))
				items++;
	}
	hid_end_parse(hd);

	/* Take finalizing callbacks in to account */
	for (i = 0; i < nitems_map; i++) {
		if (map[i].has_cb && map[i].final_cb &&
		    map[i].cb(NULL, NULL, (union hidmap_cb_ctx){}) == 0) {
			setbit(caps, i);
			items++;
		}
	}

	/* Check that all mandatory usages are present in report descriptor */
	if (items != 0) {
		for (i = 0; i < nitems_map; i++) {
			KASSERT(!(map[i].required && map[i].forbidden),
			    ("both required & forbidden item flags are set"));
			if ((map[i].required && isclr(caps, i)) ||
			    (map[i].forbidden && isset(caps, i))) {
				items = 0;
				break;
			}
		}
	}

	if (do_free)
		free(caps, M_DEVBUF);

	return (items);
}

uint32_t
hidmap_add_map(struct hidmap *hm, const struct hidmap_item *map,
    int nitems_map, hidmap_caps_t caps)
{
	void *d_ptr;
	uint32_t items;
	int i, error;
	hid_size_t d_len;
	uint8_t tlc_index = hidbus_get_index(hm->dev);

	/* Avoid double-adding of map in probe() handler */
	for (i = 0; i < hm->nmaps; i++)
		if (hm->map[i] == map)
			return (0);

	error = hid_get_report_descr(hm->dev, &d_ptr, &d_len);
	if (error != 0) {
		device_printf(hm->dev, "could not retrieve report descriptor "
		     "from device: %d\n", error);
		return (error);
	}

	hm->cb_state = HIDMAP_CB_IS_PROBING;
	items = hidmap_probe_hid_descr(d_ptr, d_len, tlc_index, map,
	    nitems_map, caps);
	if (items == 0)
		return (ENXIO);

	KASSERT(hm->nmaps < HIDMAP_MAX_MAPS,
	    ("Not more than %d maps is supported", HIDMAP_MAX_MAPS));
	hm->nhid_items += items;
	hm->map[hm->nmaps] = map;
	hm->nmap_items[hm->nmaps] = nitems_map;
	hm->nmaps++;

	return (0);
}

static bool
hidmap_parse_hid_item(struct hidmap *hm, struct hid_item *hi,
    struct hidmap_hid_item *item)
{
	const struct hidmap_item *mi;
	struct hidmap_hid_item hi_temp;
	uint32_t i;
	uint16_t uoff;
	bool found = false;

	HIDMAP_FOREACH_ITEM(hm, mi, uoff) {
		if (can_map_callback(hi, mi, uoff)) {
			bzero(&hi_temp, sizeof(hi_temp));
			hi_temp.cb = mi->cb;
			hi_temp.type = HIDMAP_TYPE_CALLBACK;
			/*
			 * Values returned by probe- and attach-stage
			 * callbacks MUST be identical.
			 */
			if (mi->cb(hm, &hi_temp,
			    (union hidmap_cb_ctx){.hi = hi}) != 0)
				break;
			bcopy(&hi_temp, item, sizeof(hi_temp));
			goto mapped;
		}
	}

	if (hi->flags & HIO_VARIABLE) {
		HIDMAP_FOREACH_ITEM(hm, mi, uoff) {
			if (can_map_variable(hi, mi, uoff)) {
				item->evtype = mi->type;
				item->code = mi->code + uoff;
				item->type = hi->flags & HIO_NULLSTATE
				    ? HIDMAP_TYPE_VAR_NULLST
				    : HIDMAP_TYPE_VARIABLE;
				item->last_val = 0;
				item->invert_value = mi->invert_value;
				switch (mi->type) {
				case EV_KEY:
					hidmap_support_key(hm, item->code);
					break;
				case EV_REL:
					evdev_support_event(hm->evdev, EV_REL);
					evdev_support_rel(hm->evdev,
					    item->code);
					break;
				case EV_ABS:
					evdev_support_event(hm->evdev, EV_ABS);
					evdev_support_abs(hm->evdev,
					    item->code,
					    hi->logical_minimum,
					    hi->logical_maximum,
					    mi->fuzz,
					    mi->flat,
					    hid_item_resolution(hi));
					break;
				case EV_SW:
					evdev_support_event(hm->evdev, EV_SW);
					evdev_support_sw(hm->evdev,
					    item->code);
					break;
				default:
					KASSERT(0, ("Unsupported event type"));
				}
				goto mapped;
			}
		}
		return (false);
	}

	if (hi->usage_minimum != 0 || hi->usage_maximum != 0) {
		HIDMAP_FOREACH_ITEM(hm, mi, uoff) {
			if (can_map_arr_range(hi, mi, uoff)) {
				hidmap_support_key(hm, mi->code + uoff);
				found = true;
			}
		}
		if (!found)
			return (false);
		item->umin = hi->usage_minimum;
		item->type = HIDMAP_TYPE_ARR_RANGE;
		item->last_key = KEY_RESERVED;
		goto mapped;
	}

	for (i = 0; i < hi->nusages; i++) {
		HIDMAP_FOREACH_ITEM(hm, mi, uoff) {
			if (can_map_arr_list(hi, mi, hi->usages[i], uoff)) {
				hidmap_support_key(hm, mi->code + uoff);
				if (item->codes == NULL)
					item->codes = malloc(
					    hi->nusages * sizeof(uint16_t),
					    M_DEVBUF, M_WAITOK | M_ZERO);
				item->codes[i] = mi->code + uoff;
				found = true;
				break;
			}
		}
	}
	if (!found)
		return (false);
	item->type = HIDMAP_TYPE_ARR_LIST;
	item->last_key = KEY_RESERVED;

mapped:
	item->id = hi->report_ID;
	item->loc = hi->loc;
	item->loc.count = 1;
	item->lmin = hi->logical_minimum;
	item->lmax = hi->logical_maximum;

	DPRINTFN(hm, 6, "usage=%04x id=%d loc=%u/%u type=%d item=%*D\n",
	    hi->usage, hi->report_ID, hi->loc.pos, hi->loc.size, item->type,
	    (int)sizeof(item->cb), &item->cb, " ");

	return (true);
}

static int
hidmap_parse_hid_descr(struct hidmap *hm, uint8_t tlc_index)
{
	const struct hidmap_item *map;
	struct hidmap_hid_item *item = hm->hid_items;
	void *d_ptr;
	struct hid_data *hd;
	struct hid_item hi;
	int i, error;
	hid_size_t d_len;

	error = hid_get_report_descr(hm->dev, &d_ptr, &d_len);
	if (error != 0) {
		DPRINTF(hm, "could not retrieve report descriptor from "
		     "device: %d\n", error);
		return (error);
	}

	/* Parse inputs */
	hd = hid_start_parse(d_ptr, d_len, 1 << hid_input);
	HIDBUS_FOREACH_ITEM(hd, &hi, tlc_index) {
		if (hi.kind != hid_input)
			continue;
		if (hi.flags & HIO_CONST)
			continue;
		for (i = 0; i < hi.loc.count; i++, hi.loc.pos += hi.loc.size)
			if (hidmap_parse_hid_item(hm, &hi, item))
				item++;
		KASSERT(item <= hm->hid_items + hm->nhid_items,
		    ("Parsed HID item array overflow"));
	}
	hid_end_parse(hd);

	/* Add finalizing callbacks to the end of list */
	for (i = 0; i < hm->nmaps; i++) {
		for (map = hm->map[i];
		     map < hm->map[i] + hm->nmap_items[i];
		     map++) {
			if (map->has_cb && map->final_cb &&
			    map->cb(hm, item, (union hidmap_cb_ctx){}) == 0) {
				item->cb = map->cb;
				item->type = HIDMAP_TYPE_FINALCB;
				item++;
			}
		}
	}

	/*
	 * Resulting number of parsed HID items can be less than expected as
	 * map items might be duplicated in different maps. Save real number.
	 */
	if (hm->nhid_items != item - hm->hid_items)
		DPRINTF(hm, "Parsed HID item number mismatch: expected=%u "
		    "result=%td\n", hm->nhid_items, item - hm->hid_items);
	hm->nhid_items = item - hm->hid_items;

	if (HIDMAP_WANT_MERGE_KEYS(hm))
		bzero(hm->key_press, howmany(KEY_CNT, 8));

	return (0);
}

int
hidmap_probe(struct hidmap* hm, device_t dev,
    const struct hid_device_id *id, int nitems_id,
    const struct hidmap_item *map, int nitems_map,
    const char *suffix, hidmap_caps_t caps)
{
	int error;

	error = hidbus_lookup_driver_info(dev, id, nitems_id);
	if (error != 0)
		return (error);

	hidmap_set_dev(hm, dev);

	error = hidmap_add_map(hm, map, nitems_map, caps);
	if (error != 0)
		return (error);

	hidbus_set_desc(dev, suffix);

	return (BUS_PROBE_DEFAULT);
}

int
hidmap_attach(struct hidmap* hm)
{
	const struct hid_device_info *hw = hid_get_device_info(hm->dev);
#ifdef HID_DEBUG
	char tunable[40];
#endif
	int error;

#ifdef HID_DEBUG
	if (hm->debug_var == NULL) {
		hm->debug_var = &hm->debug_level;
		snprintf(tunable, sizeof(tunable), "hw.hid.%s.debug",
		    device_get_name(hm->dev));
		TUNABLE_INT_FETCH(tunable, &hm->debug_level);
		SYSCTL_ADD_INT(device_get_sysctl_ctx(hm->dev),
			SYSCTL_CHILDREN(device_get_sysctl_tree(hm->dev)),
			OID_AUTO, "debug", CTLFLAG_RWTUN,
			&hm->debug_level, 0, "Verbosity level");
	}
#endif

	DPRINTFN(hm, 11, "hm=%p\n", hm);

	hm->cb_state = HIDMAP_CB_IS_ATTACHING;

	hm->hid_items = malloc(hm->nhid_items * sizeof(struct hid_item),
	    M_DEVBUF, M_WAITOK | M_ZERO);

	hidbus_set_intr(hm->dev, hidmap_intr, hm);
	hm->evdev_methods = (struct evdev_methods) {
		.ev_open = &hidmap_ev_open,
		.ev_close = &hidmap_ev_close,
	};

	hm->evdev = evdev_alloc();
	evdev_set_name(hm->evdev, device_get_desc(hm->dev));
	evdev_set_phys(hm->evdev, device_get_nameunit(hm->dev));
	evdev_set_id(hm->evdev, hw->idBus, hw->idVendor, hw->idProduct,
	    hw->idVersion);
	evdev_set_serial(hm->evdev, hw->serial);
	evdev_set_flag(hm->evdev, EVDEV_FLAG_EXT_EPOCH); /* hidbus child */
	evdev_support_event(hm->evdev, EV_SYN);
	error = hidmap_parse_hid_descr(hm, hidbus_get_index(hm->dev));
	if (error) {
		DPRINTF(hm, "error=%d\n", error);
		hidmap_detach(hm);
		return (ENXIO);
	}

	evdev_set_methods(hm->evdev, hm->dev, &hm->evdev_methods);
	hm->cb_state = HIDMAP_CB_IS_RUNNING;

	error = evdev_register(hm->evdev);
	if (error) {
		DPRINTF(hm, "error=%d\n", error);
		hidmap_detach(hm);
		return (ENXIO);
	}

	return (0);
}

int
hidmap_detach(struct hidmap* hm)
{
	struct hidmap_hid_item *hi;

	DPRINTFN(hm, 11, "\n");

	hm->cb_state = HIDMAP_CB_IS_DETACHING;

	evdev_free(hm->evdev);
	if (hm->hid_items != NULL) {
		for (hi = hm->hid_items;
		     hi < hm->hid_items + hm->nhid_items;
		     hi++)
			if (hi->type == HIDMAP_TYPE_FINALCB ||
			    hi->type == HIDMAP_TYPE_CALLBACK)
				hi->cb(hm, hi, (union hidmap_cb_ctx){});
			else if (hi->type == HIDMAP_TYPE_ARR_LIST)
				free(hi->codes, M_DEVBUF);
		free(hm->hid_items, M_DEVBUF);
	}

	free(hm->key_press, M_DEVBUF);
	free(hm->key_rel, M_DEVBUF);

	return (0);
}

MODULE_DEPEND(hidmap, hid, 1, 1, 1);
MODULE_DEPEND(hidmap, hidbus, 1, 1, 1);
MODULE_DEPEND(hidmap, evdev, 1, 1, 1);
MODULE_VERSION(hidmap, 1);
