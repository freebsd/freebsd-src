
/* This work is copyrighted. See COPYRIGHT.OLD & COPYRIGHT.NEW for   *
*  details. If they are missing then this copy is in violation of    *
*  the copyright conditions.                                        */

/*
**	lib_options.c
**
**	The routines to handle option setting.
**
*/

#include <stdlib.h>
#include "terminfo.h"
#include "curses.priv.h"

int idlok(WINDOW *win, int flag)
{
	T(("idlok(%x,%d) called", win, flag));

	if (flag == FALSE) {
		win->_idlok = FALSE;
		return OK;
	}

	if ((insert_line || parm_insert_line) && (delete_line || parm_delete_line)) {
		win->_idlok = TRUE;
	}
	return OK;
}


int clearok(WINDOW *win, int flag)
{
	T(("clearok(%x,%d) called", win, flag));

   	if (win == curscr)
	    newscr->_clear = flag;
	else
	    win->_clear = flag;
	return OK;
}


int leaveok(WINDOW *win, int flag)
{
	T(("leaveok(%x,%d) called", win, flag));

   	win->_leave = flag;
   	if (flag == TRUE)
   		curs_set(0);
   	else
   		curs_set(1);
	return OK;
}


int scrollok(WINDOW *win, int flag)
{
	T(("scrollok(%x,%d) called", win, flag));

   	win->_scroll = flag;
	return OK;
}

int halfdelay(int t)
{
	T(("halfdelay(%d) called", t));

	if (t < 1 || t > 255)
		return ERR;

	cbreak();
	SP->_cbreak = t+1;
	return OK;
}

int nodelay(WINDOW *win, int flag)
{
	T(("nodelay(%x,%d) called", win, flag));

   	if (flag == TRUE)
		win->_delay = 0;
	else win->_delay = -1;
	return OK;
}

int notimeout(WINDOW *win, bool f)
{
	T(("notimout(%x,%d) called", win, f));

	win->_notimeout = f;
	return OK;
}

int wtimeout(WINDOW *win, int delay)
{
	T(("wtimeout(%x,%d) called", win, delay));

	win->_delay = delay;
	return OK;
}

static void init_keytry();
static void add_to_try(char *, short);

int keypad(WINDOW *win, int flag)
{
	T(("keypad(%x,%d) called", win, flag));

	if (win) {
	  win->_use_keypad = flag;
	  return _nc_keypad(flag);
	}
	else
	  return ERR;
}



int meta(WINDOW *win, int flag)
{
	T(("meta(%x,%d) called", win, flag));

	win->_use_meta = flag;

	if (flag  &&  meta_on)
	    putp(meta_on);
	else if (! flag  &&  meta_off)
	    putp(meta_off);
	return OK;
}

/*
**      init_keytry()
**
**      Construct the try for the current terminal's keypad keys.
**
*/


static struct  try *newtry;

static void init_keytry()
{
	newtry = NULL;

#include "keys.tries.h"

	SP->_keytry = newtry;
}


static void add_to_try(char *str, short code)
{
static bool     out_of_memory = FALSE;
struct try      *ptr, *savedptr;

	if (! str  ||  out_of_memory)
	    	return;

	if (newtry != NULL)    {
    	ptr = savedptr = newtry;

       	for (;;) {
	       	while (ptr->ch != (unsigned char) *str
		       &&  ptr->sibling != NULL)
	       		ptr = ptr->sibling;

	       	if (ptr->ch == (unsigned char) *str) {
	    		if (*(++str)) {
	           		if (ptr->child != NULL)
	           			ptr = ptr->child;
               		else
	           			break;
	    		} else {
	        		ptr->value = code;
					return;
	   			}
			} else {
	    		if ((ptr->sibling = (struct try *) malloc(sizeof *ptr)) == NULL) {
	        		out_of_memory = TRUE;
					return;
	    		}

	    		savedptr = ptr = ptr->sibling;
	    		ptr->child = ptr->sibling = NULL;
	    		ptr->ch = *str++;
	    		ptr->value = (short) NULL;

           		break;
	       	}
	   	} /* end for (;;) */
	} else {   /* newtry == NULL :: First sequence to be added */
	    	savedptr = ptr = newtry = (struct try *) malloc(sizeof *ptr);

	    	if (ptr == NULL) {
	        	out_of_memory = TRUE;
				return;
	    	}

	    	ptr->child = ptr->sibling = NULL;
	    	ptr->ch = *(str++);
	    	ptr->value = (short) NULL;
	}

	    /* at this point, we are adding to the try.  ptr->child == NULL */

	while (*str) {
	   	ptr->child = (struct try *) malloc(sizeof *ptr);

	   	ptr = ptr->child;

	   	if (ptr == NULL) {
	       	out_of_memory = TRUE;

			ptr = savedptr;
			while (ptr != NULL) {
		    	savedptr = ptr->child;
		    	free(ptr);
		    	ptr = savedptr;
			}

			return;
		}

	   	ptr->child = ptr->sibling = NULL;
	   	ptr->ch = *(str++);
	   	ptr->value = (short) NULL;
	}

	ptr->value = code;
	return;
}

int typeahead(int fd)
{

	T(("typeahead(%d) called", fd));
	SP->_checkfd = fd;
	return OK;
}

int intrflush(WINDOW *win, bool flag)
{
	T(("intrflush(%x, %d) called", win, flag));
	return OK;
}

/* Turn the keypad on/off
 *
 * Note:  we flush the output because changing this mode causes some terminals
 * to emit different escape sequences for cursor and keypad keys.  If we don't
 * flush, then the next wgetch may get the escape sequence that corresponds to
 * the terminal state _before_ switching modes.
 */
int _nc_keypad(bool flag)
{
	if (flag  &&  keypad_xmit)
	{
	    putp(keypad_xmit);
	    (void) fflush(SP->_ofp);
	}
	else if (! flag  &&  keypad_local)
	{
	    putp(keypad_local);
	    (void) fflush(SP->_ofp);
	}

	if (SP->_keytry == UNINITIALISED)
	    init_keytry();
	return(OK);
}
