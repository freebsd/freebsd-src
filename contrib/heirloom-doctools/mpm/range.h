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

/*	Sccsid @(#)range.h	1.3 (gritter) 10/30/05	*/
const int	NOGOAL = -1;

class stream;

enum primeflush { NO, YES, EXPECTED, UNEXPECTED };	// mergestream::prime()

// Ranges do two things.  They interpose a layer between slugs and the rest
// of the program; this is important because of the grossness of the slug
// data structure (made necessary by its origins in troff output).  Ranges also
// group together other ranges into meaningful chunks like unbreakable stream
// objects, floatable objects, and page headers and footers.
// Member function height() returns a range's height as of the latest composition.
// Member function rawht() returns the range's original height in the input.
class range {
  protected:
	slug	*first;		// earliest slug in range
	int	accumV;		// accumulated V to this point
  public:
	range()		{ first = 0; accumV = 0; }
	range(slug *p)	{ first = p; accumV = 0; }
	char	*headstr()		{
		return first ? first->headstr() : (char *)""; }
	char	*typname()		{ return first->typname(); }
	int	serialno()		{ return first->serialno(); }
	int	lineno()		{ return first->lineno(); }
	virtual void	dump()		{ first->dump(); }
	virtual void	rdump()		{ dump(); }
	virtual int	print(int cv, int col)	{
		first->slugout(col); return cv; }
	virtual int	floatable()	{ return 0; }
	virtual int	brkafter()	{ return 1; }
	virtual int	isnested()	{ return 0; }
	virtual int	issp()		{ return 0; }
	virtual int	isvbox()	{ return 0; }
	virtual int	isneed()	{ return 0; }
	virtual int	iscmd()		{ return 0; }
	virtual int	cmdtype()	{ return -1; }
	virtual int	isparm()	{ return 0; }
	virtual int	parmtype()	{ return -1; }
	virtual int	parm()		{ return -1; }
	virtual int	breakable()	{ return 0; }
	virtual int	forceflush()	{ return UNEXPECTED; }
	virtual int	pn()		{ return 0; }
	virtual stream	*children()	{ return 0; }	// see page::peeloff()
	virtual void	killkids()	{ }
	virtual void	enqueue(int = 0);
	virtual int	height()	{ return 0; }
	virtual int	rawht()		{ return 0; }
	virtual int	needht()	{ return 0; }
	virtual void	reheight(int *, int *)	{ }
	virtual void	rerawht(int *, int *)	{ }
	virtual void	setheight(int) { }
	virtual void	restore()	{ }		// goals of floatables
	virtual int	goal()		{ return NOGOAL; }
	int		accum()		{ return accumV; }
	void		setaccum(int n)	{ accumV = n; }
	virtual	void	setgoal(int)	{ }
	virtual void	pickgoal(int, double)	{ }
	virtual int	numcol()	{ return first->numcol(); }
	virtual int	issentinel()	{ return 0; }
	virtual range	*clone()	{ return 0; }
	virtual int	breaking()	{ return 0; }
	virtual void	setbreaking()	{ }
};

class vboxrange : public range {
	int	dv;		// inherited from slug
	int	base;		// inherited from slug
	int	brk;		// 0 => ok to break after, 1 => no break 
  public:
	vboxrange(slug *p) : range(p) { dv = p->dv; base = p->base; brk = p->parm; }
	void	dump() {
		printf("#### VBOX brk? %d dv %d ht %d\n", brk, dv, dv+base); }
	int	print(int cv, int col) {
		printf("V%d\n", cv += dv); first->slugout(col); return cv+base; }
	int	brkafter()		{ return !brk; }
	int	isvbox()		{ return 1; }
	int	forceflush()		{ return NO; }
	int	height()		{ return dv + base; }
	int	rawht()			{ return first->dv + first->base; }
	void	reheight(int *cv, int *mv) {
		*cv += dv+base; *mv = max(*mv, *cv); }
	void	rerawht(int *cv, int *mv) {
		*cv += rawht(); *mv = max(*mv, *cv); }
};

class sprange : public range {
	int dv;
  public:
	sprange(slug *p) : range(p) { dv = first->dv; }
	void	dump() {
		printf("#### SP dv %d (originally %d)\n", dv, first->dv); }
	int	print(int cv, int col)	{
		first->slugout(col); return cv + dv; }
	int	issp()			{ return 1; }
	int	forceflush()		{ return YES; }
	int	height()		{ return dv; }
	int	rawht()			{ return first->dv; }
	void	reheight(int *, int *);
	void	rerawht(int *, int *);
	void	setheight(int n)	{ dv = n; }
};

class tmrange : public range {
  public:
	tmrange(slug *p) : range(p)	{ }
	int	forceflush()		{ return NO; }
	int	print(int cv, int col)	{ first->slugout(col); return cv; }
};

class coordrange : public range {
  public:
	coordrange(slug *p) : range(p)	{ }
	int	forceflush()		{ return NO; }
	int	print(int cv, int col)
		{ first->slugout(col); printf(" Y %d\n", cv); return cv; }
};

class nerange : public range {
  public:
	nerange(slug *p) : range(p)	{ }
	int	isneed()		{ return 1; }
	int	forceflush()		{ return YES; }
	int	needht()		{ return first->dv; }
};

class mcrange : public range {
  public:
	mcrange(slug *p) : range(p)	{ }
	int	forceflush()		{ return YES; }
};

class cmdrange : public range {
  public:
	cmdrange(slug *p) : range(p)	{ }
	int	iscmd()			{ return 1; }
	int	forceflush()		{ return YES; }
	int	cmdtype()		{ return first->parm; }
};

class parmrange : public range {
  public:
	parmrange(slug *p) : range(p)	{ }
	int	isparm()		{ return 1; }
	int	forceflush()		{ return YES; }
	int	parmtype()		{ return first->parm; }
	int	parm()			{ return first->parm2; }
};

class bsrange : public range {
  public:
	bsrange(slug *p) : range(p)	{ }
	int	forceflush()		{ return NO; }
	int	print(int cv, int col)	{ first->slugout(col); return cv; }
};

class endrange : public range {
  public:
	endrange(slug *p) : range(p)	{ }
	int	forceflush()		{ return UNEXPECTED; }
};

class eofrange : public range {
  public:
	eofrange(slug *p) : range(p)	{ }
	int	forceflush()		{ return UNEXPECTED; }
};

extern eofrange *lastrange;	// the EOF block (trailer, etc.) goes here

int measure(stream *);
int rawmeasure(stream *);

// A nestrange packages together a sequence of ranges, its subrange.
// Other parts of the program reach in and alter the dimensions of
// some of these ranges, so when the height of a range is requested
// it is computed completely afresh.
// (Note:  the alternative, of keeping around many copies of ranges
// with different dimensions, was abandoned because of the difficulty
// of ensuring that exactly one copy of each original range would be
// output.)
class nestrange : public range {
  protected:
	stream	*subrange;
	int isbreaking;
	int rawdv;
  public:
	nestrange() : range()	{ subrange = 0; isbreaking = 0; rawdv = -1; }
	nestrange(slug *p, stream *s) : range(p)
				{ subrange = s; isbreaking = 0; rawdv = -1; }
	void	rdump();
	virtual void restore();
	stream	*children()	{ return subrange; }
	void	killkids();
	int	height()	{ return measure(subrange); }
	int	rawht()		{ if (rawdv < 0 || isbreaking) rawdv = rawmeasure(subrange);
					return rawdv; }
	void	reheight(int *cv, int *mv) {
			*mv += measure(subrange); *cv = max(*mv, *cv); }
	void	rerawht(int *cv, int *mv) {
			*mv += rawht(); *cv = max(*mv, *cv); }
	int	isnested()	{ return 1; }
	int	forceflush()	{ return EXPECTED; }
	int	print(int cv, int col);
	int	breaking()	{ return isbreaking; }
	void	setbreaking()	{ isbreaking++; }
};

class usrange : public nestrange {
  public:
	usrange()	{ }
	usrange(slug *p, stream *s) : nestrange(p, s) {}
	void dump() { printf("#### US	dv %d\n", height()); }
	range	*clone();
};

class ufrange : public nestrange {
	int	goalV, goal2;
  public:
	ufrange()	{ }
	ufrange(slug *p, stream *s) : nestrange(p, s) {
		goalV = p->parm; goal2 = p->parm2; }
	void 	dump() { printf("#### UF   dv %d goal %d goal2 %d\n",
		height(), goalV, goal2); }
	int	floatable()	{ return 1; }
	void	enqueue(int = 0);
	range	*clone();
	int	goal()		{ return goalV; }
	void	setgoal(int n)	{ goalV = goal2 = n; }
	void	pickgoal(int acv, double scale);
	void	restore()	{ goalV = first->parm; goal2 = first->ht; }
};

class bfrange : public nestrange {
	int	goalV, goal2;
  public:
	bfrange()	{ }
	bfrange(slug *p, stream *s) : nestrange(p, s) {
		goalV = p->parm; goal2 = p->parm2; }
	void 	dump() { printf("#### BF   dv %d goal %d goal2 %d\n",
		height(), goalV, goal2); }
	int	floatable()	{ return 1; }
	void	enqueue(int = 0);
	range	*clone();
	int	goal()		{ return goalV; }
	void	setgoal(int n)	{ goalV = goal2 = n; }
	void	pickgoal(int acv, double scale);
	void	restore()	{ goalV = first->parm; goal2 = first->parm2; }
	int	breakable()	{ return 1; }	// can be broken
};

class ptrange : public nestrange {
	int	pgno;
  public:
	int	pn()	{ return pgno; }
	ptrange(slug *p, stream *s) : nestrange(p, s) { pgno = p->parm; }
	void 	dump() { printf("#### PT   pgno %d dv %d\n", pgno, height()); }
};

class btrange : public nestrange {
	int	pgno;
  public:
	btrange(slug *p, stream *s) : nestrange(p, s) { pgno = p->parm; }
	void 	dump() { printf("#### BT   pgno %d dv %d\n", pgno, height()); }
};

// A stream is a sequence of ranges; we use this data structure a lot
// to traverse various sequences that crop up in page-making.
class stream {
  protected:
public:
	struct strblk {		// ranges are linked by these blocks
		strblk	*next;
		range	*rp;
	};
	strblk	*first;
	strblk	*last;
	strblk	*curr;
  public:
	stream()		{ curr = last = first = 0; }
	stream(range *r)	{ curr = last = first = new strblk;
					last->rp = r; last->next = 0; }
	void	freeall();	// note:  not a destructor
	void	dump();		// top level
	void	rdump();	// recursive
	int	restoreall();
	range	*current()	{ return curr->rp; }
	range	*next()		{ return curr && curr->next ? curr->next->rp : 0; }
	void	advance()	{ curr = curr->next; }
	range	*append(range *r);
	void	split();
	int	more()		{ return curr && curr->rp; }
	int	height();
	int	rawht();
};

// A generator iterates through all the ranges of a stream
// (not just the root ranges of nestranges).
class generator {
	stream	s;
	generator *child;
  public:
	generator()		{ child = 0; }
	generator(stream *sp)	{ s = *sp; child = 0; }
	range	*next();
};

extern stream	ptlist, btlist;		// page titles

#undef	INFINITY
#define INFINITY 1000001

// A queue is a distinguished kind of stream.
// It keeps its contents in order by the serial numbers of the ranges.
// A queue can be blocked from dequeuing something to indicate
// that it's not worth considering the queue again on a given page.
class queue : public stream {
	strblk	*newguy;
  protected:
	int	blocked;
	void	check(const char *);
  public:
	queue() : blocked(0)	{ }
	range	*enqueue(range *r);
	range	*dequeue();
	void	block()		{ blocked = 1; }
	void	unblock()	{ blocked = 0; }
	int	more()		{ return !blocked && stream::more(); }
	int	empty()		{ return !stream::more(); }
	int	serialno()	{ return empty() ? INFINITY : current()->serialno(); }
};

// functions in range.c
void checkout();
void startup(FILE *);
