/*-
 * Copyright (c) 2004 Tim Kientzle
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <string.h>
#include "libutil.h"

static int	env_var_in_list(const char * const *list, char *var, size_t len);


/*
 * Default whitelist of "known safe" environment variables.
 */
static const char *default_whitelist[] = {
	/* List from SUS "Environment Variables" Appendix */
	"ARFLAGS", "CC", "CDPATH", "CFLAGS", "CHARSET", "COLUMNS",
	"DATEMSK", "DEAD", "EDITOR", "ENV", "EXINIT", "FC", "FCEDIT",
	"FFLAGS", "GET", "GFLAGS", "HISTFILE", "HISTORY", "HISTSIZE",
	"HOME", "IFS", "LANG", "LC_ALL", "LC_COLLATE", "LC_CTYPE",
	"LC_MESSAGES", "LC_MONETARY", "LC_NUMERIC", "LC_TIME", "LDFLAGS",
	"LEX", "LFLAGS", "LINENO", "LINES", "LISTER", "LOGNAME", "LPDEST",
	"MAIL", "MAILCHECK", "MAILER", "MAILPATH", "MAILRC", "MAKEFLAGS",
	"MAKESHELL", "MANPATH", "MBOX", "MORE", "MSGVERB", "NLSPATH",
	"NPROC", "OLDPWD", "OPTARG", "OPTERR", "OPTIND", "PAGER", "PATH",
	"PPID", "PRINTER", "PROCLANG", "PROJECTDIR", "PS1", "PS2", "PS3",
	"PS4", "PWD", "RANDOM", "SECONDS", "SHELL", "TERM", "TERMCAP",
	"TERMINFO", "TMPDIR", "TZ", "USER", "VISUAL", "YACC", "YFLAGS",

	/* Additional Environment Variables */
	"KRB5CCNAME", "LOGIN", "MAILDIR", "SSH_AGENT_PID", "SSH_AUTH_SOCK",

	/* Terminating NULL */
	NULL
};

static int
env_var_in_list(const char * const *list, char *var, size_t len)
{
	if (list == NULL)
		return (0);

	while (*list != NULL) {
		if (strncmp(var, *list, len) == 0 &&
		    len == strlen(*list))
			return (1);
		list++;
	}
	return (0);
}


/*
 * Scrub the environment by applying a "whitelist" of safe variables
 * and a "blacklist" of known dangerous variables.   Each environment
 * variable is examined and is retained only if it appears in a whitelist
 * and does not appear in the blacklist.
 *
 * If the first argument is NULL, a built-in whitelist will be used.
 * The second and third arguments allow clients to adjust the built-in
 * whitelist without having to replicate it.
 *
 */
void
clean_environment(const char * const *whitelist,
    const char * const *extra_whitelist)
{
	extern char **environ;
	char *p, **new, **old;
	int len;
	int safe;

	old = environ;
	new = environ;

	if (whitelist == NULL)
		whitelist = default_whitelist;

	while (*old != NULL) {
		safe = 0;
		p = strchr(*old, '=');
		if (p != NULL)
			len = p - *old;
		else
			len = strlen(*old);

		if (env_var_in_list(whitelist, *old, len) ||
		    env_var_in_list(extra_whitelist, *old, len))
			*new++ = *old;

		old++;
	}
	while (*new != NULL)
		*new++ = NULL;

}
