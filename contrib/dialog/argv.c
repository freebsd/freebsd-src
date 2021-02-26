/*
 * $Id: argv.c,v 1.13 2020/03/26 02:55:37 tom Exp $
 *
 *  argv - Reusable functions for argv-parsing.
 *
 *  Copyright 2011-2018,2020	Thomas E. Dickey
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
    size_t n, k;
    int pass;
    size_t length = strlen(blob);
    char **result = 0;

#ifdef HAVE_DLG_TRACE
    if (dialog_state.trace_output) {
	DLG_TRACE(("# dlg_string_to_argv:\n"));
	DLG_TRACE(("# given:\n"));
	for (n = k = 0; n < length; ++n) {
	    if (blob[n] == '\n') {
		DLG_TRACE(("#%s\t%.*s\\n\n",
			   k ? "+" : "",
			   (int) (n - k), blob + k));
		k = n + 1;
	    }
	}
	if (n > k) {
	    DLG_TRACE(("#%s\t%.*s\n",
		       k ? "+" : "",
		       (int) (n - k), blob + k));
	}
	DLG_TRACE(("# result:\n"));
    }
#endif
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
		    if (pass) {
			result[count] = param;
		    }
		    ++count;
		    inparm = TRUE;
		}
	    } else if (!quoted && isspace(UCH(blob[n]))) {
		if (inparm) {
		    if (pass) {
			*param = '\0';
		    }
		    ++param;
		    inparm = FALSE;
		}
	    } else {
		if (blob[n] == '\\') {
		    size_t n1 = (n + 1);
		    bool ignore = FALSE;
		    if (n1 == length) {
			break;	/* The string is terminated by a backslash */
		    } else if ((blob[n1] == '\\') ||
			       (blob[n1] == '"') ||
			       (ignore = (blob[n1] == '\n'))) {
			/* eat the backslash */
			if (pass) {
			    --length;
			    for (k = n; k < length; ++k)
				blob[k] = blob[k + 1];
			    blob[length] = '\0';
			} else {
			    ++param;	/* pretend I ate it */
			}
			if (ignore)
			    continue;
		    }
		}
		if (!inparm) {
		    if (pass) {
			result[count] = param;
		    }
		    ++count;
		    inparm = TRUE;
		}
		if (pass) {
		    *param = blob[n];
		}
		++param;
	    }
	}

	if (pass) {
	    *param = '\0';
	} else {
	    if (count) {
		result = dlg_calloc(char *, count + 1);
		assert_ptr(result, "string_to_argv");
	    } else {
		break;		/* no tokens found */
	    }
	}
    }
#ifdef HAVE_DLG_TRACE
    if (result != 0) {
	for (n = 0; result[n] != 0; ++n) {
	    DLG_TRACE(("#\targv[%d] = %s\n", (int) n, result[n]));
	}
    }
#endif
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
