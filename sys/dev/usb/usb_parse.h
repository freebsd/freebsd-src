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

#ifndef _USB2_PARSE_H_
#define	_USB2_PARSE_H_

/* structures */

struct usb_idesc_parse_state {
	struct usb_descriptor *desc;
	uint8_t iface_index;		/* current interface index */
	uint8_t iface_no_last;
	uint8_t iface_index_alt;	/* current alternate setting */
};

/* prototypes */

struct usb_descriptor *usb_desc_foreach(struct usb_config_descriptor *cd,
	    struct usb_descriptor *desc);
struct usb_interface_descriptor *usb_idesc_foreach(
	    struct usb_config_descriptor *cd,
	    struct usb_idesc_parse_state *ps);
struct usb_endpoint_descriptor *usb_edesc_foreach(
	    struct usb_config_descriptor *cd,
	    struct usb_endpoint_descriptor *ped);
uint8_t usbd_get_no_descriptors(struct usb_config_descriptor *cd,
	    uint8_t type);
uint8_t usbd_get_no_alts(struct usb_config_descriptor *cd,
	    struct usb_interface_descriptor *id);

#endif					/* _USB2_PARSE_H_ */
