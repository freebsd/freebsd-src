/*-
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "cpio_platform.h"
__FBSDID("$FreeBSD$");

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_GETOPT_LONG
#include <getopt.h>
#else
struct option {
	const char *name;
	int has_arg;
	int *flag;
	int val;
};
#define	no_argument 0
#define	required_argument 1
#endif
#ifdef HAVE_GRP_H
#include <grp.h>
#endif
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "cpio.h"

/*
 *
 * Option parsing routines for bsdcpio.
 *
 */


static const char *cpio_opts = "AaBC:F:O:cdE:f:H:hijLlmopR:rtuvW:yZz";

/*
 * On systems that lack getopt_long, long options can be specified
 * using -W longopt and -W longopt=value, e.g. "-W version" is the
 * same as "--version" and "-W format=ustar" is the same as "--format
 * ustar".  This does not rely the GNU getopt() "W;" extension, so
 * should work correctly on any system with a POSIX-compliant
 * getopt().
 */

/*
 * If you add anything, be very careful to keep this list properly
 * sorted, as the -W logic below relies on it.
 */
static const struct option cpio_longopts[] = {
	{ "create",		no_argument,	   NULL, 'o' },
	{ "extract",		no_argument,       NULL, 'i' },
	{ "file",		required_argument, NULL, 'F' },
	{ "format",             required_argument, NULL, 'H' },
	{ "help",		no_argument,	   NULL, 'h' },
	{ "insecure",		no_argument,	   NULL, OPTION_INSECURE },
	{ "link",		no_argument,	   NULL, 'l' },
	{ "list",		no_argument,	   NULL, 't' },
	{ "make-directories",	no_argument,	   NULL, 'd' },
	{ "owner",		required_argument, NULL, 'R' },
	{ "pass-through",	no_argument,	   NULL, 'p' },
	{ "preserve-modification-time", no_argument, NULL, 'm' },
	{ "quiet",		no_argument,	   NULL, OPTION_QUIET },
	{ "unconditional",	no_argument,	   NULL, 'u' },
	{ "verbose",            no_argument,       NULL, 'v' },
	{ "version",            no_argument,       NULL, OPTION_VERSION },
	{ NULL, 0, NULL, 0 }
};

/*
 * Parse command-line options using system-provided getopt() or getopt_long().
 * If option is -W, then parse argument as a long option.
 */
int
cpio_getopt(struct cpio *cpio)
{
	char *p, *q;
	const struct option *option, *option2;
	int opt;
	int option_index;
	size_t option_length;

	option_index = -1;

#ifdef HAVE_GETOPT_LONG
	opt = getopt_long(cpio->argc, cpio->argv, cpio_opts,
	    cpio_longopts, &option_index);
#else
	opt = getopt(cpio->argc, cpio->argv, cpio_opts);
#endif

	/* Support long options through -W longopt=value */
	if (opt == 'W') {
		p = optarg;
		q = strchr(optarg, '=');
		if (q != NULL) {
			option_length = (size_t)(q - p);
			optarg = q + 1;
		} else {
			option_length = strlen(p);
			optarg = NULL;
		}
		option = cpio_longopts;
		while (option->name != NULL &&
		    (strlen(option->name) < option_length ||
		    strncmp(p, option->name, option_length) != 0 )) {
			option++;
		}

		if (option->name != NULL) {
			option2 = option;
			opt = option->val;

			/* If the first match was exact, we're done. */
			if (strncmp(p, option->name, strlen(option->name)) == 0) {
				while (option->name != NULL)
					option++;
			} else {
				/* Check if there's another match. */
				option++;
				while (option->name != NULL &&
				    (strlen(option->name) < option_length ||
				    strncmp(p, option->name, option_length) != 0)) {
					option++;
				}
			}
			if (option->name != NULL)
				cpio_errc(1, 0,
				    "Ambiguous option %s "
				    "(matches both %s and %s)",
				    p, option2->name, option->name);

			if (option2->has_arg == required_argument
			    && optarg == NULL)
				cpio_errc(1, 0,
				    "Option \"%s\" requires argument", p);
		} else {
			opt = '?';
		}
	}

	return (opt);
}


/*
 * Parse the argument to the -R or --owner flag.
 *
 * The format is one of the following:
 *   <user>    - Override user but not group
 *   <user>:   - Override both, group is user's default group
 *   <user>:<group> - Override both
 *   :<group>  - Override group but not user
 *
 * A period can be used instead of the colon.
 *
 * Sets uid/gid as appropriate, -1 indicates uid/gid not specified.
 *
 */
int
owner_parse(const char *spec, int *uid, int *gid)
{
	const char *u, *ue, *g;

	*uid = -1;
	*gid = -1;

	/*
	 * Split spec into [user][:.][group]
	 *  u -> first char of username, NULL if no username
	 *  ue -> first char after username (colon, period, or \0)
	 *  g -> first char of group name
	 */
	if (*spec == ':' || *spec == '.') {
		/* If spec starts with ':' or '.', then just group. */
		ue = u = NULL;
		g = spec + 1;
	} else {
		/* Otherwise, [user] or [user][:] or [user][:][group] */
		ue = u = spec;
		while (*ue != ':' && *ue != '.' && *ue != '\0')
			++ue;
		g = ue;
		if (*g != '\0') /* Skip : or . to find first char of group. */
			++g;
	}

	if (u != NULL) {
		/* Look up user: ue is first char after end of user. */
		char *user;
		struct passwd *pwent;

		user = (char *)malloc(ue - u + 1);
		if (user == NULL) {
			cpio_warnc(errno, "Couldn't allocate memory");
			return (1);
		}
		memcpy(user, u, ue - u);
		user[ue - u] = '\0';
		pwent = getpwnam(user);
		if (pwent == NULL) {
			cpio_warnc(errno, "Couldn't lookup user ``%s''", user);
			return (1);
		}
		free(user);
		*uid = pwent->pw_uid;
		if (*ue != '\0' && *g == '\0')
			*gid = pwent->pw_gid;
	}
	if (*g != '\0') {
		struct group *grp;
		grp = getgrnam(g);
		if (grp != NULL)
			*gid = grp->gr_gid;
		else {
			cpio_warnc(errno, "Couldn't look up group ``%s''", g);
			return (1);
		}
	}
	return (0);
}
