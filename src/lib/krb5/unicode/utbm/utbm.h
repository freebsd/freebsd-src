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
 * $OpenLDAP: pkg/ldap/libraries/liblunicode/utbm/utbm.h,v 1.10 2008/01/07 23:20:05 kurt Exp $
 * $Id: utbm.h,v 1.1 1999/09/21 15:45:18 mleisher Exp $
 */

#ifndef _h_utbm
#define _h_utbm

#include "k5-int.h"

/*************************************************************************
 *
 * Types.
 *
 *************************************************************************/

/*
 * Fundamental character types.
 */
typedef krb5_ui_4 ucs4_t;
typedef krb5_ui_2 ucs2_t;

/*
 * An opaque type used for the search pattern.
 */
typedef struct _utbm_pattern_t *utbm_pattern_t;

/*************************************************************************
 *
 * Flags.
 *
 *************************************************************************/

#define UTBM_CASEFOLD          0x01
#define UTBM_IGNORE_NONSPACING 0x02
#define UTBM_SPACE_COMPRESS    0x04

/*************************************************************************
 *
 * API.
 *
 *************************************************************************/

utbm_pattern_t utbm_create_pattern (void);

void utbm_free_pattern (utbm_pattern_t pattern);

void
utbm_compile (ucs2_t *pat, unsigned long patlen,
		     unsigned long flags, utbm_pattern_t pattern);

int
utbm_exec (utbm_pattern_t pat, ucs2_t *text,
		  unsigned long textlen, unsigned long *match_start,
		  unsigned long *match_end);

/*************************************************************************
 *
 * Prototypes for the stub functions needed.
 *
 *************************************************************************/

int _utbm_isspace (ucs4_t c, int compress);

int _utbm_iscntrl (ucs4_t c);

int _utbm_nonspacing (ucs4_t c);

ucs4_t _utbm_tolower (ucs4_t c);

ucs4_t _utbm_toupper (ucs4_t c);

ucs4_t _utbm_totitle (ucs4_t c);

#endif /* _h_utbm */
