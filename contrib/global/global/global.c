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
 *	global.c				7-Nov-97
 *
 */

#include <sys/stat.h>

#include <ctype.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "global.h"

char	*progname  = "global";		/* command name */

static void	usage __P((void));
void	main __P((int, char **));
char	*outfilter __P((void));
void	completelist __P((char *, char *));
void	relative_filter __P((char *, char *, char *));
void	grep __P((char *));
int	printtag __P((FILE *, char *, char *, int));
int	regexp __P((char *));
int	search __P((char *, char *, char *, char *, int));
char	*extractpath __P((char *));
int	includepath __P((char *, char *));

char	sortfilter[MAXCOMLINE+1];	/* sort filter		*/
char	pathfilter[MAXCOMLINE+1];	/* path convert filter	*/
char	local[MAXPATHLEN+1];		/* local prefix		*/
char	*localprefix;			/* local prefix		*/
int	aflag;				/* [option]		*/
int	cflag;				/* command		*/
int	fflag;				/* command		*/
int	lflag;				/* [option]		*/
int	gflag;				/* command		*/
int	iflag;				/* command		*/
int	pflag;				/* command		*/
int	rflag;				/* [option]		*/
int	sflag;				/* command		*/
int	vflag;				/* [option]		*/
int	xflag;				/* [option]		*/

static void
usage()
{
	fprintf(stderr, "usage:\t%s\n\t%s\n\t%s\n\t%s\n\t%s\n\t%s\n\t%s\n",
		"global [-alrvx] pattern",
		"global -c [prefix]",
		"global -f[arx] file",
		"global -g[alvx] pattern",
		"global -i[v]",
		"global -p",
		"global -s[alvx] pattern");
	exit(1);
}

void
main(argc, argv)
int	argc;
char	*argv[];
{
	char	*p, *av;
	int	count;
	int	db;
	char	cwd[MAXPATHLEN+1];		/* current directory	*/
	char	root[MAXPATHLEN+1];		/* root of source tree	*/
	char	dbpath[MAXPATHLEN+1];		/* dbpath directory	*/
	char	comline[MAXCOMLINE+1];

	while (--argc > 0 && (++argv)[0][0] == '-') {
		for (p = argv[0] + 1; *p; p++) {
			switch (*p) {
			case 'a':
				aflag++;
				break;
			case 'c':
				cflag++;
				break;
			case 'f':
				fflag++;
				break;
			case 'l':
				lflag++;
				break;
			case 'g':
				gflag++;
				break;
			case 'i':
				iflag++;
				break;
			case 'p':
				pflag++;
				break;
			case 'r':
				rflag++;
				break;
			case 's':
				sflag++;
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
	av = (argc > 0) ? *argv : (char *)0;
	/*
	 * usage check.
	 */
	{
		int	commands, options;

		commands = cflag + fflag + gflag + iflag + pflag + sflag;
		options  = aflag + lflag + rflag + xflag;
		/*
		 * only -c, -i and -p allows no argment.
		 */
		if (!av && !cflag && !iflag && !pflag)
			usage();
		/*
		 * command cannot be duplicated.
		 */
		if (commands > 1)
			usage();
		/*
		 * -c and -i command allows only -v option.
		 */
		if (cflag + iflag && options)
			usage();
		/*
		 * -r is not valid for -g, -i and -s.
		 */
		if (rflag && (gflag + iflag + sflag))
			usage();
	}
	/*
	 * remove leading blanks.
	 */
	if (av && !gflag)
		for (; *av == ' ' || *av == '\t'; av++)
			;
	if (cflag && av && regexp(av))
		die("regular expression not allowed with -c option.");
	/*
	 * get path of following directories.
	 *	o current directory
	 *	o root of source tree
	 *	o dbpath directory
	 *
	 * if GTAGS not found, getdbpath doesn't return.
	 */
	getdbpath(cwd, root, dbpath);

	if (pflag) {
		fprintf(stdout, "%s\n", dbpath);
		exit(0);
	}
	/*
	 * incremental update of tag files.
	 */
	if (iflag) {
		if (chdir(root) < 0)
			die1("cannot change directory to '%s'.", root);
		sprintf(comline, "gtags -i%s %s", (vflag) ? "v" : "", dbpath);
		if (system(comline))
			exit(1);
		exit(0);
	}

	/*
	 * complete function name
	 */
	if (cflag) {
		completelist(dbpath, av);
		exit(0);
	}
	/*
	 * make sort filter.
	 */
	if (sflag && xflag)
		*sortfilter = 0;
	else if (fflag)
		sprintf(sortfilter, "sort +1n -2");
	else if (xflag)				/* print details */
		sprintf(sortfilter, "sort +0 -1 +2 -3 +1n -2");
	else					/* print just file name */
		sprintf(sortfilter, "sort | uniq");
	/*
	 * make path filter.
	 */
	if (aflag)				/* absolute path name */
		sprintf(pathfilter, "sed -e 's!\\.!%s!'", root);
	else					/* relative path name */
		relative_filter(root, cwd, pathfilter);
	/*
	 * make local prefix.
	 */
	if (lflag) {
		/*
		 * getdbpath() assure follows.
		 * cwd != "/" and cwd includes root.
		 */
		strcpy(local, cwd);
		strcat(local, "/");
		localprefix = local + strlen(root) - 1;
		*localprefix = '.';
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
	db = (rflag) ? GRTAGS : ((sflag) ? GSYMS : GTAGS);
	/*
	 * print function definitions.
	 */
	if (fflag) {
		struct stat sb;
		char	pathbuf[MAXPATHLEN+1], *path;
		char	*p;
		FILE	*op;
		DBIO	*dbio;

		/* av !~ /\.[chysS]$/) */
		p = av + strlen(av) - 1;	/* last character */
		if (stat(av, &sb) || !S_ISREG(sb.st_mode))
			die1("file '%s' not found.", av);
		if (*(p - 1) != '.' || !locatestring("chysS", p, 0))
			die("accept only file name end with '.c .h .y .s .S'.");
		/*
		 * convert path into relative from root directory of source tree.
		 */
		path = realpath(av, pathbuf);
		if (*path != '/')
			die("realpath(3) is not compatible with BSD version.");
		if (strncmp(path, root, strlen(root)))
			die1("file '%s' is out of source tree.", path);
		path += strlen(root) - 1;
		*path = '.';
		if (!(op = popen(outfilter(), "w")))
			die("cannot open output pipe.");
		dbio = gtagsopen(dbpath, db, 0);
		for (p = db_first(dbio, NULL, DBIO_SKIPMETA); p; p = db_next(dbio))
			if (includepath(p, path))
				fprintf(op, "%s\n", p);
		db_close(dbio);
		pclose(op);	
		exit(0);
	}
	/*
	 * search in current source tree.
	 */
	count = search(av, cwd, root, dbpath, db);
	/*
	 * search in library path.
	 */
	if (count == 0 && !rflag && !sflag && !regexp(av) && getenv("GTAGSLIBPATH")) {
		char	envbuf[MAXENVLEN+1];
		char	libdbpath[MAXPATHLEN+1];
		char	*p, *lib;

		strcpy(envbuf, getenv("GTAGSLIBPATH"));
		p = envbuf;
		while (p) {
			lib = p;
			if ((p = locatestring(p, ":", 0)) != NULL)
				*p++ = 0;
			if (!strncmp(lib, cwd, strlen(cwd)) || !strncmp(cwd, lib, strlen(lib)))
				continue;
			if (!gtagsexist(lib, libdbpath))
				continue;
			if (!strcmp(dbpath, libdbpath))
				continue;
			if (aflag)		/* absolute path name */
				sprintf(pathfilter, "sed -e 's!\\.!%s!'", lib);
			else
				relative_filter(lib, cwd, pathfilter);
			count = search(av, cwd, lib, libdbpath, GTAGS);
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
 * outfilter: return output filter.
 *
 *	gi)	pathfilter
 *	gi)	sortfilter
 *	r)		output filter
 */
char	*
outfilter(void)
{
	static char	filter[MAXCOMLINE+1];

	/*
	 * make output filter
	 */
	if (*sortfilter)
		sprintf(filter, "%s | %s", sortfilter, pathfilter);
	else
		strcpy(filter, pathfilter);
	return filter;
}
/*
 * completelist: print complete list of function
 *
 *	i)	dbpath	dbpath directory
 *	i)	prefix	prefix of primary key
 */
void
completelist(dbpath, prefix)
char	*dbpath;
char	*prefix;
{
	char	*p;
	DBIO	*dbio;

	dbio = gtagsopen(dbpath, GTAGS, 0);
	for (p = db_first(dbio, prefix, DBIO_KEY|DBIO_PREFIX|DBIO_SKIPMETA); p; p = db_next(dbio))
		(void)fprintf(stdout, "%s\n", p);
	db_close(dbio);
}
/*
 * relative_filter: make relative path filter
 *
 *	i)	root	the root directory of source tree
 *	i)	argcwd	current directory
 *	o)	bp	result
 *			relative path filter
 */
void
relative_filter(root, cwd, bp)
char	*root;
char	*cwd;
char	*bp;
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
	if (*c == 0 && *p)
		die("illegal root.");
	/*
	 * forward to root.
	 */
	strcpy(bp, "sed -e 's!\\./!");
	for (c = branch; *c; c++)
		if (*c == '/')
			strcat(bp, "../");
	p = root + (branch - cwd);
	/*
	 * backward to leaf.
	 */
	if (*p) {
		p++;
		strcat(bp, p);
		strcat(bp, "/");
	}
	strcat(bp, "!'");
	/*
	 * remove redundancy.
	 */
	if (*branch) {
		char	unit[256];

		bp += strlen(bp);
		p = unit;
		for (c = branch + 1; ; c++) {
			if (*c == 0 || *c == '/') {
				*p = 0;
				sprintf(bp, " -e 's!\\.\\./%s/!!'", unit);
				bp += strlen(bp);
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
 *	i)	root	root of source tree
 *	i)	bp	tag's line
 *	i)	compact	0: standard format, 1: compact format
 *	r)		output line count
 */
int
printtag(op, root, bp, compact)
FILE	*op;
char	*root;
char	*bp;
int	compact;
{
	int	count = 0;
	char	*tag, *file, *lno;
	int	opened = 0;
	char	path[MAXPATHLEN+1];
	char	*buffer;
	int	line = 0, tagline = 0;
	FILE	*ip;

	if (!xflag) {
		fprintf(op, "%s\n", extractpath(bp));
		return 1;
	}
	if (compact) {			/* compact format */
		char	*p = bp;

		tag = p;			/* tag = $1 */
		for (; !isspace(*p) ; p++)
			;
		*p++ = 0;
		for (; isspace(*p) ; p++)
			;
		file = p;			/* file = $2 */
		for (; !isspace(*p) ; p++)
			;
		*p++ = 0;
		for (; isspace(*p) ; p++)
			;
		lno = p;			/* lno = $3 */
		sprintf(path, "%s/%s", root, file + 2);
		if ((ip = fopen(path, "r")) != NULL) {
			opened = 1;
			buffer = mgets(ip, 0, NULL);
			line = 1;
		} else {
			buffer = "";
		}
		while (*lno) {
			/* get line number */
			for (tagline = 0; *lno >= '0' && *lno <= '9'; lno++)
				tagline = tagline * 10 + *lno - '0';
			if (*lno == ',')
				lno++;
			if (opened) {
				while (line < tagline) {
					if (!(buffer = mgets(ip, 0, NULL)))
						die1("unexpected end of file. '%s'", path);
					line++;
				}
			}
			if (strlen(tag) >= 16 && tagline >= 1000)
				fprintf(op, "%-16s %4d %-16s %s\n",
						tag, tagline, file, buffer);
			else
				fprintf(op, "%-16s%4d %-16s %s\n",
						tag, tagline, file, buffer);
			count++;
		}
		if (opened)
			fclose(ip);
	} else {			/* standard format */
		/*
		 * separater in key part must be ' ' to avoid sort(1)'s bug.
		 */
		detab(op, bp);
		count++;
	}
	return count;
}

/*
 * regexp: test whether regular expression included.
 *
 *	i)	s	string
 *	r)		0: not included, 1: included
 *
 * This function cannot be used for generic purpose.
 * Any character except '[a-zA-Z_0-9]' is assumed RE char..
 */
int
regexp(s)
char	*s;
{
	int	c;

	while ((c = *s++) != NULL)
		if (	(c >= 'a' && c <= 'z')	||
			(c >= 'A' && c <= 'Z')	||
			(c >= '0' && c <= '9')	||
			(c == '_')		)
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
	char	*buffer, *p, *e;
	int	linenum, count, editlen;
	regex_t	preg;

	/*
	 * convert spaces into %FF format.
	 */
	e = edit;
	for (p = pattern; *p; p++) {
		if (*p == '%' || *p == ' ' || *p == '\t') {
			sprintf(e, "%%%02x", *p);
			e += 3;
		} else
			*e++ = *p;
	}
	*e = 0;
	editlen = strlen(edit);

	if (regcomp(&preg, pattern, REG_EXTENDED) != 0)
		die("illegal regular expression.");
	if (!(op = popen(outfilter(), "w")))
		die("cannot open output pipe.");
	count = 0;
	for (findopen(); (path = findread(NULL)) != NULL; ) {
		if (!(fp = fopen(path, "r")))
			die1("cannot open file '%s'.", path);
		linenum = 0;
		while ((buffer = mgets(fp, 0, NULL)) != NULL) {
			linenum++;
			if (regexec(&preg, buffer, 0, 0, 0) == 0) {
				count++;
				if (xflag == 0) {
					fprintf(op, "%s\n", path);
					break;
				}
				if (editlen >= 16 && linenum >= 1000)
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
	pclose(op);
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
	DBIO	*dbio;
	int	compact;
	regex_t	preg;

	/*
	 * open tag file.
	 * currently only GSYMS is compact format.
	 */
	dbio = gtagsopen(dbpath, db, 0);
	compact = (db == GSYMS) ? 1 : 0;
	if (!(op = popen(outfilter(), "w")))
		die1("filter '%s' failed.", outfilter());
	/*
	 * regular expression.
	 */
	if (regexp(pattern) && regcomp(&preg, pattern, REG_EXTENDED) == 0) {
		char	prefix_buf[IDENTLEN+1];
		char	*prefix = (char *)0;

		if (*pattern == '^' && *(p = pattern + 1) && (isalpha(*p) || *p == '_')) {
			prefix = prefix_buf;
			*prefix++ = *p++;
			while (*p && (isalpha(*p) || isdigit(*p) || *p == '_'))
				*prefix++ = *p++;
			*prefix = 0;
			prefix = prefix_buf;
			p = db_first(dbio, prefix, DBIO_SKIPMETA|DBIO_PREFIX);
		} else {
			p = db_first(dbio, NULL, DBIO_SKIPMETA);
		}
		for (; p; p = db_next(dbio)) {
			if (*p == ' ')
				continue;
			if (regexec(&preg, dbio->lastkey, 0, 0, 0) == 0)
				count += printtag(op, root, p, compact);
		}
	} else {
		for (p = db_first(dbio, pattern, 0); p; p = db_next(dbio)) {
			if (lflag) {
				char	*q;
				/* locate start point of a path */
				q = locatestring(p, "./", 0);
				if (!locatestring(q, localprefix, 1))
					continue;
			}
			count += printtag(op, root, p, compact);
		}
	}
	pclose(op);
	db_close(dbio);
	return count;
}
/*
 * extractpath: extract path string of a tag line
 *
 *	i)	line	tag line
 *	r)		path
 *
 * standard format:	main	12 ./xxx/xxx/xxx.c	main(argc, argv) {
 * compact format:	main ./xxx/xxx/xxx.c 12,15,55,101
 */
char	*
extractpath(line)
char	*line;
{
	static char buf[MAXPATHLEN+1];
	char	*p, *b;
	int	c;

	if (!(p = locatestring(line, "./", 0)))
		die("illegal tag format (path not found).");
	b = buf;
	while ((c = *b++ = *p++) != NULL) {
		if (c == ' ' || c == '\t') {
			*(b - 1) = 0;
			break;
		}
	}
	return buf;
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

	if (!(p = locatestring(line, "./", 0)))
		die("illegal tag format (path not found).");
	length = strlen(path);
	if (strncmp(p, path, length))
		return 0;
	p += length;
	if (*p == ' ' || *p == '\t')
		return 1;
	return 0;
}
