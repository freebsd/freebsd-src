/* vsinfo.c
   Get information about a system from the V2 configuration files.

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
const char _uuconf_vsinfo_rcsid[] = "$FreeBSD$";
#endif

#include <errno.h>
#include <ctype.h>

/* Get the information for a particular system from the V2
   configuration files.  This does not make sure that all the default
   values are set.  */

int
_uuconf_iv2_system_internal (qglobal, zsystem, qsys)
     struct sglobal *qglobal;
     const char *zsystem;
     struct uuconf_system *qsys;
{
  char *zline;
  size_t cline;
  char **pzsplit;
  size_t csplit;
  char **pzcomma;
  size_t ccomma;
  FILE *e;
  int cchars;
  pointer pblock;
  int iret;

  e = fopen (qglobal->qprocess->zv2systems, "r");
  if (e == NULL)
    {
      if (FNO_SUCH_FILE ())
	return UUCONF_NOT_FOUND;
      qglobal->ierrno = errno;
      qglobal->zfilename = qglobal->qprocess->zv2systems;
      return (UUCONF_FOPEN_FAILED
	      | UUCONF_ERROR_ERRNO
	      | UUCONF_ERROR_FILENAME);
    }

  zline = NULL;
  cline = 0;
  pzsplit = NULL;
  csplit = 0;
  pzcomma = NULL;
  ccomma = 0;

  pblock = NULL;
  iret = UUCONF_SUCCESS;

  qglobal->ilineno = 0;

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
      zline[strcspn (zline, "#")] = '\0';

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
      qset->uuconf_fcalled = TRUE;

      if (ctoks < 2)
	continue;

      /* A time string is "time/grade,time/grade;retry".  A missing
	 grade is taken as BGRADE_LOW.  On some versions the retry
	 time is actually separated by a comma, which won't work right
	 here.  */
      zretry = strchr (pzsplit[1], ';');
      if (zretry == NULL)
	cretry = 55;
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

	  iret = _uuconf_itime_parse (qglobal, z, (long) bgrade, cretry,
				      _uuconf_itime_grade_cmp,
				      &qset->uuconf_qtimegrade,
				      pblock);

	  /* We treat a syntax error in the time field as equivalent
	     to ``never'', on the assumption that that is what V2
	     does.  */
	  if (iret == UUCONF_SYNTAX_ERROR)
	    iret = UUCONF_SUCCESS;

	  if (iret != UUCONF_SUCCESS)
	    break;

	  /* Treat any time/grade setting as both a timegrade and a
	     call-timegrade.  */
	  if (bgrade != UUCONF_GRADE_LOW)
	    qset->uuconf_qcalltimegrade = qset->uuconf_qtimegrade;
	}

      if (iret != UUCONF_SUCCESS)
	break;

      if (ctoks < 3)
	continue;

      /* Pick up the device name.  It can be followed by a comma and a
	 list of protocols (this is not actually supported by most V2
	 systems, but it should be compatible).  */
      qset->uuconf_zport = pzsplit[2];
      z = strchr (pzsplit[2], ',');
      if (z != NULL)
	{
	  qset->uuconf_zprotocols = z + 1;
	  *z = '\0';
	}

      /* If the port is "TCP", we set up a system specific port.  The
	 baud rate becomes the service number and the phone number
	 becomes the address (still stored in qsys->zphone).  */
      if (strcmp (qset->uuconf_zport, "TCP") == 0)
	{
	  qset->uuconf_zport = NULL;
	  qset->uuconf_qport = ((struct uuconf_port *)
				uuconf_malloc (pblock,
					       sizeof (struct uuconf_port)));
	  if (qset->uuconf_qport == NULL)
	    {
	      qglobal->ierrno = errno;
	      iret = UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
	      break;
	    }
	  _uuconf_uclear_port (qset->uuconf_qport);
	  qset->uuconf_qport->uuconf_zname = (char *) "TCP";
	  qset->uuconf_qport->uuconf_ttype = UUCONF_PORTTYPE_TCP;
	  qset->uuconf_qport->uuconf_ireliable
	    = (UUCONF_RELIABLE_ENDTOEND | UUCONF_RELIABLE_RELIABLE
	       | UUCONF_RELIABLE_EIGHT | UUCONF_RELIABLE_FULLDUPLEX
	       | UUCONF_RELIABLE_SPECIFIED);
	  if (ctoks < 4)
	    qset->uuconf_qport->uuconf_u.uuconf_stcp.uuconf_zport
	      = (char *) "uucp";
	  else
	    qset->uuconf_qport->uuconf_u.uuconf_stcp.uuconf_zport
	      = pzsplit[3];
	  qset->uuconf_qport->uuconf_u.uuconf_stcp.uuconf_pzdialer = NULL;
	}

      if (ctoks < 4)
	continue;

      qset->uuconf_ibaud = strtol (pzsplit[3], (char **) NULL, 10);

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

  if (pzcomma != NULL)
    free ((pointer) pzcomma);

  if (iret != UUCONF_SUCCESS)
    {
      if (zline != NULL)
	free ((pointer) zline);
      if (pzsplit != NULL)
	free ((pointer) pzsplit);
      qglobal->zfilename = qglobal->qprocess->zv2systems;
      return iret | UUCONF_ERROR_FILENAME | UUCONF_ERROR_LINENO;
    }

  if (pblock == NULL)
    {
      if (zline != NULL)
	free ((pointer) zline);
      if (pzsplit != NULL)
	free ((pointer) pzsplit);
      return UUCONF_NOT_FOUND;
    }

  /* Now read USERFILE and L.cmds to get permissions.  We can't fully
     handle USERFILE since that specifies permissions based on local
     users which we do not support.  */
  {
    e = fopen (qglobal->qprocess->zv2userfile, "r");
    if (e != NULL)
      {
	char **pzlocal, **pzremote;
	boolean fdefault_callback;
	char *zdefault_login;
	struct uuconf_system *q;

	pzlocal = NULL;
	pzremote = NULL;
	fdefault_callback = FALSE;
	zdefault_login = NULL;

	qglobal->ilineno = 0;

	while ((cchars = getline (&zline, &cline, e)) > 0)
	  {
	    int ctoks;
	    char *zcomma;
	    boolean fcallback;
	    char **pzlist, **pznew;

	    ++qglobal->ilineno;

	    --cchars;
	    if (zline[cchars] == '\n')
	      zline[cchars] = '\0';
	    zline[strcspn (zline, "#")] = '\0';

	    ctoks = _uuconf_istrsplit (zline, '\0', &pzsplit, &csplit);
	    if (ctoks < 0)
	      {
		qglobal->ierrno = errno;
		iret = UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
		break;
	      }

	    if (ctoks == 0)
	      continue;

	    /* The first field is username,machinename */
	    zcomma = strchr (pzsplit[0], ',');
	    if (zcomma == NULL)
	      continue;

	    *zcomma++ = '\0';

	    /* The rest of the line is the list of directories, except
	       that if the first directory is "c" we must call the
	       system back.  */
	    fcallback = FALSE;
	    pzlist = pzsplit + 1;
	    --ctoks;
	    if (ctoks > 0
		&& pzsplit[1][0] == 'c'
		&& pzsplit[1][1] == '\0')
	      {
		fcallback = TRUE;
		pzlist = pzsplit + 2;
		--ctoks;
	      }

	    /* Now pzsplit[0] is the user name, zcomma is the system
	       name, fcallback indicates whether a call back is
	       required, ctoks is the number of directories and pzlist
	       points to the directories.  If the system name matches,
	       then the user name is the name that the system must use
	       to log in, and the list of directories is what may be
	       transferred in by either local or remote request.
	       Otherwise, if no system name matches, then the first
	       line with no user name gives the list of directories
	       that may be transferred by local request, and the first
	       line with no system name gives the list of directories
	       that may be transferred by remote request.  */
	    if ((pzsplit[0][0] != '\0' || pzlocal != NULL)
		&& (zcomma[0] != '\0' || pzremote != NULL)
		&& strcmp (zcomma, zsystem) != 0)
	      continue;

	    /* NULL terminate the list of directories.  */
	    pznew = (char **) uuconf_malloc (pblock,
					      (ctoks + 1) * sizeof (char *));
	    if (pznew == NULL)
	      {
		qglobal->ierrno = errno;
		iret = UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
		break;
	      }
	    memcpy ((pointer) pznew, (pointer) pzlist,
		    ctoks * sizeof (char *));
	    pznew[ctoks] = NULL;

	    if (uuconf_add_block (pblock, zline) != 0)
	      {
		qglobal->ierrno = errno;
		iret = UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
		break;
	      }
	    zline = NULL;
	    cline = 0;

	    if (pzsplit[0][0] == '\0')
	      {
		pzlocal = pznew;
		fdefault_callback = fcallback;
	      }
	    else if (zcomma[0] == '\0')
	      {
		pzremote = pznew;
		zdefault_login = pzsplit[0];
	      }
	    else
	      {
		/* Both the login name and the machine name were
		   listed; require the machine to be logged in under
		   this name.  This is not fully backward compatible,
		   and perhaps should be changed.  On the other hand,
		   it is more useful.  */
		for (q = qsys; q != NULL; q = q->uuconf_qalternate)
		  {
		    q->uuconf_zcalled_login = pzsplit[0];
		    q->uuconf_fcallback = fcallback;
		    q->uuconf_pzlocal_send = pznew;
		    q->uuconf_pzlocal_receive = pznew;
		    q->uuconf_pzremote_send = pznew;
		    q->uuconf_pzremote_receive = pznew;
		  }

		break;
	      }
	  }

	(void) fclose (e);

	if (iret != UUCONF_SUCCESS)
	  {
	    if (zline != NULL)
	      free ((pointer) zline);
	    if (pzsplit != NULL)
	      free ((pointer) pzsplit);
	    qglobal->zfilename = qglobal->qprocess->zv2userfile;
	    return iret | UUCONF_ERROR_FILENAME | UUCONF_ERROR_LINENO;
	  }

	if (qsys->uuconf_pzlocal_send == (char **) &_uuconf_unset
	    && pzlocal != NULL)
	  {
	    for (q = qsys; q != NULL; q = q->uuconf_qalternate)
	      {
		q->uuconf_fcallback = fdefault_callback;
		q->uuconf_pzlocal_send = pzlocal;
		q->uuconf_pzlocal_receive = pzlocal;
	      }
	  }

	if (qsys->uuconf_pzremote_send == (char **) &_uuconf_unset
	    && pzremote != NULL)
	  {
	    for (q = qsys; q != NULL; q = q->uuconf_qalternate)
	      {
		q->uuconf_zcalled_login = zdefault_login;
		q->uuconf_pzremote_send = pzremote;
		q->uuconf_pzremote_receive = pzremote;
	      }
	  }
      }
  }

  /* Now we must read L.cmds to determine which commands may be
     executed.  */
  {
    e = fopen (qglobal->qprocess->zv2cmds, "r");
    if (e != NULL)
      {
	qglobal->ilineno = 0;

	if (getline (&zline, &cline, e) > 0)
	  {
	    ++qglobal->ilineno;

	    zline[strcspn (zline, "#\n")] = '\0';

	    while (*zline == '\0')
	      {
		if (getline (&zline, &cline, e) <= 0)
		  {
		    if (zline != NULL)
		      {
			free ((pointer) zline);
			zline = NULL;
		      }
		  }
		else
		  {
		    ++qglobal->ilineno;
		    zline[strcspn (zline, "#\n")] = '\0';
		  }
	      }

	    if (zline != NULL
		&& strncmp (zline, "PATH=", sizeof "PATH=" - 1) == 0)
	      {
		int ctoks;
		char **pznew;

		zline += sizeof "PATH=" - 1;
		ctoks = _uuconf_istrsplit (zline, ':', &pzsplit, &csplit);
		if (ctoks < 0)
		  {
		    qglobal->ierrno = errno;
		    iret = UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
		  }

		pznew = NULL;
		if (iret == UUCONF_SUCCESS)
		  {
		    pznew = ((char **)
			     uuconf_malloc (pblock,
					     (ctoks + 1) * sizeof (char *)));
		    if (pznew == NULL)
		      iret = UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
		  }
		if (iret == UUCONF_SUCCESS)
		  {
		    memcpy ((pointer) pznew, (pointer) pzsplit,
			    ctoks * sizeof (char *));
		    pznew[ctoks] = NULL;
		    qsys->uuconf_pzpath = pznew;
		    zline = NULL;
		    cline = 0;
		  }      

		if (getline (&zline, &cline, e) < 0)
		  {
		    if (zline != NULL)
		      {
			free ((pointer) zline);
			zline = NULL;
		      }
		  }
		else
		  ++qglobal->ilineno;
	      }
	  }

	if (iret == UUCONF_SUCCESS && zline != NULL)
	  {
	    while (TRUE)
	      {
		zline[strcspn (zline, "#,\n")] = '\0';
		if (*zline != '\0')
		  {
		    iret = _uuconf_iadd_string (qglobal, zline, TRUE, FALSE,
						&qsys->uuconf_pzcmds,
						pblock);
		    if (iret != UUCONF_SUCCESS)
		      break;
		  }
		if (getline (&zline, &cline, e) < 0)
		  break;
		++qglobal->ilineno;
	      }
	  }

	(void) fclose (e);

	if (iret != UUCONF_SUCCESS)
	  {
	    qglobal->zfilename = qglobal->qprocess->zv2cmds;
	    iret |= UUCONF_ERROR_FILENAME | UUCONF_ERROR_LINENO;
	  }
      }
  }

  if (zline != NULL)
    free ((pointer) zline);
  if (pzsplit != NULL)
    free ((pointer) pzsplit);

  return iret;
}
