/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated for what's essentially a complete rewrite.
 *
 * $Id: dmenu.c,v 1.2 1995/04/27 18:03:52 jkh Exp $
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

#define MAX_MENU		15

static DMenuItem shellAction = { NULL, NULL, DMENU_SHELL_ESCAPE, NULL, 0 };

/* Traverse over an internal menu */
void
dmenuOpen(DMenu *menu, int *choice, int *scroll, int *curr, int *max)
{
    char result[FILENAME_MAX];
    char **nitems = NULL;
    DMenuItem *tmp;
    int rval = 0, n = 0;

    /* First, construct the menu */
    for (tmp = menu->items; tmp->title; tmp++) {
	if (!tmp->disabled) {
	    char *addme = NULL;
	    char *title = tmp->title;
	    char *prompt = tmp->prompt;

	    if (menu->options & DMENU_RADIO_TYPE) {
		if (*title == '*') {
		    addme = "ON";
		    ++title;
		}
		else
		    addme = "OFF";
	    }
	    nitems = item_add_pair(nitems, title, prompt, curr, max);
	    if (addme)
		nitems = item_add(nitems, addme, curr, max);
	    ++n;
	}
    }
    nitems = item_add(nitems, NULL, curr, max); /* Terminate it */

    while (1) {
	/* Any helpful hints, put 'em up! */
	if (menu->helpline)
	    use_helpline(menu->helpline);
	if (menu->helpfile)
	    use_helpfile(menu->helpfile);

	/* Pop up that dialog! */
	if (menu->options & DMENU_NORMAL_TYPE) {
	    rval = dialog_menu((unsigned char *)menu->title,
			       (unsigned char *)menu->prompt,
			       -1, -1,
			       n > MAX_MENU ? MAX_MENU : n,
			       n,
			       (unsigned char **)nitems,
			       (unsigned char *)result,
			       choice, scroll);
	}
	else if (menu->options & DMENU_RADIO_TYPE) {
	    rval = dialog_radiolist((unsigned char *)menu->title,
				    (unsigned char *)menu->prompt,
				    -1, -1,
				    n > MAX_MENU ? MAX_MENU : n,
				    n,
				    (unsigned char **)nitems,
				    (unsigned char *)result);
	}
	dialog_clear();
	if (!rval) {
	    for (tmp = menu->items; tmp->title; tmp++)
		if (!strcmp(result,
			    (*tmp->title == '*') ? tmp->title + 1 :
			     tmp->title))
		    break;
	    if (!tmp->title)
		msgFatal("Menu item `%s' not found??", result);
	}
	else if (rval == 255)
	    tmp = &shellAction;
	else {
	    items_free(nitems, curr, max);
	    return;
	}
	switch (tmp->type) {
	    /* User whapped ESC twice and wants a sub-shell */
	case DMENU_SHELL_ESCAPE:
	    systemShellEscape();
	    break;

	    /* We want to simply display a file */
	case DMENU_DISPLAY_FILE:
	    systemDisplayFile((char *)tmp->ptr);
	    break;

	    /* It's a sub-menu; recurse on it */
	case DMENU_SUBMENU: {
	    int choice, scroll, curr, max;

	    choice = scroll = curr = max = 0;
	    dmenuOpen((DMenu *)tmp->ptr, &choice, &scroll, &curr, &max);
	    break;
	}

	    /* Execute it as a system command */
	case DMENU_SYSTEM_COMMAND:
	    (void)systemExecute((char *)tmp->ptr);
	    break;

	    /* Same as above, but execute it in a prgbox */
	case DMENU_SYSTEM_COMMAND_BOX:
	    dialog_prgbox(tmp->title, (char *)tmp->ptr, 22, 76, 1, 1);
	    break;

	case DMENU_CALL:
	    if (((int (*)())tmp->ptr)()) {
		items_free(nitems, curr, max);
		return;
	    }
	    break;

	case DMENU_CANCEL:
	    items_free(nitems, curr, max);
	    return;

	case DMENU_SET_VARIABLE: {
	    Variable *newvar;

	    if (!index((char *)tmp->ptr, '='))
		msgWarn("Improperly formatted variable: %s", tmp->ptr);
	    putenv((char *)tmp->ptr);
	    newvar = (Variable *)malloc(sizeof(Variable));
	    if (!newvar)
		msgFatal("Out of Memory!");
	    strncpy(newvar->value, tmp->ptr, 1024);
	    newvar->next = VarHead;
	    VarHead = newvar;
	    msgInfo("Setting option %s", newvar->value);
	}
	    break;
	    
	default:
	    msgFatal("Don't know how to deal with menu type %d", tmp->type);
	}

	/* Did the user want to make this a single-selection menu? */
	if (menu->options & DMENU_SELECTION_RETURNS) {
	    items_free(nitems, curr, max);
	    return;
	}
    }
}


