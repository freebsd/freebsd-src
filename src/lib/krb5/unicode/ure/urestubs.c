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
/*
 * Copyright 1997, 1998, 1999 Computing Research Labs,
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
 * $OpenLDAP: pkg/ldap/libraries/liblunicode/ure/urestubs.c,v 1.16 2008/01/07 23:20:05 kurt Exp $
 * $Id: urestubs.c,v 1.2 1999/09/21 15:47:44 mleisher Exp $"
 */

#include "k5-int.h"

#include "ure.h"

#include "ucdata.h"

/*
 * This file contains stub routines needed by the URE package to test
 * character properties and other Unicode implementation specific details.
 */

/*
 * This routine should return the lower case equivalent for the character or,
 * if there is no lower case quivalent, the character itself.
 */
ucs4_t _ure_tolower(ucs4_t c)
{
    return uctoupper(c);
}

static struct ucmaskmap {
	unsigned long mask1;
	unsigned long mask2;
} masks[32] = {
	{ UC_MN, 0 },	/* _URE_NONSPACING */
	{ UC_MC, 0 },	/* _URE_COMBINING */
	{ UC_ND, 0 },	/* _URE_NUMDIGIT */
	{ UC_NL|UC_NO, 0 },	/* _URE_NUMOTHER */
	{ UC_ZS, 0 },	/* _URE_SPACESEP */
	{ UC_ZL, 0 },	/* _URE_LINESEP */
	{ UC_ZP, 0 },	/* _URE_PARASEP */
	{ UC_CC, 0 },	/* _URE_CNTRL */
	{ UC_CO, 0 },	/* _URE_PUA */

	{ UC_LU, 0 },	/* _URE_UPPER */
	{ UC_LL, 0 },	/* _URE_LOWER */
	{ UC_LT, 0 },	/* _URE_TITLE */
	{ UC_LM, 0 },	/* _URE_MODIFIER */
	{ UC_LO, 0 },	/* _URE_OTHERLETTER */
	{ UC_PD, 0 },	/* _URE_DASHPUNCT */
	{ UC_PS, 0 },	/* _URE_OPENPUNCT */
	{ UC_PC, 0 },	/* _URE_CLOSEPUNCT */
	{ UC_PO, 0 },	/* _URE_OTHERPUNCT */
	{ UC_SM, 0 },	/* _URE_MATHSYM */
	{ UC_SC, 0 },	/* _URE_CURRENCYSYM */
	{ UC_SO, 0 },	/* _URE_OTHERSYM */

	{ UC_L, 0 },	/* _URE_LTR */
	{ UC_R, 0 },	/* _URE_RTL */

	{ 0, UC_EN },	/* _URE_EURONUM */
	{ 0, UC_ES },	/* _URE_EURONUMSEP */
	{ 0, UC_ET },	/* _URE_EURONUMTERM */
	{ 0, UC_AN },	/* _URE_ARABNUM */
	{ 0, UC_CS },	/* _URE_COMMONSEP */

	{ 0, UC_B },	/* _URE_BLOCKSEP */
	{ 0, UC_S },	/* _URE_SEGMENTSEP */

	{ 0, UC_WS },	/* _URE_WHITESPACE */
	{ 0, UC_ON }	/* _URE_OTHERNEUT */
};


/*
 * This routine takes a set of URE character property flags (see ure.h) along
 * with a character and tests to see if the character has one or more of those
 * properties.
 */
int
_ure_matches_properties(unsigned long props, ucs4_t c)
{
	int i;
	unsigned long mask1=0, mask2=0;

	for( i=0; i<32; i++ ) {
		if( props & (1 << i) ) {
			mask1 |= masks[i].mask1;
			mask2 |= masks[i].mask2;
		}
	}

	return ucisprop( mask1, mask2, c );
}
