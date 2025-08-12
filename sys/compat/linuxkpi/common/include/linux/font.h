/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025-2026 The FreeBSD Foundation
 * Copyright (c) 2025-2026 Jean-Sébastien Pédron <dumbbell@FreeBSD.org>
 *
 * This software was developed by Jean-Sébastien Pédron under sponsorship
 * from the FreeBSD Foundation.
 */

#ifndef _LINUXKPI_LINUX_FONT_H_
#define	_LINUXKPI_LINUX_FONT_H_

#include <linux/types.h>

struct font_desc {
	const char *name;
	const void *data;
	int idx;
	unsigned int width;
	unsigned int height;
	unsigned int charcount;
	int pref;
};

static inline const struct font_desc *
get_default_font(int xres, int yres, unsigned long *font_w,
    unsigned long *font_h)
{
	return (NULL);
}

#endif
