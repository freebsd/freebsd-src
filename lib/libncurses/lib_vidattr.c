
/* This work is copyrighted. See COPYRIGHT.OLD & COPYRIGHT.NEW for   *
*  details. If they are missing then this copy is in violation of    *
*  the copyright conditions.                                        */

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
#include <nterm.h>

static void do_color(int pair, int  (*outc)(char))
{
short fg, bg;

	if ( pair == 0 ) {
		tputs(orig_pair, 1, outc);
	} else {
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

static int current_pair = 0;
static chtype	previous_attr = 0;

int vidputs(chtype newmode, int  (*outc)(char))
{
chtype		turn_off, turn_on;

	T(("vidputs(%x) called %s", newmode, _traceattr(newmode)));
	T(("previous attribute was %s", _traceattr(previous_attr)));

	if (newmode == A_NORMAL && exit_attribute_mode) {
	    if((previous_attr & A_ALTCHARSET) && exit_alt_charset_mode)
		    tputs(exit_alt_charset_mode, 1, outc);
	    tputs(exit_attribute_mode, 1, outc);
	    current_pair = -1;
	    goto set_color;
	}
	else if (set_attributes) {
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
	    goto set_color;
	} else {
 		if (exit_attribute_mode) {
 			if((previous_attr & A_ALTCHARSET) && exit_alt_charset_mode) {
 				tputs(exit_alt_charset_mode, 1, outc);
 				previous_attr &= ~A_ALTCHARSET;
 			}
 			if (previous_attr) {
				T(("exiting attribute mode"));

 				tputs(exit_attribute_mode, 1, outc);
 				previous_attr = 0;
				current_pair = -1;
 			}
 		} else {
			turn_off = ~newmode & previous_attr;

			T(("turning %x off", turn_off));
			
			if ((turn_off & A_ALTCHARSET) && exit_alt_charset_mode) 
				tputs(exit_alt_charset_mode, 1, outc);

			if ((turn_off & A_BOLD)  &&  exit_standout_mode)
			    tputs(exit_standout_mode, 1, outc);

			if ((turn_off & A_DIM)  && exit_standout_mode)
			    tputs(exit_standout_mode, 1, outc);

			if ((turn_off & A_BLINK)  && exit_standout_mode)
			    tputs(exit_standout_mode, 1, outc);

			if ((turn_off & A_INVIS)  && exit_standout_mode)
			    tputs(exit_standout_mode, 1, outc);

			if ((turn_off & A_PROTECT)  && exit_standout_mode)
			    tputs(exit_standout_mode, 1, outc);

			if ((turn_off & A_UNDERLINE)  &&  exit_underline_mode)
			    tputs(exit_underline_mode, 1, outc);

   		 	if ((turn_off & A_REVERSE)  &&  exit_standout_mode)
			    tputs(exit_standout_mode, 1, outc);

			if ((turn_off & A_STANDOUT)  &&  exit_standout_mode)
			    tputs(exit_standout_mode, 1, outc);
		}

		turn_on = newmode & ~previous_attr;

		T(("turning %x on", turn_on));

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
	set_color:
		if (_coloron) {
		int pair = PAIR_NUMBER(newmode);
	    
	   		T(("old pair = %d -- new pair = %d", current_pair, pair));
	   		if (pair != current_pair) {
	   			current_pair = pair;
	   			do_color(pair, outc);
	   		}
	   	}
	}


	previous_attr = newmode;

	T(("vidputs finished"));
	return OK;
}

int vidattr(chtype newmode)
{

	T(("vidattr(%x) called", newmode));

	return(vidputs(newmode, _outc));
}

