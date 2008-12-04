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

#ifndef _USB2_DYNAMIC_H_
#define	_USB2_DYNAMIC_H_

/* prototypes */

struct usb2_device;
struct usb2_lookup_info;
struct usb2_device_request;

/* typedefs */

typedef usb2_error_t (usb2_temp_setup_by_index_t)(struct usb2_device *udev, uint16_t index);
typedef uint8_t (usb2_test_quirk_t)(const struct usb2_lookup_info *info, uint16_t quirk);
typedef int (usb2_quirk_ioctl_t)(unsigned long cmd, caddr_t data, int fflag, struct thread *td);
typedef void (usb2_temp_get_desc_t)(struct usb2_device *udev, struct usb2_device_request *req, const void **pPtr, uint16_t *pLength);
typedef void (usb2_temp_unsetup_t)(struct usb2_device *udev);

/* global function pointers */

extern usb2_temp_get_desc_t *usb2_temp_get_desc_p;
extern usb2_temp_setup_by_index_t *usb2_temp_setup_by_index_p;
extern usb2_temp_unsetup_t *usb2_temp_unsetup_p;
extern usb2_test_quirk_t *usb2_test_quirk_p;
extern usb2_quirk_ioctl_t *usb2_quirk_ioctl_p;
extern devclass_t usb2_devclass_ptr;

/* function prototypes */

void	usb2_temp_unload(void *);
void	usb2_quirk_unload(void *);
void	usb2_bus_unload(void *);

uint8_t	usb2_test_quirk(const struct usb2_attach_arg *uaa, uint16_t quirk);

#endif					/* _USB2_DYNAMIC_H_ */
