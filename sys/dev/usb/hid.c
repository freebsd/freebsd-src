/*	$NetBSD: hid.c,v 1.17 2001/11/13 06:24:53 lukem Exp $	*/


#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
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

#include <sys/param.h>
#include <sys/systm.h>
#if defined(__NetBSD__)
#include <sys/kernel.h>
#endif
#include <sys/malloc.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>

#include <dev/usb/hid.h>

#ifdef USB_DEBUG
#define DPRINTF(x)	if (usbdebug) logprintf x
#define DPRINTFN(n,x)	if (usbdebug>(n)) logprintf x
extern int usbdebug;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

Static void hid_clear_local(struct hid_item *);

#define MAXUSAGE 100
struct hid_data {
	u_char *start;
	u_char *end;
	u_char *p;
	struct hid_item cur;
	int32_t usages[MAXUSAGE];
	int nu;
	int minset;
	int multi;
	int multimax;
	int kindset;
};

Static void
hid_clear_local(struct hid_item *c)
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

struct hid_data *
hid_start_parse(void *d, int len, int kindset)
{
	struct hid_data *s;

	s = malloc(sizeof *s, M_TEMP, M_WAITOK|M_ZERO);
	s->start = s->p = d;
	s->end = (char *)d + len;
	s->kindset = kindset;
	return (s);
}

void
hid_end_parse(struct hid_data *s)
{

	while (s->cur.next != NULL) {
		struct hid_item *hi = s->cur.next->next;
		free(s->cur.next, M_TEMP);
		s->cur.next = hi;
	}
	free(s, M_TEMP);
}

int
hid_get_item(struct hid_data *s, struct hid_item *h)
{
	struct hid_item *c = &s->cur;
	unsigned int bTag, bType, bSize;
	u_int32_t oldpos;
	u_char *data;
	int32_t dval;
	u_char *p;
	struct hid_item *hi;
	int i;

 top:
	if (s->multimax != 0) {
		if (s->multi < s->multimax) {
			c->usage = s->usages[min(s->multi, s->nu-1)];
			s->multi++;
			*h = *c;
			c->loc.pos += c->loc.size;
			h->next = 0;
			return (1);
		} else {
			c->loc.count = s->multimax;
			s->multimax = 0;
			s->nu = 0;
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
			bType = 0xff; /* XXX what should it be */
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
			printf("BAD LENGTH %d\n", bSize);
			continue;
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
					s->multimax = c->loc.count;
					s->multi = 0;
					c->loc.count = 1;
					if (s->minset) {
						for (i = c->usage_minimum;
						     i <= c->usage_maximum;
						     i++) {
							s->usages[s->nu] = i;
							if (s->nu < MAXUSAGE-1)
								s->nu++;
						}
						s->minset = 0;
					}
					goto top;
				} else {
					*h = *c;
					h->next = 0;
					c->loc.pos +=
						c->loc.size * c->loc.count;
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
				s->nu = 0;
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
				hid_clear_local(c);
				s->nu = 0;
				return (1);
			default:
				printf("Main bTag=%d\n", bTag);
				break;
			}
			break;
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
				c->loc.size = dval;
				break;
			case 8:
				c->report_ID = dval;
				break;
			case 9:
				c->loc.count = dval;
				break;
			case 10: /* Push */
				hi = malloc(sizeof *hi, M_TEMP, M_WAITOK);
				*hi = s->cur;
				c->next = hi;
				break;
			case 11: /* Pop */
				hi = c->next;
				oldpos = c->loc.pos;
				s->cur = *hi;
				c->loc.pos = oldpos;
				free(hi, M_TEMP);
				break;
			default:
				printf("Global bTag=%d\n", bTag);
				break;
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
				if (s->nu < MAXUSAGE)
					s->usages[s->nu++] = dval;
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
				printf("Local bTag=%d\n", bTag);
				break;
			}
			break;
		default:
			printf("default bType=%d\n", bType);
			break;
		}
	}
}

int
hid_report_size(void *buf, int len, enum hid_kind k, u_int8_t *idp)
{
	struct hid_data *d;
	struct hid_item h;
	int size, id;

	id = 0;
	for (d = hid_start_parse(buf, len, 1<<k); hid_get_item(d, &h); )
		if (h.report_ID != 0)
			id = h.report_ID;
	hid_end_parse(d);
	size = h.loc.pos;
	if (id != 0) {
		size += 8;
		*idp = id;	/* XXX wrong */
	} else
		*idp = 0;
	return ((size + 7) / 8);
}

int
hid_locate(void *desc, int size, u_int32_t u, enum hid_kind k,
	   struct hid_location *loc, u_int32_t *flags)
{
	struct hid_data *d;
	struct hid_item h;

	for (d = hid_start_parse(desc, size, 1<<k); hid_get_item(d, &h); ) {
		if (h.kind == k && !(h.flags & HIO_CONST) && h.usage == u) {
			if (loc != NULL)
				*loc = h.loc;
			if (flags != NULL)
				*flags = h.flags;
			hid_end_parse(d);
			return (1);
		}
	}
	hid_end_parse(d);
	loc->size = 0;
	return (0);
}

u_long
hid_get_data(u_char *buf, struct hid_location *loc)
{
	u_int hpos = loc->pos;
	u_int hsize = loc->size;
	u_int32_t data;
	int i, s;

	DPRINTFN(10, ("hid_get_data: loc %d/%d\n", hpos, hsize));

	if (hsize == 0)
		return (0);

	data = 0;
	s = hpos / 8;
	for (i = hpos; i < hpos+hsize; i += 8)
		data |= buf[i / 8] << ((i / 8 - s) * 8);
	data >>= hpos % 8;
	data &= (1 << hsize) - 1;
	hsize = 32 - hsize;
	/* Sign extend */
	data = ((int32_t)data << hsize) >> hsize;
	DPRINTFN(10,("hid_get_data: loc %d/%d = %lu\n",
		    loc->pos, loc->size, (long)data));
	return (data);
}

int
hid_is_collection(void *desc, int size, u_int32_t usage)
{
	struct hid_data *hd;
	struct hid_item hi;
	int err;

	hd = hid_start_parse(desc, size, hid_input);
	if (hd == NULL)
		return (0);

	err = hid_get_item(hd, &hi) &&
	    hi.kind == hid_collection &&
	    hi.usage == usage;
	hid_end_parse(hd);
	return (err);
}
