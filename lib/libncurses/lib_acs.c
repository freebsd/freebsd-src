
/* This work is copyrighted. See COPYRIGHT.OLD & COPYRIGHT.NEW for   *
*  details. If they are missing then this copy is in violation of    *
*  the copyright conditions.                                        */


#include "curses.priv.h"
#include "terminfo.h"
#include <string.h>

/* line graphics */


chtype acs_map[128];

void init_acs()
{

/*
 ACS_ULCORNER	(acs_map['l'])
 ACS_LLCORNER	(acs_map['m'])
 ACS_URCORNER	(acs_map['k'])
 ACS_LRCORNER	(acs_map['j'])
 ACS_RTEE	(acs_map['u'])
 ACS_LTEE	(acs_map['t'])
 ACS_BTEE	(acs_map['v'])
 ACS_TTEE	(acs_map['w'])
 ACS_HLINE	(acs_map['q'])
 ACS_VLINE	(acs_map['x'])
 ACS_PLUS	(acs_map['n'])
 ACS_S1		(acs_map['o'])	scan line 1
 ACS_S9		(acs_map['s'])	scan line 9
 ACS_DIAMOND	(acs_map['`'])	diamond 
 ACS_CKBOARD	(acs_map['a'])	checker board (stipple) 
 ACS_DEGREE	(acs_map['f'])	degree symbol 
 ACS_PLMINUS	(acs_map['g'])	plus/minus
 ACS_BULLET	(acs_map['~'])	bullet
 ACS_LARROW	(acs_map[','])	arrow pointing left
 ACS_RARROW	(acs_map['+'])	arrow pointing right
 ACS_DARROW	(acs_map['.'])	arrow pointing down
 ACS_UARROW	(acs_map['-'])	arrow pointing up 
 ACS_BOARD	(acs_map['h'])	board of squares 
 ACS_LANTERN	(acs_map['I'])	lantern symbol 
 ACS_BLOCK	(acs_map['0'])	solid square block
*/

	T(("initializing ACS map"));

	acs_map['l'] = acs_map['m'] = acs_map['k'] = acs_map['j'] = 
	acs_map['u'] = acs_map['t'] = acs_map['v'] = acs_map['w'] = (chtype)'+' & A_CHARTEXT;
	acs_map['q'] = (chtype)'-' & A_CHARTEXT;
	acs_map['x'] = (chtype)'|' & A_CHARTEXT;
	acs_map['n'] = (chtype)'+' & A_CHARTEXT;
	acs_map['o'] = (chtype)'~' & A_CHARTEXT;
	acs_map['s'] = (chtype)'_' & A_CHARTEXT;
	acs_map['`'] = (chtype)'+' & A_CHARTEXT;
	acs_map['a'] = (chtype)':' & A_CHARTEXT;
	acs_map['f'] = (chtype)'\'' & A_CHARTEXT;
	acs_map['g'] = (chtype)'#' & A_CHARTEXT;
	acs_map['~'] = (chtype)'o' & A_CHARTEXT;
	acs_map[','] = (chtype)'<' & A_CHARTEXT;
	acs_map['+'] = (chtype)'>' & A_CHARTEXT;
	acs_map['.'] = (chtype)'v' & A_CHARTEXT;
	acs_map['-'] = (chtype)'^' & A_CHARTEXT;
	acs_map['h'] = (chtype)'#' & A_CHARTEXT;
	acs_map['I'] = (chtype)'#' & A_CHARTEXT;
	acs_map['0'] = (chtype)'#' & A_CHARTEXT;

	if (ena_acs != NULL)
		putp(ena_acs);

	if (acs_chars != NULL) {
	    int i = 0;
	    int length = strlen(acs_chars);
	    
		while (i < length) 
			switch (acs_chars[i]) {
			case 'l':case 'm':case 'k':case 'j':
			case 'u':case 't':case 'v':case 'w':
			case 'q':case 'x':case 'n':case 'o':
			case 's':case '`':case 'a':case 'f':
			case 'g':case '~':case ',':case '+':
			case '.':case '-':case 'h':case 'I':
			case '0': 
				acs_map[(unsigned int)(unsigned char)acs_chars[i]] = 
					(acs_chars[++i] & A_CHARTEXT) | A_ALTCHARSET;
			default:
				i++;
				break;
			}
	}
#ifdef TRACE
	else {
		T(("acsc not defined, using default mapping"));
	}
#endif
}

