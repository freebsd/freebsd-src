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

/* update_dir includes dir as its last component.  */

void
Create_Admin (dir, update_dir, repository, tag, date, nonbranch)
    char *dir;
    char *update_dir;
    char *repository;
    char *tag;
    char *date;
    int nonbranch;
{
    FILE *fout;
    char *cp;
    char *tmp;

#ifdef SERVER_SUPPORT
    if (trace)
    {
	char *wd = xgetwd ();
	fprintf (stderr, "%c-> Create_Admin (%s, %s, %s, %s, %s) in %s\n",
		 (server_active) ? 'S' : ' ',
                dir, update_dir, repository, tag ? tag : "",
                date ? date : "", wd);
	free (wd);
    }
#endif

    if (noexec)
	return;

    tmp = xmalloc (strlen (dir) + 100);
    if (dir != NULL)
	(void) sprintf (tmp, "%s/%s", dir, CVSADM);
    else
	(void) strcpy (tmp, CVSADM);
    if (isfile (tmp))
	error (1, 0, "there is a version in %s already", update_dir);

    make_directory (tmp);

    /* record the current cvs root for later use */

    Create_Root (dir, CVSroot_original);
    if (dir != NULL)
	(void) sprintf (tmp, "%s/%s", dir, CVSADM_REP);
    else
	(void) strcpy (tmp, CVSADM_REP);
    fout = CVS_FOPEN (tmp, "w+");
    if (fout == NULL)
    {
	if (update_dir[0] == '\0')
	    error (1, errno, "cannot open %s", tmp);
	else
	    error (1, errno, "cannot open %s/%s", update_dir, CVSADM_REP);
    }
    cp = repository;
    strip_trailing_slashes (cp);

#ifdef RELATIVE_REPOS
    /*
     * If the Repository file is to hold a relative path, try to strip off
     * the leading CVSroot argument.
     */
    if (CVSroot_directory != NULL)
    {
	char *path = xmalloc (strlen (CVSroot_directory) + 10);

	(void) sprintf (path, "%s/", CVSroot_directory);
	if (strncmp (repository, path, strlen (path)) == 0)
	    cp = repository + strlen (path);
	free (path);
    }
#endif

    if (fprintf (fout, "%s\n", cp) < 0)
    {
	if (update_dir[0] == '\0')
	    error (1, errno, "write to %s failed", tmp);
	else
	    error (1, errno, "write to %s/%s failed", update_dir, CVSADM_REP);
    }
    if (fclose (fout) == EOF)
    {
	if (update_dir[0] == '\0')
	    error (1, errno, "cannot close %s", tmp);
	else
	    error (1, errno, "cannot close %s/%s", update_dir, CVSADM_REP);
    }

    /* now, do the Entries file */
    if (dir != NULL)
	(void) sprintf (tmp, "%s/%s", dir, CVSADM_ENT);
    else
	(void) strcpy (tmp, CVSADM_ENT);
    fout = CVS_FOPEN (tmp, "w+");
    if (fout == NULL)
    {
	if (update_dir[0] == '\0')
	    error (1, errno, "cannot open %s", tmp);
	else
	    error (1, errno, "cannot open %s/%s", update_dir, CVSADM_ENT);
    }
    if (fclose (fout) == EOF)
    {
	if (update_dir[0] == '\0')
	    error (1, errno, "cannot close %s", tmp);
	else
	    error (1, errno, "cannot close %s/%s", update_dir, CVSADM_ENT);
    }

    /* Create a new CVS/Tag file */
    WriteTag (dir, tag, date, nonbranch, update_dir, repository);

#ifdef SERVER_SUPPORT
    if (server_active)
    {
	server_template (update_dir, repository);
    }

    if (trace)
    {
	fprintf (stderr, "%c<- Create_Admin\n",
		 (server_active) ? 'S' : ' ');
    }
#endif

    free (tmp);
}
