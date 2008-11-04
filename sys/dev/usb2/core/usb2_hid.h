/* $FreeBSD$ */
/*-
 * Copyright (c) 2008 Hans Petter Selasky. All rights reserved.
 * Copyright (c) 1998 The NetBSD Foundation, Inc. All rights reserved.
 * Copyright (c) 1998 Lennart Augustsson. All rights reserved.
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

#ifndef _USB2_CORE_HID_H_
#define	_USB2_CORE_HID_H_

struct usb2_hid_descriptor;
struct usb2_config_descriptor;

enum hid_kind {
	hid_input, hid_output, hid_feature, hid_collection, hid_endcollection
};

struct hid_location {
	uint32_t size;
	uint32_t count;
	uint32_t pos;
};

struct hid_item {
	/* Global */
	int32_t	_usage_page;
	int32_t	logical_minimum;
	int32_t	logical_maximum;
	int32_t	physical_minimum;
	int32_t	physical_maximum;
	int32_t	unit_exponent;
	int32_t	unit;
	int32_t	report_ID;
	/* Local */
	int32_t	usage;
	int32_t	usage_minimum;
	int32_t	usage_maximum;
	int32_t	designator_index;
	int32_t	designator_minimum;
	int32_t	designator_maximum;
	int32_t	string_index;
	int32_t	string_minimum;
	int32_t	string_maximum;
	int32_t	set_delimiter;
	/* Misc */
	int32_t	collection;
	int	collevel;
	enum hid_kind kind;
	uint32_t flags;
	/* Location */
	struct hid_location loc;
	/* */
	struct hid_item *next;
};

/* prototypes from "usb2_hid.c" */

struct hid_data *hid_start_parse(const void *d, int len, int kindset);
void	hid_end_parse(struct hid_data *s);
int	hid_get_item(struct hid_data *s, struct hid_item *h);
int	hid_report_size(const void *buf, int len, enum hid_kind k, uint8_t *id);
int	hid_locate(const void *desc, int size, uint32_t usage, enum hid_kind kind, struct hid_location *loc, uint32_t *flags);
uint32_t hid_get_data(const uint8_t *buf, uint32_t len, struct hid_location *loc);
int	hid_is_collection(const void *desc, int size, uint32_t usage);
struct usb2_hid_descriptor *hid_get_descriptor_from_usb(struct usb2_config_descriptor *cd, struct usb2_interface_descriptor *id);
usb2_error_t usb2_req_get_hid_desc(struct usb2_device *udev, struct mtx *mtx, void **descp, uint16_t *sizep, usb2_malloc_type mem, uint8_t iface_index);

#endif					/* _USB2_CORE_HID_H_ */
