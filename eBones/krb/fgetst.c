/*
 * Copyright 1987, 1988 by the Massachusetts Institute of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 *	from: fgetst.c,v 4.0 89/01/23 10:08:31 jtkohl Exp $
 *	$Id: fgetst.c,v 1.3 1995/07/18 16:38:23 mark Exp $
 */

#if 0
#ifndef lint
static char rcsid[] =
"$Id: fgetst.c,v 1.3 1995/07/18 16:38:23 mark Exp $";
#endif	/* lint */
#endif

#include <stdio.h>

/*
 * fgetst takes a file descriptor, a character pointer, and a count.
 * It reads from the file it has either read "count" characters, or
 * until it reads a null byte.  When finished, what has been read exists
 * in "s". If "count" characters were actually read, the last is changed
 * to a null, so the returned string is always null-terminated.  fgetst
 * returns the number of characters read, including the null terminator.
 */

int fgetst(FILE *f, char *s, int n)
{
    register count = n;
    int     ch;		/* NOT char; otherwise you don't see EOF */

    while ((ch = getc(f)) != EOF && ch && --count) {
	*s++ = ch;
    }
    *s = '\0';
    return (n - count);
}
