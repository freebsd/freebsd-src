/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.4 kit.
 * 
 * Create Administration.
 * 
 * Creates a CVS administration directory based on the argument repository; the
 * "Entries" file is prefilled from the "initrecord" argument.
 */

#include "cvs.h"

#ifndef lint
static char rcsid[] = "$CVSid: @(#)create_adm.c 1.28 94/09/23 $";
USE(rcsid)
#endif

void
Create_Admin (dir, repository, tag, date)
    char *dir;
    char *repository;
    char *tag;
    char *date;
{
    FILE *fout;
    char *cp;
    char tmp[PATH_MAX];

    if (noexec)
	return;

    if (dir != NULL)
	(void) sprintf (tmp, "%s/%s", dir, CVSADM);
    else
	(void) strcpy (tmp, CVSADM);

    if (isfile (tmp))
	error (1, 0, "there is a version here already");
    else
    {
	if (dir != NULL)
	    (void) sprintf (tmp, "%s/%s", dir, OCVSADM);
	else
	    (void) strcpy (tmp, OCVSADM);

	if (isfile (tmp))
	    error (1, 0, "there is a version here already");
    }

    if (dir != NULL)
	(void) sprintf (tmp, "%s/%s", dir, CVSADM);
    else
	(void) strcpy (tmp, CVSADM);
    make_directory (tmp);

#ifdef CVSADM_ROOT
    /* record the current cvs root for later use */

    Create_Root (dir, CVSroot);
#endif /* CVSADM_ROOT */
    if (dir != NULL)
	(void) sprintf (tmp, "%s/%s", dir, CVSADM_REP);
    else
	(void) strcpy (tmp, CVSADM_REP);
    fout = open_file (tmp, "w+");
    cp = repository;
    strip_path (cp);

#ifdef RELATIVE_REPOS
    /*
     * If the Repository file is to hold a relative path, try to strip off
     * the leading CVSroot argument.
     */
    if (CVSroot != NULL)
    {
	char path[PATH_MAX];

	(void) sprintf (path, "%s/", CVSroot);
	if (strncmp (repository, path, strlen (path)) == 0)
	    cp = repository + strlen (path);
    }
#endif

    if (fprintf (fout, "%s\n", cp) == EOF)
	error (1, errno, "write to %s failed", tmp);
    if (fclose (fout) == EOF)
	error (1, errno, "cannot close %s", tmp);

    /* now, do the Entries file */
    if (dir != NULL)
	(void) sprintf (tmp, "%s/%s", dir, CVSADM_ENT);
    else
	(void) strcpy (tmp, CVSADM_ENT);
    fout = open_file (tmp, "w+");
    if (fclose (fout) == EOF)
	error (1, errno, "cannot close %s", tmp);

    /* Create a new CVS/Tag file */
    WriteTag (dir, tag, date);
}
