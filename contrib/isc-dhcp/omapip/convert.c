/* convert.c

   Safe copying of option values into and out of the option buffer, which
   can't be assumed to be aligned. */

/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1996-2003 by Internet Software Consortium
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *   Internet Systems Consortium, Inc.
 *   950 Charter Street
 *   Redwood City, CA 94063
 *   <info@isc.org>
 *   http://www.isc.org/
 *
 * This software has been written for Internet Systems Consortium
 * by Ted Lemon in cooperation with Vixie Enterprises and Nominum, Inc.
 * To learn more about Internet Systems Consortium, see
 * ``http://www.isc.org/''.  To learn more about Vixie Enterprises,
 * see ``http://www.vix.com''.   To learn more about Nominum, Inc., see
 * ``http://www.nominum.com''.
 */

#ifndef lint
static char copyright[] =
"$Id: convert.c,v 1.1.2.2 2004/06/10 17:59:46 dhankins Exp $ Copyright (c) 2004 Internet Systems Consortium.  All rights reserved.\n";
#endif /* not lint */

#include <omapip/omapip_p.h>

u_int32_t getULong (buf)
	const unsigned char *buf;
{
	u_int32_t ibuf;

	memcpy (&ibuf, buf, sizeof (u_int32_t));
	return ntohl (ibuf);
}

int32_t getLong (buf)
	const unsigned char *buf;
{
	int32_t ibuf;

	memcpy (&ibuf, buf, sizeof (int32_t));
	return ntohl (ibuf);
}

u_int32_t getUShort (buf)
	const unsigned char *buf;
{
	unsigned short ibuf;

	memcpy (&ibuf, buf, sizeof (u_int16_t));
	return ntohs (ibuf);
}

int32_t getShort (buf)
	const unsigned char *buf;
{
	short ibuf;

	memcpy (&ibuf, buf, sizeof (int16_t));
	return ntohs (ibuf);
}

void putULong (obuf, val)
	unsigned char *obuf;
	u_int32_t val;
{
	u_int32_t tmp = htonl (val);
	memcpy (obuf, &tmp, sizeof tmp);
}

void putLong (obuf, val)
	unsigned char *obuf;
	int32_t val;
{
	int32_t tmp = htonl (val);
	memcpy (obuf, &tmp, sizeof tmp);
}

void putUShort (obuf, val)
	unsigned char *obuf;
	u_int32_t val;
{
	u_int16_t tmp = htons (val);
	memcpy (obuf, &tmp, sizeof tmp);
}

void putShort (obuf, val)
	unsigned char *obuf;
	int32_t val;
{
	int16_t tmp = htons (val);
	memcpy (obuf, &tmp, sizeof tmp);
}

void putUChar (obuf, val)
	unsigned char *obuf;
	u_int32_t val;
{
	*obuf = val;
}

u_int32_t getUChar (obuf)
	const unsigned char *obuf;
{
	return obuf [0];
}

int converted_length (buf, base, width)
	const unsigned char *buf;
	unsigned int base;
	unsigned int width;
{
	u_int32_t number;
	u_int32_t column;
	int power = 1;
	u_int32_t newcolumn = base;

	if (base > 16)
		return 0;

	if (width == 1)
		number = getUChar (buf);
	else if (width == 2)
		number = getUShort (buf);
	else if (width == 4)
		number = getULong (buf);

	do {
		column = newcolumn;

		if (number < column)
			return power;
		power++;
		newcolumn = column * base;
		/* If we wrap around, it must be the next power of two up. */
	} while (newcolumn > column);

	return power;
}

int binary_to_ascii (outbuf, inbuf, base, width)
	unsigned char *outbuf;
	const unsigned char *inbuf;
	unsigned int base;
	unsigned int width;
{
	u_int32_t number;
	static char h2a [] = "0123456789abcdef";
	int power = converted_length (inbuf, base, width);
	int i, j;

	if (base > 16)
		return 0;

	if (width == 1)
		number = getUChar (inbuf);
	else if (width == 2)
		number = getUShort (inbuf);
	else if (width == 4)
		number = getULong (inbuf);

	for (i = power - 1 ; i >= 0; i--) {
		outbuf [i] = h2a [number % base];
		number /= base;
	}

	return power;
}
