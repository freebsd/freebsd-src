/*
 * Copyright (c) 1998 Sendmail, Inc.  All rights reserved.
 * Copyright (c) 1983, 1995-1997 Eric P. Allman.  All rights reserved.
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#ifndef lint
static char sccsid[] = "@(#)trace.c	8.12 (Berkeley) 5/19/1998";
#endif /* not lint */

# include "sendmail.h"

/*
**  TtSETUP -- set up for trace package.
**
**	Parameters:
**		vect -- pointer to trace vector.
**		size -- number of flags in trace vector.
**		defflags -- flags to set if no value given.
**
**	Returns:
**		none
**
**	Side Effects:
**		environment is set up.
*/

u_char		*tTvect;
int		tTsize;
static char	*DefFlags;

void
tTsetup(vect, size, defflags)
	u_char *vect;
	int size;
	char *defflags;
{
	tTvect = vect;
	tTsize = size;
	DefFlags = defflags;
}
/*
**  TtFLAG -- process an external trace flag description.
**
**	Parameters:
**		s -- the trace flag.
**
**	Returns:
**		none.
**
**	Side Effects:
**		sets/clears trace flags.
*/

void
tTflag(s)
	register char *s;
{
	unsigned int first, last;
	register unsigned int i;

	if (*s == '\0')
		s = DefFlags;

	for (;;)
	{
		/* find first flag to set */
		i = 0;
		while (isascii(*s) && isdigit(*s))
			i = i * 10 + (*s++ - '0');
		first = i;

		/* find last flag to set */
		if (*s == '-')
		{
			i = 0;
			while (isascii(*++s) && isdigit(*s))
				i = i * 10 + (*s - '0');
		}
		last = i;

		/* find the level to set it to */
		i = 1;
		if (*s == '.')
		{
			i = 0;
			while (isascii(*++s) && isdigit(*s))
				i = i * 10 + (*s - '0');
		}

		/* clean up args */
		if (first >= tTsize)
			first = tTsize - 1;
		if (last >= tTsize)
			last = tTsize - 1;

		/* set the flags */
		while (first <= last)
			tTvect[first++] = i;

		/* more arguments? */
		if (*s++ == '\0')
			return;
	}
}
