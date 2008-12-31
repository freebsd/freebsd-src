/*-
 * This program is in the public domain
 *
 * $FreeBSD: src/bin/dd/gen.c,v 1.2.18.1 2008/11/25 02:59:29 kensmith Exp $
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
