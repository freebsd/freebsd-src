/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated for what's essentially a complete rewrite.
 *
 * $Id: dmenu.c,v 1.12.2.3 1995/10/15 12:40:59 jkh Exp $
 *
 * Copyright (c) 1995
 *	Jordan Hubbard.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    verbatim and that no modifications are made prior to this
 *    point in the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Jordan Hubbard
 *	for the FreeBSD Project.
 * 4. The name of Jordan Hubbard or the FreeBSD project may not be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JORDAN HUBBARD ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL JORDAN HUBBARD OR HIS PETS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, LIFE OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "sysinstall.h"
#include <sys/types.h>

#define MAX_MENU		18

/* Traverse menu but give user no control over positioning */
Boolean
dmenuOpenSimple(DMenu *menu)
{
    int choice, scroll, curr, max;

    choice = scroll = curr = max = 0;
    return dmenuOpen(menu, &choice, &scroll, &curr, &max);
}

/* Work functions for the state hook */
char *
dmenuFlagCheck(DMenuItem *item)
{
    if (*((unsigned int *)item->ptr) & item->parm)
	return "ON";
    return "OFF";
}

char *
dmenuVarCheck(DMenuItem *item)
{
    char *w, *cp, *cp2, tmp[256];

    w = (char *)item->parm;
    if (!w)
	w = (char *)item->ptr;
    if (!w)
	return "OFF";
    strncpy(tmp, w, 256);
    if ((cp = index(tmp, '=')) != NULL) {
        *(cp++) = '\0';
        cp2 = getenv(tmp);
        if (cp2)
            return !strcmp(cp, cp2) ? "ON" : "OFF";
        else
            return "OFF";
    }
    else
        return getenv(tmp) ? "ON" : "OFF";
}

char *
dmenuRadioCheck(DMenuItem *item)
{
    if (*((unsigned int *)item->ptr) == item->parm)
	return "ON";
    return "OFF";
}

static char *
checkHookVal(DMenuItem *item)
{

    if (!item->check)
	return "OFF";
    return (*item->check)(item);
}

static int
menu_height(DMenu *menu, int n)
{
    int max;
    char *t;

    for (t = menu->title, max = MAX_MENU; *t; t++) {
	if (*t == '\n')
	    --max;
    }
    return n > max ? max : n;
}

/* Traverse over an internal menu */
Boolean
dmenuOpen(DMenu *menu, int *choice, int *scroll, int *curr, int *max)
{
    char result[FILENAME_MAX];
    char **nitems = NULL;
    DMenuItem *tmp;
    int rval = 0, n = 0;

    /* First, construct the menu */
    for (tmp = menu->items; tmp->title; tmp++) {
	if (!tmp->disabled) {
	    nitems = item_add_pair(nitems, tmp->title, tmp->prompt, curr, max);
	    if (menu->options & (DMENU_RADIO_TYPE | DMENU_MULTIPLE_TYPE))
		nitems = item_add(nitems, checkHookVal(tmp), curr, max);
	    ++n;
	}
    }
    nitems = item_add(nitems, NULL, curr, max); /* Terminate it */

    while (1) {
	char buf[FILENAME_MAX];

	/* Any helpful hints, put 'em up! */
	use_helpline(menu->helpline);
	use_helpfile(systemHelpFile(menu->helpfile, buf));

	/* Pop up that dialog! */
	if (menu->options & DMENU_NORMAL_TYPE)
	    rval = dialog_menu((u_char *)menu->title, (u_char *)menu->prompt, -1, -1,
			       menu_height(menu, n), n, (u_char **)nitems, (u_char *)result, choice, scroll);

	else if (menu->options & DMENU_RADIO_TYPE)
	    rval = dialog_radiolist((u_char *)menu->title, (u_char *)menu->prompt, -1, -1,
				    menu_height(menu, n), n, (u_char **)nitems, (u_char *)result);

	else if (menu->options & DMENU_MULTIPLE_TYPE)
	    rval = dialog_checklist((u_char *)menu->title, (u_char *)menu->prompt, -1, -1,
				    menu_height(menu, n), n, (u_char **)nitems, (u_char *)result);

	/* This seems to be the only technique that works for getting the display to look right */
	dialog_clear();

	if (!rval) {
	    if (menu->options & DMENU_MULTIPLE_TYPE) {
		if (menu->options & DMENU_CALL_FIRST)
		    tmp = &(menu->items[0]);
		else {
		    if (decode_and_dispatch_multiple(menu, result) || menu->options & DMENU_SELECTION_RETURNS) {
			items_free(nitems, curr, max);
			return TRUE;
		    }
		}
	    }
	    else {
		if ((tmp = decode(menu, result)) == NULL)
		    return FALSE;
	    }
	    if (dispatch(tmp, result) == RET_DONE || (menu->options & DMENU_SELECTION_RETURNS)) {
		items_free(nitems, curr, max);
		return TRUE;
	    }
	}
	else {
	    items_free(nitems, curr, max);
	    return FALSE;
	}
    }
}
