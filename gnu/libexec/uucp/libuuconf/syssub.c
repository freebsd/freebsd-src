/* syssub.c
   System information subroutines.

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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Cygnus Support, Building 200, 1 Kendall Square, Cambridge, MA 02139.
   */

#include "uucnfi.h"

#if USE_RCS_ID
const char _uuconf_syssub_rcsid[] = "$Id: syssub.c,v 1.2 1994/05/07 18:12:59 ache Exp $";
#endif

#include <errno.h>

/* This macro operates on every string (char *) field in struct
   uuconf_system.  */
#define SYSTEM_STRINGS(OP) \
  do \
    { \
      OP (uuconf_zname); \
      OP (uuconf_zalternate); \
      OP (uuconf_zdebug); \
      OP (uuconf_zmax_remote_debug); \
      OP (uuconf_zphone); \
      OP (uuconf_zcall_login); \
      OP (uuconf_zcall_password); \
      OP (uuconf_zcalled_login); \
      OP (uuconf_zprotocols); \
      OP (uuconf_zpubdir); \
      OP (uuconf_zlocalname); \
    } \
  while (0)

/* This macro operates on every string array (char **) field in struct
   uuconf_system.  */
#define SYSTEM_STRING_ARRAYS(OP) \
  do \
    { \
      OP (uuconf_pzalias); \
      OP (uuconf_pzlocal_send); \
      OP (uuconf_pzremote_send); \
      OP (uuconf_pzlocal_receive); \
      OP (uuconf_pzremote_receive); \
      OP (uuconf_pzpath); \
      OP (uuconf_pzcmds); \
      OP (uuconf_pzforward_from); \
      OP (uuconf_pzforward_to); \
      OP (uuconf_schat.uuconf_pzchat); \
      OP (uuconf_schat.uuconf_pzprogram); \
      OP (uuconf_schat.uuconf_pzfail); \
      OP (uuconf_scalled_chat.uuconf_pzchat); \
      OP (uuconf_scalled_chat.uuconf_pzprogram); \
      OP (uuconf_scalled_chat.uuconf_pzfail); \
    } \
  while (0)

/* This macro operations on every timespan pointer (struct
   uuconf_timespan *) in struct uuconf_system.  */
#define SYSTEM_TIMESPANS(OP) \
  do \
    { \
      OP (uuconf_qtimegrade); \
      OP (uuconf_qcalltimegrade); \
      OP (uuconf_qcall_local_size); \
      OP (uuconf_qcall_remote_size); \
      OP (uuconf_qcalled_local_size); \
      OP (uuconf_qcalled_remote_size); \
    } \
  while (0)

/* This macro operates on every boolean value (of type int, although
   some type int are not boolean) field in uuconf_system.  */
#define SYSTEM_BOOLEANS(OP) \
  do \
    { \
      OP (uuconf_fcall); \
      OP (uuconf_fcalled); \
      OP (uuconf_fcallback); \
      OP (uuconf_fsequence); \
      OP (uuconf_fsend_request); \
      OP (uuconf_frec_request); \
      OP (uuconf_fcall_transfer); \
      OP (uuconf_fcalled_transfer); \
      OP (uuconf_schat.uuconf_fstrip); \
      OP (uuconf_scalled_chat.uuconf_fstrip); \
    } \
  while (0)

/* This macro operates on every generic integer (type int or long) in
   uuconf_system.  */
#define SYSTEM_INTEGERS(OP) \
  do \
    { \
      OP (uuconf_cmax_retries); \
      OP (uuconf_csuccess_wait); \
      OP (uuconf_ibaud); \
      OP (uuconf_ihighbaud); \
      OP (uuconf_cfree_space); \
      OP (uuconf_schat.uuconf_ctimeout); \
      OP (uuconf_scalled_chat.uuconf_ctimeout); \
    } \
  while (0)

/* There is no macro for uuconf_qalternate, uuconf_zport,
   uuconf_qport, uuconf_qproto_params, or uuconf_palloc.  */

/* Clear the contents of a struct uuconf_system.  */

void
_uuconf_uclear_system (q)
     struct uuconf_system *q;
{
#define CLEAR(x) q->x = (char *) &_uuconf_unset
  SYSTEM_STRINGS (CLEAR);
#undef CLEAR
#define CLEAR(x) q->x = (char **) &_uuconf_unset
  SYSTEM_STRING_ARRAYS (CLEAR);
#undef CLEAR
#define CLEAR(x) q->x = (struct uuconf_timespan *) &_uuconf_unset
  SYSTEM_TIMESPANS (CLEAR);
#undef CLEAR
#define CLEAR(x) q->x = -1
  SYSTEM_BOOLEANS (CLEAR);
  SYSTEM_INTEGERS (CLEAR);
#undef CLEAR
  q->uuconf_qalternate = NULL;
  q->uuconf_zport = (char *) &_uuconf_unset;
  q->uuconf_qport = (struct uuconf_port *) &_uuconf_unset;
  q->uuconf_qproto_params = (struct uuconf_proto_param *) &_uuconf_unset;
  q->uuconf_palloc = NULL;
}

/* Default the contents of one struct uuconf_system to the contents of
   another.  This default alternate by alternate.  Any additional
   alternates in q default to the last alternate of qdefault.  If the
   faddalternates arguments is TRUE, additional alternates or qdefault
   are added to q; these alternates are copies of the first alternate
   of q, and defaults are set from the additional alternates of
   qdefault.  */

int
_uuconf_isystem_default (qglobal, qset, qdefault, faddalternates)
     struct sglobal *qglobal;
     struct uuconf_system *qset;
     struct uuconf_system *qdefault;
     boolean faddalternates;
{
  struct uuconf_system *qalt;

  if (qset->uuconf_palloc != qdefault->uuconf_palloc)
    qset->uuconf_palloc =
      _uuconf_pmalloc_block_merge (qset->uuconf_palloc,
				   qdefault->uuconf_palloc);

  /* If we are adding alternates from the default, make sure we have
     at least as many alternates in qset as we do in qdefault.  Each
     new alternate we create gets initialized to the first alternate
     of the system.  */
  if (faddalternates)
    {
      struct uuconf_system **pq, *qdef;

      for (qdef = qdefault, pq = &qset;
	   qdef != NULL;
	   qdef = qdef->uuconf_qalternate, pq = &(*pq)->uuconf_qalternate)
	{
	  if (*pq == NULL)
	    {
	      *pq = ((struct uuconf_system *)
		     uuconf_malloc (qset->uuconf_palloc,
				    sizeof (struct uuconf_system)));
	      if (*pq == NULL)
		{
		  qglobal->ierrno = errno;
		  return UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
		}
	      **pq = *qset;
	      (*pq)->uuconf_qalternate = NULL;
	    }
	}
    }

  for (qalt = qset; qalt != NULL; qalt = qalt->uuconf_qalternate)
    {
#define DEFAULT(x) \
  if (qalt->x == (char *) &_uuconf_unset) qalt->x = qdefault->x
      SYSTEM_STRINGS (DEFAULT);
#undef DEFAULT
#define DEFAULT(x) \
  if (qalt->x == (char **) &_uuconf_unset) qalt->x = qdefault->x
      SYSTEM_STRING_ARRAYS (DEFAULT);
#undef DEFAULT
#define DEFAULT(x) \
  if (qalt->x == (struct uuconf_timespan *) &_uuconf_unset) \
    qalt->x = qdefault->x
      SYSTEM_TIMESPANS (DEFAULT);
#undef DEFAULT
#define DEFAULT(x) if (qalt->x < 0) qalt->x = qdefault->x
      SYSTEM_BOOLEANS (DEFAULT);
      SYSTEM_INTEGERS (DEFAULT);
#undef DEFAULT

      /* We only copy over zport if both zport and qport are NULL,
	 because otherwise a default zport would override a specific
	 qport.  */
      if (qalt->uuconf_zport == (char *) &_uuconf_unset
	  && qalt->uuconf_qport == (struct uuconf_port *) &_uuconf_unset)
	qalt->uuconf_zport = qdefault->uuconf_zport;
      if (qalt->uuconf_qport == (struct uuconf_port *) &_uuconf_unset)
	qalt->uuconf_qport = qdefault->uuconf_qport;

      if (qalt->uuconf_qproto_params
	  == (struct uuconf_proto_param *) &_uuconf_unset)
	qalt->uuconf_qproto_params = qdefault->uuconf_qproto_params;
      else if (qdefault->uuconf_qproto_params != NULL)
	{
	  int cnew, ca;
	  struct uuconf_proto_param *qd, *qa;

	  /* Merge in the default protocol parameters, so that a
	     system with 'g' protocol parameters won't lose the
	     default 'i' protocol parameters.  */
	  ca = 0;
	  cnew = 0;
	  for (qd = qdefault->uuconf_qproto_params;
	       qd->uuconf_bproto != '\0';
	       qd++)
	    {
	      int c;

	      c = 0;
	      for (qa = qalt->uuconf_qproto_params;
		   (qa->uuconf_bproto != '\0'
		    && qa->uuconf_bproto != qd->uuconf_bproto);
		   qa++)
		++c;
	      if (qa->uuconf_bproto == '\0')
		{
		  ++cnew;
		  ca = c;
		}
	    }

	  if (cnew > 0)
	    {
	      struct uuconf_proto_param *qnew;

	      qnew = ((struct uuconf_proto_param *)
		      uuconf_malloc (qset->uuconf_palloc,
				     ((ca + cnew + 1)
				      * sizeof (struct uuconf_proto_param))));
	      if (qnew == NULL)
		{
		  qglobal->ierrno = errno;
		  return UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
		}
	      memcpy ((pointer) qnew, (pointer) qalt->uuconf_qproto_params,
		      ca * sizeof (struct uuconf_proto_param));
	      cnew = 0;
	      for (qd = qdefault->uuconf_qproto_params;
		   qd->uuconf_bproto != '\0';
		   qd++)
		{
		  for (qa = qalt->uuconf_qproto_params;
		       (qa->uuconf_bproto != '\0'
			&& qa->uuconf_bproto != qd->uuconf_bproto);
		       qa++)
		    ;
		  if (qa->uuconf_bproto == '\0')
		    {
		      qnew[ca + cnew] = *qd;
		      ++cnew;
		    }
		}
	      qnew[ca + cnew].uuconf_bproto = '\0';
	      uuconf_free (qset->uuconf_palloc, qalt->uuconf_qproto_params);
	      qalt->uuconf_qproto_params = qnew;
	    }
	}

      if (qdefault->uuconf_qalternate != NULL)
	qdefault = qdefault->uuconf_qalternate;
    }

  return UUCONF_SUCCESS;
}

/* Put in the basic defaults.  This ensures that the fields are valid
   on every uuconf_system structure.  */

int
_uuconf_isystem_basic_default (qglobal, q)
     struct sglobal *qglobal;
     register struct uuconf_system *q;
{
  int iret;

  iret = UUCONF_SUCCESS;

  for (; q != NULL && iret == UUCONF_SUCCESS; q = q->uuconf_qalternate)
    {
      /* The default of 26 allowable retries is traditional.  */
      if (q->uuconf_cmax_retries < 0)
	q->uuconf_cmax_retries = 26;
      if (q->uuconf_schat.uuconf_pzchat == (char **) &_uuconf_unset)
	{
	  q->uuconf_schat.uuconf_pzchat = NULL;
	  iret = _uuconf_iadd_string (qglobal, (char *) "\"\"", FALSE,
				      FALSE,
				      &q->uuconf_schat.uuconf_pzchat,
				      q->uuconf_palloc);
	  if (iret != UUCONF_SUCCESS)
	    return iret;
	  iret = _uuconf_iadd_string (qglobal, (char *) "\\r\\c", FALSE,
				      FALSE,
				      &q->uuconf_schat.uuconf_pzchat,
				      q->uuconf_palloc);
	  if (iret != UUCONF_SUCCESS)
	    return iret;
	  iret = _uuconf_iadd_string (qglobal, (char *) "ogin:", FALSE,
				      FALSE,
				      &q->uuconf_schat.uuconf_pzchat,
				      q->uuconf_palloc);
	  if (iret != UUCONF_SUCCESS)
	    return iret;
	  iret = _uuconf_iadd_string (qglobal, (char *) "-BREAK", FALSE,
				      FALSE,
				      &q->uuconf_schat.uuconf_pzchat,
				      q->uuconf_palloc);
	  if (iret != UUCONF_SUCCESS)
	    return iret;
	  iret = _uuconf_iadd_string (qglobal, (char *) "-ogin:", FALSE,
				      FALSE,
				      &q->uuconf_schat.uuconf_pzchat,
				      q->uuconf_palloc);
	  if (iret != UUCONF_SUCCESS)
	    return iret;
	  iret = _uuconf_iadd_string (qglobal, (char *) "-BREAK", FALSE,
				      FALSE,
				      &q->uuconf_schat.uuconf_pzchat,
				      q->uuconf_palloc);
	  if (iret != UUCONF_SUCCESS)
	    return iret;
	  iret = _uuconf_iadd_string (qglobal, (char *) "-ogin:", FALSE,
				      FALSE,
				      &q->uuconf_schat.uuconf_pzchat,
				      q->uuconf_palloc);
	  if (iret != UUCONF_SUCCESS)
	    return iret;
	  iret = _uuconf_iadd_string (qglobal, (char *) "\\L", FALSE,
				      FALSE,
				      &q->uuconf_schat.uuconf_pzchat,
				      q->uuconf_palloc);
	  if (iret != UUCONF_SUCCESS)
	    return iret;
	  iret = _uuconf_iadd_string (qglobal, (char *) "word:", FALSE,
				      FALSE,
				      &q->uuconf_schat.uuconf_pzchat,
				      q->uuconf_palloc);
	  if (iret != UUCONF_SUCCESS)
	    return iret;
	  iret = _uuconf_iadd_string (qglobal, (char *) "\\P", FALSE,
				      FALSE,
				      &q->uuconf_schat.uuconf_pzchat,
				      q->uuconf_palloc);
	  if (iret != UUCONF_SUCCESS)
	    return iret;
	}
      if (q->uuconf_schat.uuconf_ctimeout < 0)
	q->uuconf_schat.uuconf_ctimeout = 10;
      if (q->uuconf_schat.uuconf_fstrip < 0)
	q->uuconf_schat.uuconf_fstrip = TRUE;
      if (q->uuconf_scalled_chat.uuconf_ctimeout < 0)
	q->uuconf_scalled_chat.uuconf_ctimeout = 60;
      if (q->uuconf_scalled_chat.uuconf_fstrip < 0)
	q->uuconf_scalled_chat.uuconf_fstrip = TRUE;
      if (q->uuconf_fsend_request < 0)
	q->uuconf_fsend_request = TRUE;
      if (q->uuconf_frec_request < 0)
	q->uuconf_frec_request = TRUE;
      if (q->uuconf_fcall_transfer < 0)
	q->uuconf_fcall_transfer = TRUE;
      if (q->uuconf_fcalled_transfer < 0)
	q->uuconf_fcalled_transfer = TRUE;
      if (q->uuconf_pzlocal_send == (char **) &_uuconf_unset)
	{
	  q->uuconf_pzlocal_send = NULL;
	  iret = _uuconf_iadd_string (qglobal, (char *) ZROOTDIR, FALSE,
				      FALSE, &q->uuconf_pzlocal_send,
				      q->uuconf_palloc);
	  if (iret != UUCONF_SUCCESS)
	    return iret;
	}
      if (q->uuconf_pzremote_send == (char **) &_uuconf_unset)
	{
	  q->uuconf_pzremote_send = NULL;
	  iret = _uuconf_iadd_string (qglobal, (char *) "~", FALSE, FALSE,
				      &q->uuconf_pzremote_send,
				      q->uuconf_palloc);
	  if (iret != UUCONF_SUCCESS)
	    return iret;
	}
      if (q->uuconf_pzlocal_receive == (char **) &_uuconf_unset)
	{
	  q->uuconf_pzlocal_receive = NULL;
	  iret = _uuconf_iadd_string (qglobal, (char *) "~", FALSE, FALSE,
				      &q->uuconf_pzlocal_receive,
				      q->uuconf_palloc);
	  if (iret != UUCONF_SUCCESS)
	    return iret;
	}
      if (q->uuconf_pzremote_receive == (char **) &_uuconf_unset)
	{
	  q->uuconf_pzremote_receive = NULL;
	  iret = _uuconf_iadd_string (qglobal, (char *) "~", FALSE, FALSE,
				      &q->uuconf_pzremote_receive,
				      q->uuconf_palloc);
	  if (iret != UUCONF_SUCCESS)
	    return iret;
	}

      if (q->uuconf_pzpath == (char **) &_uuconf_unset)
	{
	  char *zdup;
	  char **pz;
	  size_t csplit;
	  int c;

	  zdup = (char *) uuconf_malloc (q->uuconf_palloc, sizeof CMDPATH);
	  if (zdup == NULL)
	    {
	      qglobal->ierrno = errno;
	      return UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
	    }
		  
	  memcpy ((pointer) zdup, (pointer) CMDPATH, sizeof CMDPATH);
	  pz = NULL;
	  csplit = 0;
	  if ((c = _uuconf_istrsplit (zdup, '\0', &pz, &csplit)) < 0)
	    {
	      qglobal->ierrno = errno;
	      return UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
	    }
	  q->uuconf_pzpath = (char **) uuconf_malloc (q->uuconf_palloc,
						      ((c + 1)
						       * sizeof (char *)));
	  if (q->uuconf_pzpath == NULL)
	    {
	      qglobal->ierrno = errno;
	      return UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
	    }
	  memcpy ((pointer) q->uuconf_pzpath, (pointer) pz,
		  c * sizeof (char *));
	  q->uuconf_pzpath[c] = NULL;
	  free ((pointer) pz);
	}

      if (q->uuconf_pzcmds == (char **) &_uuconf_unset)
	{
	  q->uuconf_pzcmds = ((char **)
			      uuconf_malloc (q->uuconf_palloc,
					     3 * sizeof (const char *)));
	  if (q->uuconf_pzcmds == NULL)
	    {
	      qglobal->ierrno = errno;
	      return UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
	    }
	  q->uuconf_pzcmds[0] = (char *) "rnews";
	  q->uuconf_pzcmds[1] = (char *) "rmail";
	  q->uuconf_pzcmds[2] = NULL;
	}

      if (q->uuconf_cfree_space < 0)
	q->uuconf_cfree_space = DEFAULT_FREE_SPACE;

      if (q->uuconf_zpubdir == (const char *) &_uuconf_unset)
	q->uuconf_zpubdir = qglobal->qprocess->zpubdir;

#define SET(x) if (q->x == (char *) &_uuconf_unset) q->x = NULL
      SYSTEM_STRINGS(SET);
#undef SET
#define SET(x) if (q->x == (char **) &_uuconf_unset) q->x = NULL
      SYSTEM_STRING_ARRAYS(SET);
#undef SET
#define SET(x) \
  if (q->x == (struct uuconf_timespan *) &_uuconf_unset) q->x = NULL
      SYSTEM_TIMESPANS (SET);
#undef SET
#define SET(x) if (q->x < 0) q->x = 0
      SYSTEM_BOOLEANS (SET);
      SYSTEM_INTEGERS (SET);
#undef SET

      if (q->uuconf_zport == (char *) &_uuconf_unset)
	q->uuconf_zport = NULL;
      if (q->uuconf_qport == (struct uuconf_port *) &_uuconf_unset)
	q->uuconf_qport = NULL;
      if (q->uuconf_qproto_params
	  == (struct uuconf_proto_param *) &_uuconf_unset)
	q->uuconf_qproto_params = NULL;
    }

  return iret;
}
