/*-
 * Copyright (C) 1996
 *	David L. Nugent.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY DAVID L. NUGENT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL DAVID L. NUGENT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD: src/usr.sbin/pw/pw_conf.c,v 1.10.2.1 2000/07/16 01:48:12 davidn Exp $";
#endif /* not lint */

#include <string.h>
#include <ctype.h>
#include <fcntl.h>

#include "pw.h"

#define debugging 0

enum {
	_UC_NONE,
	_UC_DEFAULTPWD,
	_UC_REUSEUID,
	_UC_REUSEGID,
	_UC_NISPASSWD,
	_UC_DOTDIR,
	_UC_NEWMAIL,
	_UC_LOGFILE,
	_UC_HOMEROOT,
	_UC_SHELLPATH,
	_UC_SHELLS,
	_UC_DEFAULTSHELL,
	_UC_DEFAULTGROUP,
	_UC_EXTRAGROUPS,
	_UC_DEFAULTCLASS,
	_UC_MINUID,
	_UC_MAXUID,
	_UC_MINGID,
	_UC_MAXGID,
	_UC_EXPIRE,
	_UC_PASSWORD,
	_UC_FIELDS
};

static char     bourne_shell[] = "sh";

static char    *system_shells[_UC_MAXSHELLS] =
{
	bourne_shell,
	"csh",
	"tcsh"
};

static char const *booltrue[] =
{
	"yes", "true", "1", "on", NULL
};
static char const *boolfalse[] =
{
	"no", "false", "0", "off", NULL
};

static struct userconf config =
{
	0,			/* Default password for new users? (nologin) */
	0,			/* Reuse uids? */
	0,			/* Reuse gids? */
	NULL,			/* NIS version of the passwd file */
	"/usr/share/skel",	/* Where to obtain skeleton files */
	NULL,			/* Mail to send to new accounts */
	"/var/log/userlog",	/* Where to log changes */
	"/home",		/* Where to create home directory */
	"/bin",			/* Where shells are located */
	system_shells,		/* List of shells (first is default) */
	bourne_shell,		/* Default shell */
	NULL,			/* Default group name */
	NULL,			/* Default (additional) groups */
	NULL,			/* Default login class */
	1000, 32000,		/* Allowed range of uids */
	1000, 32000,		/* Allowed range of gids */
	0,			/* Days until account expires */
	0,			/* Days until password expires */
	0			/* size of default_group array */
};

static char const *comments[_UC_FIELDS] =
{
	"#\n# pw.conf - user/group configuration defaults\n#\n",
	"\n# Password for new users? no=nologin yes=loginid none=blank random=random\n",
	"\n# Reuse gaps in uid sequence? (yes or no)\n",
	"\n# Reuse gaps in gid sequence? (yes or no)\n",
	"\n# Path to the NIS passwd file (blank or 'no' for none)\n",
	"\n# Obtain default dotfiles from this directory\n",
	"\n# Mail this file to new user (/etc/newuser.msg or no)\n",
	"\n# Log add/change/remove information in this file\n",
	"\n# Root directory in which $HOME directory is created\n",
	"\n# Colon separated list of directories containing valid shells\n",
	"\n# Space separated list of available shells (without paths)\n",
	"\n# Default shell (without path)\n",
	"\n# Default group (leave blank for new group per user)\n",
	"\n# Extra groups for new users\n",
	"\n# Default login class for new users\n",
	"\n# Range of valid default user ids\n",
	NULL,
	"\n# Range of valid default group ids\n",
	NULL,
	"\n# Days after which account expires (0=disabled)\n",
	"\n# Days after which password expires (0=disabled)\n"
};

static char const *kwds[] =
{
	"",
	"defaultpasswd",
	"reuseuids",
	"reusegids",
	"nispasswd",
	"skeleton",
	"newmail",
	"logfile",
	"home",
	"shellpath",
	"shells",
	"defaultshell",
	"defaultgroup",
	"extragroups",
	"defaultclass",
	"minuid",
	"maxuid",
	"mingid",
	"maxgid",
	"expire_days",
	"password_days",
	NULL
};

static char    *
unquote(char const * str)
{
	if (str && (*str == '"' || *str == '\'')) {
		char           *p = strchr(str + 1, *str);

		if (p != NULL)
			*p = '\0';
		return (char *) (*++str ? str : NULL);
	}
	return (char *) str;
}

int
boolean_val(char const * str, int dflt)
{
	if ((str = unquote(str)) != NULL) {
		int             i;

		for (i = 0; booltrue[i]; i++)
			if (strcmp(str, booltrue[i]) == 0)
				return 1;
		for (i = 0; boolfalse[i]; i++)
			if (strcmp(str, boolfalse[i]) == 0)
				return 0;

		/*
		 * Special cases for defaultpassword
		 */
		if (strcmp(str, "random") == 0)
			return -1;
		if (strcmp(str, "none") == 0)
			return -2;
	}
	return dflt;
}

char const     *
boolean_str(int val)
{
	if (val == -1)
		return "random";
	else if (val == -2)
		return "none";
	else
		return val ? booltrue[0] : boolfalse[0];
}

char           *
newstr(char const * p)
{
	char           *q = NULL;

	if ((p = unquote(p)) != NULL) {
		int             l = strlen(p) + 1;

		if ((q = malloc(l)) != NULL)
			memcpy(q, p, l);
	}
	return q;
}

#define LNBUFSZ 1024


struct userconf *
read_userconfig(char const * file)
{
	FILE           *fp;

	extendarray(&config.groups, &config.numgroups, 200);
	memset(config.groups, 0, config.numgroups * sizeof(char *));
	if (file == NULL)
		file = _PATH_PW_CONF;
	if ((fp = fopen(file, "r")) != NULL) {
		int	    buflen = LNBUFSZ;
		char       *buf = malloc(buflen);

	nextline:
		while (fgets(buf, buflen, fp) != NULL) {
			char           *p;

			while ((p = strchr(buf, '\n')) == NULL) {
				int	  l;
				if (extendline(&buf, &buflen, buflen + LNBUFSZ) == -1) {
					int	ch;
					while ((ch = fgetc(fp)) != '\n' && ch != EOF);
					goto nextline;	/* Ignore it */
				}
				l = strlen(buf);
				if (fgets(buf + l, buflen - l, fp) == NULL)
					break;	/* Unterminated last line */
			}

			if (p != NULL)
				*p = '\0';

			if (*buf && (p = strtok(buf, " \t\r\n=")) != NULL && *p != '#') {
				static char const toks[] = " \t\r\n,=";
				char           *q = strtok(NULL, toks);
				int             i = 0;

				while (i < _UC_FIELDS && strcmp(p, kwds[i]) != 0)
					++i;
#if debugging
				if (i == _UC_FIELDS)
					printf("Got unknown kwd `%s' val=`%s'\n", p, q ? q : "");
				else
					printf("Got kwd[%s]=%s\n", p, q);
#endif
				switch (i) {
				case _UC_DEFAULTPWD:
					config.default_password = boolean_val(q, 1);
					break;
				case _UC_REUSEUID:
					config.reuse_uids = boolean_val(q, 0);
					break;
				case _UC_REUSEGID:
					config.reuse_gids = boolean_val(q, 0);
					break;
				case _UC_NISPASSWD:
					config.nispasswd = (q == NULL || !boolean_val(q, 1))
						? NULL : newstr(q);
					break;
				case _UC_DOTDIR:
					config.dotdir = (q == NULL || !boolean_val(q, 1))
						? NULL : newstr(q);
					break;
				case _UC_NEWMAIL:
					config.newmail = (q == NULL || !boolean_val(q, 1))
						? NULL : newstr(q);
					break;
				case _UC_LOGFILE:
					config.logfile = (q == NULL || !boolean_val(q, 1))
						? NULL : newstr(q);
					break;
				case _UC_HOMEROOT:
					config.home = (q == NULL || !boolean_val(q, 1))
						? "/home" : newstr(q);
					break;
				case _UC_SHELLPATH:
					config.shelldir = (q == NULL || !boolean_val(q, 1))
						? "/bin" : newstr(q);
					break;
				case _UC_SHELLS:
					for (i = 0; i < _UC_MAXSHELLS && q != NULL; i++, q = strtok(NULL, toks))
						system_shells[i] = newstr(q);
					if (i > 0)
						while (i < _UC_MAXSHELLS)
							system_shells[i++] = NULL;
					break;
				case _UC_DEFAULTSHELL:
					config.shell_default = (q == NULL || !boolean_val(q, 1))
						? (char *) bourne_shell : newstr(q);
					break;
				case _UC_DEFAULTGROUP:
					q = unquote(q);
					config.default_group = (q == NULL || !boolean_val(q, 1) || GETGRNAM(q) == NULL)
						? NULL : newstr(q);
					break;
				case _UC_EXTRAGROUPS:
					for (i = 0; q != NULL; q = strtok(NULL, toks)) {
						if (extendarray(&config.groups, &config.numgroups, i + 2) != -1)
							config.groups[i++] = newstr(q);
					}
					if (i > 0)
						while (i < config.numgroups)
							config.groups[i++] = NULL;
					break;
				case _UC_DEFAULTCLASS:
					config.default_class = (q == NULL || !boolean_val(q, 1))
						? NULL : newstr(q);
					break;
				case _UC_MINUID:
					if ((q = unquote(q)) != NULL && isdigit(*q))
						config.min_uid = (uid_t) atol(q);
					break;
				case _UC_MAXUID:
					if ((q = unquote(q)) != NULL && isdigit(*q))
						config.max_uid = (uid_t) atol(q);
					break;
				case _UC_MINGID:
					if ((q = unquote(q)) != NULL && isdigit(*q))
						config.min_gid = (gid_t) atol(q);
					break;
				case _UC_MAXGID:
					if ((q = unquote(q)) != NULL && isdigit(*q))
						config.max_gid = (gid_t) atol(q);
					break;
				case _UC_EXPIRE:
					if ((q = unquote(q)) != NULL && isdigit(*q))
						config.expire_days = atoi(q);
					break;
				case _UC_PASSWORD:
					if ((q = unquote(q)) != NULL && isdigit(*q))
						config.password_days = atoi(q);
					break;
				case _UC_FIELDS:
				case _UC_NONE:
					break;
				}
			}
		}
		free(buf);
		fclose(fp);
	}
	return &config;
}


int
write_userconfig(char const * file)
{
	int             fd;

	if (file == NULL)
		file = _PATH_PW_CONF;

	if ((fd = open(file, O_CREAT | O_RDWR | O_TRUNC | O_EXLOCK, 0644)) != -1) {
		FILE           *fp;

		if ((fp = fdopen(fd, "w")) == NULL)
			close(fd);
		else {
			int             i, j, k;
			int		len = LNBUFSZ;
			char           *buf = malloc(len);

			for (i = _UC_NONE; i < _UC_FIELDS; i++) {
				int             quote = 1;
				char const     *val = buf;

				*buf = '\0';
				switch (i) {
				case _UC_DEFAULTPWD:
					val = boolean_str(config.default_password);
					break;
				case _UC_REUSEUID:
					val = boolean_str(config.reuse_uids);
					break;
				case _UC_REUSEGID:
					val = boolean_str(config.reuse_gids);
					break;
				case _UC_NISPASSWD:
					val = config.nispasswd ? config.nispasswd : "";
					quote = 0;
					break;
				case _UC_DOTDIR:
					val = config.dotdir ? config.dotdir : boolean_str(0);
					break;
				case _UC_NEWMAIL:
					val = config.newmail ? config.newmail : boolean_str(0);
					break;
				case _UC_LOGFILE:
					val = config.logfile ? config.logfile : boolean_str(0);
					break;
				case _UC_HOMEROOT:
					val = config.home;
					break;
				case _UC_SHELLPATH:
					val = config.shelldir;
					break;
				case _UC_SHELLS:
					for (j = k = 0; j < _UC_MAXSHELLS && system_shells[j] != NULL; j++) {
						char	lbuf[64];
						int	l = snprintf(lbuf, sizeof lbuf, "%s\"%s\"", k ? "," : "", system_shells[j]);
						if (l + k + 1 < len || extendline(&buf, &len, len + LNBUFSZ) != -1) {
							strcpy(buf + k, lbuf);
							k += l;
						}
					}
					quote = 0;
					break;
				case _UC_DEFAULTSHELL:
					val = config.shell_default ? config.shell_default : bourne_shell;
					break;
				case _UC_DEFAULTGROUP:
					val = config.default_group ? config.default_group : "";
					break;
				case _UC_EXTRAGROUPS:
					extendarray(&config.groups, &config.numgroups, 200);
					for (j = k = 0; j < config.numgroups && config.groups[j] != NULL; j++) {
						char	lbuf[64];
						int	l = snprintf(lbuf, sizeof lbuf, "%s\"%s\"", k ? "," : "", config.groups[j]);
						if (l + k + 1 < len || extendline(&buf, &len, len + 1024) != -1) {
							strcpy(buf + k, lbuf);
							k +=  l;
						}
					}
					quote = 0;
					break;
				case _UC_DEFAULTCLASS:
					val = config.default_class ? config.default_class : "";
					break;
				case _UC_MINUID:
					sprintf(buf, "%lu", (unsigned long) config.min_uid);
					quote = 0;
					break;
				case _UC_MAXUID:
					sprintf(buf, "%lu", (unsigned long) config.max_uid);
					quote = 0;
					break;
				case _UC_MINGID:
					sprintf(buf, "%lu", (unsigned long) config.min_gid);
					quote = 0;
					break;
				case _UC_MAXGID:
					sprintf(buf, "%lu", (unsigned long) config.max_gid);
					quote = 0;
					break;
				case _UC_EXPIRE:
					sprintf(buf, "%d", config.expire_days);
					quote = 0;
					break;
				case _UC_PASSWORD:
					sprintf(buf, "%d", config.password_days);
					quote = 0;
					break;
				case _UC_NONE:
					break;
				}

				if (comments[i])
					fputs(comments[i], fp);

				if (*kwds[i]) {
					if (quote)
						fprintf(fp, "%s = \"%s\"\n", kwds[i], val);
					else
						fprintf(fp, "%s = %s\n", kwds[i], val);
#if debugging
					printf("WROTE: %s = %s\n", kwds[i], val);
#endif
				}
			}
			free(buf);
			return fclose(fp) != EOF;
		}
	}
	return 0;
}
