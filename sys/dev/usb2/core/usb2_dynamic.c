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
#include <dev/usb2/core/usb2_process.h>
#include <dev/usb2/core/usb2_device.h>
#include <dev/usb2/core/usb2_dynamic.h>

/* function prototypes */
static usb2_temp_get_desc_t usb2_temp_get_desc_w;
static usb2_temp_setup_by_index_t usb2_temp_setup_by_index_w;
static usb2_temp_unsetup_t usb2_temp_unsetup_w;
static usb2_test_quirk_t usb2_test_quirk_w;
static usb2_quirk_ioctl_t usb2_quirk_ioctl_w;

/* global variables */
usb2_temp_get_desc_t *usb2_temp_get_desc_p = &usb2_temp_get_desc_w;
usb2_temp_setup_by_index_t *usb2_temp_setup_by_index_p = &usb2_temp_setup_by_index_w;
usb2_temp_unsetup_t *usb2_temp_unsetup_p = &usb2_temp_unsetup_w;
usb2_test_quirk_t *usb2_test_quirk_p = &usb2_test_quirk_w;
usb2_quirk_ioctl_t *usb2_quirk_ioctl_p = &usb2_quirk_ioctl_w;
devclass_t usb2_devclass_ptr = NULL;

static usb2_error_t
usb2_temp_setup_by_index_w(struct usb2_device *udev, uint16_t index)
{
	return (USB_ERR_INVAL);
}

static uint8_t
usb2_test_quirk_w(const struct usb2_lookup_info *info, uint16_t quirk)
{
	return (0);			/* no match */
}

static int
usb2_quirk_ioctl_w(unsigned long cmd, caddr_t data, int fflag, struct thread *td)
{
	return (ENOIOCTL);
}

static void
usb2_temp_get_desc_w(struct usb2_device *udev, struct usb2_device_request *req, const void **pPtr, uint16_t *pLength)
{
	/* stall */
	*pPtr = NULL;
	*pLength = 0;
	return;
}

static void
usb2_temp_unsetup_w(struct usb2_device *udev)
{
	if (udev->usb2_template_ptr) {

		free(udev->usb2_template_ptr, M_USB);

		udev->usb2_template_ptr = NULL;
	}
	return;
}

void
usb2_quirk_unload(void *arg)
{
	/* reset function pointers */

	usb2_test_quirk_p = &usb2_test_quirk_w;
	usb2_quirk_ioctl_p = &usb2_quirk_ioctl_w;

	/* wait for CPU to exit the loaded functions, if any */

	/* XXX this is a tradeoff */

	pause("WAIT", hz);

	return;
}

void
usb2_temp_unload(void *arg)
{
	/* reset function pointers */

	usb2_temp_get_desc_p = &usb2_temp_get_desc_w;
	usb2_temp_setup_by_index_p = &usb2_temp_setup_by_index_w;
	usb2_temp_unsetup_p = &usb2_temp_unsetup_w;

	/* wait for CPU to exit the loaded functions, if any */

	/* XXX this is a tradeoff */

	pause("WAIT", hz);

	return;
}

void
usb2_bus_unload(void *arg)
{
	/* reset function pointers */

	usb2_devclass_ptr = NULL;

	/* wait for CPU to exit the loaded functions, if any */

	/* XXX this is a tradeoff */

	pause("WAIT", hz);

	return;
}
