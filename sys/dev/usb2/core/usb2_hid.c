/*	$NetBSD: hid.c,v 1.17 2001/11/13 06:24:53 lukem Exp $	*/


#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
/*-
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

#include <dev/usb2/include/usb2_standard.h>
#include <dev/usb2/include/usb2_mfunc.h>
#include <dev/usb2/include/usb2_error.h>
#include <dev/usb2/include/usb2_defs.h>
#include <dev/usb2/include/usb2_hid.h>

#define	USB_DEBUG_VAR usb2_debug

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_debug.h>
#include <dev/usb2/core/usb2_parse.h>
#include <dev/usb2/core/usb2_process.h>
#include <dev/usb2/core/usb2_device.h>
#include <dev/usb2/core/usb2_request.h>
#include <dev/usb2/core/usb2_hid.h>

static void hid_clear_local(struct hid_item *);

#define	MAXUSAGE 100
struct hid_data {
	const uint8_t *start;
	const uint8_t *end;
	const uint8_t *p;
	struct hid_item cur;
	int32_t	usages[MAXUSAGE];
	int	nu;
	int	minset;
	int	multi;
	int	multimax;
	int	kindset;
};

/*------------------------------------------------------------------------*
 *	hid_clear_local
 *------------------------------------------------------------------------*/
static void
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

/*------------------------------------------------------------------------*
 *	hid_start_parse
 *------------------------------------------------------------------------*/
struct hid_data *
hid_start_parse(const void *d, int len, int kindset)
{
	struct hid_data *s;

	s = malloc(sizeof *s, M_TEMP, M_WAITOK | M_ZERO);
	s->start = s->p = d;
	s->end = ((const uint8_t *)d) + len;
	s->kindset = kindset;
	return (s);
}

/*------------------------------------------------------------------------*
 *	hid_end_parse
 *------------------------------------------------------------------------*/
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

/*------------------------------------------------------------------------*
 *	hid_get_item
 *------------------------------------------------------------------------*/
int
hid_get_item(struct hid_data *s, struct hid_item *h)
{
	struct hid_item *c = &s->cur;
	unsigned int bTag, bType, bSize;
	uint32_t oldpos;
	const uint8_t *data;
	int32_t dval;
	const uint8_t *p;
	struct hid_item *hi;
	int i;

top:
	if (s->multimax != 0) {
		if (s->multi < s->multimax) {
			c->usage = s->usages[MIN(s->multi, s->nu - 1)];
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
			bType = 0xff;	/* XXX what should it be */
		} else {
			/* short item */
			bTag = bSize >> 4;
			bType = (bSize >> 2) & 3;
			bSize &= 3;
			if (bSize == 3)
				bSize = 4;
			data = p;
			p += bSize;
		}
		s->p = p;
		switch (bSize) {
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
		case 0:		/* Main */
			switch (bTag) {
			case 8:	/* Input */
				if (!(s->kindset & (1 << hid_input))) {
					if (s->nu > 0)
						s->nu--;
					continue;
				}
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
							if (s->nu < MAXUSAGE - 1)
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
			case 9:	/* Output */
				if (!(s->kindset & (1 << hid_output))) {
					if (s->nu > 0)
						s->nu--;
					continue;
				}
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
				if (!(s->kindset & (1 << hid_feature))) {
					if (s->nu > 0)
						s->nu--;
					continue;
				}
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
				c->physical_minimum = dval;
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
			case 10:	/* Push */
				hi = malloc(sizeof *hi, M_TEMP, M_WAITOK);
				*hi = s->cur;
				c->next = hi;
				break;
			case 11:	/* Pop */
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
					dval = c->_usage_page | (dval & 0xff);
				else if (bSize == 2)
					dval = c->_usage_page | (dval & 0xffff);
				c->usage = dval;
				if (s->nu < MAXUSAGE)
					s->usages[s->nu++] = dval;
				/* else XXX */
				break;
			case 1:
				s->minset = 1;
				if (bSize == 1)
					dval = c->_usage_page | (dval & 0xff);
				else if (bSize == 2)
					dval = c->_usage_page | (dval & 0xffff);
				c->usage_minimum = dval;
				break;
			case 2:
				if (bSize == 1)
					dval = c->_usage_page | (dval & 0xff);
				else if (bSize == 2)
					dval = c->_usage_page | (dval & 0xffff);
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

/*------------------------------------------------------------------------*
 *	hid_report_size
 *------------------------------------------------------------------------*/
int
hid_report_size(const void *buf, int len, enum hid_kind k, uint8_t *idp)
{
	struct hid_data *d;
	struct hid_item h;
	int hi, lo, size, id;

	id = 0;
	hi = lo = -1;
	for (d = hid_start_parse(buf, len, 1 << k); hid_get_item(d, &h);)
		if (h.kind == k) {
			if (h.report_ID != 0 && !id)
				id = h.report_ID;
			if (h.report_ID == id) {
				if (lo < 0)
					lo = h.loc.pos;
				hi = h.loc.pos + h.loc.size * h.loc.count;
			}
		}
	hid_end_parse(d);
	size = hi - lo;
	if (id != 0) {
		size += 8;
		*idp = id;		/* XXX wrong */
	} else
		*idp = 0;
	return ((size + 7) / 8);
}

/*------------------------------------------------------------------------*
 *	hid_locate
 *------------------------------------------------------------------------*/
int
hid_locate(const void *desc, int size, uint32_t u, enum hid_kind k,
    struct hid_location *loc, uint32_t *flags)
{
	struct hid_data *d;
	struct hid_item h;

	for (d = hid_start_parse(desc, size, 1 << k); hid_get_item(d, &h);) {
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

/*------------------------------------------------------------------------*
 *	hid_get_data
 *------------------------------------------------------------------------*/
uint32_t
hid_get_data(const uint8_t *buf, uint32_t len, struct hid_location *loc)
{
	uint32_t hpos = loc->pos;
	uint32_t hsize = loc->size;
	uint32_t data;
	int i, s, t;

	DPRINTFN(11, "hid_get_data: loc %d/%d\n", hpos, hsize);

	if (hsize == 0)
		return (0);

	data = 0;
	s = hpos / 8;
	for (i = hpos; i < (hpos + hsize); i += 8) {
		t = (i / 8);
		if (t < len) {
			data |= buf[t] << ((t - s) * 8);
		}
	}
	data >>= hpos % 8;
	data &= (1 << hsize) - 1;
	hsize = 32 - hsize;
	/* Sign extend */
	data = ((int32_t)data << hsize) >> hsize;
	DPRINTFN(11, "hid_get_data: loc %d/%d = %lu\n",
	    loc->pos, loc->size, (long)data);
	return (data);
}

/*------------------------------------------------------------------------*
 *	hid_is_collection
 *------------------------------------------------------------------------*/
int
hid_is_collection(const void *desc, int size, uint32_t usage)
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

/*------------------------------------------------------------------------*
 *	hid_get_descriptor_from_usb
 *
 * This function will search for a HID descriptor between two USB
 * interface descriptors.
 *
 * Return values:
 * NULL: No more HID descriptors.
 * Else: Pointer to HID descriptor.
 *------------------------------------------------------------------------*/
struct usb2_hid_descriptor *
hid_get_descriptor_from_usb(struct usb2_config_descriptor *cd,
    struct usb2_interface_descriptor *id)
{
	struct usb2_descriptor *desc = (void *)id;

	if (desc == NULL) {
		return (NULL);
	}
	while ((desc = usb2_desc_foreach(cd, desc))) {
		if ((desc->bDescriptorType == UDESC_HID) &&
		    (desc->bLength >= USB_HID_DESCRIPTOR_SIZE(0))) {
			return (void *)desc;
		}
		if (desc->bDescriptorType == UDESC_INTERFACE) {
			break;
		}
	}
	return (NULL);
}

/*------------------------------------------------------------------------*
 *	usb2_req_get_hid_desc
 *
 * This function will read out an USB report descriptor from the USB
 * device.
 *
 * Return values:
 * NULL: Failure.
 * Else: Success. The pointer should eventually be passed to free().
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_req_get_hid_desc(struct usb2_device *udev, struct mtx *mtx,
    void **descp, uint16_t *sizep,
    usb2_malloc_type mem, uint8_t iface_index)
{
	struct usb2_interface *iface = usb2_get_iface(udev, iface_index);
	struct usb2_hid_descriptor *hid;
	usb2_error_t err;

	if ((iface == NULL) || (iface->idesc == NULL)) {
		return (USB_ERR_INVAL);
	}
	hid = hid_get_descriptor_from_usb
	    (usb2_get_config_descriptor(udev), iface->idesc);

	if (hid == NULL) {
		return (USB_ERR_IOERROR);
	}
	*sizep = UGETW(hid->descrs[0].wDescriptorLength);
	if (*sizep == 0) {
		return (USB_ERR_IOERROR);
	}
	if (mtx)
		mtx_unlock(mtx);

	*descp = malloc(*sizep, mem, M_ZERO | M_WAITOK);

	if (mtx)
		mtx_lock(mtx);

	if (*descp == NULL) {
		return (USB_ERR_NOMEM);
	}
	err = usb2_req_get_report_descriptor
	    (udev, mtx, *descp, *sizep, iface_index);

	if (err) {
		free(*descp, mem);
		*descp = NULL;
		return (err);
	}
	return (USB_ERR_NORMAL_COMPLETION);
}
