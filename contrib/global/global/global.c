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
 *	global.c				8-Dec-98
 *
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "global.h"

const char *progname  = "global";		/* command name */

static void	usage __P((void));
static void	setcom __P((int));
int	main __P((int, char **));
void	makefilter __P((char *));
FILE	*openfilter __P((void));
void	closefilter __P((FILE *));
void	completelist __P((char *, char *, char *));
void	relative_filter __P((STRBUF *, char *, char *));
void	grep __P((char *));
void	pathlist __P((char *, char *));
void	parsefile __P((int, char **, char *, char *, char *, int));
void	printtag __P((FILE *, char *));
int	notnamechar __P((char *));
int	search __P((char *, char *, char *, char *, int));
int	includepath __P((char *, char *));

char	sortfilter[MAXFILLEN+1];	/* sort filter		*/
char	pathfilter[MAXFILLEN+1];	/* path convert filter	*/
char	*localprefix;			/* local prefix		*/
int	aflag;				/* [option]		*/
int	cflag;				/* command		*/
int	fflag;				/* command		*/
int	gflag;				/* command		*/
int	iflag;				/* command		*/
int	lflag;				/* [option]		*/
int	nflag;				/* [option]		*/
int	pflag;				/* command		*/
int	Pflag;				/* command		*/
int	rflag;				/* [option]		*/
int	sflag;				/* command		*/
int	tflag;				/* [option]		*/
int	vflag;				/* [option]		*/
int	xflag;				/* [option]		*/
int	pfilter;			/* undocumented command	*/

static void
usage()
{
	fprintf(stderr, "usage:\t%s\n\t%s\n\t%s\n\t%s\n\t%s\n\t%s\n\t%s\n\t%s\n",
		"global [-alnrtvx] pattern",
		"global -c [prefix]",
		"global -f[alnrtx] files",
		"global -g[alntvx] pattern",
		"global -i[v]",
		"global -p[v]",
		"global -P[alnt] [pattern]",
		"global -s[alntvx] pattern");
	exit(1);
}

static int	command;
static void
setcom(c)
int	c;
{
	if (command == 0)
		command = c;
	else
		usage();
}
int
main(argc, argv)
int	argc;
char	*argv[];
{
	char	*av;
	int	count;
	int	db;
	char	cwd[MAXPATHLEN+1];		/* current directory	*/
	char	root[MAXPATHLEN+1];		/* root of source tree	*/
	char	dbpath[MAXPATHLEN+1];		/* dbpath directory	*/

	while (--argc > 0 && (++argv)[0][0] == '-') {
		char	*p;

		if (!strcmp(argv[0], "--filter")) {
			pfilter++;
			continue;
		}
		for (p = argv[0] + 1; *p; p++) {
			switch (*p) {
			case 'a':
				aflag++;
				break;
			case 'c':
				cflag++;
				setcom(*p);
				break;
			case 'f':
				fflag++;
				xflag++;
				setcom(*p);
				break;
			case 'l':
				lflag++;
				break;
			case 'n':
				nflag++;
				break;
			case 'g':
				gflag++;
				setcom(*p);
				break;
			case 'i':
				iflag++;
				setcom(*p);
				break;
			case 'p':
				pflag++;
				setcom(*p);
				break;
			case 'P':
				Pflag++;
				setcom(*p);
				break;
			case 'r':
				rflag++;
				break;
			case 's':
				sflag++;
				setcom(*p);
				break;
			case 't':
				tflag++;
				break;
			case 'v':
				vflag++;
				break;
			case 'x':
				xflag++;
				break;
			default:
				usage();
			}
		}
	}
	av = (argc > 0) ? *argv : NULL;
	/*
	 * only -c, -i, -P and -p allows no argment.
	 */
	if (!av && !pfilter) {
		switch (command) {
		case 'c':
		case 'i':
		case 'p':
		case 'P':
			break;
		default:
			usage();
		}
	}
	/*
	 * invalid options are just ignored.
	 */
	/*
	 * remove leading blanks.
	 */
	if (av && !gflag)
		for (; *av == ' ' || *av == '\t'; av++)
			;
	if (cflag && av && notnamechar(av))
		die("only name char is allowed with -c option.");
	/*
	 * get path of following directories.
	 *	o current directory
	 *	o root of source tree
	 *	o dbpath directory
	 *
	 * if GTAGS not found, getdbpath doesn't return.
	 */
	if (pflag && vflag) {
		char *gtagsdbpath = getenv("GTAGSDBPATH");
		char *gtagsroot = getenv("GTAGSROOT");
		if (gtagsdbpath && !gtagsroot)
			fprintf(stdout, "warning: GTAGSDBPATH is ignored becase GTAGSROOT is not set.\n");
	}
	getdbpath(cwd, root, dbpath);

	if (pflag) {
		fprintf(stdout, "%s\n", dbpath);
		exit(0);
	}
	/*
	 * incremental update of tag files.
	 */
	if (iflag) {
		STRBUF	*sb = stropen();

		if (chdir(root) < 0)
			die1("cannot change directory to '%s'.", root);
		strputs(sb, "gtags -i");
		if (vflag)
			strputc(sb, 'v');
		strputc(sb, ' ');
		strputs(sb, dbpath);
		if (system(strvalue(sb)))
			exit(1);
		strclose(sb);
		exit(0);
	}

	/*
	 * complete function name
	 */
	if (cflag) {
		completelist(dbpath, root, av);
		exit(0);
	}
	/*
	 * make sort filter.
	 */
	if (sflag && xflag)
		strcpy(sortfilter, "");
	else if (tflag)				/* ctags format */
		strcpy(sortfilter, "sort +0 -1 +1 -2 +2n -3");
	else if (fflag)
		strcpy(sortfilter, "sort +2 -3 +1n -2");
	else if (xflag)				/* print details */
		strcpy(sortfilter, "sort +0 -1 +2 -3 +1n -2");
	else					/* print just file name */
		strcpy(sortfilter, "sort | uniq");
	/*
	 * make path filter.
	 */
	if (aflag)				/* absolute path name */
		sprintf(pathfilter, "sed -e 's!\\.!%s!'", root);
	else {					/* relative path name */
		STRBUF	*sb = stropen();

		relative_filter(sb, root, cwd);
		strcpy(pathfilter, strvalue(sb));
		strclose(sb);
	}
	/*
	 * make local prefix.
	 */
	if (lflag) {
		char	*p = cwd + strlen(root);
		STRBUF	*sb = stropen();
		/*
		 * getdbpath() assure follows.
		 * cwd != "/" and cwd includes root.
		 */
		strputc(sb, '.');
		if (*p)
			strputs(sb, p);
		strputc(sb, '/');
		localprefix = strdup(strvalue(sb));
		if (!localprefix)
			die("short of memory.");
		strclose(sb);
	}
	/*
	 * print filter.
	 */
	if (pfilter) {
		char    filter[MAXFILLEN+1];

		makefilter(filter);
		fprintf(stdout, "%s\n", filter);
		exit(0);
	}
	/*
	 * grep the pattern in a source tree.
	 */
	if (gflag) {
		if (!lflag) 
			chdir(root);
		else if (!aflag)
			sprintf(pathfilter, "sed -e 's!\\./!!'");
		grep(av);
		exit(0);
	}
	/*
	 * locate the path including the pattern in a source tree.
	 */
	if (Pflag) {
		pathlist(dbpath, av);
		exit(0);
	}
	db = (rflag) ? GRTAGS : ((sflag) ? GSYMS : GTAGS);
	/*
	 * print function definitions.
	 */
	if (fflag) {
		parsefile(argc, argv, cwd, root, dbpath, db);
		exit(0);
	}
	/*
	 * search in current source tree.
	 */
	count = search(av, cwd, root, dbpath, db);
	/*
	 * search in library path.
	 */
	if (count == 0 && !rflag && !sflag && !notnamechar(av) && getenv("GTAGSLIBPATH")) {
		char	buf[MAXENVLEN+1];
		char	libdbpath[MAXPATHLEN+1];
		char	*p, *lib;

		strcpy(buf, getenv("GTAGSLIBPATH"));
		p = buf;
		while (p) {
			lib = p;
			if ((p = locatestring(p, ":", MATCH_FIRST)) != NULL)
				*p++ = 0;
			if (!gtagsexist(lib, libdbpath))
				continue;
			if (!strcmp(dbpath, libdbpath))
				continue;
			if (aflag)		/* absolute path name */
				sprintf(pathfilter, "sed -e 's!\\.!%s!'", lib);
			else {
				STRBUF	*sb = stropen();
				relative_filter(sb, lib, cwd);
				strcpy(pathfilter, strvalue(sb));
				strclose(sb);
			}
			count = search(av, cwd, lib, libdbpath, db);
			if (count > 0) {
				strcpy(dbpath, libdbpath);
				break;
			}
		}
	}
	if (vflag) {
		if (count) {
			if (count == 1)
				fprintf(stderr, "%d object located", count);
			if (count > 1)
				fprintf(stderr, "%d objects located", count);
			fprintf(stderr, " (using '%s').\n", makepath(dbpath, dbname(db)));
		} else {
			fprintf(stderr, "'%s' not found.\n", av);
		}
	}
	exit(0);
}
/*
 * makefilter: make filter string.
 *
 *	io)	filter buffer
 */
void
makefilter(filter)
char	*filter;
{
	if (nflag)
		filter[0] = 0;
	else if (sortfilter[0] == 0 && pathfilter[0] == 0)
		filter[0] = 0;
	else if (sortfilter[0] && pathfilter[0])
		sprintf(filter, "%s | %s", sortfilter, pathfilter);
	else if (sortfilter[0])
		strcpy(filter, sortfilter);
	else
		strcpy(filter, pathfilter);
}
/*
 * openfilter: open output filter.
 *
 *	gi)	pathfilter
 *	gi)	sortfilter
 *	r)		output filter
 */
FILE	*
openfilter(void)
{
	FILE	*op;
	char	filter[MAXFILLEN+1];

	makefilter(filter);
	if (filter[0] == 0)
		op = stdout;
	else
		op = popen(filter, "w");
	return op;
}
void
closefilter(op)
FILE	*op;
{
	if (op != stdout)
		pclose(op);
}
/*
 * completelist: print complete list of function
 *
 *	i)	dbpath	dbpath directory
 *	i)	root	root directory
 *	i)	prefix	prefix of primary key
 */
void
completelist(dbpath, root, prefix)
char	*dbpath;
char	*root;
char	*prefix;
{
	char	*p;
	int	flags = GTOP_KEY;
	GTOP	*gtop;

	if (prefix && *prefix == 0)	/* In the case global -c '' */
		prefix = NULL;
	if (prefix)
		flags |= GTOP_PREFIX;
	gtop = gtagsopen(dbpath, root, GTAGS, GTAGS_READ, 0);
	for (p = gtagsfirst(gtop, prefix, flags); p; p = gtagsnext(gtop))
		(void)fprintf(stdout, "%s\n", p);
	gtagsclose(gtop);
}
/*
 * relative_filter: make relative path filter
 *
 *	i)	sb	string buffer
 *	i)	root	the root directory of source tree
 *	i)	cwd	current directory
 *	r) 		relative path filter
 */
void
relative_filter(sb, root, cwd)
STRBUF	*sb;
char	*root;
char	*cwd;
{
	char	*p, *c, *branch;

	/*
	 * get branch point.
	 */
	branch = cwd;
	for (p = root, c = cwd; *p && *c && *p == *c; p++, c++)
		if (*c == '/')
			branch = c;
	if (*p == 0 && (*c == 0 || *c == '/'))
		branch = c;
	/*
	 * forward to root.
	 */
	strputs(sb, "sed -e 's!\\./!");
	for (c = branch; *c; c++)
		if (*c == '/')
			strputs(sb, "../");
	p = root + (branch - cwd);
	/*
	 * backward to leaf.
	 */
	if (*p) {
		p++;
		strputs(sb, p);
		strputc(sb, '/');
	}
	strputs(sb, "!'");
	/*
	 * remove redundancy.
	 */
	if (*branch) {
		char	unit[256];

		p = unit;
		for (c = branch + 1; ; c++) {
			if (*c == 0 || *c == '/') {
				*p = 0;
				strputs(sb, " -e 's!\\.\\./");
				strputs(sb, unit);
				strputs(sb, "/!!'");
				if (*c == 0)
					break;
				p = unit;
			} else
				*p++ = *c;
		}
	}
}
/*
 * printtag: print a tag's line
 *
 *	i)	op	output stream
 *	i)	bp	tag's line
 */
void
printtag(op, bp)
FILE	*op;
char	*bp;
{
	if (tflag) {
		char	buf[MAXBUFLEN+1];
		char	lno[20], *l = lno;
		char	*p = bp;
		char	*q = buf;

		while (*p && !isspace(*p))
			*q++ = *p++;
		*q++ = '\t';
		for (; *p && isspace(*p); p++)
			;
		while (*p && isdigit(*p))
			*l++ = *p++;
		*l = 0;
		for (; *p && isspace(*p); p++)
			;
		while (*p && !isspace(*p))
			*q++ = *p++;
		*q++ = '\t';
		l = lno;
		while (*l)
			*q++ = *l++;
		*q = 0;
		fprintf(op, "%s\n", buf); 
	} else if (!xflag) {
		char	*p = locatestring(bp, "./", MATCH_FIRST);

		if (p == NULL)
			die("illegal tag format (path not found).");
		while (*p && *p != ' ' && *p != '\t')
			(void)putc(*p++, op);
		(void)putc('\n', op);
	} else
		detab(op, bp);
}

/*
 * notnamechar: test whether or not no name char included.
 *
 *	i)	s	string
 *	r)		0: not included, 1: included
 */
int
notnamechar(s)
char	*s;
{
	int	c;

	while ((c = *s++) != '\0')
		if (	(c >= 'a' && c <= 'z')	||
			(c >= 'A' && c <= 'Z')	||
			(c >= '0' && c <= '9')	||
			(c == '-') || (c == '_'))
			;
		else
			return 1;
	return 0;
}
/*
 * grep: grep pattern
 *
 *	i)	pattern	POSIX regular expression
 */
void
grep(pattern)
char	*pattern;
{
	FILE	*op, *fp;
	char	*path;
	char	edit[IDENTLEN+1];
	char	*buffer;
	int	linenum, count, editlen;
	regex_t	preg;

	/*
	 * convert spaces into %FF format.
	 */
	{
		char	*p, *e = edit;
		for (p = pattern; *p; p++) {
			if (*p == '%' || *p == ' ' || *p == '\t') {
				sprintf(e, "%%%02x", *p);
				e += 3;
			} else
				*e++ = *p;
		}
		*e = 0;
	}
	editlen = strlen(edit);

	if (regcomp(&preg, pattern, REG_EXTENDED) != 0)
		die("illegal regular expression.");
	if (!(op = openfilter()))
		die("cannot open output filter.");
	count = 0;
	for (findopen(); (path = findread(NULL)) != NULL; ) {
		if (!(fp = fopen(path, "r")))
			die1("cannot open file '%s'.", path);
		linenum = 0;
		while ((buffer = mgets(fp, NULL, 0)) != NULL) {
			linenum++;
			if (regexec(&preg, buffer, 0, 0, 0) == 0) {
				count++;
				if (tflag)
					fprintf(op, "%s\t%s\t%d\n",
						edit, path, linenum);
				else if (!xflag) {
					fprintf(op, "%s\n", path);
					break;
				} else if (editlen >= 16 && linenum >= 1000)
					fprintf(op, "%-16s %4d %-16s %s\n",
						edit, linenum, path, buffer);
				else
					fprintf(op, "%-16s%4d %-16s %s\n",
						edit, linenum, path, buffer);
			}
		}
		fclose(fp);
	}
	findclose();
	closefilter(op);
	if (vflag) {
		if (count == 0)
			fprintf(stderr, "object not found.\n");
		if (count == 1)
			fprintf(stderr, "%d object located.\n", count);
		if (count > 1)
			fprintf(stderr, "%d objects located.\n", count);
	}
}
/*
 * pathlist: print candidate path list.
 *
 *	i)	dbpath
 */
void
pathlist(dbpath, av)
char	*dbpath;
char	*av;
{
	FILE	*op;
	const char *tag = av;
	char	key[10], *path;
	regex_t preg;
	int	i, lim;

	if (av) {
		if (regcomp(&preg, av, REG_EXTENDED) != 0)
			die("illegal regular expression.");
	} else
		tag = "file";
	if (!(op = openfilter()))
		die("cannot open output filter.");
	if (pathopen(dbpath, 0) < 0)
		die("GPATH not found.");
	lim = nextkey();
	for (i = 0; i < lim; i++) {
		sprintf(key, "%d", i);
		if ((path = pathget(key)) == NULL)
			continue;
		if (lflag && !locatestring(path, localprefix, MATCH_AT_FIRST))
			continue;
		if (av && regexec(&preg, path + 2, 0, 0, 0) != 0)
			continue;
		if (tflag)
			fprintf(op, "%s\t%s\t1\n", tag, path);
		else
			fprintf(op, "%s\n", path);
	}
	pathclose();
	closefilter(op);
}
/*
 * parsefile: parse file to pick up tags.
 *
 *	i)	db
 *	i)	dbpath
 *	i)	root
 *	i)	cwd
 *	i)	argc
 *	i)	argv
 */
void
parsefile(argc, argv, cwd, root, dbpath, db)
int	argc;
char	**argv;
char	*cwd;
char	*root;
char	*dbpath;
int	db;
{
	char	buf[MAXPATHLEN+1], *path;
	char	*p;
	FILE	*ip, *op;
	char	*parser, *av;
	STRBUF  *sb = stropen();
	STRBUF	*com = stropen();

	/*
	 * get parser.
	 */
	if (!getconfs(dbname(db), sb))
		die1("cannot get parser for %s.", dbname(db));
	parser = strvalue(sb);

	if (!(op = openfilter()))
		die("cannot open output filter.");
	if (pathopen(dbpath, 0) < 0)
		die("GPATH not found.");
	for (; argc > 0; argv++, argc--) {
		av = argv[0];

		if (test("d", av)) {
			fprintf(stderr, "'%s' is a directory.\n", av);
			continue;
		}
		if (!test("f", NULL)) {
			fprintf(stderr, "'%s' not found.\n", av);
			continue;
		}
		/*
		 * convert path into relative from root directory of source tree.
		 */
		path = realpath(av, buf);
		if (*path != '/')
			die("realpath(3) is not compatible with BSD version.");
		if (strncmp(path, root, strlen(root))) {
			fprintf(stderr, "'%s' is out of source tree.\n", path);
			continue;
		}
		path += strlen(root) - 1;
		*path = '.';
		if (!pathget(path)) {
			fprintf(stderr, "'%s' not found in GPATH.\n", path);
			continue;
		}
		if (chdir(root) < 0)
			die1("cannot move to '%s' directory.", root);
		/*
		 * make command line.
		 */
		strstart(com);
		makecommand(parser, path, com);
		if (!(ip = popen(strvalue(com), "r")))
			die1("cannot execute '%s'.", strvalue(com));
		while ((p = mgets(ip, NULL, 0)) != NULL)
			printtag(op, p);
		pclose(ip);
		if (chdir(cwd) < 0)
			die1("cannot move to '%s' directory.", cwd);
	}
	pathclose();
	closefilter(op);
	strclose(com);
	strclose(sb);
}
/*
 * search: search specified function 
 *
 *	i)	pattern		search pattern
 *	i)	cwd		current directory
 *	i)	root		root of source tree
 *	i)	dbpath		database directory
 *	i)	db		GTAGS,GRTAGS,GSYMS
 *	r)			count of output lines
 */
int
search(pattern, cwd, root, dbpath, db)
char	*pattern;
char	*cwd;
char	*root;
char	*dbpath;
int	db;
{
	char	*p;
	int	count = 0;
	FILE	*op;
	GTOP	*gtop;
	regex_t	preg;

	/*
	 * open tag file.
	 */
	gtop = gtagsopen(dbpath, root, db, GTAGS_READ, 0);
	if (!(op = openfilter()))
		die("cannot open output filter.");
	/*
	 * regular expression.
	 */
	if (!strcmp(pattern, ".*")) {
		for (p = gtagsfirst(gtop, NULL, 0); p; p = gtagsnext(gtop)) {
			if (lflag) {
				char	*q;
				/* locate start point of a path */
				q = locatestring(p, "./", MATCH_FIRST);
				if (!locatestring(q, localprefix, MATCH_AT_FIRST))
					continue;
			}
			printtag(op, p);
			count++;
		}

	} else if (notnamechar(pattern) && regcomp(&preg, pattern, REG_EXTENDED) == 0) {
		if (*pattern == '^' && *(p = pattern + 1) && (isalpha(*p) || *p == '_')) {
			char	buf[IDENTLEN+1];
			char	*prefix = buf;

			*prefix++ = *p++;
			while (*p && (isalpha(*p) || isdigit(*p) || *p == '_'))
				*prefix++ = *p++;
			*prefix = 0;
			prefix = buf;
			p = gtagsfirst(gtop, prefix, GTOP_PREFIX);
		} else {
			p = gtagsfirst(gtop, NULL, 0);
		}
		for (; p; p = gtagsnext(gtop)) {
			/*
			 * search $1 of tag line (not key)
			 */
			if (regexec(&preg, strmake(gtop->dbop->lastdat, " \t"), 0, 0, 0) != 0)
				continue;
			if (lflag) {
				char	*q;
				/* locate start point of a path */
				q = locatestring(p, "./", MATCH_FIRST);
				if (!locatestring(q, localprefix, MATCH_AT_FIRST))
					continue;
			}
			printtag(op, p);
			count++;
		}
	} else {
		for (p = gtagsfirst(gtop, pattern, 0); p; p = gtagsnext(gtop)) {
			if (lflag) {
				char	*q;
				/* locate start point of a path */
				q = locatestring(p, "./", MATCH_FIRST);
				if (!locatestring(q, localprefix, MATCH_AT_FIRST))
					continue;
			}
			printtag(op, p);
			count++;
		}
	}
	closefilter(op);
	gtagsclose(gtop);
	return count;
}
/*
 * includepath: check if the path included in tag line or not.
 *
 *	i)	line	tag line
 *	i)	path	path
 *	r)		0: doesn't included, 1: included
 */
int
includepath(line, path)
char	*line;
char	*path;
{
	char	*p;
	int	length;

	if (!(p = locatestring(line, "./", MATCH_FIRST)))
		die("illegal tag format (path not found).");
	length = strlen(path);
	if (strncmp(p, path, length))
		return 0;
	p += length;
	if (*p == ' ' || *p == '\t')
		return 1;
	return 0;
}
