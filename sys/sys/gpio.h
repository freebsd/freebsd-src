/* $NetBSD: gpio.h,v 1.7 2009/09/25 20:27:50 mbalmer Exp $ */
/*	$OpenBSD: gpio.h,v 1.7 2008/11/26 14:51:20 mbalmer Exp $	*/
/*-
 * Copyright (c) 2009, Oleksandr Tymoshenko <gonzo@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 *
 */

/*
 * Copyright (c) 2009 Marc Balmer <marc@msys.ch>
 * Copyright (c) 2004 Alexander Yurchenko <grange@openbsd.org>
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

#ifndef __GPIO_H__
#define __GPIO_H__

#include <sys/ioccom.h>

/* GPIO pin states */
#define GPIO_PIN_LOW		0x00	/* low level (logical 0) */
#define GPIO_PIN_HIGH		0x01	/* high level (logical 1) */

/* Max name length of a pin */
#define GPIOMAXNAME		64

/* GPIO pin configuration flags */
#define GPIO_PIN_INPUT		0x00000001	/* input direction */
#define GPIO_PIN_OUTPUT		0x00000002	/* output direction */
#define GPIO_PIN_OPENDRAIN	0x00000004	/* open-drain output */
#define GPIO_PIN_PUSHPULL	0x00000008	/* push-pull output */
#define GPIO_PIN_TRISTATE	0x00000010	/* output disabled */
#define GPIO_PIN_PULLUP		0x00000020	/* internal pull-up enabled */
#define GPIO_PIN_PULLDOWN	0x00000040	/* internal pull-down enabled */
#define GPIO_PIN_INVIN		0x00000080	/* invert input */
#define GPIO_PIN_INVOUT		0x00000100	/* invert output */
#define GPIO_PIN_PULSATE	0x00000200	/* pulsate in hardware */
/* GPIO interrupt capabilities */
#define GPIO_INTR_NONE		0x00000000	/* no interrupt support */
#define GPIO_INTR_LEVEL_LOW	0x00010000	/* level trigger, low */
#define GPIO_INTR_LEVEL_HIGH	0x00020000	/* level trigger, high */
#define GPIO_INTR_EDGE_RISING	0x00040000	/* edge trigger, rising */
#define GPIO_INTR_EDGE_FALLING	0x00080000	/* edge trigger, falling */
#define GPIO_INTR_EDGE_BOTH	0x00100000	/* edge trigger, both */
#define GPIO_INTR_MASK		(GPIO_INTR_LEVEL_LOW | GPIO_INTR_LEVEL_HIGH | \
				GPIO_INTR_EDGE_RISING |			      \
				GPIO_INTR_EDGE_FALLING | GPIO_INTR_EDGE_BOTH)

struct gpio_pin {
	uint32_t gp_pin;			/* pin number */
	char gp_name[GPIOMAXNAME];		/* human-readable name */
	uint32_t gp_caps;			/* capabilities */
	uint32_t gp_flags;			/* current flags */
};

/* GPIO pin request (read/write/toggle) */
struct gpio_req {
	uint32_t gp_pin;			/* pin number */
	uint32_t gp_value;			/* value */
};

/*
 * ioctls
 */
#define GPIOMAXPIN		_IOR('G', 0, int)
#define	GPIOGETCONFIG		_IOWR('G', 1, struct gpio_pin)
#define	GPIOSETCONFIG		_IOW('G', 2, struct gpio_pin)
#define	GPIOGET			_IOWR('G', 3, struct gpio_req)
#define	GPIOSET			_IOW('G', 4, struct gpio_req)
#define	GPIOTOGGLE		_IOWR('G', 5, struct gpio_req)
#define	GPIOSETNAME		_IOW('G', 6, struct gpio_pin)

#endif /* __GPIO_H__ */
