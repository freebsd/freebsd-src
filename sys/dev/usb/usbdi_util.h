/*	$NetBSD: usbdi_util.h,v 1.23 2001/10/26 17:58:22 augustss Exp $	*/
/*	$FreeBSD$	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

usbd_status	usbd_get_desc(usbd_device_handle dev, int type, 
			      int index, int len, void *desc);
usbd_status	usbd_get_config_desc(usbd_device_handle, int, 
				     usb_config_descriptor_t *);
usbd_status	usbd_get_config_desc_full(usbd_device_handle, int, void *, int);
usbd_status	usbd_get_device_desc(usbd_device_handle dev,
				     usb_device_descriptor_t *d);
usbd_status	usbd_set_address(usbd_device_handle dev, int addr);
usbd_status	usbd_get_port_status(usbd_device_handle, 
				     int, usb_port_status_t *);
usbd_status	usbd_set_hub_feature(usbd_device_handle dev, int);
usbd_status	usbd_clear_hub_feature(usbd_device_handle, int);
usbd_status	usbd_set_port_feature(usbd_device_handle dev, int, int);
usbd_status	usbd_clear_port_feature(usbd_device_handle, int, int);
usbd_status	usbd_get_device_status(usbd_device_handle, usb_status_t *);
usbd_status	usbd_get_hub_status(usbd_device_handle, usb_hub_status_t *);
usbd_status	usbd_set_protocol(usbd_interface_handle dev, int report);
usbd_status	usbd_get_report_descriptor(usbd_device_handle dev, int ifcno,
					   int size, void *d);
struct usb_hid_descriptor *usbd_get_hid_descriptor(usbd_interface_handle ifc);
usbd_status	usbd_set_report(usbd_interface_handle iface, int type, int id,
				void *data,int len);
usbd_status	usbd_set_report_async(usbd_interface_handle iface, int type,
				      int id, void *data, int len);
usbd_status	usbd_get_report(usbd_interface_handle iface, int type, int id,
				void *data, int len);
usbd_status	usbd_set_idle(usbd_interface_handle iface, int duration, int id);
usbd_status	usbd_read_report_desc(usbd_interface_handle ifc, void **descp,
				      int *sizep, usb_malloc_type mem);
usbd_status	usbd_get_config(usbd_device_handle dev, u_int8_t *conf);
usbd_status	usbd_get_string_desc(usbd_device_handle dev, int sindex,
				     int langid, usb_string_descriptor_t *sdesc);
void		usbd_delay_ms(usbd_device_handle, u_int);


usbd_status usbd_set_config_no(usbd_device_handle dev, int no, int msg);
usbd_status usbd_set_config_index(usbd_device_handle dev, int index, int msg);

usbd_status usbd_bulk_transfer(usbd_xfer_handle xfer, usbd_pipe_handle pipe,
			       u_int16_t flags, u_int32_t timeout, void *buf,
			       u_int32_t *size, char *lbl);

void usb_detach_wait(device_ptr_t);
void usb_detach_wakeup(device_ptr_t);

