/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */
/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)getopt.c	1.10 (gritter) 12/16/07
 */
/* from OpenSolaris "getopt.c	1.23	05/06/08 SMI" */

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/


/*
 * See getopt(3C) and SUS/XPG getopt() for function definition and
 * requirements.
 *
 * This actual implementation is a bit looser than the specification
 * as it allows any character other than ':' to be used as an option
 * character - The specification only guarantees the alnum characters
 * ([a-z][A-Z][0-9]).
 */

#include <sys/types.h>
#include <string.h>
#include <stdio.h>

extern ssize_t	write(int, const void *, size_t);

char	*optarg = NULL;
int	optind = 1;
int	opterr = 1;
int	optopt = 0;

#define	ERR(s, c)	err(s, c, optstring, argv[0])
static void
err(const char *s, int c, const char *optstring, const char *argv0)
{
	char errbuf[256], *ep = errbuf;
	const char	*cp;

	if (opterr && optstring[0] != ':') {
		for (cp = argv0; *cp && ep<&errbuf[sizeof errbuf]; cp++, ep++)
			*ep = *cp;
		for (cp = ": "; *cp && ep<&errbuf[sizeof errbuf]; cp++, ep++)
			*ep = *cp;
		for (cp = s; *cp && ep<&errbuf[sizeof errbuf]; cp++, ep++)
			*ep = *cp;
		for (cp = " -- "; *cp && ep<&errbuf[sizeof errbuf]; cp++, ep++)
			*ep = *cp;
		if (ep<&errbuf[sizeof errbuf])
			*ep++ = c;
		if (ep<&errbuf[sizeof errbuf])
			*ep++ = '\n';
		write(2, errbuf, ep - errbuf);
	}
}

/*
 * getopt_sp is required to keep state between successive calls to getopt()
 * while extracting aggregated options (ie: -abcd). Hence, getopt() is not
 * thread safe or reentrant, but it really doesn't matter.
 *
 * So, why isn't this "static" you ask?  Because the historical Bourne
 * shell has actually latched on to this little piece of private data.
 */
int getopt_sp = 1;

/*
 * Determine if the specified character (c) is present in the string
 * (optstring) as a regular, single character option. If the option is found,
 * return a pointer into optstring pointing at the option character,
 * otherwise return null. The character ':' is not allowed.
 */
static char *
parse(const char *optstring, const char c)
{
	char *cp = (char *)optstring;

	if (c == ':')
		return (NULL);
	do {
		if (*cp == c)
			return (cp);
	} while (*cp++ != '\0');
	return (NULL);
}

/*
 * External function entry point.
 */
int
getopt(int argc, char *const *argv, const char *optstring)
{
	char	c;
	char	*cp;

	/*
	 * Has the end of the options been encountered?  The following
	 * implements the SUS requirements:
	 *
	 * If, when getopt() is called:
	 *	argv[optind]	is a null pointer
	 *	*argv[optind]	is not the character '-'
	 *	argv[optind]	points to the string "-"
	 * getopt() returns -1 without changing optind. If
	 *	argv[optind]	points to the string "--"
	 * getopt() returns -1 after incrementing optind.
	 */
	if (getopt_sp == 1) {
		if (optind >= argc || argv[optind][0] != '-' ||
		    argv[optind] == NULL || argv[optind][1] == '\0')
			return (EOF);
		else if (strcmp(argv[optind], "--") == 0) {
			optind++;
			return (EOF);
		}
	}

	/*
	 * Getting this far indicates that an option has been encountered.
	 * Note that the syntax of optstring applies special meanings to
	 * the characters ':' and '(', so they are not permissible as
	 * option letters. A special meaning is also applied to the ')'
	 * character, but its meaning can be determined from context.
	 * Note that the specification only requires that the alnum
	 * characters be accepted.
	 */
	optopt = c = (unsigned char)argv[optind][getopt_sp];
	optarg = NULL;
	if ((cp = parse(optstring, c)) == NULL) {
		/* LINTED: variable format specifier */
		ERR("illegal option", c);
		if (argv[optind][++getopt_sp] == '\0') {
			optind++;
			getopt_sp = 1;
		}
		return ('?');
	}
	optopt = c = *cp;

	/*
	 * A valid option has been identified.  If it should have an
	 * option-argument, process that now.  SUS defines the setting
	 * of optarg as follows:
	 *
	 *   1.	If the option was the last character in the string pointed to
	 *	by an element of argv, then optarg contains the next element
	 *	of argv, and optind is incremented by 2. If the resulting
	 *	value of optind is not less than argc, this indicates a
	 *	missing option-argument, and getopt() returns an error
	 *	indication.
	 *
	 *   2.	Otherwise, optarg points to the string following the option
	 *	character in that element of argv, and optind is incremented
	 *	by 1.
	 *
	 * The second clause allows -abcd (where b requires an option-argument)
	 * to be interpreted as "-a -b cd".
	 */
	if (*(cp + 1) == ':') {
		/* The option takes an argument */
		if (argv[optind][getopt_sp+1] != '\0') {
			optarg = &argv[optind++][getopt_sp+1];
		} else if (++optind >= argc) {
			/* LINTED: variable format specifier */
			ERR("option requires an argument", c);
			getopt_sp = 1;
			optarg = NULL;
			return (optstring[0] == ':' ? ':' : '?');
		} else
			optarg = argv[optind++];
		getopt_sp = 1;
	} else {
		/* The option does NOT take an argument */
		if (argv[optind][++getopt_sp] == '\0') {
			getopt_sp = 1;
			optind++;
		}
		optarg = NULL;
	}
	return (c);
} /* getopt() */

#ifdef __APPLE__
/*
 * Starting with Mac OS 10.5 Leopard, <unistd.h> turns getopt()
 * into getopt$UNIX2003() by default. Consequently, this function
 * is called instead of the one defined above. However, optind is
 * still taken from this file, so in effect, options are not
 * properly handled. Defining an own getopt$UNIX2003() function
 * works around this issue.
 */
int
getopt$UNIX2003(int argc, char *const argv[], const char *optstring)
{
	return getopt(argc, argv, optstring);
}
#endif	/* __APPLE__ */
