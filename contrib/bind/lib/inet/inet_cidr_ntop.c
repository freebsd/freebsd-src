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
static const char rcsid[] = "$Id: inet_cidr_ntop.c,v 8.4 1999/10/07 20:44:02 vixie Exp $";
#endif

#include "port_before.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

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

static char *	inet_cidr_ntop_ipv4 __P((const u_char *src, int bits,
					 char *dst, size_t size));

/*
 * char *
 * inet_cidr_ntop(af, src, bits, dst, size)
 *	convert network address from network to presentation format.
 *	"src"'s size is determined from its "af".
 * return:
 *	pointer to dst, or NULL if an error occurred (check errno).
 * note:
 *	192.5.5.1/28 has a nonzero host part, which means it isn't a network
 *	as called for by inet_net_ntop() but it can be a host address with
 *	an included netmask.
 * author:
 *	Paul Vixie (ISC), October 1998
 */
char *
inet_cidr_ntop(int af, const void *src, int bits, char *dst, size_t size) {
	switch (af) {
	case AF_INET:
		return (inet_cidr_ntop_ipv4(src, bits, dst, size));
	default:
		errno = EAFNOSUPPORT;
		return (NULL);
	}
}

/*
 * static char *
 * inet_cidr_ntop_ipv4(src, bits, dst, size)
 *	convert IPv4 network address from network to presentation format.
 *	"src"'s size is determined from its "af".
 * return:
 *	pointer to dst, or NULL if an error occurred (check errno).
 * note:
 *	network byte order assumed.  this means 192.5.5.240/28 has
 *	0b11110000 in its fourth octet.
 * author:
 *	Paul Vixie (ISC), October 1998
 */
static char *
inet_cidr_ntop_ipv4(const u_char *src, int bits, char *dst, size_t size) {
	char *odst = dst;
	char *t;
	size_t len = 4;
	int b, tb;

	if ((bits < -1) || (bits > 32)) {
		errno = EINVAL;
		return (NULL);
	}

	/* Find number of significant bytes in address. */
	if (bits == -1)
		len = 3;
	else
		for (len = 0,b = 1 ; b < 4; b++)
			if (*(src + b))
				len = b;

	/* Format whole octets plus nonzero trailing octets. */
	tb = (bits <= 0) ? 1 : (bits - 1);
	for (b = 0; b <= (tb / 8) || (b <= len); b++) {
		if (size < sizeof "255.")
			goto emsgsize;
		t = dst;
		dst += SPRINTF((dst, "%u", *src++));
		if (b + 1 <= (tb / 8) || (b + 1 <= len)) {
			*dst++ = '.';
			*dst = '\0';
		}
		size -= (size_t)(dst - t);
	}

	if (bits != -1) {
		/* Format CIDR /width. */
		if (size < sizeof "/32")
			goto emsgsize;
		dst += SPRINTF((dst, "/%u", bits));
	}

	return (odst);

 emsgsize:
	errno = EMSGSIZE;
	return (NULL);
}
