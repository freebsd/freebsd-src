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

/*	Sccsid @(#)page.cc	1.4 (gritter) 10/30/05	*/
#include	<locale.h>
#include	"misc.h"
#include	"slug.h"
#include	"range.h"
#include	"page.h"

const int	MAXRANGES	= 1000;
static range *ptemp[MAXRANGES];		// for movefloats()

static void swapright(int n)		// used by movefloats()
{
	range *t = ptemp[n];
	ptemp[n] = ptemp[n+1];
	ptemp[n+1] = t;
	ptemp[n]->setaccum( ptemp[n+1]->accum() -
			    ptemp[n+1]->rawht() + ptemp[n]->rawht() );
	ptemp[n+1]->setaccum( ptemp[n]->accum() + ptemp[n+1]->rawht() );
}

// Figure out the goal position for each floating range on scratch,
// and move it past stream ranges until it's as close to its goal as possible.
static void movefloats(stream *scratch, double scale)
{
	const int Huge = 10000000;
	int nranges;
	for (nranges = 0; scratch->more(); scratch->advance())
		ptemp[nranges++] = scratch->current();
	scratch->freeall();
	ufrange rtemp;
	ptemp[nranges] = &rtemp;
	rtemp.setgoal(Huge);
	int accumV = 0;				// compute accum values and
	int i;
	for (i = 0; i < nranges; i++) {	// pick closest goal for floats
		ptemp[i]->pickgoal(accumV, scale);
		ptemp[i]->setaccum(accumV += ptemp[i]->rawht());
	}
	int j;					// index for inner loop below:
	for (i = nranges; --i >= 0; )		// stably sort floats to bottom
		for (j = i; j < nranges; j++)
			if (ptemp[j]->goal() > ptemp[j+1]->goal())
				swapright(j);
			else
				break;
	if (dbg & 16)
		printf("#movefloats:  before floating, from bottom:\n");
	for (i = nranges; --i >= 0; ) {		// find topmost float
		if (ptemp[i]->goal() == NOGOAL)
			break;
		if (dbg & 16)
			printf("# serialno %d goal %d height %d\n",
				ptemp[i]->serialno(), ptemp[i]->goal(),
				ptemp[i]->rawht());
	}					// i+1 is topmost float
	for (i++ ; i < nranges; i++)		// move each float up the page
		for (j = i; j > 0; j--)		// as long as closer to its goal
			if (ptemp[j]->goal()
			  <= ptemp[j-1]->accum() + ptemp[j]->rawht()/2
			  && ptemp[j-1]->goal() == NOGOAL)
				swapright(j-1);
			else
				break;
	if (ptemp[nranges] != &rtemp)
		FATAL("goal sentinel has disappeared from movefloats");
	for (i = 0; i < nranges; i++)		// copy sorted list back
		scratch->append(ptemp[i]);
}

// Traverse the leaves of a tree of ranges, filtering out only SP and VBOX.
static range *filter(generator *g)
{
	range *r;
	while ((r = g->next()))
		if (r->isvbox() || r->issp())
			break;
	return r;
}

// Zero out leading and trailing spaces; coalesce adjacent SP's.
static void trimspace(stream *scratch)
{
	range *r, *prevr = 0;
	generator g;
	for (g = scratch; (r = filter(&g)) != 0 && r->issp(); prevr = r)
		r->setheight(0);		// zap leading SP
	for ( ; (r = filter(&g)) != 0; prevr = r)
		if (r->issp())
		{
			if (prevr && prevr->issp()) {
						// coalesce adjacent SPs
				r->setheight(max(r->rawht(), prevr->height()));
				prevr->setheight(0);
			} else			// a VBOX intervened
				r->setheight(r->rawht());
		}
	if (prevr && prevr->issp())		// zap *all* trailing space
		prevr->setheight(0);		// (since it all coalesced
						// into the last one)
}

// Pad the non-zero SP's in scratch so the total height is wantht.
// Note that the SP values in scratch are not the raw values, and
// indeed may already have been padded.
static void justify(stream *scratch, int wantht)
{
	range *r;
	int nsp = 0, hsp = 0;

	int adjht = scratch->height();
					// Find all the spaces.
	generator g;
	for (g = scratch; (r = g.next()); )
		if (r->issp() && r->height() > 0) {
			nsp++;
			hsp += r->height();
		}
	int excess = wantht - adjht;
	if (excess < 0)
		WARNING("something on page %d is oversize by %d\n",
			userpn, -excess);
	if (dbg & 16)
		printf("# justify %d: excess %d nsp %d hsp %d adjht %d\n",
			userpn, excess, nsp, hsp, adjht);
	if (excess <= 0 || nsp == 0)
		return;
					// Redistribute the excess space.
	for (g = scratch; (r = g.next()); )
		if (r->issp() && r->height() > 0) {
			int delta = (int) ((float)(r->height()*excess)/hsp + 0.5);
			if (dbg & 16)
				printf("# pad space %d by %d: hsp %d excess %d\n",
					r->height(), delta, hsp, excess);
			r->setheight(r->height() + delta);
		}
}

// If r were added to s, would the height of the composed result be at most maxht?
int wouldfit(range *r, stream *s, int maxht)
{
	if (r->rawht() + s->rawht() <= maxht)
		return 1;		// the conservative test succeeded
	stream scratch;			// local playground for costly test
	for (stream cd = *s; cd.more(); cd.advance())
		scratch.append(cd.current());
	scratch.append(r);
	movefloats(&scratch, ((double) scratch.rawht())/maxht);
	trimspace(&scratch);
	int retval = scratch.height() <= maxht;
	scratch.freeall();
	return retval;
}

// If s1 were added to s, would the height of the composed result be at most maxht?
// The computational structure is similar to that above.
int wouldfit(stream *s1, stream *s, int maxht)
{
	if (s1->rawht() + s->rawht() <= maxht)
		return 1;
	stream scratch, cd;
	for (cd = *s; cd.more(); cd.advance())
		scratch.append(cd.current());
	for (cd = *s1; cd.more(); cd.advance())
		scratch.append(cd.current());
	movefloats(&scratch, ((double) scratch.rawht())/maxht);
	trimspace(&scratch);
	int retval = scratch.height() <= maxht;
	scratch.freeall();
	return retval;
}

// All of stream *s is destined for one column or the other; which is it to be?
void multicol::choosecol(stream *s, int goalht)
{
	stream *dest;
	if (!leftblocked && wouldfit(s, &(column[0]), goalht))
		dest = &(column[0]);
	else {
		dest = &(column[1]);
		if (!s->current()->floatable())
					// a stream item is going into the right
					// column, so no more can go into the left.
			leftblocked = 1;
	}
	for (stream cd = *s; cd.more(); cd.advance())
		dest->append(cd.current());
}

double coltol = 0.5;

// Try, very hard, to put everything in the multicol into two columns
// so that the total height is at most htavail.
void multicol::compose(int defonly)
{
	if (!nonempty()) {
		setheight(0);
		return;
	}
	scratch.freeall();		// fill scratch with everything destined
					// for either column
	stream cd;
	for (cd = definite; cd.more(); cd.advance())
		scratch.append(cd.current());
	if (!defonly)
		for (cd = *(currpage->stage); cd.more(); cd.advance())
			if (cd.current()->numcol() == 2)
				scratch.append(cd.current());
	scratch.restoreall();		// in particular, floatables' goals
	int rawht = scratch.rawht();
	int halfheight = (int)(coltol*rawht);
					// choose a goal height
	int maxht = defonly ? halfheight : htavail;
secondtry:
	int i;
	for (i = 0; i < 2; i++)
		column[i].freeall();
	leftblocked = 0;
	cd = scratch;
	while (cd.more()) {
		queue ministage;	// for the minimally acceptable chunks
		ministage.freeall();	// that are to be added to either column
		while (cd.more() && !cd.current()->issentinel()) {
			ministage.enqueue(cd.current());
			cd.advance();
		}
		choosecol(&ministage, maxht);
		if (cd.more() && cd.current()->issentinel())
			cd.advance();	// past sentinel
	}
	if (height() > htavail && maxht != htavail) {
					// We tried to balance the columns, but
					// the result was too tall.  Go back
					// and try again with the less ambitious
					// goal of fitting the space available.
		maxht = htavail;
		goto secondtry;
	}
	for (i = 0; i < 2; i++) {
		movefloats(&(column[i]), ((double) column[i].rawht())/currpage->pagesize);
		trimspace(&(column[i]));
	}
	if (dbg & 32) {
		printf("#multicol::compose: htavail %d maxht %d dv %d\n",
			htavail, maxht, height());
		dump();
	}
	if (defonly)
		stretch(height());
}

// A sequence of two-column ranges waits on the stage.
// So long as the page's skeleton hasn't changed--that is, the maximum height
// available to the two-column chunk is the same--we just use the columns that
// have been built up so far, and choose a column into which to put the stage.
// If the skeleton has changed, however, then we may need to make entirely
// new decisions about which column gets what, so we recompose the whole page.
void multicol::tryout()
{
	if (htavail == prevhtavail)
		choosecol(currpage->stage, htavail);
	else
		currpage->compose(DRAFT);
	prevhtavail = htavail;
}

// Make both columns the same height.
// (Maybe this should also be governed by minfull,
// to prevent padding very underfull columns.)
void multicol::stretch(int wantht)
{
	if (wantht < height())
		FATAL("page %d: two-column chunk cannot shrink\n", userpn);
	for (int i = 0; i < 2; i++)
		justify(&(column[i]), wantht);
	if (dbg & 16)
		printf("#col hts: left %d right %d\n",
			column[0].height(), column[1].height());
}

// Report an upper bound on how tall the current two-column object is.
// The (possibly composed) heights of the two columns give a crude upper
// bound on the total height.  If the result is more than the height
// available for the two-column object, then the columns are each
// composed to give a better estimate of their heights.
int multicol::height()
{
	int retval = max(column[0].height(), column[1].height());
	if (retval < htavail)
		return retval;
	for (int i = 0; i < 2; i++) {
		movefloats(&(column[i]), ((double) column[i].height())/currpage->pagesize);
		trimspace(&(column[i]));
	}
	return max(column[0].height(), column[1].height());
}

void multicol::dump()
{
	printf("####2COL dv %d\n", height());
	printf("# left column:\n");
	column[0].dump();
	printf("# right column:\n");
	column[1].dump();
}

// From the head of queue qp, peel off a piece whose raw height is at most space.
int peeloff(stream *qp, int space)
{
	stream *s1 = qp->current()->children();
	if (!(s1 && s1->more() && s1->current()->height() <= space))
					// in other words, either qp's head is
					// not nested, or its first subrange
		return 0;		// is also too big, so we give up
	qp->split();
	s1 = qp->current()->children();
	stream *s2 = qp->next()->children();
	while (s2->more() && s2->current()->rawht() <= space) {
		s1->append(s2->current());
		space -= s2->current()->rawht();
		s2->advance();
	}
	return 1;
}

// There are four possibilities for consecutive calls to tryout().
// If we're processing a sequence of single-column ranges, tryout()
// uses the original algorithm: (1) conservative test; (2) costly test;
// (3) split a breakable item.
// If we're processing a sequence of double-column ranges, tryout()
// defers to twocol->tryout(), which gradually builds up the contents
// of the two columns until they're as tall as they can be without
// exceeding twocol->htavail.
// If we're processing a sequence of single-column ranges and we
// get a double-column range, then we use compose() to build a
// skeleton page and set twocol->htavail, the maximum height that
// should be occupied by twocol.
// If we're processing a sequence of double-column ranges and we
// get a single-column range, then we should go back and squish
// the double-column chunk as short as possible before we see if
// we can fit the single-column range.
void page::tryout()
{
	if (!stage->more())
		FATAL("empty stage in page::tryout()\n");
	int curnumcol = stage->current()->numcol();
	if (dbg & 32) {
		printf("#page::tryout(): ncol = %d, prevncol = %d; on stage:\n",
			curnumcol, prevncol);
		stage->dump();
		printf("#END of stage contents\n");
	}
	switch(curnumcol) {
	default:
		FATAL("unexpected number of columns in tryout(): %d\n",
			stage->current()->numcol());
		break;
	case 1:
		if (prevncol == 2)
			compose(FINAL);
		if (wouldfit(stage, &definite, pagesize - twocol->height()))
			commit();
		else if (stage->current()->breakable() || (blank()
			&& peeloff(stage,
				pagesize - (definite.height() +
				twocol->height())))) {
			// first add the peeled-off part that fits
			adddef(stage->dequeue());
			// then send the rest back for later
			stage->current()->setbreaking();
			welsh();
		} else if (blank()) {
			stage->current()->rdump();
			FATAL("A %s is too big to continue.\n",
			stage->current()->typname());
		} else
			welsh();
		break;
	case 2:
		if (prevncol == 1)
			compose(DRAFT);
		else
			twocol->tryout();
		if (scratch.height() <= pagesize)
			commit();
		else
			welsh();
		break;
	}
	prevncol = curnumcol;
}

// To compose the page, we (1) fill scratch with the stuff that's meant to
// go on the page; (2) compose scratch as best we can; (3) set the maximum
// height available to the two-column part of the page; (4) have the two-
// column part compose itself.
// In the computation of twocol->htavail, it does not matter that
// twocol->height() is merely an upper bound, because it is merely being
// subtracted out to give the exact total height of the single-column stuff.
void page::compose(int final)
{
	makescratch(final);
	int adjht = scratch.rawht();
	if (dbg & 16)
		printf("# page %d measure %d\n", userpn, adjht);
	movefloats(&scratch, ((double) adjht)/pagesize);
	trimspace(&scratch);
	twocol->htavail = pagesize - (scratch.height() - twocol->height());
	twocol->compose(final);
	adjht = scratch.height();
	if (dbg & 16)
		printf("# page %d measure %d after trim\n", userpn, adjht);
}

// Fill the scratch area with ranges destined for the page.
// If defonly == 0, then add anything that's on stage--this is a trial run.
// If defonly != 0, use only what's definitely on the page.
void page::makescratch(int defonly)
{
	scratch.freeall();
	stream cd;
	for (cd = definite; cd.more(); cd.advance())
		scratch.append(cd.current());
	if (!defonly)
		for (cd = *stage; cd.more(); cd.advance())
			if (cd.current()->numcol() == 1)
				scratch.append(cd.current());
	if (twocol->nonempty())
		scratch.append(twocol);
}

// Accept the current contents of the stage.
// If the stage contains two-column ranges, add a sentinel to indicate the end
// of a chunk of stage contents.
void page::commit()
{
	if (dbg & 4)
		printf("#entering page::commit()\n");
	int numcol = 0;
	while (stage->more()) {
		numcol = stage->current()->numcol();
		adddef(stage->dequeue());
	}
	if (numcol == 2)
		adddef(new sentrange);
}

// Send the current contents of the stage back to its source.
void page::welsh()
{
	if (dbg & 4)
		printf("#entering page::welsh()\n");
	while (stage->more()) {
		range *r = stage->dequeue();
		r->enqueue(ANDBLOCK);
	}
}

enum { USonly = 1 };

// So long as anything is eligible to go onto the page, keep trying.
// Once nothing is eligible, compose and justify the page.
void page::fill()
{
	while (stage->prime())
		stage->pend();
	compose(FINAL);
	if (dbg & 16)
		scratch.dump();
	if (anymore()) {
		int adjht = scratch.height();
		if (adjht > minfull*pagesize) {
			justify(&scratch, pagesize);
			adjht = scratch.height();
			int stretchamt = max(pagesize - adjht, 0);
			twocol->stretch(twocol->height() + stretchamt);
					// in case the page's stretchability lies
					// entirely in its two-column part
		} else
			WARNING("page %d only %.0f%% full; will not be adjusted\n",
				userpn, 100*(double) adjht/pagesize);
	}
}

void page::adddef(range *r)
{
	if (dbg & 4)
		printf("#entering page::adddef()\n");
	switch (r->numcol()) {
	case 1:	definite.append(r);
		break;
	case 2: twocol->definite.append(r);
		break;
	default: FATAL("%d-column range unexpected\n", r->numcol());
	}
}

int multicol::print(int cv, int col)
{
	if (col != 0)
		FATAL("multicolumn output must start in left column\n");
	int curv = cv, maxv = cv;	// print left column
	for ( ; column[0].more(); column[0].advance()) {
		curv = column[0].current()->print(curv, 0);
		maxv = max(maxv, curv);
	}
	curv = cv;			// print right column
	for ( ; column[1].more(); column[1].advance()) {
		curv = column[1].current()->print(curv, 1);
		maxv = max(maxv, curv);
	}
	return maxv;
}

void page::print()
{
	static int tops = 1, bots = 1;
	if (!scratch.more()) {
		WARNING("## Here's what's left on squeue:\n");
		squeue.dump();
		WARNING("## Here's what's left on bfqueue:\n");
		bfqueue.dump();
		WARNING("## Here's what's left on ufqueue:\n");
		ufqueue.dump();
		WARNING("page %d appears to be empty\n", userpn);
		fflush(stderr), fflush(stdout), exit(0);
					// something is very wrong if this happens
	}
	printf("p%d\n", userpn);	// print troff output page number
	if (ptlist.more()) {		// print page header
		ptlist.current()->print(0, 0);
		ptlist.advance();
	} else if (tops) {
		WARNING("ran out of page titles at %d\n", userpn);
		tops = 0;
	}
	int curv = 0;
	printf("V%d\n", curv = pagetop);// print page contents
	for ( ; scratch.more(); scratch.advance()) {
		curv = scratch.current()->print(curv, 0);
	}
	if (btlist.more()) {		// print page footer
		btlist.current()->print(0, 0);
		btlist.advance();
	} else if (bots) {
		WARNING("ran out of page bottoms at %d\n", userpn);
		bots = 0;
	}
	printf("V%d\n", physbot);	// finish troff output page
}

int	pagetop	= 0;		// top printing margin
int	pagebot = 0;		// bottom printing margin
int	physbot = 0;		// physical bottom of page

double minfull = 0.9;		// minimum fullness before padding

int	pn	= 0;		// cardinal page number
int	userpn	= 0;		// page number derived from PT slugs

static void makepage()
{
	page pg(pagebot - pagetop);
	++pn;
	userpn = ptlist.more() ? ptlist.current()->pn() : pn;
	pg.fill();
	pg.print();
}

static void conv(FILE *fp)
{
	startup(fp);		// read slugs, etc.
	while (anymore())
		makepage();
	lastrange->print(0, 0);	// trailer
	checkout();		// check that everything was printed
}

int
main(int argc, char **argv)
{
	static FILE *fp = stdin;
	setlocale(LC_CTYPE, "");
	progname = argv[0];
	while (argc > 1 && argv[1][0] == '-') {
		switch (argv[1][1]) {
		case 'd':
			dbg = atoi(&argv[1][2]);
			if (dbg == 0)
				dbg = ~0;
			break;
		case 'm':
			minfull = 0.01*atof(&argv[1][2]);
			break;
		case 'c':
			coltol = 0.01*atof(&argv[1][2]);
			break;
		case 'w':
			wantwarn = 1;
			break;
		}
		argc--;
		argv++;
	}
	if (argc <= 1)
		conv(stdin);
	else
		while (--argc > 0) {
			if (strcmp(*++argv, "-") == 0)
				fp = stdin;
			else if ((fp = fopen(*argv, "r")) == NULL)
				FATAL("can't open %s\n", *argv);
			conv(fp);
			fclose(fp);
		}
	exit(0);
}
