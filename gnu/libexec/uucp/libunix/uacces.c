/* uacces.c
   Check access to a file by user name.

   Copyright (C) 1992 Ian Lance Taylor

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

#include "uudefs.h"
#include "sysdep.h"

#include <pwd.h>
#include <errno.h>

#if HAVE_GETGRENT
#include <grp.h>
#if GETGRENT_DECLARATION_OK
#ifndef getgrent
extern struct group *getgrent ();
#endif
#endif
#endif /* HAVE_GETGRENT */

#if GETPWNAM_DECLARATION_OK
#ifndef getpwnam
extern struct passwd *getpwnam ();
#endif
#endif

/* Do access(2) on a stat structure, except that the user name is
   provided.  If the user name in zuser is NULL, require the file to
   be accessible to the world.  Return TRUE if access is permitted,
   FALSE otherwise.  This does not log an error message.  */

boolean
fsuser_access (q, imode, zuser)
     const struct stat *q;
     int imode;
     const char *zuser;
{
  static char *zuser_hold;
  static uid_t iuid_hold;
  static gid_t igid_hold;
  static int cgroups_hold;
  static gid_t *paigroups_hold;
  int ir, iw, ix, iand;

  if (imode == F_OK)
    return TRUE;

  if (zuser != NULL)
    {
      /* We keep static variables around for the last user we did, to
	 avoid looking up a user multiple times.  */
      if (zuser_hold == NULL || strcmp (zuser_hold, zuser) != 0)
	{
	  struct passwd *qpwd;

	  if (zuser_hold != NULL)
	    {
	      ubuffree (zuser_hold);
	      zuser_hold = NULL;
	      cgroups_hold = 0;
	      xfree ((pointer) paigroups_hold);
	      paigroups_hold = NULL;
	    }

	  qpwd = getpwnam ((char *) zuser);
	  if (qpwd == NULL)
	    {
	      /* Check this as a remote request.  */
	      zuser = NULL;
	    }
	  else
	    {
#if HAVE_GETGRENT
	      struct group *qg;
#endif

	      zuser_hold = zbufcpy (zuser);

	      iuid_hold = qpwd->pw_uid;
	      igid_hold = qpwd->pw_gid;

#if HAVE_GETGRENT
	      /* Get the list of groups for this user.  This is
		 definitely more appropriate for BSD than for System
		 V.  It may just be a waste of time, and perhaps it
		 should be configurable.  */
	      setgrent ();
	      while ((qg = getgrent ()) != NULL)
		{
		  const char **pz;

		  if (qg->gr_gid == igid_hold)
		    continue;
		  for (pz = (const char **) qg->gr_mem; *pz != NULL; pz++)
		    {
		      if ((*pz)[0] == *zuser
			  && strcmp (*pz, zuser) == 0)
			{
			  paigroups_hold = ((gid_t *)
					    (xrealloc
					     ((pointer) paigroups_hold,
					      ((cgroups_hold + 1)
					       * sizeof (gid_t)))));
			  paigroups_hold[cgroups_hold] = qg->gr_gid;
			  ++cgroups_hold;
			  break;
			}
		    }
		}
	      endgrent ();
#endif
	    }
	}
    }


  /* Now do the actual access check.  */

  if (zuser != NULL)
    {
      /* The superuser can do anything.  */
      if (iuid_hold == 0)
	return TRUE;

      /* If this is the uid we're running under, there's no point to
	 checking access further, because when we actually try the
	 operation the system will do the checking for us.  */
      if (iuid_hold == geteuid ())
	return TRUE;
    }

  ir = S_IROTH;
  iw = S_IWOTH;
  ix = S_IXOTH;

  if (zuser != NULL)
    {
      if (iuid_hold == q->st_uid)
	{
	  ir = S_IRUSR;
	  iw = S_IWUSR;
	  ix = S_IXUSR;
	}
      else
	{
	  boolean fgroup;

	  fgroup = FALSE;
	  if (igid_hold == q->st_gid)
	    fgroup = TRUE;
	  else
	    {
	      int i;

	      for (i = 0; i < cgroups_hold; i++)
		{
		  if (paigroups_hold[i] == q->st_gid)
		    {
		      fgroup = TRUE;
		      break;
		    }
		}
	    }

	  if (fgroup)
	    {
	      ir = S_IRGRP;
	      iw = S_IWGRP;
	      ix = S_IXGRP;
	    }
	}
    }

  iand = 0;
  if ((imode & R_OK) != 0)
    iand |= ir;
  if ((imode & W_OK) != 0)
    iand |= iw;
  if ((imode & X_OK) != 0)
    iand |= ix;

  return (q->st_mode & iand) == iand;
}
