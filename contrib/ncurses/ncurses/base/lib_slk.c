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
 *	lib_slk.c
 *	Soft key routines.
 */

#include <curses.priv.h>

#include <ctype.h>
#include <term.h>	/* num_labels, label_*, plab_norm */

MODULE_ID("$Id: lib_slk.c,v 1.16 1999/03/03 23:44:22 juergen Exp $")

/*
 * We'd like to move these into the screen context structure, but cannot,
 * because slk_init() is called before initscr()/newterm().
 */
int _nc_slk_format;  /* one more than format specified in slk_init() */

/*
 * Paint the info line for the PC style SLK emulation.
 *
 */
static void
slk_paint_info(WINDOW *win)
{
  if (win && SP->slk_format==4)
    {
      int i;

      mvwhline (win,0,0,0,getmaxx(win));
      wmove (win,0,0);

      for (i = 0; i < SP->_slk->maxlab; i++) {
	if (win && SP->slk_format==4)
	  {
	    mvwaddch(win,0,SP->_slk->ent[i].x,'F');
	    if (i<9)
	      waddch(win,'1'+i);
	    else
	      {
		waddch(win,'1');
		waddch(win,'0' + (i-9));
	      }
	  }
      }
    }
}

/*
 * Initialize soft labels.
 * Called from newterm()
 */
int
_nc_slk_initialize(WINDOW *stwin, int cols)
{
int i, x;
int res = OK;
char *p;

	T(("slk_initialize()"));

	if (SP->_slk)
	  { /* we did this already, so simply return */
	    return(OK);
	  }
	else
	  if ((SP->_slk = typeCalloc(SLK, 1)) == 0)
	    return(ERR);

	SP->_slk->ent    = NULL;
	SP->_slk->buffer = NULL;
	SP->_slk->attr   = A_STANDOUT;

	SP->_slk->maxlab = (num_labels > 0) ?
	  num_labels : MAX_SKEY(_nc_slk_format);
	SP->_slk->maxlen = (num_labels > 0) ?
	  label_width * label_height : MAX_SKEY_LEN(_nc_slk_format);
	SP->_slk->labcnt = (SP->_slk->maxlab < MAX_SKEY(_nc_slk_format)) ? 
	  MAX_SKEY(_nc_slk_format) : SP->_slk->maxlab;

	SP->_slk->ent = typeCalloc(slk_ent, SP->_slk->labcnt);
	if (SP->_slk->ent == NULL)
	  goto exception;

	p = SP->_slk->buffer = (char*) calloc(2*SP->_slk->labcnt,(1+SP->_slk->maxlen));
	if (SP->_slk->buffer == NULL)
	  goto exception;

	for (i = 0; i < SP->_slk->labcnt; i++) {
		SP->_slk->ent[i].text = p;
		p += (1 + SP->_slk->maxlen);
		SP->_slk->ent[i].form_text = p;
		p += (1 + SP->_slk->maxlen);
		memset(SP->_slk->ent[i].form_text, ' ', (unsigned)(SP->_slk->maxlen));
		SP->_slk->ent[i].visible = (i < SP->_slk->maxlab);
	}
	if (_nc_slk_format >= 3) /* PC style */
	  {
	    int gap = (cols - 3 * (3 + 4*SP->_slk->maxlen))/2;

	    if (gap < 1)
	      gap = 1;

	    for (i = x = 0; i < SP->_slk->maxlab; i++) {
	      SP->_slk->ent[i].x = x;
	      x += SP->_slk->maxlen;
	      x += (i==3 || i==7) ? gap : 1;
	    }
	    if (_nc_slk_format == 4)
	      slk_paint_info (stwin);
	  }
	else {
	  if (_nc_slk_format == 2) {	/* 4-4 */
	    int gap = cols - (SP->_slk->maxlab * SP->_slk->maxlen) - 6;

	    if (gap < 1)
			gap = 1;
	    for (i = x = 0; i < SP->_slk->maxlab; i++) {
	      SP->_slk->ent[i].x = x;
	      x += SP->_slk->maxlen;
	      x += (i == 3) ? gap : 1;
	    }
	  }
	  else
	    {
	      if (_nc_slk_format == 1) { /* 1 -> 3-2-3 */
		int gap = (cols - (SP->_slk->maxlab * SP->_slk->maxlen) - 5) / 2;

		if (gap < 1)
		  gap = 1;
		for (i = x = 0; i < SP->_slk->maxlab; i++) {
		  SP->_slk->ent[i].x = x;
		  x += SP->_slk->maxlen;
		  x += (i == 2 || i == 4) ? gap : 1;
		}
	      }
	      else
		goto exception;
	    }
	}
	SP->_slk->dirty = TRUE;
	if ((SP->_slk->win = stwin) == NULL)
	{
	exception:
		if (SP->_slk)
		{
		   FreeIfNeeded(SP->_slk->buffer);
		   FreeIfNeeded(SP->_slk->ent);
		   free(SP->_slk);
		   SP->_slk = (SLK*)0;
		   res = (ERR);
		}
	}

	/* We now reset the format so that the next newterm has again
	 * per default no SLK keys and may call slk_init again to
	 * define a new layout. (juergen 03-Mar-1999)
	 */
	SP->slk_format = _nc_slk_format;
	_nc_slk_format = 0;
	return(res);
}


/*
 * Restore the soft labels on the screen.
 */
int
slk_restore(void)
{
	T((T_CALLED("slk_restore()")));

	if (SP->_slk == NULL)
		return(ERR);
	SP->_slk->hidden = FALSE;
	SP->_slk->dirty = TRUE;
	/* we have to repaint info line eventually */
	slk_paint_info(SP->_slk->win);

	returnCode(slk_refresh());
}
