/* convert.c

   Safe copying of option values into and out of the option buffer, which
   can't be assumed to be aligned. */

/*
 * Copyright (c) 1996-1999 Internet Software Consortium.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of The Internet Software Consortium nor the names
 *    of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INTERNET SOFTWARE CONSORTIUM AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNET SOFTWARE CONSORTIUM OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This software has been written for the Internet Software Consortium
 * by Ted Lemon in cooperation with Vixie Enterprises and Nominum, Inc.
 * To learn more about the Internet Software Consortium, see
 * ``http://www.isc.org/''.  To learn more about Vixie Enterprises,
 * see ``http://www.vix.com''.   To learn more about Nominum, Inc., see
 * ``http://www.nominum.com''.
 */

#ifndef lint
static char copyright[] =
"$Id: convert.c,v 1.1 2000/08/01 22:34:36 neild Exp $ Copyright (c) 1996-1999 The Internet Software Consortium.  All rights reserved.\n";
#endif /* not lint */

#include <omapip/omapip_p.h>

u_int32_t getULong (buf)
	const unsigned char *buf;
{
	unsigned long ibuf;

	memcpy (&ibuf, buf, sizeof (u_int32_t));
	return ntohl (ibuf);
}

int32_t getLong (buf)
	const unsigned char *buf;
{
	long ibuf;

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
