/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated for what's essentially a complete rewrite.
 *
 * $Id: dmenu.c,v 1.18 1996/04/25 17:31:17 jkh Exp $
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

#include "sysinstall.h"
#include <sys/types.h>

#define MAX_MENU		15

static Boolean exited;

int
dmenuDisplayFile(dialogMenuItem *tmp)
{
    systemDisplayHelp((char *)tmp->data);
    return DITEM_SUCCESS | DITEM_RESTORE;
}

int
dmenuSubmenu(dialogMenuItem *tmp)
{
    return (dmenuOpenSimple((DMenu *)(tmp->data)) ? DITEM_SUCCESS : DITEM_FAILURE) |
	DITEM_RESTORE | DITEM_RECREATE;
}

int
dmenuSystemCommand(dialogMenuItem *self)
{
    WINDOW *w = NULL;	/* Keep lint happy */

    /* If aux is set, the command is known not to produce any screen-spoiling output */
    if (!self->aux)
	w = savescr();
    systemExecute((char *)self->data);
    if (!self->aux)
	restorescr(w);
    return DITEM_SUCCESS;
}

int
dmenuSystemCommandBox(dialogMenuItem *tmp)
{
    use_helpfile(NULL);
    use_helpline("Select OK to dismiss this dialog");
    dialog_prgbox(tmp->title, (char *)tmp->data, 22, 76, 1, 1);
    return DITEM_SUCCESS | DITEM_RESTORE;
}

int
dmenuExit(dialogMenuItem *tmp)
{
    exited = TRUE;
    return DITEM_LEAVE_MENU;
}

int
dmenuSetVariable(dialogMenuItem *tmp)
{
    variable_set((char *)tmp->data);
    return DITEM_SUCCESS;
}

int
dmenuSetFlag(dialogMenuItem *tmp)
{
    if (*((unsigned int *)tmp->data) & tmp->aux)
	*((unsigned int *)tmp->data) &= ~tmp->aux;
    else
	*((unsigned int *)tmp->data) |= tmp->aux;
    return DITEM_SUCCESS;
}

int
dmenuSetValue(dialogMenuItem *tmp)
{
    *((unsigned int *)tmp->data) = tmp->aux;
    return DITEM_SUCCESS;
}

/* Traverse menu but give user no control over positioning */
Boolean
dmenuOpenSimple(DMenu *menu)
{
    int choice, scroll, curr, max;

    choice = scroll = curr = max = 0;
    return dmenuOpen(menu, &choice, &scroll, &curr, &max);
}

/* Work functions for the state hook */
int
dmenuFlagCheck(dialogMenuItem *item)
{
    return (*((unsigned int *)item->data) & item->aux);
}

int
dmenuVarCheck(dialogMenuItem *item)
{
    char *w, *cp, *cp2, tmp[256];

    w = (char *)item->aux;
    if (!w)
	w = (char *)item->data;
    if (!w)
	return FALSE;
    strncpy(tmp, w, 256);
    if ((cp = index(tmp, '=')) != NULL) {
        *(cp++) = '\0';
        cp2 = getenv(tmp);
        if (cp2)
            return !strcmp(cp, cp2);
        else
            return FALSE;
    }
    else
        return (int)getenv(tmp);
}

int
dmenuRadioCheck(dialogMenuItem *item)
{
    return (*((unsigned int *)item->data) == item->aux);
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
    int n, rval = 0;

    /* Count up all the items */
    for (n = 0; menu->items[n].title; n++);

    while (1) {
	char buf[FILENAME_MAX];

	/* Any helpful hints, put 'em up! */
	use_helpline(menu->helpline);
	use_helpfile(systemHelpFile(menu->helpfile, buf));

	/* Pop up that dialog! */
	dialog_clear();
	if (menu->type & DMENU_NORMAL_TYPE)
	    rval = dialog_menu((u_char *)menu->title, (u_char *)menu->prompt, -1, -1,
			       menu_height(menu, n), -n, menu->items, NULL, choice, scroll);

	else if (menu->type & DMENU_RADIO_TYPE)
	    rval = dialog_radiolist((u_char *)menu->title, (u_char *)menu->prompt, -1, -1,
				    menu_height(menu, n), -n, menu->items, NULL);

	else if (menu->type & DMENU_CHECKLIST_TYPE)
	    rval = dialog_checklist((u_char *)menu->title, (u_char *)menu->prompt, -1, -1,
				    menu_height(menu, n), -n, menu->items, NULL);
	else
	    msgFatal("Menu: `%s' is of an unknown type\n", menu->title);
	clearok(stdscr, TRUE);
	if (exited) {
	    exited = FALSE;
	    return TRUE;
	}
	else if (rval)
	    return FALSE;
	else if (menu->type & DMENU_SELECTION_RETURNS)
	    return TRUE;
    }
}
