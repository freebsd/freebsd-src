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
/* Copyright 1999 Computing Research Labs, New Mexico State University
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
 * $OpenLDAP: pkg/ldap/libraries/liblunicode/ucdata/ucpgba.h,v 1.10 2008/01/07 23:20:05 kurt Exp $
 * $Id: ucpgba.h,v 1.4 1999/11/19 15:24:30 mleisher Exp $
 */

#ifndef _h_ucpgba
#define _h_ucpgba

#include "k5-int.h"

/***************************************************************************
 *
 * Macros and types.
 *
 ***************************************************************************/

/*
 * These are the direction values that can appear in render runs and render
 * strings.
 */
#define UCPGBA_LTR 0
#define UCPGBA_RTL 1

/*
 * These are the flags for cursor motion.
 */
#define UCPGBA_CURSOR_VISUAL  0
#define UCPGBA_CURSOR_LOGICAL 1

/*
 * This structure is used to contain runs of text in a particular direction.
 */
typedef struct _ucrun_t {
    struct _ucrun_t *visual_prev;  /* Pointer to the previous visual run.    */
    struct _ucrun_t *visual_next;  /* Pointer to the next visual run.        */

    struct _ucrun_t *logical_prev; /* Pointer to the previous logical run.   */
    struct _ucrun_t *logical_next; /* Pointer to the next logical run.       */

    int direction;                 /* Direction of the run.                  */

    long cursor;                   /* Position of "cursor" in the string.    */

    unsigned long *chars;          /* List of characters for the run.        */
    unsigned long *positions;      /* List of original positions in source.  */

    unsigned long *source;         /* The source string.                     */
    unsigned long start;           /* Beginning offset in the source string. */
    unsigned long end;             /* Ending offset in the source string.    */
} ucrun_t;

/*
 * This represents a string of runs rendered up to a point that is not
 * platform specific.
 */
typedef struct _ucstring_t {
    int direction;                /* Overall direction of the string.       */

    int cursor_motion;            /* Logical or visual cursor motion flag.  */

    ucrun_t *cursor;              /* The run containing the "cursor."       */

    ucrun_t *logical_first;       /* First run in the logical order.        */
    ucrun_t *logical_last;        /* Last run in the logical order.         */

    ucrun_t *visual_first;        /* First run in the visual order.         */
    ucrun_t *visual_last;         /* Last run in the visual order.          */

    unsigned long *source;        /* The source string.                     */
    unsigned long start;          /* The beginning offset in the source.    */
    unsigned long end;            /* The ending offset in the source.       */
} ucstring_t;

/***************************************************************************
 *
 * API
 *
 ***************************************************************************/

/*
 * This creates and reorders the specified substring using the
 * "Pretty Good Bidi Algorithm."  A default direction is provided for cases
 * of a string containing no strong direction characters and the default
 * cursor motion should be provided.
 */
ucstring_t *
ucstring_create (unsigned long *source,
		        unsigned long start,
		        unsigned long end,
		        int default_direction,
		        int cursor_motion);
/*
 * This releases the string.
 */
void ucstring_free (ucstring_t *string);

/*
 * This changes the cursor motion flag for the string.
 */
int
ucstring_set_cursor_motion (ucstring_t *string,
				   int cursor_motion);

/*
 * This function will move the cursor to the right depending on the
 * type of cursor motion that was specified for the string.
 *
 * A 0 is returned if no cursor motion is performed, otherwise a
 * 1 is returned.
 */
int
ucstring_cursor_right (ucstring_t *string, int count);

/*
 * This function will move the cursor to the left depending on the
 * type of cursor motion that was specified for the string.
 *
 * A 0 is returned if no cursor motion is performed, otherwise a
 * 1 is returned.
 */
int
ucstring_cursor_left (ucstring_t *string, int count);

/*
 * This routine retrieves the direction of the run containing the cursor
 * and the actual position in the original text string.
 */
void
ucstring_cursor_info (ucstring_t *string, int *direction,
			     unsigned long *position);

#endif /* _h_ucpgba */
