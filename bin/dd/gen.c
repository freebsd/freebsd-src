/*
 * This program is in the public domain
 *
 * $FreeBSD: src/bin/dd/gen.c,v 1.1 2004/03/05 19:30:13 phk Exp $
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
