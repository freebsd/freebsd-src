/*-
 * Copyright (c) 1980, 1991, 1993
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
#if 0
static char sccsid[] = "@(#)set.c	8.1 (Berkeley) 5/31/93";
#else
static const char rcsid[] =
	"$Id: set.c,v 1.9 1998/05/06 06:51:04 charnier Exp $";
#endif
#endif /* not lint */

#include <sys/types.h>
#if __STDC__
# include <stdarg.h>
#else
# include <varargs.h>
#endif
#include <unistd.h>

#include "csh.h"
#include "extern.h"

static Char	*getinx __P((Char *, int *));
static void	 asx __P((Char *, int, Char *));
static struct varent
		*getvx __P((Char *, int));
static Char	*xset __P((Char *, Char ***));
static Char	*operate __P((int, Char *, Char *));
static void	 putn1 __P((int));
static struct varent
		*madrof __P((Char *, struct varent *));
static void	 unsetv1 __P((struct varent *));
static void	 exportpath __P((Char **));
static void	 balance __P((struct varent *, int, int));


/*
 * C Shell
 */

void
/*ARGSUSED*/
doset(v, t)
    Char **v;
    struct command *t;
{
    Char *p;
    Char   *vp, op;
    Char  **vecp;
    bool    hadsub;
    int     subscr;

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
	if ((p - vp) > MAXVARLEN) {
	    stderror(ERR_NAME | ERR_VARTOOLONG);
	    return;
	}
	if (*p == '[') {
	    hadsub++;
	    p = getinx(p, &subscr);
	}
	if ((op = *p) != '\0') {
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
	    Char **e = v;

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
	    set1(vp, vecp, &shvhed);
	    *e = p;
	    v = e + 1;
	}
	else if (hadsub)
	    asx(vp, subscr, Strsave(p));
	else
	    set(vp, Strsave(p));
	if (eq(vp, STRpath)) {
	    exportpath(adrof(STRpath)->vec);
	    dohash(NULL, NULL);
	}
	else if (eq(vp, STRhistchars)) {
	    Char *pn = value(STRhistchars);

	    HIST = *pn++;
	    HISTSUB = *pn;
	}
	else if (eq(vp, STRuser)) {
	    Setenv(STRUSER, value(vp));
	    Setenv(STRLOGNAME, value(vp));
	}
	else if (eq(vp, STRwordchars)) {
	    word_chars = value(vp);
	}
	else if (eq(vp, STRterm))
	    Setenv(STRTERM, value(vp));
	else if (eq(vp, STRhome)) {
	    Char *cp;

	    cp = Strsave(value(vp));	/* get the old value back */

	    /*
	     * convert to cononical pathname (possibly resolving symlinks)
	     */
	    cp = dcanon(cp, cp);

	    set(vp, Strsave(cp));	/* have to save the new val */

	    /* and now mirror home with HOME */
	    Setenv(STRHOME, cp);
	    /* fix directory stack for new tilde home */
	    dtilde();
	    xfree((ptr_t) cp);
	}
#ifdef FILEC
	else if (eq(vp, STRfilec))
	    filec = 1;
#endif
    } while ((p = *v++) != NULL);
}

static Char *
getinx(cp, ip)
    Char *cp;
    int *ip;
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
    struct varent *v = getvx(vp, subscr);

    xfree((ptr_t) v->vec[subscr - 1]);
    v->vec[subscr - 1] = globone(p, G_APPEND);
}

static struct varent *
getvx(vp, subscr)
    Char   *vp;
    int     subscr;
{
    struct varent *v = adrof(vp);

    if (v == 0)
	udvar(vp);
    if (subscr < 1 || subscr > blklen(v->vec))
	stderror(ERR_NAME | ERR_RANGE);
    return (v);
}

void
/*ARGSUSED*/
dolet(v, t)
    Char **v;
    struct command *t;
{
    Char *p;
    Char   *vp, c, op;
    bool    hadsub;
    int     subscr;

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
	if ((op = *p) != '\0')
	    *p++ = 0;
	else
	    stderror(ERR_NAME | ERR_ASSIGN);

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
	if (op == '=')
	    if (hadsub)
		asx(vp, subscr, p);
	    else
		set(vp, p);
	else if (hadsub) {
	    struct varent *gv = getvx(vp, subscr);

	    asx(vp, subscr, operate(op, gv->vec[subscr - 1], p));
	}
	else
	    set(vp, operate(op, value(vp), p));
	if (eq(vp, STRpath)) {
	    exportpath(adrof(STRpath)->vec);
	    dohash(NULL, NULL);
	}
	xfree((ptr_t) vp);
	if (c != '=')
	    xfree((ptr_t) p);
    } while ((p = *v++) != NULL);
}

static Char *
xset(cp, vp)
    Char   *cp, ***vp;
{
    Char *dp;

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
    int    op;
    Char  *vp, *p;
{
    Char    opr[2];
    Char   *vec[5];
    Char **v = vec;
    Char  **vecp = v;
    int i;

    if (op != '=') {
	if (*vp)
	    *v++ = vp;
	opr[0] = op;
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

static Char *putp;

Char   *
putn(n)
    int n;
{
    int     num;
    static Char number[15];

    putp = number;
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
#else
    }
    else {
	num = 4;		/* confuse lint */
	if (sizeof(int) == num && ((unsigned int) n) == 0x80000000) {
	    *putp++ = '2';
	    n = 147483648;
	}
    }
#endif
    putn1(n);
    *putp = 0;
    return (Strsave(number));
}

static void
putn1(n)
    int n;
{
    if (n > 9)
	putn1(n / 10);
    *putp++ = n % 10 + '0';
}

int
getn(cp)
    Char *cp;
{
    int n;
    int     sign;

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
    struct varent *vp;

    vp = adrof1(var, head);
    return (vp == 0 || vp->vec[0] == 0 ? STRNULL : vp->vec[0]);
}

static struct varent *
madrof(pat, vp)
    Char   *pat;
    struct varent *vp;
{
    struct varent *vp1;

    for (; vp; vp = vp->v_right) {
	if (vp->v_left && (vp1 = madrof(pat, vp->v_left)))
	    return vp1;
	if (Gmatch(vp->v_name, pat))
	    return vp;
    }
    return vp;
}

struct varent *
adrof1(name, v)
    Char *name;
    struct varent *v;
{
    int cmp;

    v = v->v_left;
    while (v && ((cmp = *name - *v->v_name) ||
		 (cmp = Strcmp(name, v->v_name))))
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
set(var, val)
    Char   *var, *val;
{
    Char **vec = (Char **) xmalloc((size_t) (2 * sizeof(Char **)));

    vec[0] = val;
    vec[1] = 0;
    set1(var, vec, &shvhed);
}

void
set1(var, vec, head)
    Char   *var, **vec;
    struct varent *head;
{
    Char **oldv = vec;

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
    setq(var, vec, head);
}


void
setq(name, vec, p)
    Char   *name, **vec;
    struct varent *p;
{
    struct varent *c;
    int f;

    f = 0;			/* tree hangs off the header's left link */
    while ((c = p->v_link[f]) != NULL) {
	if ((f = *name - *c->v_name) == 0 &&
	    (f = Strcmp(name, c->v_name)) == 0) {
	    blkfree(c->vec);
	    goto found;
	}
	p = c;
	f = f > 0;
    }
    p->v_link[f] = c = (struct varent *) xmalloc((size_t) sizeof(struct varent));
    c->v_name = Strsave(name);
    c->v_bal = 0;
    c->v_left = c->v_right = 0;
    c->v_parent = p;
    balance(p, f, 0);
found:
    trim(c->vec = vec);
}

void
/*ARGSUSED*/
unset(v, t)
    Char **v;
    struct command *t;
{
    unset1(v, &shvhed);
#ifdef FILEC
    if (adrof(STRfilec) == 0)
	filec = 0;
#endif
    if (adrof(STRhistchars) == 0) {
	HIST = '!';
	HISTSUB = '^';
    }
    if (adrof(STRwordchars) == 0)
	word_chars = STR_WORD_CHARS;
}

void
unset1(v, head)
    Char *v[];
    struct varent *head;
{
    struct varent *vp;
    int cnt;

    while (*++v) {
	cnt = 0;
	while ((vp = madrof(*v, head->v_left)) != NULL)
	    unsetv1(vp), cnt++;
	if (cnt == 0)
	    setname(vis_str(*v));
    }
}

void
unsetv(var)
    Char   *var;
{
    struct varent *vp;

    if ((vp = adrof1(var, &shvhed)) == 0)
	udvar(var);
    unsetv1(vp);
}

static void
unsetv1(p)
    struct varent *p;
{
    struct varent *c, *pp;
    int f;

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
	p->vec = c->vec;
	p = c;
	c = p->v_left;
    }
    /*
     * Move c into where p is.
     */
    pp = p->v_parent;
    f = pp->v_right == p;
    if ((pp->v_link[f] = c) != NULL)
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
    set(cp, Strsave(STRNULL));
}

void
/*ARGSUSED*/
shift(v, t)
    Char **v;
    struct command *t;
{
    struct varent *argv;
    Char *name;

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
}

static void
exportpath(val)
    Char  **val;
{
    Char    exppath[BUFSIZ];

    exppath[0] = 0;
    if (val)
	while (*val) {
	    if (Strlen(*val) + Strlen(exppath) + 2 > BUFSIZ) {
		(void) fprintf(csherr,
			       "Warning: ridiculously long PATH truncated\n");
		break;
	    }
	    if (**val != '/' && (euid == 0 || uid == 0) &&
		    (intact || (intty && isatty(SHOUT))))
		    (void) fprintf(csherr,
		    "Warning: exported path contains relative components.\n");
	    (void) Strcat(exppath, *val++);
	    if (*val == 0 || eq(*val, STRRparen))
		break;
	    (void) Strcat(exppath, STRcolon);
	}
    Setenv(STRPATH, exppath);
}

#ifndef lint
 /*
  * Lint thinks these have null effect
  */
 /* macros to do single rotations on node p */
#define rright(p) (\
	t = (p)->v_left,\
	(t)->v_parent = (p)->v_parent,\
	((p)->v_left = t->v_right) ? (t->v_right->v_parent = (p)) : 0,\
	(t->v_right = (p))->v_parent = t,\
	(p) = t)
#define rleft(p) (\
	t = (p)->v_right,\
	(t)->v_parent = (p)->v_parent,\
	((p)->v_right = t->v_left) ? (t->v_left->v_parent = (p)) : 0,\
	(t->v_left = (p))->v_parent = t,\
	(p) = t)
#else
struct varent *
rleft(p)
    struct varent *p;
{
    return (p);
}
struct varent *
rright(p)
    struct varent *p;
{
    return (p);
}

#endif				/* ! lint */


/*
 * Rebalance a tree, starting at p and up.
 * F == 0 means we've come from p's left child.
 * D == 1 means we've just done a delete, otherwise an insert.
 */
static void
balance(p, f, d)
    struct varent *p;
    int f, d;
{
    struct varent *pp;

#ifndef lint
    struct varent *t;	/* used by the rotate macros */

#endif
    int ff;

    /*
     * Ok, from here on, p is the node we're operating on; pp is it's parent; f
     * is the branch of p from which we have come; ff is the branch of pp which
     * is p.
     */
    for (; (pp = p->v_parent) != NULL; p = pp, f = ff) {
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
		}
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
		}
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
plist(p)
    struct varent *p;
{
    struct varent *c;
    int len;

    if (setintr)
	(void) sigsetmask(sigblock(0) & ~sigmask(SIGINT));

    for (;;) {
	while (p->v_left)
	    p = p->v_left;
x:
	if (p->v_parent == 0)	/* is it the header? */
	    return;
	len = blklen(p->vec);
	(void) fprintf(cshout, "%s\t", short2str(p->v_name));
	if (len != 1)
	    (void) fputc('(', cshout);
	blkpr(cshout, p->vec);
	if (len != 1)
	    (void) fputc(')', cshout);
	(void) fputc('\n', cshout);
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
