/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2000, 2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: lwbuffer.c,v 1.10.206.1 2004/03/06 08:15:31 marka Exp $ */

#include <config.h>

#include <string.h>

#include <lwres/lwbuffer.h>

#include "assert_p.h"

void
lwres_buffer_init(lwres_buffer_t *b, void *base, unsigned int length)
{
	/*
	 * Make 'b' refer to the 'length'-byte region starting at base.
	 */

	REQUIRE(b != NULL);

	b->magic = LWRES_BUFFER_MAGIC;
	b->base = base;
	b->length = length;
	b->used = 0;
	b->current = 0;
	b->active = 0;
}

void
lwres_buffer_invalidate(lwres_buffer_t *b)
{
	/*
	 * Make 'b' an invalid buffer.
	 */

	REQUIRE(LWRES_BUFFER_VALID(b));

	b->magic = 0;
	b->base = NULL;
	b->length = 0;
	b->used = 0;
	b->current = 0;
	b->active = 0;
}

void
lwres_buffer_add(lwres_buffer_t *b, unsigned int n)
{
	/*
	 * Increase the 'used' region of 'b' by 'n' bytes.
	 */

	REQUIRE(LWRES_BUFFER_VALID(b));
	REQUIRE(b->used + n <= b->length);

	b->used += n;
}

void
lwres_buffer_subtract(lwres_buffer_t *b, unsigned int n)
{
	/*
	 * Decrease the 'used' region of 'b' by 'n' bytes.
	 */

	REQUIRE(LWRES_BUFFER_VALID(b));
	REQUIRE(b->used >= n);

	b->used -= n;
	if (b->current > b->used)
		b->current = b->used;
	if (b->active > b->used)
		b->active = b->used;
}

void
lwres_buffer_clear(lwres_buffer_t *b)
{
	/*
	 * Make the used region empty.
	 */

	REQUIRE(LWRES_BUFFER_VALID(b));

	b->used = 0;
	b->current = 0;
	b->active = 0;
}

void
lwres_buffer_first(lwres_buffer_t *b)
{
	/*
	 * Make the consumed region empty.
	 */

	REQUIRE(LWRES_BUFFER_VALID(b));

	b->current = 0;
}

void
lwres_buffer_forward(lwres_buffer_t *b, unsigned int n)
{
	/*
	 * Increase the 'consumed' region of 'b' by 'n' bytes.
	 */

	REQUIRE(LWRES_BUFFER_VALID(b));
	REQUIRE(b->current + n <= b->used);

	b->current += n;
}

void
lwres_buffer_back(lwres_buffer_t *b, unsigned int n)
{
	/*
	 * Decrease the 'consumed' region of 'b' by 'n' bytes.
	 */

	REQUIRE(LWRES_BUFFER_VALID(b));
	REQUIRE(n <= b->current);

	b->current -= n;
}

lwres_uint8_t
lwres_buffer_getuint8(lwres_buffer_t *b)
{
	unsigned char *cp;
	lwres_uint8_t result;

	/*
	 * Read an unsigned 8-bit integer from 'b' and return it.
	 */

	REQUIRE(LWRES_BUFFER_VALID(b));
	REQUIRE(b->used - b->current >= 1);

	cp = b->base;
	cp += b->current;
	b->current += 1;
	result = ((unsigned int)(cp[0]));

	return (result);
}

void
lwres_buffer_putuint8(lwres_buffer_t *b, lwres_uint8_t val)
{
	unsigned char *cp;

	REQUIRE(LWRES_BUFFER_VALID(b));
	REQUIRE(b->used + 1 <= b->length);

	cp = b->base;
	cp += b->used;
	b->used += 1;
	cp[0] = (val & 0x00ff);
}

lwres_uint16_t
lwres_buffer_getuint16(lwres_buffer_t *b)
{
	unsigned char *cp;
	lwres_uint16_t result;

	/*
	 * Read an unsigned 16-bit integer in network byte order from 'b',
	 * convert it to host byte order, and return it.
	 */

	REQUIRE(LWRES_BUFFER_VALID(b));
	REQUIRE(b->used - b->current >= 2);

	cp = b->base;
	cp += b->current;
	b->current += 2;
	result = ((unsigned int)(cp[0])) << 8;
	result |= ((unsigned int)(cp[1]));

	return (result);
}

void
lwres_buffer_putuint16(lwres_buffer_t *b, lwres_uint16_t val)
{
	unsigned char *cp;

	REQUIRE(LWRES_BUFFER_VALID(b));
	REQUIRE(b->used + 2 <= b->length);

	cp = b->base;
	cp += b->used;
	b->used += 2;
	cp[0] = (val & 0xff00) >> 8;
	cp[1] = (val & 0x00ff);
}

lwres_uint32_t
lwres_buffer_getuint32(lwres_buffer_t *b)
{
	unsigned char *cp;
	lwres_uint32_t result;

	/*
	 * Read an unsigned 32-bit integer in network byte order from 'b',
	 * convert it to host byte order, and return it.
	 */

	REQUIRE(LWRES_BUFFER_VALID(b));
	REQUIRE(b->used - b->current >= 4);

	cp = b->base;
	cp += b->current;
	b->current += 4;
	result = ((unsigned int)(cp[0])) << 24;
	result |= ((unsigned int)(cp[1])) << 16;
	result |= ((unsigned int)(cp[2])) << 8;
	result |= ((unsigned int)(cp[3]));

	return (result);
}

void
lwres_buffer_putuint32(lwres_buffer_t *b, lwres_uint32_t val)
{
	unsigned char *cp;

	REQUIRE(LWRES_BUFFER_VALID(b));
	REQUIRE(b->used + 4 <= b->length);

	cp = b->base;
	cp += b->used;
	b->used += 4;
	cp[0] = (unsigned char)((val & 0xff000000) >> 24);
	cp[1] = (unsigned char)((val & 0x00ff0000) >> 16);
	cp[2] = (unsigned char)((val & 0x0000ff00) >> 8);
	cp[3] = (unsigned char)(val & 0x000000ff);
}

void
lwres_buffer_putmem(lwres_buffer_t *b, const unsigned char *base,
		    unsigned int length)
{
	unsigned char *cp;

	REQUIRE(LWRES_BUFFER_VALID(b));
	REQUIRE(b->used + length <= b->length);

	cp = (unsigned char *)b->base + b->used;
	memcpy(cp, base, length);
	b->used += length;
}

void
lwres_buffer_getmem(lwres_buffer_t *b, unsigned char *base,
		    unsigned int length)
{
	unsigned char *cp;

	REQUIRE(LWRES_BUFFER_VALID(b));
	REQUIRE(b->used - b->current >= length);

	cp = b->base;
	cp += b->current;
	b->current += length;

	memcpy(base, cp, length);
}
