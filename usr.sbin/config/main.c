/*
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
"@(#) Copyright (c) 1980, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)main.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <sysexits.h>
#include <unistd.h>
#include <dirent.h>
#include "y.tab.h"
#include "config.h"

#ifndef TRUE
#define TRUE	(1)
#endif

#ifndef FALSE
#define FALSE	(0)
#endif

#define	CDIR	"../compile/"

char *	PREFIX;
char 	destdir[MAXPATHLEN];
char 	srcdir[MAXPATHLEN];

int	debugging;
int	profiling;

static void configfile(void);
static void get_srcdir(void);
static void usage(void);
static void cleanheaders(char *);

struct hdr_list {
	char *h_name;
	struct hdr_list *h_next;
} *htab;

/*
 * Config builds a set of files for building a UNIX
 * system given a description of the desired system.
 */
int
main(int argc, char **argv)
{

	struct stat buf;
	int ch, len;
	char *p;
	char xxx[MAXPATHLEN];

	while ((ch = getopt(argc, argv, "d:gp")) != -1)
		switch (ch) {
		case 'd':
			if (*destdir == '\0')
				strlcpy(destdir, optarg, sizeof(destdir));
			else
				errx(2, "directory already set");
			break;
		case 'g':
			debugging++;
			break;
		case 'p':
			profiling++;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	if (freopen(PREFIX = *argv, "r", stdin) == NULL)
		err(2, "%s", PREFIX);

	if (*destdir != '\0') {
		len = strlen(destdir);
		while (len > 1 && destdir[len - 1] == '/')
			destdir[--len] = '\0';
		get_srcdir();
	} else {
		strlcpy(destdir, CDIR, sizeof(destdir));
		strlcat(destdir, PREFIX, sizeof(destdir));
	}

	p = path((char *)NULL);
	if (stat(p, &buf)) {
		if (mkdir(p, 0777))
			err(2, "%s", p);
	}
	else if ((buf.st_mode & S_IFMT) != S_IFDIR)
		errx(2, "%s isn't a directory", p);

	STAILQ_INIT(&dtab);
	STAILQ_INIT(&fntab);
	SLIST_INIT(&cputype);
	STAILQ_INIT(&ftab);
	yyfile = *argv;
	if (yyparse())
		exit(3);
	if (machinename == NULL) {
		printf("Specify machine type, e.g. ``machine i386''\n");
		exit(1);
	}
	/*
	 * make symbolic links in compilation directory
	 * for "sys" (to make genassym.c work along with #include <sys/xxx>)
	 * and similarly for "machine".
	 */
	if (*srcdir == '\0')
		(void)snprintf(xxx, sizeof(xxx), "../../include");
	else
		(void)snprintf(xxx, sizeof(xxx), "%s/%s/include",
		    srcdir, machinename);
	(void) unlink(path("machine"));
	(void) symlink(xxx, path("machine"));
	if (strcmp(machinename, machinearch) != 0) {
		/*
		 * make symbolic links in compilation directory for
		 * machinearch, if it is different than machinename.
		 */
		if (*srcdir == '\0')
			(void)snprintf(xxx, sizeof(xxx), "../../../%s/include",
			    machinearch);
		else
			(void)snprintf(xxx, sizeof(xxx), "%s/%s/include",
			    srcdir, machinearch);
		(void) unlink(path(machinearch));
		(void) symlink(xxx, path(machinearch));
	}
	options();			/* make options .h files */
	makefile();			/* build Makefile */
	headers();			/* make a lot of .h files */
	configfile();			/* put config file into kernel*/
	cleanheaders(p);
	printf("Kernel build directory is %s\n", p);
	printf("Don't forget to do a ``make depend''\n");
	exit(0);
}

/*
 * get_srcdir
 *	determine the root of the kernel source tree
 *	and save that in srcdir.
 */
static void
get_srcdir(void)
{

	if (realpath("../..", srcdir) == NULL)
		errx(2, "Unable to find root of source tree");
}

static void
usage(void)
{

	fprintf(stderr, "usage: config [-gp] [-d destdir] sysname\n");
	exit(1);
}

/*
 * get_word
 *	returns EOF on end of file
 *	NULL on end of line
 *	pointer to the word otherwise
 */
char *
get_word(FILE *fp)
{
	static char line[80];
	int ch;
	char *cp;
	int escaped_nl = 0;

begin:
	while ((ch = getc(fp)) != EOF)
		if (ch != ' ' && ch != '\t')
			break;
	if (ch == EOF)
		return ((char *)EOF);
	if (ch == '\\'){
		escaped_nl = 1;
		goto begin;
	}
	if (ch == '\n') {
		if (escaped_nl){
			escaped_nl = 0;
			goto begin;
		}
		else
			return (NULL);
	}
	cp = line;
	*cp++ = ch;
	while ((ch = getc(fp)) != EOF) {
		if (isspace(ch))
			break;
		*cp++ = ch;
	}
	*cp = 0;
	if (ch == EOF)
		return ((char *)EOF);
	(void) ungetc(ch, fp);
	return (line);
}

/*
 * get_quoted_word
 *	like get_word but will accept something in double or single quotes
 *	(to allow embedded spaces).
 */
char *
get_quoted_word(FILE *fp)
{
	static char line[256];
	int ch;
	char *cp;
	int escaped_nl = 0;

begin:
	while ((ch = getc(fp)) != EOF)
		if (ch != ' ' && ch != '\t')
			break;
	if (ch == EOF)
		return ((char *)EOF);
	if (ch == '\\'){
		escaped_nl = 1;
		goto begin;
	}
	if (ch == '\n') {
		if (escaped_nl){
			escaped_nl = 0;
			goto begin;
		}
		else
			return (NULL);
	}
	cp = line;
	if (ch == '"' || ch == '\'') {
		int quote = ch;

		while ((ch = getc(fp)) != EOF) {
			if (ch == quote)
				break;
			if (ch == '\n') {
				*cp = 0;
				printf("config: missing quote reading `%s'\n",
					line);
				exit(2);
			}
			*cp++ = ch;
		}
	} else {
		*cp++ = ch;
		while ((ch = getc(fp)) != EOF) {
			if (isspace(ch))
				break;
			*cp++ = ch;
		}
		if (ch != EOF)
			(void) ungetc(ch, fp);
	}
	*cp = 0;
	if (ch == EOF)
		return ((char *)EOF);
	return (line);
}

/*
 * prepend the path to a filename
 */
char *
path(const char *file)
{
	char *cp = NULL;

	if (file)
		asprintf(&cp, "%s/%s", destdir, file);
	else
		cp = strdup(destdir);
	return (cp);
}

static void
configfile(void)
{
	FILE *fi, *fo;
	char *p;
	int i;
	
	fi = fopen(PREFIX, "r");
	if (!fi)
		err(2, "%s", PREFIX);
	fo = fopen(p=path("config.c.new"), "w");
	if (!fo)
		err(2, "%s", p);
	fprintf(fo, "#include \"opt_config.h\"\n");
	fprintf(fo, "#ifdef INCLUDE_CONFIG_FILE \n");
	fprintf(fo, "const char config[] = \"\\\n");
	fprintf(fo, "START CONFIG FILE %s\\n\\\n___", PREFIX);
	while (EOF != (i=getc(fi))) {
		if (i == '\n') {
			fprintf(fo, "\\n\\\n___");
		} else if (i == '\"') {
			fprintf(fo, "\\\"");
		} else if (i == '\\') {
			fprintf(fo, "\\\\");
		} else {
			putc(i, fo);
		}
	}
	fprintf(fo, "\\n\\\nEND CONFIG FILE %s\\n\\\n", PREFIX);
	fprintf(fo, "\";\n");
	fprintf(fo, "\n#endif /* INCLUDE_CONFIG_FILE */\n");
	fclose(fi);
	fclose(fo);
	moveifchanged(path("config.c.new"), path("config.c"));
}

/*
 * moveifchanged --
 *	compare two files; rename if changed.
 */
void
moveifchanged(const char *from_name, const char *to_name)
{
	char *p, *q;
	int changed;
	size_t tsize;
	struct stat from_sb, to_sb;
	int from_fd, to_fd;

	changed = 0;

	if ((from_fd = open(from_name, O_RDONLY)) < 0)
		err(EX_OSERR, "moveifchanged open(%s)", from_name);

	if ((to_fd = open(to_name, O_RDONLY)) < 0)
		changed++;

	if (!changed && fstat(from_fd, &from_sb) < 0)
		err(EX_OSERR, "moveifchanged fstat(%s)", from_name);

	if (!changed && fstat(to_fd, &to_sb) < 0)
		err(EX_OSERR, "moveifchanged fstat(%s)", to_name);

	if (!changed && from_sb.st_size != to_sb.st_size)
		changed++;

	tsize = (size_t)from_sb.st_size;

	if (!changed) {
		p = mmap(NULL, tsize, PROT_READ, MAP_SHARED, from_fd, (off_t)0);
#ifndef MAP_FAILED
#define MAP_FAILED ((caddr_t) -1)
#endif
		if (p == MAP_FAILED)
			err(EX_OSERR, "mmap %s", from_name);
		q = mmap(NULL, tsize, PROT_READ, MAP_SHARED, to_fd, (off_t)0);
		if (q == MAP_FAILED)
			err(EX_OSERR, "mmap %s", to_name);

		changed = memcmp(p, q, tsize);
		munmap(p, tsize);
		munmap(q, tsize);
	}
	if (changed) {
		if (rename(from_name, to_name) < 0)
			err(EX_OSERR, "rename(%s, %s)", from_name, to_name);
	} else {
		if (unlink(from_name) < 0)
			err(EX_OSERR, "unlink(%s)", from_name);
	}
}

static void
cleanheaders(char *p)
{
	DIR *dirp;
	struct dirent *dp;
	struct file_list *fl;
	struct hdr_list *hl;
	int i;

	remember("y.tab.h");
	remember("setdefs.h");
	STAILQ_FOREACH(fl, &ftab, f_next)
		remember(fl->f_fn);

	/*
	 * Scan the build directory and clean out stuff that looks like
	 * it might have been a leftover NFOO header, etc.
	 */
	if ((dirp = opendir(p)) == NULL)
		err(EX_OSERR, "opendir %s", p);
	while ((dp = readdir(dirp)) != NULL) {
		i = dp->d_namlen - 2;
		/* Skip non-headers */
		if (dp->d_name[i] != '.' || dp->d_name[i + 1] != 'h')
			continue;
		/* Skip special stuff, eg: bus_if.h, but check opt_*.h */
		if (index(dp->d_name, '_') &&
		    strncmp(dp->d_name, "opt_", 4) != 0)
			continue;
		/* Check if it is a target file */
		for (hl = htab; hl != NULL; hl = hl->h_next) {
			if (strcmp(dp->d_name, hl->h_name) == 0) {
				break;
			}
		}
		if (hl)
			continue;
		printf("Removing stale header: %s\n", dp->d_name);
		if (unlink(path(dp->d_name)) == -1)
			warn("unlink %s", dp->d_name);
	}
	(void)closedir(dirp);
}

void
remember(const char *file)
{
	char *s;
	struct hdr_list *hl;

	if ((s = strrchr(file, '/')) != NULL)
		s = ns(s + 1);
	else
		s = ns(file);

	if (index(s, '_') && strncmp(s, "opt_", 4) != 0) {
		free(s);
		return;
	}
	for (hl = htab; hl != NULL; hl = hl->h_next) {
		if (strcmp(s, hl->h_name) == 0) {
			free(s);
			return;
		}
	}
	hl = malloc(sizeof(*hl));
	bzero(hl, sizeof(*hl));
	hl->h_name = s;
	hl->h_next = htab;
	htab = hl;
}
