/*-
 * This program is in the public domain
 *
 * $FreeBSD: src/bin/dd/gen.c,v 1.2.16.1 2008/10/02 02:57:24 kensmith Exp $
 */

#include <stdio.h>

int
main(int argc __unused, char **argv __unused)
{
	int i;

	for (i = 0; i < 256; i++)
		putchar(i);
	return (0);
}
