/* $FreeBSD$ */
/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc. All rights reserved.
 * Copyright (c) 1998 Lennart Augustsson. All rights reserved.
 * Copyright (c) 2008 Hans Petter Selasky. All rights reserved.
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

#include <dev/usb2/include/usb2_standard.h>
#include <dev/usb2/include/usb2_ioctl.h>
#include <dev/usb2/include/usb2_mfunc.h>
#include <dev/usb2/include/usb2_devid.h>

#define	USB_DEBUG_VAR usb2_debug

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_lookup.h>
#include <dev/usb2/core/usb2_debug.h>
#include <dev/usb2/core/usb2_dynamic.h>

#include <dev/usb2/quirk/usb2_quirk.h>

MODULE_DEPEND(usb2_quirk, usb2_core, 1, 1, 1);
MODULE_VERSION(usb2_quirk, 1);

/*
 * The following macro adds one or more quirks for a USB device:
 */
#define	USB_QUIRK_ENTRY(v,p,l,h,...) \
  .vid = (v), .pid = (p), .lo_rev = (l), .hi_rev = (h), .quirks = { __VA_ARGS__ }

#define	USB_DEV_QUIRKS_MAX 128
#define	USB_SUB_QUIRKS_MAX 8

struct usb2_quirk_entry {
	uint16_t vid;
	uint16_t pid;
	uint16_t lo_rev;
	uint16_t hi_rev;
	uint16_t quirks[USB_SUB_QUIRKS_MAX];
};

static struct mtx usb2_quirk_mtx;

static struct usb2_quirk_entry usb2_quirks[USB_DEV_QUIRKS_MAX] = {
	{USB_QUIRK_ENTRY(USB_VENDOR_ASUS, USB_PRODUCT_ASUS_LCM, 0x0000, 0xFFFF, UQ_HID_IGNORE, UQ_NONE)},
	{USB_QUIRK_ENTRY(USB_VENDOR_INSIDEOUT, USB_PRODUCT_INSIDEOUT_EDGEPORT4, 0x094, 0x094, UQ_SWAP_UNICODE, UQ_NONE)},
	{USB_QUIRK_ENTRY(USB_VENDOR_DALLAS, USB_PRODUCT_DALLAS_J6502, 0x0a2, 0x0a2, UQ_BAD_ADC, UQ_NONE)},
	{USB_QUIRK_ENTRY(USB_VENDOR_DALLAS, USB_PRODUCT_DALLAS_J6502, 0x0a2, 0x0a2, UQ_AU_NO_XU, UQ_NONE)},
	{USB_QUIRK_ENTRY(USB_VENDOR_ALTEC, USB_PRODUCT_ALTEC_ADA70, 0x103, 0x103, UQ_BAD_ADC, UQ_NONE)},
	{USB_QUIRK_ENTRY(USB_VENDOR_ALTEC, USB_PRODUCT_ALTEC_ASC495, 0x000, 0x000, UQ_BAD_AUDIO, UQ_NONE)},
	{USB_QUIRK_ENTRY(USB_VENDOR_QTRONIX, USB_PRODUCT_QTRONIX_980N, 0x110, 0x110, UQ_SPUR_BUT_UP, UQ_NONE)},
	{USB_QUIRK_ENTRY(USB_VENDOR_ALCOR2, USB_PRODUCT_ALCOR2_KBD_HUB, 0x001, 0x001, UQ_SPUR_BUT_UP, UQ_NONE)},
	{USB_QUIRK_ENTRY(USB_VENDOR_MCT, USB_PRODUCT_MCT_HUB0100, 0x102, 0x102, UQ_BUS_POWERED, UQ_NONE)},
	{USB_QUIRK_ENTRY(USB_VENDOR_MCT, USB_PRODUCT_MCT_USB232, 0x102, 0x102, UQ_BUS_POWERED, UQ_NONE)},
	{USB_QUIRK_ENTRY(USB_VENDOR_TI, USB_PRODUCT_TI_UTUSB41, 0x110, 0x110, UQ_POWER_CLAIM, UQ_NONE)},
	{USB_QUIRK_ENTRY(USB_VENDOR_TELEX, USB_PRODUCT_TELEX_MIC1, 0x009, 0x009, UQ_AU_NO_FRAC, UQ_NONE)},
	{USB_QUIRK_ENTRY(USB_VENDOR_SILICONPORTALS, USB_PRODUCT_SILICONPORTALS_YAPPHONE, 0x100, 0x100, UQ_AU_INP_ASYNC, UQ_NONE)},
	{USB_QUIRK_ENTRY(USB_VENDOR_LOGITECH, USB_PRODUCT_LOGITECH_UN53B, 0x0000, 0xFFFF, UQ_NO_STRINGS, UQ_NONE)},

	/*
	 * XXX The following quirks should have a more specific revision
	 * number:
	 */
	{USB_QUIRK_ENTRY(USB_VENDOR_HP, USB_PRODUCT_HP_895C, 0x0000, 0xFFFF, UQ_BROKEN_BIDIR, UQ_NONE)},
	{USB_QUIRK_ENTRY(USB_VENDOR_HP, USB_PRODUCT_HP_880C, 0x0000, 0xFFFF, UQ_BROKEN_BIDIR, UQ_NONE)},
	{USB_QUIRK_ENTRY(USB_VENDOR_HP, USB_PRODUCT_HP_815C, 0x0000, 0xFFFF, UQ_BROKEN_BIDIR, UQ_NONE)},
	{USB_QUIRK_ENTRY(USB_VENDOR_HP, USB_PRODUCT_HP_810C, 0x0000, 0xFFFF, UQ_BROKEN_BIDIR, UQ_NONE)},
	{USB_QUIRK_ENTRY(USB_VENDOR_HP, USB_PRODUCT_HP_830C, 0x0000, 0xFFFF, UQ_BROKEN_BIDIR, UQ_NONE)},
	{USB_QUIRK_ENTRY(USB_VENDOR_HP, USB_PRODUCT_HP_1220C, 0x0000, 0xFFFF, UQ_BROKEN_BIDIR, UQ_NONE)},
	{USB_QUIRK_ENTRY(USB_VENDOR_XEROX, USB_PRODUCT_XEROX_WCM15, 0x0000, 0xFFFF, UQ_BROKEN_BIDIR, UQ_NONE)},
	/* Devices which should be ignored by uhid */
	{USB_QUIRK_ENTRY(USB_VENDOR_APC, USB_PRODUCT_APC_UPS, 0x0000, 0xFFFF, UQ_HID_IGNORE, UQ_NONE)},
	{USB_QUIRK_ENTRY(USB_VENDOR_BELKIN, USB_PRODUCT_BELKIN_F6C550AVR, 0x0000, 0xFFFF, UQ_HID_IGNORE, UQ_NONE)},
	{USB_QUIRK_ENTRY(USB_VENDOR_CYBERPOWER, USB_PRODUCT_CYBERPOWER_1500CAVRLCD, 0x0000, 0xFFFF, UQ_HID_IGNORE, UQ_NONE)},
	{USB_QUIRK_ENTRY(USB_VENDOR_DELORME, USB_PRODUCT_DELORME_EARTHMATE, 0x0000, 0xFFFF, UQ_HID_IGNORE, UQ_NONE)},
	{USB_QUIRK_ENTRY(USB_VENDOR_ITUNERNET, USB_PRODUCT_ITUNERNET_USBLCD2X20, 0x0000, 0xFFFF, UQ_HID_IGNORE, UQ_NONE)},
	{USB_QUIRK_ENTRY(USB_VENDOR_MGE, USB_PRODUCT_MGE_UPS1, 0x0000, 0xFFFF, UQ_HID_IGNORE, UQ_NONE)},
	{USB_QUIRK_ENTRY(USB_VENDOR_MGE, USB_PRODUCT_MGE_UPS2, 0x0000, 0xFFFF, UQ_HID_IGNORE, UQ_NONE)},
	{USB_QUIRK_ENTRY(USB_VENDOR_APPLE, USB_PRODUCT_APPLE_IPHONE, 0x0000, 0xFFFF, UQ_HID_IGNORE, UQ_NONE)},
	{USB_QUIRK_ENTRY(USB_VENDOR_APPLE, USB_PRODUCT_APPLE_IPHONE_3G, 0x0000, 0xFFFF, UQ_HID_IGNORE, UQ_NONE)},
	/* Devices which should be ignored by both ukbd and uhid */
	{USB_QUIRK_ENTRY(USB_VENDOR_CYPRESS, USB_PRODUCT_CYPRESS_WISPY1A, 0x0000, 0xFFFF, UQ_KBD_IGNORE, UQ_HID_IGNORE, UQ_NONE)},
	{USB_QUIRK_ENTRY(USB_VENDOR_METAGEEK, USB_PRODUCT_METAGEEK_WISPY1B, 0x0000, 0xFFFF, UQ_KBD_IGNORE, UQ_HID_IGNORE, UQ_NONE)},
	{USB_QUIRK_ENTRY(USB_VENDOR_TENX, USB_PRODUCT_TENX_UAUDIO0, 0x0101, 0x0101, UQ_AUDIO_SWAP_LR, UQ_NONE)},
	/* MS keyboards do weird things */
	{USB_QUIRK_ENTRY(USB_VENDOR_MICROSOFT, USB_PRODUCT_MICROSOFT_WLNOTEBOOK, 0x0000, 0xFFFF, UQ_MS_BAD_CLASS, UQ_MS_LEADING_BYTE, UQ_NONE)},
	{USB_QUIRK_ENTRY(USB_VENDOR_MICROSOFT, USB_PRODUCT_MICROSOFT_WLNOTEBOOK2, 0x0000, 0xFFFF, UQ_MS_BAD_CLASS, UQ_MS_LEADING_BYTE, UQ_NONE)},
	{USB_QUIRK_ENTRY(USB_VENDOR_MICROSOFT, USB_PRODUCT_MICROSOFT_WLINTELLIMOUSE, 0x0000, 0xFFFF, UQ_MS_LEADING_BYTE, UQ_NONE)},
	{USB_QUIRK_ENTRY(USB_VENDOR_MICROSOFT, USB_PRODUCT_MICROSOFT_COMFORT3000, 0x0000, 0xFFFF, UQ_MS_BAD_CLASS, UQ_MS_LEADING_BYTE, UQ_NONE)},
	{USB_QUIRK_ENTRY(USB_VENDOR_METAGEEK, USB_PRODUCT_METAGEEK_WISPY24X, 0x0000, 0xFFFF, UQ_KBD_IGNORE, UQ_HID_IGNORE, UQ_NONE)},
};

USB_MAKE_DEBUG_TABLE(USB_QUIRK);

/*------------------------------------------------------------------------*
 *	usb2_quirkstr
 *
 * This function converts an USB quirk code into a string.
 *------------------------------------------------------------------------*/
static const char *
usb2_quirkstr(uint16_t quirk)
{
	return ((quirk < USB_QUIRK_MAX) ?
	    USB_QUIRK[quirk] : "USB_QUIRK_UNKNOWN");
}

/*------------------------------------------------------------------------*
 *	usb2_test_quirk_by_info
 *
 * Returns:
 * 0: Quirk not found
 * Else: Quirk found
 *------------------------------------------------------------------------*/
static uint8_t
usb2_test_quirk_by_info(const struct usb2_lookup_info *info, uint16_t quirk)
{
	uint16_t x;
	uint16_t y;

	if (quirk == UQ_NONE) {
		return (0);
	}
	mtx_lock(&usb2_quirk_mtx);

	for (x = 0; x != USB_DEV_QUIRKS_MAX; x++) {
		/* see if quirk information does not match */
		if ((usb2_quirks[x].vid != info->idVendor) ||
		    (usb2_quirks[x].pid != info->idProduct) ||
		    (usb2_quirks[x].lo_rev > info->bcdDevice) ||
		    (usb2_quirks[x].hi_rev < info->bcdDevice)) {
			continue;
		}
		/* lookup quirk */
		for (y = 0; y != USB_SUB_QUIRKS_MAX; y++) {
			if (usb2_quirks[x].quirks[y] == quirk) {
				mtx_unlock(&usb2_quirk_mtx);
				DPRINTF("Found quirk '%s'.\n", usb2_quirkstr(quirk));
				return (1);
			}
		}
		/* no quirk found */
		break;
	}
	mtx_unlock(&usb2_quirk_mtx);
	return (0);
}

static struct usb2_quirk_entry *
usb2_quirk_get_entry(uint16_t vid, uint16_t pid,
    uint16_t lo_rev, uint16_t hi_rev, uint8_t do_alloc)
{
	uint16_t x;

	mtx_assert(&usb2_quirk_mtx, MA_OWNED);

	if ((vid | pid | lo_rev | hi_rev) == 0) {
		/* all zero - special case */
		return (usb2_quirks + USB_DEV_QUIRKS_MAX - 1);
	}
	/* search for an existing entry */
	for (x = 0; x != USB_DEV_QUIRKS_MAX; x++) {
		/* see if quirk information does not match */
		if ((usb2_quirks[x].vid != vid) ||
		    (usb2_quirks[x].pid != pid) ||
		    (usb2_quirks[x].lo_rev != lo_rev) ||
		    (usb2_quirks[x].hi_rev != hi_rev)) {
			continue;
		}
		return (usb2_quirks + x);
	}

	if (do_alloc == 0) {
		/* no match */
		return (NULL);
	}
	/* search for a free entry */
	for (x = 0; x != USB_DEV_QUIRKS_MAX; x++) {
		/* see if quirk information does not match */
		if ((usb2_quirks[x].vid |
		    usb2_quirks[x].pid |
		    usb2_quirks[x].lo_rev |
		    usb2_quirks[x].hi_rev) != 0) {
			continue;
		}
		usb2_quirks[x].vid = vid;
		usb2_quirks[x].pid = pid;
		usb2_quirks[x].lo_rev = lo_rev;
		usb2_quirks[x].hi_rev = hi_rev;

		return (usb2_quirks + x);
	}

	/* no entry found */
	return (NULL);
}

/*------------------------------------------------------------------------*
 *	usb2_quirk_ioctl - handle quirk IOCTLs
 *
 * Returns:
 * 0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
static int
usb2_quirk_ioctl(unsigned long cmd, caddr_t data,
    int fflag, struct thread *td)
{
	struct usb2_gen_quirk *pgq;
	struct usb2_quirk_entry *pqe;
	uint32_t x;
	uint32_t y;
	int err;

	switch (cmd) {
	case USB_DEV_QUIRK_GET:
		pgq = (void *)data;
		x = pgq->index % USB_SUB_QUIRKS_MAX;
		y = pgq->index / USB_SUB_QUIRKS_MAX;
		if (y >= USB_DEV_QUIRKS_MAX) {
			return (EINVAL);
		}
		mtx_lock(&usb2_quirk_mtx);
		/* copy out data */
		pgq->vid = usb2_quirks[y].vid;
		pgq->pid = usb2_quirks[y].pid;
		pgq->bcdDeviceLow = usb2_quirks[y].lo_rev;
		pgq->bcdDeviceHigh = usb2_quirks[y].hi_rev;
		strlcpy(pgq->quirkname,
		    usb2_quirkstr(usb2_quirks[y].quirks[x]),
		    sizeof(pgq->quirkname));
		mtx_unlock(&usb2_quirk_mtx);
		return (0);		/* success */

	case USB_QUIRK_NAME_GET:
		pgq = (void *)data;
		x = pgq->index;
		if (x >= USB_QUIRK_MAX) {
			return (EINVAL);
		}
		strlcpy(pgq->quirkname,
		    usb2_quirkstr(x), sizeof(pgq->quirkname));
		return (0);		/* success */

	case USB_DEV_QUIRK_ADD:
		pgq = (void *)data;

		/* check privileges */
		err = priv_check(curthread, PRIV_DRIVER);
		if (err) {
			return (err);
		}
		/* convert quirk string into numerical */
		for (y = 0; y != USB_DEV_QUIRKS_MAX; y++) {
			if (strcmp(pgq->quirkname, usb2_quirkstr(y)) == 0) {
				break;
			}
		}
		if (y == USB_DEV_QUIRKS_MAX) {
			return (EINVAL);
		}
		if (y == UQ_NONE) {
			return (EINVAL);
		}
		mtx_lock(&usb2_quirk_mtx);
		pqe = usb2_quirk_get_entry(pgq->vid, pgq->pid,
		    pgq->bcdDeviceLow, pgq->bcdDeviceHigh, 1);
		for (x = 0; x != USB_SUB_QUIRKS_MAX; x++) {
			if (pqe->quirks[x] == UQ_NONE) {
				pqe->quirks[x] = y;
				break;
			}
		}
		mtx_unlock(&usb2_quirk_mtx);
		if (x == USB_SUB_QUIRKS_MAX) {
			return (ENOMEM);
		}
		return (0);		/* success */

	case USB_DEV_QUIRK_REMOVE:
		pgq = (void *)data;
		/* check privileges */
		err = priv_check(curthread, PRIV_DRIVER);
		if (err) {
			return (err);
		}
		/* convert quirk string into numerical */
		for (y = 0; y != USB_DEV_QUIRKS_MAX; y++) {
			if (strcmp(pgq->quirkname, usb2_quirkstr(y)) == 0) {
				break;
			}
		}
		if (y == USB_DEV_QUIRKS_MAX) {
			return (EINVAL);
		}
		if (y == UQ_NONE) {
			return (EINVAL);
		}
		mtx_lock(&usb2_quirk_mtx);
		pqe = usb2_quirk_get_entry(pgq->vid, pgq->pid,
		    pgq->bcdDeviceLow, pgq->bcdDeviceHigh, 0);
		for (x = 0; x != USB_SUB_QUIRKS_MAX; x++) {
			if (pqe->quirks[x] == y) {
				pqe->quirks[x] = UQ_NONE;
				break;
			}
		}
		if (x == USB_SUB_QUIRKS_MAX) {
			mtx_unlock(&usb2_quirk_mtx);
			return (ENOMEM);
		}
		for (x = 0; x != USB_SUB_QUIRKS_MAX; x++) {
			if (pqe->quirks[x] != UQ_NONE) {
				break;
			}
		}
		if (x == USB_SUB_QUIRKS_MAX) {
			/* all quirk entries are unused - release */
			memset(pqe, 0, sizeof(pqe));
		}
		mtx_unlock(&usb2_quirk_mtx);
		return (0);		/* success */

	default:
		break;
	}
	return (ENOIOCTL);
}

static void
usb2_quirk_init(void *arg)
{
	/* initialize mutex */
	mtx_init(&usb2_quirk_mtx, "USB quirk", NULL, MTX_DEF);

	/* register our function */
	usb2_test_quirk_p = &usb2_test_quirk_by_info;
	usb2_quirk_ioctl_p = &usb2_quirk_ioctl;
	return;
}

static void
usb2_quirk_uninit(void *arg)
{
	usb2_quirk_unload(arg);

	/* destroy mutex */
	mtx_destroy(&usb2_quirk_mtx);
	return;
}

SYSINIT(usb2_quirk_init, SI_SUB_LOCK, SI_ORDER_FIRST, usb2_quirk_init, NULL);
SYSUNINIT(usb2_quirk_uninit, SI_SUB_LOCK, SI_ORDER_ANY, usb2_quirk_uninit, NULL);
