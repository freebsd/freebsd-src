/*
 * Copyright (c) 1983, 1995 Eric P. Allman
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char sccsid[] = "@(#)arpadate.c	8.4 (Berkeley) 4/21/95";
#endif /* not lint */

# include "sendmail.h"

/*
**  ARPADATE -- Create date in ARPANET format
**
**	Parameters:
**		ud -- unix style date string.  if NULL, one is created.
**
**	Returns:
**		pointer to an ARPANET date field
**
**	Side Effects:
**		none
**
**	WARNING:
**		date is stored in a local buffer -- subsequent
**		calls will overwrite.
**
**	Bugs:
**		Timezone is computed from local time, rather than
**		from whereever (and whenever) the message was sent.
**		To do better is very hard.
**
**		Some sites are now inserting the timezone into the
**		local date.  This routine should figure out what
**		the format is and work appropriately.
*/

#ifndef TZNAME_MAX
# define TZNAME_MAX	50	/* max size of timezone */
#endif

/* values for TZ_TYPE */
#define TZ_NONE		0	/* no character timezone support */
#define TZ_TM_NAME	1	/* use tm->tm_name */
#define TZ_TM_ZONE	2	/* use tm->tm_zone */
#define TZ_TZNAME	3	/* use tzname[] */
#define TZ_TIMEZONE	4	/* use timezone() */

char *
arpadate(ud)
	register char *ud;
{
	register char *p;
	register char *q;
	register int off;
	register int i;
	register struct tm *lt;
	time_t t;
	struct tm gmt;
	char *tz;
	static char b[43 + TZNAME_MAX];

	/*
	**  Get current time.
	**	This will be used if a null argument is passed and
	**	to resolve the timezone.
	*/

	(void) time(&t);
	if (ud == NULL)
		ud = ctime(&t);

	/*
	**  Crack the UNIX date line in a singularly unoriginal way.
	*/

	q = b;

	p = &ud[0];		/* Mon */
	*q++ = *p++;
	*q++ = *p++;
	*q++ = *p++;
	*q++ = ',';
	*q++ = ' ';

	p = &ud[8];		/* 16 */
	if (*p == ' ')
		p++;
	else
		*q++ = *p++;
	*q++ = *p++;
	*q++ = ' ';

	p = &ud[4];		/* Sep */
	*q++ = *p++;
	*q++ = *p++;
	*q++ = *p++;
	*q++ = ' ';

	p = &ud[20];		/* 1979 */
	*q++ = *p++;
	*q++ = *p++;
	*q++ = *p++;
	*q++ = *p++;
	*q++ = ' ';

	p = &ud[11];		/* 01:03:52 */
	for (i = 8; i > 0; i--)
		*q++ = *p++;

	/*
	 * should really get the timezone from the time in "ud" (which
	 * is only different if a non-null arg was passed which is different
	 * from the current time), but for all practical purposes, returning
	 * the current local zone will do (its all that is ever needed).
	 */
	gmt = *gmtime(&t);
	lt = localtime(&t);

	off = (lt->tm_hour - gmt.tm_hour) * 60 + lt->tm_min - gmt.tm_min;

	/* assume that offset isn't more than a day ... */
	if (lt->tm_year < gmt.tm_year)
		off -= 24 * 60;
	else if (lt->tm_year > gmt.tm_year)
		off += 24 * 60;
	else if (lt->tm_yday < gmt.tm_yday)
		off -= 24 * 60;
	else if (lt->tm_yday > gmt.tm_yday)
		off += 24 * 60;

	*q++ = ' ';
	if (off == 0)
	{
		*q++ = 'G';
		*q++ = 'M';
		*q++ = 'T';
	}
	else
	{
		tz = NULL;
#if TZ_TYPE == TZ_TM_NAME
		tz = lt->tm_name;
#endif
#if TZ_TYPE == TZ_TM_ZONE
		tz = lt->tm_zone;
#endif
#if TZ_TYPE == TZ_TZNAME
		{
			extern char *tzname[];

			tz = tzname[lt->tm_isdst];
		}
#endif
#if TZ_TYPE == TZ_TIMEZONE
		{
			extern char *timezone();

			tz = timezone(off, lt->tm_isdst);
		}
#endif
		if (off < 0)
		{
			off = -off;
			*q++ = '-';
		}
		else
			*q++ = '+';

		if (off >= 24*60)		/* should be impossible */
			off = 23*60+59;		/* if not, insert silly value */

		*q++ = (off / 600) + '0';
		*q++ = (off / 60) % 10 + '0';
		off %= 60;
		*q++ = (off / 10) + '0';
		*q++ = (off % 10) + '0';
		if (tz != NULL && *tz != '\0')
		{
			*q++ = ' ';
			*q++ = '(';
			while (*tz != '\0')
				*q++ = *tz++;
			*q++ = ')';
		}
	}
	*q = '\0';

	return (b);
}
