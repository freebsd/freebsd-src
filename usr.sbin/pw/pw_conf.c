/*-
 * Copyright (c) 1996 by David L. Nugent <davidn@blaze.net.au>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by David L. Nugent.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE DAVID L. NUGENT ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL DAVID L. NUGENT BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id$
 */

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
	"csh"
};

static char    *default_groups[_UC_MAXGROUPS] =
{
	NULL
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
	"/usr/share/skel",	/* Where to obtain skeleton files */
	NULL,			/* Mail to send to new accounts */
	"/var/log/userlog",	/* Where to log changes */
	"/home",		/* Where to create home directory */
	"/bin",			/* Where shells are located */
	system_shells,		/* List of shells (first is default) */
	bourne_shell,		/* Default shell */
	NULL,			/* Default group name */
	default_groups,		/* Default (additional) groups */
	NULL,			/* Default login class */
	1000, 32000,		/* Allowed range of uids */
	1000, 32000,		/* Allowed range of gids */
	0,			/* Days until account expires */
	0			/* Days until password expires */
};

static char const *comments[_UC_FIELDS] =
{
	"#\n# pw.conf - user/group configuration defaults\n#\n",
	"\n# Password for new users? no=nologin yes=loginid none=blank random=random\n",
	"\n# Reuse gaps in uid sequence? (yes or no)\n",
	"\n# Reuse gaps in gid sequence? (yes or no)\n",
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


struct userconf *
read_userconfig(char const * file)
{
	FILE           *fp;

	if (file == NULL)
		file = _PATH_PW_CONF;
	if ((fp = fopen(file, "r")) != NULL) {
		char            buf[_UC_MAXLINE];

		while (fgets(buf, sizeof buf, fp) != NULL) {
			char           *p = strchr(buf, '\n');

			if (p == NULL) {	/* Line too long */
				int             ch;

				while ((ch = fgetc(fp)) != '\n' && ch != EOF);
			} else {
				*p = '\0';
				if (*buf && *buf != '\n' && (p = strtok(buf, " \t\r\n=")) != NULL && *p != '#') {
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
						config.default_group = (q == NULL || !boolean_val(q, 1) || getgrnam(q) == NULL)
							? NULL : newstr(q);
						break;
					case _UC_EXTRAGROUPS:
						for (i = 0; i < _UC_MAXGROUPS && q != NULL; i++, q = strtok(NULL, toks))
							default_groups[i] = newstr(q);
						if (i > 0)
							while (i < _UC_MAXGROUPS)
								default_groups[i++] = NULL;
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
		}
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
			char            buf[_UC_MAXLINE];

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
					for (j = k = 0; j < _UC_MAXSHELLS && system_shells[j] != NULL; j++)
						k += sprintf(buf + k, "%s\"%s\"", k ? "," : "", system_shells[j]);
					quote = 0;
					break;
				case _UC_DEFAULTSHELL:
					val = config.shell_default ? config.shell_default : bourne_shell;
					break;
				case _UC_DEFAULTGROUP:
					val = config.default_group ? config.default_group : "";
					break;
				case _UC_EXTRAGROUPS:
					for (j = k = 0; j < _UC_MAXGROUPS && default_groups[j] != NULL; j++)
						k += sprintf(buf + k, "%s\"%s\"", k ? "," : "", default_groups[j]);
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
			return fclose(fp) != EOF;
		}
	}
	return 0;
}
