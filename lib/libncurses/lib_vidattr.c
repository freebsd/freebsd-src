
/***************************************************************************
*                            COPYRIGHT NOTICE                              *
****************************************************************************
*                ncurses is copyright (C) 1992, 1993, 1994                 *
*                          by Zeyd M. Ben-Halim                            *
*                          zmbenhal@netcom.com                             *
*                                                                          *
*        Permission is hereby granted to reproduce and distribute ncurses  *
*        by any means and for any fee, whether alone or as part of a       *
*        larger distribution, in source or in binary form, PROVIDED        *
*        this notice is included with any such distribution, not removed   *
*        from header files, and is reproduced in any documentation         *
*        accompanying it or the applications linked with it.               *
*                                                                          *
*        ncurses comes AS IS with no warranty, implied or expressed.       *
*                                                                          *
***************************************************************************/

/*
 *	vidputs(newmode, outc)
 *
 *	newmode is taken to be the logical 'or' of the symbols in curses.h
 *	representing graphic renditions.  The teminal is set to be in all of
 *	the given modes, if possible.
 *
 *	if set-attributes exists
 *		use it to set exactly what you want
 *	else
 *		if exit-attribute-mode exists
 *			turn off everything
 *		else
 *			turn off those which can be turned off and aren't in
 *			newmode.
 *		turn on each mode which should be on and isn't, one by one
 *
 *	NOTE that this algorithm won't achieve the desired mix of attributes
 *	in some cases, but those are probably just those cases in which it is
 *	actually impossible, anyway, so...
 *
 */

#include <string.h>
#include "curses.priv.h"
#include "terminfo.h"

static void do_color(int pair, int  (*outc)(int))
{
int fg, bg;

	if ( pair == 0 ) {
		if (orig_pair) {
			tputs(orig_pair, 1, outc);
		}
	} else if ((set_a_foreground || set_foreground) &&
		   (set_a_background || set_background)
		  ) {
		fg = FG(color_pairs[pair]);
		bg = BG(color_pairs[pair]);

		T(("setting colors: pair = %d, fg = %d, bg = %d\n", pair, fg, bg));

		if (set_a_foreground)
		    tputs(tparm(set_a_foreground, fg), 1, outc);
		else
		    tputs(tparm(set_foreground, fg), 1, outc);
		if (set_a_background)
		    tputs(tparm(set_a_background, bg), 1, outc);
		else
		    tputs(tparm(set_background, bg), 1, outc);
	}
}

#define previous_attr SP->_current_attr

int vidputs(chtype newmode, int  (*outc)(int))
{
chtype	turn_off = (~newmode & previous_attr) & ~A_COLOR;
chtype	turn_on  = (newmode & ~previous_attr) & ~A_COLOR;
int pair, current_pair;

	T(("vidputs(%x) called %s", newmode, _traceattr(newmode)));
	T(("previous attribute was %s", _traceattr(previous_attr)));

	if (newmode == previous_attr)
		return OK;

	pair = PAIR_NUMBER(newmode);
	current_pair = PAIR_NUMBER(previous_attr);

	if ((!SP || SP->_coloron) && pair == 0) {
   		T(("old pair = %d -- new pair = %d", current_pair, pair));
		if (pair != current_pair) {
			do_color(pair, outc);
			previous_attr &= ~A_COLOR;
		}
   	}

	if (newmode == A_NORMAL && exit_attribute_mode) {
		if((previous_attr & A_ALTCHARSET) && exit_alt_charset_mode) {
 			tputs(exit_alt_charset_mode, 1, outc);
 			previous_attr &= ~A_ALTCHARSET;
 		}
		if (previous_attr) {
 			tputs(exit_attribute_mode, 1, outc);
			previous_attr &= ~A_COLOR;
		}
	} else if (set_attributes) {
		if (turn_on || turn_off) {
	    		tputs(tparm(set_attributes,
				(newmode & A_STANDOUT) != 0,
				(newmode & A_UNDERLINE) != 0,
				(newmode & A_REVERSE) != 0,
				(newmode & A_BLINK) != 0,
				(newmode & A_DIM) != 0,
				(newmode & A_BOLD) != 0,
				(newmode & A_INVIS) != 0,
				(newmode & A_PROTECT) != 0,
				(newmode & A_ALTCHARSET) != 0), 1, outc);
			previous_attr &= ~A_COLOR;
		}
	} else {

		T(("turning %x off", _traceattr(turn_off)));

		if ((turn_off & A_ALTCHARSET) && exit_alt_charset_mode) {
			tputs(exit_alt_charset_mode, 1, outc);
			turn_off &= ~A_ALTCHARSET;
		}

		if ((turn_off & A_UNDERLINE)  &&  exit_underline_mode) {
			tputs(exit_underline_mode, 1, outc);
			turn_off &= ~A_UNDERLINE;
		}

		if ((turn_off & A_STANDOUT)  &&  exit_standout_mode) {
			tputs(exit_standout_mode, 1, outc);
			turn_off &= ~A_STANDOUT;
		}

		if (turn_off && exit_attribute_mode) {
			tputs(exit_attribute_mode, 1, outc);
			turn_on |= newmode & (A_UNDERLINE|A_REVERSE|A_BLINK|A_DIM|A_BOLD|A_INVIS|A_PROTECT);
			previous_attr &= ~A_COLOR;
		}

		T(("turning %x on", _traceattr(turn_on)));

		if ((turn_on & A_ALTCHARSET) && enter_alt_charset_mode)
			tputs(enter_alt_charset_mode, 1, outc);

		if ((turn_on & A_BLINK)  &&  enter_blink_mode)
			tputs(enter_blink_mode, 1, outc);

		if ((turn_on & A_BOLD)  &&  enter_bold_mode)
			tputs(enter_bold_mode, 1, outc);

		if ((turn_on & A_DIM)  &&  enter_dim_mode)
			tputs(enter_dim_mode, 1, outc);

		if ((turn_on & A_REVERSE)  &&  enter_reverse_mode)
			tputs(enter_reverse_mode, 1, outc);

		if ((turn_on & A_STANDOUT)  &&  enter_standout_mode)
			tputs(enter_standout_mode, 1, outc);

		if ((turn_on & A_PROTECT)  &&  enter_protected_mode)
			tputs(enter_protected_mode, 1, outc);

		if ((turn_on & A_INVIS)  &&  enter_secure_mode)
			tputs(enter_secure_mode, 1, outc);

		if ((turn_on & A_UNDERLINE)  &&  enter_underline_mode)
			tputs(enter_underline_mode, 1, outc);

	}

	if ((!SP || SP->_coloron) && pair != 0) {
		current_pair = PAIR_NUMBER(previous_attr);
   		T(("old pair = %d -- new pair = %d", current_pair, pair));
		if (pair != current_pair) {
			do_color(pair, outc);
		}
   	}

	previous_attr = newmode;

	T(("vidputs finished"));
	return OK;
}

int vidattr(chtype newmode)
{

	T(("vidattr(%x) called", newmode));

	return(vidputs(newmode, _outch));
}

