/* xcmd.c
   Routines to handle work requests.

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

#if USE_RCS_ID
const char xcmd_rcsid[] = "$Id: xcmd.c,v 1.1 1993/08/05 18:27:25 conklin Exp $";
#endif

#include <errno.h>

#include "uudefs.h"
#include "uuconf.h"
#include "system.h"
#include "prot.h"
#include "trans.h"

/* Local functions.  */

static boolean flocal_xcmd_request P((struct stransfer *qtrans,
				      struct sdaemon *qdaemon));
static boolean flocal_xcmd_await_reply P((struct stransfer *qtrans,
					  struct sdaemon *qdaemon,
					  const char *zdata, size_t cdata));
static boolean fremote_xcmd_reply P((struct stransfer *qtrans,
				     struct sdaemon *qdaemon));

/* Handle a local work request.  We just set up the request for
   transmission.  */

boolean
flocal_xcmd_init (qdaemon, qcmd)
     struct sdaemon *qdaemon;
     struct scmd *qcmd;
{
  struct stransfer *qtrans;

  qtrans = qtransalc (qcmd);
  qtrans->psendfn = flocal_xcmd_request;

  return fqueue_local (qdaemon, qtrans);
}

/* Send the execution request to the remote system.  */

static boolean
flocal_xcmd_request (qtrans, qdaemon)
     struct stransfer *qtrans;
     struct sdaemon *qdaemon;
{
  size_t clen;
  char *zsend;
  boolean fret;

  ulog (LOG_NORMAL, "Requesting work: %s to %s", qtrans->s.zfrom,
	qtrans->s.zto);

  /* We send the string
     X from to user options
     We put a dash in front of options.  */
  clen = (strlen (qtrans->s.zfrom) + strlen (qtrans->s.zto)
	  + strlen (qtrans->s.zuser) + strlen (qtrans->s.zoptions) + 7);
  zsend = zbufalc (clen);
  sprintf (zsend, "X %s %s %s -%s", qtrans->s.zfrom, qtrans->s.zto,
	   qtrans->s.zuser, qtrans->s.zoptions);

  fret = (*qdaemon->qproto->pfsendcmd) (qdaemon, zsend, qtrans->ilocal,
					qtrans->iremote);
  ubuffree (zsend);
  if (! fret)
    {
      utransfree (qtrans);
      return FALSE;
    }

  qtrans->fcmd = TRUE;
  qtrans->precfn = flocal_xcmd_await_reply;

  return fqueue_receive (qdaemon, qtrans);
}

/* Get a reply to an execution request from the remote system.  */

/*ARGSUSED*/
static boolean
flocal_xcmd_await_reply (qtrans, qdaemon, zdata, cdata)
     struct stransfer *qtrans;
     struct sdaemon *qdaemon;
     const char *zdata;
     size_t cdata;
{
  qtrans->precfn = NULL;

  if (zdata[0] != 'X'
      || (zdata[1] != 'Y' && zdata[1] != 'N'))
    {
      ulog (LOG_ERROR, "Bad response to work request");
      utransfree (qtrans);
      return FALSE;
    }

  if (zdata[1] == 'N')
    {
      ulog (LOG_ERROR, "%s: work request denied", qtrans->s.zfrom);
      (void) fmail_transfer (FALSE, qtrans->s.zuser, (const char *) NULL,
			     "work request denied",
			     qtrans->s.zfrom, qdaemon->qsys->uuconf_zname,
			     qtrans->s.zto, (const char *) NULL,
			     (const char *) NULL);
    }

  (void) fsysdep_did_work (qtrans->s.pseq);
  utransfree (qtrans);

  return TRUE;
}

/* Handle a remote work request.  This just queues up the requests for
   later processing.  */

boolean
fremote_xcmd_init (qdaemon, qcmd, iremote)
     struct sdaemon *qdaemon;
     struct scmd *qcmd;
     int iremote;
{
  const struct uuconf_system *qsys;
  const char *zexclam;
  const struct uuconf_system *qdestsys;
  struct uuconf_system sdestsys;
  char *zdestfile;
  boolean fmkdirs;
  struct stransfer *qtrans;
  char *zuser;
  char aboptions[5];
  char *zfrom;
  boolean fret;
  char *zfile;

  ulog (LOG_NORMAL, "Work requested: %s to %s", qcmd->zfrom,
	qcmd->zto);

  qsys = qdaemon->qsys;

  zexclam = strchr (qcmd->zto, '!');
  if (zexclam == NULL
      || zexclam == qcmd->zto
      || strncmp (qdaemon->zlocalname, qcmd->zto,
		  (size_t) (zexclam - qcmd->zto)) == 0)
    {
      const char *zconst;

      /* The files are supposed to be copied to the local system.  */
      qdestsys = NULL;
      if (zexclam == NULL)
	zconst = qcmd->zto;
      else
	zconst = zexclam + 1;

      zdestfile = zsysdep_local_file (zconst, qsys->uuconf_zpubdir);
      if (zdestfile == NULL)
	return FALSE;

      zuser = NULL;
      fmkdirs = strchr (qcmd->zoptions, 'f') != NULL;
    }
  else
    {
      size_t clen;
      char *zcopy;
      int iuuconf;
      char *zoptions;

      clen = zexclam - qcmd->zto;
      zcopy = zbufalc (clen + 1);
      memcpy (zcopy, qcmd->zto, clen);
      zcopy[clen] = '\0';

      iuuconf = uuconf_system_info (qdaemon->puuconf, zcopy, &sdestsys);
      if (iuuconf == UUCONF_NOT_FOUND)
	{
	  if (! funknown_system (qdaemon->puuconf, zcopy, &sdestsys))
	    {
	      ulog (LOG_ERROR, "%s: System not found", zcopy);
	      ubuffree (zcopy);
	      qtrans = qtransalc (qcmd);
	      qtrans->psendfn = fremote_xcmd_reply;
	      qtrans->pinfo = (pointer) "XN";
	      qtrans->iremote = iremote;
	      return fqueue_remote (qdaemon, qtrans);
	    }
	}
      else if (iuuconf != UUCONF_SUCCESS)
	{
	  ulog_uuconf (LOG_ERROR, qdaemon->puuconf, iuuconf);
	  ubuffree (zcopy);
	  return FALSE;
	}

      ubuffree (zcopy);

      qdestsys = &sdestsys;
      zdestfile = zbufcpy (zexclam + 1);

      zuser = zbufalc (strlen (qdestsys->uuconf_zname)
		       + strlen (qcmd->zuser) + sizeof "!");
      sprintf (zuser, "%s!%s", qdestsys->uuconf_zname, qcmd->zuser);
      zoptions = aboptions;
      *zoptions++ = 'C';
      if (strchr (qcmd->zoptions, 'd') != NULL)
	*zoptions++ = 'd';
      if (strchr (qcmd->zoptions, 'm') != NULL)
	*zoptions++ = 'm';
      *zoptions = '\0';
      fmkdirs = TRUE;
    }

  /* At this point we prepare to confirm the remote request.  We could
     actually fork here and let the child spool up the requests.  */
  qtrans = qtransalc (qcmd);
  qtrans->psendfn = fremote_xcmd_reply;
  qtrans->pinfo = (pointer) "XY";
  qtrans->iremote = iremote;
  if (! fqueue_remote (qdaemon, qtrans))
    {
      ubuffree (zdestfile);
      ubuffree (zuser);
      return FALSE;
    }

  /* Now we have to process each source file.  The source
     specification may or may use wildcards.  */
  zfrom = zsysdep_local_file (qcmd->zfrom, qsys->uuconf_zpubdir);
  if (zfrom == NULL)
    {
      ubuffree (zdestfile);
      ubuffree (zuser);
      return FALSE;
    }

  if (! fsysdep_wildcard_start (zfrom))
    {
      ubuffree (zfrom);
      ubuffree (zdestfile);
      ubuffree (zuser);
      return FALSE;
    }

  fret = TRUE;

  while ((zfile = zsysdep_wildcard (zfrom)) != NULL)
    {
      char *zto;
      char abtname[CFILE_NAME_LEN];

      if (! fsysdep_file_exists (zfile))
	{
	  ulog (LOG_ERROR, "%s: no such file", zfile);
	  continue;
	}

      /* Make sure the remote system is permitted to read the
	 specified file.  */
      if (! fin_directory_list (zfile, qsys->uuconf_pzremote_send,
				qsys->uuconf_zpubdir, TRUE, TRUE,
				(const char *) NULL))
	{
	  ulog (LOG_ERROR, "%s: not permitted to send", zfile);
	  break;
	}

      if (qdestsys != NULL)
	{
	  /* We really should get the original grade here.  */
	  zto = zsysdep_data_file_name (qdestsys, qdaemon->zlocalname,
					BDEFAULT_UUCP_GRADE, FALSE,
					abtname, (char *) NULL,
					(char *) NULL);
	  if (zto == NULL)
	    {
	      fret = FALSE;
	      break;
	    }
	}
      else
	{
	  zto = zsysdep_add_base (zdestfile, zfile);
	  if (zto == NULL)
	    {
	      fret = FALSE;
	      break;
	    }
	  /* We only accept a local destination if the remote system
	     has the right to create files there.  */
	  if (! fin_directory_list (zto, qsys->uuconf_pzremote_receive,
				    qsys->uuconf_zpubdir, TRUE, FALSE,
				    (const char *) NULL))
	    {
	      ulog (LOG_ERROR, "%s: not permitted to receive", zto);
	      ubuffree (zto);
	      break;
	    }
	}

      /* Copy the file either to the final destination or to the
	 spool directory.  */
      if (! fcopy_file (zfile, zto, qdestsys == NULL, fmkdirs))
	{
	  ubuffree (zto);
	  break;
	}

      ubuffree (zto);

      /* If there is a destination system, queue it up.  */
      if (qdestsys != NULL)
	{
	  struct scmd ssend;
	  char *zjobid;

	  ssend.bcmd = 'S';
	  ssend.pseq = NULL;
	  ssend.zfrom = zfile;
	  ssend.zto = zdestfile;
	  ssend.zuser = zuser;
	  ssend.zoptions = aboptions;
	  ssend.ztemp = abtname;
	  ssend.imode = ixsysdep_file_mode (zfile);
	  ssend.znotify = "";
	  ssend.cbytes = -1;
	  ssend.zcmd = NULL;
	  ssend.ipos = 0;

	  zjobid = zsysdep_spool_commands (qdestsys, BDEFAULT_UUCP_GRADE,
					   1, &ssend);
	  if (zjobid == NULL)
	    break;
	  ubuffree (zjobid);
	}

      ubuffree (zfile);
    }

  if (zfile != NULL)
    ubuffree (zfile);

  (void) fsysdep_wildcard_end ();

  ubuffree (zdestfile);
  if (qdestsys != NULL)
    (void) uuconf_system_free (qdaemon->puuconf, &sdestsys);

  ubuffree (zfrom);
  ubuffree (zuser);

  return fret;
}

/* Reply to a remote work request.  */

static boolean
fremote_xcmd_reply (qtrans, qdaemon)
     struct stransfer *qtrans;
     struct sdaemon *qdaemon;
{
  boolean fret;

  fret = (*qdaemon->qproto->pfsendcmd) (qdaemon,
					(const char *) qtrans->pinfo,
					qtrans->ilocal,
					qtrans->iremote);
  utransfree (qtrans);
  return fret;
}
