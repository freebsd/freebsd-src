/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Vladimir Kondratyev <wulf@FreeBSD.org>
 * Copyright (c) 2026 Christos Longros <chris.longros@gmail.com>
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

/*
 * Sony PS5 DualSense driver
 * https://controllers.fandom.com/wiki/Sony_DualSense
 * https://github.com/torvalds/linux/blob/master/drivers/hid/hid-playstation.c
 *
 * Touchpad and motion sensors are not supported yet.
 */

#include "opt_hid.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/stat.h>
#include <sys/sx.h>
#include <sys/sysctl.h>

#include <dev/evdev/input.h>
#include <dev/evdev/evdev.h>

#define	HID_DEBUG_VAR	ps5ds_debug
#include <dev/hid/hgame.h>
#include <dev/hid/hid.h>
#include <dev/hid/hidbus.h>
#include <dev/hid/hidquirk.h>
#include <dev/hid/hidmap.h>
#include "usbdevs.h"

#ifdef HID_DEBUG
static int ps5ds_debug = 1;

static SYSCTL_NODE(_hw_hid, OID_AUTO, ps5dsense, CTLFLAG_RW, 0,
		"Sony PS5 DualSense Gamepad");
SYSCTL_INT(_hw_hid_ps5dsense, OID_AUTO, debug, CTLFLAG_RWTUN,
		&ps5ds_debug, 0, "Debug level");
#endif

/*
 * Fixed HID report descriptor.  Output is 62 bytes, not 47: shorter reports
 * are ignored.
 */
static const uint8_t	ps5ds_rdesc[] = {
	0x05, 0x01,		/* Usage Page (Generic Desktop Ctrls)	*/
	0x09, 0x05,		/* Usage (Game Pad)			*/
	0xA1, 0x01,		/* Collection (Application)		*/
	0x85, 0x01,		/*   Report ID (1)			*/
	0x09, 0x30,		/*   Usage (X)  - left stick X		*/
	0x09, 0x31,		/*   Usage (Y)  - left stick Y		*/
	0x09, 0x33,		/*   Usage (Rx) - right stick X		*/
	0x09, 0x34,		/*   Usage (Ry) - right stick Y		*/
	0x15, 0x00,		/*   Logical Minimum (0)		*/
	0x26, 0xFF, 0x00,	/*   Logical Maximum (255)		*/
	0x75, 0x08,		/*   Report Size (8)			*/
	0x95, 0x04,		/*   Report Count (4)			*/
	0x81, 0x02,		/*   Input (Data,Var,Abs)		*/
	0x09, 0x32,		/*   Usage (Z)  - L2 trigger		*/
	0x09, 0x35,		/*   Usage (Rz) - R2 trigger		*/
	0x15, 0x00,		/*   Logical Minimum (0)		*/
	0x26, 0xFF, 0x00,	/*   Logical Maximum (255)		*/
	0x75, 0x08,		/*   Report Size (8)			*/
	0x95, 0x02,		/*   Report Count (2)			*/
	0x81, 0x02,		/*   Input (Data,Var,Abs)		*/
	0x06, 0x00, 0xFF,	/*   Usage Page (Vendor Defined 0xFF00)	*/
	0x09, 0x20,		/*   Usage (0x20)			*/
	0x15, 0x00,		/*   Logical Minimum (0)		*/
	0x26, 0xFF, 0x00,	/*   Logical Maximum (255)		*/
	0x75, 0x08,		/*   Report Size (8)			*/
	0x95, 0x01,		/*   Report Count (1)			*/
	0x81, 0x02,		/*   Input (Data,Var,Abs) - counter	*/
	0x05, 0x01,		/*   Usage Page (Generic Desktop)	*/
	0x09, 0x39,		/*   Usage (Hat switch)			*/
	0x15, 0x00,		/*   Logical Minimum (0)		*/
	0x25, 0x07,		/*   Logical Maximum (7)		*/
	0x35, 0x00,		/*   Physical Minimum (0)		*/
	0x46, 0x3B, 0x01,	/*   Physical Maximum (315)		*/
	0x65, 0x14,		/*   Unit (Eng Rot: Degree)		*/
	0x75, 0x04,		/*   Report Size (4)			*/
	0x95, 0x01,		/*   Report Count (1)			*/
	0x81, 0x42,		/*   Input (Data,Var,Abs,Null State)	*/
	0x65, 0x00,		/*   Unit (None)			*/
	0x45, 0x00,		/*   Physical Maximum (0)		*/
	0x05, 0x09,		/*   Usage Page (Button)		*/
	0x19, 0x01,		/*   Usage Minimum (0x01)		*/
	0x29, 0x0F,		/*   Usage Maximum (0x0F)		*/
	0x15, 0x00,		/*   Logical Minimum (0)		*/
	0x25, 0x01,		/*   Logical Maximum (1)		*/
	0x75, 0x01,		/*   Report Size (1)			*/
	0x95, 0x0F,		/*   Report Count (15)			*/
	0x81, 0x02,		/*   Input (Data,Var,Abs)		*/
	0x06, 0x00, 0xFF,	/*   Usage Page (Vendor Defined 0xFF00)	*/
	0x09, 0x21,		/*   Usage (0x21)			*/
	0x75, 0x01,		/*   Report Size (1)			*/
	0x95, 0x0D,		/*   Report Count (13)			*/
	0x81, 0x03,		/*   Input (Const) - unused button bits	*/
	0x09, 0x22,		/*   Usage (0x22)			*/
	0x15, 0x00,		/*   Logical Minimum (0)		*/
	0x26, 0xFF, 0x00,	/*   Logical Maximum (255)		*/
	0x75, 0x08,		/*   Report Size (8)			*/
	0x95, 0x34,		/*   Report Count (52)			*/
	0x81, 0x03,		/*   Input (Const) - sensors, touchpad	*/
	/* Output report (63 bytes total) for LED and rumble control */
	0x85, 0x02,		/*   Report ID (2)			*/
	0x09, 0x23,		/*   Usage (0x23)			*/
	0x75, 0x08,		/*   Report Size (8)			*/
	0x95, 0x3E,		/*   Report Count (62)			*/
	0x91, 0x02,		/*   Output (Data,Var,Abs)		*/
	0xC0,			/* End Collection			*/
};

#define	PS5DS_OUTPUT_REPORT2_SIZE	63

struct ps5ds_out2 {
	uint8_t	valid_flag0;
	uint8_t	valid_flag1;
	uint8_t	rumble_right;
	uint8_t	rumble_left;
	uint8_t	reserved1[4];
	uint8_t	mute_button_led;
	uint8_t	power_save_control;
	uint8_t	reserved2[28];
	uint8_t	valid_flag2;
	uint8_t	reserved3[2];
	uint8_t	lightbar_setup;
	uint8_t	led_brightness;
	uint8_t	player_leds;
	uint8_t	led_color_r;
	uint8_t	led_color_g;
	uint8_t	led_color_b;
} __attribute__((packed));

#define	PS5DS_FLAG1_LIGHTBAR		0x04
#define	PS5DS_FLAG1_PLAYER_LEDS		0x10
#define	PS5DS_FLAG2_LIGHTBAR_SETUP	0x02
#define	PS5DS_LIGHTBAR_FADE_OUT		0x02

static const uint8_t ps5ds_player_leds[] = {
	0x04,	/* Player 1: --x-- */
	0x0A,	/* Player 2: -x-x- */
	0x15,	/* Player 3: x-x-x */
	0x1B,	/* Player 4: xx-xx */
	0x1F,	/* Player 5: xxxxx */
};

static const struct ps5ds_led {
	int	r;
	int	g;
	int	b;
} ps5ds_leds[] = {
	/* The first 4 entries match the PS4, other from Linux driver */
	{ 0x00, 0x00, 0x40 },	/* Blue   */
	{ 0x40, 0x00, 0x00 },	/* Red    */
	{ 0x00, 0x40, 0x00 },	/* Green  */
	{ 0x20, 0x00, 0x20 },	/* Pink   */
	{ 0x02, 0x01, 0x00 },	/* Orange */
	{ 0x00, 0x01, 0x01 },	/* Teal   */
	{ 0x01, 0x01, 0x01 }	/* White  */
};

enum ps5ds_led_state {
	PS5DS_LED_OFF,
	PS5DS_LED_ON,
	PS5DS_LED_CNT,
};

struct ps5ds_softc {
	struct hidmap	hm;

	struct sx	lock;
	enum ps5ds_led_state	led_state;
	struct ps5ds_led	led_color;
	int		player_leds;	/* 0-4: player number, -1: off */
};

#define	PS5DS_OFFSET(field) offsetof(struct ps5ds_softc, field)
enum {
	PS5DS_SYSCTL_LED_STATE =	PS5DS_OFFSET(led_state),
	PS5DS_SYSCTL_LED_COLOR_R =	PS5DS_OFFSET(led_color.r),
	PS5DS_SYSCTL_LED_COLOR_G =	PS5DS_OFFSET(led_color.g),
	PS5DS_SYSCTL_LED_COLOR_B =	PS5DS_OFFSET(led_color.b),
	PS5DS_SYSCTL_PLAYER_LEDS =	PS5DS_OFFSET(player_leds),
#define	PS5DS_SYSCTL_LAST		PS5DS_SYSCTL_PLAYER_LEDS
};

#define PS5DS_MAP_BTN(number, code)		\
	{ HIDMAP_KEY(HUP_BUTTON, number, code) }
#define PS5DS_MAP_ABS(usage, code)		\
	{ HIDMAP_ABS(HUP_GENERIC_DESKTOP, HUG_##usage, code) }
#define PS5DS_MAP_FLT(usage, code)		\
	{ HIDMAP_ABS(HUP_GENERIC_DESKTOP, HUG_##usage, code), .flat = 15 }
#define	PS5DS_MAP_GCB(usage, callback)		\
	{ HIDMAP_ANY_CB(HUP_GENERIC_DESKTOP, HUG_##usage, callback) }
#define	PS5DS_FINALCB(cb)			\
	{ HIDMAP_FINAL_CB(&cb) }

static hidmap_cb_t	ps5ds_final_cb;

static const struct hidmap_item ps5ds_map[] = {
	PS5DS_MAP_FLT(X,		ABS_X),
	PS5DS_MAP_FLT(Y,		ABS_Y),
	PS5DS_MAP_ABS(Z,		ABS_Z),
	PS5DS_MAP_ABS(RZ,		ABS_RZ),
	PS5DS_MAP_FLT(RX,		ABS_RX),
	PS5DS_MAP_FLT(RY,		ABS_RY),
	PS5DS_MAP_BTN(1,		BTN_WEST),
	PS5DS_MAP_BTN(2,		BTN_SOUTH),
	PS5DS_MAP_BTN(3,		BTN_EAST),
	PS5DS_MAP_BTN(4,		BTN_NORTH),
	PS5DS_MAP_BTN(5,		BTN_TL),
	PS5DS_MAP_BTN(6,		BTN_TR),
	PS5DS_MAP_BTN(7,		BTN_TL2),
	PS5DS_MAP_BTN(8,		BTN_TR2),
	PS5DS_MAP_BTN(9,		BTN_SELECT),
	PS5DS_MAP_BTN(10,		BTN_START),
	PS5DS_MAP_BTN(11,		BTN_THUMBL),
	PS5DS_MAP_BTN(12,		BTN_THUMBR),
	PS5DS_MAP_BTN(13,		BTN_MODE),
	/* Click button is handled by touchpad driver */
	/* PS5DS_MAP_BTN(14,	BTN_LEFT), */
	/* Linux mutes the microphone in hardware and emits no event */
	PS5DS_MAP_BTN(15,		KEY_MICMUTE),
	PS5DS_MAP_GCB(HAT_SWITCH,	hgame_hat_switch_cb),
	PS5DS_FINALCB(			ps5ds_final_cb),
};

static const struct hid_device_id ps5ds_devs[] = {
	{ HID_BVP(BUS_USB, USB_VENDOR_SONY, 0x0ce6),
	  HID_TLC(HUP_GENERIC_DESKTOP, HUG_GAME_PAD) },
};

static int
ps5ds_final_cb(HIDMAP_CB_ARGS)
{
	struct evdev_dev *evdev = HIDMAP_CB_GET_EVDEV();

	if (HIDMAP_CB_GET_STATE() == HIDMAP_CB_IS_ATTACHING) {
		evdev_support_prop(evdev, INPUT_PROP_DIRECT);
		evdev_set_cdev_mode(evdev, UID_ROOT, GID_GAMES,
		    S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
	}

	/* Do not execute callback at interrupt handler and detach */
	return (ENOSYS);
}

static int
ps5ds_write(struct ps5ds_softc *sc)
{
	uint8_t buf[PS5DS_OUTPUT_REPORT2_SIZE];
	bool led_on;

	sx_assert(&sc->lock, SA_XLOCKED);

	memset(buf, 0, sizeof(buf));
	buf[0] = 0x02;
	led_on = sc->led_state != PS5DS_LED_OFF;
	*(struct ps5ds_out2 *)(buf + 1) = (struct ps5ds_out2) {
		.valid_flag1 = PS5DS_FLAG1_LIGHTBAR | PS5DS_FLAG1_PLAYER_LEDS,
		.player_leds = sc->player_leds < 0 ?
		    0 : ps5ds_player_leds[sc->player_leds],
		.led_color_r = led_on ? sc->led_color.r : 0,
		.led_color_g = led_on ? sc->led_color.g : 0,
		.led_color_b = led_on ? sc->led_color.b : 0,
	};

	return (hid_write(sc->hm.dev, buf, sizeof(buf)));
}

static int
ps5ds_blank(struct ps5ds_softc *sc)
{
	uint8_t buf[PS5DS_OUTPUT_REPORT2_SIZE];

	memset(buf, 0, sizeof(buf));
	buf[0] = 0x02;
	*(struct ps5ds_out2 *)(buf + 1) = (struct ps5ds_out2) {
		.valid_flag1 = PS5DS_FLAG1_LIGHTBAR | PS5DS_FLAG1_PLAYER_LEDS,
		.valid_flag2 = PS5DS_FLAG2_LIGHTBAR_SETUP,
		.lightbar_setup = PS5DS_LIGHTBAR_FADE_OUT,
	};

	return (hid_write(sc->hm.dev, buf, sizeof(buf)));
}

static int
ps5ds_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct ps5ds_softc *sc;
	int error, arg;

	if (oidp->oid_arg1 == NULL || oidp->oid_arg2 < 0 ||
	    oidp->oid_arg2 > PS5DS_SYSCTL_LAST)
		return (EINVAL);

	sc = oidp->oid_arg1;
	sx_xlock(&sc->lock);

	arg = *(int *)((char *)sc + oidp->oid_arg2);
	error = sysctl_handle_int(oidp, &arg, 0, req);

	if (error || !req->newptr)
		goto unlock;

	switch (oidp->oid_arg2) {
	case PS5DS_SYSCTL_LED_STATE:
		if (arg < 0 || arg >= PS5DS_LED_CNT)
			error = EINVAL;
		break;
	case PS5DS_SYSCTL_LED_COLOR_R:
	case PS5DS_SYSCTL_LED_COLOR_G:
	case PS5DS_SYSCTL_LED_COLOR_B:
		if (arg < 0 || arg > UINT8_MAX)
			error = EINVAL;
		break;
	case PS5DS_SYSCTL_PLAYER_LEDS:
		if (arg < -1 || arg >= (int)nitems(ps5ds_player_leds))
			error = EINVAL;
		break;
	default:
		error = EINVAL;
	}

	if (error == 0) {
		*(int *)((char *)sc + oidp->oid_arg2) = arg;
		ps5ds_write(sc);
	}
unlock:
	sx_unlock(&sc->lock);

	return (error);
}

static void
ps5ds_identify(driver_t *driver, device_t parent)
{

	/* Replace the gamepad's rudimentary report descriptor */
	if (HIDBUS_LOOKUP_ID(parent, ps5ds_devs) != NULL)
		hid_set_report_descr(parent, ps5ds_rdesc,
		    sizeof(ps5ds_rdesc));
}

static int
ps5ds_probe(device_t dev)
{
	struct ps5ds_softc *sc = device_get_softc(dev);

	hidmap_set_debug_var(&sc->hm, &HID_DEBUG_VAR);
	return (
	    HIDMAP_PROBE(&sc->hm, dev, ps5ds_devs, ps5ds_map, NULL)
	);
}

static int
ps5ds_attach(device_t dev)
{
	struct ps5ds_softc *sc = device_get_softc(dev);
	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(dev);
	struct sysctl_oid *tree = device_get_sysctl_tree(dev);
	int error, unit;

	sx_init(&sc->lock, "ps5dsense");

	unit = device_get_unit(dev);
	sc->led_state = PS5DS_LED_ON;
	sc->led_color = ps5ds_leds[unit % nitems(ps5ds_leds)];
	sc->player_leds = unit < (int)nitems(ps5ds_player_leds) ? unit : -1;

	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "led_state", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_ANYBODY, sc,
	    PS5DS_SYSCTL_LED_STATE, ps5ds_sysctl, "I",
	    "LED state: 0 - off, 1 - on.");

	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "led_color_r", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_ANYBODY, sc,
	    PS5DS_SYSCTL_LED_COLOR_R, ps5ds_sysctl, "I",
	    "Lightbar color. Red component.");

	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "led_color_g", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_ANYBODY, sc,
	    PS5DS_SYSCTL_LED_COLOR_G, ps5ds_sysctl, "I",
	    "Lightbar color. Green component.");

	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "led_color_b", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_ANYBODY, sc,
	    PS5DS_SYSCTL_LED_COLOR_B, ps5ds_sysctl, "I",
	    "Lightbar color. Blue component.");

	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "player_leds", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_ANYBODY, sc,
	    PS5DS_SYSCTL_PLAYER_LEDS, ps5ds_sysctl, "I",
	    "Player indicator LEDs: -1 - off, 0-4 - player number.");

	error = hidmap_attach(&sc->hm);
	if (error) {
		sx_destroy(&sc->lock);
		return (error);
	}

	sx_xlock(&sc->lock);
	ps5ds_write(sc);
	sx_xunlock(&sc->lock);

	return (0);
}

static int
ps5ds_detach(device_t dev)
{
	struct ps5ds_softc *sc = device_get_softc(dev);
	int error;

	error = hidmap_detach(&sc->hm);
	if (error)
		return (error);

	ps5ds_blank(sc);
	sx_destroy(&sc->lock);

	return (0);
}

static device_method_t ps5ds_methods[] = {
	DEVMETHOD(device_identify,	ps5ds_identify),
	DEVMETHOD(device_probe,		ps5ds_probe),
	DEVMETHOD(device_attach,	ps5ds_attach),
	DEVMETHOD(device_detach,	ps5ds_detach),

	DEVMETHOD_END
};

DEFINE_CLASS_0(ps5dsense, ps5ds_driver, ps5ds_methods,
    sizeof(struct ps5ds_softc));
DRIVER_MODULE(ps5dsense, hidbus, ps5ds_driver, NULL, NULL);

MODULE_DEPEND(ps5dsense, hid, 1, 1, 1);
MODULE_DEPEND(ps5dsense, hidbus, 1, 1, 1);
MODULE_DEPEND(ps5dsense, hidmap, 1, 1, 1);
MODULE_DEPEND(ps5dsense, hgame, 1, 1, 1);
MODULE_DEPEND(ps5dsense, evdev, 1, 1, 1);
MODULE_VERSION(ps5dsense, 1);
HID_PNP_INFO(ps5ds_devs);
