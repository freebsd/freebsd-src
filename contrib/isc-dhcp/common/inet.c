/* inet.c

   Subroutines to manipulate internet addresses in a safely portable
   way... */

/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1995-2003 by Internet Software Consortium
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
"$Id: inet.c,v 1.8.2.5 2004/06/10 17:59:18 dhankins Exp $ Copyright (c) 2004 Internet Systems Consortium.  All rights reserved.\n";
#endif /* not lint */

#include "dhcpd.h"

/* Return just the network number of an internet address... */

struct iaddr subnet_number (addr, mask)
	struct iaddr addr;
	struct iaddr mask;
{
	int i;
	struct iaddr rv;

	rv.len = 0;

	/* Both addresses must have the same length... */
	if (addr.len != mask.len)
		return rv;

	rv.len = addr.len;
	for (i = 0; i < rv.len; i++)
		rv.iabuf [i] = addr.iabuf [i] & mask.iabuf [i];
	return rv;
}

/* Combine a network number and a integer to produce an internet address.
   This won't work for subnets with more than 32 bits of host address, but
   maybe this isn't a problem. */

struct iaddr ip_addr (subnet, mask, host_address)
	struct iaddr subnet;
	struct iaddr mask;
	u_int32_t host_address;
{
	int i, j, k;
	u_int32_t swaddr;
	struct iaddr rv;
	unsigned char habuf [sizeof swaddr];

	swaddr = htonl (host_address);
	memcpy (habuf, &swaddr, sizeof swaddr);

	/* Combine the subnet address and the host address.   If
	   the host address is bigger than can fit in the subnet,
	   return a zero-length iaddr structure. */
	rv = subnet;
	j = rv.len - sizeof habuf;
	for (i = sizeof habuf - 1; i >= 0; i--) {
		if (mask.iabuf [i + j]) {
			if (habuf [i] > (mask.iabuf [i + j] ^ 0xFF)) {
				rv.len = 0;
				return rv;
			}
			for (k = i - 1; k >= 0; k--) {
				if (habuf [k]) {
					rv.len = 0;
					return rv;
				}
			}
			rv.iabuf [i + j] |= habuf [i];
			break;
		} else
			rv.iabuf [i + j] = habuf [i];
	}
		
	return rv;
}

/* Given a subnet number and netmask, return the address on that subnet
   for which the host portion of the address is all ones (the standard
   broadcast address). */

struct iaddr broadcast_addr (subnet, mask)
	struct iaddr subnet;
	struct iaddr mask;
{
	int i, j, k;
	struct iaddr rv;

	if (subnet.len != mask.len) {
		rv.len = 0;
		return rv;
	}

	for (i = 0; i < subnet.len; i++) {
		rv.iabuf [i] = subnet.iabuf [i] | (~mask.iabuf [i] & 255);
	}
	rv.len = subnet.len;

	return rv;
}

u_int32_t host_addr (addr, mask)
	struct iaddr addr;
	struct iaddr mask;
{
	int i;
	u_int32_t swaddr;
	struct iaddr rv;

	rv.len = 0;

	/* Mask out the network bits... */
	rv.len = addr.len;
	for (i = 0; i < rv.len; i++)
		rv.iabuf [i] = addr.iabuf [i] & ~mask.iabuf [i];

	/* Copy out up to 32 bits... */
	memcpy (&swaddr, &rv.iabuf [rv.len - sizeof swaddr], sizeof swaddr);

	/* Swap it and return it. */
	return ntohl (swaddr);
}

int addr_eq (addr1, addr2)
	struct iaddr addr1, addr2;
{
	if (addr1.len != addr2.len)
		return 0;
	return memcmp (addr1.iabuf, addr2.iabuf, addr1.len) == 0;
}

char *piaddr (addr)
	struct iaddr addr;
{
	static char pbuf [4 * 16];
	char *s = pbuf;
	int i;

	if (addr.len == 0) {
		strcpy (s, "<null address>");
	}
	for (i = 0; i < addr.len; i++) {
		sprintf (s, "%s%d", i ? "." : "", addr.iabuf [i]);
		s += strlen (s);
	}
	return pbuf;
}

char *piaddr1 (addr)
	struct iaddr addr;
{
	static char pbuf [4 * 16];
	char *s = pbuf;
	int i;

	if (addr.len == 0) {
		strcpy (s, "<null address>");
	}
	for (i = 0; i < addr.len; i++) {
		sprintf (s, "%s%d", i ? "." : "", addr.iabuf [i]);
		s += strlen (s);
	}
	return pbuf;
}

char *piaddrmask (struct iaddr addr, struct iaddr mask,
		  const char *file, int line)
{
	char *s, *t;
	int i, mw;
	unsigned len;

	for (i = 0; i < 32; i++) {
		if (!mask.iabuf [3 - i / 8])
			i += 7;
		else if (mask.iabuf [3 - i / 8] & (1 << (i % 8)))
			break;
	}
	mw = 32 - i;
	len = mw > 9 ? 2 : 1;
	len += 4;	/* three dots and a slash. */
	for (i = 0; i < (mw / 8) + 1; i++) {
		if (addr.iabuf [i] > 99)
			len += 3;
		else if (addr.iabuf [i] > 9)
			len += 2;
		else
			len++;
	}
	s = dmalloc (len + 1, file, line);
	if (!s)
		return s;
	t = s;
	sprintf (t, "%d", addr.iabuf [0]);
	t += strlen (t);
	for (i = 1; i < (mw / 8) + 1; i++) {
		sprintf (t, ".%d", addr.iabuf [i]);
		t += strlen (t);
	}
	*t++ = '/';
	sprintf (t, "%d", mw);
	return s;
}

