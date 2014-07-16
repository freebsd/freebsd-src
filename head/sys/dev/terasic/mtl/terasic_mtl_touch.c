
/*-
 * Copyright (c) 2014 Jakub Wojciech Klama <jceel@FreeBSD.org>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/condvar.h>
#include <sys/conf.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/fbio.h>				/* video_adapter_t */
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/systm.h>

#include <machine/bus.h>

#include <dev/evdev/evdev.h>
#include <dev/evdev/input.h>

#include <dev/terasic/mtl/terasic_mtl.h>

static void terasic_mtl_touch_intr(void *);
static evdev_open_t terasic_mtl_touch_open;
static evdev_close_t terasic_mtl_touch_close;

struct evdev_methods terasic_mtl_ev_methods = {
	.ev_open = &terasic_mtl_touch_open,
	.ev_close = &terasic_mtl_touch_close,
};


static uint16_t terasic_mtl_gestures[256] = {
	/* Single finger gestures */
	[TSG_NONE] = KEY_RESERVED,
	[TSG_NORTH] = BTN_NORTH,
	[TSG_EAST] = BTN_EAST,
	[TSG_WEST] = BTN_WEST,
	[TSG_SOUTH] = BTN_SOUTH,
	[TSG_CLICK] = BTN_TOUCH,
	[TSG_DCLICK] = BTN_TOUCH,
	[TSG_ROTATE_CW] = BTN_1,
	[TSG_ROTATE_CCW] = BTN_2,
	/* Two finger gestures */
	[TSG2_NORTH] = BTN_NORTH,
	[TSG2_EAST] = BTN_EAST,
	[TSG2_WEST] = BTN_WEST,
	[TSG2_SOUTH] = BTN_SOUTH,
	[TSG2_CLICK] = BTN_TOOL_DOUBLETAP,
	[TSG2_ZOOM_IN] = KEY_ZOOMIN,
	[TSG2_ZOOM_OUT] = KEY_ZOOMOUT,
};

int
terasic_mtl_touch_attach(struct terasic_mtl_softc *sc)
{
	struct input_absinfo absinfo;
	sc->mtl_evdev = evdev_alloc();
	evdev_set_name(sc->mtl_evdev, device_get_desc(sc->mtl_dev));
	evdev_set_softc(sc->mtl_evdev, sc);
	evdev_set_methods(sc->mtl_evdev, &terasic_mtl_ev_methods);
	evdev_support_event(sc->mtl_evdev, EV_SYN);
	evdev_support_event(sc->mtl_evdev, EV_KEY);
	evdev_support_event(sc->mtl_evdev, EV_ABS);
	evdev_support_abs(sc->mtl_evdev, ABS_MT_POSITION_X);
	evdev_support_abs(sc->mtl_evdev, ABS_MT_POSITION_Y);

	/* Support gestures as a keys */
	evdev_support_key(sc->mtl_evdev, BTN_TOUCH);
	evdev_support_key(sc->mtl_evdev, BTN_TOOL_DOUBLETAP);
	evdev_support_key(sc->mtl_evdev, BTN_NORTH);
	evdev_support_key(sc->mtl_evdev, BTN_SOUTH);
	evdev_support_key(sc->mtl_evdev, BTN_WEST);
	evdev_support_key(sc->mtl_evdev, BTN_EAST);
	evdev_support_key(sc->mtl_evdev, BTN_1);
	evdev_support_key(sc->mtl_evdev, BTN_2);
	evdev_support_key(sc->mtl_evdev, KEY_ZOOMIN);
	evdev_support_key(sc->mtl_evdev, KEY_ZOOMOUT);

	bzero(&absinfo, sizeof(struct input_absinfo));

	/* Set X axis bounds */
	absinfo.minimum = 0;
	absinfo.maximum = 1024;
	evdev_set_absinfo(sc->mtl_evdev, ABS_MT_POSITION_X, &absinfo);

	/* Set Y axis bounds */
	absinfo.minimum = 0;
	absinfo.maximum = 512;
	evdev_set_absinfo(sc->mtl_evdev, ABS_MT_POSITION_Y, &absinfo);

	evdev_register(sc->mtl_dev, sc->mtl_evdev);

	callout_init(&sc->mtl_evdev_callout, 1);
	return (0);
}

void
terasic_mtl_touch_detach(struct terasic_mtl_softc *sc)
{

	evdev_unregister(sc->mtl_dev, sc->mtl_evdev);
}

static int
terasic_mtl_touch_open(struct evdev_dev *evdev, void *softc)
{
	struct terasic_mtl_softc *sc = (struct terasic_mtl_softc *)softc;

	/* Start polling */
	callout_reset(&sc->mtl_evdev_callout, hz / 10,
	    terasic_mtl_touch_intr, sc);

	sc->mtl_evdev_opened = true;
	return (0);
}

static void
terasic_mtl_touch_close(struct evdev_dev *evdev, void *softc)
{
	struct terasic_mtl_softc *sc = (struct terasic_mtl_softc *)softc;

	/* Stop polling */
	callout_stop(&sc->mtl_evdev_callout);

	sc->mtl_evdev_opened = false;
}

static void
terasic_mtl_touch_intr(void *arg)
{
	struct terasic_mtl_softc *sc = (struct terasic_mtl_softc *)arg;
	struct terasic_mtl_touch_state *state = &sc->mtl_evdev_state;
	int32_t x1, y1, x2, y2;
	bool sync = false;

	int tsg = le32toh(bus_read_4(sc->mtl_reg_res,
	    TERASIC_MTL_OFF_TOUCHGESTURE));
	int touchpoints = tsg >> 8;
	int gesture = tsg & 0xff;

	if (touchpoints == 0 || touchpoints == 0xffffff) {
		/* 
		 * Check if it's a release and if so, push empty
		 * MT_SYNC event
		 */
		if (state->touchpoints != 0) {
			evdev_mt_sync(sc->mtl_evdev);
			evdev_sync(sc->mtl_evdev);
		}

		state->touchpoints = 0;

		callout_reset(&sc->mtl_evdev_callout, hz / 10,
		    terasic_mtl_touch_intr, sc);
		return;
	}

	if (gesture != 0 && gesture != 0xff) {
		uint16_t code = terasic_mtl_gestures[gesture];
		if (code != KEY_RESERVED) {
			evdev_push_event(sc->mtl_evdev, EV_KEY, code, 1);
			sync = true;
			state->gesture = code;
		}
	} else {
		if (state->gesture != 0) {
			/* Push a release event */
			evdev_push_event(sc->mtl_evdev, EV_KEY, state->gesture, 0);
			sync = true;
			state->gesture = 0;
		}
	}

	if (touchpoints > 0) {
		x1 = le32toh(bus_read_4(sc->mtl_reg_res,
		    TERASIC_MTL_OFF_TOUCHPOINT_X1));
		y1 = le32toh(bus_read_4(sc->mtl_reg_res,
		    TERASIC_MTL_OFF_TOUCHPOINT_Y1));

		if (x1 != -1 && y1 != -1 && (x1 != state->x1) &&
		    (y1 != state->y1)) {
			evdev_push_event(sc->mtl_evdev, EV_ABS,
			    ABS_MT_POSITION_X, x1);
			evdev_push_event(sc->mtl_evdev, EV_ABS,
			    ABS_MT_POSITION_Y, y1);
			evdev_mt_sync(sc->mtl_evdev);

			state->x1 = x1;
			state->y1 = y1;
			sync = true;
		}
	}

	if (touchpoints > 1) {
		x2 = le32toh(bus_read_4(sc->mtl_reg_res,
		    TERASIC_MTL_OFF_TOUCHPOINT_X2));
		y2 = le32toh(bus_read_4(sc->mtl_reg_res,
		    TERASIC_MTL_OFF_TOUCHPOINT_Y2));

		if (x2 != -1 && y2 != -1 && (x2 != state->x2) &&
		    (y2 != state->y2)) {
			evdev_push_event(sc->mtl_evdev, EV_ABS,
			    ABS_MT_POSITION_X, x2);
			evdev_push_event(sc->mtl_evdev, EV_ABS,
			    ABS_MT_POSITION_Y, y2);
			evdev_mt_sync(sc->mtl_evdev);

			state->x2 = x2;
			state->y2 = y2;
			sync = true;
		}
	}

	state->touchpoints = touchpoints;
	
	if (sync)
		evdev_sync(sc->mtl_evdev);

	callout_reset(&sc->mtl_evdev_callout, hz / 10,
	    terasic_mtl_touch_intr, sc);
}
