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
#include "y.tab.h"
#include "config.h"

#ifndef TRUE
#define TRUE	(1)
#endif

#ifndef FALSE
#define FALSE	(0)
#endif

#define	CDIR	"../../compile/"

char *	PREFIX;
char 	destdir[MAXPATHLEN];
char 	srcdir[MAXPATHLEN];

static int no_config_clobber = TRUE;
int	debugging;
int	profiling;

static void configfile __P((void));
static void get_srcdir __P((void));
static void usage __P((void));

/*
 * Config builds a set of files for building a UNIX
 * system given a description of the desired system.
 */
int
main(argc, argv)
	int argc;
	char **argv;
{

	struct stat buf;
	int ch, len;
	char *p;

	while ((ch = getopt(argc, argv, "d:gprn")) != -1)
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
		case 'n':
			/* no_config_clobber is now true by default, no-op */
			fprintf(stderr,
				"*** Using obsolete config option '-n' ***\n");
			break;
		case 'r':
			no_config_clobber = FALSE;
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
	else if ((buf.st_mode & S_IFMT) != S_IFDIR) {
		errx(2, "%s isn't a directory", p);
	}
	else if (!no_config_clobber) {
		char tmp[strlen(p) + 8];

		fprintf(stderr, "Removing old directory %s:  ", p);
		fflush(stderr);
		snprintf(tmp, sizeof(tmp), "rm -rf %s", p);
		if (system(tmp)) {
			fprintf(stderr, "Failed!\n");
			err(2, "%s", tmp);
		}
		fprintf(stderr, "Done.\n");
		if (mkdir(p, 0777))
			err(2, "%s", p);
	}

	dtab = NULL;
	if (yyparse())
		exit(3);
	switch (machine) {

	case MACHINE_I386:
	case MACHINE_PC98:
	case MACHINE_ALPHA:
		newbus_ioconf();	/* Print ioconf.c */
		break;

	default:
		printf("Specify machine type, e.g. ``machine i386''\n");
		exit(1);
	}
	/*
	 * make symbolic links in compilation directory
	 * for "sys" (to make genassym.c work along with #include <sys/xxx>)
	 * and similarly for "machine".
	 */
	{
	char xxx[MAXPATHLEN];
	if (*srcdir == '\0')
		(void)snprintf(xxx, sizeof(xxx), "../../%s/include",
		    machinename);
	else
		(void)snprintf(xxx, sizeof(xxx), "%s/%s/include",
		    srcdir, machinename);
	(void) symlink(xxx, path("machine"));
	}
	options();			/* make options .h files */
	makefile();			/* build Makefile */
	headers();			/* make a lot of .h files */
	configfile();			/* put config file into kernel*/
	printf("Kernel build directory is %s\n", p);
	exit(0);
}

/*
 * get_srcdir
 *	determine the root of the kernel source tree
 *	and save that in srcdir.
 */
static void
get_srcdir()
{
	if (realpath("../..", srcdir) == NULL)
		errx(2, "Unable to find root of source tree");
}

static void
usage()
{
		fprintf(stderr, "usage: config [-gpr] [-d destdir] sysname\n");
		exit(1);
}

/*
 * get_word
 *	returns EOF on end of file
 *	NULL on end of line
 *	pointer to the word otherwise
 */
char *
get_word(fp)
	register FILE *fp;
{
	static char line[80];
	register int ch;
	register char *cp;
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
get_quoted_word(fp)
	register FILE *fp;
{
	static char line[256];
	register int ch;
	register char *cp;
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
		register int quote = ch;

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
path(file)
	char *file;
{
	register char *cp;

	cp = malloc((size_t)(strlen(destdir) + (file ? strlen(file) : 0) + 2));
	(void) strcpy(cp, destdir);
	if (file) {
		(void) strcat(cp, "/");
		(void) strcat(cp, file);
	}
	return (cp);
}

static void
configfile()
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
	fprintf(fo, "static const char config[] = \"\\\n");
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
