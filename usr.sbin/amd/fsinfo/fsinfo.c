/*
 * Copyright (c) 1989 Jan-Simon Pendry
 * Copyright (c) 1989 Imperial College of Science, Technology & Medicine
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry at Imperial College, London.
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
"@(#) Copyright (c) 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)fsinfo.c    8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] =
	"$Id$";
#endif /* not lint */

/*
 * fsinfo
 */

#include "../fsinfo/fsinfo.h"
#include "fsi_gram.h"
#include <err.h>
#include <pwd.h>
#include <stdlib.h>

qelem *list_of_hosts;
qelem *list_of_automounts;
dict *dict_of_volnames;
dict *dict_of_hosts;
char *autodir = "/a";
char hostname[MAXHOSTNAMELEN+1];
char *username;
int file_io_errors;
int parse_errors;
int errors;
int verbose;
char idvbuf[1024];

char **g_argv;

static void usage __P((void));

/*
 * Output file prefixes
 */
char *exportfs_pref;
char *fstab_pref;
char *dumpset_pref;
char *mount_pref;
char *bootparams_pref;

/*
 * Argument cracking...
 */
static void get_args(c, v)
int c;
char *v[];
{
	int ch;
	int usageflg = 0;
	char *iptr = idvbuf;

	while ((ch = getopt(c, v, "a:b:d:e:f:h:m:D:U:I:qv")) !=  -1)
	switch (ch) {
	case 'a':
		autodir = optarg;
		break;
	case 'b':
		if (bootparams_pref)
			fatal("-b option specified twice");
		bootparams_pref = optarg;
		break;
	case 'd':
		if (dumpset_pref)
			fatal("-d option specified twice");
		dumpset_pref = optarg;
		break;
	case 'h':
		strncpy(hostname, optarg, sizeof(hostname)-1);
		break;
	case 'e':
		if (exportfs_pref)
			fatal("-e option specified twice");
		exportfs_pref = optarg;
		break;
	case 'f':
		if (fstab_pref)
			fatal("-f option specified twice");
		fstab_pref = optarg;
		break;
	case 'm':
		if (mount_pref)
			fatal("-m option specified twice");
		mount_pref = optarg;
		break;
	case 'q':
		verbose = -1;
		break;
	case 'v':
		verbose = 1;
		break;
	case 'I': case 'D': case 'U':
		sprintf(iptr, "-%c%s ", ch, optarg);
		iptr += strlen(iptr);
		break;
	default:
		usageflg++;
		break;
	}

	if (c != optind) {
		g_argv = v + optind - 1;
		if (yywrap())
			fatal("Cannot read any input files");
	} else {
		usageflg++;
	}

	if (usageflg)
		usage();

	if (g_argv[0])
		log("g_argv[0] = %s", g_argv[0]);
	else
		log("g_argv[0] = (nil)");
}

static void
usage()
{
	fprintf(stderr, "%s\n%s\n%s\n",
"usage: fsinfo [-v] [-a autodir] [-h hostname] [-b bootparams] [-d dumpsets]",
"              [-e exports] [-f fstabs] [-m automounts]",
"              [-I dir] [-D|-U string[=string]] config ...");
		exit(1);
}

/*
 * Determine username of caller
 */
static char *find_username()
{
	extern char *getlogin();
	extern char *getenv();
	char *u = getlogin();
	if (!u) {
		struct passwd *pw = getpwuid(getuid());
		if (pw)
			u = pw->pw_name;
	}
	if (!u)
		u = getenv("USER");
	if (!u)
		u = getenv("LOGNAME");
	if (!u)
		u = "root";

	return strdup(u);
}

/*
 * MAIN
 */
main(argc, argv)
int argc;
char *argv[];
{
	/*
	 * Process arguments
	 */
	get_args(argc, argv);

	/*
	 * If no hostname given then use the local name
	 */
	if (!*hostname && gethostname(hostname, sizeof(hostname)) < 0)
		err(1, "gethostname");

	/*
	 * Get the username
	 */
	username = find_username();

	/*
	 * New hosts and automounts
	 */
	list_of_hosts = new_que();
	list_of_automounts = new_que();

	/*
	 * New dictionaries
	 */
	dict_of_volnames = new_dict();
	dict_of_hosts = new_dict();

	/*
	 * Parse input
	 */
	show_area_being_processed("read config", 11);
	if (yyparse())
		errors = 1;
	errors += file_io_errors + parse_errors;

	if (errors == 0) {
		/*
		 * Do semantic analysis of input
		 */
		analyze_hosts(list_of_hosts);
		analyze_automounts(list_of_automounts);
	}

	/*
	 * Give up if errors
	 */
	if (errors == 0) {
		/*
		 * Output data files
		 */

		write_atab(list_of_automounts);
		write_bootparams(list_of_hosts);
		write_dumpset(list_of_hosts);
		write_exportfs(list_of_hosts);
		write_fstab(list_of_hosts);
	}

	col_cleanup(1);

	exit(errors);
}
