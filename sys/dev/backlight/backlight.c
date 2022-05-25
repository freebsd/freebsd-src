/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2020 Emmanuel Vadot <manu@FreeBSD.org>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/limits.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/sx.h>

#include <dev/backlight/backlight.h>

#include "backlight_if.h"

static struct sx backlight_sx;
static MALLOC_DEFINE(M_BACKLIGHT, "BACKLIGHT", "Backlight driver");
static struct unrhdr *backlight_unit;

struct backlight_softc {
	struct cdev *cdev;
	struct cdev *alias;
	int unit;
	device_t dev;
	uint32_t cached_brightness;
};

static int
backlight_ioctl(struct cdev *dev, u_long cmd, caddr_t data,
    int fflag, struct thread *td)
{
	struct backlight_softc *sc;
	struct backlight_props props;
	struct backlight_info info;
	int error;

	sc = dev->si_drv1;

	switch (cmd) {
	case BACKLIGHTGETSTATUS:
		/* Call the driver function so it fills up the props */
		bcopy(data, &props, sizeof(struct backlight_props));
		error = BACKLIGHT_GET_STATUS(sc->dev, &props);
		if (error == 0) {
			bcopy(&props, data, sizeof(struct backlight_props));
			sc->cached_brightness = props.brightness;
		}
		break;
	case BACKLIGHTUPDATESTATUS:
		bcopy(data, &props, sizeof(struct backlight_props));
		if (props.brightness == sc->cached_brightness)
			return (0);
		error = BACKLIGHT_UPDATE_STATUS(sc->dev, &props);
		if (error == 0) {
			bcopy(&props, data, sizeof(struct backlight_props));
			sc->cached_brightness = props.brightness;
		}
		break;
	case BACKLIGHTGETINFO:
		memset(&info, 0, sizeof(info));
		error = BACKLIGHT_GET_INFO(sc->dev, &info);
		if (error == 0)
			bcopy(&info, data, sizeof(struct backlight_info));
		break;
	}

	return (error);
}

static struct cdevsw backlight_cdevsw = {
	.d_version =	D_VERSION,
	.d_ioctl =	backlight_ioctl,
	.d_name =	"backlight",
};

struct cdev *
backlight_register(const char *name, device_t dev)
{
	struct make_dev_args args;
	struct backlight_softc *sc;
	struct backlight_props props;
	int error;

	sc = malloc(sizeof(*sc), M_BACKLIGHT, M_WAITOK | M_ZERO);

	sx_xlock(&backlight_sx);
	sc->unit = alloc_unr(backlight_unit);
	sc->dev = dev;
	make_dev_args_init(&args);
	args.mda_flags = MAKEDEV_CHECKNAME | MAKEDEV_WAITOK;
	args.mda_devsw = &backlight_cdevsw;
	args.mda_uid = UID_ROOT;
	args.mda_gid = GID_VIDEO;
	args.mda_mode = 0660;
	args.mda_si_drv1 = sc;
	error = make_dev_s(&args, &sc->cdev, "backlight/backlight%d", sc->unit);

	if (error != 0)
		goto fail;

	error = make_dev_alias_p(MAKEDEV_CHECKNAME | MAKEDEV_WAITOK,
	  &sc->alias, sc->cdev, "backlight/%s%d", name, sc->unit);
	if (error != 0)
		device_printf(dev, "Cannot register with alias %s%d\n", name,
		    sc->unit);

	sx_xunlock(&backlight_sx);

	error = BACKLIGHT_GET_STATUS(sc->dev, &props);
	sc->cached_brightness = props.brightness;

	return (sc->cdev);
fail:
	sx_xunlock(&backlight_sx);
	return (NULL);
}

int
backlight_destroy(struct cdev *dev)
{
	struct backlight_softc *sc;

	sc = dev->si_drv1;
	sx_xlock(&backlight_sx);
	free_unr(backlight_unit, sc->unit);
	destroy_dev(dev);
	sx_xunlock(&backlight_sx);
	return (0);
}

static void
backlight_drvinit(void *unused)
{

	backlight_unit = new_unrhdr(0, INT_MAX, NULL);
	sx_init(&backlight_sx, "Backlight sx");
}

SYSINIT(backlightdev, SI_SUB_DRIVERS, SI_ORDER_MIDDLE, backlight_drvinit, NULL);
MODULE_VERSION(backlight, 1);
