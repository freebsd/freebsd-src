/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.4 kit.
 * 
 * Find Names
 * 
 * Finds all the pertinent file names, both from the administration and from the
 * repository
 * 
 * Find Dirs
 * 
 * Finds all pertinent sub-directories of the checked out instantiation and the
 * repository (and optionally the attic)
 */

#include "cvs.h"

#ifndef lint
static char rcsid[] = "$CVSid: @(#)find_names.c 1.45 94/10/22 $";
USE(rcsid)
#endif

static int find_dirs PROTO((char *dir, List * list, int checkadm));
static int find_rcs PROTO((char *dir, List * list));

static List *filelist;

/*
 * add the key from entry on entries list to the files list
 */
static int
add_entries_proc (node, closure)
     Node *node;
     void *closure;
{
    Node *fnode;

    fnode = getnode ();
    fnode->type = FILES;
    fnode->key = xstrdup (node->key);
    if (addnode (filelist, fnode) != 0)
	freenode (fnode);
    return (0);
}

/*
 * compare two files list node (for sort)
 */
static int
fsortcmp (p, q)
    Node *p, *q;
{
    return (strcmp (p->key, q->key));
}

List *
Find_Names (repository, which, aflag, optentries)
    char *repository;
    int which;
    int aflag;
    List **optentries;
{
    List *entries;
    List *files;
    char dir[PATH_MAX];

    /* make a list for the files */
    files = filelist = getlist ();

    /* look at entries (if necessary) */
    if (which & W_LOCAL)
    {
	/* parse the entries file (if it exists) */
	entries = ParseEntries (aflag);

	if (entries != NULL)
	{
	    /* walk the entries file adding elements to the files list */
	    (void) walklist (entries, add_entries_proc, NULL);

	    /* if our caller wanted the entries list, return it; else free it */
	    if (optentries != NULL)
		*optentries = entries;
	    else
		dellist (&entries);
	}
    }

    if ((which & W_REPOS) && repository && !isreadable (CVSADM_ENTSTAT))
    {
	/* search the repository */
	if (find_rcs (repository, files) != 0)
	    error (1, errno, "cannot open directory %s", repository);

	/* search the attic too */
	if (which & W_ATTIC)
	{
	    (void) sprintf (dir, "%s/%s", repository, CVSATTIC);
	    (void) find_rcs (dir, files);
	}
    }

    /* sort the list into alphabetical order and return it */
    sortlist (files, fsortcmp);
    return (files);
}

/*
 * create a list of directories to traverse from the current directory
 */
List *
Find_Dirs (repository, which)
    char *repository;
    int which;
{
    List *dirlist;

    /* make a list for the directories */
    dirlist = getlist ();

    /* find the local ones */
    if (which & W_LOCAL)
    {
	/* look only for CVS controlled sub-directories */
	if (find_dirs (".", dirlist, 1) != 0)
	    error (1, errno, "cannot open current directory");
    }

    /* look for sub-dirs in the repository */
    if ((which & W_REPOS) && repository)
    {
	/* search the repository */
	if (find_dirs (repository, dirlist, 0) != 0)
	    error (1, errno, "cannot open directory %s", repository);

#ifdef ATTIC_DIR_SUPPORT		/* XXX - FIXME */
	/* search the attic too */
	if (which & W_ATTIC)
	{
	    char dir[PATH_MAX];

	    (void) sprintf (dir, "%s/%s", repository, CVSATTIC);
	    (void) find_dirs (dir, dirlist, 0);
	}
#endif
    }

    /* sort the list into alphabetical order and return it */
    sortlist (dirlist, fsortcmp);
    return (dirlist);
}

/*
 * Finds all the ,v files in the argument directory, and adds them to the
 * files list.  Returns 0 for success and non-zero if the argument directory
 * cannot be opened.
 */
static int
find_rcs (dir, list)
    char *dir;
    List *list;
{
    Node *p;
    struct dirent *dp;
    DIR *dirp;

    /* set up to read the dir */
    if ((dirp = opendir (dir)) == NULL)
	return (1);

    /* read the dir, grabbing the ,v files */
    while ((dp = readdir (dirp)) != NULL)
    {
	if (fnmatch (RCSPAT, dp->d_name, 0) == 0) 
	{
	    char *comma;

	    comma = strrchr (dp->d_name, ',');	/* strip the ,v */
	    *comma = '\0';
	    p = getnode ();
	    p->type = FILES;
	    p->key = xstrdup (dp->d_name);
	    if (addnode (list, p) != 0)
		freenode (p);
	}
    }
    (void) closedir (dirp);
    return (0);
}

/*
 * Finds all the subdirectories of the argument dir and adds them to the
 * specified list.  Sub-directories without a CVS administration directory
 * are optionally ignored  Returns 0 for success or 1 on error.
 */
static int
find_dirs (dir, list, checkadm)
    char *dir;
    List *list;
    int checkadm;
{
    Node *p;
    char tmp[PATH_MAX];
    struct dirent *dp;
    DIR *dirp;

    /* set up to read the dir */
    if ((dirp = opendir (dir)) == NULL)
	return (1);

    /* read the dir, grabbing sub-dirs */
    while ((dp = readdir (dirp)) != NULL)
    {
	if (strcmp (dp->d_name, ".") == 0 ||
	    strcmp (dp->d_name, "..") == 0 ||
	    strcmp (dp->d_name, CVSATTIC) == 0 ||
	    strcmp (dp->d_name, CVSLCK) == 0)
	    continue;

#ifdef DT_DIR
	if (dp->d_type != DT_DIR) 
	{
	    if (dp->d_type != DT_UNKNOWN && dp->d_type != DT_LNK)
		continue;
#endif
	    /* don't bother stating ,v files */
	    if (fnmatch (RCSPAT, dp->d_name, 0) == 0)
		continue;

	    sprintf (tmp, "%s/%s", dir, dp->d_name);
	    if (!isdir (tmp))
		continue;

#ifdef DT_DIR
	}
#endif

	/* check for administration directories (if needed) */
	if (checkadm)
	{
	    /* blow off symbolic links to dirs in local dir */
#ifdef DT_DIR
	    if (dp->d_type != DT_DIR)
	    {
		/* we're either unknown or a symlink at this point */
		if (dp->d_type == DT_LNK)
		    continue;
#endif
		if (islink (tmp))
		    continue;
#ifdef DT_DIR
	    }
#endif

	    /* check for new style */
	    (void) sprintf (tmp, "%s/%s/%s", dir, dp->d_name, CVSADM);
	    if (!isdir (tmp))
	    {
		/* and old style */
		(void) sprintf (tmp, "%s/%s/%s", dir, dp->d_name, OCVSADM);
		if (!isdir (tmp))
		    continue;
	    }
	}

	/* put it in the list */
	p = getnode ();
	p->type = DIRS;
	p->key = xstrdup (dp->d_name);
	if (addnode (list, p) != 0)
	    freenode (p);
    }
    (void) closedir (dirp);
    return (0);
}
