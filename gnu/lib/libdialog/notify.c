/*
 * File: 	notify.c
 * Author: 	Marc van Kempen
 * Desc:	display a notify box with a message
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
 */


#include <dialog.h>
#include <stdio.h>

void
dialog_notify(char *msg)
/* 
 * Desc: display an error message
 */
{
    int li, co, mco;
    char *p;
    WINDOW *w;

    /* determine # of lines in msg and max colwidth */
    li = 0;
    co = 0;
    mco = 0;
    p = msg;
    while (*p) {
	if (*p == '\n') {
	    li++;
	    if (co > mco) mco = co;
	    co = 0;
	}
	p++;
	co++;
    }
    if (co < mco) co = mco;

    li += 5;
    if (li > LINES) li = LINES;
    co += 4;
    if (co < 20) co = 20;
    if (co > COLS) co = COLS;

    w = dupwin(curscr);
    dialog_msgbox("Message", msg, li, co, TRUE);
    wrefresh(w);
    delwin(w);

    return;

} /* dialog_notify() */

