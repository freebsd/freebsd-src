/* cmdarg.c
   Look up a command with arguments in a command table.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Cygnus Support, 48 Grove Street, Somerville, MA 02144.
   */

#include "uucnfi.h"

#if USE_RCS_ID
const char _uuconf_cmdarg_rcsid[] = "$FreeBSD: src/gnu/libexec/uucp/libuuconf/cmdarg.c,v 1.7 1999/08/27 23:33:16 peter Exp $";
#endif

#include <ctype.h>

#undef strcmp
#if HAVE_STRCASECMP
#undef strcasecmp
#endif
extern int strcmp (), strcasecmp ();

/* Look up a command with arguments in a table and execute it.  */

int
uuconf_cmd_args (pglobal, cargs, pzargs, qtab, pinfo, pfiunknown, iflags,
		 pblock)
     pointer pglobal;
     int cargs;
     char **pzargs;
     const struct uuconf_cmdtab *qtab;
     pointer pinfo;
     int (*pfiunknown) P((pointer, int, char **, pointer, pointer));
     int iflags;
     pointer pblock;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  int bfirstu, bfirstl;
  int (*pficmp) P((const char *, const char *));
  register const struct uuconf_cmdtab *q;
  int itype;
  int callowed;

  bfirstu = bfirstl = pzargs[0][0];
  if ((iflags & UUCONF_CMDTABFLAG_CASE) != 0)
    pficmp = strcmp;
  else
    {
      if (islower (bfirstu))
	bfirstu = toupper (bfirstu);
      if (isupper (bfirstl))
	bfirstl = tolower (bfirstl);
      pficmp = strcasecmp;
    }

  itype = 0;

  for (q = qtab; q->uuconf_zcmd != NULL; q++)
    {
      int bfirst;

      bfirst = q->uuconf_zcmd[0];
      if (bfirst != bfirstu && bfirst != bfirstl)
	continue;

      itype = UUCONF_TTYPE_CMDTABTYPE (q->uuconf_itype);
      if (itype != UUCONF_CMDTABTYPE_PREFIX)
	{
	  if ((*pficmp) (q->uuconf_zcmd, pzargs[0]) == 0)
	    break;
	}
      else
	{
	  size_t clen;

	  clen = strlen (q->uuconf_zcmd);
	  if ((iflags & UUCONF_CMDTABFLAG_CASE) != 0)
	    {
	      if (strncmp (q->uuconf_zcmd, pzargs[0], clen) == 0)
		break;
	    }
	  else
	    {
	      if (strncasecmp (q->uuconf_zcmd, pzargs[0], clen) == 0)
		break;
	    }
	}
    }

  if (q->uuconf_zcmd == NULL)
    {
      if (pfiunknown == NULL)
	return UUCONF_CMDTABRET_CONTINUE;
      return (*pfiunknown) (pglobal, cargs, pzargs, (pointer) NULL, pinfo);
    }

  callowed = UUCONF_CARGS_CMDTABTYPE (q->uuconf_itype);
  if (callowed != 0 && callowed != cargs)
    return UUCONF_SYNTAX_ERROR | UUCONF_CMDTABRET_EXIT;

  switch (itype)
    {
    case UUCONF_TTYPE_CMDTABTYPE (UUCONF_CMDTABTYPE_STRING):
      if (cargs == 1)
	*(char **) q->uuconf_pvar = (char *) "";
      else if (cargs == 2)
	*(char **) q->uuconf_pvar = pzargs[1];
      else
	return UUCONF_SYNTAX_ERROR | UUCONF_CMDTABRET_EXIT;

      return UUCONF_CMDTABRET_KEEP;

    case UUCONF_TTYPE_CMDTABTYPE (UUCONF_CMDTABTYPE_INT):
      return _uuconf_iint (qglobal, pzargs[1], q->uuconf_pvar, TRUE);

    case UUCONF_TTYPE_CMDTABTYPE (UUCONF_CMDTABTYPE_LONG):
      return _uuconf_iint (qglobal, pzargs[1], q->uuconf_pvar, FALSE);

    case UUCONF_TTYPE_CMDTABTYPE (UUCONF_CMDTABTYPE_BOOLEAN):
      return _uuconf_iboolean (qglobal, pzargs[1], (int *) q->uuconf_pvar);

    case UUCONF_TTYPE_CMDTABTYPE (UUCONF_CMDTABTYPE_FULLSTRING):
      if (cargs == 1)
	{
	  char ***ppz = (char ***) q->uuconf_pvar;
	  int iret;
	  
	  *ppz = NULL;
	  iret = _uuconf_iadd_string (qglobal, (char *) NULL, FALSE, FALSE,
				      ppz, pblock);
	  if (iret != UUCONF_SUCCESS)
	    return iret | UUCONF_CMDTABRET_EXIT;

	  return UUCONF_CMDTABRET_CONTINUE;
	}
      else
	{
	  char ***ppz = (char ***) q->uuconf_pvar;
	  int i;

	  *ppz = NULL;
	  for (i = 1; i < cargs; i++)
	    {
	      int iret;

	      iret = _uuconf_iadd_string (qglobal, pzargs[i], FALSE, FALSE,
					  ppz, pblock);
	      if (iret != UUCONF_SUCCESS)
		{
		  *ppz = NULL;
		  return iret | UUCONF_CMDTABRET_EXIT;
		}
	    }

	  return UUCONF_CMDTABRET_KEEP;
	}

    case UUCONF_TTYPE_CMDTABTYPE (UUCONF_CMDTABTYPE_FN):
    case UUCONF_TTYPE_CMDTABTYPE (UUCONF_CMDTABTYPE_PREFIX):
      return (*q->uuconf_pifn) (pglobal, cargs, pzargs, q->uuconf_pvar,
				pinfo);

    default:
      return UUCONF_SYNTAX_ERROR | UUCONF_CMDTABRET_EXIT;
    }

  /*NOTREACHED*/
}
