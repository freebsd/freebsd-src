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
 *  Author: Zeyd M. Ben-Halim <zmbenhal@netcom.com> 1992,1995               *
 *     and: Eric S. Raymond <esr@snark.thyrsus.com>                         *
 ****************************************************************************/

/*
 * alloc_entry.c -- allocation functions for terminfo entries
 *
 *	_nc_copy_entry()
 *	_nc_init_entry()
 *	_nc_merge_entry()
 *	_nc_save_str()
 *	_nc_wrap_entry()
 *
 */

#include <curses.priv.h>

#include <tic.h>
#include <term_entry.h>

MODULE_ID("$Id: alloc_entry.c,v 1.35 2001/01/13 22:40:17 tom Exp $")

#define ABSENT_OFFSET    -1
#define CANCELLED_OFFSET -2

#define MAX_STRTAB	4096	/* documented maximum entry size */

static char stringbuf[MAX_STRTAB];	/* buffer for string capabilities */
static size_t next_free;	/* next free character in stringbuf */

NCURSES_EXPORT(void)
_nc_init_entry(TERMTYPE * const tp)
/* initialize a terminal type data block */
{
    int i;

#if NCURSES_XNAMES
    tp->num_Booleans = BOOLCOUNT;
    tp->num_Numbers = NUMCOUNT;
    tp->num_Strings = STRCOUNT;
    tp->ext_Booleans = 0;
    tp->ext_Numbers = 0;
    tp->ext_Strings = 0;
#endif
    if (tp->Booleans == 0)
	tp->Booleans = typeMalloc(char, BOOLCOUNT);
    if (tp->Numbers == 0)
	tp->Numbers = typeMalloc(short, NUMCOUNT);
    if (tp->Strings == 0)
	tp->Strings = typeMalloc(char *, STRCOUNT);

    for_each_boolean(i, tp)
	tp->Booleans[i] = FALSE;

    for_each_number(i, tp)
	tp->Numbers[i] = ABSENT_NUMERIC;

    for_each_string(i, tp)
	tp->Strings[i] = ABSENT_STRING;

    next_free = 0;
}

NCURSES_EXPORT(ENTRY *)
_nc_copy_entry(ENTRY * oldp)
{
    ENTRY *newp = typeCalloc(ENTRY, 1);

    if (newp != 0) {
	*newp = *oldp;
	_nc_copy_termtype(&(newp->tterm), &(oldp->tterm));
    }
    return newp;
}

NCURSES_EXPORT(char *)
_nc_save_str(const char *const string)
/* save a copy of string in the string buffer */
{
    size_t old_next_free = next_free;
    size_t len = strlen(string) + 1;

    if (next_free + len < MAX_STRTAB) {
	strcpy(&stringbuf[next_free], string);
	DEBUG(7, ("Saved string %s", _nc_visbuf(string)));
	DEBUG(7, ("at location %d", (int) next_free));
	next_free += len;
    }
    return (stringbuf + old_next_free);
}

NCURSES_EXPORT(void)
_nc_wrap_entry(ENTRY * const ep, bool copy_strings)
/* copy the string parts to allocated storage, preserving pointers to it */
{
    int offsets[MAX_ENTRY_SIZE / 2], useoffsets[MAX_USES];
    int i, n;
    TERMTYPE *tp = &(ep->tterm);

    if (copy_strings) {
	next_free = 0;		/* clear static storage */

	/* copy term_names, Strings, uses */
	tp->term_names = _nc_save_str(tp->term_names);
	for_each_string(i, tp) {
	    if (tp->Strings[i] != ABSENT_STRING &&
		tp->Strings[i] != CANCELLED_STRING) {
		tp->Strings[i] = _nc_save_str(tp->Strings[i]);
	    }
	}

	for (i = 0; i < ep->nuses; i++) {
	    if (ep->uses[i].name == 0) {
		ep->uses[i].name = _nc_save_str(ep->uses[i].name);
	    }
	}

	free(tp->str_table);
    }

    n = tp->term_names - stringbuf;
    for_each_string(i, &(ep->tterm)) {
	if (tp->Strings[i] == ABSENT_STRING)
	    offsets[i] = ABSENT_OFFSET;
	else if (tp->Strings[i] == CANCELLED_STRING)
	    offsets[i] = CANCELLED_OFFSET;
	else
	    offsets[i] = tp->Strings[i] - stringbuf;
    }

    for (i = 0; i < ep->nuses; i++) {
	if (ep->uses[i].name == 0)
	    useoffsets[i] = ABSENT_OFFSET;
	else
	    useoffsets[i] = ep->uses[i].name - stringbuf;
    }

    if ((tp->str_table = typeMalloc(char, next_free)) == (char *) 0)
	  _nc_err_abort("Out of memory");
    (void) memcpy(tp->str_table, stringbuf, next_free);

    tp->term_names = tp->str_table + n;
    for_each_string(i, &(ep->tterm)) {
	if (offsets[i] == ABSENT_OFFSET)
	    tp->Strings[i] = ABSENT_STRING;
	else if (offsets[i] == CANCELLED_OFFSET)
	    tp->Strings[i] = CANCELLED_STRING;
	else
	    tp->Strings[i] = tp->str_table + offsets[i];
    }

#if NCURSES_XNAMES
    if (!copy_strings) {
	if ((n = NUM_EXT_NAMES(tp)) != 0) {
	    unsigned length = 0;
	    for (i = 0; i < n; i++) {
		length += strlen(tp->ext_Names[i]) + 1;
		offsets[i] = tp->ext_Names[i] - stringbuf;
	    }
	    if ((tp->ext_str_table = typeMalloc(char, length)) == 0)
		  _nc_err_abort("Out of memory");
	    for (i = 0, length = 0; i < n; i++) {
		tp->ext_Names[i] = tp->ext_str_table + length;
		strcpy(tp->ext_Names[i], stringbuf + offsets[i]);
		length += strlen(tp->ext_Names[i]) + 1;
	    }
	}
    }
#endif

    for (i = 0; i < ep->nuses; i++) {
	if (useoffsets[i] == ABSENT_OFFSET)
	    ep->uses[i].name = 0;
	else
	    ep->uses[i].name = (tp->str_table + useoffsets[i]);
    }
}

NCURSES_EXPORT(void)
_nc_merge_entry
(TERMTYPE * const to, TERMTYPE * const from)
/* merge capabilities from `from' entry into `to' entry */
{
    int i;

#if NCURSES_XNAMES
    _nc_align_termtype(to, from);
#endif
    for_each_boolean(i, from) {
	int mergebool = from->Booleans[i];

	if (mergebool == CANCELLED_BOOLEAN)
	    to->Booleans[i] = FALSE;
	else if (mergebool == TRUE)
	    to->Booleans[i] = mergebool;
    }

    for_each_number(i, from) {
	int mergenum = from->Numbers[i];

	if (mergenum == CANCELLED_NUMERIC)
	    to->Numbers[i] = ABSENT_NUMERIC;
	else if (mergenum != ABSENT_NUMERIC)
	    to->Numbers[i] = mergenum;
    }

    /*
     * Note: the copies of strings this makes don't have their own
     * storage.  This is OK right now, but will be a problem if we
     * we ever want to deallocate entries.
     */
    for_each_string(i, from) {
	char *mergestring = from->Strings[i];

	if (mergestring == CANCELLED_STRING)
	    to->Strings[i] = ABSENT_STRING;
	else if (mergestring != ABSENT_STRING)
	    to->Strings[i] = mergestring;
    }
}
