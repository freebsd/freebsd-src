/* $Id: printf.c,v 1.3 1997/03/18 18:00:00 jj Exp $
 * printf.c:  Internal prom library printf facility.
 *
 * Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 * Copyright (C) 1997 Jakub Jelinek (jj@sunsite.mff.cuni.cz)
 */

/* This routine is internal to the prom library, no one else should know
 * about or use it!  It's simple and smelly anyway....
 */

#include <linux/kernel.h>

#include <asm/openprom.h>
#include <asm/oplib.h>

static char ppbuf[1024];

extern void prom_puts (char *, int);

void
prom_printf(char *fmt, ...)
{
	va_list args;
	char ch, *bptr, *last;
	int i;

	va_start(args, fmt);
	i = vsprintf(ppbuf, fmt, args);

	bptr = ppbuf;
	last = ppbuf;

	while((ch = *(bptr++)) != 0) {
		if(ch == '\n') {
			if (last < bptr - 1)
				prom_puts (last, bptr - 1 - last);
			prom_putchar('\r');
			last = bptr - 1;
		}
	}
	if (last < bptr - 1)
		prom_puts (last, bptr - 1 - last);
	va_end(args);
	return;
}
