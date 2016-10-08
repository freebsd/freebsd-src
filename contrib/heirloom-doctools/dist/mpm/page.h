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

/*	Sccsid @(#)page.h	1.3 (gritter) 10/30/05	*/
extern queue	squeue;			// the three queues on which ranges reside
extern queue	bfqueue;
extern queue	ufqueue;

extern double minfull;

extern double coltol;

int anymore();

// The following is used in some calls to range::enqueue(int = 0).
#define ANDBLOCK 1

class page;

enum { DRAFT = 0, FINAL = 1 };

// The mergestream currpage->stage serves as a staging area for page makeup:
// when primed, it contains a minimal acceptable chunk of input ranges.
// The page must either take or leave everything that's on stage.
class mergestream : public queue {
	page	*currpage;		// current page that's accepting stuff
  public:
	mergestream(page *cp)	{ currpage = cp; unblock(); }
	void	unblock();
	int	prime();		// stage next legal chunk
	void	pend();			// process pending chunk on stage
};

// The multicol currpage->twocol is the two-column piece of the page to which
// two-column ranges are currently being added.
// The page sets htavail to indicate how tall it is allowed to become.
// All ranges on definite must be placed when the multicol is printed.
// Each of these definite ranges also resides on one of column[0] and [1],
// which represent the current best guess about how to divide definite
// between the two columns.
class multicol : public range {
	page	*currpage;		// current page that's accepting stuff
	stream	definite;		// definitely on page
	stream	scratch;		// for trial compositions
	stream	column[2];		// left (0) and right (1) columns
	int	leftblocked;		// OK to add to left column?
	int	htavail;		// max possible ht, set by page::tryout()
	int	prevhtavail;		// max 2-colht last time we added something
	friend class	page;
public:
	multicol(page *cp)	{ currpage = cp;
				leftblocked = 0;
				htavail = 0;
				prevhtavail = -1;
				setgoal(NOGOAL); }
					// the two-column piece behaves as part
					// of the stream of single-column input.
	int	numcol()	{ return 1; }
	int	nonempty()	{ return definite.more(); }
	void	choosecol(range *, int);// add first arg to one or other column
	void	choosecol(stream*, int);// add *all ranges on first arg*
					// to one or other column
					// NOT the same as a mapcar of the
					// preceding function over the ranges
					// on the first argument!
	void	compose(int);		// divide into two columns
	void	tryout();		// decide which column gets stage contents
	void	stretch(int);		// justify both columns to given height
	int	print(int curv, int col);
	int	height();		// an upper bound on actual height
	int	rawht()		{ return max(column[0].rawht(), column[1].rawht()); }
	void	reheight(int *cv, int *mv)
				{ *cv += height(); *mv = max(*mv, *cv); }
	void	dump();
	int	isvbox()	{ return nonempty(); }	// during trimspace()
};

// These sentinel ranges are used to separate the ranges on twocol::definite
// into the chunks in which they came from the staging area.
// Thus, they preserve the results of the computation that was done to prime
// page::stage.
class sentrange : public range {
  public:
	sentrange()		{ }
	int	numcol()	{ return 2; }
	int	issentinel()	{ return 1; }
};

class page {
	int	pagesize;		// allowed maximum height
	int	prevncol;		// was last item tried 1- or 2-column?
	int	vsince;			// how many vboxes from "current" BS
					// (to avoid putting a single line on
					// a page with a very large floatable)
	stream	definite;		// definitely on page, in input order
	stream	scratch;		// playground in which to alter page
	void	cmdproc();		// process any of several commands
	void	parmproc();		// process any of several parameters
	void	tryout();		// see whether current stage contents fit
	void	compose(int);		// float and trim current page contents
	void	makescratch(int);	// fill scratch area
	void	commit();		// accept the items on stage
	void	welsh();		// reject the items on stage
	void	adddef(range *r);	// add to one of the definite queues
					// (definite or twocol->definite)
  public:
	mergestream *stage;
	friend class	mergestream;
	multicol *twocol;
	friend class multicol;
	page(int p)	{ pagesize = p;
			prevncol = 1;
			vsince = 0;
			stage = new mergestream(this);
			twocol = new multicol(this); }
	~page()	{ definite.freeall(); scratch.freeall(); }
	void	fill();
	int	blank()	{ return !definite.more() && !twocol->definite.more();}
	void	print();
};

// functions in page.c
extern int main(int, char **);
