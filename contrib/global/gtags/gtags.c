/*
 * Copyright (c) 1996, 1997 Shigio Yamaguchi. All rights reserved.
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
 *	gtags.c					12-Dec-97
 *
 */

#include <sys/stat.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "global.h"

char	*progname  = "gtags";		/* command name */

static void	usage __P((void));
void	main __P((int, char **));
int	incremental __P((char *));
void	tagadd __P((int, char *));
void	createtags __P((char *, int));
char	*current __P((void));

static int	iflag;
static int	oflag;
static int	vflag;


static void
usage()
{
	fprintf(stderr, "usage:\t%s [-i][-o][-v][dbpath]\n", progname);
	exit(1);
}

void
main(argc, argv)
int	argc;
char	*argv[];
{
	char	dbpath[MAXPATHLEN+1];
	char	env[MAXENVLEN+1];
	char	*p;
	int	db;

	while (--argc > 0 && (++argv)[0][0] == '-') {
		for (p = argv[0] + 1; *p; p++) {
			switch (*p) {
			case 'i':
				iflag++;
				break;
			case 'o':
				oflag++;
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
	if (argc > 0) {
		strcpy(dbpath, *argv);
	} else {
		if (!getcwd(dbpath, MAXPATHLEN))
			die("cannot get current directory.");
	}
	if (!strcmp(dbpath, "/"))
		die("It's root directory! What are you doing?");
	if (!test("d", dbpath))
		die1("directory '%s' not found.", dbpath);
	if (vflag)
		fprintf(stderr, "[%s] Gtags started\n", current());
	/*
	 * teach gctags(1) where is dbpath.
	 */
	sprintf(env, "GTAGSDBPATH=%s", dbpath);
	putenv(env);
	/*
	 * incremental update.
	 */
	if (iflag && test("f", makepath(dbpath, dbname(GTAGS))) &&
		test("f", makepath(dbpath, dbname(GRTAGS))))
	{
		(void)incremental(dbpath);
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
		if (vflag)
			fprintf(stderr, "[%s] Creating '%s'.\n", current(), dbname(db));
		createtags(dbpath, db);
	}

	if (vflag)
		fprintf(stderr, "[%s] Done.\n", current());
	exit(0);
}
/*
 * incremental: incremental update
 *
 *	i)	dbpath	dbpath directory
 *	r)		0: not updated, 1: updated
 */
int
incremental(dbpath)
char	*dbpath;
{
	struct stat sb;
	time_t	gtags_mtime;
	int	updated = 0;
	char	*path;
	int	db;

	if (vflag) {
		fprintf(stderr, " Tag found in '%s'.\n", dbpath);
		fprintf(stderr, " Incremental update.\n");
	}
	/*
	 * get modified time of GTAGS.
	 */
	path = makepath(dbpath, dbname(GTAGS));
	if (stat(path, &sb) < 0)
		die1("stat failed '%s'.", path);
	gtags_mtime = sb.st_mtime;

	for (findopen(); (path = findread(NULL)) != NULL; ) {
		if (stat(path, &sb) < 0)
			die1("stat failed '%s'.", path);
		/*
		 * only the path modified after GTAGS was modified.
		 */
		if (gtags_mtime < sb.st_mtime) {
			updated = 1;
			if (vflag)
				fprintf(stderr, " Updating tags of '%s' ...", path + 2);
			for (db = GTAGS; db < GTAGLIM; db++) {
				if (db == GSYMS && !test("f", makepath(dbpath, dbname(db))))
					continue;
				if (vflag)
					fprintf(stderr, "%s", dbname(db));
				tagopen(dbpath, db, 2);
				/*
				 * GTAGS needed to make GRTAGS.
				 */
				if (db == GRTAGS)
					lookupopen(dbpath);
				tagdelete(path);
				if (vflag)
					fprintf(stderr, "..");
				tagadd(db, path);
				if (db == GRTAGS)
					lookupclose();
				tagclose();
			}
			if (vflag)
				fprintf(stderr, " Done.\n");
		}
	}
	findclose();
	if (vflag) {
		if (updated)
			fprintf(stderr, " Global databases have been modified.\n");
		else
			fprintf(stderr, " Global databases are up to date.\n");
		fprintf(stderr, "[%s] Done.\n", current());

		fprintf(stderr, " Done.\n");
	}
	return updated;
}
/*
 * tagadd: add records which has specified path.
 *
 *	i)	db	0: GTAGS, 1: GRTAGS, 2: GSYMS
 *	i)	path	source file
 */
void
tagadd(db, path)
int	db;
char	*path;
{
	char	*tagline, *p, *q;
	char	key[IDENTLEN+1];
	FILE	*ip;

	stropen();
	/*
	 * make command line.
	 */
	strputs("gctags -Dex");
	if (db == GRTAGS)
		strputs("r");
	if (db == GSYMS)
		strputs("sc");
	strputc(' ');
	strputs(path);
	p = strclose();
	if (!(ip = popen(p, "r")))
		die1("cannot execute '%s'.", p);
	while ((tagline = mgets(ip, 0, NULL)) != NULL) {
		p = tagline;
		q = key;
		while (*p && !isspace(*p))
			*q++ = *p++;
		*q = 0;
		tagput(key, tagline);
	}
	pclose(ip);
}
/*
 * createtags: create tags file
 *
 *	i)	dbpath	dbpath directory
 *	i)	db	GTAGS, GRTAGS, GSYMS
 */
void
createtags(dbpath, db)
char	*dbpath;
int	db;
{
	char	*path;

	/*
	 * GTAGS needed to make GRTAGS.
	 */
	if (db == GRTAGS)
		lookupopen(dbpath);
	tagopen(dbpath, db, 1);
	for (findopen(); (path = findread(NULL)) != NULL; ) {
		/*
		 * GSYMS doesn't treat asembler.
		 */
		if (db == GSYMS) {
			char	*p = path + strlen(path) - 1;
			if ((*p == 's' || *p == 'S') && *(p - 1) == '.')
				continue;
		}
		if (vflag)
			fprintf(stderr, " extracting tags of %s.\n", path);
		tagadd(db, path);
	}
	findclose();
	tagclose();
	if (db == GRTAGS)
		lookupclose();
}
/*
 * current: current date and time
 *
 *	r)		date and time
 */
char	*
current(void)
{
	static	char	buf[80];
	time_t	tval;

	if (time(&tval) == -1)
		die("cannot get current time.");
	(void)strftime(buf, sizeof(buf), "%+", localtime(&tval));

	return buf;
}
