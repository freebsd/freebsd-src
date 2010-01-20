/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#if 0
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: l64a.c,v 1.13 2003/07/26 19:24:54 salo Exp $");
#endif /* not lint */
#endif

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stdlib.h>

#define	ADOT	46		/* ASCII '.' */
#define	ASLASH	ADOT + 1	/* ASCII '/' */
#define	A0	48		/* ASCII '0' */
#define	AA	65		/* ASCII 'A' */
#define	Aa	97		/* ASCII 'a' */

char *
l64a(long value)
{
	static char buf[8];

	(void)l64a_r(value, buf, sizeof(buf));
	return (buf);
}

int
l64a_r(long value, char *buffer, int buflen)
{
	long v;
	int digit;

	v = value & (long)0xffffffff;
	for (; v != 0 && buflen > 1; buffer++, buflen--) {
		digit = v & 0x3f;
		if (digit < 2)
			*buffer = digit + ADOT;
		else if (digit < 12)
			*buffer = digit + A0 - 2;
		else if (digit < 38)
			*buffer = digit + AA - 12;
		else
			*buffer = digit + Aa - 38;
		v >>= 6;
	}
	return (v == 0 ? 0 : -1);
}
