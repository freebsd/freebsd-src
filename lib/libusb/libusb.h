/*	$NetBSD: usb.h,v 1.8 2000/08/13 22:22:02 augustss Exp $	*/

/*
 * Copyright (c) 1999 Lennart Augustsson <augustss@netbsd.org>
 * All rights reserved.
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
 *
 * $FreeBSD$
 *
 */

typedef struct report_desc *report_desc_t;

typedef struct hid_data *hid_data_t;

typedef enum hid_kind {
	hid_input, hid_output, hid_feature, hid_collection, hid_endcollection
} hid_kind_t;

typedef struct hid_item {
	/* Global */
	int _usage_page;
	int logical_minimum;
	int logical_maximum;
	int physical_minimum;
	int physical_maximum;
	int unit_exponent;
	int unit;
	int report_size;
	int report_ID;
#define NO_REPORT_ID 0
	int report_count;
	/* Local */
	unsigned int usage;
	int usage_minimum;
	int usage_maximum;
	int designator_index;
	int designator_minimum;
	int designator_maximum;
	int string_index;
	int string_minimum;
	int string_maximum;
	int set_delimiter;
	/* Misc */
	int collection;
	int collevel;
	enum hid_kind kind;
	unsigned int flags;
	/* Absolute data position (bits) */
	unsigned int pos;
	/* */
	struct hid_item *next;
} hid_item_t;

#define HID_PAGE(u) (((u) >> 16) & 0xffff)
#define HID_USAGE(u) ((u) & 0xffff)

/* Obtaining a report descriptor, descr.c: */
report_desc_t hid_get_report_desc __P((int file));
report_desc_t hid_use_report_desc __P((unsigned char *data, unsigned int size));
void hid_dispose_report_desc __P((report_desc_t));

/* Parsing of a HID report descriptor, parse.c: */
hid_data_t hid_start_parse __P((report_desc_t d, int kindset));
void hid_end_parse __P((hid_data_t s));
int hid_get_item __P((hid_data_t s, hid_item_t *h));
int hid_report_size __P((report_desc_t d, unsigned int id, enum hid_kind k));
int hid_locate __P((report_desc_t d, unsigned int usage, enum hid_kind k, hid_item_t *h));

/* Conversion to/from usage names, usage.c: */
const char *hid_usage_page __P((int i));
const char *hid_usage_in_page __P((unsigned int u));
void hid_init __P((const char *file));

/* Extracting/insertion of data, data.c: */
int hid_get_data __P((const void *p, const hid_item_t *h));
void hid_set_data __P((void *p, const hid_item_t *h, int data));
