/*
 * Copyright (c) 1989, 1993, 1994
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

#include <sys/cdefs.h>

__FBSDID("$FreeBSD$");

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif

#ifndef lint
static const char sccsid[] = "@(#)calendar.c  8.3 (Berkeley) 3/25/94";
#endif

#include <err.h>
#include <errno.h>
#include <locale.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "pathnames.h"
#include "calendar.h"

struct passwd *pw;
int doall = 0;
time_t f_time = 0;

int f_dayAfter = 0; /* days after current date */
int f_dayBefore = 0; /* days before current date */

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ch;

	(void) setlocale(LC_ALL, "");

	while ((ch = getopt(argc, argv, "-af:t:A:B:")) != -1)
		switch (ch) {
		case '-':		/* backward contemptible */
		case 'a':
			if (getuid()) {
				errno = EPERM;
				err(1, NULL);
			}
			doall = 1;
			break;

			
		case 'f': /* other calendar file */
		        calendarFile = optarg;
			break;

		case 't': /* other date, undocumented, for tests */
			f_time = Mktime (optarg);
			break;

		case 'A': /* days after current date */
			f_dayAfter = atoi(optarg);
			break;

		case 'B': /* days before current date */
			f_dayBefore = atoi(optarg); 
			break;

		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc)
		usage();

	/* use current time */
	if (f_time <= 0) 
	    (void)time(&f_time);

	settime(f_time);
	
	if (doall)
		while ((pw = getpwent()) != NULL) {
			(void)setegid(pw->pw_gid);
			(void)initgroups(pw->pw_name, pw->pw_gid);
			(void)seteuid(pw->pw_uid);
			if (!chdir(pw->pw_dir))
				cal();
			(void)seteuid(0);
		}
	else
		cal();
	exit(0);
}


void
usage()
{
	(void)fprintf(stderr, 
		      "usage: calendar [-a] [-A days] [-B days] [-f calendarfile] [-t dd[.mm[.year]]]\n");
	exit(1);
}


