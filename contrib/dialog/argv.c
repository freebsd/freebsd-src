/*
 * $Id: argv.c,v 1.1 2011/03/02 09:56:39 tom Exp $
 *
 *  argv - Reusable functions for argv-parsing.
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
#include <string.h>

/*
 * Convert a string to an argv[], returning a char** index (which must be
 * freed by the caller).  The string is modified (replacing gaps between
 * tokens with nulls).
 */
char **
dlg_string_to_argv(char *blob)
{
    size_t n;
    int pass;
    size_t length = strlen(blob);
    char **result = 0;

    for (pass = 0; pass < 2; ++pass) {
	bool inparm = FALSE;
	bool quoted = FALSE;
	char *param = blob;
	size_t count = 0;

	for (n = 0; n < length; ++n) {
	    if (quoted && blob[n] == '"') {
		quoted = FALSE;
	    } else if (blob[n] == '"') {
		quoted = TRUE;
		if (!inparm) {
		    if (pass)
			result[count] = param;
		    ++count;
		    inparm = TRUE;
		}
	    } else if (blob[n] == '\\') {
		if (quoted && !isspace(UCH(blob[n + 1]))) {
		    if (!inparm) {
			if (pass)
			    result[count] = param;
			++count;
			inparm = TRUE;
		    }
		    if (pass) {
			*param++ = blob[n];
			*param++ = blob[n + 1];
		    }
		}
		++n;
	    } else if (!quoted && isspace(UCH(blob[n]))) {
		inparm = FALSE;
		if (pass) {
		    *param++ = '\0';
		}
	    } else {
		if (!inparm) {
		    if (pass)
			result[count] = param;
		    ++count;
		    inparm = TRUE;
		}
		if (pass) {
		    *param++ = blob[n];
		}
	    }
	}

	if (!pass) {
	    if (count) {
		result = dlg_calloc(char *, count + 1);
		assert_ptr(result, "string_to_argv");
	    } else {
		break;		/* no tokens found */
	    }
	} else {
	    *param = '\0';
	}
    }
    return result;
}

/*
 * Count the entries in an argv list.
 */
int
dlg_count_argv(char **argv)
{
    int result = 0;

    if (argv != 0) {
	while (argv[result] != 0)
	    ++result;
    }
    return result;
}

int
dlg_eat_argv(int *argcp, char ***argvp, int start, int count)
{
    int k;

    *argcp -= count;
    for (k = start; k <= *argcp; k++)
	(*argvp)[k] = (*argvp)[k + count];
    (*argvp)[*argcp] = 0;
    return TRUE;
}
