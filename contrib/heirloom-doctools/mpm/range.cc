/*
 * Changes by Gunnar Ritter, Freiburg i. Br., Germany, October 2005.
 *
 * Derived from Plan 9 source code published at the 9fans list by Rob Pike,
 * <http://lists.cse.psu.edu/archives/9fans/2002-February/015773.html>
 *
 * Copyright (C) 2003, Lucent Technologies Inc. and others.
 * All Rights Reserved.
 *
 * Distributed under the terms of the Lucent Public License Version 1.02.
 */

/*	Sccsid @(#)range.cc	1.3 (gritter) 10/30/05	*/
#include	<math.h>
#include	"misc.h"
#include	"slug.h"
#include	"range.h"

void sprange::reheight(int *cv, int *mv)
{
	if (*cv != *mv)
		WARNING("slug %d: an imbedded SP, line %d\n",
			first->serialno(), first->lineno());
	*cv += dv;
	*mv = max(*mv, *cv);
}

void sprange::rerawht(int *cv, int *mv)
{
	*cv += rawht();
	*mv = max(*mv, *cv);
}

void nestrange::restore()
{
	subrange->restoreall();
}

void stream::freeall()	// not a destructor;  called explicitly
{
	strblk *p, *q;
	for (p = first; p; p = q) {
		q = p->next;
		delete p;
	}
	first = last = curr = 0;
}

void stream::dump()
{
	for (stream s = *this; s.more(); s.advance())
		s.current()->dump();
}

void stream::rdump()
{
	for (stream s = *this; s.more(); s.advance())
		s.current()->rdump();
}

int stream::restoreall()
{
	for (stream s = *this; s.more(); s.advance())
		s.current()->restore();
	return measure(this);
}

range *stream::append(range *r)
{
	if (last == 0)
		curr = first = last = new strblk;
	else {
		last->next = new strblk;
		last = last->next;
		if (curr == 0)
			curr = last;
	}
	last->next = 0;
	return last->rp = r;
}

void stream::split()	// duplicate current() range
{
	strblk *s2 = new strblk;
	range *r2 = curr->rp->clone();
	s2->rp = r2;
	s2->next = curr->next;
	if (last == curr)
		last = s2;
	curr->next = s2;
	curr->rp->killkids();		// children only in the 2nd one
	// r2->crosslink(r1);
}

int stream::height()
{
	stream s = *this;
	int h;
	for (h = 0; s.more(); s.advance())
		h += s.current()->height();
	return h;
}

int stream::rawht()
{
	stream s = *this;
	int h;
	for (h = 0; s.more(); s.advance())
		h += s.current()->rawht();
	return h;
}

int measure(stream *sp)		// record high-water mark of stream
{				// sets nested stream heights
	stream s = *sp;
	int curv, maxv;
	for (maxv = curv = 0; s.more(); s.advance())
		s.current()->reheight(&curv, &maxv);
	return maxv;
}

int rawmeasure(stream *sp)
{
	stream s = *sp;
	int curv, maxv;
	for (maxv = curv = 0; s.more(); s.advance())
		s.current()->rerawht(&curv, &maxv);
	return maxv;
}

void nestrange::rdump()
{
	dump();
	if (subrange)
		subrange->rdump();
}

void nestrange::killkids()
{
	subrange = new stream;
}

int nestrange::print(int curv, int col)
{
	int ocurv = curv;
	first->slugout(col);
	for (stream s = *subrange; s.more(); s.advance())
		curv = s.current()->print(curv, col);
	return ocurv + height();
}

#define macroclone(rangetype) range *rangetype::clone() {\
	rangetype *t = new rangetype;\
	*t = *this;\
	return t; }

macroclone(usrange);
macroclone(ufrange);
macroclone(bfrange);

#undef macroclone

#define macropickgoal(rangetype) void rangetype::pickgoal(int acv, double scale) {\
	if (scale > 1) {\
		goalV = (int)(scale*goalV);\
		goal2 = (int)(scale*goal2);\
	}\
	if (abs(acv - goalV) > abs(acv-goal2))\
		goalV = goal2; }

macropickgoal(ufrange)
macropickgoal(bfrange)

#undef macropickgoal

range *generator::next()
{
	range *r;
	if (child) {
		if ((r = child->next()))
			return r;
		delete child;
		child = 0;
	}
	if (!s.more())
		return 0;
	r = s.current();
	if (r->isnested())
		child = new generator(r->children());
	s.advance();
	return r;
}

range *queue::enqueue(range *r)
{
	if (dbg & 8)
		printf("#entering queue::enqueue()\n");
	check("queue::enqueue");
	if (!last || last->rp->serialno() < r->serialno())	// common case
		return append(r);
	if (dbg & 8)
		printf("#queue::enqueue() pushing back\n");
	newguy = new strblk;
	newguy->rp = r;
	if (r->serialno() < first->rp->serialno()) {
		newguy->next = first;
		curr = first = newguy;
		return newguy->rp;
	}
	if (dbg & 8)
		printf("#queue::enqueue() searching down queue\n");
	for (curr = first;
		next() && next()->serialno() < r->serialno();
		curr = curr->next)
		;
	newguy->next = curr->next;
	curr->next = newguy;
	curr = first;			// restore important queue condition
	return newguy->rp;
}

range *queue::dequeue()
{
	if (dbg & 8)
		printf("#entering queue::dequeue()\n");
	check("queue::dequeue");
	curr = first->next;
	range *retval = first->rp;
	delete first;
	first = curr;
	if (!curr)
		last = 0;
	return retval;
}

// ================================================================================

//	functions that munge the troff output stored in slugs[]

// ================================================================================

static void doprefix(FILE *fp) // copy 1st "x" commands to output
{
	int c;

	while ((c = getc(fp)) != EOF) {
		if (c != 'x') {
			ungetc(c, fp);
			break;
		}
		putchar(c);
		do {
			putchar(c = getc(fp));
		} while (c != '\n');
		linenum++;
	}
//	printf("x font 1 R\n");	// horrible kludge: ensure a font for first f1 command 
}

#define	DELTASLUGS	15000

static slug	*slugs = 0;
static int	nslugs = 0;	// slugs has nslugs slots
static slug	*slugp = 0;	// next free slug in slugs

static void readslugs(FILE *fp)
{
	if ((slugs = (slug *) malloc((nslugs = DELTASLUGS)*sizeof(slug))) == NULL)
		FATAL("no room for %d-slug array\n", nslugs);
	slugp = slugs;
	for (slugp = slugs; ; slugp++) {
		if (slugp >= slugs+nslugs-2) {
			int where = slugp - slugs;
			if ((slugs = (slug *) realloc((char *) slugs, (nslugs += DELTASLUGS)*sizeof(slug))) == NULL)
				FATAL("no room for %d slugs\n", nslugs);
			WARNING("now slug array can hold %d slugs\n", nslugs);
			slugp = slugs + where;
		}
		*slugp = getslug(fp);
		if (slugp->type == EOF)
			break;
	}
	*++slugp = eofslug();
	printf("# %d slugs\n", (int)(slugp-slugs));
}

static slug *findend(slug *sp)
{
	slug *p;
	for (p = sp; p->type == sp->type; p++)	// skip runs
		;				// espec UF UF UF 
	for ( ; p < slugp; p++)
		switch (p->type) {
		case US:
		case UF:
		case BF:
		case PT:
		case BT:
			p = findend(p);
			break;
		case END:
			return p;
		}
	FATAL("walked past EOF in findend looking for %d (%s), line %d\n",
		sp->type, sp->typname(), sp->lineno());
	return sp;
}

static int markp(int i, int n, int parm)
{	// should VBOX i of n be marked to brevent breaking after it?
	if (i >= n-1)
		return 0;
	return i <= parm-2 || i >= n-parm;
}

static void markbreak(slug *p)
{
	// Mark impermissible breakpoints in BS's.
	// The parm field of a VBOX is >0 if we shouldn't break after it.
	int parm = 0;		// how many lines must stay on page
	int goahead = 1;	// true until we see the next BS
	int nowmark = 0;	// true when we should be marking
	int n = 0;
	while (p->type == BS)
		parm = p++->parm;	// latest BS parm applies
	slug *op = p;
	while (goahead) {
		switch (p->type) {
		case VBOX:		// count VBOXes so second pass knows
			if (p->dv > 0)	// knows how far to end of BS
				n++;
			break;
		case US:		// mark around EQ/EN, etc.
			nowmark = 1;
			p = findend(p);
			break;
		case UF:		// but not around floats, PTs, and BTs
		case BF:
		case PT:
		case BT:
			p = findend(p);
			break;
		case SP:		// naked SP:  probable macro botch
			nowmark = 1;	// mark around it anyhow
			break;
		case BS:		// beginning of next paragraph
		case END:		// probable macro botch
		case EOF:
			goahead = 0;	// stop work after marking
			nowmark = 1;
		default:
			break;
		}
		p++;
		if (nowmark) {
			int i = 0;	// VBOX counter for second pass
			while (op < p) {
				switch (op->type) {
				case VBOX:
					if (op->dv > 0)
						op->parm = markp(i, n, parm);
					i++;
					break;
				case US:	// caused second pass to begin
				case SP:
				case BS:
				case END:
				case EOF:
					op = p;
					break;
				case UF:	// skip on this pass too
				case BF:
				case PT:
				case BT:
					op = findend(op);
					break;
				default:
					break;
				}
			op++;
			}
			if (i != n)
				WARNING("markbreak failed : i %d n %d\n", i, n);
			op = p;
			nowmark = n = 0;
		}
	}
}

static void fixslugs()		// adjust bases and dv's, set parameters, etc.
{
	slug *prevV = 0, *p;
	for (p = slugs; p < slugp; p++) {
		if (p->type == VBOX) {
			prevV = p;
			continue;
		}
		if (p->base != 0) {
			WARNING("%s slug (type %d) has base = %d, line %d\n",
				p->typname(), p->type, p->base, p->lineno());
		}
		if ((p->type == SP) || (p->type == NE))
			continue;
		if (p->type == PAGE)
			prevV = 0;
		if (p->dv != 0)
		{
			if (prevV) {
				prevV->base = max(prevV->base, p->dv);
				p->dv = 0;
			} else {
				WARNING("s slug (type %d) has dv = %d, line %d\n",
					p->typname(), p->type, p->dv, p->lineno());
			}
		}
	}
	prevV = 0;
	int firstNP = 0, firstFO = 0, firstPL = 0;
	for (p = slugs; p < slugp; p++) {
		switch (p->type) {
		// adjust the dv in a sequence of VBOXes
		// by subtracting from each the base of the preceding VBOX
		case VBOX:
			if (prevV)
				p->dv -= prevV->base;
			prevV = p;
			break;
		case SP:
			p->dv = max(p->dv, 0);
			break;
		case PAGE:
			p->neutralize();
			prevV = 0;
			break;
		// record only first "declarations" of Page Top and bottom (FO);
		case PARM:
			switch (p->parm) {
			case NP:
				if (firstNP++ == 0)
					pagetop = p->parm2;
				p->neutralize();
				break;
			case FO:
				if (firstFO++ == 0)
					pagebot = p->parm2;
				p->neutralize();
				break;
			case PL:
				if (firstPL++ == 0)
					physbot = p->parm2;
				p->neutralize();
				break;
			}
			break;
		// things that begin groups; not US, which should nest properly
		case UF:
		case BF:
			while ((p+1)->type == p->type) {
							// join adjacent identical
				(p+1)->parm2 = p->parm;	// parm is latest
							// parm2 is previous
				p->neutralize();	// so it's not seen later
				p++;
			}
			break;
		// none of the above
		case US:
		case PT:
		case BT:
		case BS:
		case END:
		case TM:
		case COORD:
		case NE:
		case MC:
		case CMD:
		case EOF:
			break;
		default:
			WARNING("Unknown slug type %d in fixslugs, line %d\n",
				p->type, p->lineno());
			break;
		}
	}
	int pagesize = pagebot - pagetop;
	if (pagesize == 0)
		FATAL("Page dimensions not declared\n");
	if (physbot == 0)
		physbot = pagebot + pagetop;
	printf("# page top %d bot %d size %d physbot %d\n",
		pagetop, pagebot, pagesize, physbot);
	for (p = slugs; p < slugp; p++) {
		switch (p->type) {
		// normalize float parameters
		case BF:
		case UF:
					// primary goal
			p->parm = max(min(p->parm-pagetop, pagesize), 0);
					// secondary goal
			p->parm2 = max(min(p->parm2-pagetop, pagesize), 0);
			break;
		// normalize need parameters
		case NE:
			p->dv = max( min(p->dv, pagesize), 0);
			break;
		// mark permissible breaks
		case BS:
			markbreak(p);
			break;
		}
		if (dbg & 1)
			p->dump();
	}
}

void checkout()
{
	for (slug *p = slugs; p < slugp; p++)
		switch (p->type) {
		case PT:
		case BT:
			p = findend(p);
			break;
		case SP:
		case VBOX:
			if (p->seen != 1)
				WARNING("%s slug %d seen %d times\n",
					p->typname(), p->serialno(),
					p->seen);
			break;
		}
}

eofrange *lastrange;
stream	ptlist, btlist;

static slug *makeranges(slug *p, stream *s, int level)
{
	stream *t;

	for ( ; p < slugp; p++)
		switch (p->type) {
		case VBOX:
			s->append(new vboxrange(p));
			break;
		case SP:
			s->append(new sprange(p));
			break;
		case BS:
			s->append(new bsrange(p));
			break;
		case US:
			s->append(new usrange(p, t = new stream));
			p = makeranges(p+1, t, level+1);
			break;
		case BF:
			s->append(new bfrange(p, t = new stream));
			p = makeranges(p+1, t, level+1);
			break;
		case UF:
			s->append(new ufrange(p, t = new stream));
			p = makeranges(p+1, t, level+1);
			break;
		case PT:
			ptlist.append(new ptrange(p, t = new stream));
			p = makeranges(p+1, t, level+1);
			break;
		case BT:
			btlist.append(new btrange(p, t = new stream));
			p = makeranges(p+1, t, level+1);
			break;
		case END:
			s->append(new endrange(p));
			return p;
		case TM:
			s->append(new tmrange(p));
			break;
		case COORD:
			s->append(new coordrange(p));
			break;
		case NE:
			if (level) {
				WARNING("Nested NE commands are ignored, line %d\n",
					p->lineno());
				p->dv = 0;
			}
			s->append(new nerange(p));
			break;
		case MC:
			s->append(new mcrange(p));
			break;
		case CMD:
			if (level)
				WARNING("Nested command ignored, line %d\n",
					p->lineno());
			s->append(new cmdrange(p));
			break;
		case PARM:
			if (level)
				WARNING("Nested parameter ignored, line %d\n",
					p->lineno());
			s->append(new parmrange(p));
			break;
		case EOF:
			lastrange = new eofrange(p);
			return 0;
		}
	return p;
}

static queue text;			// unexamined input ranges; the real data

void startup(FILE *fp)
{
	doprefix(fp);			// peel off 'x' commands
	readslugs(fp);			// read everything into slugs[]
	fixslugs();			// measure parameters and clean up
	makeranges(slugs, &text, 0);	// add range superstructure
	measure(&text);		// heights of nested things
	rawmeasure(&text);
	while (text.more()) {
		range *r = text.dequeue();
		if (dbg & 2)
			r->dump();
		r->enqueue();
	}
}
