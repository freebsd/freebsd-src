/* $Header: /src/pub/tcsh/sh.hist.c,v 3.27 2000/06/10 22:07:56 kim Exp $ */
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
#include "sh.h"

RCSID("$Id: sh.hist.c,v 3.27 2000/06/10 22:07:56 kim Exp $")

#include "tc.h"

extern bool histvalid;
extern Char histline[];
Char HistLit = 0;

static	bool	heq	__P((struct wordent *, struct wordent *));
static	void	hfree	__P((struct Hist *));
static	void	dohist1	__P((struct Hist *, int *, int));
static	void	phist	__P((struct Hist *, int));

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
savehist(sp, mflg)
    struct wordent *sp;
    bool mflg;
{
    register struct Hist *hp, *np;
    register int histlen = 0;
    Char   *cp;

    /* throw away null lines */
    if (sp && sp->next->word[0] == '\n')
	return;
    cp = varval(STRhistory);
    if (*cp) {
	register Char *p = cp;

	while (*p) {
	    if (!Isdigit(*p)) {
		histlen = 0;
		break;
	    }
	    histlen = histlen * 10 + *p++ - '0';
	}
    }
    if (sp)
	(void) enthist(++eventno, sp, 1, mflg);
    for (hp = &Histlist; (np = hp->Hnext) != NULL;)
	if (eventno - np->Href >= histlen || histlen == 0)
	    hp->Hnext = np->Hnext, hfree(np);
	else
	    hp = np;
}

static bool
heq(a0, b0)
    struct wordent *a0, *b0;
{
    struct wordent *a = a0->next, *b = b0->next;

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
enthist(event, lp, docopy, mflg)
    int     event;
    register struct wordent *lp;
    bool    docopy;
    bool    mflg;
{
    extern time_t Htime;
    struct Hist *p = NULL, *pp = &Histlist;
    int n, r;
    register struct Hist *np;
    Char *dp;
    
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

    np = p ? p : (struct Hist *) xmalloc((size_t) sizeof(*np));

    /* Pick up timestamp set by lex() in Htime if reading saved history */
    if (Htime != (time_t) 0) {
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
	    np->histline = Strsave(histline);
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
hfree(hp)
    register struct Hist *hp;
{

    freelex(&hp->Hlex);
    if (hp->histline)
	xfree((ptr_t) hp->histline);
    xfree((ptr_t) hp);
}


/*ARGSUSED*/
void
dohist(vp, c)
    Char  **vp;
    struct command *c;
{
    int     n, hflg = 0;

    USE(c);
    if (getn(varval(STRhistory)) == 0)
	return;
    if (setintr)
#ifdef BSDSIGS
	(void) sigsetmask(sigblock((sigmask_t) 0) & ~sigmask(SIGINT));
#else
	(void) sigrelse(SIGINT);
#endif
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

    if (hflg & (HIST_LOAD | HIST_MERGE)) {
	loadhist(*vp, (hflg & HIST_MERGE) ? 1 : 0);
	return;
    }
    else if (hflg & HIST_SAVE) {
	rechist(*vp, 1);
	return;
    }
    if (*vp)
	n = getn(*vp);
    else {
	n = getn(varval(STRhistory));
    }
    dohist1(Histlist.Hnext, &n, hflg);
}

static void
dohist1(hp, np, hflg)
    struct Hist *hp;
    int    *np, hflg;
{
    bool    print = (*np) > 0;

    for (; hp != 0; hp = hp->Hnext) {
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
phist(hp, hflg)
    register struct Hist *hp;
    int     hflg;
{
    extern bool output_raw;
    if (hflg & HIST_ONLY) {
       /*
        * Control characters have to be written as is (output_raw).
        * This way one can preserve special characters (like tab) in
        * the history file.
        * From: mveksler@vnet.ibm.com (Veksler Michael)
        */
        output_raw= 1;
	if (hflg & HIST_TIME)
	    /* 
	     * Make file entry with history time in format:
	     * "+NNNNNNNNNN" (10 digits, left padded with ascii '0') 
	     */

	    xprintf("#+%010lu\n", hp->Htime);

	if (HistLit && hp->histline)
	    xprintf("%S\n", hp->histline);
	else
	    prlex(&hp->Hlex);
        output_raw= 0;
    }
    else {
	Char   *cp = str2short("%h\t%T\t%R\n");
	Char buf[INBUFSIZE];
	struct varent *vp = adrof(STRhistory);

	if (vp && vp->vec[0] && vp->vec[1])
	    cp = vp->vec[1];

	tprintf(FMT_HISTORY, buf, cp, INBUFSIZE, NULL, hp->Htime, (ptr_t) hp);
	for (cp = buf; *cp;)
	    xputchar(*cp++);
    }
}


void
fmthist(fmt, ptr, buf, bufsiz)
    int fmt;
    ptr_t ptr;
    char *buf;
    size_t bufsiz;
{
    struct Hist *hp = (struct Hist *) ptr;
    switch (fmt) {
    case 'h':
	(void) xsnprintf(buf, bufsiz, "%6d", hp->Hnum);
	break;
    case 'R':
	if (HistLit && hp->histline)
	    (void) xsnprintf(buf, bufsiz, "%S", hp->histline);
	else {
	    Char ibuf[INBUFSIZE], *ip;
	    char *p;
	    (void) sprlex(ibuf, sizeof(ibuf) / sizeof(Char), &hp->Hlex);
	    for (p = buf, ip = ibuf; (*p++ = (CHAR & *ip++)) != '\0'; )
		continue;
	}
	break;
    default:
	buf[0] = '\0';
	break;
    }
	
}

void
rechist(fname, ref)
    Char *fname;
    int ref;
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

    /*
     * The 'savehist merge' feature is intended for an environment
     * with numerous shells beeing in simultaneous use. Imagine
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
    if ((shist = adrof(STRsavehist)) != NULL)
	if (shist->vec[1] && eq(shist->vec[1], STRmerge))
	    loadhist(fname, 1);
    fp = creat(short2str(fname), 0600);
    if (fp == -1) {
	didfds = oldidfds;
	return;
    }
    ftmp = SHOUT;
    SHOUT = fp;
    dumphist[2] = snum;
    dohist(dumphist, NULL);
    (void) close(fp);
    SHOUT = ftmp;
    didfds = oldidfds;
    xfree((ptr_t) fname);
}


void
loadhist(fname, mflg)
    Char *fname;
    bool mflg;
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
