/****************************************************************************
 * Copyright (c) 1998,2000 Free Software Foundation, Inc.                   *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/****************************************************************************
 *  Author: Thomas E. Dickey <dickey@clark.net> 1998                        *
 ****************************************************************************/

/*
**	add_tries.c
**
**	Add keycode/string to tries-tree.
**
*/

#include <curses.priv.h>

MODULE_ID("$Id: add_tries.c,v 1.4 2000/12/10 02:55:07 tom Exp $")

#define SET_TRY(dst,src) if ((dst->ch = *src++) == 128) dst->ch = '\0'
#define CMP_TRY(a,b) ((a)? (a == b) : (b == 128))

NCURSES_EXPORT(void)
_nc_add_to_try(struct tries **tree, const char *str, unsigned short code)
{
    static bool out_of_memory = FALSE;
    struct tries *ptr, *savedptr;
    unsigned const char *txt = (unsigned const char *) str;

    if (txt == 0 || *txt == '\0' || out_of_memory || code == 0)
	return;

    if ((*tree) != 0) {
	ptr = savedptr = (*tree);

	for (;;) {
	    unsigned char cmp = *txt;

	    while (!CMP_TRY(ptr->ch, cmp)
		   && ptr->sibling != 0)
		ptr = ptr->sibling;

	    if (CMP_TRY(ptr->ch, cmp)) {
		if (*(++txt) == '\0') {
		    ptr->value = code;
		    return;
		}
		if (ptr->child != 0)
		    ptr = ptr->child;
		else
		    break;
	    } else {
		if ((ptr->sibling = typeCalloc(struct tries, 1)) == 0) {
		    out_of_memory = TRUE;
		    return;
		}

		savedptr = ptr = ptr->sibling;
		SET_TRY(ptr, txt);
		ptr->value = 0;

		break;
	    }
	}			/* end for (;;) */
    } else {			/* (*tree) == 0 :: First sequence to be added */
	savedptr = ptr = (*tree) = typeCalloc(struct tries, 1);

	if (ptr == 0) {
	    out_of_memory = TRUE;
	    return;
	}

	SET_TRY(ptr, txt);
	ptr->value = 0;
    }

    /* at this point, we are adding to the try.  ptr->child == 0 */

    while (*txt) {
	ptr->child = typeCalloc(struct tries, 1);

	ptr = ptr->child;

	if (ptr == 0) {
	    out_of_memory = TRUE;

	    while ((ptr = savedptr) != 0) {
		savedptr = ptr->child;
		free(ptr);
	    }

	    return;
	}

	SET_TRY(ptr, txt);
	ptr->value = 0;
    }

    ptr->value = code;
    return;
}
