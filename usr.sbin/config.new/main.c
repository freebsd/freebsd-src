/* 
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratories.
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
 *
 *	@(#)main.c	8.1 (Berkeley) 6/6/93
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1992, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "config.h"

int	firstfile __P((const char *));
int	yyparse __P((void));

extern char *optarg;
extern int optind;

static struct hashtab *opttab;
static struct hashtab *mkopttab;
static struct nvlist **nextopt;
static struct nvlist **nextmkopt;

static __dead void stop __P((void));
static int do_option __P((struct hashtab *, struct nvlist ***,
			const char *, const char *, const char *));
static int crosscheck __P((void));
static int badstar __P((void));
static int mksymlinks __P((void));
static int has_instances __P((struct devbase *, int));
static int hasparent __P((struct devi *));
static int cfcrosscheck __P((struct config *, const char *, struct nvlist *));

int
main(argc, argv)
	int argc;
	char **argv;
{
	register char *p;
	int pflag, ch;
	struct stat st;

	pflag = 0;
	while ((ch = getopt(argc, argv, "gp")) != EOF) {
		switch (ch) {

		case 'g':
			/*
			 * In addition to DEBUG, you probably wanted to
			 * set "options KGDB" and maybe others.  We could
			 * do that for you, but you really should just
			 * put them in the config file.
			 */
			(void)fputs(
			    "-g is obsolete (use makeoptions DEBUG=\"-g\")\n",
			    stderr);
			goto usage;

		case 'p':
			/*
			 * Essentially the same as makeoptions PROF="-pg",
			 * but also changes the path from ../../compile/FOO
			 * to ../../compile/FOO.prof; i.e., compile a
			 * profiling kernel based on a typical "regular"
			 * kernel.
			 *
			 * Note that if you always want profiling, you
			 * can (and should) use a "makeoptions" line.
			 */
			pflag = 1;
			break;

		case '?':
		default:
			goto usage;
		}
	}

	argc -= optind;
	argv += optind;
	if (argc != 1) {
usage:
		(void)fputs("usage: config [-p] sysname\n", stderr);
		exit(1);
	}
	conffile = argv[0];
	if (firstfile(conffile)) {
		(void)fprintf(stderr, "config: cannot read %s: %s\n",
		    conffile, strerror(errno));
		exit(2);
	}

	/*
	 * Init variables.
	 */
	minmaxusers = 1;
	maxmaxusers = 10000;
	initintern();
	initfiles();
	initsem();
	devbasetab = ht_new();
	selecttab = ht_new();
	needcnttab = ht_new();
	opttab = ht_new();
	mkopttab = ht_new();
	nextopt = &options;
	nextmkopt = &mkoptions;

	/*
	 * Handle profiling (must do this before we try to create any
	 * files).
	 */
	if (pflag) {
		char *s;

		s = emalloc(strlen(conffile) + sizeof(".PROF"));
		(void)sprintf(s, "%s.PROF", conffile);
		confdirbase = s;
		(void)addmkoption(intern("PROF"), "-pg");
		(void)addoption(intern("GPROF"), NULL);
	} else
		confdirbase = conffile;

	/*
	 * Verify, creating if necessary, the compilation directory.
	 */
	p = path(NULL);
	if (stat(p, &st)) {
		if (mkdir(p, 0777)) {
			(void)fprintf(stderr, "config: cannot create %s: %s\n",
			    p, strerror(errno));
			exit(2);
		}
	} else if (!S_ISDIR(st.st_mode)) {
		(void)fprintf(stderr, "config: %s is not a directory\n", p);
		exit(2);
	}

	/*
	 * Parse config file (including machine definitions).
	 */
	if (yyparse())
		stop();

	/*
	 * Fix (as in `set firmly in place') files.
	 */
	if (fixfiles())
		stop();

	/*
	 * Perform cross-checking.
	 */
	if (maxusers == 0) {
		if (defmaxusers) {
			(void)printf("maxusers not specified; %d assumed\n",
			    defmaxusers);
			maxusers = defmaxusers;
		} else {
			(void)fprintf(stderr,
			    "config: need \"maxusers\" line\n");
			errors++;
		}
	}
	if (crosscheck() || errors)
		stop();

	/*
	 * Squeeze things down and finish cross-checks (STAR checks must
	 * run after packing).
	 */
	pack();
	if (badstar())
		stop();

	/*
	 * Ready to go.  Build all the various files.
	 */
	if (mksymlinks() || mkmakefile() || mkheaders() || mkswap() ||
	    mkioconf())
		stop();
	(void)printf("Don't forget to run \"make depend\"\n");
	exit(0);
}

/*
 * Make a symlink for "machine" so that "#include <machine/foo.h>" works.
 */
static int
mksymlinks()
{
	int ret;
	char *p, buf[200];

	p = path("machine");
	(void)sprintf(buf, "../../%s/include", machine);
	(void)unlink(p);
	ret = symlink(buf, p);
	if (ret)
		(void)fprintf(stderr, "config: symlink(%s -> %s): %s\n",
		    p, buf, strerror(errno));
	free(p);
	return (ret);
}

static __dead void
stop()
{
	(void)fprintf(stderr, "*** Stop.\n");
	exit(1);
}

/*
 * Add an option from "options FOO".  Note that this selects things that
 * are "optional foo".
 */
void
addoption(name, value)
	const char *name, *value;
{
	register const char *n;
	register char *p, c;
	char low[500];

	if (do_option(opttab, &nextopt, name, value, "options"))
		return;

	/* make lowercase, then add to select table */
	for (n = name, p = low; (c = *n) != '\0'; n++)
		*p++ = isupper(c) ? tolower(c) : c;
	*p = 0;
	n = intern(low);
	(void)ht_insert(selecttab, n, (void *)n);
}

/*
 * Add a "make" option.
 */
void
addmkoption(name, value)
	const char *name, *value;
{

	(void)do_option(mkopttab, &nextmkopt, name, value, "mkoptions");
}

/*
 * Add a name=value pair to an option list.  The value may be NULL.
 */
static int
do_option(ht, nppp, name, value, type)
	struct hashtab *ht;
	struct nvlist ***nppp;
	const char *name, *value, *type;
{
	register struct nvlist *nv;

	/* assume it will work */
	nv = newnv(name, value, NULL, 0);
	if (ht_insert(ht, name, nv) == 0) {
		**nppp = nv;
		*nppp = &nv->nv_next;
		return (0);
	}

	/* oops, already got that option */
	nvfree(nv);
	if ((nv = ht_lookup(ht, name)) == NULL)
		panic("do_option");
	if (nv->nv_str != NULL)
		error("already have %s `%s=%s'", type, name, nv->nv_str);
	else
		error("already have %s `%s'", type, name);
	return (1);
}

/*
 * Return true if there is at least one instance of the given unit
 * on the given base (or any units, if unit == WILD).
 */
static int
has_instances(dev, unit)
	register struct devbase *dev;
	int unit;
{
	register struct devi *i;

	if (unit == WILD)
		return (dev->d_ihead != NULL);
	for (i = dev->d_ihead; i != NULL; i = i->i_bsame)
		if (unit == i->i_unit)
			return (1);
	return (0);
}

static int
hasparent(i)
	register struct devi *i;
{
	register struct nvlist *nv;
	int atunit = i->i_atunit;

	if (i->i_atdev != NULL && has_instances(i->i_atdev, atunit))
		return (1);
	if (i->i_atattr != NULL)
		for (nv = i->i_atattr->a_refs; nv != NULL; nv = nv->nv_next)
			if (has_instances(nv->nv_ptr, atunit))
				return (1);
	return (0);
}

static int
cfcrosscheck(cf, what, nv)
	register struct config *cf;
	const char *what;
	register struct nvlist *nv;
{
	register struct devbase *dev;
	int errs;

	for (errs = 0; nv != NULL; nv = nv->nv_next) {
		if (nv->nv_name == NULL)
			continue;
		dev = ht_lookup(devbasetab, nv->nv_name);
		if (dev == NULL)
			panic("cfcrosscheck(%s)", nv->nv_name);
		if (has_instances(dev, STAR) ||
		    has_instances(dev, minor(nv->nv_int) >> 3))
			continue;
		(void)fprintf(stderr,
		    "%s%d: %s says %s on %s, but there's no %s\n",
		    conffile, cf->cf_lineno,
		    cf->cf_name, what, nv->nv_str, nv->nv_str);
		errs++;
	}
	return (errs);
}

/*
 * Cross-check the configuration: make sure that each target device
 * or attribute (`at foo[0*?]') names at least one real device.  Also
 * see that the root, swap, and dump devices for all configurations
 * are there.
 */
int
crosscheck()
{
	register struct devi *i;
	register struct config *cf;
	int errs;

	errs = 0;
	for (i = alldevi; i != NULL; i = i->i_next) {
		if (i->i_at == NULL || hasparent(i))
			continue;
		xerror(conffile, i->i_lineno,
		    "%s at %s is orphaned", i->i_name, i->i_at);
		if (i->i_atunit == WILD)
			(void)fprintf(stderr, " (no %s's declared)\n",
			    i->i_base->d_name);
		else
			(void)fprintf(stderr, " (no %s declared)\n", i->i_at);
		errs++;
	}
	if (allcf == NULL) {
		(void)fprintf(stderr, "%s has no configurations!\n",
		    conffile);
		errs++;
	}
	for (cf = allcf; cf != NULL; cf = cf->cf_next) {
		if (cf->cf_root != NULL) {	/* i.e., not swap generic */
			errs += cfcrosscheck(cf, "root", cf->cf_root);
			errs += cfcrosscheck(cf, "swap", cf->cf_swap);
			errs += cfcrosscheck(cf, "dumps", cf->cf_dump);
		}
	}
	return (errs);
}

/*
 * Check to see if there is more than one *'d unit for any device,
 * or a *'d unit with a needs-count file.
 */
int
badstar()
{
	register struct devbase *d;
	register struct devi *i;
	register int errs, n;

	errs = 0;
	for (d = allbases; d != NULL; d = d->d_next) {
		for (i = d->d_ihead; i != NULL; i = i->i_bsame)
			if (i->i_unit == STAR)
				goto foundstar;
		continue;
	foundstar:
		if (ht_lookup(needcnttab, d->d_name)) {
			(void)fprintf(stderr,
		    "config: %s's cannot be *'d until its driver is fixed\n",
			    d->d_name);
			errs++;
			continue;
		}
		for (n = 0; i != NULL; i = i->i_alias)
			if (!i->i_collapsed)
				n++;
		if (n < 1)
			panic("badstar() n<1");
		if (n == 1)
			continue;
		(void)fprintf(stderr,
		    "config: %d %s*'s in configuration; can only have 1\n",
		    n, d->d_name);
		errs++;
	}
	return (errs);
}
