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
static char sccsid[] = "@(#)mkmakefile.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

/*
 * Build the makefile for the system, from
 * the information in the files files and the
 * additional files for the machine being compiled to.
 */

#include <stdio.h>
#include <ctype.h>
#include "y.tab.h"
#include "config.h"

#define next_word(fp, wd) \
	{ register char *word = get_word(fp); \
	  if (word == (char *)EOF) \
		return; \
	  else \
		wd = word; \
	}
#define next_quoted_word(fp, wd) \
	{ register char *word = get_quoted_word(fp); \
	  if (word == (char *)EOF) \
		return; \
	  else \
		wd = word; \
	}

static	struct file_list *fcur;
char *tail();
extern int old_config_present;

/*
 * Lookup a file, by name.
 */
struct file_list *
fl_lookup(file)
	register char *file;
{
	register struct file_list *fp;

	for (fp = ftab ; fp != 0; fp = fp->f_next) {
		if (eq(fp->f_fn, file))
			return (fp);
	}
	return (0);
}

/*
 * Lookup a file, by final component name.
 */
struct file_list *
fltail_lookup(file)
	register char *file;
{
	register struct file_list *fp;

	for (fp = ftab ; fp != 0; fp = fp->f_next) {
		if (eq(tail(fp->f_fn), tail(file)))
			return (fp);
	}
	return (0);
}

/*
 * Make a new file list entry
 */
struct file_list *
new_fent()
{
	register struct file_list *fp;

	fp = (struct file_list *) malloc(sizeof *fp);
	bzero(fp, sizeof *fp);
	if (fcur == 0)
		fcur = ftab = fp;
	else
		fcur->f_next = fp;
	fcur = fp;
	return (fp);
}

static	struct users {
	int	u_default;
	int	u_min;
	int	u_max;
} users[] = {
	{ 8, 2, 512 },			/* MACHINE_VAX */
	{ 8, 2, 512 },			/* MACHINE_TAHOE */
	{ 8, 2, 512 },			/* MACHINE_HP300 */
	{ 8, 2, 512 },			/* MACHINE_I386 */
	{ 8, 2, 512 },			/* MACHINE_MIPS */
	{ 8, 2, 512 },			/* MACHINE_PMAX */
	{ 8, 2, 512 },			/* MACHINE_LUNA68K */
	{ 8, 2, 512 },			/* MACHINE_NEWS3400 */
};
#define	NUSERS	(sizeof (users) / sizeof (users[0]))

/*
 * Build the makefile from the skeleton
 */
makefile()
{
	FILE *ifp, *ofp;
	char line[BUFSIZ];
	struct opt *op;
	struct users *up;
	int warn_make_clean = 0;

	read_files();
	strcpy(line, "Makefile.");
	(void) strcat(line, machinename);
	ifp = fopen(line, "r");
	if (ifp == 0) {
		perror(line);
		exit(1);
	}
	ofp = fopen(path("Makefile.new"), "w");
	if (ofp == 0) {
		perror(path("Makefile.new"));
		exit(1);
	}
	fprintf(ofp, "KERN_IDENT=%s\n", raise(ident));
	fprintf(ofp, "IDENT=");
	if (profiling)
		fprintf(ofp, " -DGPROF");

	if (cputype == 0) {
		printf("cpu type must be specified\n");
		exit(1);
	}
#if 0
	/* XXX: moved to cputype.h */
	{ struct cputype *cp;
	  for (cp = cputype; cp; cp = cp->cpu_next)
		fprintf(ofp, " -D%s", cp->cpu_name);
	}
#endif
	for (op = opt; op; op = op->op_next) {
		if (!op->op_ownfile) {
			warn_make_clean++;
			if (op->op_value)
				fprintf(ofp, " -D%s=%s", op->op_name, op->op_value);
			else
				fprintf(ofp, " -D%s", op->op_name);
		}
	}
	fprintf(ofp, "\n");
	if ((unsigned)machine > NUSERS) {
		printf("maxusers config info isn't present, using vax\n");
		up = &users[MACHINE_VAX-1];
	} else
		up = &users[machine-1];
	if (maxusers == 0) {
		printf("maxusers not specified; %d assumed\n", up->u_default);
		maxusers = up->u_default;
	} else if (maxusers < up->u_min) {
		printf("minimum of %d maxusers assumed\n", up->u_min);
		maxusers = up->u_min;
	} else if (maxusers > up->u_max)
		printf("warning: maxusers > %d (%d)\n", up->u_max, maxusers);
	fprintf(ofp, "PARAM=-DMAXUSERS=%d\n", maxusers);
	if (loadaddress != -1) {
		fprintf(ofp, "LOAD_ADDRESS=%X\n", loadaddress);
	}
	for (op = mkopt; op; op = op->op_next)
		fprintf(ofp, "%s=%s\n", op->op_name, op->op_value);
	if (debugging)
		fprintf(ofp, "DEBUG=-g\n");
	if (profiling) {
		fprintf(ofp, "PROF=-pg\n");
		fprintf(ofp, "PROFLEVEL=%d\n", profiling);
	}
	while (fgets(line, BUFSIZ, ifp) != 0) {
		if (*line != '%') {
			fprintf(ofp, "%s", line);
			continue;
		}
		if (eq(line, "%BEFORE_DEPEND\n"))
			do_before_depend(ofp);
		else if (eq(line, "%OBJS\n"))
			do_objs(ofp);
		else if (eq(line, "%CFILES\n"))
			do_cfiles(ofp);
		else if (eq(line, "%SFILES\n"))
			do_sfiles(ofp);
		else if (eq(line, "%RULES\n"))
			do_rules(ofp);
		else if (eq(line, "%LOAD\n"))
			do_load(ofp);
		else if (eq(line, "%CLEAN\n"))
			do_clean(ofp);
		else
			fprintf(stderr,
			    "Unknown %% construct in generic makefile: %s",
			    line);
	}
	(void) fclose(ifp);
	(void) fclose(ofp);
	moveifchanged(path("Makefile.new"), path("Makefile"));
#ifdef notyet
	if (warn_make_clean) {
		printf("WARNING: Unknown options used (not in ../../conf/options or ./options.%s).\n", machinename);
	 	if (old_config_present) {
			printf("It is VERY important that you do a ``make clean'' before recompiling!\n");
		}
	}
	printf("Don't forget to do a ``make depend''\n");
#endif
}

/*
 * Read in the information about files used in making the system.
 * Store it in the ftab linked list.
 */
read_files()
{
	FILE *fp;
	register struct file_list *tp, *pf;
	register struct device *dp;
	struct device *save_dp;
	register struct opt *op;
	char *wd, *this, *needs, *special, *depends, *clean;
	char fname[32];
	int nreqs, first = 1, configdep, isdup, std, filetype,
	    imp_rule, no_obj, before_depend;

	ftab = 0;
	(void) strcpy(fname, "../../conf/files");
openit:
	fp = fopen(fname, "r");
	if (fp == 0) {
		perror(fname);
		exit(1);
	}
	if(ident == NULL) {
		printf("no ident line specified\n");
		exit(1);
	}
next:
	/*
	 * filename	[ standard | optional ] [ config-dependent ]
	 *	[ dev* | profiling-routine ] [ device-driver] [ no-obj ]
	 *	[ compile-with "compile rule" [no-implicit-rule] ]
	 *      [ dependency "dependency-list"] [ before-depend ]
	 *	[ clean "file-list"]
	 */
	wd = get_word(fp);
	if (wd == (char *)EOF) {
		(void) fclose(fp);
		if (first == 1) {
			(void) sprintf(fname, "files.%s", machinename);
			first++;
			goto openit;
		}
		if (first == 2) {
			(void) sprintf(fname, "files.%s", raise(ident));
			first++;
			fp = fopen(fname, "r");
			if (fp != 0)
				goto next;
		}
		return;
	}
	if (wd == 0)
		goto next;
	/*************************************************\
	* If it's a comment ignore to the end of the line *
	\*************************************************/
	if(wd[0] == '#')
	{
		while( ((wd = get_word(fp)) != (char *)EOF) && wd)
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
	if ((pf = fl_lookup(this)) && (pf->f_type != INVISIBLE || pf->f_flags))
		isdup = 1;
	else
		isdup = 0;
	tp = 0;
	if (first == 3 && (tp = fltail_lookup(this)) != 0)
		printf("%s: Local file %s overrides %s.\n",
		    fname, this, tp->f_fn);
	nreqs = 0;
	special = 0;
	depends = 0;
	clean = 0;
	configdep = 0;
	needs = 0;
	std = 0;
	imp_rule = 0;
	no_obj = 0;
	before_depend = 0;
	filetype = NORMAL;
	if (eq(wd, "standard"))
		std = 1;
	else if (!eq(wd, "optional")) {
		printf("%s: %s must be optional or standard\n", fname, this);
		exit(1);
	}
nextparam:
	next_word(fp, wd);
	if (wd == 0)
		goto doneparam;
	if (eq(wd, "config-dependent")) {
		configdep++;
		goto nextparam;
	}
	if (eq(wd, "no-obj")) {
		no_obj++;
		goto nextparam;
	}
	if (eq(wd, "no-implicit-rule")) {
		if (special == 0) {
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
		special = ns(wd);
		goto nextparam;
	}
	nreqs++;
	if (eq(wd, "device-driver")) {
		filetype = DRIVER;
		goto nextparam;
	}
	if (eq(wd, "profiling-routine")) {
		filetype = PROFILING;
		goto nextparam;
	}
	if (needs == 0 && nreqs == 1)
		needs = ns(wd);
	if (isdup)
		goto invis;
	for (dp = dtab; dp != 0; save_dp = dp, dp = dp->d_next)
		if (eq(dp->d_name, wd)) {
			if (std && dp->d_type == PSEUDO_DEVICE &&
			    dp->d_slave <= 0)
				dp->d_slave = 1;
			goto nextparam;
		}
	if (std) {
		dp = (struct device *) malloc(sizeof *dp);
		bzero(dp, sizeof *dp);
		init_dev(dp);
		dp->d_name = ns(wd);
		dp->d_type = PSEUDO_DEVICE;
		dp->d_slave = 1;
		save_dp->d_next = dp;
		goto nextparam;
	}
	for (op = opt; op != 0; op = op->op_next)
		if (op->op_value == 0 && opteq(op->op_name, wd)) {
			if (nreqs == 1) {
				free(needs);
				needs = 0;
			}
			goto nextparam;
		}
invis:
	while ((wd = get_word(fp)) != 0)
		;
	if (tp == 0)
		tp = new_fent();
	tp->f_fn = this;
	tp->f_type = INVISIBLE;
	tp->f_needs = needs;
	tp->f_flags = isdup;
	tp->f_special = special;
	tp->f_depends = depends;
	tp->f_clean = clean;
	goto next;

doneparam:
	if (std == 0 && nreqs == 0) {
		printf("%s: what is %s optional on?\n",
		    fname, this);
		exit(1);
	}

save:
	if (wd) {
		printf("%s: syntax error describing %s\n",
		    fname, this);
		exit(1);
	}
	if (filetype == PROFILING && profiling == 0)
		goto next;
	if (tp == 0)
		tp = new_fent();
	tp->f_fn = this;
	tp->f_type = filetype;
	tp->f_flags = 0;
	if (configdep)
		tp->f_flags |= CONFIGDEP;
	if (imp_rule)
		tp->f_flags |= NO_IMPLCT_RULE;
	if (no_obj)
		tp->f_flags |= NO_OBJ;
	if (before_depend)
		tp->f_flags |= BEFORE_DEPEND;
	if (imp_rule)
		tp->f_flags |= NO_IMPLCT_RULE;
	if (no_obj)
		tp->f_flags |= NO_OBJ;
	tp->f_needs = needs;
	tp->f_special = special;
	tp->f_depends = depends;
	tp->f_clean = clean;
	if (pf && pf->f_type == INVISIBLE)
		pf->f_flags = 1;		/* mark as duplicate */
	goto next;
}

opteq(cp, dp)
	char *cp, *dp;
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

do_before_depend(fp)
	FILE *fp;
{
	register struct file_list *tp, *fl;
	register int lpos, len;
	char swapname[32];

	fputs("BEFORE_DEPEND=", fp);
	lpos = 15;
	for (tp = ftab; tp; tp = tp->f_next)
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

do_objs(fp)
	FILE *fp;
{
	register struct file_list *tp, *fl;
	register int lpos, len;
	register char *cp, och, *sp;
	char swapname[32];

	fprintf(fp, "OBJS=");
	lpos = 6;
	for (tp = ftab; tp != 0; tp = tp->f_next) {
		if (tp->f_type == INVISIBLE || tp->f_flags & NO_OBJ)
			continue;
		sp = tail(tp->f_fn);
		for (fl = conf_list; fl; fl = fl->f_next) {
			if (fl->f_type != SWAPSPEC)
				continue;
			(void) sprintf(swapname, "swap%s.c", fl->f_fn);
			if (eq(sp, swapname))
				goto cont;
		}
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
cont:
		;
	}
	if (lpos != 8)
		putc('\n', fp);
}

do_cfiles(fp)
	FILE *fp;
{
	register struct file_list *tp, *fl;
	register int lpos, len;
	char swapname[32];

	fputs("CFILES=", fp);
	lpos = 8;
	for (tp = ftab; tp; tp = tp->f_next)
		if (tp->f_type != INVISIBLE) {
			len = strlen(tp->f_fn);
			if (tp->f_fn[len - 1] != 'c')
				continue;
			if ((len = 3 + len) + lpos > 72) {
				lpos = 8;
				fputs("\\\n\t", fp);
			}
			fprintf(fp, "$S/%s ", tp->f_fn);
			lpos += len + 1;
		}
	for (fl = conf_list; fl; fl = fl->f_next)
		if (fl->f_type == SYSTEMSPEC) {
			(void) sprintf(swapname, "swap%s.c", fl->f_fn);
			if ((len = 3 + strlen(swapname)) + lpos > 72) {
				lpos = 8;
				fputs("\\\n\t", fp);
			}
			if (eq(fl->f_fn, "generic"))
				fprintf(fp, "$S/%s/%s/%s ",
				    machinename, machinename, swapname);
			else
				fprintf(fp, "%s ", swapname);
			lpos += len + 1;
		}
	if (lpos != 8)
		putc('\n', fp);
}

do_sfiles(fp)
	FILE *fp;
{
	register struct file_list *tp;
	register int lpos, len;

	fputs("SFILES=", fp);
	lpos = 8;
	for (tp = ftab; tp; tp = tp->f_next)
		if (tp->f_type != INVISIBLE) {
			len = strlen(tp->f_fn);
			if (tp->f_fn[len - 1] != 'S' && tp->f_fn[len - 1] != 's')
				continue;
			if ((len = 3 + len) + lpos > 72) {
				lpos = 8;
				fputs("\\\n\t", fp);
			}
			fprintf(fp, "$S/%s ", tp->f_fn);
			lpos += len + 1;
		}
	if (lpos != 8)
		putc('\n', fp);
}


char *
tail(fn)
	char *fn;
{
	register char *cp;

	cp = rindex(fn, '/');
	if (cp == 0)
		return (fn);
	return (cp+1);
}

/*
 * Create the makerules for each file
 * which is part of the system.
 * Devices are processed with the special c2 option -i
 * which avoids any problem areas with i/o addressing
 * (e.g. for the VAX); assembler files are processed by as.
 */
do_rules(f)
	FILE *f;
{
	register char *cp, *np, och, *tp;
	register struct file_list *ftp;
	char *special;

	for (ftp = ftab; ftp != 0; ftp = ftp->f_next) {
		if (ftp->f_type == INVISIBLE)
			continue;
		cp = (np = ftp->f_fn) + strlen(ftp->f_fn) - 1;
		och = *cp;
		if (ftp->f_flags & NO_IMPLCT_RULE) {
			if (ftp->f_depends)
				fprintf(f, "%s: %s\n", np, ftp->f_depends );
			else
				fprintf(f, "%s: \n", np );
		}
		else {
			*cp = '\0';
			if (och == 'o') {
				fprintf(f, "%so:\n\t-cp $S/%so .\n\n",
					tail(np), np);
				continue;
			}
			if (ftp->f_depends)
				fprintf(f, "%so: $S/%s%c %s\n", tail(np),
					np, och, ftp->f_depends);
			else
				fprintf(f, "%so: $S/%s%c\n", tail(np),
					np, och);
		}
		tp = tail(np);
		special = ftp->f_special;
		if (special == 0) {
			char *ftype;
			static char cmd[128];

			switch (ftp->f_type) {

			case NORMAL:
				ftype = "NORMAL";
				break;

			case DRIVER:
				ftype = "DRIVER";
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
			(void)sprintf(cmd, "${%s_%c%s}", ftype, toupper(och),
				      ftp->f_flags & CONFIGDEP? "_C" : "");
			special = cmd;
		}
		*cp = och;
		fprintf(f, "\t%s\n\n", special);
	}
}

/*
 * Create the load strings
 */
do_load(f)
	register FILE *f;
{
	register struct file_list *fl;
	register int first;
	struct file_list *do_systemspec();

	for (first = 1, fl = conf_list; fl; first = 0)
		fl = fl->f_type == SYSTEMSPEC ?
			do_systemspec(f, fl, first) : fl->f_next;
	fputs("all:", f);
	for (fl = conf_list; fl; fl = fl->f_next)
		if (fl->f_type == SYSTEMSPEC)
			fprintf(f, " %s", fl->f_needs);
	putc('\n', f);
}

do_clean(fp)
	FILE *fp;
{
	register struct file_list *tp, *fl;
	register int lpos, len;
	char swapname[32];

	fputs("CLEAN=", fp);
	lpos = 7;
	for (tp = ftab; tp; tp = tp->f_next)
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

struct file_list *
do_systemspec(f, fl, first)
	FILE *f;
	register struct file_list *fl;
	int first;
{

	fprintf(f, "%s: ${SYSTEM_DEP} swap%s.o", fl->f_needs, fl->f_fn);
	if (first)
		fprintf(f, " vers.o");
	fprintf(f, "\n\t${SYSTEM_LD_HEAD}\n");
	fprintf(f, "\t${SYSTEM_LD} swap%s.o\n", fl->f_fn);
	fprintf(f, "\t${SYSTEM_LD_TAIL}\n\n");
	do_swapspec(f, fl->f_fn);
	for (fl = fl->f_next; fl; fl = fl->f_next)
		if (fl->f_type != SWAPSPEC)
			break;
	return (fl);
}

do_swapspec(f, name)
	FILE *f;
	register char *name;
{

	if (!eq(name, "generic"))
		fprintf(f, "swap%s.o: swap%s.c\n", name, name);
	else
		fprintf(f, "swapgeneric.o: $S/%s/%s/swapgeneric.c\n",
			machinename, machinename);
	fprintf(f, "\t${NORMAL_C}\n\n");
}

char *
raise(str)
	register char *str;
{
	register char *cp = str;

	while (*str) {
		if (islower(*str))
			*str = toupper(*str);
		str++;
	}
	return (cp);
}

