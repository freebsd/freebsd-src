/*
 * Copyright (c) 1980, 1993
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
static const char copyright[] =
"@(#) Copyright (c) 1980, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)lastcomm.c	8.1 (Berkeley) 6/6/93";
#endif
#endif /* not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/acct.h>

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utmp.h>
#include "pathnames.h"

/*XXX*/#include <inttypes.h>

time_t	 expand(u_int);
char	*flagbits(int);
const	 char *getdev(dev_t);
int	 requested(char *[], struct acct *);
static	 void usage(void);

#define AC_UTIME 1 /* user */
#define AC_STIME 2 /* system */
#define AC_ETIME 4 /* elapsed */
#define AC_CTIME 8 /* user + system time, default */

#define AC_BTIME 16 /* starting time */
#define AC_FTIME 32 /* exit time (starting time + elapsed time )*/

#define AC_HZ ((double)AHZ)

int
main(int argc, char *argv[])
{
	char *p;
	struct acct ab;
	struct stat sb;
	FILE *fp;
	off_t size;
	time_t t;
	int ch;
	const char *acctfile;
	int flags = 0;

	acctfile = _PATH_ACCT;
	while ((ch = getopt(argc, argv, "f:usecSE")) != -1)
		switch((char)ch) {
		case 'f':
			acctfile = optarg;
			break;

		case 'u': 
			flags |= AC_UTIME; /* user time */
			break;
		case 's':
			flags |= AC_STIME; /* system time */
			break;
		case 'e':
			flags |= AC_ETIME; /* elapsed time */
			break;
        	case 'c':
                        flags |= AC_CTIME; /* user + system time */
			break;

        	case 'S':
                        flags |= AC_BTIME; /* starting time */
			break;
        	case 'E':
			/* exit time (starting time + elapsed time )*/
                        flags |= AC_FTIME; 
			break;

		case '?':
		default:
			usage();
		}

	/* default user + system time and starting time */
	if (!flags) {
	    flags = AC_CTIME | AC_BTIME;
	}

	argc -= optind;
	argv += optind;

	/* Open the file. */
	if ((fp = fopen(acctfile, "r")) == NULL || fstat(fileno(fp), &sb))
		err(1, "could not open %s", acctfile);

	/*
	 * Round off to integral number of accounting records, probably
	 * not necessary, but it doesn't hurt.
	 */
	size = sb.st_size - sb.st_size % sizeof(struct acct);

	/* Check if any records to display. */
	if ((unsigned)size < sizeof(struct acct))
		exit(0);

	do {
		int rv;
		size -= sizeof(struct acct);
		if (fseeko(fp, size, SEEK_SET) == -1)
			err(1, "seek %s failed", acctfile);
		if ((rv = fread(&ab, sizeof(struct acct), 1, fp)) != 1)
			err(1, "read %s returned %d", acctfile, rv);

		if (ab.ac_comm[0] == '\0') {
			ab.ac_comm[0] = '?';
			ab.ac_comm[1] = '\0';
		} else
			for (p = &ab.ac_comm[0];
			    p < &ab.ac_comm[AC_COMM_LEN] && *p; ++p)
				if (!isprint(*p))
					*p = '?';
		if (*argv && !requested(argv, &ab))
			continue;

		(void)printf("%-*.*s %-7s %-*s %-*s",
			     AC_COMM_LEN, AC_COMM_LEN, ab.ac_comm,
			     flagbits(ab.ac_flag),
			     UT_NAMESIZE, user_from_uid(ab.ac_uid, 0),
			     UT_LINESIZE, getdev(ab.ac_tty));
		
		
		/* user + system time */
		if (flags & AC_CTIME) {
			(void)printf(" %6.2f secs", 
				     (expand(ab.ac_utime) + 
				      expand(ab.ac_stime))/AC_HZ);
		}
		
		/* usr time */
		if (flags & AC_UTIME) {
			(void)printf(" %6.2f us", expand(ab.ac_utime)/AC_HZ);
		}
		
		/* system time */
		if (flags & AC_STIME) {
			(void)printf(" %6.2f sy", expand(ab.ac_stime)/AC_HZ);
		}
		
		/* elapsed time */
		if (flags & AC_ETIME) {
			(void)printf(" %8.2f es", expand(ab.ac_etime)/AC_HZ);
		}
		
		/* starting time */
		if (flags & AC_BTIME) {
			(void)printf(" %.16s", ctime(&ab.ac_btime));
		}
		
		/* exit time (starting time + elapsed time )*/
		if (flags & AC_FTIME) {
			t = ab.ac_btime;
			t += (time_t)(expand(ab.ac_etime)/AC_HZ);
			(void)printf(" %.16s", ctime(&t));
		}
		printf("\n");

 	} while (size > 0);
 	exit(0);
}

time_t
expand(u_int t)
{
	time_t nt;

	nt = t & 017777;
	t >>= 13;
	while (t) {
		t--;
		nt <<= 3;
	}
	return (nt);
}

char *
flagbits(int f)
{
	static char flags[20] = "-";
	char *p;

#define	BIT(flag, ch)	if (f & flag) *p++ = ch

	p = flags + 1;
	BIT(ASU, 'S');
	BIT(AFORK, 'F');
	BIT(ACOMPAT, 'C');
	BIT(ACORE, 'D');
	BIT(AXSIG, 'X');
	*p = '\0';
	return (flags);
}

int
requested(char *argv[], struct acct *acp)
{
	const char *p;

	do {
		p = user_from_uid(acp->ac_uid, 0);
		if (!strcmp(p, *argv))
			return (1);
		if ((p = getdev(acp->ac_tty)) && !strcmp(p, *argv))
			return (1);
		if (!strncmp(acp->ac_comm, *argv, AC_COMM_LEN))
			return (1);
	} while (*++argv);
	return (0);
}

const char *
getdev(dev_t dev)
{
	static dev_t lastdev = (dev_t)-1;
	static const char *lastname;

	if (dev == NODEV)			/* Special case. */
		return ("__");
	if (dev == lastdev)			/* One-element cache. */
		return (lastname);
	lastdev = dev;
	lastname = devname(dev, S_IFCHR);
	return (lastname);
}

static void
usage(void)
{
	(void)fprintf(stderr,
"usage: lastcomm [-EScesu] [-f file] [command ...] [user ...] [terminal ...]\n");
	exit(1);
}
