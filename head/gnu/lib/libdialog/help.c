/***************************************************************
 *
 * Program:	help.c
 * Author:	Marc van Kempen
 * Desc:	get help
 *
 *
 * Copyright (c) 1995, Marc van Kempen
 *
 * All rights reserved.
 *
 * This software may be used, modified, copied, distributed, and
 * sold, in both source and binary form provided that the above
 * copyright and these terms are retained, verbatim, as the first
 * lines of this file.  Under no circumstances is the author
 * responsible for the proper functioning of this software, nor does
 * the author assume any responsibility for damages incurred with
 * its use.
 *
 ***************************************************************/

#include <stdlib.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <dialog.h>

static char	_helpfilebuf[MAXPATHLEN];
static char	_helplinebuf[77];	  /* limit the helpline to 76 characters */
static char	*_helpfile = NULL;
static char	*_helpline = NULL;

/******************************************************************
 *
 * 	helpfile routines
 *
 ******************************************************************/

void
use_helpfile(char *hfile)
/*
 * desc: set the helpfile to be opened on pressing F1 to <helpfile>
 */
{
    if (hfile != NULL) {
	_helpfile = _helpfilebuf;
	strcpy(_helpfile, hfile);
    } else {
	_helpfile = NULL;
    }

    return;
} /* use_helpfile() */

void
display_helpfile(void)
/*
 * desc: display the current helpfile in a window
 */
{
    WINDOW	*w;
    FILE	*f;
    struct stat sb;
    char	msg[80], *buf;
    static int	in_help = FALSE;
    char        *savehline = NULL;

    if (in_help) return;	/* dont call help when you're in help */

    if (_helpfile != NULL) {
	if ((w = dupwin(newscr)) == NULL) {
	    dialog_notify("No memory to dup previous screen\n");
	    return;
	}
	if ((f = fopen(_helpfile, "r")) == NULL) {
	    sprintf(msg, "Can't open helpfile : %s\n", _helpfile);
	    dialog_notify(msg);
	    return;
	}
	if (fstat(fileno(f), &sb)) {
	    sprintf(msg, "Can't stat helpfile : %s\n", _helpfile);
	    dialog_notify(msg);
	    return;
	}
	if ((buf = (char *) malloc( sb.st_size )) == NULL) {
	    sprintf(msg, "Could not malloc space for helpfile : %s\n", _helpfile);
	    dialog_notify(msg);
	    return;
	}
	if (fread(buf, 1, sb.st_size, f) != sb.st_size) {
	    sprintf(msg, "Could not read entire help file : %s", _helpfile);
	    dialog_notify(msg);
	    free(buf);
	    return;
	}
	buf[sb.st_size] = 0;
	in_help = TRUE;
	savehline = get_helpline();
	use_helpline("Use arrowkeys, PgUp, PgDn, Home and End to move through text");
	dialog_mesgbox("Online help", buf, LINES-4, COLS-4);
	restore_helpline(savehline);
	in_help = FALSE;
	touchwin(w);
	wrefresh(w);
	delwin(w);
	free(buf);
    } else {
	/* do nothing */
    }

    return;
} /* display_helpfile() */


/******************************************************************
 *
 * 	helpline routines
 *
 ******************************************************************/

void
use_helpline(char *hline)
/*
 * desc: set the helpline to printed in dialogs
 */
{
    if (hline) {
	_helpline = _helplinebuf;
	if (strlen(hline) > 76) {
	    /* only display the first 76 characters in the helpline */
	    strncpy(_helpline, hline, 76);
	    _helpline[76] = 0;
	} else {
	    strcpy(_helpline, hline);
	}
    } else {
	_helpline = NULL;
    }

    return;
} /* use_helpline() */

void
display_helpline(WINDOW *w, int y, int width)
/*
 * desc: display the helpline at the given coordinates <y, x> in the window <w>
 */
{
    if (_helpline != NULL) {
	if (strlen(_helpline) > width - 6) {
	    _helpline[width - 6] = 0;
	}
	wmove(w, y, (int) (width - strlen(_helpline)- 4) / 2);
	wattrset(w, title_attr);
	waddstr(w, "[ ");
	waddstr(w, _helpline);
	waddstr(w, " ]");
    } else {
	/* do nothing */
    }

    return;
}

char *
get_helpline(void)
/*
 * desc: allocate new space, copy the helpline to it and return a pointer to it
 */
{
    char *hlp;

    if (_helpline) {
        hlp = (char *) malloc( strlen(_helpline) + 1 );
        strcpy(hlp, _helpline);
    } else {
        hlp = NULL;
    }

    return(hlp);
} /* get_helpline() */

void
restore_helpline(char *helpline)
/*
 * Desc: set the helpline to <helpline> and free the space allocated to it
 */
{
    use_helpline(helpline);
    free(helpline);

    return;
} /* restore_helpline() */
