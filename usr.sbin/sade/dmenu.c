/*
 * $FreeBSD$
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

#include "sade.h"
#include <errno.h>

#define MAX_MENU		15

static int
menu_height(DMenu *menu, int n)
{
    int max;
    char *t;

    max = MAX_MENU;
    if (StatusLine > 24)
	max += StatusLine - 24;
    for (t = menu->prompt; *t; t++) {
	if (*t == '\n')
	    --max;
    }
    return n > max ? max : n;
}

/* Traverse over an internal menu */
Boolean
dmenuOpen(DMenu *menu)
{
    int n, rval = 0;

    /* Count up all the items */
    for (n = 0; menu->items[n].title; n++)
	    ;

    while (1) {
	char buf[FILENAME_MAX];
	DIALOG_VARS save_vars;
	WINDOW *w = savescr();

	/* Any helpful hints, put 'em up! */
	dlg_save_vars(&save_vars);
	dialog_vars.help_line = menu->helpline;
	dialog_vars.help_file = systemHelpFile(menu->helpfile, buf);
	dlg_clear();
	/* Pop up that dialog! */
	if (menu->type & DMENU_NORMAL_TYPE) {
	    rval = xdialog_menu(menu->title, menu->prompt,
		-1, -1, menu_height(menu, n), n, menu->items);
	} else if (menu->type & DMENU_RADIO_TYPE) {
	    rval = xdialog_radiolist(menu->title, menu->prompt,
		-1, -1, menu_height(menu, n), n, menu->items);
	} else {
	    msgFatal("Menu: `%s' is of an unknown type\n", menu->title);
	}
	dlg_restore_vars(&save_vars);
	if (rval) {
	    restorescr(w);
	    return FALSE;
	} else if (menu->type & DMENU_SELECTION_RETURNS) {
	    restorescr(w);
	    return TRUE;
	} else
	    delwin(w);
    }
}
