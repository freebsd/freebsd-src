/*
 * Copyright 1998-2008 The OpenLDAP Foundation.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted only as authorized by the OpenLDAP
 * Public License.
 *
 * A copy of this license is available in file LICENSE in the
 * top-level directory of the distribution or, alternatively, at
 * <http://www.OpenLDAP.org/license.html>.
 */
/* Copyright 1997, 1998, 1999 Computing Research Labs,
 * New Mexico State University
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COMPUTING RESEARCH LAB OR NEW MEXICO STATE UNIVERSITY BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
 * OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * This work is part of OpenLDAP Software <http://www.openldap.org/>.
 * $OpenLDAP: pkg/ldap/libraries/liblunicode/utbm/utbmstub.c,v 1.8 2008/01/07 23:20:05 kurt Exp $
 * $Id: utbmstub.c,v 1.1 1999/09/21 15:45:18 mleisher Exp $
 */

#include "utbm.h"

/*
 * This should be redefined to use the `isspace' function available in the
 * Unicode support on the platform where this is being used.
 */
#define _platform_isspace(x) 0

/*
 * Return non-zero for any character that should be considered the equivalent
 * of a space character.  Return zero otherwise.
 */
int
_utbm_isspace(ucs4_t c, int compress)
{
    if (compress)
      return (c == 0x09 || c == 0x0a || c == 0x0d ||
              c == 0x2028 || c == 0x2029 || _platform_isspace(c)) ? 1 : 0;

    return _platform_isspace(c);

}

/*
 * Return non-zero if the character is a control character, or zero otherwise.
 */
int
_utbm_iscntrl(ucs4_t c)
{
    return 0;
}

/*
 * Return non-zero if the character is a non-spacing character, or zero
 * otherwise.
 */
int
_utbm_nonspacing(ucs4_t c)
{
    return 0;
}

/*
 * Convert a character to lower case.
 */
ucs4_t
_utbm_tolower(ucs4_t c)
{
    return c;
}

/*
 * Convert a character to upper case.
 */
ucs4_t
_utbm_toupper(ucs4_t c)
{
    return c;
}

/*
 * Convert a character to title case.
 */
ucs4_t
_utbm_totitle(ucs4_t c)
{
    return c;
}
