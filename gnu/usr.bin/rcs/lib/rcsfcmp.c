/*
 *                     RCS file comparison
 */
/*****************************************************************************
 *                       rcsfcmp()
 *                       Testprogram: define FCMPTEST
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





/* $Log: rcsfcmp.c,v $
 * Revision 5.9  1991/10/07  17:32:46  eggert
 * Count log lines correctly.
 *
 * Revision 5.8  1991/08/19  03:13:55  eggert
 * Tune.
 *
 * Revision 5.7  1991/04/21  11:58:22  eggert
 * Fix errno bug.  Add MS-DOS support.
 *
 * Revision 5.6  1991/02/28  19:18:47  eggert
 * Open work file at most once.
 *
 * Revision 5.5  1990/11/27  09:26:05  eggert
 * Fix comment leader bug.
 *
 * Revision 5.4  1990/11/01  05:03:42  eggert
 * Permit arbitrary data in logs and comment leaders.
 *
 * Revision 5.3  1990/09/11  02:41:15  eggert
 * Don't ignore differences inside keyword strings if -ko is set.
 *
 * Revision 5.1  1990/08/29  07:13:58  eggert
 * Clean old log messages too.
 *
 * Revision 5.0  1990/08/22  08:12:49  eggert
 * Don't append "checked in with -k by " log to logs,
 * so that checking in a program with -k doesn't change it.
 * Ansify and Posixate.  Remove lint.
 *
 * Revision 4.5  89/05/01  15:12:42  narten
 * changed copyright header to reflect current distribution rules
 * 
 * Revision 4.4  88/08/09  19:12:50  eggert
 * Shrink stdio code size.
 * 
 * Revision 4.3  87/12/18  11:40:02  narten
 * lint cleanups (Guy Harris)
 * 
 * Revision 4.2  87/10/18  10:33:06  narten
 * updting version number. Changes relative to 1.1 actually relative to 
 * 4.1
 * 
 * Revision 1.2  87/03/27  14:22:19  jenkins
 * Port to suns
 * 
 * Revision 4.1  83/05/10  16:24:04  wft
 * Marker matching now uses trymatch(). Marker pattern is now
 * checked precisely.
 * 
 * Revision 3.1  82/12/04  13:21:40  wft
 * Initial revision.
 *
 */

/*
#define FCMPTEST
*/
/* Testprogram; prints out whether two files are identical,
 * except for keywords
 */

#include  "rcsbase.h"

libId(fcmpId, "$Id: rcsfcmp.c,v 5.9 1991/10/07 17:32:46 eggert Exp $")

	static int
discardkeyval(c, f)
	register int c;
	register RILE *f;
{
	for (;;)
		switch (c) {
			case KDELIM:
			case '\n':
				return c;
			default:
				Igeteof(f, c, return EOF;);
				break;
		}
}

	int
rcsfcmp(xfp, xstatp, ufname, delta)
	register RILE *xfp;
	struct stat const *xstatp;
	char const *ufname;
	struct hshentry const *delta;
/* Compare the files xfp and ufname.  Return zero
 * if xfp has the same contents as ufname and neither has keywords,
 * otherwise -1 if they are the same ignoring keyword values,
 * and 1 if they differ even ignoring
 * keyword values. For the LOG-keyword, rcsfcmp skips the log message
 * given by the parameter delta in xfp.  Thus, rcsfcmp returns nonpositive
 * if xfp contains the same as ufname, with the keywords expanded.
 * Implementation: character-by-character comparison until $ is found.
 * If a $ is found, read in the marker keywords; if they are real keywords
 * and identical, read in keyword value. If value is terminated properly,
 * disregard it and optionally skip log message; otherwise, compare value.
 */
{
    register int xc, uc;
    char xkeyword[keylength+2];
    int eqkeyvals;
    register RILE *ufp;
    register int xeof, ueof;
    register char * tp;
    register char const *sp;
    int result;
    enum markers match1;
    struct stat ustat;

    if (!(ufp = Iopen(ufname, FOPEN_R_WORK, &ustat))) {
       efaterror(ufname);
    }
    xeof = ueof = false;
    if (Expand==OLD_EXPAND) {
	if (!(result = xstatp->st_size!=ustat.st_size)) {
#	    if has_mmap && large_memory
		result = !!memcmp(xfp->base,ufp->base,(size_t)xstatp->st_size);
#	    else
		for (;;) {
		    /* get the next characters */
		    Igeteof(xfp, xc, xeof=true;);
		    Igeteof(ufp, uc, ueof=true;);
		    if (xeof | ueof)
			goto eof;
		    if (xc != uc)
			goto return1;
		}
#	    endif
	}
    } else {
	xc = 0;
	uc = 0; /* Keep lint happy.  */
	result = 0;

	for (;;) {
	  if (xc != KDELIM) {
	    /* get the next characters */
	    Igeteof(xfp, xc, xeof=true;);
	    Igeteof(ufp, uc, ueof=true;);
	    if (xeof | ueof)
		goto eof;
	  } else {
	    /* try to get both keywords */
	    tp = xkeyword;
	    for (;;) {
		Igeteof(xfp, xc, xeof=true;);
		Igeteof(ufp, uc, ueof=true;);
		if (xeof | ueof)
		    goto eof;
		if (xc != uc)
		    break;
		switch (xc) {
		    default:
			if (xkeyword+keylength <= tp)
			    break;
			*tp++ = xc;
			continue;
		    case '\n': case KDELIM: case VDELIM:
			break;
		}
		break;
	    }
	    if (
		(xc==KDELIM || xc==VDELIM)  &&  (uc==KDELIM || uc==VDELIM)  &&
		(*tp = xc,  (match1 = trymatch(xkeyword)) != Nomatch)
	    ) {
#ifdef FCMPTEST
	      VOID printf("found common keyword %s\n",xkeyword);
#endif
	      result = -1;
	      for (;;) {
		  if (xc != uc) {
		      xc = discardkeyval(xc, xfp);
		      uc = discardkeyval(uc, ufp);
		      if ((xeof = xc==EOF)  |  (ueof = uc==EOF))
			  goto eof;
		      eqkeyvals = false;
		      break;
		  }
		  switch (xc) {
		      default:
			  Igeteof(xfp, xc, xeof=true;);
			  Igeteof(ufp, uc, ueof=true;);
			  if (xeof | ueof)
			      goto eof;
			  continue;

		      case '\n': case KDELIM:
			  eqkeyvals = true;
			  break;
		  }
		  break;
	      }
	      if (xc != uc)
		  goto return1;
	      if (xc==KDELIM) {
		  /* Skip closing KDELIM.  */
		  Igeteof(xfp, xc, xeof=true;);
		  Igeteof(ufp, uc, ueof=true;);
		  if (xeof | ueof)
		      goto eof;
		  /* if the keyword is LOG, also skip the log message in xfp*/
		  if (match1==Log) {
		      /* first, compute the number of line feeds in log msg */
		      unsigned lncnt;
		      size_t ls, ccnt;
		      sp = delta->log.string;
		      ls = delta->log.size;
		      if (ls<sizeof(ciklog)-1 || memcmp(sp,ciklog,sizeof(ciklog)-1)) {
			/* This log message was inserted.  */
			lncnt = 3;
			while (ls--) if (*sp++=='\n') lncnt++;
			for (;;) {
			    if (xc=='\n')
				if(--lncnt==0) break;
			    Igeteof(xfp, xc, goto returnresult;);
			}
			/* skip last comment leader */
			/* Can't just skip another line here, because there may be */
			/* additional characters on the line (after the Log....$)  */
			for (ccnt=Comment.size; ccnt--; ) {
			    Igeteof(xfp, xc, goto returnresult;);
			    if(xc=='\n') break;
			    /*
			     * Read to the end of the comment leader or '\n',
			     * whatever comes first.  Some editors strip
			     * trailing white space from a leader like " * ".
			     */
			}
		      }
		  }
	      } else {
		  /* both end in the same character, but not a KDELIM */
		  /* must compare string values.*/
#ifdef FCMPTEST
		  VOID printf("non-terminated keywords %s, potentially different values\n",xkeyword);
#endif
		  if (!eqkeyvals)
		      goto return1;
	      }
	    }
	  }
	  if (xc != uc)
	      goto return1;
	}
    }

  eof:
    if (xeof==ueof)
	goto returnresult;
  return1:
    result = 1;
  returnresult:
    Ifclose(ufp);
    return result;
}



#ifdef FCMPTEST

char const cmdid[] = "rcsfcmp";

main(argc, argv)
int  argc; char  *argv[];
/* first argument: comment leader; 2nd: log message, 3rd: expanded file,
 * 4th: unexpanded file
 */
{       struct hshentry delta;

	Comment.string = argv[1];
	Comment.size = strlen(argv[1]);
	delta.log.string = argv[2];
	delta.log.size = strlen(argv[2]);
	if (rcsfcmp(Iopen(argv[3], FOPEN_R_WORK, (struct stat*)0), argv[4], &delta))
                VOID printf("files are the same\n");
        else    VOID printf("files are different\n");
}
#endif
