/* $Header: /src/pub/tcsh/sh.set.c,v 3.39 2001/03/18 19:06:30 christos Exp $ */
/*
 * sh.set.c: Setting and Clearing of variables
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

RCSID("$Id: sh.set.c,v 3.39 2001/03/18 19:06:30 christos Exp $")

#include "ed.h"
#include "tw.h"

extern Char HistLit;
extern bool GotTermCaps;

static	void		 update_vars	__P((Char *));
static	Char		*getinx		__P((Char *, int *));
static	void		 asx		__P((Char *, int, Char *));
static	struct varent 	*getvx		__P((Char *, int));
static	Char		*xset		__P((Char *, Char ***));
static	Char		*operate	__P((int, Char *, Char *));
static	void	 	 putn1		__P((int));
static	struct varent	*madrof		__P((Char *, struct varent *));
static	void		 unsetv1	__P((struct varent *));
static	void		 exportpath	__P((Char **));
static	void		 balance	__P((struct varent *, int, int));

/*
 * C Shell
 */

static void
update_vars(vp)
    Char *vp;
{
    if (eq(vp, STRpath)) {
	exportpath(adrof(STRpath)->vec);
	dohash(NULL, NULL);
    }
    else if (eq(vp, STRhistchars)) {
	register Char *pn = varval(vp);

	HIST = *pn++;
	HISTSUB = *pn;
    }
    else if (eq(vp, STRpromptchars)) {
	register Char *pn = varval(vp);

	PRCH = *pn++;
	PRCHROOT = *pn;
    }
    else if (eq(vp, STRhistlit)) {
	HistLit = 1;
    }
    else if (eq(vp, STRuser)) {
	tsetenv(STRKUSER, varval(vp));
	tsetenv(STRLOGNAME, varval(vp));
    }
    else if (eq(vp, STRgroup)) {
	tsetenv(STRKGROUP, varval(vp));
    }
    else if (eq(vp, STRwordchars)) {
	word_chars = varval(vp);
    }
    else if (eq(vp, STRloginsh)) {
	loginsh = 1;
    }
    else if (eq(vp, STRsymlinks)) {
	register Char *pn = varval(vp);

	if (eq(pn, STRignore))
	    symlinks = SYM_IGNORE;
	else if (eq(pn, STRexpand))
	    symlinks = SYM_EXPAND;
	else if (eq(pn, STRchase))
	    symlinks = SYM_CHASE;
	else
	    symlinks = 0;
    }
    else if (eq(vp, STRterm)) {
	Char *cp = varval(vp);
	tsetenv(STRKTERM, cp);
#ifdef DOESNT_WORK_RIGHT
	cp = getenv("TERMCAP");
	if (cp && (*cp != '/'))	/* if TERMCAP and not a path */
	    Unsetenv(STRTERMCAP);
#endif /* DOESNT_WORK_RIGHT */
	GotTermCaps = 0;
	if (noediting && Strcmp(cp, STRnetwork) != 0 &&
	    Strcmp(cp, STRunknown) != 0 && Strcmp(cp, STRdumb) != 0) {
	    editing = 1;
	    noediting = 0;
	    set(STRedit, Strsave(STRNULL), VAR_READWRITE);
	}
	ed_Init();		/* reset the editor */
    }
    else if (eq(vp, STRhome)) {
	register Char *cp;

	cp = Strsave(varval(vp));	/* get the old value back */

	/*
	 * convert to cononical pathname (possibly resolving symlinks)
	 */
	cp = dcanon(cp, cp);

	set(vp, Strsave(cp), VAR_READWRITE);	/* have to save the new val */

	/* and now mirror home with HOME */
	tsetenv(STRKHOME, cp);
	/* fix directory stack for new tilde home */
	dtilde();
	xfree((ptr_t) cp);
    }
    else if (eq(vp, STRedit)) {
	editing = 1;
	noediting = 0;
	/* PWP: add more stuff in here later */
    }
    else if (eq(vp, STRshlvl)) {
	tsetenv(STRKSHLVL, varval(vp));
    }
    else if (eq(vp, STRbackslash_quote)) {
	bslash_quote = 1;
    }
    else if (eq(vp, STRdirstack)) {
	dsetstack();
    }
    else if (eq(vp, STRrecognize_only_executables)) {
	tw_cmd_free();
    }
    else if (eq(vp, STRkillring)) {
	SetKillRing(getn(varval(vp)));
    }
#ifndef HAVENOUTMP
    else if (eq(vp, STRwatch)) {
	resetwatch();
    }
#endif /* HAVENOUTMP */
    else if (eq(vp, STRimplicitcd)) {
	implicit_cd = ((eq(varval(vp), STRverbose)) ? 2 : 1);
    }
#ifdef COLOR_LS_F
    else if (eq(vp, STRcolor)) {
	set_color_context();
    }
#endif /* COLOR_LS_F */
#if defined(KANJI) && defined(SHORT_STRINGS) && defined(DSPMBYTE)
    else if(eq(vp, CHECK_MBYTEVAR) || eq(vp, STRnokanji)) {
	update_dspmbyte_vars();
    }
#endif
#ifdef NLS_CATALOGS
    else if (eq(vp, STRcatalog)) {
	(void) catclose(catd);
	nlsinit();
    }
#endif /* NLS_CATALOGS */
}


/*ARGSUSED*/
void
doset(v, c)
    register Char **v;
    struct command *c;
{
    register Char *p;
    Char   *vp, op;
    Char  **vecp;
    bool    hadsub;
    int     subscr;
    int	    flags = VAR_READWRITE;
    bool    first_match = 0;
    bool    last_match = 0;
    bool    changed = 0;

    USE(c);
    v++;
    do {
	changed = 0;
	/*
	 * Readonly addition From: Tim P. Starrin <noid@cyborg.larc.nasa.gov>
	 */
	if (*v && eq(*v, STRmr)) {
	    flags = VAR_READONLY;
	    v++;
	    changed = 1;
	}
	if (*v && eq(*v, STRmf) && !last_match) {
	    first_match = 1;
	    v++;
	    changed = 1;
	}
	if (*v && eq(*v, STRml) && !first_match) {
	    last_match = 1;
	    v++;
	    changed = 1;
	}
    } while(changed);
    p = *v++;
    if (p == 0) {
	plist(&shvhed, flags);
	return;
    }
    do {
	hadsub = 0;
	vp = p;
	if (letter(*p))
	    for (; alnum(*p); p++)
		continue;
	if (vp == p || !letter(*vp))
	    stderror(ERR_NAME | ERR_VARBEGIN);
	if ((p - vp) > MAXVARLEN) {
	    stderror(ERR_NAME | ERR_VARTOOLONG);
	    return;
	}
	if (*p == '[') {
	    hadsub++;
	    p = getinx(p, &subscr);
	}
	if ((op = *p) != 0) {
	    *p++ = 0;
	    if (*p == 0 && *v && **v == '(')
		p = *v++;
	}
	else if (*v && eq(*v, STRequal)) {
	    op = '=', v++;
	    if (*v)
		p = *v++;
	}
	if (op && op != '=')
	    stderror(ERR_NAME | ERR_SYNTAX);
	if (eq(p, STRLparen)) {
	    register Char **e = v;

	    if (hadsub)
		stderror(ERR_NAME | ERR_SYNTAX);
	    for (;;) {
		if (!*e)
		    stderror(ERR_NAME | ERR_MISSING, ')');
		if (**e == ')')
		    break;
		e++;
	    }
	    p = *e;
	    *e = 0;
	    vecp = saveblk(v);
	    if (first_match)
	       flags |= VAR_FIRST;
	    else if (last_match)
	       flags |= VAR_LAST;

	    set1(vp, vecp, &shvhed, flags);
	    *e = p;
	    v = e + 1;
	}
	else if (hadsub)
	    asx(vp, subscr, Strsave(p));
	else
	    set(vp, Strsave(p), flags);
	update_vars(vp);
    } while ((p = *v++) != NULL);
}

static Char *
getinx(cp, ip)
    register Char *cp;
    register int *ip;
{
    *ip = 0;
    *cp++ = 0;
    while (*cp && Isdigit(*cp))
	*ip = *ip * 10 + *cp++ - '0';
    if (*cp++ != ']')
	stderror(ERR_NAME | ERR_SUBSCRIPT);
    return (cp);
}

static void
asx(vp, subscr, p)
    Char   *vp;
    int     subscr;
    Char   *p;
{
    register struct varent *v = getvx(vp, subscr);

    if (v->v_flags & VAR_READONLY)
	stderror(ERR_READONLY|ERR_NAME, v->v_name);
    xfree((ptr_t) v->vec[subscr - 1]);
    v->vec[subscr - 1] = globone(p, G_APPEND);
}

static struct varent *
getvx(vp, subscr)
    Char   *vp;
    int     subscr;
{
    register struct varent *v = adrof(vp);

    if (v == 0)
	udvar(vp);
    if (subscr < 1 || subscr > blklen(v->vec))
	stderror(ERR_NAME | ERR_RANGE);
    return (v);
}

/*ARGSUSED*/
void
dolet(v, dummy)
    Char  **v;
    struct command *dummy;
{
    register Char *p;
    Char   *vp, c, op;
    bool    hadsub;
    int     subscr;

    USE(dummy);
    v++;
    p = *v++;
    if (p == 0) {
	prvars();
	return;
    }
    do {
	hadsub = 0;
	vp = p;
	if (letter(*p))
	    for (; alnum(*p); p++)
		continue;
	if (vp == p || !letter(*vp))
	    stderror(ERR_NAME | ERR_VARBEGIN);
	if ((p - vp) > MAXVARLEN)
	    stderror(ERR_NAME | ERR_VARTOOLONG);
	if (*p == '[') {
	    hadsub++;
	    p = getinx(p, &subscr);
	}
	if (*p == 0 && *v)
	    p = *v++;
	if ((op = *p) != 0)
	    *p++ = 0;
	else
	    stderror(ERR_NAME | ERR_ASSIGN);

	/*
	 * if there is no expression after the '=' then print a "Syntax Error"
	 * message - strike
	 */
	if (*p == '\0' && *v == NULL)
	    stderror(ERR_NAME | ERR_ASSIGN);

	vp = Strsave(vp);
	if (op == '=') {
	    c = '=';
	    p = xset(p, &v);
	}
	else {
	    c = *p++;
	    if (any("+-", c)) {
		if (c != op || *p)
		    stderror(ERR_NAME | ERR_UNKNOWNOP);
		p = Strsave(STR1);
	    }
	    else {
		if (any("<>", op)) {
		    if (c != op)
			stderror(ERR_NAME | ERR_UNKNOWNOP);
		    c = *p++;
		    stderror(ERR_NAME | ERR_SYNTAX);
		}
		if (c != '=')
		    stderror(ERR_NAME | ERR_UNKNOWNOP);
		p = xset(p, &v);
	    }
	}
	if (op == '=') {
	    if (hadsub)
		asx(vp, subscr, p);
	    else
		set(vp, p, VAR_READWRITE);
	}
	else if (hadsub) {
	    struct varent *gv = getvx(vp, subscr);

	    asx(vp, subscr, operate(op, gv->vec[subscr - 1], p));
	}
	else
	    set(vp, operate(op, varval(vp), p), VAR_READWRITE);
	update_vars(vp);
	xfree((ptr_t) vp);
	if (c != '=')
	    xfree((ptr_t) p);
    } while ((p = *v++) != NULL);
}

static Char *
xset(cp, vp)
    Char   *cp, ***vp;
{
    register Char *dp;

    if (*cp) {
	dp = Strsave(cp);
	--(*vp);
	xfree((ptr_t) ** vp);
	**vp = dp;
    }
    return (putn(expr(vp)));
}

static Char *
operate(op, vp, p)
    int     op;
    Char    *vp, *p;
{
    Char    opr[2];
    Char   *vec[5];
    register Char **v = vec;
    Char  **vecp = v;
    register int i;

    if (op != '=') {
	if (*vp)
	    *v++ = vp;
	opr[0] = (Char) op;
	opr[1] = 0;
	*v++ = opr;
	if (op == '<' || op == '>')
	    *v++ = opr;
    }
    *v++ = p;
    *v++ = 0;
    i = expr(&vecp);
    if (*vecp)
	stderror(ERR_NAME | ERR_EXPRESSION);
    return (putn(i));
}

static Char *putp, nbuf[50];

Char   *
putn(n)
    register int n;
{
    int     num;

    putp = nbuf;
    if (n < 0) {
	n = -n;
	*putp++ = '-';
    }
    num = 2;			/* confuse lint */
    if (sizeof(int) == num && ((unsigned int) n) == 0x8000) {
	*putp++ = '3';
	n = 2768;
#ifdef pdp11
    }
#else /* !pdp11 */
    }
    else {
	num = 4;		/* confuse lint */
	if (sizeof(int) == num && ((unsigned int) n) == 0x80000000) {
	    *putp++ = '2';
	    n = 147483648;
	}
    }
#endif /* pdp11 */
    putn1(n);
    *putp = 0;
    return (Strsave(nbuf));
}

static void
putn1(n)
    register int n;
{
    if (n > 9)
	putn1(n / 10);
    *putp++ = n % 10 + '0';
}

int
getn(cp)
    register Char *cp;
{
    register int n;
    int     sign;

    if (!cp)			/* PWP: extra error checking */
	stderror(ERR_NAME | ERR_BADNUM);

    sign = 0;
    if (cp[0] == '+' && cp[1])
	cp++;
    if (*cp == '-') {
	sign++;
	cp++;
	if (!Isdigit(*cp))
	    stderror(ERR_NAME | ERR_BADNUM);
    }
    n = 0;
    while (Isdigit(*cp))
	n = n * 10 + *cp++ - '0';
    if (*cp)
	stderror(ERR_NAME | ERR_BADNUM);
    return (sign ? -n : n);
}

Char   *
value1(var, head)
    Char   *var;
    struct varent *head;
{
    register struct varent *vp;

    if (!var || !head)		/* PWP: extra error checking */
	return (STRNULL);

    vp = adrof1(var, head);
    return (vp == 0 || vp->vec[0] == 0 ? STRNULL : vp->vec[0]);
}

static struct varent *
madrof(pat, vp)
    Char   *pat;
    register struct varent *vp;
{
    register struct varent *vp1;

    for (vp = vp->v_left; vp; vp = vp->v_right) {
	if (vp->v_left && (vp1 = madrof(pat, vp)) != NULL)
	    return vp1;
	if (Gmatch(vp->v_name, pat))
	    return vp;
    }
    return vp;
}

struct varent *
adrof1(name, v)
    register Char *name;
    register struct varent *v;
{
    int cmp;

    v = v->v_left;
    while (v && ((cmp = *name - *v->v_name) != 0 || 
		 (cmp = Strcmp(name, v->v_name)) != 0))
	if (cmp < 0)
	    v = v->v_left;
	else
	    v = v->v_right;
    return v;
}

/*
 * The caller is responsible for putting value in a safe place
 */
void
set(var, val, flags)
    Char   *var, *val;
    int	   flags;
{
    register Char **vec = (Char **) xmalloc((size_t) (2 * sizeof(Char **)));

    vec[0] = val;
    vec[1] = 0;
    set1(var, vec, &shvhed, flags);
}

void
set1(var, vec, head, flags)
    Char   *var, **vec;
    struct varent *head;
    int flags;
{
    register Char **oldv = vec;

    if ((flags & VAR_NOGLOB) == 0) {
	gflag = 0;
	tglob(oldv);
	if (gflag) {
	    vec = globall(oldv);
	    if (vec == 0) {
		blkfree(oldv);
		stderror(ERR_NAME | ERR_NOMATCH);
		return;
	    }
	    blkfree(oldv);
	    gargv = 0;
	}
    }
    /*
     * Uniqueness addition from: Michael Veksler <mveksler@vnet.ibm.com>
     */
    if ( flags & (VAR_FIRST | VAR_LAST) ) {
	/*
	 * Code for -f (VAR_FIRST) and -l (VAR_LAST) options.
	 * Method:
	 *  Delete all duplicate words leaving "holes" in the word array (vec).
	 *  Then remove the "holes", keeping the order of the words unchanged.
	 */
	if (vec && vec[0] && vec[1]) { /* more than one word ? */
	    int i, j;
	    int num_items;

	    for (num_items = 0; vec[num_items]; num_items++)
	        continue;
	    if (flags & VAR_FIRST) {
		/* delete duplications, keeping first occurance */
		for (i = 1; i < num_items; i++)
		    for (j = 0; j < i; j++)
			/* If have earlier identical item, remove i'th item */
			if (vec[i] && vec[j] && Strcmp(vec[j], vec[i]) == 0) {
			    free(vec[i]);
			    vec[i] = NULL;
			    break;
			}
	    } else if (flags & VAR_LAST) {
	      /* delete duplications, keeping last occurance */
		for (i = 0; i < num_items - 1; i++)
		    for (j = i + 1; j < num_items; j++)
			/* If have later identical item, remove i'th item */
			if (vec[i] && vec[j] && Strcmp(vec[j], vec[i]) == 0) {
			    /* remove identical item (the first) */
			    free(vec[i]);
			    vec[i] = NULL;
			}
	    }
	    /* Compress items - remove empty items */
	    for (j = i = 0; i < num_items; i++)
	       if (vec[i]) 
		  vec[j++] = vec[i];

	    /* NULL-fy remaining items */
	    for (; j < num_items; j++)
		 vec[j] = NULL;
	}
	/* don't let the attribute propagate */
	flags &= ~(VAR_FIRST|VAR_LAST);
    } 
    setq(var, vec, head, flags);
}


void
setq(name, vec, p, flags)
    Char   *name, **vec;
    register struct varent *p;
    int flags;
{
    register struct varent *c;
    register int f;

    f = 0;			/* tree hangs off the header's left link */
    while ((c = p->v_link[f]) != 0) {
	if ((f = *name - *c->v_name) == 0 &&
	    (f = Strcmp(name, c->v_name)) == 0) {
	    if (c->v_flags & VAR_READONLY)
		stderror(ERR_READONLY|ERR_NAME, c->v_name);
	    blkfree(c->vec);
	    c->v_flags = flags;
	    trim(c->vec = vec);
	    return;
	}
	p = c;
	f = f > 0;
    }
    p->v_link[f] = c = (struct varent *) xmalloc((size_t)sizeof(struct varent));
    c->v_name = Strsave(name);
    c->v_flags = flags;
    c->v_bal = 0;
    c->v_left = c->v_right = 0;
    c->v_parent = p;
    balance(p, f, 0);
    trim(c->vec = vec);
}

/*ARGSUSED*/
void
unset(v, c)
    Char   **v;
    struct command *c;
{
    bool did_roe, did_edit;

    USE(c);
    did_roe = adrof(STRrecognize_only_executables) != NULL;
    did_edit = adrof(STRedit) != NULL;
    unset1(v, &shvhed);
    if (adrof(STRhistchars) == 0) {
	HIST = '!';
	HISTSUB = '^';
    }
    if (adrof(STRpromptchars) == 0) {
	PRCH = '>';
	PRCHROOT = '#';
    }
    if (adrof(STRhistlit) == 0)
	HistLit = 0;
    if (adrof(STRloginsh) == 0)
	loginsh = 0;
    if (adrof(STRwordchars) == 0)
	word_chars = STR_WORD_CHARS;
    if (adrof(STRedit) == 0)
	editing = 0;
    if (adrof(STRbackslash_quote) == 0)
	bslash_quote = 0;
    if (adrof(STRsymlinks) == 0)
	symlinks = 0;
    if (adrof(STRimplicitcd) == 0)
	implicit_cd = 0;
    if (adrof(STRkillring) == 0)
	SetKillRing(0);
    if (did_edit && noediting && adrof(STRedit) == 0)
	noediting = 0;
    if (did_roe && adrof(STRrecognize_only_executables) == 0)
	tw_cmd_free();
#ifdef COLOR_LS_F
    if (adrof(STRcolor) == 0)
	set_color_context();
#endif /* COLOR_LS_F */
#if defined(KANJI) && defined(SHORT_STRINGS) && defined(DSPMBYTE)
    update_dspmbyte_vars();
#endif
#ifdef NLS_CATALOGS
    (void) catclose(catd);
    nlsinit();
#endif /* NLS_CATALOGS */
}

void
unset1(v, head)
    register Char *v[];
    struct varent *head;
{
    register struct varent *vp;
    register int cnt;

    while (*++v) {
	cnt = 0;
	while ((vp = madrof(*v, head)) != NULL)
	    if (vp->v_flags & VAR_READONLY)
		stderror(ERR_READONLY|ERR_NAME, vp->v_name);
	    else
		unsetv1(vp), cnt++;
	if (cnt == 0)
	    setname(short2str(*v));
    }
}

void
unsetv(var)
    Char   *var;
{
    register struct varent *vp;

    if ((vp = adrof1(var, &shvhed)) == 0)
	udvar(var);
    unsetv1(vp);
}

static void
unsetv1(p)
    register struct varent *p;
{
    register struct varent *c, *pp;
    register int f;

    /*
     * Free associated memory first to avoid complications.
     */
    blkfree(p->vec);
    xfree((ptr_t) p->v_name);
    /*
     * If p is missing one child, then we can move the other into where p is.
     * Otherwise, we find the predecessor of p, which is guaranteed to have no
     * right child, copy it into p, and move it's left child into it.
     */
    if (p->v_right == 0)
	c = p->v_left;
    else if (p->v_left == 0)
	c = p->v_right;
    else {
	for (c = p->v_left; c->v_right; c = c->v_right)
	    continue;
	p->v_name = c->v_name;
	p->v_flags = c->v_flags;
	p->vec = c->vec;
	p = c;
	c = p->v_left;
    }

    /*
     * Move c into where p is.
     */
    pp = p->v_parent;
    f = pp->v_right == p;
    if ((pp->v_link[f] = c) != 0)
	c->v_parent = pp;
    /*
     * Free the deleted node, and rebalance.
     */
    xfree((ptr_t) p);
    balance(pp, f, 1);
}

void
setNS(cp)
    Char   *cp;
{
    set(cp, Strsave(STRNULL), VAR_READWRITE);
}

/*ARGSUSED*/
void
shift(v, c)
    register Char **v;
    struct command *c;
{
    register struct varent *argv;
    register Char *name;

    USE(c);
    v++;
    name = *v;
    if (name == 0)
	name = STRargv;
    else
	(void) strip(name);
    argv = adrof(name);
    if (argv == 0)
	udvar(name);
    if (argv->vec[0] == 0)
	stderror(ERR_NAME | ERR_NOMORE);
    lshift(argv->vec, 1);
    update_vars(name);
}

static Char STRsep[2] = { PATHSEP, '\0' };

static void
exportpath(val)
    Char  **val;
{
  Char    	*exppath;
  size_t	exppath_size = BUFSIZE;
  exppath = (Char *)xmalloc(sizeof(Char)*exppath_size);

    exppath[0] = 0;
    if (val)
	while (*val) {
	  while (Strlen(*val) + Strlen(exppath) + 2 > exppath_size) {
	    if ((exppath
		 = (Char *)xrealloc(exppath, sizeof(Char)*(exppath_size *= 2)))
		 == NULL) {
		xprintf(CGETS(18, 1,
			      "Warning: ridiculously long PATH truncated\n"));
		break;
	      }
	    }
	    (void) Strcat(exppath, *val++);
	    if (*val == 0 || eq(*val, STRRparen))
	      break;
	    (void) Strcat(exppath, STRsep);
	  }
  tsetenv(STRKPATH, exppath);
  free(exppath);
}

#ifndef lint
 /*
  * Lint thinks these have null effect
  */
 /* macros to do single rotations on node p */
# define rright(p) (\
	t = (p)->v_left,\
	(t)->v_parent = (p)->v_parent,\
	(((p)->v_left = t->v_right) != NULL) ?\
	    (t->v_right->v_parent = (p)) : 0,\
	(t->v_right = (p))->v_parent = t,\
	(p) = t)
# define rleft(p) (\
	t = (p)->v_right,\
	((t)->v_parent = (p)->v_parent,\
	((p)->v_right = t->v_left) != NULL) ? \
		(t->v_left->v_parent = (p)) : 0,\
	(t->v_left = (p))->v_parent = t,\
	(p) = t)
#else
static struct varent *
rleft(p)
    struct varent *p;
{
    return (p);
}
static struct varent *
rright(p)
    struct varent *p;
{
    return (p);
}

#endif /* ! lint */


/*
 * Rebalance a tree, starting at p and up.
 * F == 0 means we've come from p's left child.
 * D == 1 means we've just done a delete, otherwise an insert.
 */
static void
balance(p, f, d)
    register struct varent *p;
    register int f, d;
{
    register struct varent *pp;

#ifndef lint
    register struct varent *t;	/* used by the rotate macros */
#endif /* !lint */
    register int ff;
#ifdef lint
    ff = 0;	/* Sun's lint is dumb! */
#endif

    /*
     * Ok, from here on, p is the node we're operating on; pp is it's parent; f
     * is the branch of p from which we have come; ff is the branch of pp which
     * is p.
     */
    for (; (pp = p->v_parent) != 0; p = pp, f = ff) {
	ff = pp->v_right == p;
	if (f ^ d) {		/* right heavy */
	    switch (p->v_bal) {
	    case -1:		/* was left heavy */
		p->v_bal = 0;
		break;
	    case 0:		/* was balanced */
		p->v_bal = 1;
		break;
	    case 1:		/* was already right heavy */
		switch (p->v_right->v_bal) {
		case 1:	/* sigle rotate */
		    pp->v_link[ff] = rleft(p);
		    p->v_left->v_bal = 0;
		    p->v_bal = 0;
		    break;
		case 0:	/* single rotate */
		    pp->v_link[ff] = rleft(p);
		    p->v_left->v_bal = 1;
		    p->v_bal = -1;
		    break;
		case -1:	/* double rotate */
		    (void) rright(p->v_right);
		    pp->v_link[ff] = rleft(p);
		    p->v_left->v_bal =
			p->v_bal < 1 ? 0 : -1;
		    p->v_right->v_bal =
			p->v_bal > -1 ? 0 : 1;
		    p->v_bal = 0;
		    break;
		default:
		    break;
		}
		break;
	    default:
		break;
	    }
	}
	else {			/* left heavy */
	    switch (p->v_bal) {
	    case 1:		/* was right heavy */
		p->v_bal = 0;
		break;
	    case 0:		/* was balanced */
		p->v_bal = -1;
		break;
	    case -1:		/* was already left heavy */
		switch (p->v_left->v_bal) {
		case -1:	/* single rotate */
		    pp->v_link[ff] = rright(p);
		    p->v_right->v_bal = 0;
		    p->v_bal = 0;
		    break;
		case 0:	/* signle rotate */
		    pp->v_link[ff] = rright(p);
		    p->v_right->v_bal = -1;
		    p->v_bal = 1;
		    break;
		case 1:	/* double rotate */
		    (void) rleft(p->v_left);
		    pp->v_link[ff] = rright(p);
		    p->v_left->v_bal =
			p->v_bal < 1 ? 0 : -1;
		    p->v_right->v_bal =
			p->v_bal > -1 ? 0 : 1;
		    p->v_bal = 0;
		    break;
		default:
		    break;
		}
		break;
	    default:
		break;
	    }
	}
	/*
	 * If from insert, then we terminate when p is balanced. If from
	 * delete, then we terminate when p is unbalanced.
	 */
	if ((p->v_bal == 0) ^ d)
	    break;
    }
}

void
plist(p, what)
    register struct varent *p;
    int what;
{
    register struct varent *c;
    register int len;

    if (setintr)
#ifdef BSDSIGS
	(void) sigsetmask(sigblock((sigmask_t) 0) & ~sigmask(SIGINT));
#else /* !BSDSIGS */
	(void) sigrelse(SIGINT);
#endif /* BSDSIGS */

    for (;;) {
	while (p->v_left)
	    p = p->v_left;
x:
	if (p->v_parent == 0)	/* is it the header? */
	    return;
	if ((p->v_flags & what) != 0) {
	    len = blklen(p->vec);
	    xprintf("%S\t", p->v_name);
	    if (len != 1)
		xputchar('(');
	    blkpr(p->vec);
	    if (len != 1)
		xputchar(')');
	    xputchar('\n');
	}
	if (p->v_right) {
	    p = p->v_right;
	    continue;
	}
	do {
	    c = p;
	    p = p->v_parent;
	} while (p->v_right == c);
	goto x;
    }
}

#if defined(KANJI) && defined(SHORT_STRINGS) && defined(DSPMBYTE)
bool dspmbyte_ls;

void
update_dspmbyte_vars()
{
    int lp, iskcode;
    Char *dstr1;
    struct varent *vp;
    
    /* if variable "nokanji" is set, multi-byte display is disabled */
    if ((vp = adrof(CHECK_MBYTEVAR)) && !adrof(STRnokanji)) {
	_enable_mbdisp = 1;
	dstr1 = vp->vec[0];
	if(eq (dstr1, STRKSJIS))
	    iskcode = 1;
	else if (eq(dstr1, STRKEUC))
	    iskcode = 2;
	else if (eq(dstr1, STRKBIG5))
	    iskcode = 3;
	else if ((dstr1[0] - '0') >= 0 && (dstr1[0] - '0') <= 3) {
	    iskcode = 0;
	}
	else {
	    xprintf(CGETS(18, 2,
	       "Warning: unknown multibyte display; using default(euc(JP))\n"));
	    iskcode = 2;
	}
	if (dstr1 && vp->vec[1] && eq(vp->vec[1], STRls))
	  dspmbyte_ls = 1;
	else
	  dspmbyte_ls = 0;
	for (lp = 0; lp < 256 && iskcode > 0; lp++) {
	    switch (iskcode) {
	    case 1:
		/* Shift-JIS */
		_cmap[lp] = _cmap_mbyte[lp];
		_mbmap[lp] = _mbmap_sjis[lp];
		break;
	    case 2:
		/* 2 ... euc */
		_cmap[lp] = _cmap_mbyte[lp];
		_mbmap[lp] = _mbmap_euc[lp];
		break;
	    case 3:
		/* 3 ... big5 */
		_cmap[lp] = _cmap_mbyte[lp];
		_mbmap[lp] = _mbmap_big5[lp];
		break;
	    default:
		xprintf(CGETS(18, 3,
		    "Warning: unknown multibyte code %d; multibyte disabled\n"),
		    iskcode);
		_cmap[lp] = _cmap_c[lp];
		_mbmap[lp] = 0;	/* Default map all 0 */
		_enable_mbdisp = 0;
		break;
	    }
	}
	if (iskcode == 0) {
	    /* check original table */
	    if (Strlen(dstr1) != 256) {
		xprintf(CGETS(18, 4,
       "Warning: Invalid multibyte table length (%d); multibyte disabled\n"),
		    Strlen(dstr1));
		_enable_mbdisp = 0;
	    }
	    for (lp = 0; lp < 256 && _enable_mbdisp == 1; lp++) {
		if (!((dstr1[lp] - '0') >= 0 && (dstr1[lp] - '0') <= 3)) {
		    xprintf(CGETS(18, 4,
	   "Warning: bad multibyte code at offset +%d; multibyte diabled\n"),
			lp);
		    _enable_mbdisp = 0;
		    break;
		}
	    }
	    /* set original table */
	    for (lp = 0; lp < 256; lp++) {
		if (_enable_mbdisp == 1) {
		    _cmap[lp] = _cmap_mbyte[lp];
		    _mbmap[lp] = (unsigned short) ((dstr1[lp] - '0') & 0x0f);
		}
		else {
		    _cmap[lp] = _cmap_c[lp];
		    _mbmap[lp] = 0;	/* Default map all 0 */
		}
	    }
	}
    }
    else {
	for (lp = 0; lp < 256; lp++) {
	    _cmap[lp] = _cmap_c[lp];
	    _mbmap[lp] = 0;	/* Default map all 0 */
	}
	_enable_mbdisp = 0;
	dspmbyte_ls = 0;
    }
#ifdef MBYTEDEBUG	/* Sorry, use for beta testing */
    {
	Char mbmapstr[300];
	for (lp = 0; lp < 256; lp++) {
	    mbmapstr[lp] = _mbmap[lp] + '0';
	    mbmapstr[lp+1] = 0;
	}
	set(STRmbytemap, Strsave(mbmapstr), VAR_READWRITE);
    }
#endif /* MBYTEMAP */
}

/* dspkanji/dspmbyte autosetting */
/* PATCH IDEA FROM Issei.Suzuki VERY THANKS */
void
autoset_dspmbyte(pcp)
    Char *pcp;
{
    int i;
    struct dspm_autoset_Table {
	Char *n;
	Char *v;
    } dspmt[] = {
	{ STRLANGEUCJP, STRKEUC },
	{ STRLANGEUCKR, STRKEUC },
	{ STRLANGEUCZH, STRKEUC },
	{ STRLANGEUCJPB, STRKEUC },
	{ STRLANGEUCKRB, STRKEUC },
	{ STRLANGEUCZHB, STRKEUC },
	{ STRLANGSJIS, STRKSJIS },
	{ STRLANGSJISB, STRKSJIS },
	{ STRLANGBIG5, STRKBIG5 },
	{ NULL, NULL }
    };

    if (*pcp == '\0')
	return;

    for (i = 0; dspmt[i].n; i++) {
	if (eq(pcp, dspmt[i].n)) {
	    set(CHECK_MBYTEVAR, Strsave(dspmt[i].v), VAR_READWRITE);
	    update_dspmbyte_vars();
	    break;
	}
    }
}
#endif
