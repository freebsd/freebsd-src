/* hsinfo.c
   Get information about a system from the HDB configuration files.

   Copyright (C) 1992, 1993, 1995 Ian Lance Taylor

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
const char _uuconf_hsinfo_rcsid[] = "$FreeBSD: src/gnu/libexec/uucp/libuuconf/hsinfo.c,v 1.7 1999/08/27 23:33:22 peter Exp $";
#endif

#include <errno.h>
#include <ctype.h>

static int ihadd_machine_perm P((struct sglobal *qglobal,
				 struct uuconf_system *qsys,
				 struct shpermissions *qperm));
static int ihadd_logname_perm P((struct sglobal *qglobal,
				 struct uuconf_system *qsys,
				 struct shpermissions *qperm));

/* Get the information for a particular system from the HDB
   configuration files.  This does not make sure that all the default
   values are set.  */

int
_uuconf_ihdb_system_internal (qglobal, zsystem, qsys)
     struct sglobal *qglobal;
     const char *zsystem;
     struct uuconf_system *qsys;
{
  int iret;
  struct shpermissions *qperm;
  char *zline;
  size_t cline;
  char **pzsplit;
  size_t csplit;
  char **pzcomma;
  size_t ccomma;
  pointer pblock;
  char **pz;
  boolean ffound_machine, ffound_login;
  struct shpermissions *qother_machine;
  struct uuconf_system *qalt;

  if (! qglobal->qprocess->fhdb_read_permissions)
    {
      iret = _uuconf_ihread_permissions (qglobal);
      if (iret != UUCONF_SUCCESS)
	return iret;
    }

  /* First look through the Permissions information to see if this is
     an alias for some system.  I assume that an alias applies to the
     first name in the corresponding MACHINE entry.  */

  for (qperm = qglobal->qprocess->qhdb_permissions;
       qperm != NULL;
       qperm = qperm->qnext)
    {
      if (qperm->pzalias == NULL
	  || qperm->pzmachine == NULL
	  || qperm->pzalias == (char **) &_uuconf_unset
	  || qperm->pzmachine == (char **) &_uuconf_unset)
	continue;

      for (pz = qperm->pzalias; *pz != NULL; pz++)
	{
	  if (strcmp (*pz, zsystem) == 0)
	    {
	      zsystem = qperm->pzmachine[0];
	      break;
	    }
	}
      if (*pz != NULL)
	break;
    }

  zline = NULL;
  cline = 0;
  pzsplit = NULL;
  csplit = 0;
  pzcomma = NULL;
  ccomma = 0;

  pblock = NULL;

  iret = UUCONF_SUCCESS;

  for (pz = qglobal->qprocess->pzhdb_systems; *pz != NULL; pz++)
    {
      FILE *e;
      int cchars;

      qglobal->ilineno = 0;

      e = fopen (*pz, "r");
      if (e == NULL)
	{
	  if (FNO_SUCH_FILE ())
	    continue;
	  qglobal->ierrno = errno;
	  iret = UUCONF_FOPEN_FAILED | UUCONF_ERROR_ERRNO;
	  break;
	}

      while ((cchars = _uuconf_getline (qglobal, &zline, &cline, e)) > 0)
	{
	  int ctoks, ctimes, i;
	  struct uuconf_system *qset;
	  char *z, *zretry;
	  int cretry;

	  ++qglobal->ilineno;

	  --cchars;
	  if (zline[cchars] == '\n')
	    zline[cchars] = '\0';
	  if (isspace (BUCHAR (zline[0])) || zline[0] == '#')
	    continue;

	  ctoks = _uuconf_istrsplit (zline, '\0', &pzsplit, &csplit);
	  if (ctoks < 0)
	    {
	      qglobal->ierrno = errno;
	      iret = UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
	      break;
	    }

	  /* If this isn't the system we're looking for, keep reading
	     the file.  */
	  if (ctoks < 1
	      || strcmp (zsystem, pzsplit[0]) != 0)
	    continue;

	  /* If this is the first time we've found the system, we want
	     to set *qsys directly.  Otherwise, we allocate a new
	     alternate.  */
	  if (pblock == NULL)
	    {
	      pblock = uuconf_malloc_block ();
	      if (pblock == NULL)
		{
		  qglobal->ierrno = errno;
		  iret = UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
		  break;
		}
	      _uuconf_uclear_system (qsys);
	      qsys->uuconf_palloc = pblock;
	      qset = qsys;
	    }
	  else
	    {
	      struct uuconf_system **pq;

	      qset = ((struct uuconf_system *)
		      uuconf_malloc (pblock, sizeof (struct uuconf_system)));
	      if (qset == NULL)
		{
		  qglobal->ierrno = errno;
		  iret = UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
		  break;
		}
	      _uuconf_uclear_system (qset);
	      for (pq = &qsys->uuconf_qalternate;
		   *pq != NULL;
		   pq = &(*pq)->uuconf_qalternate)
		;
	      *pq = qset;
	    }

	  /* Add this line to the memory block we are building for the
	     system.  */
	  if (uuconf_add_block (pblock, zline) != 0)
	    {
	      qglobal->ierrno = errno;
	      iret = UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
	      break;
	    }

	  zline = NULL;
	  cline = 0;

	  /* The format of a line in Systems is
	     system time device speed phone chat
	     For example,
	     airs Any ACU 9600 5551212 ogin: foo pass: bar
	     */

	  /* Get the system name.  */

	  qset->uuconf_zname = pzsplit[0];
	  qset->uuconf_fcall = TRUE;
	  qset->uuconf_fcalled = FALSE;

	  if (ctoks < 2)
	    continue;

	  /* A time string is "time/grade,time/grade;retry".  A
	     missing grade is taken as BGRADE_LOW.  */
	  zretry = strchr (pzsplit[1], ';');
	  if (zretry == NULL)
	    cretry = 0;
	  else
	    {
	      *zretry = '\0';
	      cretry = (int) strtol (zretry + 1, (char **) NULL, 10);
	    }

	  ctimes = _uuconf_istrsplit (pzsplit[1], ',', &pzcomma, &ccomma);
	  if (ctimes < 0)
	    {
	      qglobal->ierrno = errno;
	      iret = UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
	      break;
	    }

	  for (i = 0; i < ctimes; i++)
	    {
	      char *zslash;
	      char bgrade;

	      z = pzcomma[i];
	      zslash = strchr (z, '/');
	      if (zslash == NULL)
		bgrade = UUCONF_GRADE_LOW;
	      else
		{
		  *zslash = '\0';
		  bgrade = zslash[1];
		  if (! UUCONF_GRADE_LEGAL (bgrade))
		    bgrade = UUCONF_GRADE_LOW;
		}

	      iret = _uuconf_itime_parse (qglobal, z, (long) bgrade,
					  cretry, _uuconf_itime_grade_cmp,
					  &qset->uuconf_qtimegrade,
					  pblock);

	      /* We treat a syntax error in the time field as
                 equivalent to ``never'', on the assumption that that
                 is what HDB does.  */
	      if (iret == UUCONF_SYNTAX_ERROR)
		iret = UUCONF_SUCCESS;

	      if (iret != UUCONF_SUCCESS)
		break;

	      /* Treat any time/grade setting as both a timegrade and
		 a call-timegrade.  */
	      if (bgrade != UUCONF_GRADE_LOW)
		qset->uuconf_qcalltimegrade = qset->uuconf_qtimegrade;
	    }

	  if (iret != UUCONF_SUCCESS)
	    break;

	  if (ctoks < 3)
	    continue;

	  /* Pick up the device name.  It can be followed by a comma
	     and a list of protocols.  */
	  qset->uuconf_zport = pzsplit[2];
	  z = strchr (pzsplit[2], ',');
	  if (z != NULL)
	    {
	      qset->uuconf_zprotocols = z + 1;
	      *z = '\0';
	    }

	  if (ctoks < 4)
	    continue;

	  /* The speed entry can be a numeric speed, or a range of
	     speeds, or "Any", or "-".  If it starts with a letter,
	     the initial nonnumeric prefix is a modem class, which
	     gets appended to the port name.  */
	  z = pzsplit[3];
	  if (strcasecmp (z, "Any") != 0
	      && strcmp (z, "-") != 0)
	    {
	      char *zend;

	      while (*z != '\0' && ! isdigit (BUCHAR (*z)))
		++z;

	      qset->uuconf_ibaud = strtol (z, &zend, 10);
	      if (*zend == '-')
		qset->uuconf_ihighbaud = strtol (zend + 1, (char **) NULL,
						 10);

	      if (z != pzsplit[3])
		{
		  size_t cport, cclass;

		  cport = strlen (pzsplit[2]);
		  cclass = z - pzsplit[3];
		  qset->uuconf_zport = uuconf_malloc (pblock,
						      cport + cclass + 1);
		  if (qset->uuconf_zport == NULL)
		    {
		      qglobal->ierrno = errno;
		      iret = UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
		      break;
		    }
		  memcpy ((pointer) qset->uuconf_zport, (pointer) pzsplit[2],
			  cport);
		  memcpy ((pointer) (qset->uuconf_zport + cport),
			  (pointer) pzsplit[3], cclass);
		  qset->uuconf_zport[cport + cclass] = '\0';
		}
	    }

	  if (ctoks < 5)
	    continue;

	  /* Get the phone number.  */
	  qset->uuconf_zphone = pzsplit[4];

	  if (ctoks < 6)
	    continue;

	  /* Get the chat script.  We just hand this off to the chat
	     script processor, so that it will parse subsend and
	     subexpect strings correctly.  */
	  pzsplit[4] = (char *) "chat";
	  iret = _uuconf_ichat_cmd (qglobal, ctoks - 4, pzsplit + 4,
				    &qset->uuconf_schat, pblock);
	  iret &=~ UUCONF_CMDTABRET_KEEP;
	  if (iret != UUCONF_SUCCESS)
	    break;
	}
  
      (void) fclose (e);

      if (iret != UUCONF_SUCCESS)
	break;
    }

  if (zline != NULL)
    free ((pointer) zline);
  if (pzsplit != NULL)
    free ((pointer) pzsplit);
  if (pzcomma != NULL)
    free ((pointer) pzcomma);

  if (iret != UUCONF_SUCCESS)
    {
      qglobal->zfilename = *pz;
      return iret | UUCONF_ERROR_FILENAME | UUCONF_ERROR_LINENO;
    }

  if (pblock == NULL)
    return UUCONF_NOT_FOUND;

  /* Now we have to put in the Permissions information.  The relevant
     Permissions entries are those with this system in the MACHINE
     list and (if this system does not have a VALIDATE entry) those
     with a LOGNAME list but no MACHINE list.  If no entry is found
     with this system in the MACHINE list, then we must look for an
     entry with "OTHER" in the MACHINE list.  */
  ffound_machine = FALSE;
  ffound_login = FALSE;
  qother_machine = NULL;
  for (qperm = qglobal->qprocess->qhdb_permissions;
       qperm != NULL;
       qperm = qperm->qnext)
    {
      boolean fmachine;

      /* MACHINE=OTHER is recognized specially.  It appears that OTHER
	 need only be recognized by itself, not when combined with
	 other machine names.  */
      if (qother_machine == NULL
	  && qperm->pzmachine != NULL
	  && qperm->pzmachine != (char **) &_uuconf_unset
	  && qperm->pzmachine[0][0] == 'O'
	  && strcmp (qperm->pzmachine[0], "OTHER") == 0)
	qother_machine = qperm;

      /* If this system is named in a MACHINE entry, we must add the
	 appropriate information to every alternate that could be used
	 for calling out.  */
      fmachine = FALSE;
      if (! ffound_machine
	  && qperm->pzmachine != NULL
	  && qperm->pzmachine != (char **) &_uuconf_unset)
	{
	  for (pz = qperm->pzmachine; *pz != NULL; pz++)
	    {
	      if ((*pz)[0] == zsystem[0]
		  && strcmp (*pz, zsystem) == 0)
		{
		  for (qalt = qsys;
		       qalt != NULL;
		       qalt = qalt->uuconf_qalternate)
		    {
		      if (qalt->uuconf_fcall)
			{
			  iret = ihadd_machine_perm (qglobal, qalt, qperm);
			  if (iret != UUCONF_SUCCESS)
			    return iret;
			}
		    }

		  fmachine = TRUE;
		  ffound_machine = TRUE;

		  break;
		}
	    }
	}

      /* A LOGNAME line applies to this machine if it is listed in the
	 corresponding VALIDATE entry, or if it is not listed in any
	 VALIDATE entry.  On this pass through the Permissions entry
	 we pick up the information if the system appears in a
	 VALIDATE entry; if it does not, we make another pass to put
	 in all the LOGNAME lines.  */
      if (qperm->pzlogname != NULL
	  && qperm->pzlogname != (char **) &_uuconf_unset
	  && qperm->pzvalidate != NULL
	  && qperm->pzvalidate != (char **) &_uuconf_unset)
	{
	  for (pz = qperm->pzvalidate; *pz != NULL; ++pz)
	    if ((*pz)[0] == zsystem[0]
		&& strcmp (*pz, zsystem) == 0)
	      break;
	  if (*pz != NULL)
	    {
	      for (pz = qperm->pzlogname; *pz != NULL; ++pz)
		{
		  /* If this LOGNAME line is also a matching MACHINE
		     line, we can add the LOGNAME permissions to the
		     first alternate.  Otherwise, we must create a new
		     alternate.  We cannot put a LOGNAME line in the
		     first alternate if MACHINE does not match,
		     because certain permissions (e.g. READ) may be
		     specified by both types of lines, and we must use
		     LOGNAME entries only when accepting calls and
		     MACHINE entries only when placing calls.  */
		  if (fmachine
		      && (qsys->uuconf_zcalled_login == NULL
			  || (qsys->uuconf_zcalled_login
			      == (char *) &_uuconf_unset)))
		    {
		      qsys->uuconf_zcalled_login = *pz;
		      iret = ihadd_logname_perm (qglobal, qsys, qperm);
		    }
		  else
		    {
		      struct uuconf_system *qnew;
		      struct uuconf_system **pq;

		      qnew = ((struct uuconf_system *)
			      uuconf_malloc (pblock,
					     sizeof (struct uuconf_system)));
		      if (qnew == NULL)
			{
			  qglobal->ierrno = errno;
			  return UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
			}

		      *qnew = *qsys;
		      qnew->uuconf_qalternate = NULL;
		      for (pq = &qsys->uuconf_qalternate;
			   *pq != NULL;
			   pq = &(*pq)->uuconf_qalternate)
			;
		      *pq = qnew;

		      qnew->uuconf_zcalled_login = *pz;
		      qnew->uuconf_fcall = FALSE;
		      iret = ihadd_logname_perm (qglobal, qnew, qperm);
		    }

		  if (iret != UUCONF_SUCCESS)
		    return iret;
		}

	      ffound_login = TRUE;
	    }
	}
    }

  /* If we didn't find an entry for the machine, we must use the
     MACHINE=OTHER entry, if any.  */
  if (! ffound_machine && qother_machine != NULL)
    {
      for (qalt = qsys; qalt != NULL; qalt = qalt->uuconf_qalternate)
	{
	  if (qalt->uuconf_fcall)
	    {
	      iret = ihadd_machine_perm (qglobal, qalt, qother_machine);
	      if (iret != UUCONF_SUCCESS)
		return iret;
	    }
	}
    }

  /* If this system was not listed in any VALIDATE entry, then we must
     add a called-login for each LOGNAME entry in Permissions.  */
  if (! ffound_login)
    {
      for (qperm = qglobal->qprocess->qhdb_permissions;
	   qperm != NULL;
	   qperm = qperm->qnext)
	{
	  if (qperm->pzlogname == NULL
	      || qperm->pzlogname == (char **) &_uuconf_unset)
	    continue;

	  for (pz = qperm->pzlogname; *pz != NULL; pz++)
	    {
	      struct uuconf_system *qnew;
	      struct uuconf_system **pq;

	      qnew = ((struct uuconf_system *)
		      uuconf_malloc (pblock,
				      sizeof (struct uuconf_system)));
	      if (qnew == NULL)
		{
		  qglobal->ierrno = errno;
		  return UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
		}

	      *qnew = *qsys;
	      qnew->uuconf_qalternate = NULL;
	      for (pq = &qsys->uuconf_qalternate;
		   *pq != NULL;
		   pq = &(*pq)->uuconf_qalternate)
		;
	      *pq = qnew;

	      /* We recognize LOGNAME=OTHER specially, although this
		 appears to be an SCO innovation.  */
	      if (strcmp (*pz, "OTHER") == 0)
		qnew->uuconf_zcalled_login = (char *) "ANY";
	      else
		qnew->uuconf_zcalled_login = *pz;
	      qnew->uuconf_fcall = FALSE;
	      iret = ihadd_logname_perm (qglobal, qnew, qperm);
	      if (iret != UUCONF_SUCCESS)
		return iret;
	    }
	}
    }

  /* HDB permits local requests to receive to any directory, which is
     not the default put in by _uuconf_isystem_basic_default.  We set
     it here instead.  */
  for (qalt = qsys; qalt != NULL; qalt = qalt->uuconf_qalternate)
    {
      iret = _uuconf_iadd_string (qglobal, (char *) ZROOTDIR,
				  FALSE, FALSE,
				  &qalt->uuconf_pzlocal_receive,
				  pblock);
      if (iret != UUCONF_SUCCESS)
	return iret;
    }

  /* HDB does not have a maximum number of retries if a retry time is
     given in the time field.  */
  if (qsys->uuconf_qtimegrade != NULL
      && qsys->uuconf_qtimegrade != (struct uuconf_timespan *) &_uuconf_unset
      && qsys->uuconf_qtimegrade->uuconf_cretry > 0)
    qsys->uuconf_cmax_retries = 0;

  return UUCONF_SUCCESS;
}

/* Add the settings of a MACHINE line in Permissions to a system.  */

/*ARGSIGNORED*/
static int
ihadd_machine_perm (qglobal, qsys, qperm)
     struct sglobal *qglobal;
     struct uuconf_system *qsys;
     struct shpermissions *qperm;
{
  if (qperm->frequest >= 0)
    qsys->uuconf_fsend_request = qperm->frequest;
  else
    qsys->uuconf_fsend_request = FALSE;
  qsys->uuconf_pzremote_send = qperm->pzread;
  qsys->uuconf_pzremote_receive = qperm->pzwrite;
  qsys->uuconf_pzcmds = qperm->pzcommands;
  qsys->uuconf_zlocalname = qperm->zmyname;
  qsys->uuconf_zpubdir = qperm->zpubdir;
  qsys->uuconf_pzalias = qperm->pzalias;

  return UUCONF_SUCCESS;
}

/* Add the settings of a LOGNAME line in Permissions to a system.  */

/*ARGSIGNORED*/
static int
ihadd_logname_perm (qglobal, qsys, qperm)
     struct sglobal *qglobal;
     struct uuconf_system *qsys;
     struct shpermissions *qperm;
{
  qsys->uuconf_fcalled = TRUE;
  if (qperm->frequest >= 0)
    qsys->uuconf_fsend_request = qperm->frequest;
  else
    qsys->uuconf_fsend_request = FALSE;
  qsys->uuconf_fcalled_transfer = qperm->fsendfiles;
  qsys->uuconf_pzremote_send = qperm->pzread;
  qsys->uuconf_pzremote_receive = qperm->pzwrite;
  qsys->uuconf_fcallback = qperm->fcallback;
  qsys->uuconf_zlocalname = qperm->zmyname;
  qsys->uuconf_zpubdir = qperm->zpubdir;

  return UUCONF_SUCCESS;
}
