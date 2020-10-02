/*-
 * Copyright (c) 2020 The FreeBSD Foundation
 *
 * This software was developed by Emmanuel Vadot under sponsorship
 * from the FreeBSD Foundation.
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
 *
 * $FreeBSD$
 */

#ifndef _LINUX_BACKLIGHT_H_
#define _LINUX_BACKLIGHT_H_

#include <linux/notifier.h>

struct backlight_device;

enum backlight_type {
	BACKLIGHT_RAW = 0,
};

struct backlight_properties {
	int type;
	int max_brightness;
	int brightness;
	int power;
};

enum backlight_notification {
	BACKLIGHT_REGISTERED,
	BACKLIGHT_UNREGISTERED,
};

enum backlight_update_reason {
	BACKLIGHT_UPDATE_HOTKEY = 0
};

struct backlight_ops {
	int options;
#define	BL_CORE_SUSPENDRESUME   1
	int (*update_status)(struct backlight_device *);
	int (*get_brightness)(struct backlight_device *);
};

struct backlight_device {
	const struct backlight_ops *ops;
	struct backlight_properties props;
	void *data;
	struct device *dev;
	char *name;
};

#define bl_get_data(bd) (bd)->data

struct backlight_device *linux_backlight_device_register(const char *name,
    struct device *dev, void *data, const struct backlight_ops *ops, struct backlight_properties *props);
void linux_backlight_device_unregister(struct backlight_device *bd);
#define	backlight_device_register(name, dev, data, ops, props)	\
	linux_backlight_device_register(name, dev, data, ops, props)
#define	backlight_device_unregister(bd)	linux_backlight_device_unregister(bd)

static inline void
backlight_update_status(struct backlight_device *bd)
{
	bd->ops->update_status(bd);
}

static inline void
backlight_force_update(struct backlight_device *bd, int reason)
{
	bd->props.brightness = bd->ops->get_brightness(bd);
}

#endif	/* _LINUX_BACKLIGHT_H_ */
