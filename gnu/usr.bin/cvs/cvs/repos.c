/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.4 kit.
 * 
 * Name of Repository
 * 
 * Determine the name of the RCS repository and sets "Repository" accordingly.
 */

#include "cvs.h"

#ifndef lint
static const char rcsid[] = "$CVSid: @(#)repos.c 1.32 94/09/23 $";
USE(rcsid);
#endif

char *
Name_Repository (dir, update_dir)
    char *dir;
    char *update_dir;
{
    FILE *fpin;
    char *ret, *xupdate_dir;
    char repos[PATH_MAX];
    char path[PATH_MAX];
    char tmp[PATH_MAX];
    char cvsadm[PATH_MAX];
    char *cp;

    if (update_dir && *update_dir)
	xupdate_dir = update_dir;
    else
	xupdate_dir = ".";

    if (dir != NULL)
	(void) sprintf (cvsadm, "%s/%s", dir, CVSADM);
    else
	(void) strcpy (cvsadm, CVSADM);

    /* sanity checks */
    if (!isdir (cvsadm))
    {
	error (0, 0, "in directory %s:", xupdate_dir);
	error (1, 0, "there is no version here; do '%s checkout' first",
	       program_name);
    }

    if (dir != NULL)
	(void) sprintf (tmp, "%s/%s", dir, CVSADM_ENT);
    else
	(void) strcpy (tmp, CVSADM_ENT);

    if (!isreadable (tmp))
    {
	error (0, 0, "in directory %s:", xupdate_dir);
	error (1, 0, "*PANIC* administration files missing");
    }

    if (dir != NULL)
	(void) sprintf (tmp, "%s/%s", dir, CVSADM_REP);
    else
	(void) strcpy (tmp, CVSADM_REP);

    if (!isreadable (tmp))
    {
	error (0, 0, "in directory %s:", xupdate_dir);
	error (1, 0, "*PANIC* administration files missing");
    }

    /*
     * The assumption here is that the repository is always contained in the
     * first line of the "Repository" file.
     */
    fpin = open_file (tmp, "r");

    if (fgets (repos, PATH_MAX, fpin) == NULL)
    {
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
	if (CVSroot == NULL)
	{
	    error (0, 0, "in directory %s:", xupdate_dir);
	    error (0, 0, "must set the CVSROOT environment variable\n");
	    error (0, 0, "or specify the '-d' option to %s.", program_name);
	    error (1, 0, "illegal repository setting");
	}
	(void) strcpy (path, repos);
	(void) sprintf (repos, "%s/%s", CVSroot, path);
    }
#ifdef CLIENT_SUPPORT
    if (!client_active && !isdir (repos))
#else
    if (!isdir (repos))
#endif
    {
	error (0, 0, "in directory %s:", xupdate_dir);
	error (1, 0, "there is no repository %s", repos);
    }

    /* allocate space to return and fill it in */
    strip_path (repos);
    ret = xstrdup (repos);
    return (ret);
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
    if (strncmp (CVSroot, repository, strlen (CVSroot)) == 0)
    {
	char *rep = repository + strlen (CVSroot);
	return (*rep == '/') ? rep+1 : rep;
    }
    else
	return (repository);
}
