/*	$NetBSD: usbdi_util.h,v 1.17 1999/09/05 19:32:19 augustss Exp $	*/
/*	$FreeBSD: src/sys/dev/usb/usbdi_util.h,v 1.9.2.1 2000/07/02 11:44:00 n_hibma Exp $	*/

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

usbd_status	usbd_get_desc __P((usbd_device_handle dev, int type, 
				   int index, int len, void *desc));
usbd_status	usbd_get_config_desc __P((usbd_device_handle, int, 
					  usb_config_descriptor_t *));
usbd_status	usbd_get_config_desc_full __P((usbd_device_handle, int, 
					       void *, int));
usbd_status	usbd_get_device_desc __P((usbd_device_handle dev,
					  usb_device_descriptor_t *d));
usbd_status	usbd_set_address __P((usbd_device_handle dev, int addr));
usbd_status	usbd_get_port_status __P((usbd_device_handle, 
				      int, usb_port_status_t *));
usbd_status	usbd_set_hub_feature __P((usbd_device_handle dev, int));
usbd_status	usbd_clear_hub_feature __P((usbd_device_handle, int));
usbd_status	usbd_set_port_feature __P((usbd_device_handle dev, int, int));
usbd_status	usbd_clear_port_feature __P((usbd_device_handle, int, int));
usbd_status	usbd_get_device_status __P((usbd_device_handle,usb_status_t*));
usbd_status	usbd_get_hub_status __P((usbd_device_handle dev,
					 usb_hub_status_t *st));
usbd_status	usbd_set_protocol __P((usbd_interface_handle dev, int report));
usbd_status	usbd_get_report_descriptor
	__P((usbd_device_handle dev, int ifcno, int repid, int size, void *d));
struct usb_hid_descriptor *usbd_get_hid_descriptor 
	__P((usbd_interface_handle ifc));
usbd_status	usbd_set_report 
	__P((usbd_interface_handle iface,int type,int id,void *data,int len));
usbd_status	usbd_set_report_async
	__P((usbd_interface_handle iface,int type,int id,void *data,int len));
usbd_status	usbd_get_report 
	__P((usbd_interface_handle iface,int type,int id,void *data,int len));
usbd_status	usbd_set_idle 
	__P((usbd_interface_handle iface, int duration, int id));
#if defined(__NetBSD__) || defined(__OpenBSD__)
usbd_status	usbd_alloc_report_desc
	__P((usbd_interface_handle ifc, void **descp, int *sizep, int mem));
#elif defined(__FreeBSD__)
usbd_status	usbd_alloc_report_desc
	__P((usbd_interface_handle ifc, void **descp, int *sizep, struct malloc_type * mem));
#endif
usbd_status	usbd_get_config
	__P((usbd_device_handle dev, u_int8_t *conf));
usbd_status	usbd_get_string_desc
	__P((usbd_device_handle dev, int sindex, int langid, 
	     usb_string_descriptor_t *sdesc));
void		usbd_delay_ms __P((usbd_device_handle, u_int));


usbd_status usbd_set_config_no
	__P((usbd_device_handle dev, int no, int msg));
usbd_status usbd_set_config_index
	__P((usbd_device_handle dev, int index, int msg));

usbd_status usbd_bulk_transfer
	__P((usbd_xfer_handle xfer, usbd_pipe_handle pipe, u_int16_t flags,
	     u_int32_t timeout, void *buf, u_int32_t *size, char *lbl));

void usb_detach_wait __P((device_ptr_t));
void usb_detach_wakeup __P((device_ptr_t));

