/*
 * Copyright (c) 1996, 1997, 1998 Shigio Yamaguchi. All rights reserved.
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
 *	This product includes software developed by Shigio Yamaguchi.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	gtags.c					8-Oct-98
 *
 */
#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "global.h"

const char *progname  = "gtags";		/* command name */

static void	usage __P((void));
int	main __P((int, char **));
int	incremental __P((char *, char *));
void	updatetags __P((char *, char *, char *, int));
void	createtags __P((char *, char *, int));
int	printconf __P((char *));
char	*now __P((void));

int	cflag;					/* compact format */
int	iflag;					/* incremental update */
int	oflag;					/* suppress making GSYMS */
int	wflag;					/* warning message */
int	vflag;					/* verbose mode */

int	extractmethod = 0;

static void
usage()
{
	fprintf(stderr, "usage:\t%s [-c][-i][-l][-o][-w][-v][dbpath]\n", progname);
	exit(1);
}

int
main(argc, argv)
int	argc;
char	*argv[];
{
	char	dbpath[MAXPATHLEN+1];
	char	cwd[MAXPATHLEN+1];
	char	env[MAXENVLEN+1];
	STRBUF	*sb = stropen();
	int	db;

	while (--argc > 0 && (++argv)[0][0] == '-') {
		char	*p;
		/*
		 * Secret option for htags(1).
		 */
		if (!strcmp(argv[0], "--config")) {
			if (argc == 1)
				fprintf(stdout, "%s\n", configpath());
			else if (argc == 2) {
				if (!printconf(argv[1]))
					exit(1);
			}
			exit(0);
		} else if (!strcmp(argv[0], "--find")) {
			for (findopen(); (p = findread(NULL)) != NULL; )
				fprintf(stdout, "%s\n", p);
			findclose();
			exit(0);
		} else if (!strcmp(argv[0], "--expand")) {
			FILE *ip;

			++argv; --argc;
			if (argc && argv[0][0] == '-') {
				settabs(atoi(&argv[0][1]));
				++argv; --argc;
			}
			ip = (argc) ? fopen(argv[0], "r") : stdin;
			if (ip == NULL)
				exit(1);
			while ((p = mgets(ip, NULL, 0)) != NULL)
				detab(stdout, p);
			exit(0);
		}

		for (p = argv[0] + 1; *p; p++) {
			switch (*p) {
			case 'c':
				cflag++;
				break;
			case 'i':
				iflag++;
				break;
			case 'o':
				oflag++;
				break;
			case 'w':
				wflag++;
				break;
			case 'v':
				vflag++;
				break;
			/* for compatibility */
			case 's':
			case 'e':
				break;
			default:
				usage();
			}
		}
	}
	if (!getcwd(cwd, MAXPATHLEN))
		die("cannot get current directory.");
	if (argc > 0)
		realpath(*argv,dbpath) ;
	else
		strcpy(dbpath, cwd);
	if (!strcmp(dbpath, "/"))
		die("It's root directory! What are you doing?");
	if (!test("d", dbpath))
		die1("directory '%s' not found.", dbpath);
	if (vflag)
		fprintf(stderr, "[%s] Gtags started\n", now());
	/*
	 * load .gtagsrc or /etc/gtags.conf
	 */
	openconf();
	if (getconfb("extractmethod"))
		extractmethod = 1;
	strstart(sb);
	if (getconfs("format", sb) && !strcmp(strvalue(sb), "compact"))
		cflag++;
	/*
	 * teach gctags(1) where is dbpath.
	 */
	sprintf(env, "GTAGSDBPATH=%s", dbpath);
	putenv(env);
	if (wflag) {
		sprintf(env, "GTAGSWARNING=1");
		putenv(env);
	}
	/*
	 * incremental update.
	 */
	if (iflag && test("f", makepath(dbpath, dbname(GTAGS))) &&
		test("f", makepath(dbpath, dbname(GRTAGS))))
	{
		/* open for version check */
		GTOP *gtop = gtagsopen(dbpath, cwd, GTAGS, GTAGS_MODIFY, 0);
		gtagsclose(gtop);
		if (!test("f", makepath(dbpath, "GPATH")))
			die("Old version tag file found. Please remake it.");
		(void)incremental(dbpath, cwd);
		exit(0);
	}
	if (iflag && vflag)
		fprintf(stderr, " GTAGS and GRTAGS not found. -i option ignored.\n");
	/*
 	 * create GTAGS, GRTAGS and GSYMS
	 */
	for (db = GTAGS; db < GTAGLIM; db++) {

		if (oflag && db == GSYMS)
			continue;
		strstart(sb);
		if (!getconfs(dbname(db), sb))
			continue;
		if (!usable(strmake(strvalue(sb), " \t")))
			die1("Parser '%s' not found or not executable.", strmake(strvalue(sb), " \t"));
		if (vflag)
			fprintf(stderr, "[%s] Creating '%s'.\n", now(), dbname(db));
		createtags(dbpath, cwd, db);
	}

	if (vflag)
		fprintf(stderr, "[%s] Done.\n", now());
	closeconf();
	exit(0);
}
/*
 * incremental: incremental update
 *
 *	i)	dbpath	dbpath directory
 *	i)	root	root directory of source tree
 *	r)		0: not updated, 1: updated
 */
int
incremental(dbpath, root)
char	*dbpath;
char	*root;
{
	struct stat statp;
	time_t	gtags_mtime;
	STRBUF	*addlist = stropen();
	STRBUF	*updatelist = stropen();
	STRBUF	*deletelist = stropen();
	int	updated = 0;
	char	*path;

	if (vflag) {
		fprintf(stderr, " Tag found in '%s'.\n", dbpath);
		fprintf(stderr, " Incremental update.\n");
	}
	/*
	 * get modified time of GTAGS.
	 */
	path = makepath(dbpath, dbname(GTAGS));
	if (stat(path, &statp) < 0)
		die1("stat failed '%s'.", path);
	gtags_mtime = statp.st_mtime;

	if (pathopen(dbpath, 0) < 0)
		die("GPATH not found.");
	/*
	 * make add list and update list.
	 */
	for (findopen(); (path = findread(NULL)) != NULL; ) {
		if (stat(path, &statp) < 0)
			die1("stat failed '%s'.", path);
		if (!pathget(path))
			strnputs(addlist, path, strlen(path) + 1);
		else if (gtags_mtime < statp.st_mtime)
			strnputs(updatelist, path, strlen(path) + 1);
	}
	findclose();
	/*
	 * make delete list.
	 */
	{
		int i, limit = nextkey();

		for (i = 0; i < limit; i++) {
			if ((path = pathiget(i)) == NULL)
				continue;
			if (!test("f", path))
				strnputs(deletelist, path, strlen(path) + 1);
		}
	}
	pathclose();
	if (strbuflen(addlist) + strbuflen(deletelist) + strbuflen(updatelist))
		updated = 1;
	/*
	 * execute updating.
	 */
	if (strbuflen(updatelist) > 0) {
		char	*start = strvalue(updatelist);
		char	*end = start + strbuflen(updatelist);
		char	*p;

		for (p = start; p < end; p += strlen(p) + 1)
			updatetags(dbpath, root, p, 0);
		updated = 1;
	}
	if (strbuflen(addlist) > 0) {
		char	*start = strvalue(addlist);
		char	*end = start + strbuflen(addlist);
		char	*p;

		for (p = start; p < end; p += strlen(p) + 1)
			updatetags(dbpath, root, p, 1);
		updated = 1;
	}
	if (strbuflen(deletelist) > 0) {
		char	*start = strvalue(deletelist);
		char	*end = start + strbuflen(deletelist);
		char	*p;

		for (p = start; p < end; p += strlen(p) + 1)
			updatetags(dbpath, root, p, 2);

		pathopen(dbpath, 2);
		for (p = start; p < end; p += strlen(p) + 1)
			pathdel(p);
		pathclose();
		updated = 1;
	}
	if (vflag) {
		if (updated)
			fprintf(stderr, " Global databases have been modified.\n");
		else
			fprintf(stderr, " Global databases are up to date.\n");
		fprintf(stderr, "[%s] Done.\n", now());
	}
	strclose(addlist);
	strclose(deletelist);
	strclose(updatelist);
	return updated;
}
/*
 * updatetags: update tag file.
 *
 *	i)	dbpath	directory in which tag file exist
 *	i)	root	root directory of source tree
 *	i)	path	path which should be updated
 *	i)	type	0:update, 1:add, 2:delete
 */
void
updatetags(dbpath, root, path, type)
char	*dbpath;
char	*root;
char	*path;
int	type;
{
	GTOP	*gtop;
	STRBUF	*sb = stropen();
	int	db;
	const char *msg = NULL;

	switch (type) {
	case 0:	msg = "Updating"; break;
	case 1: msg = "Adding"; break;
	case 2:	msg = "Deleting"; break;
	}
	if (vflag)
		fprintf(stderr, " %s tags of '%s' ...", msg, path + 2);
	for (db = GTAGS; db < GTAGLIM; db++) {
		int	flags = 0;

		if (db == GSYMS && !test("f", makepath(dbpath, dbname(db))))
			continue;
		if (vflag)
			fprintf(stderr, "%s", dbname(db));
		/*
		 * get tag command.
		 */
		strstart(sb);
		if (!getconfs(dbname(db), sb))
			die1("cannot get tag command. (%s)", dbname(db));
		gtop = gtagsopen(dbpath, root, db, GTAGS_MODIFY, 0);
		/*
		 * GTAGS needed to make GRTAGS.
		 */
		if (db == GRTAGS && !test("f", makepath(dbpath, "GTAGS")))
			die("GTAGS needed to create GRTAGS.");
		if (type != 1)
			gtagsdelete(gtop, path);
		if (vflag)
			fprintf(stderr, "..");
		if (type != 2) {
			if (db == GSYMS)
				flags |= GTAGS_UNIQUE;
			if (extractmethod)
				flags |= GTAGS_EXTRACTMETHOD;
			gtagsadd(gtop, strvalue(sb), path, flags);
		}
		gtagsclose(gtop);
	}
	if (vflag)
		fprintf(stderr, " Done.\n");
	strclose(sb);
}
/*
 * createtags: create tags file
 *
 *	i)	dbpath	dbpath directory
 *	i)	root	root directory of source tree
 *	i)	db	GTAGS, GRTAGS, GSYMS
 */
void
createtags(dbpath, root, db)
char	*dbpath;
char	*root;
int	db;
{
	char	*path;
	GTOP	*gtop;
	int	flags;
	char	*comline;
	STRBUF	*sb = stropen();

	/*
	 * get tag command.
	 */
	if (!getconfs(dbname(db), sb))
		die1("cannot get tag command. (%s)", dbname(db));
	comline = strdup(strvalue(sb));
	if (!comline)
		die("short of memory.");
	/*
	 * GTAGS needed to make GRTAGS.
	 */
	if (db == GRTAGS && !test("f", makepath(dbpath, "GTAGS")))
		die("GTAGS needed to create GRTAGS.");
	flags = 0;
	strstart(sb);
	if (cflag) {
		flags |= GTAGS_COMPACT;
		flags |= GTAGS_PATHINDEX;
	}
	strstart(sb);
	if (vflag > 1 && getconfs(dbname(db), sb))
		fprintf(stderr, " using tag command '%s <path>'.\n", strvalue(sb));
	gtop = gtagsopen(dbpath, root, db, GTAGS_CREATE, flags);
	for (findopen(); (path = findread(NULL)) != NULL; ) {
		int	gflags = 0;
		/*
		 * GSYMS doesn't treat asembler.
		 */
		if (db == GSYMS)
			if (locatestring(path, ".s", MATCH_AT_LAST) != NULL ||
			    locatestring(path, ".S", MATCH_AT_LAST) != NULL)
				continue;
		if (vflag)
			fprintf(stderr, " extracting tags of %s.\n", path);
		if (db == GSYMS)
			gflags |= GTAGS_UNIQUE;
		if (extractmethod)
			gflags |= GTAGS_EXTRACTMETHOD;
		gtagsadd(gtop, comline, path, gflags);
	}
	findclose();
	gtagsclose(gtop);
	free(comline);
	strclose(sb);
}
/*
 * now: current date and time
 *
 *	r)		date and time
 */
char *
now(void)
{
	static	char	buf[80];
	time_t	tval;

	if (time(&tval) == -1)
		die("cannot get current time.");
	(void)strftime(buf, sizeof(buf), "%a %b %e %H:%M:%S %Z %Y", localtime(&tval));
	return buf;
}
/*
 * printconf: print configuration data.
 *
 *	i)	name	label of config data
 *	r)		exit code
 */
int
printconf(name)
char	*name;
{
	STRBUF  *sb;
	int	num;
	int	exist = 1;

	if (getconfn(name, &num))
		fprintf(stdout, "%d\n", num);
	else if (getconfb(name))
		fprintf(stdout, "1\n");
	else {
		sb = stropen();
		if (getconfs(name, sb))
			fprintf(stdout, "%s\n", strvalue(sb));
		else
			exist = 0;
		strclose(sb);
	}
	return exist;
}
