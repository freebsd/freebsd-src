/*
 *  $Id: help.c,v 1.2 2011/06/25 00:27:16 tom Exp $
 *
 *  help.c -- implements the help dialog
 *
 *  Copyright 2011	Thomas E. Dickey
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License, version 2.1
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program; if not, write to
 *	Free Software Foundation, Inc.
 *	51 Franklin St., Fifth Floor
 *	Boston, MA 02110, USA.
 */

#include <dialog.h>

/*
 * Display a help-file as a textbox widget.
 */
int
dialog_helpfile(const char *title,
		const char *file,
		int height,
		int width)
{
    int result = DLG_EXIT_ERROR;

    if (!dialog_vars.in_helpfile && file != 0 && *file != '\0') {
	dialog_vars.in_helpfile = TRUE;
	result = dialog_textbox(title, file, height, width);
	dialog_vars.in_helpfile = FALSE;
    }
    return (result);
}
