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
static char copyright[] =
"@(#) Copyright (c) 1980, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)main.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <stdio.h>
#include <ctype.h>
#include "y.tab.h"
#include "config.h"

#ifndef TRUE
#define TRUE	(1)
#endif

#ifndef FALSE
#define FALSE	(0)
#endif

static char *PREFIX;
static int no_config_clobber = FALSE;
int old_config_present;

/*
 * Config builds a set of files for building a UNIX
 * system given a description of the desired system.
 */
main(argc, argv)
	int argc;
	char **argv;
{

	extern char *optarg;
	extern int optind;
	struct stat buf;
	int ch;
	char *p;

	while ((ch = getopt(argc, argv, "gpn")) != EOF)
		switch (ch) {
		case 'g':
			debugging++;
			break;
		case 'p':
			profiling++;
			break;
		case 'n':
			no_config_clobber = TRUE;
			break;
		case '?':
		default:
			goto usage;
		}
	argc -= optind;
	argv += optind;

	if (argc != 1) {
usage:		fputs("usage: config [-gpn] sysname\n", stderr);
		exit(1);
	}

	if (freopen(PREFIX = *argv, "r", stdin) == NULL) {
		perror(PREFIX);
		exit(2);
	}
	if (getenv("NO_CONFIG_CLOBBER"))
		no_config_clobber = TRUE;

	p = path((char *)NULL);
	if (stat(p, &buf)) {
		if (mkdir(p, 0777)) {
			perror(p);
			exit(2);
		}
	}
	else if ((buf.st_mode & S_IFMT) != S_IFDIR) {
		fprintf(stderr, "config: %s isn't a directory.\n", p);
		exit(2);
	}
#ifndef NO_CLOBBER_EVER
	else if (!no_config_clobber) {
		char tmp[strlen(p) + 8];

		fprintf(stderr, "Removing old directory %s:  ", p);
		fflush(stderr);
		sprintf(tmp, "rm -rf %s", p);
		if (system(tmp)) {
			fprintf(stderr, "Failed!\n");
			perror(tmp);
			exit(2);
		}
		fprintf(stderr, "Done.\n");
		if (mkdir(p, 0777)) {
			perror(p);
			exit(2);
		}
	}
#endif
	else
		old_config_present++;

	loadaddress = -1;
	dtab = NULL;
	confp = &conf_list;
	compp = &comp_list;
	if (yyparse())
		exit(3);
	switch (machine) {

	case MACHINE_VAX:
		vax_ioconf();		/* Print ioconf.c */
		ubglue();		/* Create ubglue.s */
		break;

	case MACHINE_TAHOE:
		tahoe_ioconf();
		vbglue();
		break;

	case MACHINE_HP300:
	case MACHINE_LUNA68K:
		hp300_ioconf();
		hpglue();
		break;

	case MACHINE_I386:
		i386_ioconf();		/* Print ioconf.c */
		vector();		/* Create vector.s */
		break;

	case MACHINE_MIPS:
	case MACHINE_PMAX:
		pmax_ioconf();
		break;

	case MACHINE_NEWS3400:
		news_ioconf();
		break;

	default:
		printf("Specify machine type, e.g. ``machine vax''\n");
		exit(1);
	}
	/*
	 * make symbolic links in compilation directory
	 * for "sys" (to make genassym.c work along with #include <sys/xxx>)
	 * and similarly for "machine".
	 */
	{
	char xxx[80];

	(void) sprintf(xxx, "../../%s/include", machinename);
	(void) symlink(xxx, path("machine"));
	}
	options();			/* make options .h files */
	makefile();			/* build Makefile */
	headers();			/* make a lot of .h files */
	swapconf();			/* swap config files */
	printf("Kernel build directory is %s\n", p);
	exit(0);
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
	if (ch == '\n')
		if (escaped_nl){
			escaped_nl = 0;
			goto begin;
		}
		else
			return (NULL);
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
	if (ch == '\n')
		if (escaped_nl){
			escaped_nl = 0;
			goto begin;
		}
		else
			return (NULL);
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

#define	CDIR	"../../compile/"
	cp = malloc((unsigned int)(sizeof(CDIR) + strlen(PREFIX) +
	    (file ? strlen(file) : 0) + 2));
	(void) strcpy(cp, CDIR);
	(void) strcat(cp, PREFIX);
	if (file) {
		(void) strcat(cp, "/");
		(void) strcat(cp, file);
	}
	return (cp);
}
