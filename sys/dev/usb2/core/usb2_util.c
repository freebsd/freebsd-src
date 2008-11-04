/* $FreeBSD$ */
/*-
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

#include <dev/usb2/include/usb2_defs.h>
#include <dev/usb2/include/usb2_mfunc.h>
#include <dev/usb2/include/usb2_error.h>
#include <dev/usb2/include/usb2_standard.h>

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_util.h>
#include <dev/usb2/core/usb2_process.h>
#include <dev/usb2/core/usb2_device.h>
#include <dev/usb2/core/usb2_request.h>
#include <dev/usb2/core/usb2_busdma.h>

#include <dev/usb2/controller/usb2_controller.h>
#include <dev/usb2/controller/usb2_bus.h>

/* function prototypes */
#if (USB_USE_CONDVAR == 0)
static int usb2_msleep(void *chan, struct mtx *mtx, int priority, const char *wmesg, int timo);

#endif

/*------------------------------------------------------------------------*
 * device_delete_all_children - delete all children of a device
 *------------------------------------------------------------------------*/
int
device_delete_all_children(device_t dev)
{
	device_t *devlist;
	int devcount;
	int error;

	error = device_get_children(dev, &devlist, &devcount);
	if (error == 0) {
		while (devcount-- > 0) {
			error = device_delete_child(dev, devlist[devcount]);
			if (error) {
				break;
			}
		}
		free(devlist, M_TEMP);
	}
	return (error);
}

/*------------------------------------------------------------------------*
 *	device_set_usb2_desc
 *
 * This function can be called at probe or attach to set the USB
 * device supplied textual description for the given device.
 *------------------------------------------------------------------------*/
void
device_set_usb2_desc(device_t dev)
{
	struct usb2_attach_arg *uaa;
	struct usb2_device *udev;
	struct usb2_interface *iface;
	char *temp_p;
	usb2_error_t err;

	if (dev == NULL) {
		/* should not happen */
		return;
	}
	uaa = device_get_ivars(dev);
	if (uaa == NULL) {
		/* can happend if called at the wrong time */
		return;
	}
	udev = uaa->device;
	iface = uaa->iface;

	if ((iface == NULL) ||
	    (iface->idesc == NULL) ||
	    (iface->idesc->iInterface == 0)) {
		err = USB_ERR_INVAL;
	} else {
		err = 0;
	}

	temp_p = (char *)udev->bus->scratch[0].data;

	if (!err) {
		/* try to get the interface string ! */
		err = usb2_req_get_string_any
		    (udev, NULL, temp_p,
		    sizeof(udev->bus->scratch), iface->idesc->iInterface);
	}
	if (err) {
		/* use default description */
		usb2_devinfo(udev, temp_p,
		    sizeof(udev->bus->scratch));
	}
	device_set_desc_copy(dev, temp_p);
	device_printf(dev, "<%s> on %s\n", temp_p,
	    device_get_nameunit(udev->bus->bdev));
	return;
}

/*------------------------------------------------------------------------*
 *	 usb2_pause_mtx - factored out code
 *
 * This function will delay the code by the passed number of
 * milliseconds. The passed mutex "mtx" will be dropped while waiting,
 * if "mtx" is not NULL. The number of milliseconds per second is 1024
 * for sake of optimisation.
 *------------------------------------------------------------------------*/
void
usb2_pause_mtx(struct mtx *mtx, uint32_t ms)
{
	if (cold) {
		ms = (ms + 1) * 1024;
		DELAY(ms);

	} else {

		ms = USB_MS_TO_TICKS(ms);
		/*
		 * Add one to the number of ticks so that we don't return
		 * too early!
		 */
		ms++;

		if (mtx != NULL)
			mtx_unlock(mtx);

		if (pause("USBWAIT", ms)) {
			/* ignore */
		}
		if (mtx != NULL)
			mtx_lock(mtx);
	}
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_printBCD
 *
 * This function will print the version number "bcd" to the string
 * pointed to by "p" having a maximum length of "p_len" bytes
 * including the terminating zero.
 *------------------------------------------------------------------------*/
void
usb2_printBCD(char *p, uint16_t p_len, uint16_t bcd)
{
	if (snprintf(p, p_len, "%x.%02x", bcd >> 8, bcd & 0xff)) {
		/* ignore any errors */
	}
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_trim_spaces
 *
 * This function removes spaces at the beginning and the end of the string
 * pointed to by the "p" argument.
 *------------------------------------------------------------------------*/
void
usb2_trim_spaces(char *p)
{
	char *q;
	char *e;

	if (p == NULL)
		return;
	q = e = p;
	while (*q == ' ')		/* skip leading spaces */
		q++;
	while ((*p = *q++))		/* copy string */
		if (*p++ != ' ')	/* remember last non-space */
			e = p;
	*e = 0;				/* kill trailing spaces */
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_get_devid
 *
 * This function returns the USB Vendor and Product ID like a 32-bit
 * unsigned integer.
 *------------------------------------------------------------------------*/
uint32_t
usb2_get_devid(device_t dev)
{
	struct usb2_attach_arg *uaa = device_get_ivars(dev);

	return ((uaa->info.idVendor << 16) | (uaa->info.idProduct));
}

/*------------------------------------------------------------------------*
 *	usb2_make_str_desc - convert an ASCII string into a UNICODE string
 *------------------------------------------------------------------------*/
uint8_t
usb2_make_str_desc(void *ptr, uint16_t max_len, const char *s)
{
	struct usb2_string_descriptor *p = ptr;
	uint8_t totlen;
	int j;

	if (max_len < 2) {
		/* invalid length */
		return (0);
	}
	max_len = ((max_len / 2) - 1);

	j = strlen(s);

	if (j < 0) {
		j = 0;
	}
	if (j > 126) {
		j = 126;
	}
	if (max_len > j) {
		max_len = j;
	}
	totlen = (max_len + 1) * 2;

	p->bLength = totlen;
	p->bDescriptorType = UDESC_STRING;

	while (max_len--) {
		USETW2(p->bString[max_len], 0, s[max_len]);
	}
	return (totlen);
}

#if (USB_USE_CONDVAR == 0)

/*------------------------------------------------------------------------*
 *	usb2_cv_init - wrapper function
 *------------------------------------------------------------------------*/
void
usb2_cv_init(struct cv *cv, const char *desc)
{
	cv_init(cv, desc);
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_cv_destroy - wrapper function
 *------------------------------------------------------------------------*/
void
usb2_cv_destroy(struct cv *cv)
{
	cv_destroy(cv);
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_cv_wait - wrapper function
 *------------------------------------------------------------------------*/
void
usb2_cv_wait(struct cv *cv, struct mtx *mtx)
{
	int err;

	err = usb2_msleep(cv, mtx, 0, cv_wmesg(cv), 0);
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_cv_wait_sig - wrapper function
 *------------------------------------------------------------------------*/
int
usb2_cv_wait_sig(struct cv *cv, struct mtx *mtx)
{
	int err;

	err = usb2_msleep(cv, mtx, PCATCH, cv_wmesg(cv), 0);
	return (err);
}

/*------------------------------------------------------------------------*
 *	usb2_cv_timedwait - wrapper function
 *------------------------------------------------------------------------*/
int
usb2_cv_timedwait(struct cv *cv, struct mtx *mtx, int timo)
{
	int err;

	if (timo == 0)
		timo = 1;		/* zero means no timeout */
	err = usb2_msleep(cv, mtx, 0, cv_wmesg(cv), timo);
	return (err);
}

/*------------------------------------------------------------------------*
 *	usb2_cv_signal - wrapper function
 *------------------------------------------------------------------------*/
void
usb2_cv_signal(struct cv *cv)
{
	wakeup_one(cv);
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_cv_broadcast - wrapper function
 *------------------------------------------------------------------------*/
void
usb2_cv_broadcast(struct cv *cv)
{
	wakeup(cv);
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_msleep - wrapper function
 *------------------------------------------------------------------------*/
static int
usb2_msleep(void *chan, struct mtx *mtx, int priority, const char *wmesg,
    int timo)
{
	int err;

	if (mtx == &Giant) {
		err = tsleep(chan, priority, wmesg, timo);
	} else {
#ifdef mtx_sleep
		err = mtx_sleep(chan, mtx, priority, wmesg, timo);
#else
		err = msleep(chan, mtx, priority, wmesg, timo);
#endif
	}
	return (err);
}

#endif
