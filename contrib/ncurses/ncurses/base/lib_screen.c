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


#include <curses.priv.h>

#include <sys/stat.h>
#include <time.h>
#include <term.h>	/* exit_ca_mode, non_rev_rmcup */

MODULE_ID("$Id: lib_screen.c,v 1.15 1999/07/24 20:05:29 tom Exp $")

static time_t	dumptime;

WINDOW *getwin(FILE *filep)
{
	WINDOW	tmp, *nwin;
	int	n;

	T((T_CALLED("getwin(%p)"), filep));

	(void) fread(&tmp, sizeof(WINDOW), 1, filep);
	if (ferror(filep))
		returnWin(0);

	if ((nwin = newwin(tmp._maxy+1, tmp._maxx+1, 0, 0)) == 0)
		returnWin(0);

	/*
	 * We deliberately do not restore the _parx, _pary, or _parent
	 * fields, because the window hierarchy within which they
	 * made sense is probably gone.
	 */
	nwin->_curx       = tmp._curx;
	nwin->_cury       = tmp._cury;
	nwin->_maxy       = tmp._maxy;
	nwin->_maxx       = tmp._maxx;
	nwin->_begy       = tmp._begy;
	nwin->_begx       = tmp._begx;
	nwin->_yoffset    = tmp._yoffset;
	nwin->_flags      = tmp._flags & ~(_SUBWIN|_ISPAD);

	nwin->_attrs      = tmp._attrs;
	nwin->_bkgd       = tmp._bkgd;

	nwin->_clear      = tmp._clear;
	nwin->_scroll     = tmp._scroll;
	nwin->_leaveok    = tmp._leaveok;
	nwin->_use_keypad = tmp._use_keypad;
	nwin->_delay      = tmp._delay;
	nwin->_immed      = tmp._immed;
	nwin->_sync       = tmp._sync;

	nwin->_regtop     = tmp._regtop;
	nwin->_regbottom  = tmp._regbottom;

	for (n = 0; n < nwin->_maxy + 1; n++)
	{
		(void) fread(nwin->_line[n].text,
			      sizeof(chtype), (size_t)(nwin->_maxx + 1), filep);
		if (ferror(filep))
		{
			delwin(nwin);
			returnWin(0);
		}
	}
	touchwin(nwin);

	returnWin(nwin);
}

int putwin(WINDOW *win, FILE *filep)
{
        int code = ERR; 
	int n;

	T((T_CALLED("putwin(%p,%p)"), win, filep));

	if (win) {
	  (void) fwrite(win, sizeof(WINDOW), 1, filep);
	  if (ferror(filep))
	    returnCode(code);

	  for (n = 0; n < win->_maxy + 1; n++)
	    {
	      (void) fwrite(win->_line[n].text,
			    sizeof(chtype), (size_t)(win->_maxx + 1), filep);
	      if (ferror(filep))
		returnCode(code);
	    }
	  code = OK;
	}
	returnCode(code);
}

int scr_restore(const char *file)
{
	FILE	*fp = 0;

	T((T_CALLED("scr_restore(%s)"), _nc_visbuf(file)));

	if (_nc_access(file, R_OK) < 0
	 || (fp = fopen(file, "rb")) == 0)
	    returnCode(ERR);
	else
	{
	    delwin(newscr);
	    newscr = getwin(fp);
	    (void) fclose(fp);
	    returnCode(OK);
	}
}

int scr_dump(const char *file)
{
	FILE	*fp = 0;

	T((T_CALLED("scr_dump(%s)"), _nc_visbuf(file)));

	if (_nc_access(file, W_OK) < 0
	 || (fp = fopen(file, "wb")) == 0)
	    returnCode(ERR);
	else
	{
	    (void) putwin(newscr, fp);
	    (void) fclose(fp);
	    dumptime = time((time_t *)0);
	    returnCode(OK);
	}
}

int scr_init(const char *file)
{
	FILE	*fp = 0;
	struct stat	stb;

	T((T_CALLED("scr_init(%s)"), _nc_visbuf(file)));

	if (exit_ca_mode && non_rev_rmcup)
	    returnCode(ERR);

	if (_nc_access(file, R_OK) < 0
	 || (fp = fopen(file, "rb")) == 0)
	    returnCode(ERR);
	else if (fstat(STDOUT_FILENO, &stb) || stb.st_mtime > dumptime)
	    returnCode(ERR);
	else
	{
	    delwin(curscr);
	    curscr = getwin(fp);
	    (void) fclose(fp);
	    returnCode(OK);
	}
}

int scr_set(const char *file)
{
    T((T_CALLED("scr_set(%s)"), _nc_visbuf(file)));

    if (scr_init(file) == ERR)
	returnCode(ERR);
    else
    {
	delwin(newscr);
	newscr = dupwin(curscr);
	returnCode(OK);
    }
}
