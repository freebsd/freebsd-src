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
  "$FreeBSD$";
#endif /* not lint */

#include <err.h>
#include <fcntl.h>
#include <locale.h>
#include <paths.h>
#include <stdbool.h>
#include <sys/wait.h>
#include "pw.h"

#if !defined(_PATH_YP)
#define	_PATH_YP	"/var/yp/"
#endif
const char     *Modes[] = {
  "add", "del", "mod", "show", "next",
  NULL};
const char     *Which[] = {"user", "group", NULL};
static const char *Combo1[] = {
  "useradd", "userdel", "usermod", "usershow", "usernext",
  "lock", "unlock",
  "groupadd", "groupdel", "groupmod", "groupshow", "groupnext",
  NULL};
static const char *Combo2[] = {
  "adduser", "deluser", "moduser", "showuser", "nextuser",
  "lock", "unlock",
  "addgroup", "delgroup", "modgroup", "showgroup", "nextgroup",
  NULL};

struct pwf PWF =
{
	PWF_REGULAR,
	setpwent,
	endpwent,
	getpwent,
	getpwuid,
	getpwnam,
	setgrent,
	endgrent,
	getgrent,
	getgrgid,
	getgrnam,

};
struct pwf VPWF =
{
	PWF_ALT,
	vsetpwent,
	vendpwent,
	vgetpwent,
	vgetpwuid,
	vgetpwnam,
	vsetgrent,
	vendgrent,
	vgetgrent,
	vgetgrgid,
	vgetgrnam,
};

struct pwconf conf;

static struct cargs arglist;

static int      getindex(const char *words[], const char *word);
static void     cmdhelp(int mode, int which);


int
main(int argc, char *argv[])
{
	int             ch;
	int             mode = -1;
	int             which = -1;
	long		id = -1;
	char		*config = NULL;
	struct stat	st;
	const char	*errstr;
	char		arg, *name;
	bool		relocated, nis;

	static const char *opts[W_NUM][M_NUM] =
	{
		{ /* user */
			"R:V:C:qn:u:c:d:e:p:g:G:mM:k:s:oL:i:w:h:H:Db:NPy:Y",
			"R:V:C:qn:u:rY",
			"R:V:C:qn:u:c:d:e:p:g:G:mM:l:k:s:w:L:h:H:FNPY",
			"R:V:C:qn:u:FPa7",
			"R:V:C:q",
			"R:V:C:q",
			"R:V:C:q"
		},
		{ /* grp  */
			"R:V:C:qn:g:h:H:M:opNPY",
			"R:V:C:qn:g:Y",
			"R:V:C:qn:d:g:l:h:H:FM:m:NPY",
			"R:V:C:qn:g:FPa",
			"R:V:C:q"
		 }
	};

	static int      (*funcs[W_NUM]) (int _mode, char *_name, long _id,
	    struct cargs * _args) =
	{			/* Request handlers */
		pw_user,
		pw_group
	};

	name = NULL;
	relocated = nis = false;
	memset(&conf, 0, sizeof(conf));
	strlcpy(conf.rootdir, "/", sizeof(conf.rootdir));
	strlcpy(conf.etcpath, _PATH_PWD, sizeof(conf.etcpath));
	conf.fd = -1;

	LIST_INIT(&arglist);

	(void)setlocale(LC_ALL, "");

	/*
	 * Break off the first couple of words to determine what exactly
	 * we're being asked to do
	 */
	while (argc > 1) {
		int             tmp;

		if (*argv[1] == '-') {
			/*
			 * Special case, allow pw -V<dir> <operation> [args] for scripts etc.
			 */
			arg = argv[1][1];
			if (arg == 'V' || arg == 'R') {
				if (relocated)
					errx(EXIT_FAILURE, "Both '-R' and '-V' "
					    "specified, only one accepted");
				relocated = true;
				optarg = &argv[1][2];
				if (*optarg == '\0') {
					if (stat(argv[2], &st) != 0)
						errx(EX_OSFILE, \
						    "no such directory `%s'",
						    argv[2]);
					if (!S_ISDIR(st.st_mode))
						errx(EX_OSFILE, "`%s' not a "
						    "directory", argv[2]);
					optarg = argv[2];
					++argv;
					--argc;
				}
				memcpy(&PWF, &VPWF, sizeof PWF);
				if (arg == 'R') {
					strlcpy(conf.rootdir, optarg,
					    sizeof(conf.rootdir));
					PWF._altdir = PWF_ROOTDIR;
				}
				snprintf(conf.etcpath, sizeof(conf.etcpath),
				    "%s%s", optarg, arg == 'R' ? "/etc" : "");
			} else
				break;
		}
		else if (mode == -1 && (tmp = getindex(Modes, argv[1])) != -1)
			mode = tmp;
		else if (which == -1 && (tmp = getindex(Which, argv[1])) != -1)
			which = tmp;
		else if ((mode == -1 && which == -1) &&
			 ((tmp = getindex(Combo1, argv[1])) != -1 ||
			  (tmp = getindex(Combo2, argv[1])) != -1)) {
			which = tmp / M_NUM;
			mode = tmp % M_NUM;
		} else if (strcmp(argv[1], "help") == 0 && argv[2] == NULL)
			cmdhelp(mode, which);
		else if (which != -1 && mode != -1) {
			if (strspn(argv[1], "0123456789") == strlen(argv[1])) {
				id = strtonum(argv[1], 0, LONG_MAX, &errstr);
				if (errstr != NULL)
					errx(EX_USAGE, "Bad id '%s': %s",
					    argv[1], errstr);
			} else
				name = argv[1];
		} else
			errx(EX_USAGE, "unknown keyword `%s'", argv[1]);
		++argv;
		--argc;
	}

	/*
	 * Bail out unless the user is specific!
	 */
	if (mode == -1 || which == -1)
		cmdhelp(mode, which);

	conf.rootfd = open(conf.rootdir, O_DIRECTORY|O_CLOEXEC);
	if (conf.rootfd == -1)
		errx(EXIT_FAILURE, "Unable to open '%s'", conf.rootdir);
	conf.which = which;
	/*
	 * We know which mode we're in and what we're about to do, so now
	 * let's dispatch the remaining command line args in a genric way.
	 */
	optarg = NULL;

	while ((ch = getopt(argc, argv, opts[which][mode])) != -1) {
		switch (ch) {
		case '?':
			errx(EX_USAGE, "unknown switch");
			break;
		case '7':
			conf.v7 = true;
			break;
		case 'C':
			conf.config = optarg;
			config = conf.config;
			break;
		case 'F':
			conf.force = true;
			break;
		case 'N':
			conf.dryrun = true;
			break;
		case 'l':
			if (strlen(optarg) >= MAXLOGNAME)
				errx(EX_USAGE, "new name too long: %s", optarg);
			conf.newname = optarg;
			break;
		case 'P':
			conf.pretty = true;
			break;
		case 'Y':
			nis = true;
			break;
		case 'a':
			conf.all = true;
			break;
		case 'c':
			conf.gecos = pw_checkname(optarg, 1);
			break;
		case 'g':
			if (which == 0) { /* for user* */
				addarg(&arglist, 'g', optarg);
				break;
			}
			if (strspn(optarg, "0123456789") != strlen(optarg))
				errx(EX_USAGE, "-g expects a number");
			id = strtonum(optarg, 0, LONG_MAX, &errstr);
			if (errstr != NULL)
				errx(EX_USAGE, "Bad id '%s': %s", optarg,
				    errstr);
			break;
		case 'u':
			if (strspn(optarg, "0123456789,") != strlen(optarg))
				errx(EX_USAGE, "-u expects a number");
			if (strchr(optarg, ',') != NULL) {
				addarg(&arglist, 'u', optarg);
				break;
			}
			id = strtonum(optarg, 0, LONG_MAX, &errstr);
			if (errstr != NULL)
				errx(EX_USAGE, "Bad id '%s': %s", optarg,
				    errstr);
			break;
		case 'n':
			if (strspn(optarg, "0123456789") != strlen(optarg)) {
				name = optarg;
				break;
			}
			id = strtonum(optarg, 0, LONG_MAX, &errstr);
			if (errstr != NULL)
				errx(EX_USAGE, "Bad id '%s': %s", optarg,
				    errstr);
			break;
		case 'H':
			if (conf.fd != -1)
				errx(EX_USAGE, "'-h' and '-H' are mutually "
				    "exclusive options");
			conf.precrypted = true;
			if (strspn(optarg, "0123456789") != strlen(optarg))
				errx(EX_USAGE, "'-H' expects a file descriptor");

			conf.fd = strtonum(optarg, 0, INT_MAX, &errstr);
			if (errstr != NULL)
				errx(EX_USAGE, "Bad file descriptor '%s': %s",
				    optarg, errstr);
			break;
		case 'h':
			if (conf.fd != -1)
				errx(EX_USAGE, "'-h' and '-H' are mutually "
				    "exclusive options");

			if (strcmp(optarg, "-") == 0)
				conf.fd = '-';
			else if (strspn(optarg, "0123456789") == strlen(optarg)) {
				conf.fd = strtonum(optarg, 0, INT_MAX, &errstr);
				if (errstr != NULL)
					errx(EX_USAGE, "'-h' expects a "
					    "file descriptor or '-'");
			} else
				errx(EX_USAGE, "'-h' expects a file "
				    "descriptor or '-'");
			break;
		case 'o':
			conf.checkduplicate = true;
			break;
		case 'q':
			conf.quiet = true;
			break;
		case 'r':
			conf.deletehome = true;
			break;
		default:
			addarg(&arglist, ch, optarg);
			break;
		}
		optarg = NULL;
	}

	if (name != NULL && strlen(name) >= MAXLOGNAME)
		errx(EX_USAGE, "name too long: %s", name);

	/*
	 * Must be root to attempt an update
	 */
	if (geteuid() != 0 && mode != M_PRINT && mode != M_NEXT && !conf.dryrun)
		errx(EX_NOPERM, "you must be root to run this program");

	/*
	 * We should immediately look for the -q 'quiet' switch so that we
	 * don't bother with extraneous errors
	 */
	if (conf.quiet)
		freopen(_PATH_DEVNULL, "w", stderr);

	/*
	 * Set our base working path if not overridden
	 */

	if (config == NULL) {	/* Only override config location if -C not specified */
		asprintf(&config, "%s/pw.conf", conf.etcpath);
		if (config == NULL)
			errx(EX_OSERR, "out of memory");
	}

	/*
	 * Now, let's do the common initialisation
	 */
	conf.userconf = read_userconfig(config);

	ch = funcs[which] (mode, name, id, &arglist);

	/*
	 * If everything went ok, and we've been asked to update
	 * the NIS maps, then do it now
	 */
	if (ch == EXIT_SUCCESS && nis) {
		pid_t	pid;

		fflush(NULL);
		if (chdir(_PATH_YP) == -1)
			warn("chdir(" _PATH_YP ")");
		else if ((pid = fork()) == -1)
			warn("fork()");
		else if (pid == 0) {
			/* Is make anywhere else? */
			execlp("/usr/bin/make", "make", (char *)NULL);
			_exit(1);
		} else {
			int   i;
			waitpid(pid, &i, 0);
			if ((i = WEXITSTATUS(i)) != 0)
				errx(ch, "make exited with status %d", i);
			else
				pw_log(conf.userconf, mode, which, "NIS maps updated");
		}
	}
	return ch;
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
cmdhelp(int mode, int which)
{
	if (which == -1)
		fprintf(stderr, "usage:\n  pw [user|group|lock|unlock] [add|del|mod|show|next] [help|switches/values]\n");
	else if (mode == -1)
		fprintf(stderr, "usage:\n  pw %s [add|del|mod|show|next] [help|switches/values]\n", Which[which]);
	else {

		/*
		 * We need to give mode specific help
		 */
		static const char *help[W_NUM][M_NUM] =
		{
			{
				"usage: pw useradd [name] [switches]\n"
				"\t-V etcdir      alternate /etc location\n"
				"\t-R rootir      alternate root directory\n"
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
				"\t-M mode        home directory permissions\n"
				"\t-s shell       name of login shell\n"
				"\t-o             duplicate uid ok\n"
				"\t-L class       user class\n"
				"\t-h fd          read password on fd\n"
				"\t-H fd          read encrypted password on fd\n"
				"\t-Y             update NIS maps\n"
				"\t-N             no update\n"
				"  Setting defaults:\n"
				"\t-V etcdir      alternate /etc location\n"
				"\t-R rootir      alternate root directory\n"
			        "\t-D             set user defaults\n"
				"\t-b dir         default home root dir\n"
				"\t-e period      default expiry period\n"
				"\t-p period      default password change period\n"
				"\t-g group       default group\n"
				"\t-G grp1,grp2   additional groups\n"
				"\t-L class       default user class\n"
				"\t-k dir         default home skeleton\n"
				"\t-M mode        home directory permissions\n"
				"\t-u min,max     set min,max uids\n"
				"\t-i min,max     set min,max gids\n"
				"\t-w method      set default password method\n"
				"\t-s shell       default shell\n"
				"\t-y path        set NIS passwd file path\n",
				"usage: pw userdel [uid|name] [switches]\n"
				"\t-V etcdir      alternate /etc location\n"
				"\t-R rootir      alternate root directory\n"
				"\t-n name        login name\n"
				"\t-u uid         user id\n"
				"\t-Y             update NIS maps\n"
				"\t-r             remove home & contents\n",
				"usage: pw usermod [uid|name] [switches]\n"
				"\t-V etcdir      alternate /etc location\n"
				"\t-R rootir      alternate root directory\n"
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
				"\t-M mode        home directory permissions\n"
				"\t-s shell       name of login shell\n"
				"\t-w method      set new password using method\n"
				"\t-h fd          read password on fd\n"
				"\t-H fd          read encrypted password on fd\n"
				"\t-Y             update NIS maps\n"
				"\t-N             no update\n",
				"usage: pw usershow [uid|name] [switches]\n"
				"\t-V etcdir      alternate /etc location\n"
				"\t-R rootir      alternate root directory\n"
				"\t-n name        login name\n"
				"\t-u uid         user id\n"
				"\t-F             force print\n"
				"\t-P             prettier format\n"
				"\t-a             print all users\n"
				"\t-7             print in v7 format\n",
				"usage: pw usernext [switches]\n"
				"\t-V etcdir      alternate /etc location\n"
				"\t-R rootir      alternate root directory\n"
				"\t-C config      configuration file\n"
				"\t-q             quiet operation\n",
				"usage pw: lock [switches]\n"
				"\t-V etcdir      alternate /etc locations\n"
				"\t-C config      configuration file\n"
				"\t-q             quiet operation\n",
				"usage pw: unlock [switches]\n"
				"\t-V etcdir      alternate /etc locations\n"
				"\t-C config      configuration file\n"
				"\t-q             quiet operation\n"
			},
			{
				"usage: pw groupadd [group|gid] [switches]\n"
				"\t-V etcdir      alternate /etc location\n"
				"\t-R rootir      alternate root directory\n"
				"\t-C config      configuration file\n"
				"\t-q             quiet operation\n"
				"\t-n group       group name\n"
				"\t-g gid         group id\n"
				"\t-M usr1,usr2   add users as group members\n"
				"\t-o             duplicate gid ok\n"
				"\t-Y             update NIS maps\n"
				"\t-N             no update\n",
				"usage: pw groupdel [group|gid] [switches]\n"
				"\t-V etcdir      alternate /etc location\n"
				"\t-R rootir      alternate root directory\n"
				"\t-n name        group name\n"
				"\t-g gid         group id\n"
				"\t-Y             update NIS maps\n",
				"usage: pw groupmod [group|gid] [switches]\n"
				"\t-V etcdir      alternate /etc location\n"
				"\t-R rootir      alternate root directory\n"
				"\t-C config      configuration file\n"
				"\t-q             quiet operation\n"
				"\t-F             force add if not exists\n"
				"\t-n name        group name\n"
				"\t-g gid         group id\n"
				"\t-M usr1,usr2   replaces users as group members\n"
				"\t-m usr1,usr2   add users as group members\n"
				"\t-d usr1,usr2   delete users as group members\n"
				"\t-l name        new group name\n"
				"\t-Y             update NIS maps\n"
				"\t-N             no update\n",
				"usage: pw groupshow [group|gid] [switches]\n"
				"\t-V etcdir      alternate /etc location\n"
				"\t-R rootir      alternate root directory\n"
				"\t-n name        group name\n"
				"\t-g gid         group id\n"
				"\t-F             force print\n"
				"\t-P             prettier format\n"
				"\t-a             print all accounting groups\n",
				"usage: pw groupnext [switches]\n"
				"\t-V etcdir      alternate /etc location\n"
				"\t-R rootir      alternate root directory\n"
				"\t-C config      configuration file\n"
				"\t-q             quiet operation\n"
			}
		};

		fprintf(stderr, "%s", help[which][mode]);
	}
	exit(EXIT_FAILURE);
}

struct carg    *
getarg(struct cargs * _args, int ch)
{
	struct carg    *c;

	if (_args == NULL)
		return (NULL);
	
	c = LIST_FIRST(_args);

	while (c != NULL && c->ch != ch)
		c = LIST_NEXT(c, list);
	return c;
}

struct carg    *
addarg(struct cargs * _args, int ch, char *argstr)
{
	struct carg    *ca = malloc(sizeof(struct carg));

	if (ca == NULL)
		errx(EX_OSERR, "out of memory");
	ca->ch = ch;
	ca->val = argstr;
	LIST_INSERT_HEAD(_args, ca, list);
	return ca;
}
