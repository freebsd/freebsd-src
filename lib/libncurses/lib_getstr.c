
/* This work is copyrighted. See COPYRIGHT.OLD & COPYRIGHT.NEW for   *
*  details. If they are missing then this copy is in violation of    *
*  the copyright conditions.                                        */

/*
**	lib_getstr.c
**
**	The routine wgetstr().
**
*/

#include "curses.priv.h"
#include "unctrl.h"

inline void backspace(WINDOW *win)
{
	mvwaddstr(curscr, win->_begy + win->_cury, win->_begx + win->_curx,
		 "\b \b");
	waddstr(win, "\b \b");
	fputs("\b \b", SP->_ofp);
	fflush(SP->_ofp);
	SP->_curscol--; 
}

int wgetnstr(WINDOW *win, char *str, int maxlen)
{
bool	oldnl, oldecho, oldraw, oldcbreak, oldkeypad;
char	erasec;
char	killc;
char	*oldstr;
int ch;
  
	T(("wgetnstr(%x,%x, %d) called", win, str, maxlen));

	oldnl = SP->_nl;
	oldecho = SP->_echo;
	oldraw = SP->_raw;
	oldcbreak = SP->_cbreak;
	oldkeypad = win->_use_keypad;
	nl();
	noecho();
	noraw();
	cbreak();
	keypad(win, TRUE);

	erasec = erasechar();
	killc = killchar();

	oldstr = str;

	vidattr(win->_attrs);
	if (is_wintouched(win) || (win->_flags & _HASMOVED))
		wrefresh(win);

	while ((ch = wgetch(win)) != ERR) {
		if (ch == '\n' || ch == '\r')
			break;
	   	if (ch == erasec || ch == KEY_LEFT || ch == KEY_BACKSPACE) {
			if (str > oldstr) {
		    		str--;
		    		backspace(win);
			}
	 	} else if (ch == killc) {
			while (str > oldstr) {
			    	str--;
		    		backspace(win);
			}
		} else if (maxlen >= 0 && str - oldstr >= maxlen) {
		    beep();
		} else {
			mvwaddstr(curscr, win->_begy + win->_cury,
				  win->_begx + win->_curx, unctrl(ch));
			waddstr(win, unctrl(ch));
			if (oldecho == TRUE) {
				fputs(unctrl(ch), SP->_ofp);
				fflush(SP->_ofp);
			}
			SP->_curscol++;
			*str++ = ch;
	   	}
	}

    	win->_curx = 0;
    	if (win->_cury < win->_maxy)
       		win->_cury++;
	wrefresh(win);

	if (oldnl == FALSE)
	    nonl();

	if (oldecho == TRUE)
	    echo();

	if (oldraw == TRUE)
	    raw();

	if (oldcbreak == FALSE)
	    nocbreak();

	if (oldkeypad == FALSE)
		keypad(win, FALSE);

	if (ch == ERR) {
		*str = '\0';
		return ERR;
	}
	*str = '\0';

	T(("wgetnstr returns \"%s\"", visbuf(oldstr)));

	return(OK);
}
