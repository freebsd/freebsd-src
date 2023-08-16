/*	$NetBSD: hid.c,v 1.17 2001/11/13 06:24:53 lukem Exp $	*/
/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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

#include "opt_hid.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/sysctl.h>

#define	HID_DEBUG_VAR	hid_debug
#include <dev/hid/hid.h>
#include <dev/hid/hidquirk.h>

#include "hid_if.h"

/*
 * Define this unconditionally in case a kernel module is loaded that
 * has been compiled with debugging options.
 */
int	hid_debug = 0;

SYSCTL_NODE(_hw, OID_AUTO, hid, CTLFLAG_RW, 0, "HID debugging");
SYSCTL_INT(_hw_hid, OID_AUTO, debug, CTLFLAG_RWTUN,
    &hid_debug, 0, "Debug level");

static void hid_clear_local(struct hid_item *);
static uint8_t hid_get_byte(struct hid_data *s, const uint16_t wSize);

static hid_test_quirk_t hid_test_quirk_w;
hid_test_quirk_t *hid_test_quirk_p = &hid_test_quirk_w;

#define	MAXUSAGE 64
#define	MAXPUSH 4
#define	MAXID 16
#define	MAXLOCCNT 2048

struct hid_pos_data {
	int32_t rid;
	uint32_t pos;
};

struct hid_data {
	const uint8_t *start;
	const uint8_t *end;
	const uint8_t *p;
	struct hid_item cur[MAXPUSH];
	struct hid_pos_data last_pos[MAXID];
	int32_t	usages_min[MAXUSAGE];
	int32_t	usages_max[MAXUSAGE];
	int32_t usage_last;	/* last seen usage */
	uint32_t loc_size;	/* last seen size */
	uint32_t loc_count;	/* last seen count */
	uint32_t ncount;	/* end usage item count */
	uint32_t icount;	/* current usage item count */
	uint8_t	kindset;	/* we have 5 kinds so 8 bits are enough */
	uint8_t	pushlevel;	/* current pushlevel */
	uint8_t	nusage;		/* end "usages_min/max" index */
	uint8_t	iusage;		/* current "usages_min/max" index */
	uint8_t ousage;		/* current "usages_min/max" offset */
	uint8_t	susage;		/* usage set flags */
};

/*------------------------------------------------------------------------*
 *	hid_clear_local
 *------------------------------------------------------------------------*/
static void
hid_clear_local(struct hid_item *c)
{

	c->loc.count = 0;
	c->loc.size = 0;
	c->nusages = 0;
	memset(c->usages, 0, sizeof(c->usages));
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

static void
hid_switch_rid(struct hid_data *s, struct hid_item *c, int32_t next_rID)
{
	uint8_t i;

	/* check for same report ID - optimise */

	if (c->report_ID == next_rID)
		return;

	/* save current position for current rID */

	if (c->report_ID == 0) {
		i = 0;
	} else {
		for (i = 1; i != MAXID; i++) {
			if (s->last_pos[i].rid == c->report_ID)
				break;
			if (s->last_pos[i].rid == 0)
				break;
		}
	}
	if (i != MAXID) {
		s->last_pos[i].rid = c->report_ID;
		s->last_pos[i].pos = c->loc.pos;
	}

	/* store next report ID */

	c->report_ID = next_rID;

	/* lookup last position for next rID */

	if (next_rID == 0) {
		i = 0;
	} else {
		for (i = 1; i != MAXID; i++) {
			if (s->last_pos[i].rid == next_rID)
				break;
			if (s->last_pos[i].rid == 0)
				break;
		}
	}
	if (i != MAXID) {
		s->last_pos[i].rid = next_rID;
		c->loc.pos = s->last_pos[i].pos;
	} else {
		DPRINTF("Out of RID entries, position is set to zero!\n");
		c->loc.pos = 0;
	}
}

/*------------------------------------------------------------------------*
 *	hid_start_parse
 *------------------------------------------------------------------------*/
struct hid_data *
hid_start_parse(const void *d, hid_size_t len, int kindset)
{
	struct hid_data *s;

	if ((kindset-1) & kindset) {
		DPRINTFN(0, "Only one bit can be "
		    "set in the kindset\n");
		return (NULL);
	}

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
	if (s == NULL)
		return;

	free(s, M_TEMP);
}

/*------------------------------------------------------------------------*
 *	get byte from HID descriptor
 *------------------------------------------------------------------------*/
static uint8_t
hid_get_byte(struct hid_data *s, const uint16_t wSize)
{
	const uint8_t *ptr;
	uint8_t retval;

	ptr = s->p;

	/* check if end is reached */
	if (ptr == s->end)
		return (0);

	/* read out a byte */
	retval = *ptr;

	/* check if data pointer can be advanced by "wSize" bytes */
	if ((s->end - ptr) < wSize)
		ptr = s->end;
	else
		ptr += wSize;

	/* update pointer */
	s->p = ptr;

	return (retval);
}

/*------------------------------------------------------------------------*
 *	hid_get_item
 *------------------------------------------------------------------------*/
int
hid_get_item(struct hid_data *s, struct hid_item *h)
{
	struct hid_item *c;
	unsigned int bTag, bType, bSize;
	uint32_t oldpos;
	int32_t mask;
	int32_t dval;

	if (s == NULL)
		return (0);

	c = &s->cur[s->pushlevel];

 top:
	/* check if there is an array of items */
	if (s->icount < s->ncount) {
		/* get current usage */
		if (s->iusage < s->nusage) {
			dval = s->usages_min[s->iusage] + s->ousage;
			c->usage = dval;
			s->usage_last = dval;
			if (dval == s->usages_max[s->iusage]) {
				s->iusage ++;
				s->ousage = 0;
			} else {
				s->ousage ++;
			}
		} else {
			DPRINTFN(1, "Using last usage\n");
			dval = s->usage_last;
		}
		c->nusages = 1;
		/* array type HID item may have multiple usages */
		while ((c->flags & HIO_VARIABLE) == 0 && s->ousage == 0 &&
		    s->iusage < s->nusage && c->nusages < HID_ITEM_MAXUSAGE)
			c->usages[c->nusages++] = s->usages_min[s->iusage++];
		if ((c->flags & HIO_VARIABLE) == 0 && s->ousage == 0 &&
		    s->iusage < s->nusage)
			DPRINTFN(0, "HID_ITEM_MAXUSAGE should be increased "
			    "up to %hhu to parse the HID report descriptor\n",
			    s->nusage);
		s->icount ++;
		/* 
		 * Only copy HID item, increment position and return
		 * if correct kindset!
		 */
		if (s->kindset & (1 << c->kind)) {
			*h = *c;
			DPRINTFN(1, "%u,%u,%u\n", h->loc.pos,
			    h->loc.size, h->loc.count);
			c->loc.pos += c->loc.size * c->loc.count;
			return (1);
		}
	}

	/* reset state variables */
	s->icount = 0;
	s->ncount = 0;
	s->iusage = 0;
	s->nusage = 0;
	s->susage = 0;
	s->ousage = 0;
	hid_clear_local(c);

	/* get next item */
	while (s->p != s->end) {
		bSize = hid_get_byte(s, 1);
		if (bSize == 0xfe) {
			/* long item */
			bSize = hid_get_byte(s, 1);
			bSize |= hid_get_byte(s, 1) << 8;
			bTag = hid_get_byte(s, 1);
			bType = 0xff;	/* XXX what should it be */
		} else {
			/* short item */
			bTag = bSize >> 4;
			bType = (bSize >> 2) & 3;
			bSize &= 3;
			if (bSize == 3)
				bSize = 4;
		}
		switch (bSize) {
		case 0:
			dval = 0;
			mask = 0;
			break;
		case 1:
			dval = (int8_t)hid_get_byte(s, 1);
			mask = 0xFF;
			break;
		case 2:
			dval = hid_get_byte(s, 1);
			dval |= hid_get_byte(s, 1) << 8;
			dval = (int16_t)dval;
			mask = 0xFFFF;
			break;
		case 4:
			dval = hid_get_byte(s, 1);
			dval |= hid_get_byte(s, 1) << 8;
			dval |= hid_get_byte(s, 1) << 16;
			dval |= hid_get_byte(s, 1) << 24;
			mask = 0xFFFFFFFF;
			break;
		default:
			dval = hid_get_byte(s, bSize);
			DPRINTFN(0, "bad length %u (data=0x%02x)\n",
			    bSize, dval);
			continue;
		}

		switch (bType) {
		case 0:		/* Main */
			switch (bTag) {
			case 8:	/* Input */
				c->kind = hid_input;
		ret:
				c->flags = dval;
				c->loc.count = s->loc_count;
				c->loc.size = s->loc_size;

				if (c->flags & HIO_VARIABLE) {
					/* range check usage count */
					if (c->loc.count > MAXLOCCNT) {
						DPRINTFN(0, "Number of "
						    "items(%u) truncated to %u\n",
						    (unsigned)(c->loc.count),
						    MAXLOCCNT);
						s->ncount = MAXLOCCNT;
					} else
						s->ncount = c->loc.count;

					/* 
					 * The "top" loop will return
					 * one and one item:
					 */
					c->loc.count = 1;
				} else {
					s->ncount = 1;
				}
				goto top;

			case 9:	/* Output */
				c->kind = hid_output;
				goto ret;
			case 10:	/* Collection */
				c->kind = hid_collection;
				c->collection = dval;
				c->collevel++;
				c->usage = s->usage_last;
				c->nusages = 1;
				*h = *c;
				return (1);
			case 11:	/* Feature */
				c->kind = hid_feature;
				goto ret;
			case 12:	/* End collection */
				c->kind = hid_endcollection;
				if (c->collevel == 0) {
					DPRINTFN(0, "invalid end collection\n");
					return (0);
				}
				c->collevel--;
				*h = *c;
				return (1);
			default:
				DPRINTFN(0, "Main bTag=%d\n", bTag);
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
				/* mask because value is unsigned */
				s->loc_size = dval & mask;
				break;
			case 8:
				hid_switch_rid(s, c, dval & mask);
				break;
			case 9:
				/* mask because value is unsigned */
				s->loc_count = dval & mask;
				break;
			case 10:	/* Push */
				/* stop parsing, if invalid push level */
				if ((s->pushlevel + 1) >= MAXPUSH) {
					DPRINTFN(0, "Cannot push item @ %d\n", s->pushlevel);
					return (0);
				}
				s->pushlevel ++;
				s->cur[s->pushlevel] = *c;
				/* store size and count */
				c->loc.size = s->loc_size;
				c->loc.count = s->loc_count;
				/* update current item pointer */
				c = &s->cur[s->pushlevel];
				break;
			case 11:	/* Pop */
				/* stop parsing, if invalid push level */
				if (s->pushlevel == 0) {
					DPRINTFN(0, "Cannot pop item @ 0\n");
					return (0);
				}
				s->pushlevel --;
				/* preserve position */
				oldpos = c->loc.pos;
				c = &s->cur[s->pushlevel];
				/* restore size and count */
				s->loc_size = c->loc.size;
				s->loc_count = c->loc.count;
				/* set default item location */
				c->loc.pos = oldpos;
				c->loc.size = 0;
				c->loc.count = 0;
				break;
			default:
				DPRINTFN(0, "Global bTag=%d\n", bTag);
				break;
			}
			break;
		case 2:		/* Local */
			switch (bTag) {
			case 0:
				if (bSize != 4)
					dval = (dval & mask) | c->_usage_page;

				/* set last usage, in case of a collection */
				s->usage_last = dval;

				if (s->nusage < MAXUSAGE) {
					s->usages_min[s->nusage] = dval;
					s->usages_max[s->nusage] = dval;
					s->nusage ++;
				} else {
					DPRINTFN(0, "max usage reached\n");
				}

				/* clear any pending usage sets */
				s->susage = 0;
				break;
			case 1:
				s->susage |= 1;

				if (bSize != 4)
					dval = (dval & mask) | c->_usage_page;
				c->usage_minimum = dval;

				goto check_set;
			case 2:
				s->susage |= 2;

				if (bSize != 4)
					dval = (dval & mask) | c->_usage_page;
				c->usage_maximum = dval;

			check_set:
				if (s->susage != 3)
					break;

				/* sanity check */
				if ((s->nusage < MAXUSAGE) &&
				    (c->usage_minimum <= c->usage_maximum)) {
					/* add usage range */
					s->usages_min[s->nusage] = 
					    c->usage_minimum;
					s->usages_max[s->nusage] = 
					    c->usage_maximum;
					s->nusage ++;
				} else {
					DPRINTFN(0, "Usage set dropped\n");
				}
				s->susage = 0;
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
				DPRINTFN(0, "Local bTag=%d\n", bTag);
				break;
			}
			break;
		default:
			DPRINTFN(0, "default bType=%d\n", bType);
			break;
		}
	}
	return (0);
}

/*------------------------------------------------------------------------*
 *	hid_report_size
 *------------------------------------------------------------------------*/
int
hid_report_size(const void *buf, hid_size_t len, enum hid_kind k, uint8_t id)
{
	struct hid_data *d;
	struct hid_item h;
	uint32_t temp;
	uint32_t hpos;
	uint32_t lpos;
	int report_id = 0;

	hpos = 0;
	lpos = 0xFFFFFFFF;

	for (d = hid_start_parse(buf, len, 1 << k); hid_get_item(d, &h);) {
		if (h.kind == k && h.report_ID == id) {
			/* compute minimum */
			if (lpos > h.loc.pos)
				lpos = h.loc.pos;
			/* compute end position */
			temp = h.loc.pos + (h.loc.size * h.loc.count);
			/* compute maximum */
			if (hpos < temp)
				hpos = temp;
			if (h.report_ID != 0)
				report_id = 1;
		}
	}
	hid_end_parse(d);

	/* safety check - can happen in case of currupt descriptors */
	if (lpos > hpos)
		temp = 0;
	else
		temp = hpos - lpos;

	/* return length in bytes rounded up */
	return ((temp + 7) / 8 + report_id);
}

int
hid_report_size_max(const void *buf, hid_size_t len, enum hid_kind k,
    uint8_t *id)
{
	struct hid_data *d;
	struct hid_item h;
	uint32_t temp;
	uint32_t hpos;
	uint32_t lpos;
	uint8_t any_id;

	any_id = 0;
	hpos = 0;
	lpos = 0xFFFFFFFF;

	for (d = hid_start_parse(buf, len, 1 << k); hid_get_item(d, &h);) {
		if (h.kind == k) {
			/* check for ID-byte presence */
			if ((h.report_ID != 0) && !any_id) {
				if (id != NULL)
					*id = h.report_ID;
				any_id = 1;
			}
			/* compute minimum */
			if (lpos > h.loc.pos)
				lpos = h.loc.pos;
			/* compute end position */
			temp = h.loc.pos + (h.loc.size * h.loc.count);
			/* compute maximum */
			if (hpos < temp)
				hpos = temp;
		}
	}
	hid_end_parse(d);

	/* safety check - can happen in case of currupt descriptors */
	if (lpos > hpos)
		temp = 0;
	else
		temp = hpos - lpos;

	/* check for ID byte */
	if (any_id)
		temp += 8;
	else if (id != NULL)
		*id = 0;

	/* return length in bytes rounded up */
	return ((temp + 7) / 8);
}

/*------------------------------------------------------------------------*
 *	hid_locate
 *------------------------------------------------------------------------*/
int
hid_locate(const void *desc, hid_size_t size, int32_t u, enum hid_kind k,
    uint8_t index, struct hid_location *loc, uint32_t *flags, uint8_t *id)
{
	struct hid_data *d;
	struct hid_item h;
	int i;

	for (d = hid_start_parse(desc, size, 1 << k); hid_get_item(d, &h);) {
		for (i = 0; i < h.nusages; i++) {
			if (h.kind == k && h.usages[i] == u) {
				if (index--)
					break;
				if (loc != NULL)
					*loc = h.loc;
				if (flags != NULL)
					*flags = h.flags;
				if (id != NULL)
					*id = h.report_ID;
				hid_end_parse(d);
				return (1);
			}
		}
	}
	if (loc != NULL)
		loc->size = 0;
	if (flags != NULL)
		*flags = 0;
	if (id != NULL)
		*id = 0;
	hid_end_parse(d);
	return (0);
}

/*------------------------------------------------------------------------*
 *	hid_get_data
 *------------------------------------------------------------------------*/
static uint32_t
hid_get_data_sub(const uint8_t *buf, hid_size_t len, struct hid_location *loc,
    int is_signed)
{
	uint32_t hpos = loc->pos;
	uint32_t hsize = loc->size;
	uint32_t data;
	uint32_t rpos;
	uint8_t n;

	DPRINTFN(11, "hid_get_data: loc %d/%d\n", hpos, hsize);

	/* Range check and limit */
	if (hsize == 0)
		return (0);
	if (hsize > 32)
		hsize = 32;

	/* Get data in a safe way */	
	data = 0;
	rpos = (hpos / 8);
	n = (hsize + 7) / 8;
	rpos += n;
	while (n--) {
		rpos--;
		if (rpos < len)
			data |= buf[rpos] << (8 * n);
	}

	/* Correctly shift down data */
	data = (data >> (hpos % 8));
	n = 32 - hsize;

	/* Mask and sign extend in one */
	if (is_signed != 0)
		data = (int32_t)((int32_t)data << n) >> n;
	else
		data = (uint32_t)((uint32_t)data << n) >> n;

	DPRINTFN(11, "hid_get_data: loc %d/%d = %lu\n",
	    loc->pos, loc->size, (long)data);
	return (data);
}

int32_t
hid_get_data(const uint8_t *buf, hid_size_t len, struct hid_location *loc)
{
	return (hid_get_data_sub(buf, len, loc, 1));
}

uint32_t
hid_get_udata(const uint8_t *buf, hid_size_t len, struct hid_location *loc)
{
        return (hid_get_data_sub(buf, len, loc, 0));
}

/*------------------------------------------------------------------------*
 *	hid_put_data
 *------------------------------------------------------------------------*/
void
hid_put_udata(uint8_t *buf, hid_size_t len,
    struct hid_location *loc, unsigned int value)
{
	uint32_t hpos = loc->pos;
	uint32_t hsize = loc->size;
	uint64_t data;
	uint64_t mask;
	uint32_t rpos;
	uint8_t n;

	DPRINTFN(11, "hid_put_data: loc %d/%d = %u\n", hpos, hsize, value);

	/* Range check and limit */
	if (hsize == 0)
		return;
	if (hsize > 32)
		hsize = 32;

	/* Put data in a safe way */	
	rpos = (hpos / 8);
	n = (hsize + 7) / 8;
	data = ((uint64_t)value) << (hpos % 8);
	mask = ((1ULL << hsize) - 1ULL) << (hpos % 8);
	rpos += n;
	while (n--) {
		rpos--;
		if (rpos < len) {
			buf[rpos] &= ~(mask >> (8 * n));
			buf[rpos] |= (data >> (8 * n));
		}
	}
}

/*------------------------------------------------------------------------*
 *	hid_is_collection
 *------------------------------------------------------------------------*/
int
hid_is_collection(const void *desc, hid_size_t size, int32_t usage)
{
	struct hid_data *hd;
	struct hid_item hi;
	int err;

	hd = hid_start_parse(desc, size, 0);
	if (hd == NULL)
		return (0);

	while ((err = hid_get_item(hd, &hi))) {
		 if (hi.kind == hid_collection &&
		     hi.usage == usage)
			break;
	}
	hid_end_parse(hd);
	return (err);
}

/*------------------------------------------------------------------------*
 * calculate HID item resolution. unit/mm for distances, unit/rad for angles
 *------------------------------------------------------------------------*/
int32_t
hid_item_resolution(struct hid_item *hi)
{
	/*
	 * hid unit scaling table according to HID Usage Table Review
	 * Request 39 Tbl 17 http://www.usb.org/developers/hidpage/HUTRR39b.pdf
	 */
	static const int64_t scale[0x10][2] = {
	    [0x00] = { 1, 1 },
	    [0x01] = { 1, 10 },
	    [0x02] = { 1, 100 },
	    [0x03] = { 1, 1000 },
	    [0x04] = { 1, 10000 },
	    [0x05] = { 1, 100000 },
	    [0x06] = { 1, 1000000 },
	    [0x07] = { 1, 10000000 },
	    [0x08] = { 100000000, 1 },
	    [0x09] = { 10000000, 1 },
	    [0x0A] = { 1000000, 1 },
	    [0x0B] = { 100000, 1 },
	    [0x0C] = { 10000, 1 },
	    [0x0D] = { 1000, 1 },
	    [0x0E] = { 100, 1 },
	    [0x0F] = { 10, 1 },
	};
	int64_t logical_size;
	int64_t physical_size;
	int64_t multiplier;
	int64_t divisor;
	int64_t resolution;

	switch (hi->unit) {
	case HUM_CENTIMETER:
		multiplier = 1;
		divisor = 10;
		break;
	case HUM_INCH:
	case HUM_INCH_EGALAX:
		multiplier = 10;
		divisor = 254;
		break;
	case HUM_RADIAN:
		multiplier = 1;
		divisor = 1;
		break;
	case HUM_DEGREE:
		multiplier = 573;
		divisor = 10;
		break;
	default:
		return (0);
	}

	if ((hi->logical_maximum <= hi->logical_minimum) ||
	    (hi->physical_maximum <= hi->physical_minimum) ||
	    (hi->unit_exponent < 0) || (hi->unit_exponent >= nitems(scale)))
		return (0);

	logical_size = (int64_t)hi->logical_maximum -
	    (int64_t)hi->logical_minimum;
	physical_size = (int64_t)hi->physical_maximum -
	    (int64_t)hi->physical_minimum;
	/* Round to ceiling */
	resolution = logical_size * multiplier * scale[hi->unit_exponent][0] /
	    (physical_size * divisor * scale[hi->unit_exponent][1]);

	if (resolution > INT32_MAX)
		return (0);

	return (resolution);
}

/*------------------------------------------------------------------------*
 *	hid_is_mouse
 *
 * This function will decide if a USB descriptor belongs to a USB mouse.
 *
 * Return values:
 * Zero: Not a USB mouse.
 * Else: Is a USB mouse.
 *------------------------------------------------------------------------*/
int
hid_is_mouse(const void *d_ptr, uint16_t d_len)
{
	struct hid_data *hd;
	struct hid_item hi;
	int mdepth;
	int found;

	hd = hid_start_parse(d_ptr, d_len, 1 << hid_input);
	if (hd == NULL)
		return (0);

	mdepth = 0;
	found = 0;

	while (hid_get_item(hd, &hi)) {
		switch (hi.kind) {
		case hid_collection:
			if (mdepth != 0)
				mdepth++;
			else if (hi.collection == 1 &&
			     hi.usage ==
			      HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_MOUSE))
				mdepth++;
			break;
		case hid_endcollection:
			if (mdepth != 0)
				mdepth--;
			break;
		case hid_input:
			if (mdepth == 0)
				break;
			if (hi.usage ==
			     HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_X) &&
			    (hi.flags & (HIO_CONST|HIO_RELATIVE)) == HIO_RELATIVE)
				found++;
			if (hi.usage ==
			     HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_Y) &&
			    (hi.flags & (HIO_CONST|HIO_RELATIVE)) == HIO_RELATIVE)
				found++;
			break;
		default:
			break;
		}
	}
	hid_end_parse(hd);
	return (found);
}

/*------------------------------------------------------------------------*
 *	hid_is_keyboard
 *
 * This function will decide if a USB descriptor belongs to a USB keyboard.
 *
 * Return values:
 * Zero: Not a USB keyboard.
 * Else: Is a USB keyboard.
 *------------------------------------------------------------------------*/
int
hid_is_keyboard(const void *d_ptr, uint16_t d_len)
{
	if (hid_is_collection(d_ptr, d_len,
	    HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_KEYBOARD)))
		return (1);
	return (0);
}

/*------------------------------------------------------------------------*
 *	hid_test_quirk - test a device for a given quirk
 *
 * Return values:
 * false: The HID device does not have the given quirk.
 * true: The HID device has the given quirk.
 *------------------------------------------------------------------------*/
bool
hid_test_quirk(const struct hid_device_info *dev_info, uint16_t quirk)
{
	bool found;
	uint8_t x;

	if (quirk == HQ_NONE)
		return (false);

	/* search the automatic per device quirks first */
	for (x = 0; x != HID_MAX_AUTO_QUIRK; x++) {
		if (dev_info->autoQuirk[x] == quirk)
			return (true);
	}

	/* search global quirk table, if any */
	found = (hid_test_quirk_p) (dev_info, quirk);

	return (found);
}

static bool
hid_test_quirk_w(const struct hid_device_info *dev_info, uint16_t quirk)
{
	return (false);			/* no match */
}

int
hid_add_dynamic_quirk(struct hid_device_info *dev_info, uint16_t quirk)
{
	uint8_t x;

	for (x = 0; x != HID_MAX_AUTO_QUIRK; x++) {
		if (dev_info->autoQuirk[x] == 0 ||
		    dev_info->autoQuirk[x] == quirk) {
			dev_info->autoQuirk[x] = quirk;
			return (0);     /* success */
		}
	}
	return (ENOSPC);
}

void
hid_quirk_unload(void *arg)
{
	/* reset function pointer */
	hid_test_quirk_p = &hid_test_quirk_w;
#ifdef NOT_YET
	hidquirk_ioctl_p = &hidquirk_ioctl_w;
#endif

	/* wait for CPU to exit the loaded functions, if any */

	/* XXX this is a tradeoff */

	pause("WAIT", hz);
}

int
hid_intr_start(device_t dev)
{
	return (HID_INTR_START(device_get_parent(dev), dev));
}

int
hid_intr_stop(device_t dev)
{
	return (HID_INTR_STOP(device_get_parent(dev), dev));
}

void
hid_intr_poll(device_t dev)
{
	HID_INTR_POLL(device_get_parent(dev), dev);
}

int
hid_get_rdesc(device_t dev, void *data, hid_size_t len)
{
	return (HID_GET_RDESC(device_get_parent(dev), dev, data, len));
}

int
hid_read(device_t dev, void *data, hid_size_t maxlen, hid_size_t *actlen)
{
	return (HID_READ(device_get_parent(dev), dev, data, maxlen, actlen));
}

int
hid_write(device_t dev, const void *data, hid_size_t len)
{
	return (HID_WRITE(device_get_parent(dev), dev, data, len));
}

int
hid_get_report(device_t dev, void *data, hid_size_t maxlen, hid_size_t *actlen,
    uint8_t type, uint8_t id)
{
	return (HID_GET_REPORT(device_get_parent(dev), dev, data, maxlen,
	    actlen, type, id));
}

int
hid_set_report(device_t dev, const void *data, hid_size_t len, uint8_t type,
    uint8_t id)
{
	return (HID_SET_REPORT(device_get_parent(dev), dev, data, len, type,
	    id));
}

int
hid_set_idle(device_t dev, uint16_t duration, uint8_t id)
{
	return (HID_SET_IDLE(device_get_parent(dev), dev, duration, id));
}

int
hid_set_protocol(device_t dev, uint16_t protocol)
{
	return (HID_SET_PROTOCOL(device_get_parent(dev), dev, protocol));
}

int
hid_ioctl(device_t dev, unsigned long cmd, uintptr_t data)
{
	return (HID_IOCTL(device_get_parent(dev), dev, cmd, data));
}

MODULE_VERSION(hid, 1);
