/****************************************************************************
 * Copyright (c) 1998,1999,2000 Free Software Foundation, Inc.              *
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

/* lib_color.c
 *
 * Handles color emulation of SYS V curses
 */

#include <curses.priv.h>

#include <term.h>
#include <tic.h>

MODULE_ID("$Id: lib_color.c,v 1.55 2000/12/10 02:43:27 tom Exp $")

/*
 * These should be screen structure members.  They need to be globals for
 * historical reasons.  So we assign them in start_color() and also in
 * set_term()'s screen-switching logic.
 */
NCURSES_EXPORT_VAR(int)
COLOR_PAIRS = 0;
NCURSES_EXPORT_VAR(int)
COLORS = 0;

/*
 * Given a RGB range of 0..1000, we'll normally set the individual values
 * to about 2/3 of the maximum, leaving full-range for bold/bright colors.
 */
#define RGB_ON  680
#define RGB_OFF 0
/* *INDENT-OFF* */
static const color_t cga_palette[] =
{
    /*  R               G               B */
    {RGB_OFF,		RGB_OFF,	RGB_OFF},	/* COLOR_BLACK */
    {RGB_ON,		RGB_OFF,	RGB_OFF},	/* COLOR_RED */
    {RGB_OFF,		RGB_ON,		RGB_OFF},	/* COLOR_GREEN */
    {RGB_ON,		RGB_ON,		RGB_OFF},	/* COLOR_YELLOW */
    {RGB_OFF,		RGB_OFF,	RGB_ON},	/* COLOR_BLUE */
    {RGB_ON,		RGB_OFF,	RGB_ON},	/* COLOR_MAGENTA */
    {RGB_OFF,		RGB_ON,		RGB_ON},	/* COLOR_CYAN */
    {RGB_ON,		RGB_ON,		RGB_ON},	/* COLOR_WHITE */
};

static const color_t hls_palette[] =
{
    /*  H       L       S */
    {	0,	0,	0},		/* COLOR_BLACK */
    {	120,	50,	100},		/* COLOR_RED */
    {	240,	50,	100},		/* COLOR_GREEN */
    {	180,	50,	100},		/* COLOR_YELLOW */
    {	330,	50,	100},		/* COLOR_BLUE */
    {	60,	50,	100},		/* COLOR_MAGENTA */
    {	300,	50,	100},		/* COLOR_CYAN */
    {	0,	50,	100},		/* COLOR_WHITE */
};
/* *INDENT-ON* */

#if NCURSES_EXT_FUNCS
/*
 * These are called from _nc_do_color(), which in turn is called from
 * vidattr - so we have to assume that SP may be null.
 */
     static int
       default_fg(void)
{
    return (SP != 0) ? SP->_default_fg : COLOR_WHITE;
}

static int
default_bg(void)
{
    return SP != 0 ? SP->_default_bg : COLOR_BLACK;
}
#else
#define default_fg() COLOR_WHITE
#define default_bg() COLOR_BLACK
#endif

/*
 * SVr4 curses is known to interchange color codes (1,4) and (3,6), possibly
 * to maintain compatibility with a pre-ANSI scheme.  The same scheme is
 * also used in the FreeBSD syscons.
 */
     static int
       toggled_colors(int c)
{
    if (c < 16) {
	static const int table[] =
	{0, 4, 2, 6, 1, 5, 3, 7,
	 8, 12, 10, 14, 9, 13, 11, 15};
	c = table[c];
    }
    return c;
}

static void
set_background_color(int bg, int (*outc) (int))
{
    if (set_a_background) {
	TPUTS_TRACE("set_a_background");
	tputs(tparm(set_a_background, bg), 1, outc);
    } else {
	TPUTS_TRACE("set_background");
	tputs(tparm(set_background, toggled_colors(bg)), 1, outc);
    }
}

static void
set_foreground_color(int fg, int (*outc) (int))
{
    if (set_a_foreground) {
	TPUTS_TRACE("set_a_foreground");
	tputs(tparm(set_a_foreground, fg), 1, outc);
    } else {
	TPUTS_TRACE("set_foreground");
	tputs(tparm(set_foreground, toggled_colors(fg)), 1, outc);
    }
}

static bool
set_original_colors(void)
{
    if (orig_pair != 0) {
	TPUTS_TRACE("orig_pair");
	putp(orig_pair);
	return TRUE;
    } else if (orig_colors != NULL) {
	TPUTS_TRACE("orig_colors");
	putp(orig_colors);
	return TRUE;
    }
    return FALSE;
}

NCURSES_EXPORT(int)
start_color(void)
{
    int n;
    const color_t *tp;

    T((T_CALLED("start_color()")));

    if (set_original_colors() != TRUE) {
	set_foreground_color(default_fg(), _nc_outch);
	set_background_color(default_bg(), _nc_outch);
    }

    if (VALID_NUMERIC(max_pairs))
	COLOR_PAIRS = SP->_pair_count = max_pairs;
    else
	returnCode(ERR);
    if ((SP->_color_pairs = typeCalloc(unsigned short, max_pairs)) == 0)
	  returnCode(ERR);
    SP->_color_pairs[0] = PAIR_OF(default_fg(), default_bg());
    if (VALID_NUMERIC(max_colors))
	COLORS = SP->_color_count = max_colors;
    else
	returnCode(ERR);
    SP->_coloron = 1;

    if ((SP->_color_table = typeMalloc(color_t, COLORS)) == 0)
	returnCode(ERR);
    tp = (hue_lightness_saturation) ? hls_palette : cga_palette;
    for (n = 0; n < COLORS; n++) {
	if (n < 8) {
	    SP->_color_table[n] = tp[n];
	} else {
	    SP->_color_table[n] = tp[n % 8];
	    if (hue_lightness_saturation) {
		SP->_color_table[n].green = 100;
	    } else {
		if (SP->_color_table[n].red)
		    SP->_color_table[n].red = 1000;
		if (SP->_color_table[n].green)
		    SP->_color_table[n].green = 1000;
		if (SP->_color_table[n].blue)
		    SP->_color_table[n].blue = 1000;
	    }
	}
    }

    T(("started color: COLORS = %d, COLOR_PAIRS = %d", COLORS, COLOR_PAIRS));

    returnCode(OK);
}

/* This function was originally written by Daniel Weaver <danw@znyx.com> */
static void
rgb2hls(short r, short g, short b, short *h, short *l, short *s)
/* convert RGB to HLS system */
{
    short min, max, t;

    if ((min = g < r ? g : r) > b)
	min = b;
    if ((max = g > r ? g : r) < b)
	max = b;

    /* calculate lightness */
    *l = (min + max) / 20;

    if (min == max) {		/* black, white and all shades of gray */
	*h = 0;
	*s = 0;
	return;
    }

    /* calculate saturation */
    if (*l < 50)
	*s = ((max - min) * 100) / (max + min);
    else
	*s = ((max - min) * 100) / (2000 - max - min);

    /* calculate hue */
    if (r == max)
	t = 120 + ((g - b) * 60) / (max - min);
    else if (g == max)
	t = 240 + ((b - r) * 60) / (max - min);
    else
	t = 360 + ((r - g) * 60) / (max - min);

    *h = t % 360;
}

/*
 * Extension (1997/1/18) - Allow negative f/b values to set default color
 * values.
 */
NCURSES_EXPORT(int)
init_pair
(short pair, short f, short b)
{
    unsigned result;

    T((T_CALLED("init_pair(%d,%d,%d)"), pair, f, b));

    if ((pair < 0) || (pair >= COLOR_PAIRS))
	returnCode(ERR);
#if NCURSES_EXT_FUNCS
    if (SP->_default_color) {
	if (f < 0)
	    f = C_MASK;
	if (b < 0)
	    b = C_MASK;
	if (f >= COLORS && f != C_MASK)
	    returnCode(ERR);
	if (b >= COLORS && b != C_MASK)
	    returnCode(ERR);
    } else
#endif
    {
	if ((f < 0) || (f >= COLORS)
	    || (b < 0) || (b >= COLORS)
	    || (pair < 1))
	    returnCode(ERR);
    }

    /*
     * When a pair's content is changed, replace its colors (if pair was
     * initialized before a screen update is performed replacing original
     * pair colors with the new ones).
     */
    result = PAIR_OF(f, b);
    if (SP->_color_pairs[pair] != 0
	&& SP->_color_pairs[pair] != result) {
	int y, x;
	attr_t z = COLOR_PAIR(pair);

	for (y = 0; y <= curscr->_maxy; y++) {
	    struct ldat *ptr = &(curscr->_line[y]);
	    bool changed = FALSE;
	    for (x = 0; x <= curscr->_maxx; x++) {
		if ((ptr->text[x] & A_COLOR) == z) {
		    /* Set the old cell to zero to ensure it will be
		       updated on the next doupdate() */
		    ptr->text[x] = 0;
		    CHANGED_CELL(ptr, x);
		    changed = TRUE;
		}
	    }
	    if (changed)
		_nc_make_oldhash(y);
	}
    }
    SP->_color_pairs[pair] = result;
    if ((int) (SP->_current_attr & A_COLOR) == COLOR_PAIR(pair))
	SP->_current_attr |= A_COLOR;	/* force attribute update */

    if (initialize_pair) {
	const color_t *tp = hue_lightness_saturation ? hls_palette : cga_palette;

	T(("initializing pair: pair = %d, fg=(%d,%d,%d), bg=(%d,%d,%d)",
	   pair,
	   tp[f].red, tp[f].green, tp[f].blue,
	   tp[b].red, tp[b].green, tp[b].blue));

	if (initialize_pair) {
	    TPUTS_TRACE("initialize_pair");
	    putp(tparm(initialize_pair,
		       pair,
		       tp[f].red, tp[f].green, tp[f].blue,
		       tp[b].red, tp[b].green, tp[b].blue));
	}
    }

    returnCode(OK);
}

NCURSES_EXPORT(int)
init_color
(short color, short r, short g, short b)
{
    T((T_CALLED("init_color(%d,%d,%d,%d)"), color, r, g, b));

    if (initialize_color == NULL)
	returnCode(ERR);

    if (color < 0 || color >= COLORS)
	returnCode(ERR);
    if (r < 0 || r > 1000 || g < 0 || g > 1000 || b < 0 || b > 1000)
	returnCode(ERR);

    if (hue_lightness_saturation)
	rgb2hls(r, g, b,
		&SP->_color_table[color].red,
		&SP->_color_table[color].green,
		&SP->_color_table[color].blue);
    else {
	SP->_color_table[color].red = r;
	SP->_color_table[color].green = g;
	SP->_color_table[color].blue = b;
    }

    if (initialize_color) {
	TPUTS_TRACE("initialize_color");
	putp(tparm(initialize_color, color, r, g, b));
    }
    returnCode(OK);
}

NCURSES_EXPORT(bool)
can_change_color(void)
{
    T((T_CALLED("can_change_color()")));
    returnCode((can_change != 0) ? TRUE : FALSE);
}

NCURSES_EXPORT(bool)
has_colors(void)
{
    T((T_CALLED("has_colors()")));
    returnCode((VALID_NUMERIC(max_colors) && VALID_NUMERIC(max_pairs)
		&& (((set_foreground != NULL)
		     && (set_background != NULL))
		    || ((set_a_foreground != NULL)
			&& (set_a_background != NULL))
		    || set_color_pair)) ? TRUE : FALSE);
}

NCURSES_EXPORT(int)
color_content
(short color, short *r, short *g, short *b)
{
    T((T_CALLED("color_content(%d,%p,%p,%p)"), color, r, g, b));
    if (color < 0 || color >= COLORS)
	returnCode(ERR);

    if (r)
	*r = SP->_color_table[color].red;
    if (g)
	*g = SP->_color_table[color].green;
    if (b)
	*b = SP->_color_table[color].blue;
    returnCode(OK);
}

NCURSES_EXPORT(int)
pair_content
(short pair, short *f, short *b)
{
    T((T_CALLED("pair_content(%d,%p,%p)"), pair, f, b));

    if ((pair < 0) || (pair >= COLOR_PAIRS))
	returnCode(ERR);
    if (f)
	*f = ((SP->_color_pairs[pair] >> C_SHIFT) & C_MASK);
    if (b)
	*b = (SP->_color_pairs[pair] & C_MASK);

    returnCode(OK);
}

NCURSES_EXPORT(void)
_nc_do_color
(int old_pair, int pair, bool reverse, int (*outc) (int))
{
    NCURSES_COLOR_T fg = C_MASK, bg = C_MASK;
    NCURSES_COLOR_T old_fg, old_bg;

    if (pair < 0 || pair >= COLOR_PAIRS) {
	return;
    } else if (pair != 0) {
	if (set_color_pair) {
	    TPUTS_TRACE("set_color_pair");
	    tputs(tparm(set_color_pair, pair), 1, outc);
	    return;
	} else if (SP != 0) {
	    pair_content(pair, &fg, &bg);
	}
    }

    if (old_pair >= 0 && SP != 0) {
	pair_content(old_pair, &old_fg, &old_bg);
	if ((fg == C_MASK && old_fg != C_MASK)
	    || (bg == C_MASK && old_bg != C_MASK)) {
#if NCURSES_EXT_FUNCS
	    /*
	     * A minor optimization - but extension.  If "AX" is specified in
	     * the terminal description, treat it as screen's indicator of ECMA
	     * SGR 39 and SGR 49, and assume the two sequences are independent.
	     */
	    if (SP->_has_sgr_39_49 && old_bg == C_MASK && old_fg != C_MASK) {
		tputs("\033[39m", 1, outc);
	    } else if (SP->_has_sgr_39_49 && old_fg == C_MASK && old_bg != C_MASK) {
		tputs("\033[49m", 1, outc);
	    } else
#endif
		set_original_colors();
	}
    } else {
	set_original_colors();
	if (old_pair < 0)
	    return;
    }

#if NCURSES_EXT_FUNCS
    if (fg == C_MASK)
	fg = default_fg();
    if (bg == C_MASK)
	bg = default_bg();
#endif

    if (reverse) {
	NCURSES_COLOR_T xx = fg;
	fg = bg;
	bg = xx;
    }

    TR(TRACE_ATTRS, ("setting colors: pair = %d, fg = %d, bg = %d", pair,
		     fg, bg));

    if (fg != C_MASK) {
	set_foreground_color(fg, outc);
    }
    if (bg != C_MASK) {
	set_background_color(bg, outc);
    }
}
