/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated for what's essentially a complete rewrite.
 *
 * $Id$
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
#include <sys/wait.h>

#define MAX_MENU	15

static DMenuItem shellAction = { NULL, NULL, MENU_SHELL_ESCAPE, NULL, 0 };

/* Traverse over an internal menu */
void
dmenuOpen(DMenu *menu, int *choice, int *scroll, int *curr, int *max)
{
    char result[FILENAME_MAX];
    char **nitems = NULL;
    DMenuItem *tmp;
    int rval, n = 0;
    char *sh = NULL;

    /* First, construct the menu */
    for (tmp = menu->items; tmp->title; tmp++) {
	if (!tmp->disabled) {
	    nitems = item_add(nitems, tmp->title, curr, max);
	    nitems = item_add(nitems, tmp->prompt, curr, max);
	    ++n;
	}
    }
    nitems = item_add(nitems, NULL, curr, max); /* Terminate it */

    /* Any helpful hints, put 'em up! */
    if (menu->helpline)
	use_helpline(menu->helpline);
    if (menu->helpfile)
	use_helpfile(menu->helpfile);

    while (1) {
	/* Pop up that dialog! */
	rval = dialog_menu((unsigned char *)menu->title,
			   (unsigned char *)menu->prompt,
			   -1, -1, n > MAX_MENU ? MAX_MENU : n, n,
			   (unsigned char **)nitems, (unsigned char *)result,
			   choice, scroll);
	dialog_clear();
	if (!rval) {
	    for (tmp = menu->items; tmp->title; tmp++)
		if (!strcmp(result, tmp->title))
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
	case MENU_SHELL_ESCAPE:
	    if (file_executable("/bin/sh"))
		sh = "/bin/sh";
	    else if (file_executable("/stand/sh"))
		sh = "/stand/sh";
	    else {
		msgWarn("No shell available, sorry!");
		break;
	    }
	    setenv("PS1", "freebsd% ", 1);
	    dialog_clear();
	    dialog_update();
	    move(0, 0);
	    standout();
	    addstr("Type `exit' to leave this shell and continue install.");
	    standend();
	    refresh();
	    end_dialog();
	    DialogActive = FALSE;
	    if (fork() == 0)
		execlp(sh, "-sh", 0);
	    else
		wait(NULL);
	    dialog_clear();
	    DialogActive = TRUE;
	    break;

	case MENU_DISPLAY_FILE: {
	    char buf[FILENAME_MAX], *cp, *fname = NULL;

	    if (file_readable((char *)tmp->ptr))
		fname = (char *)tmp->ptr;
	    else if ((cp = getenv("LANG")) != NULL) {
		snprintf(buf, FILENAME_MAX, "%s/%s", cp, tmp->ptr);
		if (file_readable(buf))
		    fname = buf;
	    }
	    else {
		snprintf(buf, FILENAME_MAX, "english/%s", tmp->ptr);
		if (file_readable(buf))
		    fname = buf;
	    }
	    if (!fname) {
		snprintf(buf, FILENAME_MAX, "The %s file is not provided on the 1.2MB floppy image.", (char *)tmp->ptr);
		dialog_msgbox("Sorry!", buf, -1, -1, 1);
		dialog_clear_norefresh();
	    }
	    else {
		dialog_clear_norefresh();
		dialog_textbox(tmp->title, fname, LINES, COLS);
		dialog_clear_norefresh();
	    }
	}
	    break;

	case MENU_SUBMENU: {
	    int choice, scroll, curr, max;

	    choice = scroll = curr = max = 0;
	    dmenuOpen((DMenu *)tmp->ptr, &choice, &scroll, &curr, &max);
	    break;
	}

	case MENU_SYSTEM_COMMAND:
	    (void)systemExecute((char *)tmp->ptr);
	    break;

	case MENU_CALL:
	    ((void (*)())tmp->ptr)();
	    break;

	case MENU_SET_VARIABLE: {
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
	}
	    break;
	    
	default:
	    msgFatal("Don't know how to deal with menu type %d", tmp->type);
	}
    }
}


