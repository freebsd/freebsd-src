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
 *	$Id: pw.c,v 1.1.1.2 1996/12/09 23:55:20 joerg Exp $
 */

#include "pw.h"

static char    *progname = "pw";

const char     *Modes[] = {"add", "del", "mod", "show", "next", NULL};
const char     *Which[] = {"user", "group", NULL};
static const char *Combo1[] = {
  "useradd", "userdel", "usermod", "usershow", "usernext",
  "groupadd", "groupdel", "groupmod", "groupshow", "groupnext",
  NULL};
static const char *Combo2[] = {
  "adduser", "deluser", "moduser", "showuser", "nextuser",
  "addgroup", "delgroup", "modgroup", "showgroup", "nextgroup",
NULL};

static struct cargs arglist;

static int      getindex(const char *words[], const char *word);
static void     cmdhelp(int mode, int which);


int
main(int argc, char *argv[])
{
	int             ch;
	int             mode = -1;
	int             which = -1;
	struct userconf *cnf;

	static const char *opts[W_NUM][M_NUM] =
	{
		{ /* user */
			"C:qn:u:c:d:e:p:g:G:mk:s:oL:i:w:h:Db:NP",
			"C:qn:u:r",
			"C:qn:u:c:d:e:p:g:G:mk:s:w:L:h:FNP",
			"C:qn:u:FPa",
			"C:q"
		},
		{ /* grp  */
			"C:qn:g:h:M:pNP",
			"C:qn:g:",
			"C:qn:g:l:h:FM:m:NP",
			"C:qn:g:FPa",
			"C:q"
		 }
	};

	static int      (*funcs[W_NUM]) (struct userconf * _cnf, int _mode, struct cargs * _args) =
	{			/* Request handlers */
		pw_user,
		pw_group
	};

	umask(0);		/* We wish to handle this manually */
	progname = strrchr(argv[0], '/');
	if (progname != NULL)
		++progname;
	else
		progname = argv[0];

	LIST_INIT(&arglist);

	/*
	 * Break off the first couple of words to determine what exactly
	 * we're being asked to do
	 */
	while (argc > 1 && *argv[1] != '-') {
		int             tmp;

		if ((tmp = getindex(Modes, argv[1])) != -1)
			mode = tmp;
		else if ((tmp = getindex(Which, argv[1])) != -1)
			which = tmp;
		else if ((tmp = getindex(Combo1, argv[1])) != -1 || (tmp = getindex(Combo2, argv[1])) != -1) {
			which = tmp / M_NUM;
			mode = tmp % M_NUM;
		} else if (strcmp(argv[1], "help") == 0)
			cmdhelp(mode, which);
		else if (which != -1 && mode != -1 && arglist.lh_first == NULL)
			addarg(&arglist, 'n', argv[1]);
		else
			cmderr(EX_USAGE, "Unknown keyword `%s'\n", argv[1]);
		++argv;
		--argc;
	}

	/*
	 * Bail out unless the user is specific!
	 */
	if (mode == -1 || which == -1)
		cmdhelp(mode, which);

	/*
	 * We know which mode we're in and what we're about to do, so now
	 * let's dispatch the remaining command line args in a genric way.
	 */
	argv[0] = progname;	/* Preserve this */
	optarg = NULL;

	while ((ch = getopt(argc, argv, opts[which][mode])) != -1) {
		if (ch == '?')
			cmderr(EX_USAGE, NULL);
		else
			addarg(&arglist, ch, optarg);
		optarg = NULL;
	}

	/*
	 * Must be root to attempt an update
	 */
	if (getuid() != 0 && mode != M_PRINT && mode != M_NEXT && getarg(&arglist, 'N')==NULL)
		cmderr(EX_NOPERM, "you must be root to run this program\n");

	/*
	 * We should immediately look for the -q 'quiet' switch so that we
	 * don't bother with extraneous errors
	 */
	if (getarg(&arglist, 'q') != NULL)
		freopen("/dev/null", "w", stderr);

	/*
	 * Now, let's do the common initialisation
	 */
	cnf = read_userconfig(getarg(&arglist, 'C') ? getarg(&arglist, 'C')->val : NULL);
	return funcs[which] (cnf, mode, &arglist);
}

static int
getindex(const char *words[], const char *word)
{
	int             i = 0;

	while (words[i]) {
		if (strcmp(words[i], word) == 0)
			return i;
		i++;
	}
	return -1;
}


/*
 * This is probably an overkill for a cmdline help system, but it reflects
 * the complexity of the command line.
 */

static void
banner(void)
{
	fprintf(stderr, "%s: ", progname);
}

void
cmderr(int ec, char const * fmt,...)
{
	if (fmt != NULL) {
		va_list         argp;

		banner();
		va_start(argp, fmt);
		vfprintf(stderr, fmt, argp);
		va_end(argp);
	}
	exit(ec);
}

static void
cmdhelp(int mode, int which)
{
	banner();
	if (which == -1)
		fprintf(stderr, "usage: %s [user|group] [add|del|mod|show|next] [ help | switches/values ]\n", progname);
	else if (mode == -1)
		fprintf(stderr, "usage: %s %s [add|del|mod|show|next] [ help | switches/values ]\n", progname, Which[which]);
	else {

		/*
		 * We need to give mode specific help
		 */
		static const char *help[W_NUM][M_NUM] =
		{
			{
				"usage: %s useradd [name] [switches]\n"
				"\t-C config      configuration file\n"
				"\t-q             quiet operation\n"
				"  Adding users:\n"
				"\t-n name        login name\n"
				"\t-u uid         user id\n"
				"\t-c comment     user name/comment\n"
				"\t-d directory   home directory\n"
				"\t-e date        account expiry date\n"
				"\t-p date        password expiry date\n"
				"\t-g grp         initial group\n"
				"\t-G grp1,grp2   additional groups\n"
				"\t-m [ -k dir ]  create and set up home\n"
				"\t-s shell       name of login shell\n"
				"\t-o             duplicate uid ok\n"
				"\t-L class       user class\n"
				"\t-h fd          read password on fd\n"
				"\t-N             no update\n"
				"  Setting defaults:\n"
				"\t-D             set user defaults\n"
				"\t-b dir         default home root dir\n"
				"\t-e period      default expiry period\n"
				"\t-p period      default password change period\n"
				"\t-g group       default group\n"
				"\t-G grp1,grp2   additional groups\n"
				"\t-L class       default user class\n"
				"\t-k dir         default home skeleton\n"
				"\t-u min,max     set min,max uids\n"
				"\t-i min,max     set min,max gids\n"
				"\t-w method      set default password method\n"
				"\t-s shell       default shell\n",
				"usage: %s userdel [uid|name] [switches]\n"
				"\t-n name        login name\n"
				"\t-u uid         user id\n"
				"\t-r             remove home & contents\n",
				"usage: %s usermod [uid|name] [switches]\n"
				"\t-C config      configuration file\n"
				"\t-q             quiet operation\n"
				"\t-F             force add if no user\n"
				"\t-n name        login name\n"
				"\t-u uid         user id\n"
				"\t-c comment     user name/comment\n"
				"\t-d directory   home directory\n"
				"\t-e date        account expiry date\n"
				"\t-p date        password expiry date\n"
				"\t-g grp         initial group\n"
				"\t-G grp1,grp2   additional groups\n"
				"\t-l name        new login name\n"
				"\t-L class       user class\n"
				"\t-m [ -k dir ]  create and set up home\n"
				"\t-s shell       name of login shell\n"
				"\t-w method      set new password using method\n"
				"\t-h fd          read password on fd\n"
				"\t-N             no update\n",
				"usage: %s usershow [uid|name] [switches]\n"
				"\t-n name        login name\n"
				"\t-u uid         user id\n"
				"\t-F             force print\n"
				"\t-P             prettier format\n"
				"\t-a             print all users\n",
				"usage: %s usernext [switches]\n"
				"\t-C config      configuration file\n"
			},
			{
				"usage: %s groupadd [group|gid] [switches]\n"
				"\t-C config      configuration file\n"
				"\t-q             quiet operation\n"
				"\t-n group       group name\n"
				"\t-g gid         group id\n"
				"\t-M usr1,usr2   add users as group members\n"
				"\t-o             duplicate gid ok\n"
				"\t-N             no update\n",
				"usage: %s groupdel [group|gid] [switches]\n"
				"\t-n name        group name\n"
				"\t-g gid         group id\n",
				"usage: %s groupmod [group|gid] [switches]\n"
				"\t-C config      configuration file\n"
				"\t-q             quiet operation\n"
				"\t-F             force add if not exists\n"
				"\t-n name        group name\n"
				"\t-g gid         group id\n"
				"\t-M usr1,usr2   replaces users as group members\n"
				"\t-m usr1,usr2   add users as group members\n"
				"\t-l name        new group name\n"
				"\t-N             no update\n",
				"usage: %s groupshow [group|gid] [switches]\n"
				"\t-n name        group name\n"
				"\t-g gid         group id\n"
				"\t-F             force print\n"
				"\t-P             prettier format\n"
				"\t-a             print all accounting groups\n",
				"usage: %s groupnext [switches]\n"
				"\t-C config      configuration file\n"
			}
		};

		fprintf(stderr, help[which][mode], progname);
	}
	exit(EXIT_FAILURE);
}

struct carg    *
getarg(struct cargs * _args, int ch)
{
	struct carg    *c = _args->lh_first;

	while (c != NULL && c->ch != ch)
		c = c->list.le_next;
	return c;
}

struct carg    *
addarg(struct cargs * _args, int ch, char *argstr)
{
	struct carg    *ca = malloc(sizeof(struct carg));

	if (ca == NULL)
		cmderr(EX_OSERR, "Abort - out of memory\n");
	ca->ch = ch;
	ca->val = argstr;
	LIST_INSERT_HEAD(_args, ca, list);
	return ca;
}
