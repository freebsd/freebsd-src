/* rdperm.c
   Read the HDB Permissions file.

   Copyright (C) 1992 Ian Lance Taylor

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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Infinity Development Systems, P.O. Box 520, Waltham, MA 02254.
   */

#include "uucnfi.h"

#if USE_RCS_ID
const char _uuconf_rdperm_rcsid[] = "$Id: rdperm.c,v 1.1 1993/08/05 18:25:50 conklin Exp $";
#endif

#include <errno.h>
#include <ctype.h>

static int ihcolon P((pointer pglobal, int argc, char **argv, pointer pvar,
		      pointer pinfo));
static int ihsendfiles P((pointer pglobal, int argc, char **argv,
			  pointer pvar, pointer pinfo));
static int ihunknownperm P((pointer pglobal, int argc, char **argv,
			    pointer pvar, pointer pinfo));
static int ihadd_norw P((struct sglobal *qglobal, char ***ppz, char **pzno));

/* These routines reads in the HDB Permissions file.  We store the
   entries in a linked list of shpermissions structures, so we only
   have to actually read the file once.  */

/* This command table and static structure are used to parse a line
   from Permissions.  The entries are parsed as follows:

   Multiple strings separated by colons: LOGNAME, MACHINE, READ,
   WRITE, NOREAD, NOWRITE, COMMANDS, VALIDATE, ALIAS.

   Boolean values: REQUEST, CALLBACK.

   Simple strings: MYNAME, PUBDIR.

   "Yes" or "call": SENDFILES.

   The NOREAD and NOWRITE entries are merged into the READ and WRITE
   entries, rather than being permanently stored.  They are handled
   specially in the uuconf_cmdtab table.  */

static const struct cmdtab_offset asHperm_cmds[] =
{
  { "NOREAD", UUCONF_CMDTABTYPE_FN | 2, (size_t) -1, ihcolon },
  { "NOWRITE", UUCONF_CMDTABTYPE_FN | 2, (size_t) -1, ihcolon },
  { "LOGNAME", UUCONF_CMDTABTYPE_FN | 2,
      offsetof (struct shpermissions, pzlogname), ihcolon },
  { "MACHINE", UUCONF_CMDTABTYPE_FN | 2,
      offsetof (struct shpermissions, pzmachine), ihcolon },
  { "REQUEST", UUCONF_CMDTABTYPE_BOOLEAN,
      offsetof (struct shpermissions, frequest), NULL },
  { "SENDFILES", UUCONF_CMDTABTYPE_FN | 2,
      offsetof (struct shpermissions, fsendfiles), ihsendfiles },
  { "READ", UUCONF_CMDTABTYPE_FN | 2,
      offsetof (struct shpermissions, pzread), ihcolon },
  { "WRITE", UUCONF_CMDTABTYPE_FN | 2,
      offsetof (struct shpermissions, pzwrite), ihcolon },
  { "CALLBACK", UUCONF_CMDTABTYPE_BOOLEAN,
      offsetof (struct shpermissions, fcallback), NULL },
  { "COMMANDS", UUCONF_CMDTABTYPE_FN | 2,
      offsetof (struct shpermissions, pzcommands), ihcolon },
  { "VALIDATE", UUCONF_CMDTABTYPE_FN | 2,
      offsetof (struct shpermissions, pzvalidate), ihcolon },
  { "MYNAME", UUCONF_CMDTABTYPE_STRING,
      offsetof (struct shpermissions, zmyname), NULL },
  { "PUBDIR", UUCONF_CMDTABTYPE_STRING,
      offsetof (struct shpermissions, zpubdir), NULL },
  { "ALIAS", UUCONF_CMDTABTYPE_FN | 2,
      offsetof (struct shpermissions, pzalias), ihcolon },
  { NULL, 0, 0, NULL }
};

#define CHPERM_CMDS (sizeof asHperm_cmds / sizeof asHperm_cmds[0])

/* Actually read the Permissions file into a linked list of
   structures.  */

int
_uuconf_ihread_permissions (qglobal)
     struct sglobal *qglobal;
{
  char *zperm;
  FILE *e;
  int iret;
  struct uuconf_cmdtab as[CHPERM_CMDS];
  char **pznoread, **pznowrite;
  struct shpermissions shperm;
  char *zline;
  size_t cline;
  char **pzsplit;
  size_t csplit;
  int cchars;
  struct shpermissions *qlist, **pq;

  if (qglobal->qprocess->fhdb_read_permissions)
    return UUCONF_SUCCESS;

  zperm = (char *) uuconf_malloc (qglobal->pblock,
				  (sizeof OLDCONFIGLIB
				   + sizeof HDB_PERMISSIONS - 1));
  if (zperm == NULL)
    {
      qglobal->ierrno = errno;
      return UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
    }

  memcpy ((pointer) zperm, (pointer) OLDCONFIGLIB,
	  sizeof OLDCONFIGLIB - 1);
  memcpy ((pointer) (zperm + sizeof OLDCONFIGLIB - 1),
	  (pointer) HDB_PERMISSIONS, sizeof HDB_PERMISSIONS);

  e = fopen (zperm, "r");
  if (e == NULL)
    {
      uuconf_free (qglobal->pblock, zperm);
      qglobal->qprocess->fhdb_read_permissions = TRUE;
      return UUCONF_SUCCESS;
    }

  _uuconf_ucmdtab_base (asHperm_cmds, CHPERM_CMDS, (char *) &shperm, as);
  as[0].uuconf_pvar = (pointer) &pznoread;
  as[1].uuconf_pvar = (pointer) &pznowrite;

  zline = NULL;
  cline = 0;
  pzsplit = NULL;
  csplit = 0;

  qlist = NULL;
  pq = &qlist;

  qglobal->ilineno = 0;

  iret = UUCONF_SUCCESS;

  while ((cchars = _uuconf_getline (qglobal, &zline, &cline, e)) > 0)
    {
      int centries;
      struct shpermissions *qnew;
      int i;

      ++qglobal->ilineno;

      --cchars;
      if (zline[cchars] == '\n')
	zline[cchars] = '\0';
      if (isspace (BUCHAR (zline[0])) || zline[0] == '#')
	continue;

      centries = _uuconf_istrsplit (zline, '\0', &pzsplit, &csplit);
      if (centries < 0)
	{
	  qglobal->ierrno = errno;
	  iret = UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
	  break;
	}

      if (centries == 0)
	continue;

      shperm.pzlogname = (char **) &_uuconf_unset;
      shperm.pzmachine = (char **) &_uuconf_unset;
      shperm.frequest = -1;
      shperm.fsendfiles = -1;
      shperm.pzread = (char **) &_uuconf_unset;
      shperm.pzwrite = (char **) &_uuconf_unset;
      shperm.fcallback = -1;
      shperm.pzcommands = (char **) &_uuconf_unset;
      shperm.pzvalidate = (char **) &_uuconf_unset;
      shperm.zmyname = (char *) &_uuconf_unset;
      shperm.zpubdir = (char *) &_uuconf_unset;
      shperm.pzalias = (char **) &_uuconf_unset;
      pznoread = (char **) &_uuconf_unset;
      pznowrite = (char **) &_uuconf_unset;

      for (i = 0; i < centries; i++)
	{
	  char *zeq;
	  char *azargs[2];

	  zeq = strchr (pzsplit[i], '=');
	  if (zeq == NULL)
	    {
	      iret = UUCONF_SYNTAX_ERROR;
	      qglobal->qprocess->fhdb_read_permissions = TRUE;
	      break;
	    }
	  *zeq = '\0';

	  azargs[0] = pzsplit[i];
	  azargs[1] = zeq + 1;

	  iret = uuconf_cmd_args (qglobal, 2, azargs, as, (pointer) NULL,
				  ihunknownperm, 0, qglobal->pblock);
	  if ((iret & UUCONF_CMDTABRET_KEEP) != 0)
	    {
	      iret &=~ UUCONF_CMDTABRET_KEEP;

	      if (uuconf_add_block (qglobal->pblock, zline) != 0)
		{
		  qglobal->ierrno = errno;
		  iret = UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
		  break;
		}

	      zline = NULL;
	      cline = 0;
	    }
	  if ((iret & UUCONF_CMDTABRET_EXIT) != 0)
	    {
	      iret &=~ UUCONF_CMDTABRET_EXIT;
	      break;
	    }
	}

      if (iret != UUCONF_SUCCESS)
	break;

      if (shperm.pzmachine == (char **) &_uuconf_unset
	  && shperm.pzlogname == (char **) &_uuconf_unset)
	{
	  iret = UUCONF_SYNTAX_ERROR;
	  qglobal->qprocess->fhdb_read_permissions = TRUE;
	  break;
	}

      /* Attach any NOREAD or NOWRITE entries to the corresponding
	 READ or WRITE entries in the format expected for the
	 pzlocal_receive, etc., fields in uuconf_system.  */
      if (pznoread != NULL)
	{
	  iret = ihadd_norw (qglobal, &shperm.pzread, pznoread);
	  if (iret != UUCONF_SUCCESS)
	    break;
	  uuconf_free (qglobal->pblock, pznoread);
	}

      if (pznowrite != NULL)
	{
	  iret = ihadd_norw (qglobal, &shperm.pzwrite, pznowrite);
	  if (iret != UUCONF_SUCCESS)
	    break;
	  uuconf_free (qglobal->pblock, pznowrite);
	}

      qnew = ((struct shpermissions *)
	      uuconf_malloc (qglobal->pblock,
			     sizeof (struct shpermissions)));
      if (qnew == NULL)
	{
	  qglobal->ierrno = errno;
	  iret = UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
	  break;
	}

      *qnew = shperm;
      *pq = qnew;
      pq = &qnew->qnext;
      *pq = NULL;
    }

  (void) fclose (e);

  if (zline != NULL)
    free ((pointer) zline);
  if (pzsplit != NULL)
    free ((pointer) pzsplit);

  if (iret == UUCONF_SUCCESS)
    {
      qglobal->qprocess->qhdb_permissions = qlist;
      qglobal->qprocess->fhdb_read_permissions = TRUE;
    }
  else
    {
      qglobal->zfilename = zperm;
      iret |= UUCONF_ERROR_FILENAME | UUCONF_ERROR_LINENO;
    }

  return iret;
}

/* Split the argument into colon separated strings, and assign a NULL
   terminated array of strings to pvar.  */

/*ARGSUSED*/
static int
ihcolon (pglobal, argc, argv, pvar, pinfo)
     pointer pglobal;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  char ***ppz = (char ***) pvar;
  char **pzsplit;
  size_t csplit;
  int centries;
  int i;
  int iret;

  *ppz = NULL;

  pzsplit = NULL;
  csplit = 0;

  centries = _uuconf_istrsplit (argv[1], ':', &pzsplit, &csplit);
  if (centries < 0)
    {
      qglobal->ierrno = errno;
      return (UUCONF_MALLOC_FAILED
	      | UUCONF_ERROR_ERRNO
	      | UUCONF_CMDTABRET_EXIT);
    }

  if (centries == 0)
    {
      if (pzsplit != NULL)
	free ((pointer) pzsplit);
      return UUCONF_CMDTABRET_CONTINUE;
    }

  iret = UUCONF_SUCCESS;

  for (i = 0; i < centries; i++)
    {
      iret = _uuconf_iadd_string (qglobal, pzsplit[i], FALSE, FALSE,
				  ppz, qglobal->pblock);
      if (iret != UUCONF_SUCCESS)
	{
	  iret |= UUCONF_CMDTABRET_EXIT;
	  break;
	}
    }

  free ((pointer) pzsplit);

  return UUCONF_CMDTABRET_KEEP;
}

/* Handle the SENDFILES parameter, which can take "yes" or "call" or
   "no" as an argument.  The string "call" is equivalent to "no".  */

/*ARGSUSED*/
static int
ihsendfiles (pglobal, argc, argv, pvar, pinfo)
     pointer pglobal;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  int *pi = (int *) pvar;

  switch (argv[1][0])
    {
    case 'C':
    case 'c':
    case 'N':
    case 'n':
      *pi = FALSE;
      break;
    case 'Y':
    case 'y':
      *pi = TRUE;
      break;
    default:
      return UUCONF_SYNTAX_ERROR | UUCONF_CMDTABRET_EXIT;
    }

  return UUCONF_CMDTABRET_CONTINUE;
}

/* If there is an unknown Permissions entry, return a syntax error.
   This should probably be more clever.  */

/*ARGSUSED*/
static int
ihunknownperm (pglobal, argc, argv, pvar, pinfo)
     pointer pglobal;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  return UUCONF_SYNTAX_ERROR | UUCONF_CMDTABRET_EXIT;
}

/* Add a NOREAD or NOWRITE entry to a READ or WRITE entry.  */

static int
ihadd_norw (qglobal, ppz, pzno)
     struct sglobal *qglobal;
     char ***ppz;
     char **pzno;
{
  register char **pz;

  if (pzno == (char **) &_uuconf_unset)
    return UUCONF_SUCCESS;

  for (pz = pzno; *pz != NULL; pz++)
    {
      size_t csize;
      char *znew;
      int iret;

      csize = strlen (*pz) + 1;
      znew = (char *) uuconf_malloc (qglobal->pblock, csize + 1);
      if (znew == NULL)
	{
	  qglobal->ierrno = errno;
	  return UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
	}
      znew[0] = '!';
      memcpy ((pointer) (znew + 1), (pointer) *pz, csize);
      iret = _uuconf_iadd_string (qglobal, znew, FALSE, FALSE, ppz,
				  qglobal->pblock);
      if (iret != UUCONF_SUCCESS)
	return iret;
    }

  return UUCONF_SUCCESS;
}
