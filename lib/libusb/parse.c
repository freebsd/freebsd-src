/*	$NetBSD: parse.c,v 1.9 2000/03/17 18:09:17 augustss Exp $	*/

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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>

#include "libusb.h"
#include "usbvar.h"

#define MAXUSAGE 100
struct hid_data {
	u_char *start;
	u_char *end;
	u_char *p;
	hid_item_t cur;
	unsigned int usages[MAXUSAGE];
	int nusage;
	int minset;
	int multi;
	int multimax;
	int kindset;
};

static int min(int x, int y) { return x < y ? x : y; }

static void
hid_clear_local(hid_item_t *c)
{
	c->usage = 0;
	c->usage_minimum = 0;
	c->usage_maximum = 0;
	c->designator_index = 0;
	c->designator_minimum = 0;
	c->designator_maximum = 0;
	c->string_index = 0;
	c->string_minimum = 0;
	c->string_maximum = 0;
	c->set_delimiter = 0;
}

hid_data_t
hid_start_parse(report_desc_t d, int kindset)
{
	struct hid_data *s;

	s = malloc(sizeof *s);
	memset(s, 0, sizeof *s);
	s->start = s->p = d->data;
	s->end = d->data + d->size;
	s->kindset = kindset;
	return (s);
}

void
hid_end_parse(hid_data_t s)
{
	while (s->cur.next) {
		hid_item_t *hi = s->cur.next->next;
		free(s->cur.next);
		s->cur.next = hi;
	}
	free(s);
}

int
hid_get_item(hid_data_t s, hid_item_t *h)
{
	hid_item_t *c;
	unsigned int bTag = 0, bType = 0, bSize, oldpos;
	unsigned char *data;
	int dval;
	unsigned char *p;
	hid_item_t *hi;
	int i;

	c = &s->cur;

 top:
	if (s->multimax) {
		if (s->multi < s->multimax) {
			c->usage = s->usages[min(s->multi, s->nusage-1)];
			s->multi++;
			*h = *c;
			c->pos += c->report_size;
			h->next = 0;
			return (1);
		} else {
			c->report_count = s->multimax;
			s->multimax = 0;
			s->nusage = 0;
			hid_clear_local(c);
		}
	}
	for (;;) {
		p = s->p;
		if (p >= s->end)
			return (0);

		bSize = *p++;
		if (bSize == 0xfe) {
			/* long item */
			bSize = *p++;
			bSize |= *p++ << 8;
			bTag = *p++;
			data = p;
			p += bSize;
		} else {
			/* short item */
			bTag = bSize >> 4;
			bType = (bSize >> 2) & 3;
			bSize &= 3;
			if (bSize == 3) bSize = 4;
			data = p;
			p += bSize;
		}
		s->p = p;
		/*
		 * The spec is unclear if the data is signed or unsigned.
		 */
		switch(bSize) {
		case 0:
			dval = 0;
			break;
		case 1:
			dval = (int8_t)*data++;
			break;
		case 2:
			dval = *data++;
			dval |= *data++ << 8;
			dval = (int16_t)dval;
			break;
		case 4:
			dval = *data++;
			dval |= *data++ << 8;
			dval |= *data++ << 16;
			dval |= *data++ << 24;
			break;
		default:
			return (-1);
		}

		switch (bType) {
		case 0:			/* Main */
			switch (bTag) {
			case 8:		/* Input */
				if (!(s->kindset & (1 << hid_input)))
					continue;
				c->kind = hid_input;
				c->flags = dval;
			ret:
				if (c->flags & HIO_VARIABLE) {
					s->multimax = c->report_count;
					s->multi = 0;
					c->report_count = 1;
					if (s->minset) {
						for (i = c->usage_minimum;
						     i <= c->usage_maximum;
						     i++) {
							s->usages[s->nusage] = i;
							if (s->nusage < MAXUSAGE-1)
								s->nusage++;
						}
						s->minset = 0;
					}
					goto top;
				} else {
					if (s->minset)
						c->usage = c->usage_minimum;
					*h = *c;
					h->next = 0;
					c->pos += c->report_size * c->report_count;
					hid_clear_local(c);
					s->minset = 0;
					return (1);
				}
			case 9:		/* Output */
				if (!(s->kindset & (1 << hid_output)))
					continue;
				c->kind = hid_output;
				c->flags = dval;
				goto ret;
			case 10:	/* Collection */
				c->kind = hid_collection;
				c->collection = dval;
				c->collevel++;
				*h = *c;
				hid_clear_local(c);
				c->report_ID = NO_REPORT_ID;
				s->nusage = 0;
				return (1);
			case 11:	/* Feature */
				if (!(s->kindset & (1 << hid_feature)))
					continue;
				c->kind = hid_feature;
				c->flags = dval;
				goto ret;
			case 12:	/* End collection */
				c->kind = hid_endcollection;
				c->collevel--;
				*h = *c;
				/*hid_clear_local(c);*/
				s->nusage = 0;
				return (1);
			default:
				return (-2);
			}

		case 1:		/* Global */
			switch (bTag) {
			case 0:
				c->_usage_page = dval << 16;
				break;
			case 1:
				c->logical_minimum = dval;
				break;
			case 2:
				c->logical_maximum = dval;
				break;
			case 3:
				c->physical_maximum = dval;
				break;
			case 4:
				c->physical_maximum = dval;
				break;
			case 5:
				c->unit_exponent = dval;
				break;
			case 6:
				c->unit = dval;
				break;
			case 7:
				c->report_size = dval;
				break;
			case 8:
				c->report_ID = dval;
				break;
			case 9:
				c->report_count = dval;
				break;
			case 10: /* Push */
				hi = malloc(sizeof *hi);
				*hi = s->cur;
				c->next = hi;
				break;
			case 11: /* Pop */
				hi = c->next;
				oldpos = c->pos;
				s->cur = *hi;
				c->pos = oldpos;
				free(hi);
				break;
			default:
				return (-3);
			}
			break;
		case 2:		/* Local */
			switch (bTag) {
			case 0:
				if (bSize == 1)
					dval = c->_usage_page | (dval&0xff);
				else if (bSize == 2)
					dval = c->_usage_page | (dval&0xffff);
				c->usage = dval;
				if (s->nusage < MAXUSAGE)
					s->usages[s->nusage++] = dval;
				/* else XXX */
				break;
			case 1:
				s->minset = 1;
				if (bSize == 1)
					dval = c->_usage_page | (dval&0xff);
				else if (bSize == 2)
					dval = c->_usage_page | (dval&0xffff);
				c->usage_minimum = dval;
				break;
			case 2:
				if (bSize == 1)
					dval = c->_usage_page | (dval&0xff);
				else if (bSize == 2)
					dval = c->_usage_page | (dval&0xffff);
				c->usage_maximum = dval;
				break;
			case 3:
				c->designator_index = dval;
				break;
			case 4:
				c->designator_minimum = dval;
				break;
			case 5:
				c->designator_maximum = dval;
				break;
			case 7:
				c->string_index = dval;
				break;
			case 8:
				c->string_minimum = dval;
				break;
			case 9:
				c->string_maximum = dval;
				break;
			case 10:
				c->set_delimiter = dval;
				break;
			default:
				return (-4);
			}
			break;
		default:
			return (-5);
		}
	}
}

int
hid_report_size(report_desc_t r, enum hid_kind k, int *idp)
{
	struct hid_data *d;
	hid_item_t h;
	int size, id;

	id = 0;
	if (idp)
		*idp = 0;
	memset(&h, 0, sizeof h);
	for (d = hid_start_parse(r, 1<<k); hid_get_item(d, &h); ) {
		if (h.report_ID != NO_REPORT_ID) {
			if (idp)
				*idp = h.report_ID;
			id = 8;
		}
	}
	hid_end_parse(d);
	size = h.pos + id;
	return ((size + 7) / 8);
}

int
hid_locate(desc, u, k, h)
	report_desc_t desc;
	unsigned int u;
	enum hid_kind k;
	hid_item_t *h;
{
	hid_data_t d;

	for (d = hid_start_parse(desc, 1<<k); hid_get_item(d, h); ) {
		if (h->kind == k && !(h->flags & HIO_CONST) && h->usage == u) {
			hid_end_parse(d);
			return (1);
		}
	}
	hid_end_parse(d);
	h->report_size = 0;
	return (0);
}
