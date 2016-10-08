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

/*	Sccsid @(#)queue.cc	1.3 (gritter) 10/30/05	*/
#include	"misc.h"
#include	"slug.h"
#include	"range.h"
#include	"page.h"

queue	squeue;
queue	bfqueue;
queue	ufqueue;

// We use the stream function current() to access a queue's head.
// Thus, queue member curr should always point to its first range.
void queue::check(const char *whence)
{
	if (dbg & 8) {
		const char *p;
		if (this == &squeue)
			p = "squeue";
		else if (this == &bfqueue)
			p = "bfqueue";
		else if (this == &ufqueue)
			p = "ufqueue";
		else
			p = "weird queue";
		printf("#checking %s\n", p);
	}
	if (first != curr)
		FATAL("check(%s): first != curr, line %d\n", whence, curr->rp->lineno());
}

// When ranges are told to enqueue themselves, they are being rejected from the
// stage back onto their original queues.
// They reset any parameters that may have been altered by staging or trial
// composition.

void	range::enqueue(int block)
{
	squeue.enqueue(this);
	if (block)
		squeue.block();
}

void	ufrange::enqueue(int block)
{
	restore();			// both goal positions
	ufqueue.enqueue(this);
	if (block)
		ufqueue.block();
}

void	bfrange::enqueue(int block)
{
	restore();			// both goal positions
	bfqueue.enqueue(this);
	if (block)
		bfqueue.block();
}

int anymore()
{
	return !(squeue.empty() && ufqueue.empty() && bfqueue.empty());
}

void mergestream::unblock()
{
	squeue.unblock();
	bfqueue.unblock();
	ufqueue.unblock();
}

// Fill the staging area with a minimal chunk of input ranges.
int mergestream::prime()
{
	if (dbg & 4)
		printf("#entering mergestream::prime()\n");
	if (!empty())
		return 1;
	int brkok = 1;			// is it OK to break after the last
					// VBOX that was added to the stage?
	int needheight = -1;		// minimum acceptable height of the
					// chunk being constructed on stage
	// If the range at the head of any queue is breaking,
	// deal with it first.
	if (squeue.more() && squeue.current()->breaking())
		enqueue(squeue.dequeue());
	else if (bfqueue.more() && (bfqueue.current()->breaking() ||
		(bfqueue.serialno() < squeue.serialno())))
		enqueue(bfqueue.dequeue());
	else if (ufqueue.more() && (ufqueue.current()->breaking() ||
		(ufqueue.serialno() < squeue.serialno())))
		enqueue(ufqueue.dequeue());
	else while (squeue.more()) {
		// Fill the stage with enough ranges to be a valid chunk.
		range *r = squeue.dequeue();
		if (r->isvbox()) {	// VBOX
			if (dbg & 16)
				printf("#VBOX: !empty: %d; brkok: %d; vsince: %d\n",
					!empty(), brkok, currpage->vsince);
			if (!empty()	// there's something there
				&& brkok
					// it's OK to break here
				&& currpage->vsince >= 2
					// enough stream has gone onto this page
				&& rawht() >= needheight
					// current need has been satisfied
				) {
					// the stage already contains enough
					// ranges, so this one can wait
				r->enqueue();
				break;
			} else {
				if (r->rawht() > 0) {
					++currpage->vsince;
					brkok = r->brkafter();
				}
				enqueue(r);
			}
		} else if (r->isnested() || r->issp()) {	// US, SP
			if (!empty() && rawht() >= needheight) {
					// enough already, wait
				r->enqueue();
				break;
			}
			currpage->vsince = 0;
			enqueue(r);
			if (height() >= needheight)
				break;
		} else if (r->isneed()) {	// NE
			if (!empty() && rawht() >= needheight) {
					// not currently working on an unsatisfied NEed 
				r->enqueue();
				break;
			}
					// deal with overlapping NEeds
			needheight = rawht() + max(needheight - rawht(), r->needht());
			enqueue(r);
		} else if (r->forceflush() == NO) {
			enqueue(r);
		} else if (r->forceflush() == YES) {
			currpage->vsince = 0;
			if (!empty()) {
					// ready or not, r must wait
				r->enqueue();
				break;
			}
			enqueue(r);
			break;
		} else
			FATAL("unexpected  %s[%s] in prime(), line %d\n",
				r->typname(), r->headstr(), r->lineno());
	}
	return more();			// 0 if nothing was staged
}

void page::cmdproc()
{
	if (stage->next())
		FATAL("more than a single command on bsqueue\n");
	switch (stage->current()->cmdtype()) {
	case FC:	// freeze the current 2-column range and start a new one
		adddef(stage->dequeue());
		twocol->compose(FINAL);
		adddef(twocol);
		twocol = new multicol(this);
		break;
	case BP:	// force a page break
		adddef(stage->dequeue());
		squeue.block();
		break;
	case FL:	// flush out all floatables that precede this range:
			// no more stream input allowed until they're past
		if (stage->serialno() > ufqueue.serialno() ||
			stage->serialno() > bfqueue.serialno()) {
			range *r = stage->dequeue();
			r->enqueue(ANDBLOCK);
		} else
			adddef(stage->dequeue());
		break;
	default:
		stage->current()->dump();
		FATAL("unknown command\n");
	}
}

void page::parmproc()
{
	if (stage->next())
		FATAL("more than a single parameter on bsqueue\n");
	switch (stage->current()->parmtype()) {
	case NP:	// page top margin
		if (blank())
			pagetop = stage->current()->parm();
		pagesize = pagebot - pagetop;
		break;
	case FO:
		if (blank())
			pagebot = stage->current()->parm();
		pagesize = pagebot - pagetop;
		break;
	case PL:
		if (blank())
			physbot = stage->current()->parm();
		break;
	case MF:
		minfull = 0.01*stage->current()->parm();
		break;
	case CT:
		coltol = 0.01*stage->current()->parm();
		break;
	case WARN:
		wantwarn = stage->current()->parm();
		break;
	case DBG:
		dbg = stage->current()->parm();
		break;
	default:
		stage->current()->dump();
		FATAL("unknown parameter\n");
	}
	adddef(stage->dequeue());
}

// Process the contents of the staging area; a relic that used to do more.
void mergestream::pend()
{
	if (dbg & 4)
		printf("#entering mergestream::pend()\n");
	if (!more())
		return;
	if (current()->iscmd())
		currpage->cmdproc();
	else if (current()->isparm())
		currpage->parmproc();
	else
		currpage->tryout();
}
