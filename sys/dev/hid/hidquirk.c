/* $FreeBSD$ */
/*-
 * SPDX-License-Identifier: BSD-2-Clause-NetBSD
 *
 * Copyright (c) 1998 The NetBSD Foundation, Inc. All rights reserved.
 * Copyright (c) 1998 Lennart Augustsson. All rights reserved.
 * Copyright (c) 2008 Hans Petter Selasky. All rights reserved.
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

#include <sys/stdint.h>
#include <sys/stddef.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/sysctl.h>
#include <sys/sx.h>
#include <sys/unistd.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/priv.h>

#include <dev/evdev/input.h>

#define	HID_DEBUG_VAR hid_debug
#include <dev/hid/hid.h>
#include <dev/hid/hidquirk.h>
#include "usbdevs.h"


MODULE_DEPEND(hidquirk, hid, 1, 1, 1);
MODULE_VERSION(hidquirk, 1);

#define	HID_DEV_QUIRKS_MAX 384
#define	HID_SUB_QUIRKS_MAX 8
#define	HID_QUIRK_ENVROOT "hw.hid.quirk."

struct hidquirk_entry {
	uint16_t bus;
	uint16_t vid;
	uint16_t pid;
	uint16_t lo_rev;
	uint16_t hi_rev;
	uint16_t quirks[HID_SUB_QUIRKS_MAX];
};

static struct mtx hidquirk_mtx;

#define	HID_QUIRK_VP(b,v,p,l,h,...) \
  { .bus = (b), .vid = (v), .pid = (p), .lo_rev = (l), .hi_rev = (h), \
    .quirks = { __VA_ARGS__ } }
#define	USB_QUIRK(v,p,l,h,...) \
  HID_QUIRK_VP(BUS_USB, USB_VENDOR_##v, USB_PRODUCT_##v##_##p, l, h, __VA_ARGS__)

static struct hidquirk_entry hidquirks[HID_DEV_QUIRKS_MAX] = {
	USB_QUIRK(ASUS, LCM, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(QTRONIX, 980N, 0x110, 0x110, HQ_SPUR_BUT_UP),
	USB_QUIRK(ALCOR2, KBD_HUB, 0x001, 0x001, HQ_SPUR_BUT_UP),
	USB_QUIRK(LOGITECH, G510S, 0x0000, 0xFFFF, HQ_KBD_BOOTPROTO),
	/* Devices which should be ignored by usbhid */
	USB_QUIRK(APC, UPS, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(BELKIN, F6H375USB, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(BELKIN, F6C550AVR, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(BELKIN, F6C1250TWRK, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(BELKIN, F6C1500TWRK, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(BELKIN, F6C900UNV, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(BELKIN, F6C100UNV, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(BELKIN, F6C120UNV, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(BELKIN, F6C800UNV, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(BELKIN, F6C1100UNV, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(CYBERPOWER, BC900D, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(CYBERPOWER, 1500CAVRLCD, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(CYBERPOWER, OR2200LCDRM2U, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(DELL2, VARIOUS_UPS, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(CYPRESS, SILVERSHIELD, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(DELORME, EARTHMATE, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(DREAMLINK, DL100B, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(MICROCHIP, PICOLCD20X2, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(MICROCHIP, PICOLCD4X20, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(LIEBERT, POWERSURE_PXT, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(LIEBERT2, PSI1000, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(LIEBERT2, POWERSURE_PSA, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(MGE, UPS1, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(MGE, UPS2, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(POWERCOM, IMPERIAL_SERIES, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(POWERCOM, SMART_KING_PRO, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(POWERCOM, WOW, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(POWERCOM, VANGUARD, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(POWERCOM, BLACK_KNIGHT_PRO, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(TRIPPLITE2, AVR550U, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(TRIPPLITE2, AVR750U, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(TRIPPLITE2, ECO550UPS, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(TRIPPLITE2, T750_INTL, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(TRIPPLITE2, RT_2200_INTL, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(TRIPPLITE2, OMNI1000LCD, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(TRIPPLITE2, OMNI900LCD, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(TRIPPLITE2, SMART_2200RMXL2U, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(TRIPPLITE2, UPS_3014, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(TRIPPLITE2, SU1500RTXL2UA, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(TRIPPLITE2, SU6000RT4U, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(TRIPPLITE2, SU1500RTXL2UA_2, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(APPLE, IPHONE, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(APPLE, IPHONE_3G, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(MEGATEC, UPS, 0x0000, 0xffff, HQ_HID_IGNORE),
	/* Devices which should be ignored by both ukbd and uhid */
	USB_QUIRK(CYPRESS, WISPY1A, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(METAGEEK, WISPY1B, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(METAGEEK, WISPY24X, 0x0000, 0xffff, HQ_HID_IGNORE),
	USB_QUIRK(METAGEEK2, WISPYDBX, 0x0000, 0xffff, HQ_HID_IGNORE),
	/* MS keyboards do weird things */
	USB_QUIRK(MICROSOFT, NATURAL4000, 0x0000, 0xFFFF, HQ_KBD_BOOTPROTO),
	USB_QUIRK(MICROSOFT, WLINTELLIMOUSE, 0x0000, 0xffff, HQ_MS_LEADING_BYTE),
	/* Quirk for Corsair Vengeance K60 keyboard */
	USB_QUIRK(CORSAIR, K60, 0x0000, 0xffff, HQ_KBD_BOOTPROTO),
	/* Quirk for Corsair Gaming K68 keyboard */
	USB_QUIRK(CORSAIR, K68, 0x0000, 0xffff, HQ_KBD_BOOTPROTO),
	/* Quirk for Corsair Vengeance K70 keyboard */
	USB_QUIRK(CORSAIR, K70, 0x0000, 0xffff, HQ_KBD_BOOTPROTO),
	/* Quirk for Corsair K70 RGB keyboard */
	USB_QUIRK(CORSAIR, K70_RGB, 0x0000, 0xffff, HQ_KBD_BOOTPROTO),
	/* Quirk for Corsair STRAFE Gaming keyboard */
	USB_QUIRK(CORSAIR, STRAFE, 0x0000, 0xffff, HQ_KBD_BOOTPROTO),
	USB_QUIRK(CORSAIR, STRAFE2, 0x0000, 0xffff, HQ_KBD_BOOTPROTO),
	/* Holtek USB gaming keyboard */
	USB_QUIRK(HOLTEK, F85, 0x0000, 0xffff, HQ_KBD_BOOTPROTO),
	/* Quirk for Kensington Slimblade Trackball */
	USB_QUIRK(KENSINGTON, SLIMBLADE, 0x0000, 0xffff, HQ_MS_VENDOR_BTN),
};
#undef HID_QUIRK_VP
#undef USB_QUIRK

/* hidquirk.h exposes only HID_QUIRK_LIST macro when HQ() is defined */
#define	HQ(x)	[HQ_##x] = "HQ_"#x
#include "hidquirk.h"
static const char *hidquirk_str[HID_QUIRK_MAX] = { HID_QUIRK_LIST() };
#undef HQ

static hid_test_quirk_t hid_test_quirk_by_info;

/*------------------------------------------------------------------------*
 *	hidquirkstr
 *
 * This function converts an USB quirk code into a string.
 *------------------------------------------------------------------------*/
static const char *
hidquirkstr(uint16_t quirk)
{
	return ((quirk < HID_QUIRK_MAX && hidquirk_str[quirk] != NULL) ?
	    hidquirk_str[quirk] : "HQ_UNKNOWN");
}

/*------------------------------------------------------------------------*
 *	hid_strquirk
 *
 * This function converts a string into a HID quirk code.
 *
 * Returns:
 * Less than HID_QUIRK_MAX: Quirk code
 * Else: Quirk code not found
 *------------------------------------------------------------------------*/
static uint16_t
hid_strquirk(const char *str, size_t len)
{
	const char *quirk;
	uint16_t x;

	for (x = 0; x != HID_QUIRK_MAX; x++) {
		quirk = hidquirkstr(x);
		if (strncmp(str, quirk, len) == 0 &&
		    quirk[len] == 0)
			break;
	}
	return (x);
}

/*------------------------------------------------------------------------*
 *	hid_test_quirk_by_info
 *
 * Returns:
 * false: Quirk not found
 * true: Quirk found
 *------------------------------------------------------------------------*/
bool
hid_test_quirk_by_info(const struct hid_device_info *info, uint16_t quirk)
{
	uint16_t x;
	uint16_t y;

	if (quirk == HQ_NONE)
		goto done;

	mtx_lock(&hidquirk_mtx);

	for (x = 0; x != HID_DEV_QUIRKS_MAX; x++) {
		/* see if quirk information does not match */
		if ((hidquirks[x].bus != info->idBus) ||
		    (hidquirks[x].vid != info->idVendor) ||
		    (hidquirks[x].lo_rev > info->idVersion) ||
		    (hidquirks[x].hi_rev < info->idVersion)) {
			continue;
		}
		/* see if quirk only should match vendor ID */
		if (hidquirks[x].pid != info->idProduct) {
			if (hidquirks[x].pid != 0)
				continue;

			for (y = 0; y != HID_SUB_QUIRKS_MAX; y++) {
				if (hidquirks[x].quirks[y] == HQ_MATCH_VENDOR_ONLY)
					break;
			}
			if (y == HID_SUB_QUIRKS_MAX)
				continue;
		}
		/* lookup quirk */
		for (y = 0; y != HID_SUB_QUIRKS_MAX; y++) {
			if (hidquirks[x].quirks[y] == quirk) {
				mtx_unlock(&hidquirk_mtx);
				DPRINTF("Found quirk '%s'.\n", hidquirkstr(quirk));
				return (true);
			}
		}
	}
	mtx_unlock(&hidquirk_mtx);
done:
	return (false);			/* no quirk match */
}

static struct hidquirk_entry *
hidquirk_get_entry(uint16_t bus, uint16_t vid, uint16_t pid,
    uint16_t lo_rev, uint16_t hi_rev, uint8_t do_alloc)
{
	uint16_t x;

	mtx_assert(&hidquirk_mtx, MA_OWNED);

	if ((bus | vid | pid | lo_rev | hi_rev) == 0) {
		/* all zero - special case */
		return (hidquirks + HID_DEV_QUIRKS_MAX - 1);
	}
	/* search for an existing entry */
	for (x = 0; x != HID_DEV_QUIRKS_MAX; x++) {
		/* see if quirk information does not match */
		if ((hidquirks[x].bus != bus) ||
		    (hidquirks[x].vid != vid) ||
		    (hidquirks[x].pid != pid) ||
		    (hidquirks[x].lo_rev != lo_rev) ||
		    (hidquirks[x].hi_rev != hi_rev)) {
			continue;
		}
		return (hidquirks + x);
	}

	if (do_alloc == 0) {
		/* no match */
		return (NULL);
	}
	/* search for a free entry */
	for (x = 0; x != HID_DEV_QUIRKS_MAX; x++) {
		/* see if quirk information does not match */
		if ((hidquirks[x].bus |
		    hidquirks[x].vid |
		    hidquirks[x].pid |
		    hidquirks[x].lo_rev |
		    hidquirks[x].hi_rev) != 0) {
			continue;
		}
		hidquirks[x].bus = bus;
		hidquirks[x].vid = vid;
		hidquirks[x].pid = pid;
		hidquirks[x].lo_rev = lo_rev;
		hidquirks[x].hi_rev = hi_rev;

		return (hidquirks + x);
	}

	/* no entry found */
	return (NULL);
}

/*------------------------------------------------------------------------*
 *	usb_quirk_strtou16
 *
 * Helper function to scan a 16-bit integer.
 *------------------------------------------------------------------------*/
static uint16_t
hidquirk_strtou16(const char **pptr, const char *name, const char *what)
{
	unsigned long value;
	char *end;

	value = strtoul(*pptr, &end, 0);
	if (value > 65535 || *pptr == end || (*end != ' ' && *end != '\t')) {
		printf("%s: %s 16-bit %s value set to zero\n",
		    name, what, *end == 0 ? "incomplete" : "invalid");
		return (0);
	}
	*pptr = end + 1;
	return ((uint16_t)value);
}

/*------------------------------------------------------------------------*
 *	usb_quirk_add_entry_from_str
 *
 * Add a USB quirk entry from string.
 *     "VENDOR PRODUCT LO_REV HI_REV QUIRK[,QUIRK[,...]]"
 *------------------------------------------------------------------------*/
static void
hidquirk_add_entry_from_str(const char *name, const char *env)
{
	struct hidquirk_entry entry = { };
	struct hidquirk_entry *new;
	uint16_t quirk_idx;
	uint16_t quirk;
	const char *end;

	/* check for invalid environment variable */
	if (name == NULL || env == NULL)
		return;

	if (bootverbose)
		printf("Adding HID QUIRK '%s' = '%s'\n", name, env);

	/* parse device information */
	entry.bus = hidquirk_strtou16(&env, name, "Bus ID");
	entry.vid = hidquirk_strtou16(&env, name, "Vendor ID");
	entry.pid = hidquirk_strtou16(&env, name, "Product ID");
	entry.lo_rev = hidquirk_strtou16(&env, name, "Low revision");
	entry.hi_rev = hidquirk_strtou16(&env, name, "High revision");

	/* parse quirk information */
	quirk_idx = 0;
	while (*env != 0 && quirk_idx != HID_SUB_QUIRKS_MAX) {
		/* skip whitespace before quirks */
		while (*env == ' ' || *env == '\t')
			env++;

		/* look for quirk separation character */
		end = strchr(env, ',');
		if (end == NULL)
			end = env + strlen(env);

		/* lookup quirk in string table */
		quirk = hid_strquirk(env, end - env);
		if (quirk < HID_QUIRK_MAX) {
			entry.quirks[quirk_idx++] = quirk;
		} else {
			printf("%s: unknown HID quirk '%.*s' (skipped)\n",
			    name, (int)(end - env), env);
		}
		env = end;

		/* skip quirk delimiter, if any */
		if (*env != 0)
			env++;
	}

	/* register quirk */
	if (quirk_idx != 0) {
		if (*env != 0) {
			printf("%s: Too many HID quirks, only %d allowed!\n",
			    name, HID_SUB_QUIRKS_MAX);
		}
		mtx_lock(&hidquirk_mtx);
		new = hidquirk_get_entry(entry.bus, entry.vid, entry.pid,
		    entry.lo_rev, entry.hi_rev, 1);
		if (new == NULL)
			printf("%s: HID quirks table is full!\n", name);
		else
			memcpy(new->quirks, entry.quirks, sizeof(entry.quirks));
		mtx_unlock(&hidquirk_mtx);
	} else {
		printf("%s: No USB quirks found!\n", name);
	}
}

static void
hidquirk_init(void *arg)
{
	char envkey[sizeof(HID_QUIRK_ENVROOT) + 2];	/* 2 digits max, 0 to 99 */
	int i;

	/* initialize mutex */
	mtx_init(&hidquirk_mtx, "HID quirk", NULL, MTX_DEF);

	/* look for quirks defined by the environment variable */
	for (i = 0; i != 100; i++) {
		snprintf(envkey, sizeof(envkey), HID_QUIRK_ENVROOT "%d", i);

		/* Stop at first undefined var */
		if (!testenv(envkey))
			break;

		/* parse environment variable */
		hidquirk_add_entry_from_str(envkey, kern_getenv(envkey));
	}

	/* register our function */
	hid_test_quirk_p = &hid_test_quirk_by_info;
}

static void
hidquirk_uninit(void *arg)
{
	hid_quirk_unload(arg);

	/* destroy mutex */
	mtx_destroy(&hidquirk_mtx);
}

SYSINIT(hidquirk_init, SI_SUB_LOCK, SI_ORDER_FIRST, hidquirk_init, NULL);
SYSUNINIT(hidquirk_uninit, SI_SUB_LOCK, SI_ORDER_ANY, hidquirk_uninit, NULL);
