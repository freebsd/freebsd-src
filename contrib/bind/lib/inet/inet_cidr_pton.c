/*
 * Copyright (c) 1998,1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static const char rcsid[] = "$Id: inet_cidr_pton.c,v 8.4 2000/12/23 08:14:53 vixie Exp $";
#endif

#include "port_before.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <isc/assertions.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "port_after.h"

#ifdef SPRINTF_CHAR
# define SPRINTF(x) strlen(sprintf/**/x)
#else
# define SPRINTF(x) ((size_t)sprintf x)
#endif

static int	inet_cidr_pton_ipv4 __P((const char *src, u_char *dst,
					 int *bits));

/*
 * int
 * inet_cidr_pton(af, src, dst, *bits)
 *	convert network address from presentation to network format.
 *	accepts inet_pton()'s input for this "af" plus trailing "/CIDR".
 *	"dst" is assumed large enough for its "af".  "bits" is set to the
 *	/CIDR prefix length, which can have defaults (like /32 for IPv4).
 * return:
 *	-1 if an error occurred (inspect errno; ENOENT means bad format).
 *	0 if successful conversion occurred.
 * note:
 *	192.5.5.1/28 has a nonzero host part, which means it isn't a network
 *	as called for by inet_net_pton() but it can be a host address with
 *	an included netmask.
 * author:
 *	Paul Vixie (ISC), October 1998
 */
int
inet_cidr_pton(int af, const char *src, void *dst, int *bits) {
	switch (af) {
	case AF_INET:
		return (inet_cidr_pton_ipv4(src, dst, bits));
	default:
		errno = EAFNOSUPPORT;
		return (-1);
	}
}

static int
inet_cidr_pton_ipv4(const char *src, u_char *dst, int *pbits) {
	static const char digits[] = "0123456789";
	const u_char *odst = dst;
	int n, ch, tmp, bits;
	size_t size = 4;

	/* Get the mantissa. */
	while (ch = *src++, (isascii(ch) && isdigit(ch))) {
		tmp = 0;
		do {
			n = strchr(digits, ch) - digits;
			INSIST(n >= 0 && n <= 9);
			tmp *= 10;
			tmp += n;
			if (tmp > 255)
				goto enoent;
		} while ((ch = *src++) != '\0' && isascii(ch) && isdigit(ch));
		if (size-- == 0)
			goto emsgsize;
		*dst++ = (u_char) tmp;
		if (ch == '\0' || ch == '/')
			break;
		if (ch != '.')
			goto enoent;
	}

	/* Get the prefix length if any. */
	bits = -1;
	if (ch == '/' && isascii(src[0]) && isdigit(src[0]) && dst > odst) {
		/* CIDR width specifier.  Nothing can follow it. */
		ch = *src++;	/* Skip over the /. */
		bits = 0;
		do {
			n = strchr(digits, ch) - digits;
			INSIST(n >= 0 && n <= 9);
			bits *= 10;
			bits += n;
		} while ((ch = *src++) != '\0' && isascii(ch) && isdigit(ch));
		if (ch != '\0')
			goto enoent;
		if (bits > 32)
			goto emsgsize;
	}

	/* Firey death and destruction unless we prefetched EOS. */
	if (ch != '\0')
		goto enoent;

	/* Prefix length can default to /32 only if all four octets spec'd. */
	if (bits == -1) {
		if (dst - odst == 4)
			bits = 32;
		else
			goto enoent;
	}

	/* If nothing was written to the destination, we found no address. */
	if (dst == odst)
		goto enoent;

	/* If prefix length overspecifies mantissa, life is bad. */
	if ((bits / 8) > (dst - odst))
		goto enoent;

	/* Extend address to four octets. */
	while (size-- > 0)
		*dst++ = 0;

	*pbits = bits;
	return (0);

 enoent:
	errno = ENOENT;
	return (-1);

 emsgsize:
	errno = EMSGSIZE;
	return (-1);
}
