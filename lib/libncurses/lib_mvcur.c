/*---------------------------------------------------------------------------
 *
 *	lib_mvcur.c
 *
 *	The routine mvcur() etc.
 *
 *	last edit-date: [Wed Jun 16 14:13:22 1993]
 *
 *	-hm	conversion from termcap -> terminfo
 *	-hm	optimization debugging
 *	-hm	zeyd's ncurses 0.7 update
 *	-hm	eat_newline_glitch bugfix
 *	-hm	hpux lint'ing ..
 *
 *---------------------------------------------------------------------------*/
 
/* This work is copyrighted. See COPYRIGHT.OLD & COPYRIGHT.NEW for   *
*  details. If they are missing then this copy is in violation of    *
*  the copyright conditions.                                         */

#include <string.h>
#include <stdlib.h>
#include "terminfo.h"
#include "curses.priv.h"

#ifndef OPT_MVCUR
/*
**
**	mvcur(oldrow, oldcol, newrow, newcol)
**	A hack for terminals that are smart enough
**	to know how to move cursor.
**	There is still a bug in the alternative long-
**	winded code.
**
*/

int mvcur(int oldrow, int oldcol, int newrow, int newcol)
{
    T(("mvcur(%d,%d,%d,%d) called", oldrow, oldcol, newrow, newcol));

	if(!cursor_address)
		return ERR;

	newrow %= lines;
	newcol %= columns;

	if (cursor_address)
		putp(tparm(cursor_address, newrow, newcol));
	return OK;
		
}

#else

#define BUFSIZE	128			/* size of strategy buffer */

struct Sequence
{
	int	vec[BUFSIZE];	/* vector of operations */
	int	*end;		/* end of vector */
	int	cost;		/* cost of vector */
};

static void row(struct Sequence *outseq, int orow, int nrow);
static void column(struct Sequence *outseq, int ocol, int ncol);
static void simp_col(struct Sequence *outseq, int oc, int nc);
static void zero_seq(struct Sequence *seq);
static void add_seq(struct Sequence *seq1, struct Sequence *seq2);
static void out_seq(struct Sequence *seq);
static void update_ops(void);
static void init_costs(int costs[]);
static int countc(char ch);
static void add_op(struct Sequence *seq, int op, ...);
static char *sequence(int op);

static int c_count;			/* used for counting tputs output */

#define	INFINITY	1000		/* biggest, impossible sequence cost */
#define NUM_OPS		16		/* num. term. control sequences */
#define NUM_NPARM	9		/* num. ops wo/ parameters */

	/* operator indexes into op_info */

#define	CARRIAGE_RETURN	0		/* watch out for nl mapping */
#define	CURS_DOWN	1
#define	CURS_HOME	2
#define	CURS_LEFT	3
#define	CURS_RIGHT	4
#define	CURS_TO_LL	5
#define	CURS_UP		6
#define	TAB		7
#define	BACK_TAB	8
#define	ROW_ADDR	9
#define	COL_ADDR	10
#define	P_DOWN_CURS	11
#define	P_LEFT_CURS	12
#define	P_RIGHT_CURS	13
#define	P_UP_CURS	14
#define	CURS_ADDR	15

static bool	loc_init = FALSE;	/* set if op_info is init'ed */

static bool	rel_ok;			/* set if we really know where we are */

/*
 *	op_info[NUM_OPS]
 *
 *	op_info[] contains for operations with no parameters
 *	the cost of the operation.  These ops should be first in the array.
 *	For operations with parameters, op_info[] contains
 *	the negative of the number of parameters.
 */

static int	op_info[NUM_OPS] = {
	0,		/* carriage_return */
	0,		/* cursor_down */
	0,		/* cursor_home */
	0,		/* cursor_left */
	0,		/* cursor_right */
	0,		/* cursor_to_ll */
	0,		/* cursor_up */
	0,		/* tab */
	0,		/* back_tab */
	-1,		/* row_address */
	-1,		/* column_address */
	-1,		/* parm_down_cursor */
	-1,		/* parm_left_cursor */
	-1,		/* parm_right_cursor */
	-1,		/* parm_up_cursor */
	-2		/* cursor_address */
};

/*
**	Make_seq_best(best, try)
**	
**	Make_seq_best() copies try to best if try->cost < best->cost
**
**	fixed the old version, now it really runs .. (-hm/08.04.93)
**
*/

inline void Make_seq_best(struct Sequence *best, struct Sequence *try)
{
	if (best->cost > try->cost) {
		register int *sptr;

		sptr = try->vec;			/* src ptr */
		best->end = best->vec;			/* dst ptr */
		while(sptr != try->end)			/* copy src -> dst */
			*(best->end++) = *(sptr++);
		best->cost = try->cost;			/* copy cost */
	}
}


/*
**
**	mvcur(oldrow, oldcol, newrow, newcol)
**
**	mvcur() optimally moves the cursor from the position
**	specified by (oldrow, oldcol) to (newrow, newcol).  If
**	(oldrow, oldcol) == (-1, -1), mvcur() does not use relative
**	cursor motions.  If the coordinates are otherwise
**	out of bounds, it mods them into range.
**
**	Revisions needed:
**		eat_newline_glitch, auto_right_margin
*/

int mvcur(int oldrow, int oldcol, int newrow, int newcol)
{
struct Sequence	seqA, seqB,	/* allocate work structures */
		col0seq,	/* sequence to get from col0 to nc */
		*best,		/* best sequence so far */
		*try;		/* next try */
bool		nlstat = SP->_nl; /* nl-output-mapping in effect ?*/

	T(("=============================\nmvcur(%d,%d,%d,%d) called",
	   oldrow, oldcol, newrow, newcol));

	if ((oldrow == newrow) && (oldcol == newcol))
		return OK;
		
	if (oldcol == columns-1 && eat_newline_glitch && auto_right_margin) {
		putp(tparm(cursor_address, newrow, newcol));
		return OK;
	}

#if 0
	if (nlstat)
		nonl();
#endif
	update_ops();			/* make sure op_info[] is current */

	if (oldrow < 0  ||  oldcol < 0 || (eat_newline_glitch && oldcol == 0 )) {
		rel_ok = FALSE;		/* relative ops ok? */
	} else {
		rel_ok = TRUE;
		oldrow %= lines;	/* mod values into range */
		oldcol %= columns;
	}
	
	newrow %= lines;
	newcol %= columns;

	best = &seqA;
	try = &seqB;

	/* try out direct cursor addressing */

	zero_seq(best);
	add_op(best, CURS_ADDR, newrow, newcol);

	/* try out independent row/column addressing */

	if(rel_ok) {
		zero_seq(try);
		row(try, oldrow, newrow);
		column(try, oldcol, newcol);
		Make_seq_best(best, try);
	}

	zero_seq(&col0seq);		/* store seq. to get from c0 to nc */
	column(&col0seq, 0, newcol);

	if(col0seq.cost < INFINITY) {	/* can get from col0 to newcol */

		/* try out homing and then row/column */

		if (! rel_ok  ||  newcol < oldcol  ||  newrow < oldrow) {
			zero_seq(try);
			add_op(try, CURS_HOME, 1);
			row(try, 0, newrow);
			add_seq(try, &col0seq);
			Make_seq_best(best, try);
		}

		/* try out homing to last line  and then row/column */

		if (! rel_ok  ||  newcol < oldcol  ||  newrow > oldrow) {
			zero_seq(try);
			add_op(try, CURS_TO_LL, 1);
			row(try, lines - 1, newrow);
			add_seq(try, &col0seq);
			Make_seq_best(best, try);
		}
	}

	out_seq(best);
#if 0
	if(nlstat)
		nl();
#endif

	T(("==================================="));

	return OK;
}

/*
**	row(outseq, oldrow, newrow)
**
**	row() adds the best sequence for moving
**  		the cursor from oldrow to newrow to seq.
**	row() considers row_address, parm_up/down_cursor
**  		and cursor_up/down.
*/

static void
row(struct Sequence *outseq,	/* where to put the output */
int orow, int nrow)		/* old, new cursor locations */
{
struct Sequence	seqA, seqB,
		*best,		/* best sequence so far */
		*try;		/* next try */

int	parm_cursor, one_step;

	best = &seqA;
	try = &seqB;

	if (nrow == orow)
		return;

	if (nrow < orow) {
		parm_cursor = P_UP_CURS;
		one_step = CURS_UP;
	} else {
		parm_cursor = P_DOWN_CURS;
		one_step = CURS_DOWN;
	}

	/* try out direct row addressing */

	zero_seq(best);
	add_op(best, ROW_ADDR, nrow);

	/* try out paramaterized up or down motion */

	if (rel_ok) {
		zero_seq(try);
		add_op(try, parm_cursor, abs(orow - nrow));
		Make_seq_best(best, try);
	}

	/* try getting there one step at a time... */

	if (rel_ok) {
		zero_seq(try);
		add_op(try, one_step, abs(orow-nrow));
		Make_seq_best(best, try);
	}

	add_seq(outseq, best);
}


/*
**	column(outseq, oldcol, newcol)
**
**	column() adds the best sequence for moving
**		the cursor from oldcol to newcol to outseq.
**	column() considers column_address, parm_left/right_cursor,
**		simp_col(), and carriage_return followed by simp_col().
*/

static void column(struct Sequence *outseq,	/* where to put the output */
int ocol, int ncol)		/* old, new cursor  column */
{
struct Sequence	seqA, seqB,
		*best, *try;
int		parm_cursor;	/* set to either parm_up/down_cursor */

	best = &seqA;
	try = &seqB;

	if (ncol == ocol)
		return;

	if (ncol < ocol)
		parm_cursor = P_LEFT_CURS;
	else
		parm_cursor = P_RIGHT_CURS;

	/* try out direct column addressing */

	zero_seq(best);
	add_op(best, COL_ADDR, ncol);

	/* try carriage_return then simp_col() */

	if(! rel_ok  ||  (ncol < ocol)) {
		zero_seq(try);
		add_op(try, CARRIAGE_RETURN, 1);
		simp_col(try, 0, ncol);
		Make_seq_best(best, try);
	}
	if(rel_ok) {
		/* try out paramaterized left or right motion */

		zero_seq(try);
		add_op(try, parm_cursor, abs(ocol - ncol));
		Make_seq_best(best, try);

		/* try getting there with simp_col() */

		zero_seq(try);
		simp_col(try, ocol, ncol);
		Make_seq_best(best, try);
	}

	add_seq(outseq, best);
}


/*
** 	simp_col(outseq, oldcol, newcol)
**
**	simp_col() adds the best simple sequence for getting
**		from oldcol to newcol to outseq.
**	simp_col() considers (back_)tab and cursor_left/right.
**
**  Revisions needed:
**	Simp_col asssumes that the cost of a (back_)tab
**	is less then the cost of one-stepping to get to the same column.
**	Should sometimes use overprinting instead of cursor_right.
*/

static void
simp_col( struct Sequence *outseq,		/* place to put sequence */
int oc, int nc)				/* old column, new column */
{
struct Sequence	seqA, seqB, tabseq,
		*best, *try;
int	mytab, tabs, onepast,
		one_step, opp_step; 

	onepast = -1;
	
	if (oc == nc)
		return;

	if(! rel_ok) {
		outseq->cost = INFINITY;
		return;
	}

	best = &seqA;
	try  = &seqB;

	if(oc < nc) {
		mytab = TAB;

		if (init_tabs > 0  &&  op_info[TAB] < INFINITY) {
			tabs = (nc / init_tabs) - (oc / init_tabs);
			onepast = ((nc / init_tabs) + 1) * init_tabs;
			if (tabs)
				oc = onepast - init_tabs; /* consider it done */
		} else {
			tabs = 0;
		}
		one_step = CURS_RIGHT;
		opp_step = CURS_LEFT;
	} else {
		mytab = BACK_TAB;
		if (init_tabs > 0  &&  op_info[BACK_TAB] < INFINITY) {
			tabs = (oc / init_tabs) - (nc / init_tabs);
			onepast = ((nc - 1) / init_tabs) * init_tabs;
			if (tabs)
				oc = onepast + init_tabs; /* consider it done */
		} else {
			tabs = 0;
		}
		one_step = CURS_LEFT;
		opp_step = CURS_RIGHT;
	}

	/* tab as close as possible to nc */

	zero_seq(&tabseq);
	add_op(&tabseq, mytab, tabs);

	/* try extra tab and backing up */

	zero_seq(best);

	if (onepast >= 0  &&  onepast < columns) {
		add_op(best, mytab, 1);
		add_op(best, opp_step, abs(onepast - nc));
	} else {
		best->cost = INFINITY;	/* make sure of next swap */
	}

	/* try stepping to nc */

	zero_seq(try);
	add_op(try, one_step, abs(nc - oc));
	Make_seq_best(best, try);
	
	if (tabseq.cost < INFINITY)
		add_seq(outseq, &tabseq);
	add_seq(outseq, best);
}


/*
**	zero_seq(seq) empties seq.
**	add_seq(seq1, seq2) adds seq1 to seq2.
**	out_seq(seq) outputs a sequence.
*/

static void
zero_seq(seq)
struct Sequence	*seq;
{
	seq->end = seq->vec;
	seq->cost = 0;
}

static void
add_seq(struct Sequence	*seq1, struct Sequence *seq2)
{
int	*vptr;

	T(("add_seq(%x, %x)", seq1, seq2));

	if(seq1->cost >= INFINITY  ||  seq2->cost >= INFINITY)
		seq1->cost = INFINITY;
	else {
		vptr = seq2->vec;
		while (vptr != seq2->end)
		*(seq1->end++) = *(vptr++);
		seq1->cost += seq2->cost;
	}
}


static void
out_seq(struct Sequence *seq)
{
int	*opptr, prm[9], ps, p, op;
int	count;
char	*sequence();

	T(("out_seq(%x)", seq));

	if (seq->cost >= INFINITY)
		return;

	for (opptr = seq->vec;  opptr < seq->end;  opptr++) {
		op = *opptr;			/* grab operator */
		ps = -op_info[op];
		if(ps > 0) {				/* parameterized */
			for (p = 0;  p < ps;  p++)	/* fill in needed parms */
				prm[p] = *(++opptr);

			putp(tparm(sequence(op),
				prm[0], prm[1], prm[2], prm[3], prm[4],
				prm[5], prm[6], prm[7], prm[8]));
		} else {
			count = *(++opptr);
			/*rev should save tputs output instead of mult calls */
			while (count--)			/* do count times */
				putp(sequence(op));
		}
	}
}


/*
**	update_ops()
**
**	update_ops() makes sure that
**	the op_info[] array is updated and initializes
**	the cost array for SP if needed.
*/

static void
update_ops()
{
	T(("update_ops()"));

	if (SP) {			/* SP structure exists */
	int op; 

		if (! SP->_costinit) {	/* this term not yet assigned costs */
			loc_init = FALSE;	/* if !SP in the future, new term */
			init_costs(SP->_costs);	/* fill term costs */
			SP->_costinit = TRUE;
		}

		for (op = 0;  op < NUM_NPARM;  op++)
			op_info[op] = SP->_costs[op];	/* set up op_info */
		
		/* check for newline that might be mapped... */

		if (SP->_nlmapping  &&  index(sequence(CURS_DOWN), '\n'))
			op_info[CURS_DOWN] = INFINITY;
	} else {
		if (! loc_init) {		/* using local costs */
			loc_init = TRUE;
			init_costs(op_info);		/* set up op_info */
		}

		/* check for newline that might be mapped... */

		if (index(sequence(CURS_DOWN), '\n'))
			op_info[CURS_DOWN] = INFINITY;
	}
}


/*
**	init_costs(costs)
**
**	init_costs() fills the array costs[NUM_NPARM]
** 	with costs calculated by doing tputs() calls.
*/

static void
init_costs(int costs[])
{
int	i;

	for (i = 0;  i < NUM_NPARM;  i++) {
		if (sequence(i) != (char *) 0) {
			c_count = 0;
			tputs(sequence(i), 1, countc);
			costs[i] = c_count;
		} else
			costs[i] = INFINITY;
	}
}


/*
**	countc() increments global var c_count.
*/

static int countc(char ch)
{
	return(c_count++);
}

/*
**	add_op(seq, op, p0, p1, ... , p8)
**
**	add_op() adds the operator op and the appropriate
**  	number of paramaters to seq.  It also increases the 
**  	cost appropriately.
**	if op has no parameters, p0 is taken to be a count.
*/

static void add_op(struct Sequence *seq, int op, ...)
{
va_list	argp;
int	num_ps, p;
	
	T(("adding op %d to sequence", op));

	va_start(argp, op);
	
	num_ps = - op_info[op];		/* get parms or -cost */

	*(seq->end++) = op;

	if (num_ps == (- INFINITY)  ||  sequence(op) == (char *) 0) {
		seq->cost = INFINITY;
	} else if (num_ps <= 0) {		/* no parms, -cost */
		int i = va_arg(argp, int);
		seq->cost -= i * num_ps;	/* ADD count * cost */
		*(seq->end++) = i;
	} else {
	int prm[9];
		 
		for (p = 0;  p < num_ps;  p++)
			*(seq->end++) = prm[p] = va_arg(argp, int);

		c_count = 0;
		
		tputs(tparm(sequence(op), prm[0], prm[1], prm[2], prm[3], prm[4],
			    prm[5], prm[6], prm[7], prm[8]), 1, countc);

		seq->cost += c_count;
	}
	va_end(argp);
}


/*
**	char	*sequence(op)
**
**	sequence() returns a pointer to the op's
**      terminal control sequence.
*/

static char *sequence(int op)
{
	T(("sequence(%d)", op));

	switch(op) {
		case CARRIAGE_RETURN:
			return (carriage_return);
		case CURS_DOWN:
			return (cursor_down);
		case CURS_HOME:
			return (cursor_home);
		case CURS_LEFT:
			return (cursor_left);
		case CURS_RIGHT:
			return (cursor_right);
		case CURS_TO_LL:
			return (cursor_to_ll);
		case CURS_UP:
			return (cursor_up);
		case TAB:
			return (tab);
		case BACK_TAB:
			return (back_tab);
		case ROW_ADDR:
			return (row_address);
		case COL_ADDR:
			return (column_address);
		case P_DOWN_CURS:
			return (parm_down_cursor);
		case P_LEFT_CURS:
			return (parm_left_cursor);
		case P_RIGHT_CURS:
			return (parm_right_cursor);
		case P_UP_CURS:
			return (parm_up_cursor);
		case CURS_ADDR:
			return (cursor_address);
		default:
			return ((char *) 0);
	}
}

#endif

