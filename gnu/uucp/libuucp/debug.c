/* debug.c
   UUCP debugging functions.

   Copyright (C) 1991, 1992 Ian Lance Taylor

   This file is part of the Taylor UUCP package.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Infinity Development Systems, P.O. Box 520, Waltham, MA 02254.
   */

#include "uucp.h"

#include <ctype.h>

#include "uudefs.h"

/* The debugging level.  */
int iDebug;

/* Parse a debugging string.  This may be a simple number, which sets
   the given number of bits in iDebug, or it may be a series of single
   letters.  */

static const char * const azDebug_names[] = DEBUG_NAMES;

int
idebug_parse (z)
     const char *z;
{
  char *zend;
  int i, iret;
  char *zcopy, *ztok;

  if (strncasecmp (z, DEBUG_NONE, sizeof DEBUG_NONE - 1) == 0)
    return 0;

  i = (int) strtol ((char *) z, &zend, 0);
  if (*zend == '\0')
    {
      if (i > 15)
	i = 15;
      else if (i < 0)
	i = 0;
      return (1 << i) - 1;
    }

  zcopy = zbufcpy (z);

  iret = 0;

  for (ztok = strtok (zcopy, ",");
       ztok != NULL;
       ztok = strtok ((char *) NULL, ","))
    {
      if (strcasecmp (ztok, "all") == 0)
	{
	  iret = DEBUG_MAX;
	  break;
	}
      for (i = 0; azDebug_names[i] != NULL; i++)
	{
	  if (strncasecmp (ztok, azDebug_names[i],
			   strlen (azDebug_names[i])) == 0)
	    {
	      iret |= 1 << i;
	      break;
	    }
	}
      if (azDebug_names[i] == NULL)
	ulog (LOG_ERROR, "Unrecognized debugging option \"%s\"",
	      ztok);
    }

  ubuffree (zcopy);

  return iret;
}

/* A debugging routine used when displaying buffers.  */

size_t
cdebug_char (z, ichar)
     char *z;
     int ichar;
{
  char b;

  if (isprint (BUCHAR (ichar))
      && ichar != '\"'
      && ichar != '\\')
    {
      *z++ = (char) ichar;
      *z = '\0';
      return 1;
    }

  *z++ = '\\';

  switch (ichar)
    {
    case '\n':
      b = 'n';
      break;
    case '\r':
      b = 'r';
      break;
    case '\"':
      b = '\"';
      break;
    case '\\':
      b = '\\';
      break;
    default:
      sprintf (z, "%03o", (unsigned int) BUCHAR (ichar));
      return strlen (z) + 1;
    }

  *z++ = b;
  *z = '\0';
  return 2;
}      

/* Display a buffer when debugging.  */

void
udebug_buffer (zhdr, zbuf, clen)
     const char *zhdr;
     const char *zbuf;
     size_t clen;
{
  char *z, *zalc;
  int i;

  zalc = zbufalc (clen * 4 + 1);

  z = zalc;
  for (i = 0; i < clen && i < 80; i++)
    z += cdebug_char (z, zbuf[i]);
  if (i < clen)
    {
      *z++ = '.';
      *z++ = '.';
      *z++ = '.';
    }
  *z = '\0';
  
  ulog (LOG_DEBUG, "%s %lu \"%s\"", zhdr, (unsigned long) clen, zalc);

  ubuffree (zalc);
}
