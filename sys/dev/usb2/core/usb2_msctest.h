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

#ifndef _USB2_MSCTEST_H_
#define	_USB2_MSCTEST_H_

usb2_error_t usb2_test_autoinstall(struct usb2_device *udev, uint8_t iface_index, uint8_t do_eject);
usb2_error_t usb2_test_huawei(struct usb2_device *udev, struct usb2_attach_arg *uaa);
int	usb2_lookup_huawei(struct usb2_attach_arg *uaa);

/* Huawei specific defines */

#define	U3GINFO(flag,speed) ((flag)|((speed) * 256))
#define	U3G_GET_SPEED(uaa) (USB_GET_DRIVER_INFO(uaa) / 256)

#define	U3GFL_NONE		0x00
#define	U3GFL_HUAWEI_INIT	0x01	/* Requires init command (Huawei
					 * cards) */
#define	U3GFL_SCSI_EJECT	0x02	/* Requires SCSI eject command
					 * (Novatel) */
#define	U3GFL_SIERRA_INIT	0x04	/* Requires init command (Sierra
					 * cards) */

#define	U3GSP_GPRS		0
#define	U3GSP_EDGE		1
#define	U3GSP_CDMA		2
#define	U3GSP_UMTS		3
#define	U3GSP_HSDPA		4
#define	U3GSP_HSUPA		5
#define	U3GSP_HSPA		6
#define	U3GSP_MAX		7

#endif					/* _USB2_MSCTEST_H_ */
