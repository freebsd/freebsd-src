/****************************************************************************
 * Copyright (c) 1998 Free Software Foundation, Inc.                        *
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
 *  Author: Zeyd M. Ben-Halim <zmbenhal@netcom.com> 1992,1995               *
 *     and: Eric S. Raymond <esr@snark.thyrsus.com>                         *
 ****************************************************************************/

/*
 *	lib_slkset.c
 *      Set soft label text.
 */
#include <curses.priv.h>
#include <ctype.h>

MODULE_ID("$Id: lib_slkset.c,v 1.3 1998/02/11 12:13:56 tom Exp $")

int
slk_set(int i, const char *astr, int format)
{
SLK *slk = SP->_slk;
size_t len;
const char *str = astr;
const char *p;

	T((T_CALLED("slk_set(%d, \"%s\", %d)"), i, str, format));

	if (slk == NULL || i < 1 || i > slk->labcnt || format < 0 || format > 2)
		returnCode(ERR);
	if (str == NULL)
		str = "";

	while (isspace(*str)) str++; /* skip over leading spaces  */
	p = str;
	while (isprint(*p)) p++;     /* The first non-print stops */

	--i; /* Adjust numbering of labels */

	len = (size_t)(p - str);
	if (len > (unsigned)slk->maxlen)
	  len = slk->maxlen;
	if (len==0)
	  slk->ent[i].text[0] = 0;
	else
	  (void) strncpy(slk->ent[i].text, str, len);
	memset(slk->ent[i].form_text,' ', (unsigned)slk->maxlen);
	slk->ent[i].text[slk->maxlen] = 0;
	/* len = strlen(slk->ent[i].text); */

	switch(format) {
	case 0: /* left-justified */
		memcpy(slk->ent[i].form_text,
		       slk->ent[i].text,
		       len);
		break;
	case 1: /* centered */
		memcpy(slk->ent[i].form_text+(slk->maxlen - len)/2,
		       slk->ent[i].text,
		       len);
		break;
	case 2: /* right-justified */
		memcpy(slk->ent[i].form_text+ slk->maxlen - len,
		       slk->ent[i].text,
		       len);
		break;
	}
	slk->ent[i].form_text[slk->maxlen] = 0;
	slk->ent[i].dirty = TRUE;
	returnCode(OK);
}
