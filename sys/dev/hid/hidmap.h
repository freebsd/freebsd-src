/*-
 * SPDX-License-Identifier: BSD-2-Clause
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

#ifndef _HIDMAP_H_
#define _HIDMAP_H_

#include <sys/param.h>

#include <dev/hid/hid.h>

#define	HIDMAP_MAX_MAPS	4

struct hid_device_id;
struct hidmap_hid_item;
struct hidmap_item;
struct hidmap;

enum hidmap_cb_state {
	HIDMAP_CB_IS_PROBING,
	HIDMAP_CB_IS_ATTACHING,
	HIDMAP_CB_IS_RUNNING,
	HIDMAP_CB_IS_DETACHING,
};

#define	HIDMAP_KEY_NULL	0xFF	/* Special event code to discard input */

/* Third parameter of hidmap callback has different type depending on state */
union hidmap_cb_ctx {
	struct hid_item	*hi;	/* Probe- and attach-stage callbacks */
	int32_t		data;	/* Run-stage callbacks */
	uint8_t		rid;	/* Run-stage finalizing callbacks */
};

#define	HIDMAP_CB_ARGS							\
	struct hidmap *hm, struct hidmap_hid_item *hi, union hidmap_cb_ctx ctx
typedef int hidmap_cb_t(HIDMAP_CB_ARGS);

/* These helpers can be used at any stage of any callbacks */
#define	HIDMAP_CB_GET_STATE(...)					\
	((hm == NULL) ? HIDMAP_CB_IS_PROBING : hm->cb_state)
#define	HIDMAP_CB_GET_DEV(...)						\
	(hm == NULL ? NULL : hm->dev)
#define	HIDMAP_CB_GET_SOFTC(...)					\
	(hm == NULL ? NULL : device_get_softc(hm->dev))
#define	HIDMAP_CB_GET_EVDEV(...)					\
	(hm == NULL ? NULL : hm->evdev)
#define	HIDMAP_CB_UDATA		(hi->udata)
#define	HIDMAP_CB_UDATA64	(hi->udata64)
/* Special helpers for run-stage of finalizing callbacks */
#define	HIDMAP_CB_GET_RID(...)	(ctx.rid)
#define	HIDMAP_CB_GET_DATA(loc)						\
	hid_get_data(hm->intr_buf, hm->intr_len, (loc))
#define	HIDMAP_CB_GET_UDATA(loc)					\
	hid_get_udata(hm->intr_buf, hm->intr_len, (loc))

enum hidmap_relabs {
	HIDMAP_RELABS_ANY = 0,
	HIDMAP_RELATIVE,
	HIDMAP_ABSOLUTE,
};

struct hidmap_item {
	union {
		struct {
			uint16_t	type;	/* Evdev event type */
			uint16_t	code;	/* Evdev event code */
			uint16_t	fuzz;	/* Evdev event fuzz */
			uint16_t	flat;	/* Evdev event flat */
		};
		hidmap_cb_t		*cb;	/* Reporting callback */
	};
	int32_t 		usage;		/* HID usage (base) */
	uint16_t		nusages;	/* number of usages */
	bool			required:1;	/* Required by driver */
	enum hidmap_relabs	relabs:2;
	bool			has_cb:1;
	bool			final_cb:1;
	bool			invert_value:1;
	bool			forbidden:1;	/* Forbidden by driver */
	u_int			reserved:9;
};

#define	HIDMAP_ANY(_page, _usage, _type, _code)				\
	.usage = HID_USAGE2((_page), (_usage)),				\
	.nusages = 1,							\
	.type = (_type),						\
	.code = (_code)
#define	HIDMAP_ANY_RANGE(_page, _usage_from, _usage_to, _type, _code)	\
	.usage = HID_USAGE2((_page), (_usage_from)),			\
	.nusages = (_usage_to) - (_usage_from) + 1,			\
	.type = (_type),						\
	.code = (_code)
#define	HIDMAP_ANY_CB(_page, _usage, _callback)				\
	.usage = HID_USAGE2((_page), (_usage)),				\
	.nusages = 1,							\
	.cb = (_callback),						\
	.has_cb = true
#define	HIDMAP_ANY_CB_RANGE(_page, _usage_from, _usage_to, _callback)	\
	.usage = HID_USAGE2((_page), (_usage_from)),			\
	.nusages = (_usage_to) - (_usage_from) + 1,			\
	.cb = (_callback),						\
	.has_cb = true
#define	HIDMAP_KEY(_page, _usage, _code)				\
	HIDMAP_ANY((_page), (_usage), EV_KEY, (_code)),			\
		.relabs = HIDMAP_RELABS_ANY
#define	HIDMAP_KEY_RANGE(_page, _ufrom, _uto, _code)			\
	HIDMAP_ANY_RANGE((_page), (_ufrom), (_uto), EV_KEY, (_code)),	\
		.relabs = HIDMAP_RELABS_ANY
#define	HIDMAP_REL(_page, _usage, _code)				\
	HIDMAP_ANY((_page), (_usage), EV_REL, (_code)),			\
		.relabs = HIDMAP_RELATIVE
#define	HIDMAP_ABS(_page, _usage, _code)				\
	HIDMAP_ANY((_page), (_usage), EV_ABS, (_code)),			\
		.relabs = HIDMAP_ABSOLUTE
#define	HIDMAP_SW(_page, _usage, _code)					\
	HIDMAP_ANY((_page), (_usage), EV_SW, (_code)),			\
		.relabs = HIDMAP_RELABS_ANY
#define	HIDMAP_REL_CB(_page, _usage, _callback)				\
	HIDMAP_ANY_CB((_page), (_usage), (_callback)),			\
		.relabs = HIDMAP_RELATIVE
#define	HIDMAP_ABS_CB(_page, _usage, _callback)				\
	HIDMAP_ANY_CB((_page), (_usage), (_callback)),			\
		.relabs = HIDMAP_ABSOLUTE
/*
 * Special callback function which is not tied to particular HID input usage
 * but called at the end evdev properties setting or interrupt handler
 * just before evdev_register() or evdev_sync() calls.
 */
#define	HIDMAP_FINAL_CB(_callback)					\
	HIDMAP_ANY_CB(0, 0, (_callback)), .final_cb = true

enum hidmap_type {
	HIDMAP_TYPE_FINALCB = 0,/* No HID item associated. Runs unconditionally
				 * at the end of other items processing */
	HIDMAP_TYPE_CALLBACK,	/* HID item is reported with user callback */
	HIDMAP_TYPE_VARIABLE,	/* HID item is variable (single usage) */
	HIDMAP_TYPE_VAR_NULLST,	/* HID item is null state variable */
	HIDMAP_TYPE_ARR_LIST,	/* HID item is array with list of usages */
	HIDMAP_TYPE_ARR_RANGE,	/* Array with range (min;max) of usages */
};

struct hidmap_hid_item {
	union {
		hidmap_cb_t	*cb;		/* Callback */
		struct {			/* Variable */
			uint16_t	evtype;	/* Evdev event type */
			uint16_t	code;	/* Evdev event code */
		};
		uint16_t	*codes;		/* Array list map type */
		int32_t		umin;		/* Array range map type */
	};
	union {
		void		*udata;		/* Callback private context */
		uint64_t	udata64;
		int32_t		last_val;	/* Last reported value (var) */
		uint16_t	last_key;	/* Last reported key (array) */
	};
	struct hid_location	loc;		/* HID item location */
	int32_t			lmin;		/* HID item logical minimum */
	int32_t			lmax;		/* HID item logical maximum */
	enum hidmap_type	type:8;
	uint8_t			id;		/* Report ID */
	bool			invert_value;
};

struct hidmap {
	device_t		dev;

	struct evdev_dev	*evdev;
	struct evdev_methods	evdev_methods;

	/* Scatter-gather list of maps */
	int			nmaps;
	uint32_t		nmap_items[HIDMAP_MAX_MAPS];
	const struct hidmap_item	*map[HIDMAP_MAX_MAPS];

	/* List of preparsed HID items */
	uint32_t		nhid_items;
	struct hidmap_hid_item	*hid_items;

	/* Key event merging buffers */
	uint8_t			*key_press;
	uint8_t			*key_rel;
	uint16_t		key_min;
	uint16_t		key_max;

	int			*debug_var;
	int			debug_level;
	enum hidmap_cb_state	cb_state;
	void *			intr_buf;
	hid_size_t		intr_len;
};

typedef	uint8_t *		hidmap_caps_t;
#define	HIDMAP_CAPS_SZ(nitems)	howmany((nitems), 8)
#define	HIDMAP_CAPS(name, map)	uint8_t	(name)[HIDMAP_CAPS_SZ(nitems(map))]
static inline bool
hidmap_test_cap(hidmap_caps_t caps, int cap)
{
	return (isset(caps, cap) != 0);
}

/*
 * It is safe to call any of following procedures in device_probe context
 * that makes possible to write probe-only drivers with attach/detach handlers
 * inherited from hidmap. See hcons and hsctrl drivers for example.
 */
static inline void
hidmap_set_dev(struct hidmap *hm, device_t dev)
{
	hm->dev = dev;
}

/* Hack to avoid #ifdef-ing of hidmap_set_debug_var in hidmap based drivers */
#ifdef HID_DEBUG
#define	hidmap_set_debug_var(h, d)	_hidmap_set_debug_var((h), (d))
#else
#define	hidmap_set_debug_var(...)
#endif
void	_hidmap_set_debug_var(struct hidmap *hm, int *debug_var);
#define	HIDMAP_ADD_MAP(hm, map, caps)					\
	hidmap_add_map((hm), (map), nitems(map), (caps))
uint32_t hidmap_add_map(struct hidmap *hm, const struct hidmap_item *map,
	    int nitems_map, hidmap_caps_t caps);

/* Versions of evdev_* functions capable to merge key events with same codes */
void	hidmap_support_key(struct hidmap *hm, uint16_t key);
void	hidmap_push_key(struct hidmap *hm, uint16_t key, int32_t value);

void	hidmap_intr(void *context, void *buf, hid_size_t len);
#define	HIDMAP_PROBE(hm, dev, id, map, suffix)				\
	hidmap_probe((hm), (dev), (id), nitems(id), (map), nitems(map),	\
	    (suffix), NULL)
int	hidmap_probe(struct hidmap* hm, device_t dev,
	    const struct hid_device_id *id, int nitems_id,
	    const struct hidmap_item *map, int nitems_map,
	    const char *suffix, hidmap_caps_t caps);
int	hidmap_attach(struct hidmap *hm);
int	hidmap_detach(struct hidmap *hm);

#endif	/* _HIDMAP_H_ */
