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
 * $OpenLDAP: pkg/ldap/libraries/liblunicode/ure/ure.c,v 1.19 2008/01/07 23:20:05 kurt Exp $
 * $Id: ure.c,v 1.2 1999/09/21 15:47:43 mleisher Exp $"
 */

#include <k5-int.h>

#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#include "ure.h"

/*
 * Flags used internally in the DFA.
 */
#define _URE_DFA_CASEFOLD  0x01
#define _URE_DFA_BLANKLINE 0x02

static unsigned long cclass_flags[] = {
    0,
    _URE_NONSPACING,
    _URE_COMBINING,
    _URE_NUMDIGIT,
    _URE_NUMOTHER,
    _URE_SPACESEP,
    _URE_LINESEP,
    _URE_PARASEP,
    _URE_CNTRL,
    _URE_PUA,
    _URE_UPPER,
    _URE_LOWER,
    _URE_TITLE,
    _URE_MODIFIER,
    _URE_OTHERLETTER,
    _URE_DASHPUNCT,
    _URE_OPENPUNCT,
    _URE_CLOSEPUNCT,
    _URE_OTHERPUNCT,
    _URE_MATHSYM,
    _URE_CURRENCYSYM,
    _URE_OTHERSYM,
    _URE_LTR,
    _URE_RTL,
    _URE_EURONUM,
    _URE_EURONUMSEP,
    _URE_EURONUMTERM,
    _URE_ARABNUM,
    _URE_COMMONSEP,
    _URE_BLOCKSEP,
    _URE_SEGMENTSEP,
    _URE_WHITESPACE,
    _URE_OTHERNEUT,
};

/*
 * Symbol types for the DFA.
 */
#define _URE_ANY_CHAR   1
#define _URE_CHAR       2
#define _URE_CCLASS     3
#define _URE_NCCLASS    4
#define _URE_BOL_ANCHOR 5
#define _URE_EOL_ANCHOR 6

/*
 * Op codes for converting the NFA to a DFA.
 */
#define _URE_SYMBOL     10
#define _URE_PAREN      11
#define _URE_QUEST      12
#define _URE_STAR       13
#define _URE_PLUS       14
#define _URE_ONE        15
#define _URE_AND        16
#define _URE_OR         17

#define _URE_NOOP       0xffff

#define _URE_REGSTART 0x8000
#define _URE_REGEND   0x4000

/*
 * Structure used to handle a compacted range of characters.
 */
typedef struct {
    ucs4_t min_code;
    ucs4_t max_code;
} _ure_range_t;

typedef struct {
    _ure_range_t *ranges;
    ucs2_t ranges_used;
    ucs2_t ranges_size;
} _ure_ccl_t;

typedef union {
    ucs4_t chr;
    _ure_ccl_t ccl;
} _ure_sym_t;

/*
 * This is a general element structure used for expressions and stack
 * elements.
 */
typedef struct {
    ucs2_t reg;
    ucs2_t onstack;
    ucs2_t type;
    ucs2_t lhs;
    ucs2_t rhs;
} _ure_elt_t;

/*
 * This is a structure used to track a list or a stack of states.
 */
typedef struct {
    ucs2_t *slist;
    ucs2_t slist_size;
    ucs2_t slist_used;
} _ure_stlist_t;

/*
 * Structure to track the list of unique states for a symbol
 * during reduction.
 */
typedef struct {
    ucs2_t id;
    ucs2_t type;
    unsigned long mods;
    unsigned long props;
    _ure_sym_t sym;
    _ure_stlist_t states;
} _ure_symtab_t;

/*
 * Structure to hold a single state.
 */
typedef struct {
    ucs2_t id;
    ucs2_t accepting;
    ucs2_t pad;
    _ure_stlist_t st;
    _ure_elt_t *trans;
    ucs2_t trans_size;
    ucs2_t trans_used;
} _ure_state_t;

/*
 * Structure used for keeping lists of states.
 */
typedef struct {
    _ure_state_t *states;
    ucs2_t states_size;
    ucs2_t states_used;
} _ure_statetable_t;

/*
 * Structure to track pairs of DFA states when equivalent states are
 * merged.
 */
typedef struct {
    ucs2_t l;
    ucs2_t r;
} _ure_equiv_t;

/*
 * Structure used for constructing the NFA and reducing to a minimal DFA.
 */
typedef struct _ure_buffer_t {
    int reducing;
    int error;
    unsigned long flags;

    _ure_stlist_t stack;

    /*
     * Table of unique symbols encountered.
     */
    _ure_symtab_t *symtab;
    ucs2_t symtab_size;
    ucs2_t symtab_used;

    /*
     * Tracks the unique expressions generated for the NFA and when the NFA is
     * reduced.
     */
    _ure_elt_t *expr;
    ucs2_t expr_used;
    ucs2_t expr_size;

    /*
     * The reduced table of unique groups of NFA states.
     */
    _ure_statetable_t states;

    /*
     * Tracks states when equivalent states are merged.
     */
    _ure_equiv_t *equiv;
    ucs2_t equiv_used;
    ucs2_t equiv_size;
} _ure_buffer_t;

typedef struct {
    ucs2_t symbol;
    ucs2_t next_state;
} _ure_trans_t;

typedef struct {
    ucs2_t accepting;
    ucs2_t ntrans;
    _ure_trans_t *trans;
} _ure_dstate_t;

typedef struct _ure_dfa_t {
    unsigned long flags;

    _ure_symtab_t *syms;
    ucs2_t nsyms;

    _ure_dstate_t *states;
    ucs2_t nstates;

    _ure_trans_t *trans;
    ucs2_t ntrans;
} _ure_dfa_t;

/*************************************************************************
 *
 * Functions.
 *
 *************************************************************************/

static void
_ure_memmove(char *dest, char *src, unsigned long bytes)
{
    long i, j;

    i = (long) bytes;
    j = i & 7;
    i = (i + 7) >> 3;

    /*
     * Do a memmove using Ye Olde Duff's Device for efficiency.
     */
    if (src < dest) {
        src += bytes;
        dest += bytes;

        switch (j) {
          case 0: do {
              *--dest = *--src;
            case 7: *--dest = *--src;
            case 6: *--dest = *--src;
            case 5: *--dest = *--src;
            case 4: *--dest = *--src;
            case 3: *--dest = *--src;
            case 2: *--dest = *--src;
            case 1: *--dest = *--src;
          } while (--i > 0);
        }
    } else if (src > dest) {
        switch (j) {
          case 0: do {
              *dest++ = *src++;
            case 7: *dest++ = *src++;
            case 6: *dest++ = *src++;
            case 5: *dest++ = *src++;
            case 4: *dest++ = *src++;
            case 3: *dest++ = *src++;
            case 2: *dest++ = *src++;
            case 1: *dest++ = *src++;
          } while (--i > 0);
        }
    }
}

static void
_ure_push(ucs2_t v, _ure_buffer_t *b)
{
    _ure_stlist_t *s;

    if (b == 0)
      return;

    /*
     * If the `reducing' parameter is non-zero, check to see if the value
     * passed is already on the stack.
     */
    if (b->reducing != 0 && b->expr[v].onstack != 0)
      return;

    s = &b->stack;
    if (s->slist_used == s->slist_size) {
        if (s->slist_size == 0)
          s->slist = (ucs2_t *) malloc(sizeof(ucs2_t) << 3);
        else
          s->slist = (ucs2_t *) realloc((char *) s->slist,
                                        sizeof(ucs2_t) * (s->slist_size + 8));
        s->slist_size += 8;
    }
    s->slist[s->slist_used++] = v;

    /*
     * If the `reducing' parameter is non-zero, flag the element as being on
     * the stack.
     */
    if (b->reducing != 0)
      b->expr[v].onstack = 1;
}

static ucs2_t
_ure_peek(_ure_buffer_t *b)
{
    if (b == 0 || b->stack.slist_used == 0)
      return _URE_NOOP;

    return b->stack.slist[b->stack.slist_used - 1];
}

static ucs2_t
_ure_pop(_ure_buffer_t *b)
{
    ucs2_t v;

    if (b == 0 || b->stack.slist_used == 0)
      return _URE_NOOP;

    v = b->stack.slist[--b->stack.slist_used];
    if (b->reducing)
      b->expr[v].onstack = 0;

    return v;
}

/*************************************************************************
 *
 * Start symbol parse functions.
 *
 *************************************************************************/

/*
 * Parse a comma-separated list of integers that represent character
 * properties.  Combine them into a mask that is returned in the `mask'
 * variable, and return the number of characters consumed.
 */
static unsigned long
_ure_prop_list(ucs2_t *pp, unsigned long limit, unsigned long *mask,
               _ure_buffer_t *b)
{
    unsigned long n, m;
    ucs2_t *sp, *ep;

    sp = pp;
    ep = sp + limit;

    for (m = n = 0; b->error == _URE_OK && sp < ep; sp++) {
        if (*sp == ',') {
            /*
             * Encountered a comma, so select the next character property flag
             * and reset the number.
             */
            m |= cclass_flags[n];
            n = 0;
        } else if (*sp >= '0' && *sp <= '9')
          /*
           * Encountered a digit, so start or continue building the cardinal
           * that represents the character property flag.
           */
          n = (n * 10) + (*sp - '0');
        else
          /*
           * Encountered something that is not part of the property list.
           * Indicate that we are done.
           */
          break;

        /*
         * If a property number greater than 32 occurs, then there is a
         * problem.  Most likely a missing comma separator.
         */
        if (n > 32)
          b->error = _URE_INVALID_PROPERTY;
    }

    if (b->error == _URE_OK && n != 0)
      m |= cclass_flags[n];

    /*
     * Set the mask that represents the group of character properties.
     */
    *mask = m;

    /*
     * Return the number of characters consumed.
     */
    return sp - pp;
}

/*
 * Collect a hex number with 1 to 4 digits and return the number
 * of characters used.
 */
static unsigned long
_ure_hex(ucs2_t *np, unsigned long limit, ucs4_t *n)
{
    ucs2_t i;
    ucs2_t *sp, *ep;
    ucs4_t nn;

    sp = np;
    ep = sp + limit;

    for (nn = 0, i = 0; i < 4 && sp < ep; i++, sp++) {
        if (*sp >= '0' && *sp <= '9')
          nn = (nn << 4) + (*sp - '0');
        else if (*sp >= 'A' && *sp <= 'F')
          nn = (nn << 4) + ((*sp - 'A') + 10);
        else if (*sp >= 'a' && *sp <= 'f')
          nn = (nn << 4) + ((*sp - 'a') + 10);
        else
          /*
           * Encountered something that is not a hex digit.
           */
          break;
    }

    /*
     * Assign the character code collected and return the number of
     * characters used.
     */
    *n = nn;

    return sp - np;
}

/*
 * Insert a range into a character class, removing duplicates and ordering
 * them in increasing range-start order.
 */
static void
_ure_add_range(_ure_ccl_t *ccl, _ure_range_t *r, _ure_buffer_t *b)
{
    ucs2_t i;
    ucs4_t tmp;
    _ure_range_t *rp;

    /*
     * If the `casefold' flag is set, then make sure both endpoints of the
     * range are converted to lower case.
     */
    if (b->flags & _URE_DFA_CASEFOLD) {
        r->min_code = _ure_tolower(r->min_code);
        r->max_code = _ure_tolower(r->max_code);
    }

    /*
     * Swap the range endpoints if they are not in increasing order.
     */
    if (r->min_code > r->max_code) {
        tmp = r->min_code;
        r->min_code = r->max_code;
        r->max_code = tmp;
    }

    for (i = 0, rp = ccl->ranges;
         i < ccl->ranges_used && r->min_code < rp->min_code; i++, rp++) ;

    /*
     * Check for a duplicate.
     */
    if (i < ccl->ranges_used &&
        r->min_code == rp->min_code && r->max_code == rp->max_code)
      return;

    if (ccl->ranges_used == ccl->ranges_size) {
        if (ccl->ranges_size == 0)
          ccl->ranges = (_ure_range_t *) malloc(sizeof(_ure_range_t) << 3);
        else
          ccl->ranges = (_ure_range_t *)
              realloc((char *) ccl->ranges,
                      sizeof(_ure_range_t) * (ccl->ranges_size + 8));
        ccl->ranges_size += 8;
    }

    rp = ccl->ranges + ccl->ranges_used;

    if (i < ccl->ranges_used)
      _ure_memmove((char *) (rp + 1), (char *) rp,
                   sizeof(_ure_range_t) * (ccl->ranges_used - i));

    ccl->ranges_used++;
    rp->min_code = r->min_code;
    rp->max_code = r->max_code;
}

#define _URE_ALPHA_MASK  (_URE_UPPER|_URE_LOWER|_URE_OTHERLETTER|\
_URE_MODIFIER|_URE_TITLE|_URE_NONSPACING|_URE_COMBINING)
#define _URE_ALNUM_MASK  (_URE_ALPHA_MASK|_URE_NUMDIGIT)
#define _URE_PUNCT_MASK  (_URE_DASHPUNCT|_URE_OPENPUNCT|_URE_CLOSEPUNCT|\
_URE_OTHERPUNCT)
#define _URE_GRAPH_MASK (_URE_NUMDIGIT|_URE_NUMOTHER|_URE_ALPHA_MASK|\
_URE_MATHSYM|_URE_CURRENCYSYM|_URE_OTHERSYM)
#define _URE_PRINT_MASK (_URE_GRAPH_MASK|_URE_SPACESEP)
#define _URE_SPACE_MASK  (_URE_SPACESEP|_URE_LINESEP|_URE_PARASEP)

typedef void (*_ure_cclsetup_t)(
    _ure_symtab_t *sym,
    unsigned long mask,
    _ure_buffer_t *b
);

typedef struct {
    ucs2_t key;
    unsigned int len : 8;
    unsigned int next : 8;
    _ure_cclsetup_t func;
    unsigned long mask;
} _ure_trie_t;

static void
_ure_ccl_setup(_ure_symtab_t *sym, unsigned long mask, _ure_buffer_t *b)
{
    sym->props |= mask;
}

static void
_ure_space_setup(_ure_symtab_t *sym, unsigned long mask, _ure_buffer_t *b)
{
    _ure_range_t range;

    sym->props |= mask;

    /*
     * Add the additional characters needed for handling isspace().
     */
    range.min_code = range.max_code = '\t';
    _ure_add_range(&sym->sym.ccl, &range, b);
    range.min_code = range.max_code = '\r';
    _ure_add_range(&sym->sym.ccl, &range, b);
    range.min_code = range.max_code = '\n';
    _ure_add_range(&sym->sym.ccl, &range, b);
    range.min_code = range.max_code = '\f';
    _ure_add_range(&sym->sym.ccl, &range, b);
    range.min_code = range.max_code = 0xfeff;
    _ure_add_range(&sym->sym.ccl, &range, b);
}

static void
_ure_xdigit_setup(_ure_symtab_t *sym, unsigned long mask, _ure_buffer_t *b)
{
    _ure_range_t range;

    /*
     * Add the additional characters needed for handling isxdigit().
     */
    range.min_code = '0';
    range.max_code = '9';
    _ure_add_range(&sym->sym.ccl, &range, b);
    range.min_code = 'A';
    range.max_code = 'F';
    _ure_add_range(&sym->sym.ccl, &range, b);
    range.min_code = 'a';
    range.max_code = 'f';
    _ure_add_range(&sym->sym.ccl, &range, b);
}

static const _ure_trie_t cclass_trie[] = {
    {0x003a, 1, 1, 0, 0},
    {0x0061, 9, 10, 0, 0},
    {0x0063, 8, 19, 0, 0},
    {0x0064, 7, 24, 0, 0},
    {0x0067, 6, 29, 0, 0},
    {0x006c, 5, 34, 0, 0},
    {0x0070, 4, 39, 0, 0},
    {0x0073, 3, 49, 0, 0},
    {0x0075, 2, 54, 0, 0},
    {0x0078, 1, 59, 0, 0},
    {0x006c, 1, 11, 0, 0},
    {0x006e, 2, 13, 0, 0},
    {0x0070, 1, 16, 0, 0},
    {0x0075, 1, 14, 0, 0},
    {0x006d, 1, 15, 0, 0},
    {0x003a, 1, 16, _ure_ccl_setup, _URE_ALNUM_MASK},
    {0x0068, 1, 17, 0, 0},
    {0x0061, 1, 18, 0, 0},
    {0x003a, 1, 19, _ure_ccl_setup, _URE_ALPHA_MASK},
    {0x006e, 1, 20, 0, 0},
    {0x0074, 1, 21, 0, 0},
    {0x0072, 1, 22, 0, 0},
    {0x006c, 1, 23, 0, 0},
    {0x003a, 1, 24, _ure_ccl_setup, _URE_CNTRL},
    {0x0069, 1, 25, 0, 0},
    {0x0067, 1, 26, 0, 0},
    {0x0069, 1, 27, 0, 0},
    {0x0074, 1, 28, 0, 0},
    {0x003a, 1, 29, _ure_ccl_setup, _URE_NUMDIGIT},
    {0x0072, 1, 30, 0, 0},
    {0x0061, 1, 31, 0, 0},
    {0x0070, 1, 32, 0, 0},
    {0x0068, 1, 33, 0, 0},
    {0x003a, 1, 34, _ure_ccl_setup, _URE_GRAPH_MASK},
    {0x006f, 1, 35, 0, 0},
    {0x0077, 1, 36, 0, 0},
    {0x0065, 1, 37, 0, 0},
    {0x0072, 1, 38, 0, 0},
    {0x003a, 1, 39, _ure_ccl_setup, _URE_LOWER},
    {0x0072, 2, 41, 0, 0},
    {0x0075, 1, 45, 0, 0},
    {0x0069, 1, 42, 0, 0},
    {0x006e, 1, 43, 0, 0},
    {0x0074, 1, 44, 0, 0},
    {0x003a, 1, 45, _ure_ccl_setup, _URE_PRINT_MASK},
    {0x006e, 1, 46, 0, 0},
    {0x0063, 1, 47, 0, 0},
    {0x0074, 1, 48, 0, 0},
    {0x003a, 1, 49, _ure_ccl_setup, _URE_PUNCT_MASK},
    {0x0070, 1, 50, 0, 0},
    {0x0061, 1, 51, 0, 0},
    {0x0063, 1, 52, 0, 0},
    {0x0065, 1, 53, 0, 0},
    {0x003a, 1, 54, _ure_space_setup, _URE_SPACE_MASK},
    {0x0070, 1, 55, 0, 0},
    {0x0070, 1, 56, 0, 0},
    {0x0065, 1, 57, 0, 0},
    {0x0072, 1, 58, 0, 0},
    {0x003a, 1, 59, _ure_ccl_setup, _URE_UPPER},
    {0x0064, 1, 60, 0, 0},
    {0x0069, 1, 61, 0, 0},
    {0x0067, 1, 62, 0, 0},
    {0x0069, 1, 63, 0, 0},
    {0x0074, 1, 64, 0, 0},
    {0x003a, 1, 65, _ure_xdigit_setup, 0},
};

/*
 * Probe for one of the POSIX colon delimited character classes in the static
 * trie.
 */
static unsigned long
_ure_posix_ccl(ucs2_t *cp, unsigned long limit, _ure_symtab_t *sym,
               _ure_buffer_t *b)
{
    int i;
    unsigned long n;
    const _ure_trie_t *tp;
    ucs2_t *sp, *ep;

    /*
     * If the number of characters left is less than 7, then this cannot be
     * interpreted as one of the colon delimited classes.
     */
    if (limit < 7)
      return 0;

    sp = cp;
    ep = sp + limit;
    tp = cclass_trie;
    for (i = 0; sp < ep && i < 8; i++, sp++) {
        n = tp->len;

        for (; n > 0 && tp->key != *sp; tp++, n--) ;

        if (n == 0)
          return 0;

        if (*sp == ':' && (i == 6 || i == 7)) {
            sp++;
            break;
        }
        if (sp + 1 < ep)
          tp = cclass_trie + tp->next;
    }
    if (tp->func == 0)
      return 0;

    (*tp->func)(sym, tp->mask, b);

    return sp - cp;
}

/*
 * Construct a list of ranges and return the number of characters consumed.
 */
static unsigned long
_ure_cclass(ucs2_t *cp, unsigned long limit, _ure_symtab_t *symp,
            _ure_buffer_t *b)
{
    int range_end;
    unsigned long n;
    ucs2_t *sp, *ep;
    ucs4_t c, last;
    _ure_ccl_t *cclp;
    _ure_range_t range;

    sp = cp;
    ep = sp + limit;

    if (*sp == '^') {
      symp->type = _URE_NCCLASS;
      sp++;
    } else
      symp->type = _URE_CCLASS;

    for (last = 0, range_end = 0;
         b->error == _URE_OK && sp < ep && *sp != ']'; ) {
        c = *sp++;
        if (c == '\\') {
            if (sp == ep) {
                /*
                 * The EOS was encountered when expecting the reverse solidus
                 * to be followed by the character it is escaping.  Set an
                 * error code and return the number of characters consumed up
                 * to this point.
                 */
                b->error = _URE_UNEXPECTED_EOS;
                return sp - cp;
            }

            c = *sp++;
            switch (c) {
              case 'a':
                c = 0x07;
                break;
              case 'b':
                c = 0x08;
                break;
              case 'f':
                c = 0x0c;
                break;
              case 'n':
                c = 0x0a;
                break;
              case 'r':
                c = 0x0d;
                break;
              case 't':
                c = 0x09;
                break;
              case 'v':
                c = 0x0b;
                break;
              case 'p':
              case 'P':
                sp += _ure_prop_list(sp, ep - sp, &symp->props, b);
                /*
                 * Invert the bit mask of the properties if this is a negated
                 * character class or if 'P' is used to specify a list of
                 * character properties that should *not* match in a
                 * character class.
                 */
                if (c == 'P')
                  symp->props = ~symp->props;
                continue;
                break;
              case 'x':
              case 'X':
              case 'u':
              case 'U':
                if (sp < ep &&
                    ((*sp >= '0' && *sp <= '9') ||
                     (*sp >= 'A' && *sp <= 'F') ||
                     (*sp >= 'a' && *sp <= 'f')))
                  sp += _ure_hex(sp, ep - sp, &c);
            }
        } else if (c == ':') {
            /*
             * Probe for a POSIX colon delimited character class.
             */
            sp--;
            if ((n = _ure_posix_ccl(sp, ep - sp, symp, b)) == 0)
              sp++;
            else {
                sp += n;
                continue;
            }
        }

        cclp = &symp->sym.ccl;

        /*
         * Check to see if the current character is a low surrogate that needs
         * to be combined with a preceding high surrogate.
         */
        if (last != 0) {
            if (c >= 0xdc00 && c <= 0xdfff)
              /*
               * Construct the UTF16 character code.
               */
              c = 0x10000 + (((last & 0x03ff) << 10) | (c & 0x03ff));
            else {
                /*
                 * Add the isolated high surrogate to the range.
                 */
                if (range_end == 1)
                  range.max_code = last & 0xffff;
                else
                  range.min_code = range.max_code = last & 0xffff;

                _ure_add_range(cclp, &range, b);
                range_end = 0;
            }
        }

        /*
         * Clear the last character code.
         */
        last = 0;

        /*
         * This slightly awkward code handles the different cases needed to
         * construct a range.
         */
        if (c >= 0xd800 && c <= 0xdbff) {
            /*
             * If the high surrogate is followed by a range indicator, simply
             * add it as the range start.  Otherwise, save it in case the next
             * character is a low surrogate.
             */
            if (*sp == '-') {
                sp++;
                range.min_code = c;
                range_end = 1;
            } else
              last = c;
        } else if (range_end == 1) {
            range.max_code = c;
            _ure_add_range(cclp, &range, b);
            range_end = 0;
        } else {
            range.min_code = range.max_code = c;
            if (*sp == '-') {
                sp++;
                range_end = 1;
            } else
              _ure_add_range(cclp, &range, b);
        }
    }

    if (sp < ep && *sp == ']')
      sp++;
    else
      /*
       * The parse was not terminated by the character class close symbol
       * (']'), so set an error code.
       */
      b->error = _URE_CCLASS_OPEN;

    return sp - cp;
}

/*
 * Probe for a low surrogate hex code.
 */
static unsigned long
_ure_probe_ls(ucs2_t *ls, unsigned long limit, ucs4_t *c)
{
    ucs4_t i, code;
    ucs2_t *sp, *ep;

    for (i = code = 0, sp = ls, ep = sp + limit; i < 4 && sp < ep; sp++) {
        if (*sp >= '0' && *sp <= '9')
          code = (code << 4) + (*sp - '0');
        else if (*sp >= 'A' && *sp <= 'F')
          code = (code << 4) + ((*sp - 'A') + 10);
        else if (*sp >= 'a' && *sp <= 'f')
          code = (code << 4) + ((*sp - 'a') + 10);
        else
          break;
    }

    *c = code;
    return (0xdc00 <= code && code <= 0xdfff) ? sp - ls : 0;
}

static unsigned long
_ure_compile_symbol(ucs2_t *sym, unsigned long limit, _ure_symtab_t *symp,
                    _ure_buffer_t *b)
{
    ucs4_t c;
    ucs2_t *sp, *ep;

    sp = sym;
    ep = sym + limit;

    if ((c = *sp++) == '\\') {

        if (sp == ep) {
            /*
             * The EOS was encountered when expecting the reverse solidus to
             * be followed by the character it is escaping.  Set an error code
             * and return the number of characters consumed up to this point.
             */
            b->error = _URE_UNEXPECTED_EOS;
            return sp - sym;
        }

        c = *sp++;
        switch (c) {
          case 'p':
          case 'P':
            symp->type = (c == 'p') ? _URE_CCLASS : _URE_NCCLASS;
            sp += _ure_prop_list(sp, ep - sp, &symp->props, b);
            break;
          case 'a':
            symp->type = _URE_CHAR;
            symp->sym.chr = 0x07;
            break;
          case 'b':
            symp->type = _URE_CHAR;
            symp->sym.chr = 0x08;
            break;
          case 'f':
            symp->type = _URE_CHAR;
            symp->sym.chr = 0x0c;
            break;
          case 'n':
            symp->type = _URE_CHAR;
            symp->sym.chr = 0x0a;
            break;
          case 'r':
            symp->type = _URE_CHAR;
            symp->sym.chr = 0x0d;
            break;
          case 't':
            symp->type = _URE_CHAR;
            symp->sym.chr = 0x09;
            break;
          case 'v':
            symp->type = _URE_CHAR;
            symp->sym.chr = 0x0b;
            break;
          case 'x':
          case 'X':
          case 'u':
          case 'U':
            /*
             * Collect between 1 and 4 digits representing a UCS2 code.  Fall
             * through to the next case.
             */
            if (sp < ep &&
                ((*sp >= '0' && *sp <= '9') ||
                 (*sp >= 'A' && *sp <= 'F') ||
                 (*sp >= 'a' && *sp <= 'f')))
              sp += _ure_hex(sp, ep - sp, &c);
            /* FALLTHROUGH */
          default:
            /*
             * Simply add an escaped character here.
             */
            symp->type = _URE_CHAR;
            symp->sym.chr = c;
        }
    } else if (c == '^' || c == '$')
      /*
       * Handle the BOL and EOL anchors.  This actually consists simply of
       * setting a flag that indicates that the user supplied anchor match
       * function should be called.  This needs to be done instead of simply
       * matching line/paragraph separators because beginning-of-text and
       * end-of-text tests are needed as well.
       */
      symp->type = (c == '^') ? _URE_BOL_ANCHOR : _URE_EOL_ANCHOR;
    else if (c == '[')
      /*
       * Construct a character class.
       */
      sp += _ure_cclass(sp, ep - sp, symp, b);
    else if (c == '.')
      symp->type = _URE_ANY_CHAR;
    else {
        symp->type = _URE_CHAR;
        symp->sym.chr = c;
    }

    /*
     * If the symbol type happens to be a character and is a high surrogate,
     * then probe forward to see if it is followed by a low surrogate that
     * needs to be added.
     */
    if (sp < ep && symp->type == _URE_CHAR &&
        0xd800 <= symp->sym.chr && symp->sym.chr <= 0xdbff) {

        if (0xdc00 <= *sp && *sp <= 0xdfff) {
            symp->sym.chr = 0x10000 + (((symp->sym.chr & 0x03ff) << 10) |
                                       (*sp & 0x03ff));
            sp++;
        } else if (*sp == '\\' && (*(sp + 1) == 'x' || *(sp + 1) == 'X' ||
                                 *(sp + 1) == 'u' || *(sp + 1) == 'U')) {
            sp += _ure_probe_ls(sp + 2, ep - (sp + 2), &c);
            if (0xdc00 <= c && c <= 0xdfff) {
                /*
                 * Take into account the \[xu] in front of the hex code.
                 */
                sp += 2;
                symp->sym.chr = 0x10000 + (((symp->sym.chr & 0x03ff) << 10) |
                                           (c & 0x03ff));
            }
        }
    }

    /*
     * Last, make sure any _URE_CHAR type symbols are changed to lower case if
     * the `casefold' flag is set.
     */
    if ((b->flags & _URE_DFA_CASEFOLD) && symp->type == _URE_CHAR)
      symp->sym.chr = _ure_tolower(symp->sym.chr);

    /*
     * If the symbol constructed is anything other than one of the anchors,
     * make sure the _URE_DFA_BLANKLINE flag is removed.
     */
    if (symp->type != _URE_BOL_ANCHOR && symp->type != _URE_EOL_ANCHOR)
      b->flags &= ~_URE_DFA_BLANKLINE;

    /*
     * Return the number of characters consumed.
     */
    return sp - sym;
}

static int
_ure_sym_neq(_ure_symtab_t *a, _ure_symtab_t *b)
{
    if (a->type != b->type || a->mods != b->mods || a->props != b->props)
      return 1;

    if (a->type == _URE_CCLASS || a->type == _URE_NCCLASS) {
        if (a->sym.ccl.ranges_used != b->sym.ccl.ranges_used)
          return 1;
        if (a->sym.ccl.ranges_used > 0 &&
            memcmp((char *) a->sym.ccl.ranges, (char *) b->sym.ccl.ranges,
                   sizeof(_ure_range_t) * a->sym.ccl.ranges_used) != 0)
          return 1;
    } else if (a->type == _URE_CHAR && a->sym.chr != b->sym.chr)
      return 1;
    return 0;
}

/*
 * Construct a symbol, but only keep unique symbols.
 */
static ucs2_t
_ure_make_symbol(ucs2_t *sym, unsigned long limit, unsigned long *consumed,
                 _ure_buffer_t *b)
{
    ucs2_t i;
    _ure_symtab_t *sp, symbol;

    /*
     * Build the next symbol so we can test to see if it is already in the
     * symbol table.
     */
    (void) memset((char *) &symbol, '\0', sizeof(_ure_symtab_t));
    *consumed = _ure_compile_symbol(sym, limit, &symbol, b);

    /*
     * Check to see if the symbol exists.
     */
    for (i = 0, sp = b->symtab;
         i < b->symtab_used && _ure_sym_neq(&symbol, sp); i++, sp++) ;

    if (i < b->symtab_used) {
        /*
         * Free up any ranges used for the symbol.
         */
        if ((symbol.type == _URE_CCLASS || symbol.type == _URE_NCCLASS) &&
            symbol.sym.ccl.ranges_size > 0)
          free((char *) symbol.sym.ccl.ranges);

        return b->symtab[i].id;
    }

    /*
     * Need to add the new symbol.
     */
    if (b->symtab_used == b->symtab_size) {
        if (b->symtab_size == 0)
          b->symtab = (_ure_symtab_t *) malloc(sizeof(_ure_symtab_t) << 3);
        else
          b->symtab = (_ure_symtab_t *)
              realloc((char *) b->symtab,
                      sizeof(_ure_symtab_t) * (b->symtab_size + 8));
        sp = b->symtab + b->symtab_size;
        (void) memset((char *) sp, '\0', sizeof(_ure_symtab_t) << 3);
        b->symtab_size += 8;
    }

    symbol.id = b->symtab_used++;
    (void) memcpy((char *) &b->symtab[symbol.id], (char *) &symbol,
                  sizeof(_ure_symtab_t));

    return symbol.id;
}

/*************************************************************************
 *
 * End symbol parse functions.
 *
 *************************************************************************/

static ucs2_t
_ure_make_expr(ucs2_t type, ucs2_t lhs, ucs2_t rhs, _ure_buffer_t *b)
{
    ucs2_t i;

    if (b == 0)
      return _URE_NOOP;

    /*
     * Determine if the expression already exists or not.
     */
    for (i = 0; i < b->expr_used; i++) {
        if (b->expr[i].type == type && b->expr[i].lhs == lhs &&
            b->expr[i].rhs == rhs)
          break;
    }
    if (i < b->expr_used)
      return i;

    /*
     * Need to add a new expression.
     */
    if (b->expr_used == b->expr_size) {
        if (b->expr_size == 0)
          b->expr = (_ure_elt_t *) malloc(sizeof(_ure_elt_t) << 3);
        else
          b->expr = (_ure_elt_t *)
              realloc((char *) b->expr,
                      sizeof(_ure_elt_t) * (b->expr_size + 8));
        b->expr_size += 8;
    }

    b->expr[b->expr_used].onstack = 0;
    b->expr[b->expr_used].type = type;
    b->expr[b->expr_used].lhs = lhs;
    b->expr[b->expr_used].rhs = rhs;

    return b->expr_used++;
}

static unsigned char spmap[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

#define _ure_isspecial(cc) ((cc) > 0x20 && (cc) < 0x7f && \
                            (spmap[(cc) >> 3] & (1 << ((cc) & 7))))

/*
 * Convert the regular expression into an NFA in a form that will be easy to
 * reduce to a DFA.  The starting state for the reduction will be returned.
 */
static ucs2_t
_ure_re2nfa(ucs2_t *re, unsigned long relen, _ure_buffer_t *b)
{
    ucs2_t c, state, top, sym, *sp, *ep;
    unsigned long used;

    state = _URE_NOOP;

    sp = re;
    ep = sp + relen;
    while (b->error == _URE_OK && sp < ep) {
        c = *sp++;
        switch (c) {
          case '(':
            _ure_push(_URE_PAREN, b);
            break;
          case ')':
            /*
             * Check for the case of too many close parentheses.
             */
            if (_ure_peek(b) == _URE_NOOP) {
                b->error = _URE_UNBALANCED_GROUP;
                break;
            }

            while ((top = _ure_peek(b)) == _URE_AND || top == _URE_OR)
              /*
               * Make an expression with the AND or OR operator and its right
               * hand side.
               */
              state = _ure_make_expr(_ure_pop(b), _ure_pop(b), state, b);

            /*
             * Remove the _URE_PAREN off the stack.
             */
            (void) _ure_pop(b);
            break;
          case '*':
            state = _ure_make_expr(_URE_STAR, state, _URE_NOOP, b);
            break;
          case '+':
            state = _ure_make_expr(_URE_PLUS, state, _URE_NOOP, b);
            break;
          case '?':
            state = _ure_make_expr(_URE_QUEST, state, _URE_NOOP, b);
            break;
          case '|':
            while ((top = _ure_peek(b)) == _URE_AND || top == _URE_OR)
              /*
               * Make an expression with the AND or OR operator and its right
               * hand side.
               */
              state = _ure_make_expr(_ure_pop(b), _ure_pop(b), state, b);

            _ure_push(state, b);
            _ure_push(_URE_OR, b);
            break;
          default:
            sp--;
            sym = _ure_make_symbol(sp, ep - sp, &used, b);
            sp += used;
            state = _ure_make_expr(_URE_SYMBOL, sym, _URE_NOOP, b);
            break;
        }

        if (c != '(' && c != '|' && sp < ep &&
            (!_ure_isspecial(*sp) || *sp == '(')) {
            _ure_push(state, b);
            _ure_push(_URE_AND, b);
        }
    }
    while ((top = _ure_peek(b)) == _URE_AND || top == _URE_OR)
      /*
       * Make an expression with the AND or OR operator and its right
       * hand side.
       */
      state = _ure_make_expr(_ure_pop(b), _ure_pop(b), state, b);

    if (b->stack.slist_used > 0)
      b->error = _URE_UNBALANCED_GROUP;

    return (b->error == _URE_OK) ? state : _URE_NOOP;
}

static void
_ure_add_symstate(ucs2_t sym, ucs2_t state, _ure_buffer_t *b)
{
    ucs2_t i, *stp;
    _ure_symtab_t *sp;

    /*
     * Locate the symbol in the symbol table so the state can be added.
     * If the symbol doesn't exist, then a real problem exists.
     */
    for (i = 0, sp = b->symtab; i < b->symtab_used && sym != sp->id;
         i++, sp++) ;

    /*
     * Now find out if the state exists in the symbol's state list.
     */
    for (i = 0, stp = sp->states.slist;
         i < sp->states.slist_used && state > *stp; i++, stp++) ;

    if (i == sp->states.slist_used || state < *stp) {
        /*
         * Need to add the state in order.
         */
        if (sp->states.slist_used == sp->states.slist_size) {
            if (sp->states.slist_size == 0)
              sp->states.slist = (ucs2_t *) malloc(sizeof(ucs2_t) << 3);
            else
              sp->states.slist = (ucs2_t *)
                  realloc((char *) sp->states.slist,
                          sizeof(ucs2_t) * (sp->states.slist_size + 8));
            sp->states.slist_size += 8;
        }
        if (i < sp->states.slist_used)
          (void) _ure_memmove((char *) (sp->states.slist + i + 1),
                              (char *) (sp->states.slist + i),
                              sizeof(ucs2_t) * (sp->states.slist_used - i));
        sp->states.slist[i] = state;
        sp->states.slist_used++;
    }
}

static ucs2_t
_ure_add_state(ucs2_t nstates, ucs2_t *states, _ure_buffer_t *b)
{
    ucs2_t i;
    _ure_state_t *sp;

    for (i = 0, sp = b->states.states; i < b->states.states_used; i++, sp++) {
        if (sp->st.slist_used == nstates &&
            memcmp((char *) states, (char *) sp->st.slist,
                   sizeof(ucs2_t) * nstates) == 0)
          break;
    }

    if (i == b->states.states_used) {
        /*
         * Need to add a new DFA state (set of NFA states).
         */
        if (b->states.states_used == b->states.states_size) {
            if (b->states.states_size == 0)
              b->states.states = (_ure_state_t *)
                  malloc(sizeof(_ure_state_t) << 3);
            else
              b->states.states = (_ure_state_t *)
                  realloc((char *) b->states.states,
                          sizeof(_ure_state_t) * (b->states.states_size + 8));
            sp = b->states.states + b->states.states_size;
            (void) memset((char *) sp, '\0', sizeof(_ure_state_t) << 3);
            b->states.states_size += 8;
        }

        sp = b->states.states + b->states.states_used++;
        sp->id = i;

        if (sp->st.slist_used + nstates > sp->st.slist_size) {
            if (sp->st.slist_size == 0)
              sp->st.slist = (ucs2_t *)
                  malloc(sizeof(ucs2_t) * (sp->st.slist_used + nstates));
            else
              sp->st.slist = (ucs2_t *)
                  realloc((char *) sp->st.slist,
                          sizeof(ucs2_t) * (sp->st.slist_used + nstates));
            sp->st.slist_size = sp->st.slist_used + nstates;
        }
        sp->st.slist_used = nstates;
        (void) memcpy((char *) sp->st.slist, (char *) states,
                      sizeof(ucs2_t) * nstates);
    }

    /*
     * Return the ID of the DFA state representing a group of NFA states.
     */
    return i;
}

static void
_ure_reduce(ucs2_t start, _ure_buffer_t *b)
{
    ucs2_t i, j, state, eval, syms, rhs;
    ucs2_t s1, s2, ns1, ns2;
    _ure_state_t *sp;
    _ure_symtab_t *smp;

    b->reducing = 1;

    /*
     * Add the starting state for the reduction.
     */
    _ure_add_state(1, &start, b);

    /*
     * Process each set of NFA states that get created.
     */
    for (i = 0; i < b->states.states_used; i++) {
        sp = b->states.states + i;

        /*
         * Push the current states on the stack.
         */
        for (j = 0; j < sp->st.slist_used; j++)
          _ure_push(sp->st.slist[j], b);

        /*
         * Reduce the NFA states.
         */
        for (j = sp->accepting = syms = 0; j < b->stack.slist_used; j++) {
            state = b->stack.slist[j];
            eval = 1;

            /*
             * This inner loop is the iterative equivalent of recursively
             * reducing subexpressions generated as a result of a reduction.
             */
            while (eval) {
                switch (b->expr[state].type) {
                  case _URE_SYMBOL:
                    ns1 = _ure_make_expr(_URE_ONE, _URE_NOOP, _URE_NOOP, b);
                    _ure_add_symstate(b->expr[state].lhs, ns1, b);
                    syms++;
                    eval = 0;
                    break;
                  case _URE_ONE:
                    sp->accepting = 1;
                    eval = 0;
                    break;
                  case _URE_QUEST:
                    s1 = b->expr[state].lhs;
                    ns1 = _ure_make_expr(_URE_ONE, _URE_NOOP, _URE_NOOP, b);
                    state = _ure_make_expr(_URE_OR, ns1, s1, b);
                    break;
                  case _URE_PLUS:
                    s1 = b->expr[state].lhs;
                    ns1 = _ure_make_expr(_URE_STAR, s1, _URE_NOOP, b);
                    state = _ure_make_expr(_URE_AND, s1, ns1, b);
                    break;
                  case _URE_STAR:
                    s1 = b->expr[state].lhs;
                    ns1 = _ure_make_expr(_URE_ONE, _URE_NOOP, _URE_NOOP, b);
                    ns2 = _ure_make_expr(_URE_PLUS, s1, _URE_NOOP, b);
                    state = _ure_make_expr(_URE_OR, ns1, ns2, b);
                    break;
                  case _URE_OR:
                    s1 = b->expr[state].lhs;
                    s2 = b->expr[state].rhs;
                    _ure_push(s1, b);
                    _ure_push(s2, b);
                    eval = 0;
                    break;
                  case _URE_AND:
                    s1 = b->expr[state].lhs;
                    s2 = b->expr[state].rhs;
                    switch (b->expr[s1].type) {
                      case _URE_SYMBOL:
                        _ure_add_symstate(b->expr[s1].lhs, s2, b);
                        syms++;
                        eval = 0;
                        break;
                      case _URE_ONE:
                        state = s2;
                        break;
                      case _URE_QUEST:
                        ns1 = b->expr[s1].lhs;
                        ns2 = _ure_make_expr(_URE_AND, ns1, s2, b);
                        state = _ure_make_expr(_URE_OR, s2, ns2, b);
                        break;
                      case _URE_PLUS:
                        ns1 = b->expr[s1].lhs;
                        ns2 = _ure_make_expr(_URE_OR, s2, state, b);
                        state = _ure_make_expr(_URE_AND, ns1, ns2, b);
                        break;
                      case _URE_STAR:
                        ns1 = b->expr[s1].lhs;
                        ns2 = _ure_make_expr(_URE_AND, ns1, state, b);
                        state = _ure_make_expr(_URE_OR, s2, ns2, b);
                        break;
                      case _URE_OR:
                        ns1 = b->expr[s1].lhs;
                        ns2 = b->expr[s1].rhs;
                        ns1 = _ure_make_expr(_URE_AND, ns1, s2, b);
                        ns2 = _ure_make_expr(_URE_AND, ns2, s2, b);
                        state = _ure_make_expr(_URE_OR, ns1, ns2, b);
                        break;
                      case _URE_AND:
                        ns1 = b->expr[s1].lhs;
                        ns2 = b->expr[s1].rhs;
                        ns2 = _ure_make_expr(_URE_AND, ns2, s2, b);
                        state = _ure_make_expr(_URE_AND, ns1, ns2, b);
                        break;
                    }
                }
            }
        }

        /*
         * Clear the state stack.
         */
        while (_ure_pop(b) != _URE_NOOP) ;

        /*
         * Reset the state pointer because the reduction may have moved it
         * during a reallocation.
         */
        sp = b->states.states + i;

        /*
         * Generate the DFA states for the symbols collected during the
         * current reduction.
         */
        if (sp->trans_used + syms > sp->trans_size) {
            if (sp->trans_size == 0)
              sp->trans = (_ure_elt_t *)
                  malloc(sizeof(_ure_elt_t) * (sp->trans_used + syms));
            else
              sp->trans = (_ure_elt_t *)
                  realloc((char *) sp->trans,
                          sizeof(_ure_elt_t) * (sp->trans_used + syms));
            sp->trans_size = sp->trans_used + syms;
        }

        /*
         * Go through the symbol table and generate the DFA state transitions
         * for each symbol that has collected NFA states.
         */
        for (j = syms = 0, smp = b->symtab; j < b->symtab_used; j++, smp++) {
            sp = b->states.states + i;

            if (smp->states.slist_used > 0) {
                sp->trans[syms].lhs = smp->id;
                rhs = _ure_add_state(smp->states.slist_used,
                                     smp->states.slist, b);
                /*
                 * Reset the state pointer in case the reallocation moves it
                 * in memory.
                 */
                sp = b->states.states + i;
                sp->trans[syms].rhs = rhs;

                smp->states.slist_used = 0;
                syms++;
            }
        }

        /*
         * Set the number of transitions actually used.
         */
        sp->trans_used = syms;
    }
    b->reducing = 0;
}

static void
_ure_add_equiv(ucs2_t l, ucs2_t r, _ure_buffer_t *b)
{
    ucs2_t tmp;

    l = b->states.states[l].id;
    r = b->states.states[r].id;

    if (l == r)
      return;

    if (l > r) {
        tmp = l;
        l = r;
        r = tmp;
    }

    /*
     * Check to see if the equivalence pair already exists.
     */
    for (tmp = 0; tmp < b->equiv_used &&
             (b->equiv[tmp].l != l || b->equiv[tmp].r != r);
         tmp++) ;

    if (tmp < b->equiv_used)
      return;

    if (b->equiv_used == b->equiv_size) {
        if (b->equiv_size == 0)
          b->equiv = (_ure_equiv_t *) malloc(sizeof(_ure_equiv_t) << 3);
        else
          b->equiv = (_ure_equiv_t *) realloc((char *) b->equiv,
                                              sizeof(_ure_equiv_t) *
                                              (b->equiv_size + 8));
        b->equiv_size += 8;
    }
    b->equiv[b->equiv_used].l = l;
    b->equiv[b->equiv_used].r = r;
    b->equiv_used++;
}

/*
 * Merge the DFA states that are equivalent.
 */
static void
_ure_merge_equiv(_ure_buffer_t *b)
{
    ucs2_t i, j, k, eq, done;
    _ure_state_t *sp1, *sp2, *ls, *rs;

    for (i = 0; i < b->states.states_used; i++) {
        sp1 = b->states.states + i;
        if (sp1->id != i)
          continue;
        for (j = 0; j < i; j++) {
            sp2 = b->states.states + j;
            if (sp2->id != j)
              continue;
            b->equiv_used = 0;
            _ure_add_equiv(i, j, b);
            for (eq = 0, done = 0; eq < b->equiv_used; eq++) {
                ls = b->states.states + b->equiv[eq].l;
                rs = b->states.states + b->equiv[eq].r;
                if (ls->accepting != rs->accepting ||
                    ls->trans_used != rs->trans_used) {
                    done = 1;
                    break;
                }
                for (k = 0; k < ls->trans_used &&
                         ls->trans[k].lhs == rs->trans[k].lhs; k++) ;
                if (k < ls->trans_used) {
                    done = 1;
                    break;
                }

                for (k = 0; k < ls->trans_used; k++)
                  _ure_add_equiv(ls->trans[k].rhs, rs->trans[k].rhs, b);
            }
            if (done == 0)
              break;
        }
        for (eq = 0; j < i && eq < b->equiv_used; eq++)
          b->states.states[b->equiv[eq].r].id =
              b->states.states[b->equiv[eq].l].id;
    }

    /*
     * Renumber the states appropriately.
     */
    for (i = eq = 0, sp1 = b->states.states; i < b->states.states_used;
         sp1++, i++)
      sp1->id = (sp1->id == i) ? eq++ : b->states.states[sp1->id].id;
}

/*************************************************************************
 *
 * API.
 *
 *************************************************************************/

ure_buffer_t
ure_buffer_create(void)
{
    ure_buffer_t b;

    b = (ure_buffer_t) calloc(1, sizeof(_ure_buffer_t));

    return b;
}

void
ure_buffer_free(ure_buffer_t buf)
{
    unsigned long i;

    if (buf == 0)
      return;

    if (buf->stack.slist_size > 0)
      free((char *) buf->stack.slist);

    if (buf->expr_size > 0)
      free((char *) buf->expr);

    for (i = 0; i < buf->symtab_size; i++) {
        if (buf->symtab[i].states.slist_size > 0)
          free((char *) buf->symtab[i].states.slist);
    }

    if (buf->symtab_size > 0)
      free((char *) buf->symtab);

    for (i = 0; i < buf->states.states_size; i++) {
        if (buf->states.states[i].trans_size > 0)
          free((char *) buf->states.states[i].trans);
        if (buf->states.states[i].st.slist_size > 0)
          free((char *) buf->states.states[i].st.slist);
    }

    if (buf->states.states_size > 0)
      free((char *) buf->states.states);

    if (buf->equiv_size > 0)
      free((char *) buf->equiv);

    free((char *) buf);
}

ure_dfa_t
ure_compile(ucs2_t *re, unsigned long relen, int casefold, ure_buffer_t buf)
{
    ucs2_t i, j, state;
    _ure_state_t *sp;
    _ure_dstate_t *dsp;
    _ure_trans_t *tp;
    ure_dfa_t dfa;

    if (re == 0 || *re == 0 || relen == 0 || buf == 0)
      return 0;

    /*
     * Reset the various fields of the compilation buffer.  Default the flags
     * to indicate the presense of the "^$" pattern.  If any other pattern
     * occurs, then this flag will be removed.  This is done to catch this
     * special pattern and handle it specially when matching.
     */
    buf->flags = _URE_DFA_BLANKLINE | ((casefold) ? _URE_DFA_CASEFOLD : 0);
    buf->reducing = 0;
    buf->stack.slist_used = 0;
    buf->expr_used = 0;

    for (i = 0; i < buf->symtab_used; i++)
      buf->symtab[i].states.slist_used = 0;
    buf->symtab_used = 0;

    for (i = 0; i < buf->states.states_used; i++) {
        buf->states.states[i].st.slist_used = 0;
        buf->states.states[i].trans_used = 0;
    }
    buf->states.states_used = 0;

    /*
     * Construct the NFA.  If this stage returns a 0, then an error occured or
     * an empty expression was passed.
     */
    if ((state = _ure_re2nfa(re, relen, buf)) == _URE_NOOP)
      return 0;

    /*
     * Do the expression reduction to get the initial DFA.
     */
    _ure_reduce(state, buf);

    /*
     * Merge all the equivalent DFA states.
     */
    _ure_merge_equiv(buf);

    /*
     * Construct the minimal DFA.
     */
    dfa = (ure_dfa_t) malloc(sizeof(_ure_dfa_t));
    (void) memset((char *) dfa, '\0', sizeof(_ure_dfa_t));

    dfa->flags = buf->flags & (_URE_DFA_CASEFOLD|_URE_DFA_BLANKLINE);

    /*
     * Free up the NFA state groups and transfer the symbols from the buffer
     * to the DFA.
     */
    for (i = 0; i < buf->symtab_size; i++) {
        if (buf->symtab[i].states.slist_size > 0)
          free((char *) buf->symtab[i].states.slist);
    }
    dfa->syms = buf->symtab;
    dfa->nsyms = buf->symtab_used;

    buf->symtab_used = buf->symtab_size = 0;

    /*
     * Collect the total number of states and transitions needed for the DFA.
     */
    for (i = state = 0, sp = buf->states.states; i < buf->states.states_used;
         i++, sp++) {
        if (sp->id == state) {
            dfa->nstates++;
            dfa->ntrans += sp->trans_used;
            state++;
        }
    }

    /*
     * Allocate enough space for the states and transitions.
     */
    dfa->states = (_ure_dstate_t *) malloc(sizeof(_ure_dstate_t) *
                                           dfa->nstates);
    dfa->trans = (_ure_trans_t *) malloc(sizeof(_ure_trans_t) * dfa->ntrans);

    /*
     * Actually transfer the DFA states from the buffer.
     */
    dsp = dfa->states;
    tp = dfa->trans;
    for (i = state = 0, sp = buf->states.states; i < buf->states.states_used;
         i++, sp++) {
        if (sp->id == state) {
            dsp->trans = tp;
            dsp->ntrans = sp->trans_used;
            dsp->accepting = sp->accepting;

            /*
             * Add the transitions for the state.
             */
            for (j = 0; j < dsp->ntrans; j++, tp++) {
                tp->symbol = sp->trans[j].lhs;
                tp->next_state = buf->states.states[sp->trans[j].rhs].id;
            }

            dsp++;
            state++;
        }
    }

    return dfa;
}

void
ure_dfa_free(ure_dfa_t dfa)
{
    ucs2_t i;

    if (dfa == 0)
      return;

    for (i = 0; i < dfa->nsyms; i++) {
        if ((dfa->syms[i].type == _URE_CCLASS ||
             dfa->syms[i].type == _URE_NCCLASS) &&
            dfa->syms[i].sym.ccl.ranges_size > 0)
          free((char *) dfa->syms[i].sym.ccl.ranges);
    }
    if (dfa->nsyms > 0)
      free((char *) dfa->syms);

    if (dfa->nstates > 0)
      free((char *) dfa->states);
    if (dfa->ntrans > 0)
      free((char *) dfa->trans);
    free((char *) dfa);
}

void
ure_write_dfa(ure_dfa_t dfa, FILE *out)
{
    ucs2_t i, j, k, h, l;
    _ure_dstate_t *sp;
    _ure_symtab_t *sym;
    _ure_range_t *rp;

    if (dfa == 0 || out == 0)
      return;

    /*
     * Write all the different character classes.
     */
    for (i = 0, sym = dfa->syms; i < dfa->nsyms; i++, sym++) {
        if (sym->type == _URE_CCLASS || sym->type == _URE_NCCLASS) {
            fprintf(out, "C%hd = ", sym->id);
            if (sym->sym.ccl.ranges_used > 0) {
                putc('[', out);
                if (sym->type == _URE_NCCLASS)
                  putc('^', out);
            }
            if (sym->props != 0) {
                if (sym->type == _URE_NCCLASS)
                  fprintf(out, "\\P");
                else
                  fprintf(out, "\\p");
                for (k = h = 0; k < 32; k++) {
                    if (sym->props & (1 << k)) {
                        if (h != 0)
                          putc(',', out);
                        fprintf(out, "%d", k + 1);
                        h = 1;
                    }
                }
            }
            /*
             * Dump the ranges.
             */
            for (k = 0, rp = sym->sym.ccl.ranges;
                 k < sym->sym.ccl.ranges_used; k++, rp++) {
                /*
                 * Check for UTF16 characters.
                 */
                if (0x10000 <= rp->min_code &&
                    rp->min_code <= 0x10ffff) {
                    h = (ucs2_t) (((rp->min_code - 0x10000) >> 10) + 0xd800);
                    l = (ucs2_t) (((rp->min_code - 0x10000) & 1023) + 0xdc00);
                    fprintf(out, "\\x%04hX\\x%04hX", h, l);
                } else
                    fprintf(out, "\\x%04lX",
                            (unsigned long)(rp->min_code & 0xffff));
                if (rp->max_code != rp->min_code) {
                    putc('-', out);
                    if (rp->max_code >= 0x10000 &&
                        rp->max_code <= 0x10ffff) {
                        h = (ucs2_t) (((rp->max_code - 0x10000) >> 10) + 0xd800);
                        l = (ucs2_t) (((rp->max_code - 0x10000) & 1023) + 0xdc00);
                        fprintf(out, "\\x%04hX\\x%04hX", h, l);
                    } else
                        fprintf(out, "\\x%04lX",
                                (unsigned long)(rp->max_code & 0xffff));
                }
            }
            if (sym->sym.ccl.ranges_used > 0)
              putc(']', out);
            putc('\n', out);
        }
    }

    for (i = 0, sp = dfa->states; i < dfa->nstates; i++, sp++) {
        fprintf(out, "S%hd = ", i);
        if (sp->accepting) {
            fprintf(out, "1 ");
            if (sp->ntrans)
              fprintf(out, "| ");
        }
        for (j = 0; j < sp->ntrans; j++) {
            if (j > 0)
              fprintf(out, "| ");

            sym = dfa->syms + sp->trans[j].symbol;
            switch (sym->type) {
              case _URE_CHAR:
                if (0x10000 <= sym->sym.chr && sym->sym.chr <= 0x10ffff) {
                    /*
                     * Take care of UTF16 characters.
                     */
                    h = (ucs2_t) (((sym->sym.chr - 0x10000) >> 10) + 0xd800);
                    l = (ucs2_t) (((sym->sym.chr - 0x10000) & 1023) + 0xdc00);
                    fprintf(out, "\\x%04hX\\x%04hX ", h, l);
                } else
                    fprintf(out, "\\x%04lX ",
                            (unsigned long)(sym->sym.chr & 0xffff));
                break;
              case _URE_ANY_CHAR:
                fprintf(out, "<any> ");
                break;
              case _URE_BOL_ANCHOR:
                fprintf(out, "<bol-anchor> ");
                break;
              case _URE_EOL_ANCHOR:
                fprintf(out, "<eol-anchor> ");
                break;
              case _URE_CCLASS:
              case _URE_NCCLASS:
                fprintf(out, "[C%hd] ", sym->id);
                break;
            }
            fprintf(out, "S%hd", sp->trans[j].next_state);
            if (j + 1 < sp->ntrans)
              putc(' ', out);
        }
        putc('\n', out);
    }
}

#define _ure_issep(cc) ((cc) == '\n' || (cc) == '\r' || (cc) == 0x2028 ||\
                        (cc) == 0x2029)

int
ure_exec(ure_dfa_t dfa, int flags, ucs2_t *text, unsigned long textlen,
         unsigned long *match_start, unsigned long *match_end)
{
    int i, j, matched, found;
    unsigned long ms, me;
    ucs4_t c;
    ucs2_t *sp, *ep, *lp;
    _ure_dstate_t *stp;
    _ure_symtab_t *sym;
    _ure_range_t *rp;

    if (dfa == 0 || text == 0)
      return 0;

    /*
     * Handle the special case of an empty string matching the "^$" pattern.
     */
    if (textlen == 0 && (dfa->flags & _URE_DFA_BLANKLINE)) {
        *match_start = *match_end = 0;
        return 1;
    }

    sp = text;
    ep = sp + textlen;

    ms = me = ~0;

    stp = dfa->states;

    for (found = 0; found == 0 && sp < ep; ) {
        lp = sp;
        c = *sp++;

        /*
         * Check to see if this is a high surrogate that should be
         * combined with a following low surrogate.
         */
        if (sp < ep && 0xd800 <= c && c <= 0xdbff &&
            0xdc00 <= *sp && *sp <= 0xdfff)
          c = 0x10000 + (((c & 0x03ff) << 10) | (*sp++ & 0x03ff));

        /*
         * Determine if the character is non-spacing and should be skipped.
         */
        if (_ure_matches_properties(_URE_NONSPACING, c) &&
            (flags & URE_IGNORE_NONSPACING)) {
            sp++;
            continue;
        }

        if (dfa->flags & _URE_DFA_CASEFOLD)
          c = _ure_tolower(c);

        /*
         * See if one of the transitions matches.
         */
        for (i = 0, matched = 0; matched == 0 && i < stp->ntrans; i++) {
            sym = dfa->syms + stp->trans[i].symbol;
            switch (sym->type) {
              case _URE_ANY_CHAR:
                if ((flags & URE_DOT_MATCHES_SEPARATORS) ||
                    !_ure_issep(c))
                  matched = 1;
                break;
              case _URE_CHAR:
                if (c == sym->sym.chr)
                  matched = 1;
                break;
              case _URE_BOL_ANCHOR:
                if (lp == text) {
                    sp = lp;
                    matched = 1;
                } else if (_ure_issep(c)) {
                    if (c == '\r' && sp < ep && *sp == '\n')
                      sp++;
                    lp = sp;
                    matched = 1;
                }
                break;
              case _URE_EOL_ANCHOR:
                if (_ure_issep(c)) {
                    /*
                     * Put the pointer back before the separator so the match
                     * end position will be correct.  This case will also
                     * cause the `sp' pointer to be advanced over the current
                     * separator once the match end point has been recorded.
                     */
                    sp = lp;
                    matched = 1;
                }
                break;
              case _URE_CCLASS:
              case _URE_NCCLASS:
                if (sym->props != 0)
                  matched = _ure_matches_properties(sym->props, c);
                for (j = 0, rp = sym->sym.ccl.ranges;
                     j < sym->sym.ccl.ranges_used; j++, rp++) {
                    if (rp->min_code <= c && c <= rp->max_code)
                      matched = 1;
                }
                if (sym->type == _URE_NCCLASS)
                  matched = !matched;
                break;
            }

            if (matched) {
                if (ms == ~0UL)
                  ms = lp - text;
                else
                  me = sp - text;
                stp = dfa->states + stp->trans[i].next_state;

                /*
                 * If the match was an EOL anchor, adjust the pointer past the
                 * separator that caused the match.  The correct match
                 * position has been recorded already.
                 */
                if (sym->type == _URE_EOL_ANCHOR) {
                    /*
                     * Skip the character that caused the match.
                     */
                    sp++;

                    /*
                     * Handle the infamous CRLF situation.
                     */
                    if (sp < ep && c == '\r' && *sp == '\n')
                      sp++;
                }
            }
        }

        if (matched == 0) {
            if (stp->accepting == 0) {
                /*
                 * If the last state was not accepting, then reset
                 * and start over.
                 */
                stp = dfa->states;
                ms = me = ~0;
            } else
              /*
               * The last state was accepting, so terminate the matching
               * loop to avoid more work.
               */
              found = 1;
        } else if (sp == ep) {
            if (!stp->accepting) {
                /*
                 * This ugly hack is to make sure the end-of-line anchors
                 * match when the source text hits the end.  This is only done
                 * if the last subexpression matches.
                 */
                for (i = 0; found == 0 && i < stp->ntrans; i++) {
                    sym = dfa->syms + stp->trans[i].symbol;
                    if (sym->type ==_URE_EOL_ANCHOR) {
                        stp = dfa->states + stp->trans[i].next_state;
                        if (stp->accepting) {
                            me = sp - text;
                            found = 1;
                        } else
                          break;
                    }
                }
            } else {
                /*
                 * Make sure any conditions that match all the way to the end
                 * of the string match.
                 */
                found = 1;
                me = sp - text;
            }
        }
    }

    if (found == 0)
      ms = me = ~0;

    *match_start = ms;
    *match_end = me;

    return (ms != ~0UL) ? 1 : 0;
}
