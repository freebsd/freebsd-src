/* time.c
   Parse a time string into a uuconf_timespan structure.

   Copyright (C) 1992, 1993 Ian Lance Taylor

   This file is part of the Taylor UUCP uuconf library.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License
   as published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Cygnus Support, 48 Grove Street, Somerville, MA 02144.
   */

#include "uucnfi.h"

#if USE_RCS_ID
const char _uuconf_time_rcsid[] = "$FreeBSD: src/gnu/libexec/uucp/libuuconf/time.c,v 1.6 1999/08/27 23:33:33 peter Exp $";
#endif

#include <ctype.h>
#include <errno.h>

static int itadd_span P((struct sglobal *qglobal, int istart, int iend,
			 long ival, int cretry,
			 int (*picmp) P((long, long)),
			 struct uuconf_timespan **pqspan,
			 pointer pblock));
static int itnew P((struct sglobal *qglobal, struct uuconf_timespan **pqset,
		    struct uuconf_timespan *qnext, int istart, int iend,
		    long ival, int cretry, pointer pblock));

/* An array of weekday abbreviations.  The code below assumes that
   each one starts with a lower case letter.  */

static const struct
{
  const char *zname;
  int imin;
  int imax;
} asTdays[] =
{
  { "any", 0, 6 },
  { "wk", 1, 5 },
  { "su", 0, 0 },
  { "mo", 1, 1 },
  { "tu", 2, 2 },
  { "we", 3, 3 },
  { "th", 4, 4 },
  { "fr", 5, 5 },
  { "sa", 6, 6 },
  { "never", -1, -2 },
  { "none", -1, -2 },
  { NULL, 0, 0 }
};

/* Parse a time string and add it to a span list.  This function is
   given the value, the retry time, and the comparison function to
   use.  */

int
_uuconf_itime_parse (qglobal, ztime, ival, cretry, picmp, pqspan, pblock)
     struct sglobal *qglobal;
     char *ztime;
     long ival;
     int cretry;
     int (*picmp) P((long, long));
     struct uuconf_timespan **pqspan;
     pointer pblock;
{
  struct uuconf_timespan *qlist;
  char bfirst;
  const char *z;

  qlist = *pqspan;
  if (qlist == (struct uuconf_timespan *) &_uuconf_unset)
    qlist = NULL;

  /* Expand the string using a timetable.  Keep rechecking the string
     until it does not match.  */
  while (TRUE)
    {
      char **pz;
      char *zfound;

      bfirst = *ztime;
      if (isupper (BUCHAR (bfirst)))
	bfirst = tolower (BUCHAR (bfirst));

      zfound = NULL;
      pz = qglobal->qprocess->pztimetables;

      /* We want the last timetable to have been defined with this
	 name, so we always look through the entire table.  */
      while (*pz != NULL)
	{
	  if ((bfirst == (*pz)[0]
	       || (isupper (BUCHAR ((*pz)[0]))
		   && (int) bfirst == (int) tolower (BUCHAR ((*pz)[0]))))
	      && strcasecmp (ztime, *pz) == 0)
	    zfound = pz[1];
	  pz += 2;
	}
      if (zfound == NULL)
	break;
      ztime = zfound;
    }

  /* Look through the time string.  */
  z = ztime;
  while (*z != '\0')
    {
      int iday;
      boolean afday[7];
      int istart, iend;

      if (*z == ',' || *z == '|')
	++z;
      if (*z == '\0' || *z == ';')
	break;

      for (iday = 0; iday < 7; iday++)
	afday[iday] = FALSE;

      /* Get the days.  */
      do
	{
	  bfirst = *z;
	  if (isupper (BUCHAR (bfirst)))
	    bfirst = tolower (BUCHAR (bfirst));
	  for (iday = 0; asTdays[iday].zname != NULL; iday++)
	    {
	      size_t clen;

	      if (bfirst != asTdays[iday].zname[0])
		continue;

	      clen = strlen (asTdays[iday].zname);
	      if (strncasecmp (z, asTdays[iday].zname, clen) == 0)
		{
		  int iset;

		  for (iset = asTdays[iday].imin;
		       iset <= asTdays[iday].imax;
		       iset++)
		    afday[iset] = TRUE;
		  z += clen;
		  break;
		}
	    }
	  if (asTdays[iday].zname == NULL)
	    return UUCONF_SYNTAX_ERROR;
	}
      while (isalpha (BUCHAR (*z)));

      /* Get the hours.  */
      if (! isdigit (BUCHAR (*z)))
	{
	  istart = 0;
	  iend = 24 * 60;
	}
      else
	{
	  char *zendnum;

	  istart = (int) strtol ((char *) z, &zendnum, 10);
	  if (*zendnum != '-' || ! isdigit (BUCHAR (zendnum[1])))
	    return UUCONF_SYNTAX_ERROR;
	  z = zendnum + 1;
	  iend = (int) strtol ((char *) z, &zendnum, 10);
	  z = zendnum;

	  istart = (istart / 100) * 60 + istart % 100;
	  iend = (iend / 100) * 60 + iend % 100;
	}

      /* Add the times we've found onto the list.  */
      for (iday = 0; iday < 7; iday++)
	{
	  if (afday[iday])
	    {
	      int iminute, iret;

	      iminute = iday * 24 * 60;
	      if (istart < iend)
		iret = itadd_span (qglobal, iminute + istart,
				   iminute + iend, ival, cretry, picmp,
				   &qlist, pblock);
	      else
		{
		  /* Wrap around midnight.  */
		  iret = itadd_span (qglobal, iminute, iminute + iend,
				     ival, cretry, picmp, &qlist, pblock);
		  if (iret == UUCONF_SUCCESS)
		    iret = itadd_span (qglobal, iminute + istart,
				       iminute + 24 * 60, ival, cretry,
				       picmp, &qlist, pblock);
		}

	      if (iret != UUCONF_SUCCESS)
		return iret;
	    }
	}
    }

  *pqspan = qlist;

  return UUCONF_SUCCESS;
}

/* Add a time span to an existing list of time spans.  We keep the
   list sorted by time to make this operation easier.  This modifies
   the existing list, and returns the modified version.  It takes a
   comparison function which should return < 0 if the first argument
   should take precedence over the second argument and == 0 if they
   are the same (for grades this is igradecmp; for sizes it is minus
   (the binary operator)).  */

static int
itadd_span (qglobal, istart, iend, ival, cretry, picmp, pqspan, pblock)
     struct sglobal *qglobal;
     int istart;
     int iend;
     long ival;
     int cretry;
     int (*picmp) P((long, long));
     struct uuconf_timespan **pqspan;
     pointer pblock;
{
  struct uuconf_timespan **pq;
  int iret;

  /* istart < iend  */
  for (pq = pqspan; *pq != NULL; pq = &(*pq)->uuconf_qnext)
    {
      int icmp;

      /* Invariant: PREV (*pq) == NULL || PREV (*pq)->iend <= istart  */
      /* istart < iend && (*pq)->istart < (*pq)->iend  */

      if (iend <= (*pq)->uuconf_istart)
	{
	  /* istart < iend <= (*pq)->istart < (*pq)->iend  */
	  /* No overlap, and we're at the right spot.  See if we can
	     combine these spans.  */
	  if (iend == (*pq)->uuconf_istart
	      && cretry == (*pq)->uuconf_cretry
	      && (*picmp) (ival, (*pq)->uuconf_ival) == 0)
	    {
	      (*pq)->uuconf_istart = istart;
	      return UUCONF_SUCCESS;
	    }
	  /* We couldn't combine them.  */
	  break;
	}

      if ((*pq)->uuconf_iend <= istart)
	{
	  /* (*pq)->istart < (*pq)->iend <= istart < iend  */
	  /* No overlap.  Try attaching this span.  */
	  if ((*pq)->uuconf_iend == istart
	      && (*pq)->uuconf_cretry == cretry
	      && ((*pq)->uuconf_qnext == NULL
		  || iend <= (*pq)->uuconf_qnext->uuconf_istart)
	      && (*picmp) (ival, (*pq)->uuconf_ival) == 0)
	    {
	      (*pq)->uuconf_iend = iend;
	      return UUCONF_SUCCESS;
	    }
	  /* Couldn't attach; keep looking for the right spot.  We
	     might be able to combine part of the new span onto an
	     existing span, but it's probably not worth it.  */
	  continue;
	}

      /* istart < iend
	 && (*pq)->istart < (*pq)->iend
	 && istart < (*pq)->iend
	 && (*pq)->istart < iend  */
      /* Overlap.  */

      icmp = (*picmp) (ival, (*pq)->uuconf_ival);

      if (icmp == 0)
	{
	  /* Just expand the old span to include the new span.  */
	  if (istart < (*pq)->uuconf_istart)
	    (*pq)->uuconf_istart = istart;
	  if ((*pq)->uuconf_iend >= iend)
	    return UUCONF_SUCCESS;
	  if ((*pq)->uuconf_qnext == NULL
	      || iend <= (*pq)->uuconf_qnext->uuconf_istart)
	    {
	      (*pq)->uuconf_iend = iend;
	      return UUCONF_SUCCESS;
	    }
	  /* The span we are adding overlaps the next span as well.
	     Expand the old span up to the next old span, and keep
	     trying to add the new span.  */
	  (*pq)->uuconf_iend = (*pq)->uuconf_qnext->uuconf_istart;
	  istart = (*pq)->uuconf_iend;
	}
      else if (icmp < 0)
	{
	  /* Replace the old span with the new span.  */
	  if ((*pq)->uuconf_istart < istart)
	    {
	      /* Save the initial portion of the old span.  */
	      iret = itnew (qglobal, pq, *pq, (*pq)->uuconf_istart, istart,
			    (*pq)->uuconf_ival, (*pq)->uuconf_cretry,
			    pblock);
	      if (iret != UUCONF_SUCCESS)
		return iret;
	      pq = &(*pq)->uuconf_qnext;
	    }
	  if (iend < (*pq)->uuconf_iend)
	    {
	      /* Save the final portion of the old span.  */
	      iret = itnew (qglobal, &(*pq)->uuconf_qnext,
			    (*pq)->uuconf_qnext, iend, (*pq)->uuconf_iend,
			    (*pq)->uuconf_ival, (*pq)->uuconf_cretry,
			    pblock);
	      if (iret != UUCONF_SUCCESS)
		return iret;
	    }
	  (*pq)->uuconf_ival = ival;
	  (*pq)->uuconf_istart = istart;
	  (*pq)->uuconf_cretry = cretry;
	  if ((*pq)->uuconf_qnext == NULL
	      || iend <= (*pq)->uuconf_qnext->uuconf_istart)
	    {
	      (*pq)->uuconf_iend = iend;
	      return UUCONF_SUCCESS;
	    }
	  /* Move this span up to the next one, and keep trying to add
	     the new span.  */
	  (*pq)->uuconf_iend = (*pq)->uuconf_qnext->uuconf_istart;
	  istart = (*pq)->uuconf_iend;
	}
      else
	{
	  /* Leave the old span untouched.  */
	  if (istart < (*pq)->uuconf_istart)
	    {
	      /* Put in the initial portion of the new span.  */
	      iret = itnew (qglobal, pq, *pq, istart, (*pq)->uuconf_istart,
			    ival, cretry, pblock);
	      if (iret != UUCONF_SUCCESS)
		return iret;
	      pq = &(*pq)->uuconf_qnext;
	    }
	  if (iend <= (*pq)->uuconf_iend)
	    return UUCONF_SUCCESS;
	  /* Keep trying to add the new span.  */
	  istart = (*pq)->uuconf_iend;
	}
    }

  /* This is the spot for the new span, and there's no overlap.  */

  return itnew (qglobal, pq, *pq, istart, iend, ival, cretry, pblock);
}

/* A utility function to create a new uuconf_timespan structure.  */

static int
itnew (qglobal, pqset, qnext, istart, iend, ival, cretry, pblock)
     struct sglobal *qglobal;
     struct uuconf_timespan **pqset;
     struct uuconf_timespan *qnext;
     int istart;
     int iend;
     long ival;
     int cretry;
     pointer pblock;
{
  register struct uuconf_timespan *qnew;

  qnew = ((struct uuconf_timespan *)
	  uuconf_malloc (pblock, sizeof (struct uuconf_timespan)));
  if (qnew == NULL)
    {
      qglobal->ierrno = errno;
      return UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
    }

  qnew->uuconf_qnext = qnext;
  qnew->uuconf_istart = istart;
  qnew->uuconf_iend = iend;
  qnew->uuconf_ival = ival;
  qnew->uuconf_cretry = cretry;

  *pqset = qnew;

  return UUCONF_SUCCESS;
}
