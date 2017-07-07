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
 * $OpenLDAP: pkg/ldap/libraries/liblunicode/ure/ure.h,v 1.15 2008/01/07 23:20:05 kurt Exp $
 * $Id: ure.h,v 1.2 1999/09/21 15:47:44 mleisher Exp $
 */

#ifndef _h_ure
#define _h_ure

#include "k5-int.h"

#include <stdio.h>

/*
 * Set of character class flags.
 */
#define _URE_NONSPACING  0x00000001
#define _URE_COMBINING   0x00000002
#define _URE_NUMDIGIT    0x00000004
#define _URE_NUMOTHER    0x00000008
#define _URE_SPACESEP    0x00000010
#define _URE_LINESEP     0x00000020
#define _URE_PARASEP     0x00000040
#define _URE_CNTRL       0x00000080
#define _URE_PUA         0x00000100

#define _URE_UPPER       0x00000200
#define _URE_LOWER       0x00000400
#define _URE_TITLE       0x00000800
#define _URE_MODIFIER    0x00001000
#define _URE_OTHERLETTER 0x00002000
#define _URE_DASHPUNCT   0x00004000
#define _URE_OPENPUNCT   0x00008000
#define _URE_CLOSEPUNCT  0x00010000
#define _URE_OTHERPUNCT  0x00020000
#define _URE_MATHSYM     0x00040000
#define _URE_CURRENCYSYM 0x00080000
#define _URE_OTHERSYM    0x00100000

#define _URE_LTR         0x00200000
#define _URE_RTL         0x00400000

#define _URE_EURONUM     0x00800000
#define _URE_EURONUMSEP  0x01000000
#define _URE_EURONUMTERM 0x02000000
#define _URE_ARABNUM     0x04000000
#define _URE_COMMONSEP   0x08000000

#define _URE_BLOCKSEP    0x10000000
#define _URE_SEGMENTSEP  0x20000000

#define _URE_WHITESPACE  0x40000000
#define _URE_OTHERNEUT   0x80000000

/*
 * Error codes.
 */
#define _URE_OK               0
#define _URE_UNEXPECTED_EOS   -1
#define _URE_CCLASS_OPEN      -2
#define _URE_UNBALANCED_GROUP -3
#define _URE_INVALID_PROPERTY -4

/*
 * Options that can be combined for searching.
 */
#define URE_IGNORE_NONSPACING      0x01
#define URE_DOT_MATCHES_SEPARATORS 0x02

typedef krb5_ui_4 ucs4_t;
typedef krb5_ui_2 ucs2_t;

/*
 * Opaque type for memory used when compiling expressions.
 */
typedef struct _ure_buffer_t *ure_buffer_t;

/*
 * Opaque type for the minimal DFA used when matching.
 */
typedef struct _ure_dfa_t *ure_dfa_t;

/*************************************************************************
 *
 * API.
 *
 *************************************************************************/

ure_buffer_t ure_buffer_create (void);

void ure_buffer_free (ure_buffer_t buf);

ure_dfa_t
ure_compile (ucs2_t *re, unsigned long relen,
		    int casefold, ure_buffer_t buf);

void ure_dfa_free (ure_dfa_t dfa);

void ure_write_dfa (ure_dfa_t dfa, FILE *out);

int
ure_exec (ure_dfa_t dfa, int flags, ucs2_t *text,
		 unsigned long textlen, unsigned long *match_start,
		 unsigned long *match_end);

/*************************************************************************
 *
 * Prototypes for stub functions used for URE.  These need to be rewritten to
 * use the Unicode support available on the system.
 *
 *************************************************************************/

ucs4_t _ure_tolower (ucs4_t c);

int
_ure_matches_properties (unsigned long props, ucs4_t c);

#endif /* _h_ure */
