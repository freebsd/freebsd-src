/****************************************************************************
 * Copyright (c) 1998 Free Software Foundation, Inc.                        *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/****************************************************************************
 *  Author: Zeyd M. Ben-Halim <zmbenhal@netcom.com> 1992,1995               *
 *     and: Eric S. Raymond <esr@snark.thyrsus.com>                         *
 ****************************************************************************/


/*
**	lib_mvcur.c
**
**	The routines for moving the physical cursor and scrolling:
**
**		void _nc_mvcur_init(void)
**
**		void _nc_mvcur_resume(void)
**
**		int mvcur(int old_y, int old_x, int new_y, int new_x)
**
**		void _nc_mvcur_wrap(void)
**
** Comparisons with older movement optimizers:
**    SVr3 curses mvcur() can't use cursor_to_ll or auto_left_margin.
**    4.4BSD curses can't use cuu/cud/cuf/cub/hpa/vpa/tab/cbt for local
** motions.  It doesn't use tactics based on auto_left_margin.  Weirdly
** enough, it doesn't use its own hardware-scrolling routine to scroll up
** destination lines for out-of-bounds addresses!
**    old ncurses optimizer: less accurate cost computations (in fact,
** it was broken and had to be commented out!).
**
** Compile with -DMAIN to build an interactive tester/timer for the movement
** optimizer.  You can use it to investigate the optimizer's behavior.
** You can also use it for tuning the formulas used to determine whether
** or not full optimization is attempted.
**
** This code has a nasty tendency to find bugs in terminfo entries, because it
** exercises the non-cup movement capabilities heavily.  If you think you've
** found a bug, try deleting subsets of the following capabilities (arranged
** in decreasing order of suspiciousness): it, tab, cbt, hpa, vpa, cuu, cud,
** cuf, cub, cuu1, cud1, cuf1, cub1.  It may be that one or more are wrong.
**
** Note: you should expect this code to look like a resource hog in a profile.
** That's because it does a lot of I/O, through the tputs() calls.  The I/O
** cost swamps the computation overhead (and as machines get faster, this
** will become even more true).  Comments in the test exerciser at the end
** go into detail about tuning and how you can gauge the optimizer's
** effectiveness.
**/

/****************************************************************************
 *
 * Constants and macros for optimizer tuning.
 *
 ****************************************************************************/

/*
 * The average overhead of a full optimization computation in character
 * transmission times.  If it's too high, the algorithm will be a bit
 * over-biased toward using cup rather than local motions; if it's too
 * low, the algorithm may spend more time than is strictly optimal
 * looking for non-cup motions.  Profile the optimizer using the `t'
 * command of the exerciser (see below), and round to the nearest integer.
 *
 * Yes, I (esr) thought about computing expected overhead dynamically, say
 * by derivation from a running average of optimizer times.  But the
 * whole point of this optimization is to *decrease* the frequency of
 * system calls. :-)
 */
#define COMPUTE_OVERHEAD	1	/* I use a 90MHz Pentium @ 9.6Kbps */

/*
 * LONG_DIST is the distance we consider to be just as costly to move over as a
 * cup sequence is to emit.  In other words, it's the length of a cup sequence
 * adjusted for average computation overhead.  The magic number is the length
 * of "\033[yy;xxH", the typical cup sequence these days.
 */
#define LONG_DIST		(8 - COMPUTE_OVERHEAD)

/*
 * Tell whether a motion is optimizable by local motions.  Needs to be cheap to
 * compute. In general, all the fast moves go to either the right or left edge
 * of the screen.  So any motion to a location that is (a) further away than
 * LONG_DIST and (b) further inward from the right or left edge than LONG_DIST,
 * we'll consider nonlocal.
 */
#define NOT_LOCAL(fy, fx, ty, tx)	((tx > LONG_DIST) && (tx < screen_lines - 1 - LONG_DIST) && (abs(ty-fy) + abs(tx-fx) > LONG_DIST))

/****************************************************************************
 *
 * External interfaces
 *
 ****************************************************************************/

/*
 * For this code to work OK, the following components must live in the
 * screen structure:
 *
 *	int		_char_padding;	// cost of character put
 *	int		_cr_cost;	// cost of (carriage_return)
 *	int		_cup_cost;	// cost of (cursor_address)
 *	int		_home_cost;	// cost of (cursor_home)
 *	int		_ll_cost;	// cost of (cursor_to_ll)
 *#if USE_HARD_TABS
 *	int		_ht_cost;	// cost of (tab)
 *	int		_cbt_cost;	// cost of (back_tab)
 *#endif USE_HARD_TABS
 *	int		_cub1_cost;	// cost of (cursor_left)
 *	int		_cuf1_cost;	// cost of (cursor_right)
 *	int		_cud1_cost;	// cost of (cursor_down)
 *	int		_cuu1_cost;	// cost of (cursor_up)
 *	int		_cub_cost;	// cost of (parm_cursor_left)
 *	int		_cuf_cost;	// cost of (parm_cursor_right)
 *	int		_cud_cost;	// cost of (parm_cursor_down)
 *	int		_cuu_cost;	// cost of (parm_cursor_up)
 *	int		_hpa_cost;	// cost of (column_address)
 *	int		_vpa_cost;	// cost of (row_address)
 *	int		_ech_cost;	// cost of (erase_chars)
 *	int		_rep_cost;	// cost of (repeat_char)
 *
 * The USE_HARD_TABS switch controls whether it is reliable to use tab/backtabs
 * for local motions.  On many systems, it's not, due to uncertainties about
 * tab delays and whether or not tabs will be expanded in raw mode.  If you
 * have parm_right_cursor, tab motions don't win you a lot anyhow.
 */

#include <curses.priv.h>
#include <term.h>
#include <ctype.h>

MODULE_ID("$Id: lib_mvcur.c,v 1.60 1999/10/03 01:08:27 Alexander.V.Lukyanov Exp $")

#define STRLEN(s)       (s != 0) ? strlen(s) : 0

#define CURRENT_ATTR	SP->_current_attr	/* current phys attribute */
#define CURRENT_ROW	SP->_cursrow		/* phys cursor row */
#define CURRENT_COLUMN	SP->_curscol		/* phys cursor column */
#define REAL_ATTR	SP->_current_attr	/* phys current attribute */
#define WANT_CHAR(y, x)	SP->_newscr->_line[y].text[x]	/* desired state */
#define BAUDRATE	cur_term->_baudrate	/* bits per second */

#if defined(MAIN) || defined(NCURSES_TEST)
#include <sys/time.h>

static bool profiling = FALSE;
static float diff;
#endif /* MAIN */

#define OPT_SIZE 512

static int normalized_cost(const char *const cap, int affcnt);

#if !HAVE_STRSTR
char * _nc_strstr(const char *haystack, const char *needle)
{
	size_t len1 = strlen(haystack);
	size_t len2 = strlen(needle);
	char *result = 0;

	while ((len1 != 0) && (len1-- >= len2)) {
		if (!strncmp(haystack, needle, len2)) {
			result = haystack;
			break;
		}
		haystack++;
	}
	return result;
}
#endif

/****************************************************************************
 *
 * Initialization/wrapup (including cost pre-computation)
 *
 ****************************************************************************/

#ifdef TRACE
static int
trace_cost_of(const char *capname, const char *cap, int affcnt)
{
	int result = _nc_msec_cost(cap,affcnt);
	TR(TRACE_CHARPUT|TRACE_MOVE, ("CostOf %s %d", capname, result));
	return result;
}
#define CostOf(cap,affcnt) trace_cost_of(#cap,cap,affcnt);

static int
trace_normalized_cost(const char *capname, const char *cap, int affcnt)
{
	int result = normalized_cost(cap,affcnt);
	TR(TRACE_CHARPUT|TRACE_MOVE, ("NormalizedCost %s %d", capname, result));
	return result;
}
#define NormalizedCost(cap,affcnt) trace_normalized_cost(#cap,cap,affcnt);

#else

#define CostOf(cap,affcnt) _nc_msec_cost(cap,affcnt);
#define NormalizedCost(cap,affcnt) normalized_cost(cap,affcnt);

#endif

int _nc_msec_cost(const char *const cap, int affcnt)
/* compute the cost of a given operation */
{
    if (cap == 0)
	return(INFINITY);
    else
    {
	const	char	*cp;
	float	cum_cost = 0;

	for (cp = cap; *cp; cp++)
	{
	    /* extract padding, either mandatory or required */
	    if (cp[0] == '$' && cp[1] == '<' && strchr(cp, '>'))
	    {
		float	number = 0;

		for (cp += 2; *cp != '>'; cp++)
		{
		    if (isdigit(*cp))
			number = number * 10 + (*cp - '0');
		    else if (*cp == '*')
			number *= affcnt;
		    else if (*cp == '.' && (*++cp != '>') && isdigit(*cp))
			number += (*cp - '0') / 10.0;
		}

		cum_cost += number * 10;
	    }
	    else
		cum_cost += SP->_char_padding;
	}

	return((int)cum_cost);
    }
}

static int normalized_cost(const char *const cap, int affcnt)
/* compute the effective character-count for an operation (round up) */
{
	int cost = _nc_msec_cost(cap, affcnt);
	if (cost != INFINITY)
		cost = (cost + SP->_char_padding - 1) / SP->_char_padding;
	return cost;
}

static void reset_scroll_region(void)
/* Set the scroll-region to a known state (the default) */
{
    if (change_scroll_region)
    {
	TPUTS_TRACE("change_scroll_region");
	putp(tparm(change_scroll_region, 0, screen_lines - 1));
    }
}

void _nc_mvcur_resume(void)
/* what to do at initialization time and after each shellout */
{
    /* initialize screen for cursor access */
    if (enter_ca_mode)
    {
	TPUTS_TRACE("enter_ca_mode");
	putp(enter_ca_mode);
    }

    /*
     * Doing this here rather than in _nc_mvcur_wrap() ensures that
     * ncurses programs will see a reset scroll region even if a
     * program that messed with it died ungracefully.
     *
     * This also undoes the effects of terminal init strings that assume
     * they know the screen size.  This is useful when you're running
     * a vt100 emulation through xterm.
     */
    reset_scroll_region();
    SP->_cursrow = SP->_curscol = -1;

    /* restore cursor shape */
    if (SP->_cursor != -1)
    {
	int cursor = SP->_cursor;
	SP->_cursor = -1;
	curs_set (cursor);
    }
}

void _nc_mvcur_init(void)
/* initialize the cost structure */
{
    /*
     * 9 = 7 bits + 1 parity + 1 stop.
     */
    SP->_char_padding = (9 * 1000 * 10) / (BAUDRATE > 0 ? BAUDRATE : 9600);
    if (SP->_char_padding <= 0)
	SP->_char_padding = 1;	/* must be nonzero */
    TR(TRACE_CHARPUT|TRACE_MOVE, ("char_padding %d msecs", SP->_char_padding));

    /* non-parameterized local-motion strings */
    SP->_cr_cost   = CostOf(carriage_return, 0);
    SP->_home_cost = CostOf(cursor_home, 0);
    SP->_ll_cost   = CostOf(cursor_to_ll, 0);
#if USE_HARD_TABS
    SP->_ht_cost   = CostOf(tab, 0);
    SP->_cbt_cost  = CostOf(back_tab, 0);
#endif /* USE_HARD_TABS */
    SP->_cub1_cost = CostOf(cursor_left, 0);
    SP->_cuf1_cost = CostOf(cursor_right, 0);
    SP->_cud1_cost = CostOf(cursor_down, 0);
    SP->_cuu1_cost = CostOf(cursor_up, 0);

    SP->_smir_cost = CostOf(enter_insert_mode, 0);
    SP->_rmir_cost = CostOf(exit_insert_mode, 0);
    SP->_ip_cost = 0;
    if (insert_padding) {
	SP->_ip_cost = CostOf(insert_padding, 0);
    }

    /*
     * Assumption: if the terminal has memory_relative addressing, the
     * initialization strings or smcup will set single-page mode so we
     * can treat it like absolute screen addressing.  This seems to be true
     * for all cursor_mem_address terminal types in the terminfo database.
     */
    SP->_address_cursor = cursor_address ? cursor_address : cursor_mem_address;

    /*
     * Parametrized local-motion strings.  This static cost computation
     * depends on the following assumptions:
     *
     * (1) They never have * padding.  In the entire master terminfo database
     *     as of March 1995, only the obsolete Zenith Z-100 pc violates this.
     *	   (Proportional padding is found mainly in insert, delete and scroll
     *     capabilities).
     *
     * (2) The average case of cup has two two-digit parameters.  Strictly,
     *     the average case for a 24 * 80 screen has ((10*10*(1 + 1)) +
     *     (14*10*(1 + 2)) + (10*70*(2 + 1)) + (14*70*4)) / (24*80) = 3.458
     *     digits of parameters.  On a 25x80 screen the average is 3.6197.
     *     On larger screens the value gets much closer to 4.
     *
     * (3) The average case of cub/cuf/hpa/ech/rep has 2 digits of parameters
     *     (strictly, (((10 * 1) + (70 * 2)) / 80) = 1.8750).
     *
     * (4) The average case of cud/cuu/vpa has 2 digits of parameters
     *     (strictly, (((10 * 1) + (14 * 2)) / 24) = 1.5833).
     *
     * All these averages depend on the assumption that all parameter values
     * are equally probable.
     */
    SP->_cup_cost  = CostOf(tparm(SP->_address_cursor, 23, 23), 1);
    SP->_cub_cost  = CostOf(tparm(parm_left_cursor, 23), 1);
    SP->_cuf_cost  = CostOf(tparm(parm_right_cursor, 23), 1);
    SP->_cud_cost  = CostOf(tparm(parm_down_cursor, 23), 1);
    SP->_cuu_cost  = CostOf(tparm(parm_up_cursor, 23), 1);
    SP->_hpa_cost  = CostOf(tparm(column_address, 23), 1);
    SP->_vpa_cost  = CostOf(tparm(row_address, 23), 1);

    /* non-parameterized screen-update strings */
    SP->_ed_cost   = NormalizedCost(clr_eos, 1);
    SP->_el_cost   = NormalizedCost(clr_eol, 1);
    SP->_el1_cost  = NormalizedCost(clr_bol, 1);
    SP->_dch1_cost = NormalizedCost(delete_character, 1);
    SP->_ich1_cost = NormalizedCost(insert_character, 1);

    /* parameterized screen-update strings */
    SP->_dch_cost  = NormalizedCost(tparm(parm_dch, 23), 1);
    SP->_ich_cost  = NormalizedCost(tparm(parm_ich, 23), 1);
    SP->_ech_cost  = NormalizedCost(tparm(erase_chars, 23), 1);
    SP->_rep_cost  = NormalizedCost(tparm(repeat_char, ' ', 23), 1);

    SP->_cup_ch_cost = NormalizedCost(tparm(SP->_address_cursor, 23, 23), 1);
    SP->_hpa_ch_cost = NormalizedCost(tparm(column_address, 23), 1);

    /* pre-compute some capability lengths */
    SP->_carriage_return_length = STRLEN(carriage_return);
    SP->_cursor_home_length     = STRLEN(cursor_home);
    SP->_cursor_to_ll_length    = STRLEN(cursor_to_ll);

    /*
     * If save_cursor is used within enter_ca_mode, we should not use it for
     * scrolling optimization, since the corresponding restore_cursor is not
     * nested on the various terminals (vt100, xterm, etc.) which use this
     * feature.
     */
    if (save_cursor != 0
     && enter_ca_mode != 0
     && strstr(enter_ca_mode, save_cursor) != 0) {
	T(("...suppressed sc/rc capability due to conflict with smcup/rmcup"));
	save_cursor = 0;
	restore_cursor = 0;
    }

    /*
     * A different, possibly better way to arrange this would be to set
     * SP->_endwin = TRUE at window initialization time and let this be
     * called by doupdate's return-from-shellout code.
     */
    _nc_mvcur_resume();
}

void _nc_mvcur_wrap(void)
/* wrap up cursor-addressing mode */
{
    /* leave cursor at screen bottom */
    mvcur(-1, -1, screen_lines - 1, 0);

    /* set cursor to normal mode */
    if (SP->_cursor != -1)
	curs_set(1);

    if (exit_ca_mode)
    {
	TPUTS_TRACE("exit_ca_mode");
	putp(exit_ca_mode);
    }
    /*
     * Reset terminal's tab counter.  There's a long-time bug that
     * if you exit a "curses" program such as vi or more, tab
     * forward, and then backspace, the cursor doesn't go to the
     * right place.  The problem is that the kernel counts the
     * escape sequences that reset things as column positions.
     * Utter a \r to reset this invisibly.
     */
    _nc_outch('\r');
}

/****************************************************************************
 *
 * Optimized cursor movement
 *
 ****************************************************************************/

/*
 * Perform repeated-append, returning cost
 */
static inline int
repeated_append (int total, int num, int repeat, char *dst, const char *src)
{
	register size_t src_len = strlen(src);
	register size_t dst_len = STRLEN(dst);

	if ((dst_len + repeat * src_len) < OPT_SIZE-1) {
		total += (num * repeat);
		if (dst) {
		    dst += dst_len;
		    while (repeat-- > 0) {
			(void) strcpy(dst, src);
			dst += src_len;
		    }
		}
	} else {
		total = INFINITY;
	}
	return total;
}

#ifndef NO_OPTIMIZE
#define NEXTTAB(fr)	(fr + init_tabs - (fr % init_tabs))

/*
 * Assume back_tab (CBT) does not wrap backwards at the left margin, return
 * a negative value at that point to simplify the loop.
 */
#define LASTTAB(fr)	((fr > 0) ? ((fr - 1) / init_tabs) * init_tabs : -1)

/* Note: we'd like to inline this for speed, but GNU C barfs on the attempt. */

static int
relative_move(char *result, int from_y,int from_x,int to_y,int to_x, bool ovw)
/* move via local motions (cuu/cuu1/cud/cud1/cub1/cub/cuf1/cuf/vpa/hpa) */
{
    int		n, vcost = 0, hcost = 0;

    if (result)
	result[0] = '\0';

    if (to_y != from_y)
    {
	vcost = INFINITY;

	if (row_address)
	{
	    if (result)
		(void) strcpy(result, tparm(row_address, to_y));
	    vcost = SP->_vpa_cost;
	}

	if (to_y > from_y)
	{
	    n = (to_y - from_y);

	    if (parm_down_cursor && SP->_cud_cost < vcost)
	    {
		if (result)
		    (void) strcpy(result, tparm(parm_down_cursor, n));
		vcost = SP->_cud_cost;
	    }

	    if (cursor_down && (n * SP->_cud1_cost < vcost))
	    {
		if (result)
		    result[0] = '\0';
		vcost = repeated_append(0, SP->_cud1_cost, n, result, cursor_down);
	    }
	}
	else /* (to_y < from_y) */
	{
	    n = (from_y - to_y);

	    if (parm_up_cursor && SP->_cup_cost < vcost)
	    {
		if (result)
		    (void) strcpy(result, tparm(parm_up_cursor, n));
		vcost = SP->_cup_cost;
	    }

	    if (cursor_up && (n * SP->_cuu1_cost < vcost))
	    {
		if (result)
		    result[0] = '\0';
		vcost = repeated_append(0, SP->_cuu1_cost, n, result, cursor_up);
	    }
	}

	if (vcost == INFINITY)
	    return(INFINITY);
    }

    if (result)
	result += strlen(result);

    if (to_x != from_x)
    {
	char	str[OPT_SIZE];

	hcost = INFINITY;

	if (column_address)
	{
	    if (result)
		(void) strcpy(result, tparm(column_address, to_x));
	    hcost = SP->_hpa_cost;
	}

	if (to_x > from_x)
	{
	    n = to_x - from_x;

	    if (parm_right_cursor && SP->_cuf_cost < hcost)
	    {
		if (result)
		    (void) strcpy(result, tparm(parm_right_cursor, n));
		hcost = SP->_cuf_cost;
	    }

	    if (cursor_right)
	    {
		int	lhcost = 0;

		str[0] = '\0';

#if USE_HARD_TABS
		/* use hard tabs, if we have them, to do as much as possible */
		if (init_tabs > 0 && tab)
		{
		    int	nxt, fr;

		    for (fr = from_x; (nxt = NEXTTAB(fr)) <= to_x; fr = nxt)
		    {
			lhcost = repeated_append(lhcost, SP->_ht_cost, 1, str, tab);
			if (lhcost == INFINITY)
				break;
		    }

		    n = to_x - fr;
		    from_x = fr;
		}
#endif /* USE_HARD_TABS */

#if defined(REAL_ATTR) && defined(WANT_CHAR)
#ifdef BSD_TPUTS
		/*
		 * If we're allowing BSD-style padding in tputs, don't generate
		 * a string with a leading digit.  Otherwise, that will be
		 * interpreted as a padding value rather than sent to the
		 * screen.
		 */
		if (ovw
		 && n > 0
		 && vcost == 0
		 && str[0] == '\0'
		 && isdigit(TextOf(WANT_CHAR(to_y, from_x))))
			ovw = FALSE;
#endif
		/*
		 * If we have no attribute changes, overwrite is cheaper.
		 * Note: must suppress this by passing in ovw = FALSE whenever
		 * WANT_CHAR would return invalid data.  In particular, this
		 * is true between the time a hardware scroll has been done
		 * and the time the structure WANT_CHAR would access has been
		 * updated.
		 */
		if (ovw)
		{
		    int	i;

		    for (i = 0; i < n; i++)
			if ((WANT_CHAR(to_y, from_x + i) & A_ATTRIBUTES) != CURRENT_ATTR)
			{
			    ovw = FALSE;
			    break;
			}
		}
		if (ovw)
		{
		    char	*sp;
		    int	i;

		    sp = str + strlen(str);

		    for (i = 0; i < n; i++)
			*sp++ = WANT_CHAR(to_y, from_x + i);
		    *sp = '\0';
		    lhcost += n * SP->_char_padding;
		}
		else
#endif /* defined(REAL_ATTR) && defined(WANT_CHAR) */
		{
		    lhcost = repeated_append(lhcost, SP->_cuf1_cost, n, str, cursor_right);
		}

		if (lhcost < hcost)
		{
		    if (result)
			(void) strcpy(result, str);
		    hcost = lhcost;
		}
	    }
	}
	else /* (to_x < from_x) */
	{
	    n = from_x - to_x;

	    if (parm_left_cursor && SP->_cub_cost < hcost)
	    {
		if (result)
		    (void) strcpy(result, tparm(parm_left_cursor, n));
		hcost = SP->_cub_cost;
	    }

	    if (cursor_left)
	    {
		int	lhcost = 0;

		str[0] = '\0';

#if USE_HARD_TABS
		if (init_tabs > 0 && back_tab)
		{
		    int	nxt, fr;

		    for (fr = from_x; (nxt = LASTTAB(fr)) >= to_x; fr = nxt)
		    {
			lhcost = repeated_append(lhcost, SP->_cbt_cost, 1, str, back_tab);
			if (lhcost == INFINITY)
				break;
		    }

		    n = fr - to_x;
		}
#endif /* USE_HARD_TABS */

		lhcost = repeated_append(lhcost, SP->_cub1_cost, n, str, cursor_left);

		if (lhcost < hcost)
		{
		    if (result)
			(void) strcpy(result, str);
		    hcost = lhcost;
		}
	    }
	}

	if (hcost == INFINITY)
	    return(INFINITY);
    }

    return(vcost + hcost);
}
#endif /* !NO_OPTIMIZE */

/*
 * With the machinery set up above, it's conceivable that
 * onscreen_mvcur could be modified into a recursive function that does
 * an alpha-beta search of motion space, as though it were a chess
 * move tree, with the weight function being boolean and the search
 * depth equated to length of string.  However, this would jack up the
 * computation cost a lot, especially on terminals without a cup
 * capability constraining the search tree depth.  So we settle for
 * the simpler method below.
 */

static inline int
onscreen_mvcur(int yold,int xold,int ynew,int xnew, bool ovw)
/* onscreen move from (yold, xold) to (ynew, xnew) */
{
    char	use[OPT_SIZE], *sp;
    int		tactic = 0, newcost, usecost = INFINITY;
    int		t5_cr_cost;

#if defined(MAIN) || defined(NCURSES_TEST)
    struct timeval before, after;

    gettimeofday(&before, NULL);
#endif /* MAIN */

    /* tactic #0: use direct cursor addressing */
    sp = tparm(SP->_address_cursor, ynew, xnew);
    if (sp)
    {
	tactic = 0;
	(void) strcpy(use, sp);
	usecost = SP->_cup_cost;

#if defined(TRACE) || defined(NCURSES_TEST)
	if (!(_nc_optimize_enable & OPTIMIZE_MVCUR))
	    goto nonlocal;
#endif /* TRACE */

	/*
	 * We may be able to tell in advance that the full optimization
	 * will probably not be worth its overhead.  Also, don't try to
	 * use local movement if the current attribute is anything but
	 * A_NORMAL...there are just too many ways this can screw up
	 * (like, say, local-movement \n getting mapped to some obscure
	 * character because A_ALTCHARSET is on).
	 */
	if (yold == -1 || xold == -1 || NOT_LOCAL(yold, xold, ynew, xnew))
	{
#if defined(MAIN) || defined(NCURSES_TEST)
	    if (!profiling)
	    {
		(void) fputs("nonlocal\n", stderr);
		goto nonlocal;	/* always run the optimizer if profiling */
	    }
#else
	    goto nonlocal;
#endif /* MAIN */
	}
    }

#ifndef NO_OPTIMIZE
    /* tactic #1: use local movement */
    if (yold != -1 && xold != -1
		&& ((newcost=relative_move(NULL, yold, xold, ynew, xnew, ovw))!=INFINITY)
		&& newcost < usecost)
    {
	tactic = 1;
	usecost = newcost;
    }

    /* tactic #2: use carriage-return + local movement */
    if (yold != -1 && carriage_return
		&& ((newcost=relative_move(NULL, yold,0,ynew,xnew, ovw)) != INFINITY)
		&& SP->_cr_cost + newcost < usecost)
    {
	tactic = 2;
	usecost = SP->_cr_cost + newcost;
    }

    /* tactic #3: use home-cursor + local movement */
    if (cursor_home
	&& ((newcost=relative_move(NULL, 0, 0, ynew, xnew, ovw)) != INFINITY)
	&& SP->_home_cost + newcost < usecost)
    {
	tactic = 3;
	usecost = SP->_home_cost + newcost;
    }

    /* tactic #4: use home-down + local movement */
    if (cursor_to_ll
	&& ((newcost=relative_move(NULL, screen_lines-1, 0, ynew, xnew, ovw)) != INFINITY)
	&& SP->_ll_cost + newcost < usecost)
    {
	tactic = 4;
	usecost = SP->_ll_cost + newcost;
    }

    /*
     * tactic #5: use left margin for wrap to right-hand side,
     * unless strange wrap behavior indicated by xenl might hose us.
     */
    t5_cr_cost = (xold>0 ? SP->_cr_cost : 0);
    if (auto_left_margin && !eat_newline_glitch
	&& yold > 0 && cursor_left
	&& ((newcost=relative_move(NULL, yold-1, screen_columns-1, ynew, xnew, ovw)) != INFINITY)
	&& t5_cr_cost + SP->_cub1_cost + newcost < usecost)
    {
	tactic = 5;
	usecost = t5_cr_cost + SP->_cub1_cost + newcost;
    }

    /*
     * These cases are ordered by estimated relative frequency.
     */
    if (tactic)
    {
	if (tactic == 1)
	    (void) relative_move(use, yold, xold, ynew, xnew, ovw);
	else if (tactic == 2)
	{
	    (void) strcpy(use, carriage_return);
	    (void) relative_move(use + SP->_carriage_return_length,
				 yold,0,ynew,xnew, ovw);
	}
	else if (tactic == 3)
	{
	    (void) strcpy(use, cursor_home);
	    (void) relative_move(use + SP->_cursor_home_length,
				 0, 0, ynew, xnew, ovw);
	}
	else if (tactic == 4)
	{
	    (void) strcpy(use, cursor_to_ll);
	    (void) relative_move(use + SP->_cursor_to_ll_length,
				 screen_lines-1, 0, ynew, xnew, ovw);
	}
	else /* if (tactic == 5) */
	{
	    use[0] = '\0';
	    if (xold > 0)
		(void) strcat(use, carriage_return);
	    (void) strcat(use, cursor_left);
	    (void) relative_move(use + strlen(use),
				 yold-1, screen_columns-1, ynew, xnew, ovw);
	}
    }
#endif /* !NO_OPTIMIZE */

#if defined(MAIN) || defined(NCURSES_TEST)
    gettimeofday(&after, NULL);
    diff = after.tv_usec - before.tv_usec
	+ (after.tv_sec - before.tv_sec) * 1000000;
    if (!profiling)
	(void) fprintf(stderr, "onscreen: %d msec, %f 28.8Kbps char-equivalents\n",
		       (int)diff, diff/288);
#endif /* MAIN */

 nonlocal:
    if (usecost != INFINITY)
    {
	TPUTS_TRACE("mvcur");
	tputs(use, 1, _nc_outch);
	return(OK);
    }
    else
	return(ERR);
}

int mvcur(int yold, int xold, int ynew, int xnew)
/* optimized cursor move from (yold, xold) to (ynew, xnew) */
{
    TR(TRACE_MOVE, ("mvcur(%d,%d,%d,%d) called", yold, xold, ynew, xnew));

    if (yold == ynew && xold == xnew)
	return(OK);

    /*
     * Most work here is rounding for terminal boundaries getting the
     * column position implied by wraparound or the lack thereof and
     * rolling up the screen to get ynew on the screen.
     */

    if (xnew >= screen_columns)
    {
	ynew += xnew / screen_columns;
	xnew %= screen_columns;
    }
    if (xold >= screen_columns)
    {
	int	l;

	l = (xold + 1) / screen_columns;
	yold += l;
	if (yold >= screen_lines)
		l -= (yold - screen_lines - 1);

	while (l > 0) {
		if (newline)
		{
			TPUTS_TRACE("newline");
			tputs(newline, 0, _nc_outch);
		}
		else
			putchar('\n');
		l--;
		if (xold > 0)
		{
			if (carriage_return)
			{
				TPUTS_TRACE("carriage_return");
				tputs(carriage_return, 0, _nc_outch);
			}
			else
				putchar('\r');
			xold = 0;
		}
	}
    }

    if (yold > screen_lines - 1)
	yold = screen_lines - 1;
    if (ynew > screen_lines - 1)
	ynew = screen_lines - 1;

    /* destination location is on screen now */
    return(onscreen_mvcur(yold, xold, ynew, xnew, TRUE));
}

#if defined(TRACE) || defined(NCURSES_TEST)
int _nc_optimize_enable = OPTIMIZE_ALL;
#endif

#if defined(MAIN) || defined(NCURSES_TEST)
/****************************************************************************
 *
 * Movement optimizer test code
 *
 ****************************************************************************/

#include <tic.h>
#include <dump_entry.h>

const char *_nc_progname = "mvcur";

static unsigned long xmits;

int tputs(const char *string, int affcnt GCC_UNUSED, int (*outc)(int) GCC_UNUSED)
/* stub tputs() that dumps sequences in a visible form */
{
    if (profiling)
	xmits += strlen(string);
    else
	(void) fputs(_nc_visbuf(string), stdout);
    return(OK);
}

int putp(const char *string)
{
    return(tputs(string, 1, _nc_outch));
}

int _nc_outch(int ch)
{
    putc(ch, stdout);
    return OK;
}

static char	tname[MAX_ALIAS];

static void load_term(void)
{
    (void) setupterm(tname, STDOUT_FILENO, NULL);
}

static int roll(int n)
{
    int i, j;

    i = (RAND_MAX / n) * n;
    while ((j = rand()) >= i)
	continue;
    return (j % n);
}

int main(int argc GCC_UNUSED, char *argv[] GCC_UNUSED)
{
    (void) strcpy(tname, termname());
    load_term();
    _nc_setupscreen(lines, columns, stdout);
    baudrate();

    _nc_mvcur_init();
    NC_BUFFERED(FALSE);

    (void) puts("The mvcur tester.  Type ? for help");

    fputs("smcup:", stdout);
    putchar('\n');

    for (;;)
    {
	int	fy, fx, ty, tx, n, i;
	char	buf[BUFSIZ], capname[BUFSIZ];

	(void) fputs("> ", stdout);
	(void) fgets(buf, sizeof(buf), stdin);

	if (buf[0] == '?')
	{
(void) puts("?                -- display this help message");
(void) puts("fy fx ty tx      -- (4 numbers) display (fy,fx)->(ty,tx) move");
(void) puts("s[croll] n t b m -- display scrolling sequence");
(void) printf("r[eload]         -- reload terminal info for %s\n", termname());
(void) puts("l[oad] <term>    -- load terminal info for type <term>");
(void) puts("d[elete] <cap>   -- delete named capability");
(void) puts("i[nspect]        -- display terminal capabilities");
(void) puts("c[ost]           -- dump cursor-optimization cost table");
(void) puts("o[optimize]      -- toggle movement optimization");
(void) puts("t[orture] <num>  -- torture-test with <num> random moves");
(void) puts("q[uit]           -- quit the program");
	}
	else if (sscanf(buf, "%d %d %d %d", &fy, &fx, &ty, &tx) == 4)
	{
	    struct timeval before, after;

	    putchar('"');

	    gettimeofday(&before, NULL);
	    mvcur(fy, fx, ty, tx);
	    gettimeofday(&after, NULL);

	    printf("\" (%ld msec)\n",
		(long)(after.tv_usec - before.tv_usec + (after.tv_sec - before.tv_sec) * 1000000));
	}
	else if (sscanf(buf, "s %d %d %d %d", &fy, &fx, &ty, &tx) == 4)
	{
	    struct timeval before, after;

	    putchar('"');

	    gettimeofday(&before, NULL);
	    _nc_scrolln(fy, fx, ty, tx);
	    gettimeofday(&after, NULL);

	    printf("\" (%ld msec)\n",
		(long)(after.tv_usec - before.tv_usec + (after.tv_sec - before.tv_sec) * 1000000));
	}
	else if (buf[0] == 'r')
	{
	    (void) strcpy(tname, termname());
	    load_term();
	}
	else if (sscanf(buf, "l %s", tname) == 1)
	{
	    load_term();
	}
	else if (sscanf(buf, "d %s", capname) == 1)
	{
	    struct name_table_entry const	*np = _nc_find_entry(capname,
							 _nc_info_hash_table);

	    if (np == NULL)
		(void) printf("No such capability as \"%s\"\n", capname);
	    else
	    {
		switch(np->nte_type)
		{
		case BOOLEAN:
		    cur_term->type.Booleans[np->nte_index] = FALSE;
		    (void) printf("Boolean capability `%s' (%d) turned off.\n",
				  np->nte_name, np->nte_index);
		    break;

		case NUMBER:
		    cur_term->type.Numbers[np->nte_index] = -1;
		    (void) printf("Number capability `%s' (%d) set to -1.\n",
				  np->nte_name, np->nte_index);
		    break;

		case STRING:
		    cur_term->type.Strings[np->nte_index] = (char *)NULL;
		    (void) printf("String capability `%s' (%d) deleted.\n",
				  np->nte_name, np->nte_index);
		    break;
		}
	    }
	}
	else if (buf[0] == 'i')
	{
	     dump_init((char *)NULL, F_TERMINFO, S_TERMINFO, 70, 0, FALSE);
	     dump_entry(&cur_term->type, FALSE, TRUE, 0);
	     putchar('\n');
	}
	else if (buf[0] == 'o')
	{
	     if (_nc_optimize_enable & OPTIMIZE_MVCUR)
	     {
		 _nc_optimize_enable &=~ OPTIMIZE_MVCUR;
		 (void) puts("Optimization is now off.");
	     }
	     else
	     {
		 _nc_optimize_enable |= OPTIMIZE_MVCUR;
		 (void) puts("Optimization is now on.");
	     }
	}
	/*
	 * You can use the `t' test to profile and tune the movement
	 * optimizer.  Use iteration values in three digits or more.
	 * At above 5000 iterations the profile timing averages are stable
	 * to within a millisecond or three.
	 *
	 * The `overhead' field of the report will help you pick a
	 * COMPUTE_OVERHEAD figure appropriate for your processor and
	 * expected line speed.  The `total estimated time' is
	 * computation time plus a character-transmission time
	 * estimate computed from the number of transmits and the baud
	 * rate.
	 *
	 * Use this together with the `o' command to get a read on the
	 * optimizer's effectiveness.  Compare the total estimated times
	 * for `t' runs of the same length in both optimized and un-optimized
	 * modes.  As long as the optimized times are less, the optimizer
	 * is winning.
	 */
	else if (sscanf(buf, "t %d", &n) == 1)
	{
	    float cumtime = 0, perchar;
	    int speeds[] = {2400, 9600, 14400, 19200, 28800, 38400, 0};

	    srand((unsigned)(getpid() + time((time_t *)0)));
	    profiling = TRUE;
	    xmits = 0;
	    for (i = 0; i < n; i++)
	    {
		/*
		 * This does a move test between two random locations,
		 * Random moves probably short-change the optimizer,
		 * which will work better on the short moves probably
		 * typical of doupdate()'s usage pattern.  Still,
		 * until we have better data...
		 */
#ifdef FIND_COREDUMP
		int from_y = roll(lines);
		int to_y = roll(lines);
		int from_x = roll(columns);
		int to_x = roll(columns);

		printf("(%d,%d) -> (%d,%d)\n", from_y, from_x, to_y, to_x);
		mvcur(from_y, from_x, to_y, to_x);
#else
		mvcur(roll(lines), roll(columns), roll(lines), roll(columns));
#endif /* FIND_COREDUMP */
		if (diff)
		    cumtime += diff;
	    }
	    profiling = FALSE;

	    /*
	     * Average milliseconds per character optimization time.
	     * This is the key figure to watch when tuning the optimizer.
	     */
	    perchar = cumtime / n;

	    (void) printf("%d moves (%ld chars) in %d msec, %f msec each:\n",
			  n, xmits, (int)cumtime, perchar);

	    for (i = 0; speeds[i]; i++)
	    {
		/*
		 * Total estimated time for the moves, computation and
		 * transmission both. Transmission time is an estimate
		 * assuming 9 bits/char, 8 bits + 1 stop bit.
		 */
		float totalest = cumtime + xmits * 9 * 1e6 / speeds[i];

		/*
		 * Per-character optimization overhead in character transmits
		 * at the current speed.  Round this to the nearest integer
		 * to figure COMPUTE_OVERHEAD for the speed.
		 */
		float overhead = speeds[i] * perchar / 1e6;

		(void) printf("%6d bps: %3.2f char-xmits overhead; total estimated time %15.2f\n",
			      speeds[i], overhead, totalest);
	    }
	}
	else if (buf[0] == 'c')
	{
	    (void) printf("char padding: %d\n", SP->_char_padding);
	    (void) printf("cr cost: %d\n", SP->_cr_cost);
	    (void) printf("cup cost: %d\n", SP->_cup_cost);
	    (void) printf("home cost: %d\n", SP->_home_cost);
	    (void) printf("ll cost: %d\n", SP->_ll_cost);
#if USE_HARD_TABS
	    (void) printf("ht cost: %d\n", SP->_ht_cost);
	    (void) printf("cbt cost: %d\n", SP->_cbt_cost);
#endif /* USE_HARD_TABS */
	    (void) printf("cub1 cost: %d\n", SP->_cub1_cost);
	    (void) printf("cuf1 cost: %d\n", SP->_cuf1_cost);
	    (void) printf("cud1 cost: %d\n", SP->_cud1_cost);
	    (void) printf("cuu1 cost: %d\n", SP->_cuu1_cost);
	    (void) printf("cub cost: %d\n", SP->_cub_cost);
	    (void) printf("cuf cost: %d\n", SP->_cuf_cost);
	    (void) printf("cud cost: %d\n", SP->_cud_cost);
	    (void) printf("cuu cost: %d\n", SP->_cuu_cost);
	    (void) printf("hpa cost: %d\n", SP->_hpa_cost);
	    (void) printf("vpa cost: %d\n", SP->_vpa_cost);
	}
	else if (buf[0] == 'x' || buf[0] == 'q')
	    break;
	else
	    (void) puts("Invalid command.");
    }

    (void) fputs("rmcup:", stdout);
    _nc_mvcur_wrap();
    putchar('\n');

    return(0);
}

#endif /* MAIN */

/* lib_mvcur.c ends here */
