/*-
 * Copyright (c) 1990, 1993, 1994
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
#if 0
static char sccsid[] = "@(#)pr_time.c	8.2 (Berkeley) 4/4/94";
#endif
static const char rcsid[] =
	"$Id: pr_time.c,v 1.9 1997/08/25 06:42:18 charnier Exp $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/time.h>

#include <stdio.h>
#include <string.h>

#include "extern.h"

/*
 * pr_attime --
 *	Print the time since the user logged in.
 *
 *	Note: SCCS forces the bizarre string manipulation, things like
 *	8.2 get replaced in the source code.
 */
void
pr_attime(started, now)
	time_t *started, *now;
{
	static char buf[256];
	struct tm *tp;
	time_t diff;
	char fmt[20];

	tp = localtime(started);
	diff = *now - *started;

	/* If more than a week, use day-month-year. */
	if (diff > 86400 * 7)
		(void)strcpy(fmt, "%d%b%y");

	/* If not today, use day-hour-am/pm. */
	else if (*now / 86400 != *started / 86400) {
		(void)strcpy(fmt, __CONCAT("%a%", "I%p"));
	}

	/* Default is hh:mm{am,pm}. */
	else {
		(void)strcpy(fmt, __CONCAT("%l:%", "M%p"));
	}

	(void)strftime(buf, sizeof(buf) - 1, fmt, tp);
	buf[sizeof(buf) - 1] = '\0';
	(void)printf("%s", buf);
}

/*
 * pr_idle --
 *	Display the idle time.
 */
int
pr_idle(idle)
	time_t idle;
{
	/* If idle more than 36 hours, print as a number of days. */
	if (idle >= 36 * 3600) {
		int days = idle / 86400;
		(void)printf(" %dday%s ", days, days > 1 ? "s" : " " );
		if (days >= 10)
			return(1);
	}

	/* If idle more than an hour, print as HH:MM. */
	else if (idle >= 3600)
		(void)printf(" %2d:%02d ",
		    idle / 3600, (idle % 3600) / 60);

	else if (idle / 60 == 0)
		(void)printf("     - ");

	/* Else print the minutes idle. */
	else
		(void)printf("    %2d ", idle / 60);

	return(0); /* not idle longer than 9 days */
}
