/*
 *                     RCS keyword extraction
 */
/*****************************************************************************
 *                       main routine: getoldkeys()
 *                       Testprogram: define KEEPTEST
 *****************************************************************************
 */

/* Copyright (C) 1982, 1988, 1989 Walter Tichy
   Copyright 1990, 1991 by Paul Eggert
   Distributed under license by the Free Software Foundation, Inc.

This file is part of RCS.

RCS is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

RCS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RCS; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

Report problems and direct all questions to:

    rcs-bugs@cs.purdue.edu

*/



/* $Log: rcskeep.c,v $
 * Revision 5.4  1991/08/19  03:13:55  eggert
 * Tune.
 *
 * Revision 5.3  1991/04/21  11:58:25  eggert
 * Shorten names to keep them distinct on shortname hosts.
 *
 * Revision 5.2  1990/10/04  06:30:20  eggert
 * Parse time zone offsets; future RCS versions may output them.
 *
 * Revision 5.1  1990/09/20  02:38:56  eggert
 * ci -k now checks dates more thoroughly.
 *
 * Revision 5.0  1990/08/22  08:12:53  eggert
 * Retrieve old log message if there is one.
 * Don't require final newline.
 * Remove compile-time limits; use malloc instead.  Tune.
 * Permit dates past 1999/12/31.  Ansify and Posixate.
 *
 * Revision 4.6  89/05/01  15:12:56  narten
 * changed copyright header to reflect current distribution rules
 * 
 * Revision 4.5  88/08/09  19:13:03  eggert
 * Remove lint and speed up by making FILE *fp local, not global.
 * 
 * Revision 4.4  87/12/18  11:44:21  narten
 * more lint cleanups (Guy Harris)
 * 
 * Revision 4.3  87/10/18  10:35:50  narten
 * Updating version numbers. Changes relative to 1.1 actually relative
 * to 4.1
 * 
 * Revision 1.3  87/09/24  14:00:00  narten
 * Sources now pass through lint (if you ignore printf/sprintf/fprintf 
 * warnings)
 * 
 * Revision 1.2  87/03/27  14:22:29  jenkins
 * Port to suns
 * 
 * Revision 4.1  83/05/10  16:26:44  wft
 * Added new markers Id and RCSfile; extraction added.
 * Marker matching with trymatch().
 * 
 * Revision 3.2  82/12/24  12:08:26  wft
 * added missing #endif.
 *
 * Revision 3.1  82/12/04  13:22:41  wft
 * Initial revision.
 *
 */

/*
#define KEEPTEST
*/
/* Testprogram; prints out the keyword values found. */

#include  "rcsbase.h"

libId(keepId, "$Id: rcskeep.c,v 5.4 1991/08/19 03:13:55 eggert Exp $")

static int checknum P((char const*,int));
static int getval P((RILE*,struct buf*,int));
static int get0val P((int,RILE*,struct buf*,int));
static int keepdate P((RILE*));
static int keepid P((int,RILE*,struct buf*));
static int keeprev P((RILE*));

int prevkeys;
struct buf prevauthor, prevdate, prevrev, prevstate;

	int
getoldkeys(fp)
	register RILE *fp;
/* Function: Tries to read keyword values for author, date,
 * revision number, and state out of the file fp.
 * If FNAME is nonnull, it is opened and closed instead of using FP.
 * The results are placed into
 * prevauthor, prevdate, prevrev, prevstate.
 * Aborts immediately if it finds an error and returns false.
 * If it returns true, it doesn't mean that any of the
 * values were found; instead, check to see whether the corresponding arrays
 * contain the empty string.
 */
{
    register int c;
    char keyword[keylength+1];
    register char * tp;
    int needs_closing;

    if (prevkeys)
	return true;

    needs_closing = false;
    if (!fp) {
	if (!(fp = Iopen(workfilename, FOPEN_R_WORK, (struct stat*)0))) {
	    eerror(workfilename);
	    return false;
	}
	needs_closing = true;
    }

    /* initialize to empty */
    bufscpy(&prevauthor, "");
    bufscpy(&prevdate, "");
    bufscpy(&prevrev, "");
    bufscpy(&prevstate, "");

    c = '\0'; /* anything but KDELIM */
    for (;;) {
        if ( c==KDELIM) {
	    do {
		/* try to get keyword */
		tp = keyword;
		for (;;) {
		    Igeteof(fp, c, goto ok;);
		    switch (c) {
			default:
			    if (keyword+keylength <= tp)
				break;
			    *tp++ = c;
			    continue;

			case '\n': case KDELIM: case VDELIM:
			    break;
		    }
		    break;
		}
	    } while (c==KDELIM);
            if (c!=VDELIM) continue;
	    *tp = c;
	    Igeteof(fp, c, break;);
	    switch (c) {
		case ' ': case '\t': break;
		default: continue;
	    }

	    switch (trymatch(keyword)) {
            case Author:
		if (!keepid(0, fp, &prevauthor))
		    return false;
		c = 0;
                break;
            case Date:
		if (!(c = keepdate(fp)))
		    return false;
                break;
            case Header:
            case Id:
		if (!(
		      getval(fp, (struct buf*)nil, false) &&
		      keeprev(fp) &&
		      (c = keepdate(fp)) &&
		      keepid(c, fp, &prevauthor) &&
		      keepid(0, fp, &prevstate)
		))
		    return false;
		/* Skip either ``who'' (new form) or ``Locker: who'' (old).  */
		if (getval(fp, (struct buf*)nil, true) &&
		    getval(fp, (struct buf*)nil, true))
			c = 0;
		else if (nerror)
			return false;
		else
			c = KDELIM;
		break;
            case Locker:
            case Log:
            case RCSfile:
            case Source:
		if (!getval(fp, (struct buf*)nil, false))
		    return false;
		c = 0;
                break;
            case Revision:
		if (!keeprev(fp))
		    return false;
		c = 0;
                break;
            case State:
		if (!keepid(0, fp, &prevstate))
		    return false;
		c = 0;
                break;
            default:
               continue;
            }
	    if (!c)
		Igeteof(fp, c, c=0;);
	    if (c != KDELIM) {
		error("closing %c missing on keyword", KDELIM);
		return false;
	    }
	    if (*prevauthor.string && *prevdate.string && *prevrev.string && *prevstate.string) {
                break;
           }
        }
	Igeteof(fp, c, break;);
    }

 ok:
    if (needs_closing)
	Ifclose(fp);
    else
	Irewind(fp);
    prevkeys = true;
    return true;
}

	static int
badly_terminated()
{
	error("badly terminated keyword value");
	return false;
}

	static int
getval(fp, target, optional)
	register RILE *fp;
	struct buf *target;
	int optional;
/* Reads a keyword value from FP into TARGET.
 * Returns true if one is found, false otherwise.
 * Does not modify target if it is nil.
 * Do not report an error if OPTIONAL is set and KDELIM is found instead.
 */
{
	int c;
	Igeteof(fp, c, return badly_terminated(););
	return get0val(c, fp, target, optional);
}

	static int
get0val(c, fp, target, optional)
	register int c;
	register RILE *fp;
	struct buf *target;
	int optional;
/* Reads a keyword value from C+FP into TARGET, perhaps OPTIONALly.
 * Same as getval, except C is the lookahead character.
 */
{   register char * tp;
    char const *tlim;
    register int got1;

    if (target) {
	bufalloc(target, 1);
	tp = target->string;
	tlim = tp + target->size;
    } else
	tlim = tp = 0;
    got1 = false;
    for (;;) {
	switch (c) {
	    default:
		got1 = true;
		if (tp) {
		    *tp++ = c;
		    if (tlim <= tp)
			tp = bufenlarge(target, &tlim);
		}
		break;

	    case ' ':
	    case '\t':
		if (tp) {
		    *tp = 0;
#		    ifdef KEEPTEST
			VOID printf("getval: %s\n", target);
#		    endif
		}
		if (!got1)
		    error("too much white space in keyword value");
		return got1;

	    case KDELIM:
		if (!got1 && optional)
		    return false;
		/* fall into */
	    case '\n':
	    case 0:
		return badly_terminated();
	}
	Igeteof(fp, c, return badly_terminated(););
    }
}


	static int
keepdate(fp)
	RILE *fp;
/* Function: reads a date prevdate; checks format
 * Return 0 on error, lookahead character otherwise.
 */
{
    struct buf prevday, prevtime, prevzone;
    register char const *p;
    register int c;

    c = 0;
    bufautobegin(&prevday);
    if (getval(fp,&prevday,false)) {
	bufautobegin(&prevtime);
	if (getval(fp,&prevtime,false)) {
	    bufautobegin(&prevzone);
	    bufscpy(&prevzone, "");
	    Igeteof(fp, c, c=0;);
	    if (c=='-' || c=='+')
		if (!get0val(c,fp,&prevzone,false))
		    c = 0;
		else
		    Igeteof(fp, c, c=0;);
	    if (c) {
		p = prevday.string;
		bufalloc(&prevdate, strlen(p) + strlen(prevtime.string) + strlen(prevzone.string) + 5);
		VOID sprintf(prevdate.string, "%s%s %s %s",
		    /* Parse dates put out by old versions of RCS.  */
		    isdigit(p[0]) && isdigit(p[1]) && p[2]=='/'  ?  "19"  :  "",
		    p, prevtime.string, prevzone.string
		);
	    }
	    bufautoend(&prevzone);
	}
	bufautoend(&prevtime);
    }
    bufautoend(&prevday);
    return c;
}

	static int
keepid(c, fp, b)
	int c;
	RILE *fp;
	struct buf *b;
/* Get previous identifier from C+FP into B.  */
{
	if (!c)
	    Igeteof(fp, c, return false;);
	if (!get0val(c, fp, b, false))
	    return false;
	checksid(b->string);
	return true;
}

	static int
keeprev(fp)
	RILE *fp;
/* Get previous revision from FP into prevrev.  */
{
	return getval(fp,&prevrev,false) && checknum(prevrev.string,-1);
}


	static int
checknum(sp,fields)
	register char const *sp;
	int fields;
{    register int dotcount;
     dotcount=0;
     while(*sp) {
        if (*sp=='.') dotcount++;
	else if (!isdigit(*sp)) return false;
        sp++;
     }
     return fields<0 ? dotcount&1 : dotcount==fields;
}



#ifdef KEEPTEST

char const cmdid[] ="keeptest";

	int
main(argc, argv)
int  argc; char  *argv[];
{
        while (*(++argv)) {
		workfilename = *argv;
		getoldkeys((RILE*)0);
                VOID printf("%s:  revision: %s, date: %s, author: %s, state: %s\n",
			    *argv, prevrev.string, prevdate.string, prevauthor.string, prevstate.string);
	}
	exitmain(EXIT_SUCCESS);
}
#endif
