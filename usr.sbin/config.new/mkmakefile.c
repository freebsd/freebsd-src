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
 *	@(#)mkmakefile.c	8.1 (Berkeley) 6/6/93
 */

#include <sys/param.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"

/*
 * Make the Makefile.
 */

static int emitdefs __P((FILE *));
static int emitobjs __P((FILE *));
static int emitcfiles __P((FILE *));
static int emitsfiles __P((FILE *));
static int emitfiles __P((FILE *, int));
static int emitrules __P((FILE *));
static int emitload __P((FILE *));

int
mkmakefile()
{
	register FILE *ifp, *ofp;
	register int lineno;
	register int (*fn) __P((FILE *));
	register char *ofname;
	char line[BUFSIZ], ifname[200];

	(void)sprintf(ifname, "Makefile.%s", machine);
	if ((ifp = fopen(ifname, "r")) == NULL) {
		(void)fprintf(stderr, "config: cannot read %s: %s\n",
		    ifname, strerror(errno));
		return (1);
	}
	ofname = path("Makefile");
	if ((ofp = fopen(ofname, "w")) == NULL) {
		(void)fprintf(stderr, "config: cannot write %s: %s\n",
		    ofname, strerror(errno));
		free(ofname);
		return (1);
	}
	if (emitdefs(ofp) != 0)
		goto wrerror;
	lineno = 0;
	while (fgets(line, sizeof(line), ifp) != NULL) {
		lineno++;
		if (line[0] != '%') {
			if (fputs(line, ofp) < 0)
				goto wrerror;
			continue;
		}
		if (strcmp(line, "%OBJS\n") == 0)
			fn = emitobjs;
		else if (strcmp(line, "%CFILES\n") == 0)
			fn = emitcfiles;
		else if (strcmp(line, "%SFILES\n") == 0)
			fn = emitsfiles;
		else if (strcmp(line, "%RULES\n") == 0)
			fn = emitrules;
		else if (strcmp(line, "%LOAD\n") == 0)
			fn = emitload;
		else {
			xerror(ifname, lineno,
			    "unknown %% construct ignored: %s", line);
			continue;
		}
		if ((*fn)(ofp))
			goto wrerror;
	}
	if (ferror(ifp)) {
		(void)fprintf(stderr,
		    "config: error reading %s (at line %d): %s\n",
		    ifname, lineno, strerror(errno));
		goto bad;
		/* (void)unlink(ofname); */
		free(ofname);
		return (1);
	}
	if (fclose(ofp)) {
		ofp = NULL;
		goto wrerror;
	}
	(void)fclose(ifp);
	free(ofname);
	return (0);
wrerror:
	(void)fprintf(stderr, "config: error writing %s: %s\n",
	    ofname, strerror(errno));
bad:
	if (ofp != NULL)
		(void)fclose(ofp);
	/* (void)unlink(ofname); */
	free(ofname);
	return (1);
}

static int
emitdefs(fp)
	register FILE *fp;
{
	register struct nvlist *nv;
	register char *sp;

	if (fputs("IDENT=", fp) < 0)
		return (1);
	sp = "";
	for (nv = options; nv != NULL; nv = nv->nv_next) {
		if (fprintf(fp, "%s-D%s%s%s", sp, nv->nv_name,
		    nv->nv_str ? "=" : "", nv->nv_str ? nv->nv_str : "") < 0)
			return (1);
		sp = " ";
	}
	if (putc('\n', fp) < 0)
		return (1);
	if (fprintf(fp, "PARAM=-DMAXUSERS=%d\n", maxusers) < 0)
		return (1);
	for (nv = mkoptions; nv != NULL; nv = nv->nv_next)
		if (fprintf(fp, "%s=%s\n", nv->nv_name, nv->nv_str) < 0)
			return (1);
	return (0);
}

static int
emitobjs(fp)
	register FILE *fp;
{
	register struct files *fi;
	register int lpos, len, sp;

	if (fputs("OBJS=", fp) < 0)
		return (1);
	sp = '\t';
	lpos = 7;
	for (fi = allfiles; fi != NULL; fi = fi->fi_next) {
		if ((fi->fi_flags & FI_SEL) == 0)
			continue;
		len = strlen(fi->fi_base) + 2;
		if (lpos + len > 72) {
			if (fputs(" \\\n", fp) < 0)
				return (1);
			sp = '\t';
			lpos = 7;
		}
		if (fprintf(fp, "%c%s.o", sp, fi->fi_base) < 0)
			return (1);
		lpos += len + 1;
		sp = ' ';
	}
	if (lpos != 7 && putc('\n', fp) < 0)
		return (1);
	return (0);
}

static int
emitcfiles(fp)
	FILE *fp;
{

	return (emitfiles(fp, 'c'));
}

static int
emitsfiles(fp)
	FILE *fp;
{

	return (emitfiles(fp, 's'));
}

static int
emitfiles(fp, suffix)
	register FILE *fp;
	int suffix;
{
	register struct files *fi;
	register struct config *cf;
	register int lpos, len, sp;
	char swapname[100];

	if (fprintf(fp, "%cFILES=", toupper(suffix)) < 0)
		return (1);
	sp = '\t';
	lpos = 7;
	for (fi = allfiles; fi != NULL; fi = fi->fi_next) {
		if ((fi->fi_flags & FI_SEL) == 0)
			continue;
		len = strlen(fi->fi_path);
		if (fi->fi_path[len - 1] != suffix)
			continue;
		if (*fi->fi_path != '/')
			len += 3;	/* "$S/" */
		if (lpos + len > 72) {
			if (fputs(" \\\n", fp) < 0)
				return (1);
			sp = '\t';
			lpos = 7;
		}
		if (fprintf(fp, "%c%s%s", sp, *fi->fi_path != '/' ? "$S/" : "",
		    fi->fi_path) < 0)
			return (1);
		lpos += len + 1;
		sp = ' ';
	}
	/*
	 * The allfiles list does not include the configuration-specific
	 * C source files.  These files should be eliminated someday, but
	 * for now, we have to add them to ${CFILES} (and only ${CFILES}).
	 */
	if (suffix == 'c') {
		for (cf = allcf; cf != NULL; cf = cf->cf_next) {
			if (cf->cf_root == NULL)
				(void)sprintf(swapname,
				    "$S/%s/%s/swapgeneric.c",
				    machine, machine);
			else
				(void)sprintf(swapname, "swap%s.c",
				    cf->cf_name);
			len = strlen(swapname);
			if (lpos + len > 72) {
				if (fputs(" \\\n", fp) < 0)
					return (1);
				sp = '\t';
				lpos = 7;
			}
			if (fprintf(fp, "%c%s", sp, swapname) < 0)
				return (1);
			lpos += len + 1;
			sp = ' ';
		}
	}
	if (lpos != 7 && putc('\n', fp) < 0)
		return (1);
	return (0);
}

/*
 * Emit the make-rules.
 */
static int
emitrules(fp)
	register FILE *fp;
{
	register struct files *fi;
	register const char *cp;
	int ch;
	char buf[200];

	for (fi = allfiles; fi != NULL; fi = fi->fi_next) {
		if ((fi->fi_flags & FI_SEL) == 0)
			continue;
		if (fprintf(fp, "%s.o: %s%s\n", fi->fi_base,
		    *fi->fi_path != '/' ? "$S/" : "", fi->fi_path) < 0)
			return (1);
		if ((cp = fi->fi_mkrule) == NULL) {
			cp = fi->fi_flags & FI_DRIVER ? "DRIVER" : "NORMAL";
			ch = fi->fi_lastc;
			if (islower(ch))
				ch = toupper(ch);
			(void)sprintf(buf, "${%s_%c%s}", cp, ch,
			    fi->fi_flags & FI_CONFIGDEP ? "_C" : "");
			cp = buf;
		}
		if (fprintf(fp, "\t%s\n\n", cp) < 0)
			return (1);
	}
	return (0);
}

/*
 * Emit the load commands.
 *
 * This function is not to be called `spurt'.
 */
static int
emitload(fp)
	register FILE *fp;
{
	register struct config *cf;
	register const char *nm, *swname;
	int first;

	if (fputs("all:", fp) < 0)
		return (1);
	for (cf = allcf; cf != NULL; cf = cf->cf_next) {
		if (fprintf(fp, " %s", cf->cf_name) < 0)
			return (1);
	}
	if (fputs("\n\n", fp) < 0)
		return (1);
	for (first = 1, cf = allcf; cf != NULL; cf = cf->cf_next) {
		nm = cf->cf_name;
		swname = cf->cf_root != NULL ? cf->cf_name : "generic";
		if (fprintf(fp, "%s: ${SYSTEM_DEP} swap%s.o", nm, swname) < 0)
			return (1);
		if (first) {
			if (fputs(" newvers", fp) < 0)
				return (1);
			first = 0;
		}
		if (fprintf(fp, "\n\
\t${SYSTEM_LD_HEAD}\n\
\t${SYSTEM_LD} swap%s.o\n\
\t${SYSTEM_LD_TAIL}\n\
\n\
swap%s.o: ", swname, swname) < 0)
			return (1);
		if (cf->cf_root != NULL) {
			if (fprintf(fp, "swap%s.c\n", nm) < 0)
				return (1);
		} else {
			if (fprintf(fp, "$S/%s/%s/swapgeneric.c\n",
			    machine, machine) < 0)
				return (1);
		}
		if (fputs("\t${NORMAL_C}\n\n", fp) < 0)
			return (1);
	}
	return (0);
}
