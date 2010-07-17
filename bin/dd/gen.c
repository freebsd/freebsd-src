/*-
 * This program is in the public domain
 *
 * $FreeBSD: src/bin/dd/gen.c,v 1.2.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $
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
