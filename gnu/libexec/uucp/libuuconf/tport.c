/* tport.c
   Find a port in the Taylor UUCP configuration files.

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
const char _uuconf_tport_rcsid[] = "$FreeBSD$";
#endif

#include <errno.h>

static int ipport P((pointer pglobal, int argc, char **argv, pointer pvar,
		     pointer pinfo));
static int ipunknown P((pointer pglobal, int argc, char **argv,
			pointer pvar, pointer pinfo));

/* Find a port in the Taylor UUCP configuration files by name, baud
   rate, and special purpose function.  */

int
uuconf_taylor_find_port (pglobal, zname, ibaud, ihighbaud, pifn, pinfo,
			 qport)
     pointer pglobal;
     const char *zname;
     long ibaud;
     long ihighbaud;
     int (*pifn) P((struct uuconf_port *, pointer));
     pointer pinfo;
     struct uuconf_port *qport;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  FILE *e;
  pointer pblock;
  char *zfree;
  int iret;
  char **pz;

  if (ihighbaud == 0L)
    ihighbaud = ibaud;

  e = NULL;
  pblock = NULL;
  zfree = NULL;
  iret = UUCONF_NOT_FOUND;

  for (pz = qglobal->qprocess->pzportfiles; *pz != NULL; pz++)
    {
      struct uuconf_cmdtab as[2];
      char *zport;
      struct uuconf_port sdefault;
      int ilineno;

      e = fopen (*pz, "r");
      if (e == NULL)
	{
	  if (FNO_SUCH_FILE ())
	    continue;
	  qglobal->ierrno = errno;
	  iret = UUCONF_FOPEN_FAILED | UUCONF_ERROR_ERRNO;
	  break;
	}

      qglobal->ilineno = 0;

      /* Gather the default information from the top of the file.  We
	 do this by handling the "port" command ourselves and passing
	 every other command to _uuconf_iport_cmd via ipunknown.  The
	 value of zport will be an malloc block.  */
      as[0].uuconf_zcmd = "port";
      as[0].uuconf_itype = UUCONF_CMDTABTYPE_FN | 2;
      as[0].uuconf_pvar = (pointer) &zport;
      as[0].uuconf_pifn = ipport;

      as[1].uuconf_zcmd = NULL;

      pblock = uuconf_malloc_block ();
      if (pblock == NULL)
	{
	  qglobal->ierrno = errno;
	  iret = UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
	  break;
	}

      _uuconf_uclear_port (&sdefault);
      sdefault.uuconf_palloc = pblock;
      zport = NULL;
      iret = uuconf_cmd_file (pglobal, e, as, (pointer) &sdefault,
			      ipunknown, UUCONF_CMDTABFLAG_BACKSLASH,
			      pblock);
      if (iret != UUCONF_SUCCESS)
	{
	  zfree = zport;
	  break;
	}

      /* Now skip until we find a port with a matching name.  If the
	 zname argument is NULL, we will have to read every port.  */
      iret = UUCONF_NOT_FOUND;
      while (zport != NULL)
	{
	  uuconf_cmdtabfn piunknown;
	  boolean fmatch;

	  if (zname == NULL || strcmp (zname, zport) == 0)
	    {
	      piunknown = ipunknown;
	      *qport = sdefault;
	      qport->uuconf_zname = zport;
	      zfree = zport;
	      fmatch = TRUE;
	    }
	  else
	    {
	      piunknown = NULL;
	      free ((pointer) zport);
	      fmatch = FALSE;
	    }

	  zport = NULL;
	  ilineno = qglobal->ilineno;
	  iret = uuconf_cmd_file (pglobal, e, as, (pointer) qport,
				  piunknown, UUCONF_CMDTABFLAG_BACKSLASH,
				  pblock);
	  qglobal->ilineno += ilineno;
	  if (iret != UUCONF_SUCCESS)
	    break;
	  iret = UUCONF_NOT_FOUND;

	  /* We may have just gathered information about a port.  See
	     if it matches the name, the baud rate and the special
	     function.  */
	  if (fmatch)
	    {
	      if (ibaud != 0)
		{
		  if (qport->uuconf_ttype == UUCONF_PORTTYPE_MODEM)
		    {
		      long imbaud, imhigh, imlow;

		      imbaud = qport->uuconf_u.uuconf_smodem.uuconf_ibaud;
		      imhigh = qport->uuconf_u.uuconf_smodem.uuconf_ihighbaud;
		      imlow = qport->uuconf_u.uuconf_smodem.uuconf_ilowbaud;

		      if (imbaud == 0 && imlow == 0)
			;
		      else if (ibaud <= imbaud && imbaud <= ihighbaud)
			;
		      else if (imlow != 0
			       && imlow <= ihighbaud
			       && imhigh >= ibaud)
			;
		      else
			fmatch = FALSE;
		    }
		  else if (qport->uuconf_ttype == UUCONF_PORTTYPE_DIRECT)
		    {
		      long idbaud;

		      idbaud = qport->uuconf_u.uuconf_sdirect.uuconf_ibaud;
		      if (idbaud != 0 && idbaud != ibaud)
			fmatch = FALSE;
		    }
		}
	    }

	  if (fmatch)
	    {
	      if (pifn != NULL)
		{
		  iret = (*pifn) (qport, pinfo);
		  if (iret == UUCONF_NOT_FOUND)
		    fmatch = FALSE;
		  else if (iret != UUCONF_SUCCESS)
		    break;
		}
	    }

	  if (fmatch)
	    {
	      if (uuconf_add_block (pblock, zfree) == 0)
		{
		  zfree = NULL;
		  iret = UUCONF_SUCCESS;
		}
	      else
		{
		  qglobal->ierrno = errno;
		  iret = UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
		}
	      break;
	    }

	  if (zfree != NULL)
	    {
	      free ((pointer) zfree);
	      zfree = NULL;
	    }
	}

      (void) fclose (e);
      e = NULL;

      if (iret != UUCONF_NOT_FOUND)
	break;

      uuconf_free_block (pblock);
      pblock = NULL;
    }

  if (e != NULL)
    (void) fclose (e);
  if (zfree != NULL)
    free ((pointer) zfree);
  if (iret != UUCONF_SUCCESS && pblock != NULL)
    uuconf_free_block (pblock);

  if (iret != UUCONF_SUCCESS && iret != UUCONF_NOT_FOUND)
    {
      qglobal->zfilename = *pz;
      iret |= UUCONF_ERROR_FILENAME;
    }

  return iret;
}

/* Handle a "port" command.  This copies the string onto the heap and
   returns the pointer in *pvar.  It returns UUCONF_CMDTABRET_EXIT to
   force uuconf_cmd_file to stop reading and return to the code above,
   which will then check the port just read to see if it matches.  */

/*ARGSUSED*/
static int
ipport (pglobal, argc, argv, pvar, pinfo)
     pointer pglobal;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  char **pz = (char **) pvar;
  size_t csize;

  csize = strlen (argv[1]) + 1;
  *pz = malloc (csize);
  if (*pz == NULL)
    {
      qglobal->ierrno = errno;
      return (UUCONF_MALLOC_FAILED
	      | UUCONF_ERROR_ERRNO
	      | UUCONF_CMDTABRET_EXIT);
    }
  memcpy ((pointer) *pz, (pointer) argv[1], csize);
  return UUCONF_CMDTABRET_EXIT;
}

/* Handle an unknown command by passing it on to _uuconf_iport_cmd,
   which will parse it into the port structure. */

/*ARGSUSED*/
static int
ipunknown (pglobal, argc, argv, pvar, pinfo)
     pointer pglobal;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  struct uuconf_port *qport = (struct uuconf_port *) pinfo;
  int iret;

  iret = _uuconf_iport_cmd (qglobal, argc, argv, qport);
  if (UUCONF_ERROR_VALUE (iret) != UUCONF_SUCCESS)
    iret |= UUCONF_CMDTABRET_EXIT;
  return iret;
}
