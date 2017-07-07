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
 * $OpenLDAP: pkg/ldap/libraries/liblunicode/utbm/utbm.c,v 1.9 2008/01/07 23:20:05 kurt Exp $
 * $Id: utbm.c,v 1.1 1999/09/21 15:45:17 mleisher Exp $
 */

/*
 * Assumptions:
 * 1. Case conversions of UTF-16 characters must also be UTF-16 characters.
 * 2. Case conversions are all one-to-one.
 * 3. Text and pattern have already been normalized in some fashion.
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "utbm.h"

/*
 * Single pattern character.
 */
typedef struct {
    ucs4_t lc;
    ucs4_t uc;
    ucs4_t tc;
} _utbm_char_t;

typedef struct {
    _utbm_char_t *ch;
    unsigned long skip;
} _utbm_skip_t;

typedef struct _utbm_pattern_t {
    unsigned long flags;

    _utbm_char_t *pat;
    unsigned long pat_used;
    unsigned long pat_size;
    unsigned long patlen;

    _utbm_skip_t *skip;
    unsigned long skip_used;
    unsigned long skip_size;

    unsigned long md4;
} _utbm_pattern_t;

/*************************************************************************
 *
 * Support functions.
 *
 *************************************************************************/

/*
 * Routine to look up the skip value for a character.
 */
static unsigned long
_utbm_skip(utbm_pattern_t p, ucs2_t *start, ucs2_t *end)
{
    unsigned long i;
    ucs4_t c1, c2;
    _utbm_skip_t *sp;

    if (start >= end)
      return 0;

    c1 = *start;
    c2 = (start + 1 < end) ? *(start + 1) : ~0;
    if (0xd800 <= c1 && c1 <= 0xdbff && 0xdc00 <= c2 && c2 <= 0xdfff)
      c1 = 0x10000 + (((c1 & 0x03ff) << 10) | (c2 & 0x03ff));

    for (i = 0, sp = p->skip; i < p->skip_used; i++, sp++) {
        if (!((c1 ^ sp->ch->uc) & (c1 ^ sp->ch->lc) & (c1 ^ sp->ch->tc))) {
            return ((unsigned long) (end - start) < sp->skip) ?
                end - start : sp->skip;
        }
    }
    return p->patlen;
}

static int
_utbm_match(utbm_pattern_t pat, ucs2_t *text, ucs2_t *start, ucs2_t *end,
            unsigned long *match_start, unsigned long *match_end)
{
    int check_space;
    ucs4_t c1, c2;
    unsigned long count;
    _utbm_char_t *cp;

    /*
     * Set the potential match endpoint first.
     */
    *match_end = (start - text) + 1;

    c1 = *start;
    c2 = (start + 1 < end) ? *(start + 1) : ~0;
    if (0xd800 <= c1 && c1 <= 0xdbff && 0xdc00 <= c2 && c2 <= 0xdfff) {
        c1 = 0x10000 + (((c1 & 0x03ff) << 10) | (c2 & 0x03ff));
        /*
         * Adjust the match end point to occur after the UTF-16 character.
         */
        *match_end = *match_end + 1;
    }

    if (pat->pat_used == 1) {
        *match_start = start - text;
        return 1;
    }

    /*
     * Compare backward.
     */
    cp = pat->pat + (pat->pat_used - 1);

    for (count = pat->patlen; start > text && count > 0;) {
        /*
         * Ignore non-spacing characters if indicated.
         */
        if (pat->flags & UTBM_IGNORE_NONSPACING) {
            while (start > text && _utbm_nonspacing(c1)) {
                c2 = *--start;
                c1 = (start - 1 > text) ? *(start - 1) : ~0;
                if (0xdc00 <= c2 && c2 <= 0xdfff &&
                    0xd800 <= c1 && c1 <= 0xdbff) {
                    c1 = 0x10000 + (((c1 & 0x03ff) << 10) | (c2 & 0x03ff));
                    start--;
                } else
                  c1 = c2;
            }
        }

        /*
         * Handle space compression if indicated.
         */
        if (pat->flags & UTBM_SPACE_COMPRESS) {
            check_space = 0;
            while (start > text &&
                   (_utbm_isspace(c1, 1) || _utbm_iscntrl(c1))) {
                check_space = _utbm_isspace(c1, 1);
                c2 = *--start;
                c1 = (start - 1 > text) ? *(start - 1) : ~0;
                if (0xdc00 <= c2 && c2 <= 0xdfff &&
                    0xd800 <= c1 && c1 <= 0xdbff) {
                    c1 = 0x10000 + (((c1 & 0x03ff) << 10) | (c2 & 0x03ff));
                    start--;
                } else
                  c1 = c2;
            }
            /*
             * Handle things if space compression was indicated and one or
             * more member characters were found.
             */
            if (check_space) {
                if (cp->uc != ' ')
                  return 0;
                cp--;
                count--;
            }
        }

        /*
         * Handle the normal comparison cases.
         */
        if (count > 0 && ((c1 ^ cp->uc) & (c1 ^ cp->lc) & (c1 ^ cp->tc)))
          return 0;

        count -= (c1 >= 0x10000) ? 2 : 1;
        if (count > 0) {
            cp--;

            /*
             * Get the next preceding character.
             */
            if (start > text) {
                c2 = *--start;
                c1 = (start - 1 > text) ? *(start - 1) : ~0;
                if (0xdc00 <= c2 && c2 <= 0xdfff &&
                    0xd800 <= c1 && c1 <= 0xdbff) {
                    c1 = 0x10000 + (((c1 & 0x03ff) << 10) | (c2 & 0x03ff));
                    start--;
                } else
                  c1 = c2;
            }
        }
    }

    /*
     * Set the match start position.
     */
    *match_start = start - text;
    return 1;
}

/*************************************************************************
 *
 * API.
 *
 *************************************************************************/

utbm_pattern_t
utbm_create_pattern(void)
{
    utbm_pattern_t p;

    p = (utbm_pattern_t) malloc(sizeof(_utbm_pattern_t));
    (void) memset((char *) p, '\0', sizeof(_utbm_pattern_t));
    return p;
}

void
utbm_free_pattern(utbm_pattern_t pattern)
{
    if (pattern == 0)
      return;

    if (pattern->pat_size > 0)
      free((char *) pattern->pat);

    if (pattern->skip_size > 0)
      free((char *) pattern->skip);

    free((char *) pattern);
}

void
utbm_compile(ucs2_t *pat, unsigned long patlen, unsigned long flags,
             utbm_pattern_t p)
{
    int have_space;
    unsigned long i, j, k, slen;
    _utbm_char_t *cp;
    _utbm_skip_t *sp;
    ucs4_t c1, c2, sentinel;

    if (p == 0 || pat == 0 || *pat == 0 || patlen == 0)
      return;

    /*
     * Reset the pattern buffer.
     */
    p->patlen = p->pat_used = p->skip_used = 0;

    /*
     * Set the flags.
     */
    p->flags = flags;

    /*
     * Initialize the extra skip flag.
     */
    p->md4 = 1;

    /*
     * Allocate more storage if necessary.
     */
    if (patlen > p->pat_size) {
        if (p->pat_size == 0) {
            p->pat = (_utbm_char_t *) malloc(sizeof(_utbm_char_t) * patlen);
            p->skip = (_utbm_skip_t *) malloc(sizeof(_utbm_skip_t) * patlen);
        } else {
            p->pat = (_utbm_char_t *)
                realloc((char *) p->pat, sizeof(_utbm_char_t) * patlen);
            p->skip = (_utbm_skip_t *)
                realloc((char *) p->skip, sizeof(_utbm_skip_t) * patlen);
        }
        p->pat_size = p->skip_size = patlen;
    }

    /*
     * Preprocess the pattern to remove controls (if specified) and determine
     * case.
     */
    for (have_space = 0, cp = p->pat, i = 0; i < patlen; i++) {
        c1 = pat[i];
        c2 = (i + 1 < patlen) ? pat[i + 1] : ~0;
        if (0xd800 <= c1 && c1 <= 0xdbff && 0xdc00 <= c2 && c2 <= 0xdfff)
          c1 = 0x10000 + (((c1 & 0x03ff) << 10) | (c2 & 0x03ff));

        /*
         * Make sure the `have_space' flag is turned off if the character
         * is not an appropriate one.
         */
        if (!_utbm_isspace(c1, flags & UTBM_SPACE_COMPRESS))
          have_space = 0;

        /*
         * If non-spacing characters should be ignored, do it here.
         */
        if ((flags & UTBM_IGNORE_NONSPACING) && _utbm_nonspacing(c1))
          continue;

        /*
         * Check if spaces and controls need to be compressed.
         */
        if (flags & UTBM_SPACE_COMPRESS) {
            if (_utbm_isspace(c1, 1)) {
                if (!have_space) {
                    /*
                     * Add a space and set the flag.
                     */
                    cp->uc = cp->lc = cp->tc = ' ';
                    cp++;

                    /*
                     * Increase the real pattern length.
                     */
                    p->patlen++;
                    sentinel = ' ';
                    have_space = 1;
                }
                continue;
            }

            /*
             * Ignore all control characters.
             */
            if (_utbm_iscntrl(c1))
              continue;
        }

        /*
         * Add the character.
         */
        if (flags & UTBM_CASEFOLD) {
            cp->uc = _utbm_toupper(c1);
            cp->lc = _utbm_tolower(c1);
            cp->tc = _utbm_totitle(c1);
        } else
          cp->uc = cp->lc = cp->tc = c1;

        /*
         * Set the sentinel character.
         */
        sentinel = cp->uc;

        /*
         * Move to the next character.
         */
        cp++;

        /*
         * Increase the real pattern length appropriately.
         */
        p->patlen += (c1 >= 0x10000) ? 2 : 1;

        /*
         * Increment the loop index for UTF-16 characters.
         */
        i += (c1 >= 0x10000) ? 1 : 0;

    }

    /*
     * Set the number of characters actually used.
     */
    p->pat_used = cp - p->pat;

    /*
     * Go through and construct the skip array and determine the actual length
     * of the pattern in UCS2 terms.
     */
    slen = p->patlen - 1;
    cp = p->pat;
    for (i = k = 0; i < p->pat_used; i++, cp++) {
        /*
         * Locate the character in the skip array.
         */
        for (sp = p->skip, j = 0;
             j < p->skip_used && sp->ch->uc != cp->uc; j++, sp++) ;

        /*
         * If the character is not found, set the new skip element and
         * increase the number of skip elements.
         */
        if (j == p->skip_used) {
            sp->ch = cp;
            p->skip_used++;
        }

        /*
         * Set the updated skip value.  If the character is UTF-16 and is
         * not the last one in the pattern, add one to its skip value.
         */
        sp->skip = slen - k;
        if (cp->uc >= 0x10000 && k + 2 < slen)
          sp->skip++;

        /*
         * Set the new extra skip for the sentinel character.
         */
        if (((cp->uc >= 0x10000 && k + 2 <= slen) || k + 1 <= slen) &&
            cp->uc == sentinel)
          p->md4 = slen - k;

        /*
         * Increase the actual index.
         */
        k += (cp->uc >= 0x10000) ? 2 : 1;
    }
}

int
utbm_exec(utbm_pattern_t pat, ucs2_t *text, unsigned long textlen,
          unsigned long *match_start, unsigned long *match_end)
{
    unsigned long k;
    ucs2_t *start, *end;

    if (pat == 0 || pat->pat_used == 0 || text == 0 || textlen == 0 ||
        textlen < pat->patlen)
      return 0;

    start = text + pat->patlen;
    end = text + textlen;

    /*
     * Adjust the start point if it points to a low surrogate.
     */
    if (0xdc00 <= *start && *start <= 0xdfff &&
        0xd800 <= *(start - 1) && *(start - 1) <= 0xdbff)
      start--;

    while (start < end) {
        while ((k = _utbm_skip(pat, start, end))) {
            start += k;
            if (start < end && 0xdc00 <= *start && *start <= 0xdfff &&
                0xd800 <= *(start - 1) && *(start - 1) <= 0xdbff)
              start--;
        }

        if (start < end &&
            _utbm_match(pat, text, start, end, match_start, match_end))
          return 1;

        start += pat->md4;
        if (start < end && 0xdc00 <= *start && *start <= 0xdfff &&
            0xd800 <= *(start - 1) && *(start - 1) <= 0xdbff)
          start--;
    }
    return 0;
}
