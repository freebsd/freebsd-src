/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* hack.invent.c - version 1.0.3 */
/* $FreeBSD: src/games/hack/hack.invent.c,v 1.4 1999/11/16 10:26:36 marcel Exp $ */

#include	"hack.h"
#include	<stdio.h>
extern struct obj *splitobj();
extern struct obj zeroobj;
extern char morc;
extern char quitchars[];
static char *xprname();

#ifndef NOWORM
#include	"def.wseg.h"
extern struct wseg *wsegs[32];
#endif NOWORM

#define	NOINVSYM	'#'

static int lastinvnr = 51;	/* 0 ... 51 */
static
assigninvlet(otmp)
struct obj *otmp;
{
	boolean inuse[52];
	int i;
	struct obj *obj;

	for(i = 0; i < 52; i++) inuse[i] = FALSE;
	for(obj = invent; obj; obj = obj->nobj) if(obj != otmp) {
		i = obj->invlet;
		if('a' <= i && i <= 'z') inuse[i - 'a'] = TRUE; else
		if('A' <= i && i <= 'Z') inuse[i - 'A' + 26] = TRUE;
		if(i == otmp->invlet) otmp->invlet = 0;
	}
	if((i = otmp->invlet) &&
	    (('a' <= i && i <= 'z') || ('A' <= i && i <= 'Z')))
		return;
	for(i = lastinvnr+1; i != lastinvnr; i++) {
		if(i == 52) { i = -1; continue; }
		if(!inuse[i]) break;
	}
	otmp->invlet = (inuse[i] ? NOINVSYM :
			(i < 26) ? ('a'+i) : ('A'+i-26));
	lastinvnr = i;
}

struct obj *
addinv(obj)
struct obj *obj;
{
	struct obj *otmp;

	/* merge or attach to end of chain */
	if(!invent) {
		invent = obj;
		otmp = 0;
	} else
	for(otmp = invent; /* otmp */; otmp = otmp->nobj) {
		if(merged(otmp, obj, 0))
			return(otmp);
		if(!otmp->nobj) {
			otmp->nobj = obj;
			break;
		}
	}
	obj->nobj = 0;

	if(flags.invlet_constant) {
		assigninvlet(obj);
		/*
		 * The ordering of the chain is nowhere significant
		 * so in case you prefer some other order than the
		 * historical one, change the code below.
		 */
		if(otmp) {	/* find proper place in chain */
			otmp->nobj = 0;
			if((invent->invlet ^ 040) > (obj->invlet ^ 040)) {
				obj->nobj = invent;
				invent = obj;
			} else
			for(otmp = invent; ; otmp = otmp->nobj) {
			    if(!otmp->nobj ||
				(otmp->nobj->invlet ^ 040) > (obj->invlet ^ 040)){
				obj->nobj = otmp->nobj;
				otmp->nobj = obj;
				break;
			    }
			}
		}
	}

	return(obj);
}

useup(obj)
struct obj *obj;
{
	if(obj->quan > 1){
		obj->quan--;
		obj->owt = weight(obj);
	} else {
		setnotworn(obj);
		freeinv(obj);
		obfree(obj, (struct obj *) 0);
	}
}

freeinv(obj)
struct obj *obj;
{
	struct obj *otmp;

	if(obj == invent)
		invent = invent->nobj;
	else {
		for(otmp = invent; otmp->nobj != obj; otmp = otmp->nobj)
			if(!otmp->nobj) panic("freeinv");
		otmp->nobj = obj->nobj;
	}
}

/* destroy object in fobj chain (if unpaid, it remains on the bill) */
delobj(obj) struct obj *obj; {
	freeobj(obj);
	unpobj(obj);
	obfree(obj, (struct obj *) 0);
}

/* unlink obj from chain starting with fobj */
freeobj(obj) struct obj *obj; {
	struct obj *otmp;

	if(obj == fobj) fobj = fobj->nobj;
	else {
		for(otmp = fobj; otmp->nobj != obj; otmp = otmp->nobj)
			if(!otmp) panic("error in freeobj");
		otmp->nobj = obj->nobj;
	}
}

/* Note: freegold throws away its argument! */
freegold(gold) struct gold *gold; {
	struct gold *gtmp;

	if(gold == fgold) fgold = gold->ngold;
	else {
		for(gtmp = fgold; gtmp->ngold != gold; gtmp = gtmp->ngold)
			if(!gtmp) panic("error in freegold");
		gtmp->ngold = gold->ngold;
	}
	free((char *) gold);
}

deltrap(trap)
struct trap *trap;
{
	struct trap *ttmp;

	if(trap == ftrap)
		ftrap = ftrap->ntrap;
	else {
		for(ttmp = ftrap; ttmp->ntrap != trap; ttmp = ttmp->ntrap) ;
		ttmp->ntrap = trap->ntrap;
	}
	free((char *) trap);
}

struct wseg *m_atseg;

struct monst *
m_at(x,y)
int x,y;
{
	struct monst *mtmp;
#ifndef NOWORM
	struct wseg *wtmp;
#endif NOWORM

	m_atseg = 0;
	for(mtmp = fmon; mtmp; mtmp = mtmp->nmon){
		if(mtmp->mx == x && mtmp->my == y)
			return(mtmp);
#ifndef NOWORM
		if(mtmp->wormno){
		    for(wtmp = wsegs[mtmp->wormno]; wtmp; wtmp = wtmp->nseg)
		    if(wtmp->wx == x && wtmp->wy == y){
			m_atseg = wtmp;
			return(mtmp);
		    }
		}
#endif NOWORM
	}
	return(0);
}

struct obj *
o_at(x,y)
int x,y;
{
	struct obj *otmp;

	for(otmp = fobj; otmp; otmp = otmp->nobj)
		if(otmp->ox == x && otmp->oy == y) return(otmp);
	return(0);
}

struct obj *
sobj_at(n,x,y)
int n,x,y;
{
	struct obj *otmp;

	for(otmp = fobj; otmp; otmp = otmp->nobj)
		if(otmp->ox == x && otmp->oy == y && otmp->otyp == n)
			return(otmp);
	return(0);
}

carried(obj) struct obj *obj; {
struct obj *otmp;
	for(otmp = invent; otmp; otmp = otmp->nobj)
		if(otmp == obj) return(1);
	return(0);
}

carrying(type)
int type;
{
	struct obj *otmp;

	for(otmp = invent; otmp; otmp = otmp->nobj)
		if(otmp->otyp == type)
			return(TRUE);
	return(FALSE);
}

struct obj *
o_on(id, objchn) unsigned int id; struct obj *objchn; {
	while(objchn) {
		if(objchn->o_id == id) return(objchn);
		objchn = objchn->nobj;
	}
	return((struct obj *) 0);
}

struct trap *
t_at(x,y)
int x,y;
{
	struct trap *trap = ftrap;
	while(trap) {
		if(trap->tx == x && trap->ty == y) return(trap);
		trap = trap->ntrap;
	}
	return(0);
}

struct gold *
g_at(x,y)
int x,y;
{
	struct gold *gold = fgold;
	while(gold) {
		if(gold->gx == x && gold->gy == y) return(gold);
		gold = gold->ngold;
	}
	return(0);
}

/* make dummy object structure containing gold - for temporary use only */
struct obj *
mkgoldobj(q)
long q;
{
	struct obj *otmp;

	otmp = newobj(0);
	/* should set o_id etc. but otmp will be freed soon */
	otmp->olet = '$';
	u.ugold -= q;
	OGOLD(otmp) = q;
	flags.botl = 1;
	return(otmp);
}

/*
 * getobj returns:
 *	struct obj *xxx:	object to do something with.
 *	(struct obj *) 0	error return: no object.
 *	&zeroobj		explicitly no object (as in w-).
 */
struct obj *
getobj(let,word)
char *let,*word;
{
	struct obj *otmp;
	char ilet,ilet1,ilet2;
	char buf[BUFSZ];
	char lets[BUFSZ];
	int foo = 0, foo2;
	char *bp = buf;
	xchar allowcnt = 0;	/* 0, 1 or 2 */
	boolean allowgold = FALSE;
	boolean allowall = FALSE;
	boolean allownone = FALSE;
	xchar foox = 0;
	long cnt;

	if(*let == '0') let++, allowcnt = 1;
	if(*let == '$') let++, allowgold = TRUE;
	if(*let == '#') let++, allowall = TRUE;
	if(*let == '-') let++, allownone = TRUE;
	if(allownone) *bp++ = '-';
	if(allowgold) *bp++ = '$';
	if(bp > buf && bp[-1] == '-') *bp++ = ' ';

	ilet = 'a';
	for(otmp = invent; otmp; otmp = otmp->nobj){
	    if(!*let || index(let, otmp->olet)) {
		bp[foo++] = flags.invlet_constant ? otmp->invlet : ilet;

		/* ugly check: remove inappropriate things */
		if((!strcmp(word, "take off") &&
		    !(otmp->owornmask & (W_ARMOR - W_ARM2)))
		|| (!strcmp(word, "wear") &&
		    (otmp->owornmask & (W_ARMOR | W_RING)))
		|| (!strcmp(word, "wield") &&
		    (otmp->owornmask & W_WEP))) {
			foo--;
			foox++;
		}
	    }
	    if(ilet == 'z') ilet = 'A'; else ilet++;
	}
	bp[foo] = 0;
	if(foo == 0 && bp > buf && bp[-1] == ' ') *--bp = 0;
	(void) strcpy(lets, bp);	/* necessary since we destroy buf */
	if(foo > 5) {			/* compactify string */
		foo = foo2 = 1;
		ilet2 = bp[0];
		ilet1 = bp[1];
		while(ilet = bp[++foo2] = bp[++foo]){
			if(ilet == ilet1+1){
				if(ilet1 == ilet2+1)
					bp[foo2 - 1] = ilet1 = '-';
				else if(ilet2 == '-') {
					bp[--foo2] = ++ilet1;
					continue;
				}
			}
			ilet2 = ilet1;
			ilet1 = ilet;
		}
	}
	if(!foo && !allowall && !allowgold && !allownone) {
		pline("You don't have anything %sto %s.",
			foox ? "else " : "", word);
		return(0);
	}
	for(;;) {
		if(!buf[0])
			pline("What do you want to %s [*]? ", word);
		else
			pline("What do you want to %s [%s or ?*]? ",
				word, buf);

		cnt = 0;
		ilet = readchar();
		while(digit(ilet) && allowcnt) {
			if (cnt < 100000000)
			    cnt = 10*cnt + (ilet - '0');
			else
			    cnt = 999999999;
			allowcnt = 2;	/* signal presence of cnt */
			ilet = readchar();
		}
		if(digit(ilet)) {
			pline("No count allowed with this command.");
			continue;
		}
		if(index(quitchars,ilet))
			return((struct obj *)0);
		if(ilet == '-') {
			return(allownone ? &zeroobj : (struct obj *) 0);
		}
		if(ilet == '$') {
			if(!allowgold){
				pline("You cannot %s gold.", word);
				continue;
			}
			if(!(allowcnt == 2 && cnt < u.ugold))
				cnt = u.ugold;
			return(mkgoldobj(cnt));
		}
		if(ilet == '?') {
			doinv(lets);
			if(!(ilet = morc)) continue;
			/* he typed a letter (not a space) to more() */
		} else if(ilet == '*') {
			doinv((char *) 0);
			if(!(ilet = morc)) continue;
			/* ... */
		}
		if(flags.invlet_constant) {
			for(otmp = invent; otmp; otmp = otmp->nobj)
				if(otmp->invlet == ilet) break;
		} else {
			if(ilet >= 'A' && ilet <= 'Z') ilet += 'z'-'A'+1;
			ilet -= 'a';
			for(otmp = invent; otmp && ilet;
					ilet--, otmp = otmp->nobj) ;
		}
		if(!otmp) {
			pline("You don't have that object.");
			continue;
		}
		if(cnt < 0 || otmp->quan < cnt) {
			pline("You don't have that many! [You have %u]"
			, otmp->quan);
			continue;
		}
		break;
	}
	if(!allowall && let && !index(let,otmp->olet)) {
		pline("That is a silly thing to %s.",word);
		return(0);
	}
	if(allowcnt == 2) {	/* cnt given */
		if(cnt == 0) return(0);
		if(cnt != otmp->quan) {
			struct obj *obj;
			obj = splitobj(otmp, (int) cnt);
			if(otmp == uwep) setuwep(obj);
		}
	}
	return(otmp);
}

ckunpaid(otmp) struct obj *otmp; {
	return( otmp->unpaid );
}

/* interactive version of getobj - used for Drop and Identify */
/* return the number of times fn was called successfully */
ggetobj(word, fn, max)
char *word;
int (*fn)(),  max;
{
char buf[BUFSZ];
char *ip;
char sym;
int oletct = 0, iletct = 0;
boolean allflag = FALSE;
char olets[20], ilets[20];
int (*ckfn)() = (int (*)()) 0;
xchar allowgold = (u.ugold && !strcmp(word, "drop")) ? 1 : 0;	/* BAH */
	if(!invent && !allowgold){
		pline("You have nothing to %s.", word);
		return(0);
	} else {
		struct obj *otmp = invent;
		int uflg = 0;

		if(allowgold) ilets[iletct++] = '$';
		ilets[iletct] = 0;
		while(otmp) {
			if(!index(ilets, otmp->olet)){
				ilets[iletct++] = otmp->olet;
				ilets[iletct] = 0;
			}
			if(otmp->unpaid) uflg = 1;
			otmp = otmp->nobj;
		}
		ilets[iletct++] = ' ';
		if(uflg) ilets[iletct++] = 'u';
		if(invent) ilets[iletct++] = 'a';
		ilets[iletct] = 0;
	}
	pline("What kinds of thing do you want to %s? [%s] ",
		word, ilets);
	getlin(buf);
	if(buf[0] == '\033') {
		clrlin();
		return(0);
	}
	ip = buf;
	olets[0] = 0;
	while(sym = *ip++){
		if(sym == ' ') continue;
		if(sym == '$') {
			if(allowgold == 1)
				(*fn)(mkgoldobj(u.ugold));
			else if(!u.ugold)
				pline("You have no gold.");
			allowgold = 2;
		} else
		if(sym == 'a' || sym == 'A') allflag = TRUE; else
		if(sym == 'u' || sym == 'U') ckfn = ckunpaid; else
		if(index("!%?[()=*/\"0", sym)){
			if(!index(olets, sym)){
				olets[oletct++] = sym;
				olets[oletct] = 0;
			}
		}
		else pline("You don't have any %c's.", sym);
	}
	if(allowgold == 2 && !oletct)
		return(1);	/* he dropped gold (or at least tried to) */
	else
		return(askchain(invent, olets, allflag, fn, ckfn, max));
}

/*
 * Walk through the chain starting at objchn and ask for all objects
 * with olet in olets (if nonNULL) and satisfying ckfn (if nonNULL)
 * whether the action in question (i.e., fn) has to be performed.
 * If allflag then no questions are asked. Max gives the max nr of
 * objects to be treated. Return the number of objects treated.
 */
askchain(objchn, olets, allflag, fn, ckfn, max)
struct obj *objchn;
char *olets;
int allflag;
int (*fn)(), (*ckfn)();
int max;
{
struct obj *otmp, *otmp2;
char sym, ilet;
int cnt = 0;
	ilet = 'a'-1;
	for(otmp = objchn; otmp; otmp = otmp2){
		if(ilet == 'z') ilet = 'A'; else ilet++;
		otmp2 = otmp->nobj;
		if(olets && *olets && !index(olets, otmp->olet)) continue;
		if(ckfn && !(*ckfn)(otmp)) continue;
		if(!allflag) {
			pline(xprname(otmp, ilet));
			addtopl(" [nyaq]? ");
			sym = readchar();
		}
		else	sym = 'y';

		switch(sym){
		case 'a':
			allflag = 1;
		case 'y':
			cnt += (*fn)(otmp);
			if(--max == 0) goto ret;
		case 'n':
		default:
			break;
		case 'q':
			goto ret;
		}
	}
	pline(cnt ? "That was all." : "No applicable objects.");
ret:
	return(cnt);
}

obj_to_let(obj)	/* should of course only be called for things in invent */
struct obj *obj;
{
	struct obj *otmp;
	char ilet;

	if(flags.invlet_constant)
		return(obj->invlet);
	ilet = 'a';
	for(otmp = invent; otmp && otmp != obj; otmp = otmp->nobj)
		if(++ilet > 'z') ilet = 'A';
	return(otmp ? ilet : NOINVSYM);
}

prinv(obj)
struct obj *obj;
{
	pline(xprname(obj, obj_to_let(obj)));
}

static char *
xprname(obj,let)
struct obj *obj;
char let;
{
	static char li[BUFSZ];

	(void) sprintf(li, "%c - %s.",
		flags.invlet_constant ? obj->invlet : let,
		doname(obj));
	return(li);
}

ddoinv()
{
	doinv((char *) 0);
	return(0);
}

/* called with 0 or "": all objects in inventory */
/* otherwise: all objects with (serial) letter in lets */
doinv(lets)
char *lets;
{
	struct obj *otmp;
	char ilet;
	int ct = 0;
	char any[BUFSZ];

	morc = 0;		/* just to be sure */

	if(!invent){
		pline("Not carrying anything.");
		return;
	}

	cornline(0, (char *) 0);
	ilet = 'a';
	for(otmp = invent; otmp; otmp = otmp->nobj) {
	    if(flags.invlet_constant) ilet = otmp->invlet;
	    if(!lets || !*lets || index(lets, ilet)) {
		    cornline(1, xprname(otmp, ilet));
		    any[ct++] = ilet;
	    }
	    if(!flags.invlet_constant) if(++ilet > 'z') ilet = 'A';
	}
	any[ct] = 0;
	cornline(2, any);
}

dotypeinv ()				/* free after Robert Viduya */
/* Changed to one type only, so he doesnt have to type cr */
{
    char c, ilet;
    char stuff[BUFSZ];
    int stct;
    struct obj *otmp;
    boolean billx = inshop() && doinvbill(0);
    boolean unpd = FALSE;

	if (!invent && !u.ugold && !billx) {
	    pline ("You aren't carrying anything.");
	    return(0);
	}

	stct = 0;
	if(u.ugold) stuff[stct++] = '$';
	stuff[stct] = 0;
	for(otmp = invent; otmp; otmp = otmp->nobj) {
	    if (!index (stuff, otmp->olet)) {
		stuff[stct++] = otmp->olet;
		stuff[stct] = 0;
	    }
	    if(otmp->unpaid)
		unpd = TRUE;
	}
	if(unpd) stuff[stct++] = 'u';
	if(billx) stuff[stct++] = 'x';
	stuff[stct] = 0;

	if(stct > 1) {
	    pline ("What type of object [%s] do you want an inventory of? ",
		stuff);
	    c = readchar();
	    if(index(quitchars,c)) return(0);
	} else
	    c = stuff[0];

	if(c == '$')
	    return(doprgold());

	if(c == 'x' || c == 'X') {
	    if(billx)
		(void) doinvbill(1);
	    else
		pline("No used-up objects on the shopping bill.");
	    return(0);
	}

	if((c == 'u' || c == 'U') && !unpd) {
		pline("You are not carrying any unpaid objects.");
		return(0);
	}

	stct = 0;
	ilet = 'a';
	for (otmp = invent; otmp; otmp = otmp -> nobj) {
	    if(flags.invlet_constant) ilet = otmp->invlet;
	    if (c == otmp -> olet || (c == 'u' && otmp -> unpaid))
		stuff[stct++] = ilet;
	    if(!flags.invlet_constant) if(++ilet > 'z') ilet = 'A';
	}
	stuff[stct] = '\0';
	if(stct == 0)
		pline("You have no such objects.");
	else
		doinv (stuff);

	return(0);
}

/* look at what is here */
dolook() {
    struct obj *otmp, *otmp0;
    struct gold *gold;
    char *verb = Blind ? "feel" : "see";
    int	ct = 0;

    if(!u.uswallow) {
	if(Blind) {
	    pline("You try to feel what is lying here on the floor.");
	    if(Levitation) {				/* ab@unido */
		pline("You cannot reach the floor!");
		return(1);
	    }
	}
	otmp0 = o_at(u.ux, u.uy);
	gold = g_at(u.ux, u.uy);
    }

    if(u.uswallow || (!otmp0 && !gold)) {
	pline("You %s no objects here.", verb);
	return(!!Blind);
    }

    cornline(0, "Things that are here:");
    for(otmp = otmp0; otmp; otmp = otmp->nobj) {
	if(otmp->ox == u.ux && otmp->oy == u.uy) {
	    ct++;
	    cornline(1, doname(otmp));
	    if(Blind && otmp->otyp == DEAD_COCKATRICE && !uarmg) {
		pline("Touching the dead cockatrice is a fatal mistake ...");
		pline("You die ...");
		killer = "dead cockatrice";
		done("died");
	    }
	}
    }

    if(gold) {
	char gbuf[30];

	(void) sprintf(gbuf, "%ld gold piece%s",
		gold->amount, plur(gold->amount));
	if(!ct++)
	    pline("You %s here %s.", verb, gbuf);
	else
	    cornline(1, gbuf);
    }

    if(ct == 1 && !gold) {
	pline("You %s here %s.", verb, doname(otmp0));
	cornline(3, (char *) 0);
    }
    if(ct > 1)
	cornline(2, (char *) 0);
    return(!!Blind);
}

stackobj(obj) struct obj *obj; {
struct obj *otmp = fobj;
	for(otmp = fobj; otmp; otmp = otmp->nobj) if(otmp != obj)
	if(otmp->ox == obj->ox && otmp->oy == obj->oy &&
		merged(obj,otmp,1))
			return;
}

/* merge obj with otmp and delete obj if types agree */
merged(otmp,obj,lose) struct obj *otmp, *obj; {
	if(obj->otyp == otmp->otyp &&
	  obj->unpaid == otmp->unpaid &&
	  obj->spe == otmp->spe &&
	  obj->dknown == otmp->dknown &&
	  obj->cursed == otmp->cursed &&
	  (index("%*?!", obj->olet) ||
	    (obj->known == otmp->known &&
		(obj->olet == WEAPON_SYM && obj->otyp < BOOMERANG)))) {
		otmp->quan += obj->quan;
		otmp->owt += obj->owt;
		if(lose) freeobj(obj);
		obfree(obj,otmp);	/* free(obj), bill->otmp */
		return(1);
	} else	return(0);
}

/*
 * Gold is no longer displayed; in fact, when you have a lot of money,
 * it may take a while before you have counted it all.
 * [Bug: d$ and pickup still tell you how much it was.]
 */
extern int (*occupation)();
extern char *occtxt;
static long goldcounted;

countgold(){
	if((goldcounted += 100*(u.ulevel + 1)) >= u.ugold) {
		long eps = 0;
		if(!rn2(2)) eps = rnd((int) (u.ugold/100 + 1));
		pline("You probably have about %ld gold pieces.",
			u.ugold + eps);
		return(0);	/* done */
	}
	return(1);		/* continue */
}

doprgold(){
	if(!u.ugold)
		pline("You do not carry any gold.");
	else if(u.ugold <= 500)
		pline("You are carrying %ld gold pieces.", u.ugold);
	else {
		pline("You sit down in order to count your gold pieces.");
		goldcounted = 500;
		occupation = countgold;
		occtxt = "counting your gold";
	}
	return(1);
}

/* --- end of gold counting section --- */

doprwep(){
	if(!uwep) pline("You are empty handed.");
	else prinv(uwep);
	return(0);
}

doprarm(){
	if(!uarm && !uarmg && !uarms && !uarmh)
		pline("You are not wearing any armor.");
	else {
		char lets[6];
		int ct = 0;

		if(uarm) lets[ct++] = obj_to_let(uarm);
		if(uarm2) lets[ct++] = obj_to_let(uarm2);
		if(uarmh) lets[ct++] = obj_to_let(uarmh);
		if(uarms) lets[ct++] = obj_to_let(uarms);
		if(uarmg) lets[ct++] = obj_to_let(uarmg);
		lets[ct] = 0;
		doinv(lets);
	}
	return(0);
}

doprring(){
	if(!uleft && !uright)
		pline("You are not wearing any rings.");
	else {
		char lets[3];
		int ct = 0;

		if(uleft) lets[ct++] = obj_to_let(uleft);
		if(uright) lets[ct++] = obj_to_let(uright);
		lets[ct] = 0;
		doinv(lets);
	}
	return(0);
}

digit(c) char c; {
	return(c >= '0' && c <= '9');
}
