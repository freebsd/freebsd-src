/* $Header: /p/tcsh/cvsroot/tcsh/sh.hist.c,v 3.40 2007/03/01 17:14:51 christos Exp $ */
/*
 * sh.hist.c: Shell history expansions and substitutions
 */
/*-
 * Copyright (c) 1980, 1991 The Regents of the University of California.
 * All rights reserved.
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
#include "sh.h"

RCSID("$tcsh: sh.hist.c,v 3.40 2007/03/01 17:14:51 christos Exp $")

#include "tc.h"

extern int histvalid;
extern struct Strbuf histline;
Char HistLit = 0;

static	int	heq	(const struct wordent *, const struct wordent *);
static	void	hfree	(struct Hist *);
static	void	dohist1	(struct Hist *, int *, int);
static	void	phist	(struct Hist *, int);

#define HIST_ONLY	0x01
#define HIST_SAVE	0x02
#define HIST_LOAD	0x04
#define HIST_REV	0x08
#define HIST_CLEAR	0x10
#define HIST_MERGE	0x20
#define HIST_TIME	0x40

/*
 * C shell
 */

void
savehist(struct wordent *sp, int mflg)
{
    struct Hist *hp, *np;
    int histlen = 0;
    Char   *cp;

    /* throw away null lines */
    if (sp && sp->next->word[0] == '\n')
	return;
    cp = varval(STRhistory);
    while (*cp) {
	if (!Isdigit(*cp)) {
	    histlen = 0;
	    break;
	}
	histlen = histlen * 10 + *cp++ - '0';
    }
    if (sp)
	(void) enthist(++eventno, sp, 1, mflg);
    for (hp = &Histlist; (np = hp->Hnext) != NULL;)
	if (eventno - np->Href >= histlen || histlen == 0)
	    hp->Hnext = np->Hnext, hfree(np);
	else
	    hp = np;
}

static int
heq(const struct wordent *a0, const struct wordent *b0)
{
    const struct wordent *a = a0->next, *b = b0->next;

    for (;;) {
	if (Strcmp(a->word, b->word) != 0)
	    return 0;
	a = a->next;
	b = b->next;
	if (a == a0)
	    return (b == b0) ? 1 : 0;
	if (b == b0)
	    return 0;
    } 
}


struct Hist *
enthist(int event, struct wordent *lp, int docopy, int mflg)
{
    struct Hist *p = NULL, *pp = &Histlist;
    int n, r;
    struct Hist *np;
    const Char *dp;
    
    if ((dp = varval(STRhistdup)) != STRNULL) {
	if (eq(dp, STRerase)) {
	    /* masaoki@akebono.tky.hp.com (Kobayashi Masaoki) */
	    struct Hist *px;
	    for (p = pp; (px = p, p = p->Hnext) != NULL;)
		if (heq(lp, &(p->Hlex))){
		    px->Hnext = p->Hnext;
		    if (Htime != 0 && p->Htime > Htime)
			Htime = p->Htime;
		    n = p->Href;
		    hfree(p);
		    for (p = px->Hnext; p != NULL; p = p->Hnext)
			p->Href = n--;
		    break;
		}
	}
	else if (eq(dp, STRall)) {
	    for (p = pp; (p = p->Hnext) != NULL;)
		if (heq(lp, &(p->Hlex))) {
		    eventno--;
		    break;
		}
	}
	else if (eq(dp, STRprev)) {
	    if (pp->Hnext && heq(lp, &(pp->Hnext->Hlex))) {
		p = pp->Hnext;
		eventno--;
	    }
	}
    }

    np = p ? p : xmalloc(sizeof(*np));

    /* Pick up timestamp set by lex() in Htime if reading saved history */
    if (Htime != 0) {
	np->Htime = Htime;
	Htime = 0;
    }
    else
	(void) time(&(np->Htime));

    if (p == np)
	return np;

    np->Hnum = np->Href = event;
    if (docopy) {
	copylex(&np->Hlex, lp);
	if (histvalid)
	    np->histline = Strsave(histline.s);
	else
	    np->histline = NULL;
    }
    else {
	np->Hlex.next = lp->next;
	lp->next->prev = &np->Hlex;
	np->Hlex.prev = lp->prev;
	lp->prev->next = &np->Hlex;
	np->histline = NULL;
    }
    if (mflg)
      {
        while ((p = pp->Hnext) && (p->Htime > np->Htime))
	  pp = p;
	while (p && p->Htime == np->Htime)
	  {
	    if (heq(&p->Hlex, &np->Hlex))
	      {
	        eventno--;
		hfree(np);
	        return (p);
	      }
	    pp = p;
	    p = p->Hnext;
	  }
	for (p = Histlist.Hnext; p != pp->Hnext; p = p->Hnext)
	  {
	    n = p->Hnum; r = p->Href;
	    p->Hnum = np->Hnum; p->Href = np->Href;
	    np->Hnum = n; np->Href = r;
	  }
      }
    np->Hnext = pp->Hnext;
    pp->Hnext = np;
    return (np);
}

static void
hfree(struct Hist *hp)
{

    freelex(&hp->Hlex);
    if (hp->histline)
	xfree(hp->histline);
    xfree(hp);
}


/*ARGSUSED*/
void
dohist(Char **vp, struct command *c)
{
    int     n, hflg = 0;

    USE(c);
    if (getn(varval(STRhistory)) == 0)
	return;
    while (*++vp && **vp == '-') {
	Char   *vp2 = *vp;

	while (*++vp2)
	    switch (*vp2) {
	    case 'c':
		hflg |= HIST_CLEAR;
		break;
	    case 'h':
		hflg |= HIST_ONLY;
		break;
	    case 'r':
		hflg |= HIST_REV;
		break;
	    case 'S':
		hflg |= HIST_SAVE;
		break;
	    case 'L':
		hflg |= HIST_LOAD;
		break;
	    case 'M':
	    	hflg |= HIST_MERGE;
		break;
	    case 'T':
	    	hflg |= HIST_TIME;
		break;
	    default:
		stderror(ERR_HISTUS, "chrSLMT");
		break;
	    }
    }

    if (hflg & HIST_CLEAR) {
	struct Hist *np, *hp;
	for (hp = &Histlist; (np = hp->Hnext) != NULL;)
	    hp->Hnext = np->Hnext, hfree(np);
    }

    if (hflg & (HIST_LOAD | HIST_MERGE))
	loadhist(*vp, (hflg & HIST_MERGE) ? 1 : 0);
    else if (hflg & HIST_SAVE)
	rechist(*vp, 1);
    else {
	if (*vp)
	    n = getn(*vp);
	else {
	    n = getn(varval(STRhistory));
	}
	dohist1(Histlist.Hnext, &n, hflg);
    }
}

static void
dohist1(struct Hist *hp, int *np, int hflg)
{
    int    print = (*np) > 0;

    for (; hp != 0; hp = hp->Hnext) {
	if (setintr) {
	    int old_pintr_disabled;

	    pintr_push_enable(&old_pintr_disabled);
	    cleanup_until(&old_pintr_disabled);
	}
	(*np)--;
	if ((hflg & HIST_REV) == 0) {
	    dohist1(hp->Hnext, np, hflg);
	    if (print)
		phist(hp, hflg);
	    return;
	}
	if (*np >= 0)
	    phist(hp, hflg);
    }
}

static void
phist(struct Hist *hp, int hflg)
{
    if (hflg & HIST_ONLY) {
	int old_output_raw;

       /*
        * Control characters have to be written as is (output_raw).
        * This way one can preserve special characters (like tab) in
        * the history file.
        * From: mveksler@vnet.ibm.com (Veksler Michael)
        */
	old_output_raw = output_raw;
        output_raw = 1;
	cleanup_push(&old_output_raw, output_raw_restore);
	if (hflg & HIST_TIME)
	    /* 
	     * Make file entry with history time in format:
	     * "+NNNNNNNNNN" (10 digits, left padded with ascii '0') 
	     */

	    xprintf("#+%010lu\n", (unsigned long)hp->Htime);

	if (HistLit && hp->histline)
	    xprintf("%S\n", hp->histline);
	else
	    prlex(&hp->Hlex);
        cleanup_until(&old_output_raw);
    }
    else {
	Char   *cp = str2short("%h\t%T\t%R\n");
	Char *p;
	struct varent *vp = adrof(STRhistory);

	if (vp && vp->vec != NULL && vp->vec[0] && vp->vec[1])
	    cp = vp->vec[1];

	p = tprintf(FMT_HISTORY, cp, NULL, hp->Htime, hp);
	cleanup_push(p, xfree);
	for (cp = p; *cp;)
	    xputwchar(*cp++);
	cleanup_until(p);
    }
}


char *
fmthist(int fmt, ptr_t ptr)
{
    struct Hist *hp = ptr;
    char *buf;

    switch (fmt) {
    case 'h':
	return xasprintf("%6d", hp->Hnum);
    case 'R':
	if (HistLit && hp->histline)
	    return xasprintf("%S", hp->histline);
	else {
	    Char *istr, *ip;
	    char *p;

	    istr = sprlex(&hp->Hlex);
	    buf = xmalloc(Strlen(istr) * MB_LEN_MAX + 1);

	    for (p = buf, ip = istr; *ip != '\0'; ip++)
		p += one_wctomb(p, CHAR & *ip);

	    *p = '\0';
	    xfree(istr);
	    return buf;
	}
    default:
	buf = xmalloc(1);
	buf[0] = '\0';
	return buf;
    }
}

void
rechist(Char *fname, int ref)
{
    Char    *snum;
    int     fp, ftmp, oldidfds;
    struct varent *shist;
    static Char   *dumphist[] = {STRhistory, STRmhT, 0, 0};

    if (fname == NULL && !ref) 
	return;
    /*
     * If $savehist is just set, we use the value of $history
     * else we use the value in $savehist
     */
    if (((snum = varval(STRsavehist)) == STRNULL) &&
	((snum = varval(STRhistory)) == STRNULL))
	snum = STRmaxint;


    if (fname == NULL) {
	if ((fname = varval(STRhistfile)) == STRNULL)
	    fname = Strspl(varval(STRhome), &STRtildothist[1]);
	else
	    fname = Strsave(fname);
    }
    else
	fname = globone(fname, G_ERROR);
    cleanup_push(fname, xfree);

    /*
     * The 'savehist merge' feature is intended for an environment
     * with numerous shells being in simultaneous use. Imagine
     * any kind of window system. All these shells 'share' the same 
     * ~/.history file for recording their command line history. 
     * Currently the automatic merge can only succeed when the shells
     * nicely quit one after another. 
     *
     * Users that like to nuke their environment require here an atomic
     * 	loadhist-creat-dohist(dumphist)-close
     * sequence.
     *
     * jw.
     */ 
    /*
     * We need the didfds stuff before loadhist otherwise
     * exec in a script will fail to print if merge is set.
     * From: mveksler@iil.intel.com (Veksler Michael)
     */
    oldidfds = didfds;
    didfds = 0;
    if ((shist = adrof(STRsavehist)) != NULL && shist->vec != NULL)
	if (shist->vec[1] && eq(shist->vec[1], STRmerge))
	    loadhist(fname, 1);
    fp = xcreat(short2str(fname), 0600);
    if (fp == -1) {
	didfds = oldidfds;
	cleanup_until(fname);
	return;
    }
    ftmp = SHOUT;
    SHOUT = fp;
    dumphist[2] = snum;
    dohist(dumphist, NULL);
    xclose(fp);
    SHOUT = ftmp;
    didfds = oldidfds;
    cleanup_until(fname);
}


void
loadhist(Char *fname, int mflg)
{
    static Char   *loadhist_cmd[] = {STRsource, NULL, NULL, NULL};
    loadhist_cmd[1] = mflg ? STRmm : STRmh;

    if (fname != NULL)
	loadhist_cmd[2] = fname;
    else if ((fname = varval(STRhistfile)) != STRNULL)
	loadhist_cmd[2] = fname;
    else
	loadhist_cmd[2] = STRtildothist;

    dosource(loadhist_cmd, NULL);
}
