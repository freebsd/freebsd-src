/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.4 kit.
 */

#include "cvs.h"
#include "getline.h"

/* Determine the name of the RCS repository for directory DIR in the
   current working directory, or for the current working directory
   itself if DIR is NULL.  Returns the name in a newly-malloc'd
   string.  On error, gives a fatal error and does not return.
   UPDATE_DIR is the path from where cvs was invoked (for use in error
   messages), and should contain DIR as its last component.
   UPDATE_DIR can be NULL to signify the directory in which cvs was
   invoked.  */

char *
Name_Repository (dir, update_dir)
    char *dir;
    char *update_dir;
{
    FILE *fpin;
    char *xupdate_dir;
    char *repos = NULL;
    size_t repos_allocated = 0;
    char *tmp;
    char *cp;

    if (update_dir && *update_dir)
	xupdate_dir = update_dir;
    else
	xupdate_dir = ".";

    if (dir != NULL)
    {
	tmp = xmalloc (strlen (dir) + sizeof (CVSADM_REP) + 10);
	(void) sprintf (tmp, "%s/%s", dir, CVSADM_REP);
    }
    else
	tmp = xstrdup (CVSADM_REP);

    /*
     * The assumption here is that the repository is always contained in the
     * first line of the "Repository" file.
     */
    fpin = CVS_FOPEN (tmp, "r");

    if (fpin == NULL)
    {
	int save_errno = errno;
	char *cvsadm;

	if (dir != NULL)
	{
	    cvsadm = xmalloc (strlen (dir) + sizeof (CVSADM) + 10);
	    (void) sprintf (cvsadm, "%s/%s", dir, CVSADM);
	}
	else
	    cvsadm = xstrdup (CVSADM);

	if (!isdir (cvsadm))
	{
	    error (0, 0, "in directory %s:", xupdate_dir);
	    error (1, 0, "there is no version here; do '%s checkout' first",
		   program_name);
	}
	free (cvsadm);

	if (existence_error (save_errno))
	{
	    /* FIXME: This is a very poorly worded error message.  It
	       occurs at least in the case where the user manually
	       creates a directory named CVS, so the error message
	       should be more along the lines of "CVS directory found
	       without administrative files; use CVS to create the CVS
	       directory, or rename it to something else if the
	       intention is to store something besides CVS
	       administrative files".  */
	    error (0, 0, "in directory %s:", xupdate_dir);
	    error (1, 0, "*PANIC* administration files missing");
	}

	error (1, save_errno, "cannot open %s", tmp);
    }
    free (tmp);

    if (getline (&repos, &repos_allocated, fpin) < 0)
    {
	/* FIXME: should be checking for end of file separately.  */
	error (0, 0, "in directory %s:", xupdate_dir);
	error (1, errno, "cannot read %s", CVSADM_REP);
    }
    (void) fclose (fpin);
    if ((cp = strrchr (repos, '\n')) != NULL)
	*cp = '\0';			/* strip the newline */

    /*
     * If this is a relative repository pathname, turn it into an absolute
     * one by tacking on the CVSROOT environment variable. If the CVSROOT
     * environment variable is not set, die now.
     */
    if (strcmp (repos, "..") == 0 || strncmp (repos, "../", 3) == 0)
    {
	error (0, 0, "in directory %s:", xupdate_dir);
	error (0, 0, "`..'-relative repositories are not supported.");
	error (1, 0, "illegal source repository");
    }
    if (! isabsolute(repos))
    {
	char *newrepos;

	if (CVSroot_original == NULL)
	{
	    error (0, 0, "in directory %s:", xupdate_dir);
	    error (0, 0, "must set the CVSROOT environment variable\n");
	    error (0, 0, "or specify the '-d' option to %s.", program_name);
	    error (1, 0, "illegal repository setting");
	}
	newrepos = xmalloc (strlen (CVSroot_directory) + strlen (repos) + 10);
	(void) sprintf (newrepos, "%s/%s", CVSroot_directory, repos);
	free (repos);
	repos = newrepos;
    }

    strip_trailing_slashes (repos);
    return repos;
}

/*
 * Return a pointer to the repository name relative to CVSROOT from a
 * possibly fully qualified repository
 */
char *
Short_Repository (repository)
    char *repository;
{
    if (repository == NULL)
	return (NULL);

    /* If repository matches CVSroot at the beginning, strip off CVSroot */
    /* And skip leading '/' in rep, in case CVSroot ended with '/'. */
    if (strncmp (CVSroot_directory, repository,
		 strlen (CVSroot_directory)) == 0)
    {
	char *rep = repository + strlen (CVSroot_directory);
	return (*rep == '/') ? rep+1 : rep;
    }
    else
	return (repository);
}
