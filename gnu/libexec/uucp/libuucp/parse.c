/* parse.c
   Parse a UUCP command string.

   Copyright (C) 1991, 1992, 1993 Ian Lance Taylor

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Cygnus Support, 48 Grove Street, Somerville, MA 02144.
   */

#include "uucp.h"

#if USE_RCS_ID
const char parse_rcsid[] = "$FreeBSD$";
#endif

#include "uudefs.h"

/* Parse a UUCP command string into an scmd structure.  This is called
   by the 'g' protocol and the UNIX command file reading routines.  It
   destroys the string it is passed, and the scmd string pointers are
   left pointing into it.  For the convenience of the Unix work file
   routines, it will parse "P" into a simple 'P' command (representing
   a poll file).  It returns TRUE if the string is successfully
   parsed, FALSE otherwise.  */

boolean
fparse_cmd (zcmd, qcmd)
     char *zcmd;
     struct scmd *qcmd;
{
  char *z, *zend;

  z = strtok (zcmd, " \t\n");
  if (z == NULL)
    return FALSE;

  qcmd->bcmd = *z;
  if (qcmd->bcmd != 'S'
      && qcmd->bcmd != 'R'
      && qcmd->bcmd != 'X'
      && qcmd->bcmd != 'E'
      && qcmd->bcmd != 'H'
      && qcmd->bcmd != 'P')
    return FALSE;

  qcmd->bgrade = '\0';
  qcmd->pseq = NULL;
  qcmd->zfrom = NULL;
  qcmd->zto = NULL;
  qcmd->zuser = NULL;
  qcmd->zoptions = NULL;
  qcmd->ztemp = NULL;
  qcmd->imode = 0666;
  qcmd->znotify = NULL;
  qcmd->cbytes = -1;
  qcmd->zcmd = NULL;
  qcmd->ipos = 0;

  /* Handle hangup commands specially.  If it's just "H", return
     the command 'H' to indicate a hangup request.  If it's "HY"
     return 'Y' and if it's "HN" return 'N'.  */
  if (qcmd->bcmd == 'H')
    {
      if (z[1] != '\0')
	{
	  if (z[1] == 'Y')
	    qcmd->bcmd = 'Y';
	  else if (z[1] == 'N')
	    qcmd->bcmd = 'N';
	  else
	    return FALSE;
	}

      return TRUE;
    }
  if (qcmd->bcmd == 'P')
    return TRUE;

  if (z[1] != '\0')
    return FALSE;

  z = strtok ((char *) NULL, " \t\n");
  if (z == NULL)
    return FALSE;
  qcmd->zfrom = z;

  z = strtok ((char *) NULL, " \t\n");
  if (z == NULL)
    return FALSE;
  qcmd->zto = z;
      
  z = strtok ((char *) NULL, " \t\n");
  if (z == NULL)
    return FALSE;
  qcmd->zuser = z;

  z = strtok ((char *) NULL, " \t\n");
  if (z == NULL || *z != '-')
    return FALSE;
  qcmd->zoptions = z + 1;

  if (qcmd->bcmd == 'X')
    return TRUE;

  if (qcmd->bcmd == 'R')
    {
      z = strtok ((char *) NULL, " \t\n");
      if (z != NULL)
	{
	  if (strcmp (z, "dummy") != 0)
	    {
	      /* This may be the maximum number of bytes the remote
		 system wants to receive, if it using Taylor UUCP size
		 negotiation.  */
	      qcmd->cbytes = strtol (z, &zend, 0);
	      if (*zend != '\0')
		qcmd->cbytes = -1;
	    }
	  else
	    {
	      /* This is from an SVR4 system, and may include the
		 position at which to start sending the file.  The
		 next fields are the mode bits, the remote owner (?),
		 the remote temporary file name, and finally the
		 restart position.  */
	      if (strtok ((char *) NULL, " \t\n") != NULL
		  && strtok ((char *) NULL, " \t\n") != NULL
		  && strtok ((char *) NULL, " \t\n") != NULL)
		{
		  z = strtok ((char *) NULL, " \t\n");
		  if (z != NULL)
		    {
		      qcmd->ipos = strtol (z, &zend, 0);
		      if (*zend != '\0')
			qcmd->ipos = 0;
		    }
		}
	    }
	}

      return TRUE;
    }      

  z = strtok ((char *) NULL, " \t\n");
  if (z == NULL)
    return FALSE;
  qcmd->ztemp = z;

  z = strtok ((char *) NULL, " \t\n");
  if (z == NULL)
    return FALSE;
  qcmd->imode = (int) strtol (z, &zend, 0);
  if (*zend != '\0')
    return FALSE;

  /* As a magic special case, if the mode came out as the decimal
     values 666 or 777, assume that they actually meant the octal
     values.  Most systems use a leading zero, but a few do not.
     Since both 666 and 777 are greater than the largest legal mode
     value, which is 0777 == 511, this hack does not restrict any
     legal values.  */
  if (qcmd->imode == 666)
    qcmd->imode = 0666;
  else if (qcmd->imode == 777)
    qcmd->imode = 0777;

  z = strtok ((char *) NULL, " \t\n");
  if (qcmd->bcmd == 'E' && z == NULL)
    return FALSE;
  qcmd->znotify = z;

  /* SVR4 UUCP will send the string "dummy" after the notify string
     but before the size.  I do not know when it sends anything other
     than "dummy".  Fortunately, it doesn't really hurt to not get the
     file size.  */
  if (z != NULL && strcmp (z, "dummy") == 0)
    z = strtok ((char *) NULL, " \t\n");

  if (z != NULL)
    {
      z = strtok ((char *) NULL, " \t\n");
      if (z != NULL)
	{
	  qcmd->cbytes = strtol (z, &zend, 0);
	  if (*zend != '\0')
	    qcmd->cbytes = -1;
	}
      else if (qcmd->bcmd == 'E')
	return FALSE;
      
      if (z != NULL)
	{
	  z = strtok ((char *) NULL, "");
	  if (z != NULL)
	    z[strcspn (z, "\n")] = '\0';
	  if (qcmd->bcmd == 'E' && z == NULL)
	    return FALSE;
	  qcmd->zcmd = z;
	}
    }

  return TRUE;
}
