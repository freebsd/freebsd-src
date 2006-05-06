/*-
 * This program is in the public domain
 *
 * $FreeBSD: src/bin/dd/gen.c,v 1.2 2005/01/10 08:39:21 imp Exp $
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
