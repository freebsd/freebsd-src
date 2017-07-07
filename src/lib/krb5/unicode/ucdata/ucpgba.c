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
/* Copyright 2001 Computing Research Labs, New Mexico State University
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
 * $OpenLDAP: pkg/ldap/libraries/liblunicode/ucdata/ucpgba.c,v 1.9 2008/01/07 23:20:05 kurt Exp $
 * $Id: ucpgba.c,v 1.5 2001/01/02 18:46:20 mleisher Exp $
 */

#include "k5-int.h"
#include "k5-utf8.h"
#include "k5-unicode.h"

#include <stdio.h>
#include <stdlib.h>

#include "ucdata.h"
#include "ucpgba.h"

/*
 * These macros are used while reordering of RTL runs of text for the
 * special case of non-spacing characters being in runs of weakly
 * directional text.  They check for weak and non-spacing, and digits and
 * non-spacing.
 */
#define ISWEAKSPECIAL(cc)  ucisprop(cc, UC_EN|UC_ES|UC_MN, UC_ET|UC_AN|UC_CS)
#define ISDIGITSPECIAL(cc) ucisprop(cc, UC_ND|UC_MN, 0)

/*
 * These macros are used while breaking a string into runs of text in
 * different directions.  Descriptions:
 *
 * ISLTR_LTR - Test for members of an LTR run in an LTR context.  This looks
 *             for characters with ltr, non-spacing, weak, and neutral
 *             properties.
 *
 * ISRTL_RTL - Test for members of an RTL run in an RTL context.  This looks
 *             for characters with rtl, non-spacing, weak, and neutral
 *             properties.
 *
 * ISRTL_NEUTRAL  - Test for RTL or neutral characters.
 *
 * ISWEAK_NEUTRAL - Test for weak or neutral characters.
 */
#define ISLTR_LTR(cc) ucisprop(cc, UC_L|UC_MN|UC_EN|UC_ES,\
                               UC_ET|UC_CS|UC_B|UC_S|UC_WS|UC_ON)

#define ISRTL_RTL(cc) ucisprop(cc, UC_R|UC_MN|UC_EN|UC_ES,\
                               UC_ET|UC_AN|UC_CS|UC_B|UC_S|UC_WS|UC_ON)

#define ISRTL_NEUTRAL(cc) ucisprop(cc, UC_R, UC_B|UC_S|UC_WS|UC_ON)
#define ISWEAK_NEUTRAL(cc) ucisprop(cc, UC_EN|UC_ES, \
                                    UC_B|UC_S|UC_WS|UC_ON|UC_ET|UC_AN|UC_CS)

/*
 * This table is temporarily hard-coded here until it can be constructed
 * automatically somehow.
 */
static unsigned long _symmetric_pairs[] = {
    0x0028, 0x0029, 0x0029, 0x0028, 0x003C, 0x003E, 0x003E, 0x003C,
    0x005B, 0x005D, 0x005D, 0x005B, 0x007B, 0x007D, 0x007D, 0x007B,
    0x2045, 0x2046, 0x2046, 0x2045, 0x207D, 0x207E, 0x207E, 0x207D,
    0x208D, 0x208E, 0x208E, 0x208D, 0x3008, 0x3009, 0x3009, 0x3008,
    0x300A, 0x300B, 0x300B, 0x300A, 0x300C, 0x300D, 0x300D, 0x300C,
    0x300E, 0x300F, 0x300F, 0x300E, 0x3010, 0x3011, 0x3011, 0x3010,
    0x3014, 0x3015, 0x3015, 0x3014, 0x3016, 0x3017, 0x3017, 0x3016,
    0x3018, 0x3019, 0x3019, 0x3018, 0x301A, 0x301B, 0x301B, 0x301A,
    0xFD3E, 0xFD3F, 0xFD3F, 0xFD3E, 0xFE59, 0xFE5A, 0xFE5A, 0xFE59,
    0xFE5B, 0xFE5C, 0xFE5C, 0xFE5B, 0xFE5D, 0xFE5E, 0xFE5E, 0xFE5D,
    0xFF08, 0xFF09, 0xFF09, 0xFF08, 0xFF3B, 0xFF3D, 0xFF3D, 0xFF3B,
    0xFF5B, 0xFF5D, 0xFF5D, 0xFF5B, 0xFF62, 0xFF63, 0xFF63, 0xFF62,
};

static int _symmetric_pairs_size =
sizeof(_symmetric_pairs)/sizeof(_symmetric_pairs[0]);

/*
 * This routine looks up the other form of a symmetric pair.
 */
static unsigned long
_ucsymmetric_pair(unsigned long c)
{
    int i;

    for (i = 0; i < _symmetric_pairs_size; i += 2) {
        if (_symmetric_pairs[i] == c)
          return _symmetric_pairs[i+1];
    }
    return c;
}

/*
 * This routine creates a new run, copies the text into it, links it into the
 * logical text order chain and returns it to the caller to be linked into
 * the visual text order chain.
 */
static ucrun_t *
_add_run(ucstring_t *str, unsigned long *src,
         unsigned long start, unsigned long end, int direction)
{
    long i, t;
    ucrun_t *run;

    run = (ucrun_t *) malloc(sizeof(ucrun_t));
    run->visual_next = run->visual_prev = 0;
    run->direction = direction;

    run->cursor = ~0;

    run->chars = (unsigned long *)
        malloc(sizeof(unsigned long) * ((end - start) << 1));
    run->positions = run->chars + (end - start);

    run->source = src;
    run->start = start;
    run->end = end;

    if (direction == UCPGBA_RTL) {
        /*
         * Copy the source text into the run in reverse order and select
         * replacements for the pairwise punctuation and the <> characters.
         */
        for (i = 0, t = end - 1; start < end; start++, t--, i++) {
            run->positions[i] = t;
            if (ucissymmetric(src[t]) || src[t] == '<' || src[t] == '>')
              run->chars[i] = _ucsymmetric_pair(src[t]);
            else
              run->chars[i] = src[t];
        }
    } else {
        /*
         * Copy the source text into the run directly.
         */
        for (i = start; i < end; i++) {
            run->positions[i - start] = i;
            run->chars[i - start] = src[i];
        }
    }

    /*
     * Add the run to the logical list for cursor traversal.
     */
    if (str->logical_first == 0)
      str->logical_first = str->logical_last = run;
    else {
        run->logical_prev = str->logical_last;
        str->logical_last->logical_next = run;
        str->logical_last = run;
    }

    return run;
}

static void
_ucadd_rtl_segment(ucstring_t *str, unsigned long *source, unsigned long start,
                   unsigned long end)
{
    unsigned long s, e;
    ucrun_t *run, *lrun;

    /*
     * This is used to splice runs into strings with overall LTR direction.
     * The `lrun' variable will never be NULL because at least one LTR run was
     * added before this RTL run.
     */
    lrun = str->visual_last;

    for (e = s = start; s < end;) {
        for (; e < end && ISRTL_NEUTRAL(source[e]); e++) ;

        if (e > s) {
            run = _add_run(str, source, s, e, UCPGBA_RTL);

            /*
             * Add the run to the visual list for cursor traversal.
             */
            if (str->visual_first != 0) {
                if (str->direction == UCPGBA_LTR) {
                    run->visual_prev = lrun;
                    run->visual_next = lrun->visual_next;
                    if (lrun->visual_next != 0)
                      lrun->visual_next->visual_prev = run;
                    lrun->visual_next = run;
                    if (lrun == str->visual_last)
                      str->visual_last = run;
                } else {
                    run->visual_next = str->visual_first;
                    str->visual_first->visual_prev = run;
                    str->visual_first = run;
                }
            } else
              str->visual_first = str->visual_last = run;
        }

        /*
         * Handle digits in a special way.  This makes sure the weakly
         * directional characters appear on the expected sides of a number
         * depending on whether that number is Arabic or not.
         */
        for (s = e; e < end && ISWEAKSPECIAL(source[e]); e++) {
            if (!ISDIGITSPECIAL(source[e]) &&
                (e + 1 == end || !ISDIGITSPECIAL(source[e + 1])))
              break;
        }

        if (e > s) {
            run = _add_run(str, source, s, e, UCPGBA_LTR);

            /*
             * Add the run to the visual list for cursor traversal.
             */
            if (str->visual_first != 0) {
                if (str->direction == UCPGBA_LTR) {
                    run->visual_prev = lrun;
                    run->visual_next = lrun->visual_next;
                    if (lrun->visual_next != 0)
                      lrun->visual_next->visual_prev = run;
                    lrun->visual_next = run;
                    if (lrun == str->visual_last)
                      str->visual_last = run;
                } else {
                    run->visual_next = str->visual_first;
                    str->visual_first->visual_prev = run;
                    str->visual_first = run;
                }
            } else
              str->visual_first = str->visual_last = run;
        }

        /*
         * Collect all weak non-digit sequences for an RTL segment.  These
         * will appear as part of the next RTL segment or will be added as
         * an RTL segment by themselves.
         */
        for (s = e; e < end && ucisweak(source[e]) && !ucisdigit(source[e]);
             e++) ;
    }

    /*
     * Capture any weak non-digit sequences that occur at the end of the RTL
     * run.
     */
    if (e > s) {
        run = _add_run(str, source, s, e, UCPGBA_RTL);

        /*
         * Add the run to the visual list for cursor traversal.
         */
        if (str->visual_first != 0) {
            if (str->direction == UCPGBA_LTR) {
                run->visual_prev = lrun;
                run->visual_next = lrun->visual_next;
                if (lrun->visual_next != 0)
                  lrun->visual_next->visual_prev = run;
                lrun->visual_next = run;
                if (lrun == str->visual_last)
                  str->visual_last = run;
            } else {
                run->visual_next = str->visual_first;
                str->visual_first->visual_prev = run;
                str->visual_first = run;
            }
        } else
          str->visual_first = str->visual_last = run;
    }
}

static void
_ucadd_ltr_segment(ucstring_t *str, unsigned long *source, unsigned long start,
                   unsigned long end)
{
    ucrun_t *run;

    run = _add_run(str, source, start, end, UCPGBA_LTR);

    /*
     * Add the run to the visual list for cursor traversal.
     */
    if (str->visual_first != 0) {
        if (str->direction == UCPGBA_LTR) {
            run->visual_prev = str->visual_last;
            str->visual_last->visual_next = run;
            str->visual_last = run;
        } else {
            run->visual_next = str->visual_first;
            str->visual_first->visual_prev = run;
            str->visual_first = run;
        }
    } else
      str->visual_first = str->visual_last = run;
}

ucstring_t *
ucstring_create(unsigned long *source, unsigned long start, unsigned long end,
                int default_direction, int cursor_motion)
{
    int rtl_first;
    unsigned long s, e, ld;
    ucstring_t *str;

    str = (ucstring_t *) malloc(sizeof(ucstring_t));

    /*
     * Set the initial values.
     */
    str->cursor_motion = cursor_motion;
    str->logical_first = str->logical_last = 0;
    str->visual_first = str->visual_last = str->cursor = 0;
    str->source = source;
    str->start = start;
    str->end = end;

    /*
     * If the length of the string is 0, then just return it at this point.
     */
    if (start == end)
      return str;

    /*
     * This flag indicates whether the collection loop for RTL is called
     * before the LTR loop the first time.
     */
    rtl_first = 0;

    /*
     * Look for the first character in the string that has strong
     * directionality.
     */
    for (s = start; s < end && !ucisstrong(source[s]); s++) ;

    if (s == end)
      /*
       * If the string contains no characters with strong directionality, use
       * the default direction.
       */
      str->direction = default_direction;
    else
      str->direction = ucisrtl(source[s]) ? UCPGBA_RTL : UCPGBA_LTR;

    if (str->direction == UCPGBA_RTL)
      /*
       * Set the flag that causes the RTL collection loop to run first.
       */
      rtl_first = 1;

    /*
     * This loop now separates the string into runs based on directionality.
     */
    for (s = e = 0; s < end; s = e) {
        if (!rtl_first) {
            /*
             * Determine the next run of LTR text.
             */

            ld = s;
            while (e < end && ISLTR_LTR(source[e])) {
                if (ucisdigit(source[e]) &&
                    !(0x660 <= source[e] && source[e] <= 0x669))
                  ld = e;
                e++;
            }
            if (str->direction != UCPGBA_LTR) {
                while (e > ld && ISWEAK_NEUTRAL(source[e - 1]))
                  e--;
            }

            /*
             * Add the LTR segment to the string.
             */
            if (e > s)
              _ucadd_ltr_segment(str, source, s, e);
        }

        /*
         * Determine the next run of RTL text.
         */
        ld = s = e;
        while (e < end && ISRTL_RTL(source[e])) {
            if (ucisdigit(source[e]) &&
                !(0x660 <= source[e] && source[e] <= 0x669))
              ld = e;
            e++;
        }
        if (str->direction != UCPGBA_RTL) {
            while (e > ld && ISWEAK_NEUTRAL(source[e - 1]))
              e--;
        }

        /*
         * Add the RTL segment to the string.
         */
        if (e > s)
          _ucadd_rtl_segment(str, source, s, e);

        /*
         * Clear the flag that allowed the RTL collection loop to run first
         * for strings with overall RTL directionality.
         */
        rtl_first = 0;
    }

    /*
     * Set up the initial cursor run.
     */
    str->cursor = str->logical_first;
    if (str != 0)
      str->cursor->cursor = (str->cursor->direction == UCPGBA_RTL) ?
          str->cursor->end - str->cursor->start : 0;

    return str;
}

void
ucstring_free(ucstring_t *s)
{
    ucrun_t *l, *r;

    if (s == 0)
      return;

    for (l = 0, r = s->visual_first; r != 0; r = r->visual_next) {
        if (r->end > r->start)
          free((char *) r->chars);
        if (l)
          free((char *) l);
        l = r;
    }
    if (l)
      free((char *) l);

    free((char *) s);
}

int
ucstring_set_cursor_motion(ucstring_t *str, int cursor_motion)
{
    int n;

    if (str == 0)
      return -1;

    n = str->cursor_motion;
    str->cursor_motion = cursor_motion;
    return n;
}

static int
_ucstring_visual_cursor_right(ucstring_t *str, int count)
{
    int cnt = count;
    unsigned long size;
    ucrun_t *cursor;

    if (str == 0)
      return 0;

    cursor = str->cursor;
    while (cnt > 0) {
        size = cursor->end - cursor->start;
        if ((cursor->direction == UCPGBA_RTL && cursor->cursor + 1 == size) ||
            cursor->cursor + 1 > size) {
            /*
             * If the next run is NULL, then the cursor is already on the
             * far right end already.
             */
            if (cursor->visual_next == 0)
              /*
               * If movement occured, then report it.
               */
              return (cnt != count);

            /*
             * Move to the next run.
             */
            str->cursor = cursor = cursor->visual_next;
            cursor->cursor = (cursor->direction == UCPGBA_RTL) ? -1 : 0;
            size = cursor->end - cursor->start;
        } else
          cursor->cursor++;
        cnt--;
    }
    return 1;
}

static int
_ucstring_logical_cursor_right(ucstring_t *str, int count)
{
    int cnt = count;
    unsigned long size;
    ucrun_t *cursor;

    if (str == 0)
      return 0;

    cursor = str->cursor;
    while (cnt > 0) {
        size = cursor->end - cursor->start;
        if (str->direction == UCPGBA_RTL) {
            if (cursor->direction == UCPGBA_RTL) {
                if (cursor->cursor + 1 == size) {
                    if (cursor == str->logical_first)
                      /*
                       * Already at the beginning of the string.
                       */
                      return (cnt != count);

                    str->cursor = cursor = cursor->logical_prev;
                    size = cursor->end - cursor->start;
                    cursor->cursor = (cursor->direction == UCPGBA_LTR) ?
                        size : 0;
                } else
                  cursor->cursor++;
            } else {
                if (cursor->cursor == 0) {
                    if (cursor == str->logical_first)
                      /*
                       * At the beginning of the string already.
                       */
                      return (cnt != count);

                    str->cursor = cursor = cursor->logical_prev;
                    size = cursor->end - cursor->start;
                    cursor->cursor = (cursor->direction == UCPGBA_LTR) ?
                        size : 0;
                } else
                  cursor->cursor--;
            }
        } else {
            if (cursor->direction == UCPGBA_RTL) {
                if (cursor->cursor == 0) {
                    if (cursor == str->logical_last)
                      /*
                       * Already at the end of the string.
                       */
                      return (cnt != count);

                    str->cursor = cursor = cursor->logical_next;
                    size = cursor->end - cursor->start;
                    cursor->cursor = (cursor->direction == UCPGBA_LTR) ?
                        0 : size - 1;
                } else
                  cursor->cursor--;
            } else {
                if (cursor->cursor + 1 > size) {
                    if (cursor == str->logical_last)
                      /*
                       * Already at the end of the string.
                       */
                      return (cnt != count);

                    str->cursor = cursor = cursor->logical_next;
                    cursor->cursor = (cursor->direction == UCPGBA_LTR) ?
                        0 : size - 1;
                } else
                  cursor->cursor++;
            }
        }
        cnt--;
    }
    return 1;
}

int
ucstring_cursor_right(ucstring_t *str, int count)
{
    if (str == 0)
      return 0;
    return (str->cursor_motion == UCPGBA_CURSOR_VISUAL) ?
        _ucstring_visual_cursor_right(str, count) :
        _ucstring_logical_cursor_right(str, count);
}

static int
_ucstring_visual_cursor_left(ucstring_t *str, int count)
{
    int cnt = count;
    unsigned long size;
    ucrun_t *cursor;

    if (str == 0)
      return 0;

    cursor = str->cursor;
    while (cnt > 0) {
        size = cursor->end - cursor->start;
        if ((cursor->direction == UCPGBA_LTR && cursor->cursor == 0) ||
            cursor->cursor - 1 < -1) {
            /*
             * If the preceding run is NULL, then the cursor is already on the
             * far left end already.
             */
            if (cursor->visual_prev == 0)
              /*
               * If movement occured, then report it.
               */
              return (cnt != count);

            /*
             * Move to the previous run.
             */
            str->cursor = cursor = cursor->visual_prev;
            size = cursor->end - cursor->start;
            cursor->cursor = (cursor->direction == UCPGBA_RTL) ?
                size : size - 1;
        } else
          cursor->cursor--;
        cnt--;
    }
    return 1;
}

static int
_ucstring_logical_cursor_left(ucstring_t *str, int count)
{
    int cnt = count;
    unsigned long size;
    ucrun_t *cursor;

    if (str == 0)
      return 0;

    cursor = str->cursor;
    while (cnt > 0) {
        size = cursor->end - cursor->start;
        if (str->direction == UCPGBA_RTL) {
            if (cursor->direction == UCPGBA_RTL) {
                if (cursor->cursor == -1) {
                    if (cursor == str->logical_last)
                      /*
                       * Already at the end of the string.
                       */
                      return (cnt != count);

                    str->cursor = cursor = cursor->logical_next;
                    size = cursor->end - cursor->start;
                    cursor->cursor = (cursor->direction == UCPGBA_LTR) ?
                        0 : size - 1;
                } else
                  cursor->cursor--;
            } else {
                if (cursor->cursor + 1 > size) {
                    if (cursor == str->logical_last)
                      /*
                       * At the end of the string already.
                       */
                      return (cnt != count);

                    str->cursor = cursor = cursor->logical_next;
                    size = cursor->end - cursor->start;
                    cursor->cursor = (cursor->direction == UCPGBA_LTR) ?
                        0 : size - 1;
                } else
                  cursor->cursor++;
            }
        } else {
            if (cursor->direction == UCPGBA_RTL) {
                if (cursor->cursor + 1 == size) {
                    if (cursor == str->logical_first)
                      /*
                       * Already at the beginning of the string.
                       */
                      return (cnt != count);

                    str->cursor = cursor = cursor->logical_prev;
                    size = cursor->end - cursor->start;
                    cursor->cursor = (cursor->direction == UCPGBA_LTR) ?
                        size : 0;
                } else
                  cursor->cursor++;
            } else {
                if (cursor->cursor == 0) {
                    if (cursor == str->logical_first)
                      /*
                       * Already at the beginning of the string.
                       */
                      return (cnt != count);

                    str->cursor = cursor = cursor->logical_prev;
                    cursor->cursor = (cursor->direction == UCPGBA_LTR) ?
                        size : 0;
                } else
                  cursor->cursor--;
            }
        }
        cnt--;
    }
    return 1;
}

int
ucstring_cursor_left(ucstring_t *str, int count)
{
    if (str == 0)
      return 0;
    return (str->cursor_motion == UCPGBA_CURSOR_VISUAL) ?
        _ucstring_visual_cursor_left(str, count) :
        _ucstring_logical_cursor_left(str, count);
}

void
ucstring_cursor_info(ucstring_t *str, int *direction, unsigned long *position)
{
    long c;
    unsigned long size;
    ucrun_t *cursor;

    if (str == 0 || direction == 0 || position == 0)
      return;

    cursor = str->cursor;

    *direction = cursor->direction;

    c = cursor->cursor;
    size = cursor->end - cursor->start;

    if (c == size)
      *position = (cursor->direction == UCPGBA_RTL) ?
          cursor->start : cursor->positions[c - 1];
    else if (c == -1)
      *position = (cursor->direction == UCPGBA_RTL) ?
          cursor->end : cursor->start;
    else
      *position = cursor->positions[c];
}
