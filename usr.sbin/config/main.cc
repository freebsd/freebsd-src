/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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
 * 3. Neither the name of the University nor the names of its contributors
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
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/param.h>

#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "y.tab.h"
#include "config.h"
#include "configvers.h"

#ifndef TRUE
#define TRUE	(1)
#endif

#ifndef FALSE
#define FALSE	(0)
#endif

#define	CDIR	"../compile/"

char	*machinename;
char	*machinearch;

struct cfgfile_head	cfgfiles;
struct cputype_head	cputype;
struct opt_head		opt, mkopt, rmopts;
struct opt_list_head	otab;
struct envvar_head	envvars;
struct hint_head	hints;
struct includepath_head	includepath;

char *	PREFIX;
char 	destdir[MAXPATHLEN];
char 	srcdir[MAXPATHLEN];

int	debugging;
int	found_defaults;
int	incignore;

/*
 * Preserve old behaviour in INCLUDE_CONFIG_FILE handling (files are included
 * literally).
 */
int	filebased = 0;
int	versreq;

static void configfile(void);
static void get_srcdir(void);
static void usage(void);
static void cleanheaders(char *);
static void kernconfdump(const char *);
static void badversion(void);
static void checkversion(void);

struct hdr_list {
	const char *h_name;
	struct hdr_list *h_next;
} *htab;

static std::stringstream line_buf;

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
	char *kernfile;
	struct includepath* ipath;
	int printmachine;
	bool cust_dest = false;

	printmachine = 0;
	kernfile = NULL;
	SLIST_INIT(&includepath);
	SLIST_INIT(&cputype);
	SLIST_INIT(&mkopt);
	SLIST_INIT(&opt);
	SLIST_INIT(&rmopts);
	STAILQ_INIT(&cfgfiles);
	STAILQ_INIT(&dtab);
	STAILQ_INIT(&fntab);
	STAILQ_INIT(&ftab);
	STAILQ_INIT(&hints);
	STAILQ_INIT(&envvars);
	while ((ch = getopt(argc, argv, "Cd:gI:mps:Vx:")) != -1)
		switch (ch) {
		case 'C':
			filebased = 1;
			break;
		case 'd':
			if (*destdir == '\0')
				strlcpy(destdir, optarg, sizeof(destdir));
			else
				errx(EXIT_FAILURE, "directory already set");
			cust_dest = true;
			break;
		case 'g':
			debugging++;
			break;
		case 'I':
			ipath = (struct includepath *) \
			    	calloc(1, sizeof (struct includepath));
			if (ipath == NULL)
				err(EXIT_FAILURE, "calloc");
			ipath->path = optarg;
			SLIST_INSERT_HEAD(&includepath, ipath, path_next);
			break;
		case 'm':
			printmachine = 1;
			break;
		case 's':
			if (*srcdir == '\0')
				strlcpy(srcdir, optarg, sizeof(srcdir));
			else
				errx(EXIT_FAILURE, "src directory already set");
			break;
		case 'V':
			printf("%d\n", CONFIGVERS);
			exit(0);
		case 'x':
			kernfile = optarg;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (kernfile != NULL) {
		kernconfdump(kernfile);
		exit(EXIT_SUCCESS);
	}

	if (argc != 1)
		usage();

	PREFIX = *argv;
	if (stat(PREFIX, &buf) != 0 || !S_ISREG(buf.st_mode))
		err(2, "%s", PREFIX);
	if (freopen("DEFAULTS", "r", stdin) != NULL) {
		found_defaults = 1;
		yyfile = "DEFAULTS";
	} else {
		if (freopen(PREFIX, "r", stdin) == NULL)
			err(2, "%s", PREFIX);
		yyfile = PREFIX;
	}
	if (*destdir != '\0') {
		len = strlen(destdir);
		while (len > 1 && destdir[len - 1] == '/')
			destdir[--len] = '\0';
		if (*srcdir == '\0')
			get_srcdir();
	} else {
		strlcpy(destdir, CDIR, sizeof(destdir));
		strlcat(destdir, PREFIX, sizeof(destdir));
	}

	if (yyparse())
		exit(3);

	/*
	 * Ensure that required elements (machine, cpu, ident) are present.
	 */
	if (machinename == NULL) {
		printf("Specify machine type, e.g. ``machine i386''\n");
		exit(1);
	}
	if (ident == NULL) {
		printf("no ident line specified\n");
		exit(1);
	}
	if (SLIST_EMPTY(&cputype)) {
		printf("cpu type must be specified\n");
		exit(1);
	}
	checkversion();

	if (printmachine) {
		printf("%s\t%s\n",machinename,machinearch);
		exit(0);
	}

	/*
	 * Make CDIR directory, if doing a default destination. Some version
	 * control systems delete empty directories and this seemlessly copes.
	 */
	if (!cust_dest && stat(CDIR, &buf))
		if (mkdir(CDIR, 0777))
			err(2, "%s", CDIR);
	/* Create the compile directory */
	p = path((char *)NULL);
	if (stat(p, &buf)) {
		if (mkdir(p, 0777))
			err(2, "%s", p);
	} else if (!S_ISDIR(buf.st_mode))
		errx(EXIT_FAILURE, "%s isn't a directory", p);

	configfile();			/* put config file into kernel*/
	options();			/* make options .h files */
	makefile();			/* build Makefile */
	makeenv();			/* build env.c */
	makehints();			/* build hints.c */
	headers();			/* make a lot of .h files */
	cleanheaders(p);
	printf("Kernel build directory is %s\n", p);
	printf("Don't forget to do ``make cleandepend && make depend''\n");
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
	struct stat lg, phy;
	char *p, *pwd;
	int i;

	if (realpath("../..", srcdir) == NULL)
		err(EXIT_FAILURE, "Unable to find root of source tree");
	if ((pwd = getenv("PWD")) != NULL && *pwd == '/' &&
	    (pwd = strdup(pwd)) != NULL) {
		/* Remove the last two path components. */
		for (i = 0; i < 2; i++) {
			if ((p = strrchr(pwd, '/')) == NULL) {
				free(pwd);
				return;
			}
			*p = '\0';
		}
		if (stat(pwd, &lg) != -1 && stat(srcdir, &phy) != -1 &&
		    lg.st_dev == phy.st_dev && lg.st_ino == phy.st_ino)
			strlcpy(srcdir, pwd, MAXPATHLEN);
		free(pwd);
	}
}

static void
usage(void)
{

	fprintf(stderr,
	    "usage: config [-CgmpV] [-d destdir] [-s srcdir] sysname\n");
	fprintf(stderr, "       config -x kernel\n");
	exit(EX_USAGE);
}

static void
init_line_buf(void)
{

	line_buf.str("");
}

static std::string
get_line_buf(void)
{

	line_buf.flush();
	if (!line_buf.good()) {
		errx(EXIT_FAILURE, "failed to generate line buffer, "
		    "partial line = %s", line_buf.str().c_str());
	}

	return line_buf.str();
}

/*
 * get_word
 *	returns EOF on end of file
 *	NULL on end of line
 *	pointer to the word otherwise
 */
configword
get_word(FILE *fp)
{
	int ch;
	int escaped_nl = 0;

	init_line_buf();
begin:
	while ((ch = getc(fp)) != EOF)
		if (ch != ' ' && ch != '\t')
			break;
	if (ch == EOF)
		return (configword().eof(true));
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
			return (configword().eol(true));
	}
	line_buf << (char)ch;
	/* Negation operator is a word by itself. */
	if (ch == '!') {
		return (configword(get_line_buf()));
	}
	while ((ch = getc(fp)) != EOF) {
		if (isspace(ch))
			break;
		line_buf << (char)ch;
	}
	if (ch == EOF)
		return (configword().eof(true));
	(void) ungetc(ch, fp);
	return (configword(get_line_buf()));
}

/*
 * get_quoted_word
 *	like get_word but will accept something in double or single quotes
 *	(to allow embedded spaces).
 */
configword
get_quoted_word(FILE *fp)
{
	int ch;
	int escaped_nl = 0;

	init_line_buf();
begin:
	while ((ch = getc(fp)) != EOF)
		if (ch != ' ' && ch != '\t')
			break;
	if (ch == EOF)
		return (configword().eof(true));
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
			return (configword().eol(true));
	}
	if (ch == '"' || ch == '\'') {
		int quote = ch;

		escaped_nl = 0;
		while ((ch = getc(fp)) != EOF) {
			if (ch == quote && !escaped_nl)
				break;
			if (ch == '\n' && !escaped_nl) {
				printf("config: missing quote reading `%s'\n",
					get_line_buf().c_str());
				exit(2);
			}
			if (ch == '\\' && !escaped_nl) {
				escaped_nl = 1;
				continue;
			}
			if (ch != quote && escaped_nl)
				line_buf << "\\";
			line_buf << (char)ch;
			escaped_nl = 0;
		}
	} else {
		line_buf << (char)ch;
		while ((ch = getc(fp)) != EOF) {
			if (isspace(ch))
				break;
			line_buf << (char)ch;
		}
		if (ch != EOF)
			(void) ungetc(ch, fp);
	}
	if (ch == EOF)
		return (configword().eof(true));
	return (configword(get_line_buf()));
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
	if (cp == NULL)
		err(EXIT_FAILURE, "malloc");
	return (cp);
}

/*
 * Generate configuration file based on actual settings. With this mode, user
 * will be able to obtain and build conifguration file with one command.
 */
static void
configfile_dynamic(std::ostringstream &cfg)
{
	struct cputype *cput;
	struct device *d;
	struct opt *ol;
	char *lend;
	unsigned int i;

	asprintf(&lend, "\\n\\\n");
	assert(lend != NULL);
	cfg << "options\t" << OPT_AUTOGEN << lend;
	cfg << "ident\t" << ident << lend;
	cfg << "machine\t" << machinename << lend;
	SLIST_FOREACH(cput, &cputype, cpu_next)
		cfg << "cpu\t" << cput->cpu_name << lend;
	SLIST_FOREACH(ol, &mkopt, op_next)
		cfg << "makeoptions\t" << ol->op_name << '=' <<
		    ol->op_value << lend;
	SLIST_FOREACH(ol, &opt, op_next) {
		if (strncmp(ol->op_name, "DEV_", 4) == 0)
			continue;
		cfg << "options\t" << ol->op_name;
		if (ol->op_value != NULL) {
			cfg << '=';
			for (i = 0; i < strlen(ol->op_value); i++) {
				if (ol->op_value[i] == '"')
					cfg << '\\' << ol->op_value[i];
				else
					cfg << ol->op_value[i];
			}
		}

		cfg << lend;
	}
	/*
	 * Mark this file as containing everything we need.
	 */
	STAILQ_FOREACH(d, &dtab, d_next)
		cfg << "device\t" << d->d_name << lend;
	free(lend);
}

/*
 * Generate file from the configuration files.
 */
static void
configfile_filebased(std::ostringstream &cfg)
{
	FILE *cff;
	struct cfgfile *cf;
	int i;

	/*
	 * Try to read all configuration files. Since those will be present as
	 * C string in the macro, we have to slash their ends then the line
	 * wraps.
	 */
	STAILQ_FOREACH(cf, &cfgfiles, cfg_next) {
		cff = fopen(cf->cfg_path, "r");
		if (cff == NULL) {
			warn("Couldn't open file %s", cf->cfg_path);
			continue;
		}
		while ((i = getc(cff)) != EOF) {
			if (i == '\n')
				cfg << "\\n\\\n";
			else if (i == '"' || i == '\'')
				cfg << '\\' << i;
			else
				cfg << i;
		}
		fclose(cff);
	}
}

static void
configfile(void)
{
	FILE *fo;
	std::ostringstream cfg;
	char *p;

	/* Add main configuration file to the list of files to be included */
	cfgfile_add(PREFIX);
	p = path("config.c.new");
	fo = fopen(p, "w");
	if (!fo)
		err(2, "%s", p);
	free(p);

	if (filebased) {
		/* Is needed, can be used for backward compatibility. */
		configfile_filebased(cfg);
	} else {
		configfile_dynamic(cfg);
	}

	cfg.flush();
	/* 
	 * We print first part of the template, replace our tag with
	 * configuration files content and later continue writing our
	 * template.
	 */
	p = strstr(kernconfstr, KERNCONFTAG);
	if (p == NULL)
		errx(EXIT_FAILURE, "Something went terribly wrong!");
	*p = '\0';
	fprintf(fo, "%s", kernconfstr);
	fprintf(fo, "%s", cfg.str().c_str());
	p += strlen(KERNCONFTAG);
	fprintf(fo, "%s", p);
	fclose(fo);
	moveifchanged("config.c.new", "config.c");
	cfgfile_removeall();
}

/*
 * moveifchanged --
 *	compare two files; rename if changed.
 */
void
moveifchanged(const char *from_name, const char *to_name)
{
	char *p, *q;
	char *from_path, *to_path;
	int changed;
	size_t tsize;
	struct stat from_sb, to_sb;
	int from_fd, to_fd;

	changed = 0;

	from_path = path(from_name);
	to_path = path(to_name);
	if ((from_fd = open(from_path, O_RDONLY)) < 0)
		err(EX_OSERR, "moveifchanged open(%s)", from_name);

	if ((to_fd = open(to_path, O_RDONLY)) < 0)
		changed++;

	if (!changed && fstat(from_fd, &from_sb) < 0)
		err(EX_OSERR, "moveifchanged fstat(%s)", from_path);

	if (!changed && fstat(to_fd, &to_sb) < 0)
		err(EX_OSERR, "moveifchanged fstat(%s)", to_path);

	if (!changed && from_sb.st_size != to_sb.st_size)
		changed++;

	if (!changed) {
		tsize = (size_t)from_sb.st_size;

		p = (char *)mmap(NULL, tsize, PROT_READ, MAP_SHARED, from_fd,
		    (off_t)0);
		if (p == MAP_FAILED)
			err(EX_OSERR, "mmap %s", from_path);
		q = (char *)mmap(NULL, tsize, PROT_READ, MAP_SHARED, to_fd,
		    (off_t)0);
		if (q == MAP_FAILED)
			err(EX_OSERR, "mmap %s", to_path);

		changed = memcmp(p, q, tsize);
		munmap(p, tsize);
		munmap(q, tsize);
	}

	if (changed) {
		if (rename(from_path, to_path) < 0)
			err(EX_OSERR, "rename(%s, %s)", from_path, to_path);
	} else {
		if (unlink(from_path) < 0)
			err(EX_OSERR, "unlink(%s)", from_path);
	}

	close(from_fd);
	if (to_fd >= 0)
		close(to_fd);

	free(from_path);
	free(to_path);
}

static void
cleanheaders(char *p)
{
	DIR *dirp;
	struct dirent *dp;
	struct file_list *fl;
	struct hdr_list *hl;
	size_t len;

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
		len = strlen(dp->d_name);
		/* Skip non-headers */
		if (len < 2 || dp->d_name[len - 2] != '.' ||
		    dp->d_name[len - 1] != 'h')
			continue;
		/* Skip special stuff, eg: bus_if.h, but check opt_*.h */
		if (strchr(dp->d_name, '_') &&
		    strncmp(dp->d_name, "opt_", 4) != 0)
			continue;
		/* Check if it is a target file */
		for (hl = htab; hl != NULL; hl = hl->h_next) {
			if (eq(dp->d_name, hl->h_name)) {
				break;
			}
		}
		if (hl)
			continue;
		printf("Removing stale header: %s\n", dp->d_name);
		p = path(dp->d_name);
		if (unlink(p) == -1)
			warn("unlink %s", dp->d_name);
		free(p);
	}
	(void)closedir(dirp);
}

void
remember(const char *file)
{
	const char *s;
	struct hdr_list *hl;

	if ((s = strrchr(file, '/')) != NULL)
		s = ns(s + 1);
	else
		s = ns(file);

	if (strchr(s, '_') && strncmp(s, "opt_", 4) != 0) {
		free(__DECONST(char *, s));
		return;
	}
	for (hl = htab; hl != NULL; hl = hl->h_next) {
		if (eq(s, hl->h_name)) {
			free(__DECONST(char *, s));
			return;
		}
	}
	hl = (struct hdr_list *)calloc(1, sizeof(*hl));
	if (hl == NULL)
		err(EXIT_FAILURE, "calloc");
	hl->h_name = s;
	hl->h_next = htab;
	htab = hl;
}

/*
 * This one is quick hack. Will be probably moved to elf(3) interface.
 * It takes kernel configuration file name, passes it as an argument to
 * elfdump -a, which output is parsed by some UNIX tools...
 */
static void
kernconfdump(const char *file)
{
	struct stat st;
	FILE *fp, *pp;
	int error, osz, r;
	size_t i, off, size, t1, t2, align;
	char *cmd, *o;

	r = open(file, O_RDONLY);
	if (r == -1)
		err(EXIT_FAILURE, "Couldn't open file '%s'", file);
	error = fstat(r, &st);
	if (error == -1)
		err(EXIT_FAILURE, "fstat() failed");
	if (S_ISDIR(st.st_mode))
		errx(EXIT_FAILURE, "'%s' is a directory", file);
	fp = fdopen(r, "r");
	if (fp == NULL)
		err(EXIT_FAILURE, "fdopen() failed");
	osz = 1024;
	o = (char *)calloc(1, osz);
	if (o == NULL)
		err(EXIT_FAILURE, "Couldn't allocate memory");
	/* ELF note section header. */
	asprintf(&cmd, "/usr/bin/elfdump -c %s | grep -A 8 kern_conf"
	    "| tail -5 | cut -d ' ' -f 2 | paste - - - - -", file);
	if (cmd == NULL)
		errx(EXIT_FAILURE, "asprintf() failed");
	pp = popen(cmd, "r");
	if (pp == NULL)
		errx(EXIT_FAILURE, "popen() failed");
	free(cmd);
	(void)fread(o, osz, 1, pp);
	pclose(pp);
	r = sscanf(o, "%zu%zu%zu%zu%zu", &off, &size, &t1, &t2, &align);
	free(o);
	if (size > SIZE_MAX - off || off + size > (size_t)st.st_size)
		errx(EXIT_FAILURE, "%s: incoherent ELF headers", file);
	if (r != 5)
		errx(EXIT_FAILURE, "File %s doesn't contain configuration "
		    "file. Either unsupported, or not compiled with "
		    "INCLUDE_CONFIG_FILE", file);
	r = fseek(fp, off, SEEK_CUR);
	if (r != 0)
		err(EXIT_FAILURE, "fseek() failed");
	for (i = 0; i < size; i++) {
		r = fgetc(fp);
		if (r == EOF)
			break;
		if (r == '\0') {
			assert(i == size - 1 &&
			    ("\\0 found in the middle of a file"));
			break;
		}
		fputc(r, stdout);
	}
	fclose(fp);
}

static void
badversion(void)
{
	fprintf(stderr, "ERROR: version of config(8) does not match kernel!\n");
	fprintf(stderr, "config version = %d, ", CONFIGVERS);
	fprintf(stderr, "version required = %d\n\n", versreq);
	fprintf(stderr, "Make sure that /usr/src/usr.sbin/config is in sync\n");
	fprintf(stderr, "with your /usr/src/sys and install a new config binary\n");
	fprintf(stderr, "before trying this again.\n\n");
	fprintf(stderr, "If running the new config fails check your config\n");
	fprintf(stderr, "file against the GENERIC or LINT config files for\n");
	fprintf(stderr, "changes in config syntax, or option/device naming\n");
	fprintf(stderr, "conventions\n\n");
	exit(1);
}

static void
checkversion(void)
{
	FILE *ifp;
	char line[BUFSIZ];

	ifp = open_makefile_template();
	while (fgets(line, BUFSIZ, ifp) != 0) {
		if (*line != '%')
			continue;
		if (strncmp(line, "%VERSREQ=", 9) != 0)
			continue;
		versreq = atoi(line + 9);
		if (MAJOR_VERS(versreq) == MAJOR_VERS(CONFIGVERS) &&
		    versreq <= CONFIGVERS)
			continue;
		badversion();
	}
	fclose(ifp);
}
