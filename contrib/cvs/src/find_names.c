/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
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

static int find_dirs PROTO((char *dir, List * list, int checkadm,
			    List *entries));
static int find_rcs PROTO((char *dir, List * list));
static int add_subdir_proc PROTO((Node *, void *));
static int register_subdir_proc PROTO((Node *, void *));

/*
 * add the key from entry on entries list to the files list
 */
static int add_entries_proc PROTO((Node *, void *));
static int
add_entries_proc (node, closure)
     Node *node;
     void *closure;
{
    Entnode *entnode;
    Node *fnode;
    List *filelist = (List *) closure;

    entnode = (Entnode *) node->data;
    if (entnode->type != ENT_FILE)
	return (0);

    fnode = getnode ();
    fnode->type = FILES;
    fnode->key = xstrdup (node->key);
    if (addnode (filelist, fnode) != 0)
	freenode (fnode);
    return (0);
}

/* Find files in the repository and/or working directory.  On error,
   may either print a nonfatal error and return NULL, or just give
   a fatal error.  On success, return non-NULL (even if it is an empty
   list).  */

List *
Find_Names (repository, which, aflag, optentries)
    char *repository;
    int which;
    int aflag;
    List **optentries;
{
    List *entries;
    List *files;

    /* make a list for the files */
    files = getlist ();

    /* look at entries (if necessary) */
    if (which & W_LOCAL)
    {
	/* parse the entries file (if it exists) */
	entries = Entries_Open (aflag, NULL);
	if (entries != NULL)
	{
	    /* walk the entries file adding elements to the files list */
	    (void) walklist (entries, add_entries_proc, files);

	    /* if our caller wanted the entries list, return it; else free it */
	    if (optentries != NULL)
		*optentries = entries;
	    else
		Entries_Close (entries);
	}
    }

    if ((which & W_REPOS) && repository && !isreadable (CVSADM_ENTSTAT))
    {
	/* search the repository */
	if (find_rcs (repository, files) != 0)
	{
	    error (0, errno, "cannot open directory %s", repository);
	    goto error_exit;
	}

	/* search the attic too */
	if (which & W_ATTIC)
	{
	    char *dir;
	    dir = xmalloc (strlen (repository) + sizeof (CVSATTIC) + 10);
	    (void) sprintf (dir, "%s/%s", repository, CVSATTIC);
	    if (find_rcs (dir, files) != 0
		&& !existence_error (errno))
		/* For now keep this a fatal error, seems less useful
		   for access control than the case above.  */
		error (1, errno, "cannot open directory %s", dir);
	    free (dir);
	}
    }

    /* sort the list into alphabetical order and return it */
    sortlist (files, fsortcmp);
    return (files);
 error_exit:
    dellist (&files);
    return NULL;
}

/*
 * Add an entry from the subdirs list to the directories list.  This
 * is called via walklist.
 */

static int
add_subdir_proc (p, closure)
     Node *p;
     void *closure;
{
    List *dirlist = (List *) closure;
    Entnode *entnode;
    Node *dnode;

    entnode = (Entnode *) p->data;
    if (entnode->type != ENT_SUBDIR)
	return 0;

    dnode = getnode ();
    dnode->type = DIRS;
    dnode->key = xstrdup (entnode->user);
    if (addnode (dirlist, dnode) != 0)
	freenode (dnode);
    return 0;
}

/*
 * Register a subdirectory.  This is called via walklist.
 */

/*ARGSUSED*/
static int
register_subdir_proc (p, closure)
     Node *p;
     void *closure;
{
    List *entries = (List *) closure;

    Subdir_Register (entries, (char *) NULL, p->key);
    return 0;
}

/*
 * create a list of directories to traverse from the current directory
 */
List *
Find_Directories (repository, which, entries)
    char *repository;
    int which;
    List *entries;
{
    List *dirlist;

    /* make a list for the directories */
    dirlist = getlist ();

    /* find the local ones */
    if (which & W_LOCAL)
    {
	List *tmpentries;
	struct stickydirtag *sdtp;

	/* Look through the Entries file.  */

	if (entries != NULL)
	    tmpentries = entries;
	else if (isfile (CVSADM_ENT))
	    tmpentries = Entries_Open (0, NULL);
	else
	    tmpentries = NULL;

	if (tmpentries != NULL)
	    sdtp = (struct stickydirtag *) tmpentries->list->data;

	/* If we do have an entries list, then if sdtp is NULL, or if
           sdtp->subdirs is nonzero, all subdirectory information is
           recorded in the entries list.  */
	if (tmpentries != NULL && (sdtp == NULL || sdtp->subdirs))
	    walklist (tmpentries, add_subdir_proc, (void *) dirlist);
	else
	{
	    /* This is an old working directory, in which subdirectory
               information is not recorded in the Entries file.  Find
               the subdirectories the hard way, and, if possible, add
               it to the Entries file for next time.  */

	    /* FIXME-maybe: find_dirs is bogus for this usage because
	       it skips CVSATTIC and CVSLCK directories--those names
	       should be special only in the repository.  However, in
	       the interests of not perturbing this code, we probably
	       should leave well enough alone unless we want to write
	       a sanity.sh test case (which would operate by manually
	       hacking on the CVS/Entries file).  */

	    if (find_dirs (".", dirlist, 1, tmpentries) != 0)
		error (1, errno, "cannot open current directory");
	    if (tmpentries != NULL)
	    {
		if (! list_isempty (dirlist))
		    walklist (dirlist, register_subdir_proc,
			      (void *) tmpentries);
		else
		    Subdirs_Known (tmpentries);
	    }
	}

	if (entries == NULL && tmpentries != NULL)
	    Entries_Close (tmpentries);
    }

    /* look for sub-dirs in the repository */
    if ((which & W_REPOS) && repository)
    {
	/* search the repository */
	if (find_dirs (repository, dirlist, 0, entries) != 0)
	    error (1, errno, "cannot open directory %s", repository);

	/* We don't need to look in the attic because directories
	   never go in the attic.  In the future, there hopefully will
	   be a better mechanism for detecting whether a directory in
	   the repository is alive or dead; it may or may not involve
	   moving directories to the attic.  */
    }

    /* sort the list into alphabetical order and return it */
    sortlist (dirlist, fsortcmp);
    return (dirlist);
}

/*
 * Finds all the ,v files in the argument directory, and adds them to the
 * files list.  Returns 0 for success and non-zero if the argument directory
 * cannot be opened, in which case errno is set to indicate the error.
 * In the error case LIST is left in some reasonable state (unchanged, or
 * containing the files which were found before the error occurred).
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
    if ((dirp = CVS_OPENDIR (dir)) == NULL)
	return (1);

    /* read the dir, grabbing the ,v files */
    errno = 0;
    while ((dp = CVS_READDIR (dirp)) != NULL)
    {
	if (CVS_FNMATCH (RCSPAT, dp->d_name, 0) == 0) 
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
	errno = 0;
    }
    if (errno != 0)
    {
	int save_errno = errno;
	(void) CVS_CLOSEDIR (dirp);
	errno = save_errno;
	return 1;
    }
    (void) CVS_CLOSEDIR (dirp);
    return (0);
}

/*
 * Finds all the subdirectories of the argument dir and adds them to
 * the specified list.  Sub-directories without a CVS administration
 * directory are optionally ignored.  If ENTRIES is not NULL, all
 * files on the list are ignored.  Returns 0 for success or 1 on
 * error, in which case errno is set to indicate the error.
 */
static int
find_dirs (dir, list, checkadm, entries)
    char *dir;
    List *list;
    int checkadm;
    List *entries;
{
    Node *p;
    char *tmp = NULL;
    size_t tmp_size = 0;
    struct dirent *dp;
    DIR *dirp;
    int skip_emptydir = 0;

    /* First figure out whether we need to skip directories named
       Emptydir.  Except in the CVSNULLREPOS case, Emptydir is just
       a normal directory name.  */
    if (isabsolute (dir)
	&& strncmp (dir, current_parsed_root->directory, strlen (current_parsed_root->directory)) == 0
	&& ISDIRSEP (dir[strlen (current_parsed_root->directory)])
	&& strcmp (dir + strlen (current_parsed_root->directory) + 1, CVSROOTADM) == 0)
	skip_emptydir = 1;

    /* set up to read the dir */
    if ((dirp = CVS_OPENDIR (dir)) == NULL)
	return (1);

    /* read the dir, grabbing sub-dirs */
    errno = 0;
    while ((dp = CVS_READDIR (dirp)) != NULL)
    {
	if (strcmp (dp->d_name, ".") == 0 ||
	    strcmp (dp->d_name, "..") == 0 ||
	    strcmp (dp->d_name, CVSATTIC) == 0 ||
	    strcmp (dp->d_name, CVSLCK) == 0 ||
	    strcmp (dp->d_name, CVSREP) == 0)
	    goto do_it_again;

	/* findnode() is going to be significantly faster than stat()
	   because it involves no system calls.  That is why we bother
	   with the entries argument, and why we check this first.  */
	if (entries != NULL && findnode (entries, dp->d_name) != NULL)
	    goto do_it_again;

	if (skip_emptydir
	    && strcmp (dp->d_name, CVSNULLREPOS) == 0)
	    goto do_it_again;

#ifdef DT_DIR
	if (dp->d_type != DT_DIR) 
	{
	    if (dp->d_type != DT_UNKNOWN && dp->d_type != DT_LNK)
		goto do_it_again;
#endif
	    /* don't bother stating ,v files */
	    if (CVS_FNMATCH (RCSPAT, dp->d_name, 0) == 0)
		goto do_it_again;

	    expand_string (&tmp,
			   &tmp_size,
			   strlen (dir) + strlen (dp->d_name) + 10);
	    sprintf (tmp, "%s/%s", dir, dp->d_name);
	    if (!isdir (tmp))
		goto do_it_again;

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
		    goto do_it_again;
#endif
		/* Note that we only get here if we already set tmp
		   above.  */
		if (islink (tmp))
		    goto do_it_again;
#ifdef DT_DIR
	    }
#endif

	    /* check for new style */
	    expand_string (&tmp,
			   &tmp_size,
			   (strlen (dir) + strlen (dp->d_name)
			    + sizeof (CVSADM) + 10));
	    (void) sprintf (tmp, "%s/%s/%s", dir, dp->d_name, CVSADM);
	    if (!isdir (tmp))
		goto do_it_again;
	}

	/* put it in the list */
	p = getnode ();
	p->type = DIRS;
	p->key = xstrdup (dp->d_name);
	if (addnode (list, p) != 0)
	    freenode (p);

    do_it_again:
	errno = 0;
    }
    if (errno != 0)
    {
	int save_errno = errno;
	(void) CVS_CLOSEDIR (dirp);
	errno = save_errno;
	return 1;
    }
    (void) CVS_CLOSEDIR (dirp);
    if (tmp != NULL)
	free (tmp);
    return (0);
}
