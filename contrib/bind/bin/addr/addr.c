#if !defined(lint) && !defined(SABER)
static const char rcsid[] = "$Id: addr.c,v 8.9 2002/05/21 02:26:21 marka Exp $";
#endif /* not lint */

/*
 * Copyright (c) 1996,1999 by Internet Software Consortium.
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

#include "port_before.h"
#include <sys/param.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "port_after.h"

static const char *prog = "addr";

#define BIGGEST_ADDRESS IN6ADDRSZ

static void
usage() {
	fprintf(stderr,
		"usage:  %s [-4] [-6] [-n hexstring] [-p address]\n",
		prog);
	exit(1);
}

/* Warning: this scribbles on `dst' even if it's going to return `0'. */
static int
hexstring(const char *src, u_char *dst, int len) {
	static const char xdigits[] = "0123456789abcdef";
	u_char *ptr = dst, *end = dst + len;
	u_int val;
	int ch, digits;

	val = 0;
	digits = 0;
	memset(dst, 0, len);
	while ((ch = *src++) != '\0') {
		if (ch == '0' && (*src == 'x' || *src == 'X')) {
			src++;
			continue;
		}
		if (isascii(ch) && (isspace(ch) || ispunct(ch))) {
			if (digits > 0) {
				if (ptr == end)
					return (0);
				*ptr++ = (u_char) (val & 0xff);
				val = 0;
				digits = 0;
			}
			digits = 0;
			continue;
		}
		if (!isascii(ch) || !isxdigit(ch))
			return (0);
		if (isupper(ch))
			ch = tolower(ch);
		/* Clock it in using little endian arithmetic. */
		val <<= 4;
		val |= (strchr(xdigits, ch) - xdigits);
		if (++digits == 2) {
			if (ptr == end)
				return (0);
			*ptr++ = (u_char) (val & 0xff);
			digits = 0;
			val = 0;
		}
	}
	if (digits > 0) {
		if (ptr == end)
			return (0);
		*ptr++ = (u_char) (val & 0xff);
	}
	return ((ptr - dst) == len);
}

static void
display(const char *input, int af, const u_char *addr, int len) {
	static int before = 0;
	char p[sizeof "xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:255.255.255.255"];
	int i;

	if (before)
		putchar('\n');
	else
		before++;

	printf("Input: \"%s\"\n", input);
	printf("Network: [af%d len%d]", af, len);
	for (i = 0; i < len; i++)
		printf(" %02x", addr[i]);
	putchar('\n');
	printf("Presentation: \"%s\"\n", inet_ntop(af, addr, p, sizeof p));
}

int
main(argc, argv)
	int argc;
	char *argv[];
{
	u_char addr[BIGGEST_ADDRESS];
	int optchr, af, len, some;

	prog = argv[0];
	af = AF_INET;
	len = INADDRSZ;
	some = 0;
	while ((optchr = getopt(argc, argv, "46n:p:")) != -1) {
		switch (optchr) {
		case '4':
			af = AF_INET;
			len = INADDRSZ;
			break;
		case '6':
			af = AF_INET6;
			len = IN6ADDRSZ;
			break;
		case 'n':
			if (!hexstring(optarg, addr, len)) {
				fprintf(stderr, "bad hex string: \"%s\"\n",
					optarg);
				usage();
				/* NOTREACHED */
			}
			display(optarg, af, addr, len);
			some++;
			break;
		case 'p':
			if (inet_pton(af, optarg, addr) <= 0) {
				fprintf(stderr, "bad address: \"%s\"\n",
					optarg);
				usage();
				/* NOTREACHED */
			}
			display(optarg, af, addr, len);
			some++;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	if (!some)
		usage();
	exit(0);
	/* NOTREACHED */
}
