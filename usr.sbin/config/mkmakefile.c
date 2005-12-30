/*
 * Copyright (c) 1993, 19801990
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
#if 0
static char sccsid[] = "@(#)mkmakefile.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

/*
 * Build the makefile for the system, from
 * the information in the files files and the
 * additional files for the machine being compiled to.
 */

#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include "y.tab.h"
#include "config.h"
#include "configvers.h"

#define next_word(fp, wd) \
	{ char *word = get_word(fp); \
	  if (word == (char *)EOF) \
		return; \
	  else \
		wd = word; \
	}
#define next_quoted_word(fp, wd) \
	{ char *word = get_quoted_word(fp); \
	  if (word == (char *)EOF) \
		return; \
	  else \
		wd = word; \
	}

static char *tail(char *);
static void do_clean(FILE *);
static void do_rules(FILE *);
static void do_xxfiles(char *, FILE *);
static void do_objs(FILE *);
static void do_before_depend(FILE *);
static int opteq(const char *, const char *);
static void read_files(void);

/*
 * Lookup a file, by name.
 */
static struct file_list *
fl_lookup(char *file)
{
	struct file_list *fp;

	STAILQ_FOREACH(fp, &ftab, f_next) {
		if (eq(fp->f_fn, file))
			return (fp);
	}
	return (0);
}

/*
 * Make a new file list entry
 */
static struct file_list *
new_fent(void)
{
	struct file_list *fp;

	fp = (struct file_list *) malloc(sizeof *fp);
	bzero(fp, sizeof *fp);
	STAILQ_INSERT_TAIL(&ftab, fp, f_next);
	return (fp);
}

/*
 * Build the makefile from the skeleton
 */
void
makefile(void)
{
	FILE *ifp, *ofp;
	char line[BUFSIZ];
	struct opt *op;
	int versreq;

	read_files();
	snprintf(line, sizeof(line), "../../conf/Makefile.%s", machinename);
	ifp = fopen(line, "r");
	if (ifp == 0) {
		snprintf(line, sizeof(line), "Makefile.%s", machinename);
		ifp = fopen(line, "r");
	}
	if (ifp == 0)
		err(1, "%s", line);

	ofp = fopen(path("Makefile.new"), "w");
	if (ofp == 0)
		err(1, "%s", path("Makefile.new"));
	fprintf(ofp, "KERN_IDENT=%s\n", ident);
	SLIST_FOREACH(op, &mkopt, op_next)
		fprintf(ofp, "%s=%s\n", op->op_name, op->op_value);
	if (debugging)
		fprintf(ofp, "DEBUG=-g\n");
	if (profiling)
		fprintf(ofp, "PROFLEVEL=%d\n", profiling);
	if (*srcdir != '\0')
		fprintf(ofp,"S=%s\n", srcdir);
	while (fgets(line, BUFSIZ, ifp) != 0) {
		if (*line != '%') {
			fprintf(ofp, "%s", line);
			continue;
		}
		if (eq(line, "%BEFORE_DEPEND\n"))
			do_before_depend(ofp);
		else if (eq(line, "%OBJS\n"))
			do_objs(ofp);
		else if (strncmp(line, "%FILES.", 7) == 0)
			do_xxfiles(line, ofp);
		else if (eq(line, "%RULES\n"))
			do_rules(ofp);
		else if (eq(line, "%CLEAN\n"))
			do_clean(ofp);
		else if (strncmp(line, "%VERSREQ=", sizeof("%VERSREQ=") - 1) == 0) {
			versreq = atoi(line + sizeof("%VERSREQ=") - 1);
			if (versreq != CONFIGVERS) {
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
		} else
			fprintf(stderr,
			    "Unknown %% construct in generic makefile: %s",
			    line);
	}
	(void) fclose(ifp);
	(void) fclose(ofp);
	moveifchanged(path("Makefile.new"), path("Makefile"));
}

/*
 * Build hints.c from the skeleton
 */
void
makehints(void)
{
	FILE *ifp, *ofp;
	char line[BUFSIZ];
	char *s;

	if (hints) {
		ifp = fopen(hints, "r");
		if (ifp == NULL)
			err(1, "%s", hints);
	} else {
		ifp = NULL;
	}
	ofp = fopen(path("hints.c.new"), "w");
	if (ofp == NULL)
		err(1, "%s", path("hints.c.new"));
	fprintf(ofp, "#include <sys/types.h>\n");
	fprintf(ofp, "#include <sys/systm.h>\n");
	fprintf(ofp, "\n");
	fprintf(ofp, "int hintmode = %d;\n", hintmode);
	fprintf(ofp, "char static_hints[] = {\n");
	if (ifp) {
		while (fgets(line, BUFSIZ, ifp) != 0) {
			/* zap trailing CR and/or LF */
			while ((s = rindex(line, '\n')) != NULL)
				*s = '\0';
			while ((s = rindex(line, '\r')) != NULL)
				*s = '\0';
			/* remove # comments */
			s = index(line, '#');
			if (s)
				*s = '\0';
			/* remove any whitespace and " characters */
			s = line;
			while (*s) {
				if (*s == ' ' || *s == '\t' || *s == '"') {
					while (*s) {
						s[0] = s[1];
						s++;
					}
					/* start over */
					s = line;
					continue;
				}
				s++;
			}
			/* anything left? */
			if (*line == '\0')
				continue;
			fprintf(ofp, "\"%s\\0\"\n", line);
		}
	}
	fprintf(ofp, "\"\\0\"\n};\n");
	if (ifp)
		fclose(ifp);
	fclose(ofp);
	moveifchanged(path("hints.c.new"), path("hints.c"));
}

/*
 * Build env.c from the skeleton
 */
void
makeenv(void)
{
	FILE *ifp, *ofp;
	char line[BUFSIZ];
	char *s;

	if (env) {
		ifp = fopen(env, "r");
		if (ifp == NULL)
			err(1, "%s", env);
	} else {
		ifp = NULL;
	}
	ofp = fopen(path("env.c.new"), "w");
	if (ofp == NULL)
		err(1, "%s", path("env.c.new"));
	fprintf(ofp, "#include <sys/types.h>\n");
	fprintf(ofp, "#include <sys/systm.h>\n");
	fprintf(ofp, "\n");
	fprintf(ofp, "int envmode = %d;\n", envmode);
	fprintf(ofp, "char static_env[] = {\n");
	if (ifp) {
		while (fgets(line, BUFSIZ, ifp) != 0) {
			/* zap trailing CR and/or LF */
			while ((s = rindex(line, '\n')) != NULL)
				*s = '\0';
			while ((s = rindex(line, '\r')) != NULL)
				*s = '\0';
			/* remove # comments */
			s = index(line, '#');
			if (s)
				*s = '\0';
			/* remove any whitespace and " characters */
			s = line;
			while (*s) {
				if (*s == ' ' || *s == '\t' || *s == '"') {
					while (*s) {
						s[0] = s[1];
						s++;
					}
					/* start over */
					s = line;
					continue;
				}
				s++;
			}
			/* anything left? */
			if (*line == '\0')
				continue;
			fprintf(ofp, "\"%s\\0\"\n", line);
		}
	}
	fprintf(ofp, "\"\\0\"\n};\n");
	if (ifp)
		fclose(ifp);
	fclose(ofp);
	moveifchanged(path("env.c.new"), path("env.c"));
}

static void
read_file(char *fname)
{
	FILE *fp;
	struct file_list *tp;
	struct device *dp;
	struct opt *op;
	char *wd, *this, *compilewith, *depends, *clean, *warning;
	int compile, match, nreqs, devfound, std, filetype,
	    imp_rule, no_obj, before_depend, mandatory, nowerror;

	fp = fopen(fname, "r");
	if (fp == 0)
		err(1, "%s", fname);
next:
	/*
	 * filename    [ standard | mandatory | optional ]
	 *	[ dev* [ | dev* ... ] | profiling-routine ] [ no-obj ]
	 *	[ compile-with "compile rule" [no-implicit-rule] ]
	 *      [ dependency "dependency-list"] [ before-depend ]
	 *	[ clean "file-list"] [ warning "text warning" ]
	 */
	wd = get_word(fp);
	if (wd == (char *)EOF) {
		(void) fclose(fp);
		return;
	} 
	if (wd == 0)
		goto next;
	if (wd[0] == '#')
	{
		while (((wd = get_word(fp)) != (char *)EOF) && wd)
			;
		goto next;
	}
	this = ns(wd);
	next_word(fp, wd);
	if (wd == 0) {
		printf("%s: No type for %s.\n",
		    fname, this);
		exit(1);
	}
	tp = fl_lookup(this);
	compile = 0;
	match = 1;
	nreqs = 0;
	compilewith = 0;
	depends = 0;
	clean = 0;
	warning = 0;
	std = mandatory = 0;
	imp_rule = 0;
	no_obj = 0;
	before_depend = 0;
	nowerror = 0;
	filetype = NORMAL;
	if (eq(wd, "standard")) {
		std = 1;
	/*
	 * If an entry is marked "mandatory", config will abort if it's
	 * not called by a configuration line in the config file.  Apart
	 * from this, the device is handled like one marked "optional".
	 */
	} else if (eq(wd, "mandatory")) {
		mandatory = 1;
	} else if (!eq(wd, "optional")) {
		printf("%s: %s must be optional, mandatory or standard\n",
		       fname, this);
		exit(1);
	}
nextparam:
	next_word(fp, wd);
	if (wd == 0) {
		compile += match;
		if (compile && tp == NULL)
			goto doneparam;
		goto next;
	}
	if (eq(wd, "|")) {
		if (nreqs == 0) {
			printf("%s: syntax error describing %s\n",
			    fname, this);
			exit(1);
		}
		compile += match;
		match = 1;
		nreqs = 0;
		goto nextparam;
	}
	if (eq(wd, "no-obj")) {
		no_obj++;
		goto nextparam;
	}
	if (eq(wd, "no-implicit-rule")) {
		if (compilewith == 0) {
			printf("%s: alternate rule required when "
			       "\"no-implicit-rule\" is specified.\n",
			       fname);
		}
		imp_rule++;
		goto nextparam;
	}
	if (eq(wd, "before-depend")) {
		before_depend++;
		goto nextparam;
	}
	if (eq(wd, "dependency")) {
		next_quoted_word(fp, wd);
		if (wd == 0) {
			printf("%s: %s missing compile command string.\n",
			       fname, this);
			exit(1);
		}
		depends = ns(wd);
		goto nextparam;
	}
	if (eq(wd, "clean")) {
		next_quoted_word(fp, wd);
		if (wd == 0) {
			printf("%s: %s missing clean file list.\n",
			       fname, this);
			exit(1);
		}
		clean = ns(wd);
		goto nextparam;
	}
	if (eq(wd, "compile-with")) {
		next_quoted_word(fp, wd);
		if (wd == 0) {
			printf("%s: %s missing compile command string.\n",
			       fname, this);
			exit(1);
		}
		compilewith = ns(wd);
		goto nextparam;
	}
	if (eq(wd, "warning")) {
		next_quoted_word(fp, wd);
		if (wd == 0) {
			printf("%s: %s missing warning text string.\n",
				fname, this);
			exit(1);
		}
		warning = ns(wd);
		goto nextparam;
	}
	nreqs++;
	if (eq(wd, "local")) {
		filetype = LOCAL;
		goto nextparam;
	}
	if (eq(wd, "no-depend")) {
		filetype = NODEPEND;
		goto nextparam;
	}
	if (eq(wd, "profiling-routine")) {
		filetype = PROFILING;
		goto nextparam;
	}
	if (eq(wd, "nowerror")) {
		nowerror = 1;
		goto nextparam;
	}
	devfound = 0;		/* XXX duplicate device entries */
	STAILQ_FOREACH(dp, &dtab, d_next)
		if (eq(dp->d_name, wd)) {
			dp->d_done |= DEVDONE;
			devfound = 1;
		}
	if (devfound)
		goto nextparam;
	if (mandatory) {
		printf("%s: mandatory device \"%s\" not found\n",
		       fname, wd);
		exit(1);
	}
	if (std) {
		printf("standard entry %s has a device keyword - %s!\n",
		       this, wd);
		exit(1);
	}
	SLIST_FOREACH(op, &opt, op_next)
		if (op->op_value == 0 && opteq(op->op_name, wd))
			goto nextparam;
	match = 0;
	goto nextparam;

doneparam:
	if (std == 0 && nreqs == 0) {
		printf("%s: what is %s optional on?\n",
		    fname, this);
		exit(1);
	}

	if (wd) {
		printf("%s: syntax error describing %s\n",
		    fname, this);
		exit(1);
	}
	if (filetype == PROFILING && profiling == 0)
		goto next;
	tp = new_fent();
	tp->f_fn = this;
	tp->f_type = filetype;
	if (imp_rule)
		tp->f_flags |= NO_IMPLCT_RULE;
	if (no_obj)
		tp->f_flags |= NO_OBJ;
	if (before_depend)
		tp->f_flags |= BEFORE_DEPEND;
	if (nowerror)
		tp->f_flags |= NOWERROR;
	tp->f_compilewith = compilewith;
	tp->f_depends = depends;
	tp->f_clean = clean;
	tp->f_warn = warning;
	goto next;
}

/*
 * Read in the information about files used in making the system.
 * Store it in the ftab linked list.
 */
static void
read_files(void)
{
	char fname[MAXPATHLEN];
	struct files_name *nl, *tnl;
	
	(void) snprintf(fname, sizeof(fname), "../../conf/files");
	read_file(fname);
	(void) snprintf(fname, sizeof(fname),
		       	"../../conf/files.%s", machinename);
	read_file(fname);
	for (nl = STAILQ_FIRST(&fntab); nl != NULL; nl = tnl) {
		read_file(nl->f_name);
		tnl = STAILQ_NEXT(nl, f_next);
		free(nl->f_name);
		free(nl);
	}
}

static int
opteq(const char *cp, const char *dp)
{
	char c, d;

	for (; ; cp++, dp++) {
		if (*cp != *dp) {
			c = isupper(*cp) ? tolower(*cp) : *cp;
			d = isupper(*dp) ? tolower(*dp) : *dp;
			if (c != d)
				return (0);
		}
		if (*cp == 0)
			return (1);
	}
}

static void
do_before_depend(FILE *fp)
{
	struct file_list *tp;
	int lpos, len;

	fputs("BEFORE_DEPEND=", fp);
	lpos = 15;
	STAILQ_FOREACH(tp, &ftab, f_next)
		if (tp->f_flags & BEFORE_DEPEND) {
			len = strlen(tp->f_fn);
			if ((len = 3 + len) + lpos > 72) {
				lpos = 8;
				fputs("\\\n\t", fp);
			}
			if (tp->f_flags & NO_IMPLCT_RULE)
				fprintf(fp, "%s ", tp->f_fn);
			else
				fprintf(fp, "$S/%s ", tp->f_fn);
			lpos += len + 1;
		}
	if (lpos != 8)
		putc('\n', fp);
}

static void
do_objs(FILE *fp)
{
	struct file_list *tp;
	int lpos, len;
	char *cp, och, *sp;

	fprintf(fp, "OBJS=");
	lpos = 6;
	STAILQ_FOREACH(tp, &ftab, f_next) {
		if (tp->f_flags & NO_OBJ)
			continue;
		sp = tail(tp->f_fn);
		cp = sp + (len = strlen(sp)) - 1;
		och = *cp;
		*cp = 'o';
		if (len + lpos > 72) {
			lpos = 8;
			fprintf(fp, "\\\n\t");
		}
		fprintf(fp, "%s ", sp);
		lpos += len + 1;
		*cp = och;
	}
	if (lpos != 8)
		putc('\n', fp);
}

static void
do_xxfiles(char *tag, FILE *fp)
{
	struct file_list *tp;
	int lpos, len, slen;
	char *suff, *SUFF;

	if (tag[strlen(tag) - 1] == '\n')
		tag[strlen(tag) - 1] = '\0';

	suff = ns(tag + 7);
	SUFF = ns(suff);
	raisestr(SUFF);
	slen = strlen(suff);

	fprintf(fp, "%sFILES=", SUFF);
	lpos = 8;
	STAILQ_FOREACH(tp, &ftab, f_next)
		if (tp->f_type != NODEPEND) {
			len = strlen(tp->f_fn);
			if (tp->f_fn[len - slen - 1] != '.')
				continue;
			if (strcasecmp(&tp->f_fn[len - slen], suff) != 0)
				continue;
			if ((len = 3 + len) + lpos > 72) {
				lpos = 8;
				fputs("\\\n\t", fp);
			}
			if (tp->f_type != LOCAL)
				fprintf(fp, "$S/%s ", tp->f_fn);
			else
				fprintf(fp, "%s ", tp->f_fn);
			lpos += len + 1;
		}
	if (lpos != 8)
		putc('\n', fp);
}

static char *
tail(char *fn)
{
	char *cp;

	cp = rindex(fn, '/');
	if (cp == 0)
		return (fn);
	return (cp+1);
}

/*
 * Create the makerules for each file
 * which is part of the system.
 */
static void
do_rules(FILE *f)
{
	char *cp, *np, och, *tp;
	struct file_list *ftp;
	char *compilewith;

	STAILQ_FOREACH(ftp, &ftab, f_next) {
		if (ftp->f_warn)
			printf("WARNING: %s\n", ftp->f_warn);
		cp = (np = ftp->f_fn) + strlen(ftp->f_fn) - 1;
		och = *cp;
		if (ftp->f_flags & NO_IMPLCT_RULE) {
			if (ftp->f_depends)
				fprintf(f, "%s: %s\n", np, ftp->f_depends);
			else
				fprintf(f, "%s: \n", np);
		}
		else {
			*cp = '\0';
			if (och == 'o') {
				fprintf(f, "%so:\n\t-cp $S/%so .\n\n",
					tail(np), np);
				continue;
			}
			if (ftp->f_depends) {
				fprintf(f, "%sln: $S/%s%c %s\n", tail(np),
					np, och, ftp->f_depends);
				fprintf(f, "\t${NORMAL_LINT}\n\n");
				fprintf(f, "%so: $S/%s%c %s\n", tail(np),
					np, och, ftp->f_depends);
			}
			else {
				fprintf(f, "%sln: $S/%s%c\n", tail(np),
					np, och);
				fprintf(f, "\t${NORMAL_LINT}\n\n");
				fprintf(f, "%so: $S/%s%c\n", tail(np),
					np, och);
			}
		}
		tp = tail(np);
		compilewith = ftp->f_compilewith;
		if (compilewith == 0) {
			const char *ftype = NULL;
			static char cmd[128];

			switch (ftp->f_type) {

			case NORMAL:
				ftype = "NORMAL";
				break;

			case PROFILING:
				if (!profiling)
					continue;
				ftype = "PROFILE";
				break;

			default:
				printf("config: don't know rules for %s\n", np);
				break;
			}
			snprintf(cmd, sizeof(cmd), "${%s_%c%s}", ftype,
			    toupper(och),
			    ftp->f_flags & NOWERROR ? "_NOWERROR" : "");
			compilewith = cmd;
		}
		*cp = och;
		fprintf(f, "\t%s\n\n", compilewith);
	}
}

static void
do_clean(FILE *fp)
{
	struct file_list *tp;
	int lpos, len;

	fputs("CLEAN=", fp);
	lpos = 7;
	STAILQ_FOREACH(tp, &ftab, f_next)
		if (tp->f_clean) {
			len = strlen(tp->f_clean);
			if (len + lpos > 72) {
				lpos = 8;
				fputs("\\\n\t", fp);
			}
			fprintf(fp, "%s ", tp->f_clean);
			lpos += len + 1;
		}
	if (lpos != 8)
		putc('\n', fp);
}

char *
raisestr(char *str)
{
	char *cp = str;

	while (*str) {
		if (islower(*str))
			*str = toupper(*str);
		str++;
	}
	return (cp);
}
